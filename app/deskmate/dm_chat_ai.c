/****************************************************************************
 * dm_chat_ai.c 鈥?MiMo cloud chat + local offline fallback
 ****************************************************************************/

#include "deskmate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#ifdef CONFIG_DESKMATE_APP

/* MiMo OpenAI-compatible endpoint (user-provided key for this device). */
#define MIMO_BASE "https://token-plan-sgp.xiaomimimo.com/v1"
#define MIMO_KEY  "YOUR_MIMO_API_KEY"
#define MIMO_MODEL "mimo-v2.5-pro"
#define CHAT_REQ_PATH "/tmp/dm_chat_req.json"
#define CHAT_RSP_PATH "/tmp/dm_chat_rsp.json"
#define CHAT_MAX_REPLY 240

static char s_last_reply[CHAT_MAX_REPLY];
static char s_persona_ctx[512];

static const char *const s_local_lines[] = {
  "鎴戝湪鍛€傛兂鑱婁粈涔堬紝鎴栬€呰涓嶈鍏堜笓娉ㄤ竴浼氬効锛?,
  "浠婂ぉ涔熻濂藉ソ鐓ч【鑷繁鍛€銆?,
  "璁颁綇锛氬畬鎴愭瘮瀹岀編鏇撮噸瑕併€?,
  "绱簡灏辨瓏涓€涓嬶紝鎴戦櫔浣犮€?,
  "瑕佷笉瑕佸枬鍙ｆ按锛熻韩浣撴槸鏈挶銆?,
  "浣犲凡缁忓緢妫掍簡锛岀户缁姞娌癸紒",
  "闇€瑕佹垜甯綘璁扮偣浠€涔堝悧锛熷幓澶囧繕鐪嬬湅銆?,
  "鐜?2048 鏀炬澗涓€涓嬩篃鍙互鍝︺€?,
};

