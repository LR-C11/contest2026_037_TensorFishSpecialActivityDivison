/****************************************************************************
 * dm_chat_ai.c — MiMo cloud chat + local offline fallback
 ****************************************************************************/

#include "deskmate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <syslog.h>
#include <netutils/cJSON.h>

#ifdef CONFIG_DESKMATE_APP

/* MiMo OpenAI-compatible endpoint. Store the rotated key only on-device. */
#define MIMO_BASE "https://token-plan-ams.xiaomimimo.com/v1"
#define MIMO_KEY_PATH "/data/deskmate_mimo.key"
#define MIMO_CURL_CONFIG "/tmp/dm_chat_mimo.conf"
#define MIMO_MODEL "mimo-v2.6-flash"
#define CHAT_REQ_PATH "/tmp/dm_chat_req.json"
#define CHAT_RSP_PATH "/tmp/dm_chat_rsp.json"
#define CHAT_CURL_SCRIPT "/tmp/dm_chat_curl.sh"
#define CHAT_MAX_REPLY 240

static int chat_key_load(char *key, size_t key_cap)
{
  FILE *f;
  size_t i;
  size_t n;
  int valid = 0;

  if (!key || key_cap < 16)
    {
      return 0;
    }
  key[0] = '\0';
  f = fopen(MIMO_KEY_PATH, "rb");
  if (!f)
    {
      return 0;
    }
  if (fgets(key, (int)key_cap, f))
    {
      n = strlen(key);
      while (n > 0 && (key[n - 1] == '\r' || key[n - 1] == '\n' ||
                       key[n - 1] == ' ' || key[n - 1] == '\t'))
        {
          key[--n] = '\0';
        }
      valid = n > 8 && n < key_cap - 1;
      for (i = 0; valid && i < n; i++)
        {
          unsigned char ch = (unsigned char)key[i];
          if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
                (ch >= '0' && ch <= '9') || ch == '-' || ch == '_' ||
                ch == '.'))
            {
              valid = 0;
            }
        }
    }
  fclose(f);
  if (!valid)
    {
      memset(key, 0, key_cap);
    }
  return valid;
}

static int run_curl_script(const char *line)
{
  char key[128];
  FILE *kf;
  FILE *sf;
  int key_write_ok;
  int key_close_rc;
  int script_write_ok;
  int script_close_rc;
  int rc;

  if (!chat_key_load(key, sizeof(key)))
    {
      syslog(LOG_ERR, "[dm-chat] runtime API key missing or invalid\n");
      return -1;
    }

  kf = fopen(MIMO_CURL_CONFIG, "w");
  if (!kf || chmod(MIMO_CURL_CONFIG, S_IRUSR | S_IWUSR) != 0)
    {
      if (kf)
        {
          fclose(kf);
        }
      memset(key, 0, sizeof(key));
      unlink(MIMO_CURL_CONFIG);
      return -1;
    }
  key_write_ok = fprintf(kf, "header = \"Authorization: Bearer %s\"\n",
                         key) > 0;
  key_close_rc = fclose(kf);
  memset(key, 0, sizeof(key));
  if (!key_write_ok || key_close_rc != 0)
    {
      unlink(MIMO_CURL_CONFIG);
      return -1;
    }

  sf = fopen(CHAT_CURL_SCRIPT, "w");
  if (!sf)
    {
      unlink(MIMO_CURL_CONFIG);
      syslog(LOG_ERR, "[dm-chat] cannot create curl script\n");
      return -1;
    }
  script_write_ok = fputs("#!/bin/sh\n", sf) != EOF &&
                    fputs(line, sf) != EOF && fputc('\n', sf) != EOF;
  script_close_rc = fclose(sf);
  if (!script_write_ok || script_close_rc != 0)
    {
      unlink(MIMO_CURL_CONFIG);
      syslog(LOG_ERR, "[dm-chat] cannot write curl script\n");
      return -1;
    }

  rc = system("sh " CHAT_CURL_SCRIPT);
  unlink(MIMO_CURL_CONFIG);
  if (rc != 0)
    {
      syslog(LOG_ERR, "[dm-chat] curl script failed status=%d\n", rc);
      return -1;
    }
  return 0;
}

static char s_last_reply[CHAT_MAX_REPLY];
static int s_last_request_remote_ok;
static char s_persona_ctx[512];