static const char *local_reply(const char *text)
{
  if (!text || !text[0])
    {
      return s_local_lines[0];
    }

  if (strstr(text, "浣犲ソ") || strstr(text, "hi") || strstr(text, "hello") ||
      strstr(text, "鍡?))
    {
      return "浣犲ソ鍛€锛佹垜鏄?openvela灏忓姪鎵嬶紝妗岃竟鐨勫皬鎼。銆?;
    }
  if (strstr(text, "浣犳槸璋?) || strstr(text, "浠嬬粛"))
    {
      return "鎴戞槸 openvela灏忓姪鎵嬶細闄綘涓撴敞銆佽浜嬨€佽亰澶╃殑妗岄潰灏忎紮浼淬€?;
    }
  if (strstr(text, "绗戣瘽") || strstr(text, "鏃犺亰"))
    {
      return "涓轰粈涔堢▼搴忓憳鎬诲垎涓嶆竻涓囧湥鑺傚拰鍦ｈ癁鑺傦紵鍥犱负 Oct 31 == Dec 25銆?;
    }
  if (strstr(text, "鍔犳补") || strstr(text, "榧撳姳") || strstr(text, "绱?))
    {
      return "娣卞懠鍚搞€備綘涓嶉渶瑕佷竴娆″仛瀹屾墍鏈変簨锛屽厛鍋氫笅涓€浠跺氨濂姐€?;
    }
  if (strstr(text, "涓撴敞") || strstr(text, "宸ヤ綔") || strstr(text, "瀛︿範"))
    {
      return "瑕佷笉鏉ヤ竴娈典笓娉紵1 鍒嗛挓涔熷彲浠ワ紝寮€濮嬩簡灏变笉绠楁櫄銆?;
    }
  if (strstr(text, "澶╂皵"))
    {
      return "鎴戣繖杈规殏鏃剁湅涓嶄簡澶╂皵锛屼笉杩囪寰楀嚭闂ㄧ湅涓€鐪煎ぉ绌哄憖銆?;
    }
  if (strstr(text, "鍠濇按"))
    {
      return "濂戒富鎰忥紝鐜板湪灏卞幓鍠濅竴鍙ｆ按鍚с€?;
    }
  if (strstr(text, "鍚冭嵂") || strstr(text, "鑽?))
    {
      {
        const char *cur = dm_t("鍘汇€屽姛鑳?鈫?鍚冭嵂銆嶇湅鐪嬩粖澶╀换鍔°€?,
                               "Check Features 鈫?Meds for today.");
        return cur;
      }
    }
  if (strstr(text, "2048") || strstr(text, "娓告垙") || strstr(text, "鐜?))
    {
      return "鍔熻兘閲屾湁 2048锛屾粦涓€婊戝緢瑙ｅ帇銆傜帺瀹岃寰楀洖鏉ヤ笓娉ㄥ摝銆?;
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

static void extract_content(const char *path, char *out, size_t cap)
{
  FILE *f;
  char *buf;
  long sz;
  char *p;
  char *q;

  out[0] = '\0';
  f = fopen(path, "rb");
  if (!f)
    {
      return;
    }
  fseek(f, 0, SEEK_END);
  sz = ftell(f);
  fseek(f, 0, SEEK_SET);
  if (sz <= 0 || sz > 64 * 1024)
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

  p = strstr(buf, "\"content\"");
  if (!p)
    {
      free(buf);
      return;
    }
  p = strchr(p + 9, '"');
  if (!p)
    {
      free(buf);
      return;
    }
  q = strchr(p + 1, '"');
  if (!q)
    {
      free(buf);
      return;
    }
  {
    size_t n = (size_t)(q - (p + 1));
    if (n >= cap)
      {
        n = cap - 1;
      }
    memcpy(out, p + 1, n);
    out[n] = '\0';
    /* unescape \" and \\ and \n lightly */
    {
      char *r = out;
      char *w = out;
      while (*r)
        {
          if (r[0] == '\\' && r[1] == 'n')
            {
              *w++ = ' ';
              r += 2;
            }
          else if (r[0] == '\\' && r[1] == '"')
            {
              *w++ = '"';
              r += 2;
            }
          else if (r[0] == '\\' && r[1] == '\\')
            {
              *w++ = '\\';
              r += 2;
            }
          else
            {
              *w++ = *r++;
            }
        }
      *w = '\0';
    }
  }
  free(buf);
}

int dm_chat_ai_available(void)
{
  return dm_wifi_connected() ? 1 : 0;
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
  char body[900];
  char cmd[1200];
  FILE *f;
  int n;

  s_last_reply[0] = '\0';
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

  if (json_escape(user_text, esc, sizeof(esc)) < 0)
    {
      return local_reply(user_text);
    }

  n = snprintf(body, sizeof(body),
               "{\"model\":\"%s\",\"messages\":["
               "{\"role\":\"system\",\"content\":\"%s\"},"
               "{\"role\":\"user\",\"content\":\"%s\"}],"
               "\"temperature\":0.7,\"max_tokens\":200}",
               MIMO_MODEL,
               s_persona_ctx[0] ? s_persona_ctx
                                 : "浣犳槸openvela灏忓姪鎵嬶紝妗岃竟闄即鍔╂墜锛屽洖绛旂畝娲佹俯鏆栵紝"
                                   "涓嶈秴杩?0瀛楋紝鐢ㄤ腑鏂囥€?,
               esc);
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
  snprintf(cmd, sizeof(cmd),
           "curl -sS -m 20 -X POST \"%s/chat/completions\" "
           "-H \"Authorization: Bearer %s\" "
           "-H \"Content-Type: application/json\" "
           "-d @%s -o %s",
           MIMO_BASE, MIMO_KEY, CHAT_REQ_PATH, CHAT_RSP_PATH);
  if (system(cmd) != 0)
    {
      strncpy(s_last_reply, local_reply(user_text),
              sizeof(s_last_reply) - 1);
      s_last_reply[sizeof(s_last_reply) - 1] = '\0';
      return s_last_reply;
    }

  extract_content(CHAT_RSP_PATH, s_last_reply, sizeof(s_last_reply));
  if (!s_last_reply[0])
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

#endif /* CONFIG_DESKMATE_APP */