static const char *const s_local_lines[] = {
  "我在呢。想聊什么，或者要不要先专注一会儿？",
  "今天也要好好照顾自己呀。",
  "记住：完成比完美更重要。",
  "累了就歇一下，我陪你。",
  "要不要喝口水？身体是本钱。",
  "你已经很棒了，继续加油！",
  "需要我帮你记点什么吗？去备忘看看。",
  "玩 2048 放松一下也可以哦。",
};

static const char *local_reply(const char *text)
{
  if (!text || !text[0])
    {
      return s_local_lines[0];
    }

  if (strstr(text, "你好") || strstr(text, "hi") || strstr(text, "hello") ||
      strstr(text, "嗨"))
    {
      return "你好呀！我是 openvela小助手，桌边的小搭档。";
    }
  if (strstr(text, "你是谁") || strstr(text, "介绍"))
    {
      return "我是 openvela小助手：陪你专注、记事、聊天的桌面小伙伴。";
    }
  if (strstr(text, "笑话") || strstr(text, "无聊"))
    {
      return "为什么程序员总分不清万圣节和圣诞节？因为 Oct 31 == Dec 25。";
    }
  if (strstr(text, "加油") || strstr(text, "鼓励") || strstr(text, "累"))
    {
      return "深呼吸。你不需要一次做完所有事，先做下一件就好。";
    }
  if (strstr(text, "专注") || strstr(text, "工作") || strstr(text, "学习"))
    {
      return "要不来一段专注？1 分钟也可以，开始了就不算晚。";
    }
  if (strstr(text, "天气"))
    {
      return "我这边暂时看不了天气，不过记得出门看一眼天空呀。";
    }
  if (strstr(text, "喝水"))
    {
      return "好主意，现在就去喝一口水吧。";
    }
  if (strstr(text, "吃药") || strstr(text, "药"))
    {
      {
        const char *cur = dm_t("去「功能 → 吃药」看看今天任务。",
                               "Check Features → Meds for today.");
        return cur;
      }
    }
  if (strstr(text, "2048") || strstr(text, "游戏") || strstr(text, "玩"))
    {
      return "功能里有 2048，滑一滑很解压。玩完记得回来专注哦。";
    }

  return s_local_lines[rand() % (int)(sizeof(s_local_lines) /
                                      sizeof(s_local_lines[0]))];
}

static int json_escape(const char *in, char *out, size_t cap)
{
  size_t o = 0;
  size_t i;

  if (!in || !out || cap < 2)
    {
      return -1;
    }
  for (i = 0; in[i] && o + 8 < cap; i++)
    {
      unsigned char ch = (unsigned char)in[i];
      if (ch == '"' || ch == '\\')
        {
          out[o++] = '\\';
          out[o++] = (char)ch;
        }
      else if (ch == '\n')
        {
          out[o++] = '\\';
          out[o++] = 'n';
        }
      else if (ch < 0x20)
        {
          /* skip control chars */
        }
      else
        {
          out[o++] = (char)ch;
        }
    }
  out[o] = '\0';
  return (int)o;
}

/* ---- multi-turn conversation memory (shared with voice chat) ---- */
#define HIST_TURN_MAX 3
#define HIST_LINE     128

static struct
{
  char user[HIST_LINE];
  char bot[HIST_LINE];
} s_hist[HIST_TURN_MAX];
static int s_hist_n;

void dm_chat_hist_reset(void)
{
  s_hist_n = 0;
}

void dm_chat_hist_push(const char *user, const char *assistant)
{
  int i;

  if (!user || !user[0])
    {
      return;
    }
  if (s_hist_n == HIST_TURN_MAX)
    {
      for (i = 1; i < HIST_TURN_MAX; i++)
        {
          s_hist[i - 1] = s_hist[i];
        }
      s_hist_n = HIST_TURN_MAX - 1;
    }
  strncpy(s_hist[s_hist_n].user, user, HIST_LINE - 1);
  s_hist[s_hist_n].user[HIST_LINE - 1] = '\0';
  if (assistant)
    {
      strncpy(s_hist[s_hist_n].bot, assistant, HIST_LINE - 1);
      s_hist[s_hist_n].bot[HIST_LINE - 1] = '\0';
    }
  else
    {
      s_hist[s_hist_n].bot[0] = '\0';
    }
  s_hist_n++;
}

/* emit {"role":..,"content":..}, entries (each with trailing comma) */
int dm_chat_hist_json(char *out, int cap)
{
  char esc[HIST_LINE * 2];
  int o = 0;
  int i;

  if (!out || cap < 2)
    {
      return -1;
    }
  out[0] = '\0';
  for (i = 0; i < s_hist_n; i++)
    {
      int n;

      if (s_hist[i].user[0])
        {
          json_escape(s_hist[i].user, esc, sizeof(esc));
          n = snprintf(out + o, (size_t)(cap - o),
                       "{\"role\":\"user\",\"content\":\"%s\"},", esc);
          if (n < 0 || n >= cap - o)
            {
              out[o] = '\0';
              return -1;
            }
          o += n;
        }
      if (s_hist[i].bot[0])
        {
          json_escape(s_hist[i].bot, esc, sizeof(esc));
          n = snprintf(out + o, (size_t)(cap - o),
                       "{\"role\":\"assistant\",\"content\":\"%s\"},", esc);
          if (n < 0 || n >= cap - o)
            {
              out[o] = '\0';
              return -1;
            }
          o += n;
        }
    }
  return o;
}

static void extract_content(const char *path, char *out, size_t cap)
{
  FILE *f;
  char *buf;
  long sz;
  cJSON *root;
  cJSON *choices;
  cJSON *choice;
  cJSON *message;
  cJSON *content;

  if (!out || cap < 2)
    {
      return;
    }
  out[0] = '\0';
  f = fopen(path, "rb");
  if (!f)
    {
      return;
    }
  if (fseek(f, 0, SEEK_END) != 0 || (sz = ftell(f)) <= 0 ||
      sz > 64 * 1024 || fseek(f, 0, SEEK_SET) != 0)
    {
      fclose(f);
      return;
    }
  buf = malloc((size_t)sz + 1);
  if (!buf)
    {
      fclose(f);
      return;
    }
  if (fread(buf, 1, (size_t)sz, f) != (size_t)sz)
    {
      free(buf);
      fclose(f);
      return;
    }
  buf[sz] = '\0';
  fclose(f);

  root = cJSON_Parse(buf);
  free(buf);
  if (!root)
    {
      return;
    }
  choices = cJSON_GetObjectItem(root, "choices");
  choice = choices && cJSON_IsArray(choices)
             ? cJSON_GetArrayItem(choices, 0) : NULL;
  message = choice ? cJSON_GetObjectItem(choice, "message") : NULL;
  content = message ? cJSON_GetObjectItem(message, "content") : NULL;
  if (content && cJSON_IsString(content) && content->valuestring)
    {
      snprintf(out, cap, "%s", content->valuestring);
    }
  cJSON_Delete(root);
}

int dm_chat_ai_available(void)
{
  return dm_wifi_connected() ? 1 : 0;
}

/* True only when the most recent chat answer came from the cloud API. */
int dm_chat_ai_remote_ok(void)
{
  return s_last_request_remote_ok;
}

void dm_chat_ai_set_persona(const char *persona)
{
  if (!persona)
    {
      s_persona_ctx[0] = '\0';
      return;
    }
  strncpy(s_persona_ctx, persona, sizeof(s_persona_ctx) - 1);
    s_persona_ctx[sizeof(s_persona_ctx) - 1] = '\0';
}

const char *dm_chat_ai_ask(const char *user_text)
{
  char esc[400];
  char persona[1024];
  char hist[1400];
  char body[2400];
  char cmd[1200];
  const char *persona_src;
  FILE *f;
  int n;

  s_last_reply[0] = '\0';
  s_last_request_remote_ok = 0;
  if (!user_text || !user_text[0])
    {
      return local_reply("");
    }

  if (!dm_wifi_connected())
    {
      strncpy(s_last_reply, local_reply(user_text), sizeof(s_last_reply) - 1);
      s_last_reply[sizeof(s_last_reply) - 1] = '\0';
      return s_last_reply;
    }

  /* Do not send HTTPS while the board clock is still at its boot epoch;
   * this causes mbedTLS to reject valid certificates as not-yet-valid. */
  if (dm_wifi_wait_time_sync(40) != 0)
    {
      syslog(LOG_WARNING, "[dm-chat] system clock not synchronized\n");
      strncpy(s_last_reply, local_reply(user_text), sizeof(s_last_reply) - 1);
      s_last_reply[sizeof(s_last_reply) - 1] = '\0';
      return s_last_reply;
    }

  if (json_escape(user_text, esc, sizeof(esc)) < 0)
    {
      return local_reply(user_text);
    }

  persona_src = s_persona_ctx[0] ? s_persona_ctx
                                 : "你是openvela小助手，桌边陪伴助手，回答简洁温暖，不超过80字，用中文。";
  if (json_escape(persona_src, persona, sizeof(persona)) < 0)
    {
      return local_reply(user_text);
    }

  dm_chat_hist_json(hist, sizeof(hist));
  n = snprintf(body, sizeof(body),
               "{\"model\":\"%s\",\"messages\":["
               "{\"role\":\"system\",\"content\":\"%s\"},"
               "%s"
               "{\"role\":\"user\",\"content\":\"%s\"}],"
               "\"max_completion_tokens\":400,\"thinking\":{\"type\":\"disabled\"}}",
               MIMO_MODEL, persona, hist, esc);
  if (n <= 0 || n >= (int)sizeof(body))
    {
      return local_reply(user_text);
    }

  f = fopen(CHAT_REQ_PATH, "w");
  if (!f)
    {
      return local_reply(user_text);
    }
  fwrite(body, 1, (size_t)n, f);
  fclose(f);

  unlink(CHAT_RSP_PATH);
  /* Use the device-supported short options and reject HTTP error codes. */
  snprintf(cmd, sizeof(cmd),
           "curl -K " MIMO_CURL_CONFIG " -s -S -f -m 20 -X POST %s/chat/completions "
           "-H 'Content-Type: application/json' "
           "-d @%s -o %s",
           MIMO_BASE, CHAT_REQ_PATH, CHAT_RSP_PATH);
  if (run_curl_script(cmd) != 0)
    {
      strncpy(s_last_reply, local_reply(user_text),
              sizeof(s_last_reply) - 1);
      s_last_reply[sizeof(s_last_reply) - 1] = '\0';
      return s_last_reply;
    }

  extract_content(CHAT_RSP_PATH, s_last_reply, sizeof(s_last_reply));
  if (s_last_reply[0])
    {
      s_last_request_remote_ok = 1;
      dm_chat_hist_push(user_text, s_last_reply);
    }
  else
    {
      strncpy(s_last_reply, local_reply(user_text),
              sizeof(s_last_reply) - 1);
      s_last_reply[sizeof(s_last_reply) - 1] = '\0';
    }
  return s_last_reply;
}

const char *dm_chat_local_reply(const char *user_text)
{
  return local_reply(user_text);
}


/* ---- dictionary AI definition ---- */
int dm_dict_ai_define(const char *word, char *out, int cap)
{
  char esc[96];
  char body[700];
  char cmd[1100];
  FILE *f;
  int n;

  if (!word || !word[0] || !out || cap < 8)
    {
      return -1;
    }
  out[0] = '\0';
  if (!dm_wifi_connected() || dm_wifi_wait_time_sync(40) != 0)
    {
      return -1;
    }
  if (json_escape(word, esc, sizeof(esc)) < 0)
    {
      return -1;
    }
  n = snprintf(body, sizeof(body),
               "{\"model\":\"%s\",\"messages\":["
               "{\"role\":\"system\",\"content\":\"你是专业词典助手，用中文简洁解释单词，不超过100字。\"},"
               "{\"role\":\"user\",\"content\":\"解释单词 %s：词性、常见释义、一个例句。\"}],"
               "\"max_completion_tokens\":180,\"thinking\":{\"type\":\"disabled\"}}",
               MIMO_MODEL, esc);
  if (n <= 0 || n >= (int)sizeof(body))
    {
      return -1;
    }
  f = fopen(CHAT_REQ_PATH, "w");
  if (!f)
    {
      return -1;
    }
  fwrite(body, 1, (size_t)n, f);
  fclose(f);
  unlink(CHAT_RSP_PATH);
  snprintf(cmd, sizeof(cmd),
           "curl -K " MIMO_CURL_CONFIG " -s -S -f -m 18 -X POST %s/chat/completions "
           "-H 'Content-Type: application/json' "
           "-d @%s -o %s",
           MIMO_BASE, CHAT_REQ_PATH, CHAT_RSP_PATH);
  if (run_curl_script(cmd) != 0)
    {
      return -1;
    }
  extract_content(CHAT_RSP_PATH, out, (size_t)cap);
  return out[0] ? 0 : -1;
}

#endif /* CONFIG_DESKMATE_APP */
