/****************************************************************************
 * dm_ui_chat.c — Chat: mascot + text + voice (simple)
 ****************************************************************************/

#include "deskmate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef CONFIG_DESKMATE_APP

#define CHAT_MSG_MAX 2
#define CHAT_LINE_MAX 80

typedef struct
{
  int who; /* 0 user, 1 mate */
  char text[CHAT_LINE_MAX];
} chat_msg_t;

static chat_msg_t s_msgs[CHAT_MSG_MAX];
static int s_msg_n;
static lv_obj_t *s_log;
static lv_obj_t *s_status;
static lv_obj_t *s_ptt_btn;
static lv_obj_t *s_inp;
static bool s_ptt_on;
static bool s_busy;
static lv_timer_t *s_ai_timer;
static char s_pending[CHAT_LINE_MAX];

static void refresh_log(void)
{
  char blob[CHAT_MSG_MAX * (CHAT_LINE_MAX + 12)];
  size_t o = 0;
  int i;

  blob[0] = '\0';
  for (i = 0; i < s_msg_n; i++)
    {
      int n = snprintf(blob + o, sizeof(blob) - o, "%s%s\n",
                       s_msgs[i].who ? "小M: " : "你: ", s_msgs[i].text);
      if (n <= 0)
        {
          break;
        }
      o += (size_t)n;
      if (o >= sizeof(blob))
        {
          break;
        }
    }

  if (s_log)
    {
      lv_label_set_text(s_log, blob[0] ? blob
                                       : dm_t("还没有对话",
                                              "No messages yet"));
    }
}

static void push_msg(int who, const char *text)
{
  if (!text)
    {
      return;
    }
  if (s_msg_n >= CHAT_MSG_MAX)
    {
      memmove(&s_msgs[0], &s_msgs[1],
              sizeof(chat_msg_t) * (CHAT_MSG_MAX - 1));
      s_msg_n = CHAT_MSG_MAX - 1;
    }
  s_msgs[s_msg_n].who = who;
  strncpy(s_msgs[s_msg_n].text, text, CHAT_LINE_MAX - 1);
  s_msgs[s_msg_n].text[CHAT_LINE_MAX - 1] = '\0';
  s_msg_n++;
  refresh_log();
}

static void set_status(const char *zh, const char *en)
{
  if (s_status)
    {
      lv_label_set_text(s_status, dm_t(zh, en));
    }
}

static void ai_worker(lv_timer_t *t)
{
  const char *reply;
  int online;

  (void)t;
  if (!s_busy)
    {
      return;
    }

  online = dm_chat_ai_available();
  reply = dm_chat_ai_ask(s_pending);
  push_msg(1, reply ? reply : "……");
  set_status(online ? "已回复" : "离线回复",
             online ? "Replied" : "Offline reply");
  dm_face_set(FACE_HAPPY, EYE_DECOR_NONE, 6);
  s_busy = false;
  if (s_ai_timer)
    {
      lv_timer_pause(s_ai_timer);
    }
}

static void send_text(const char *text)
{
  if (!text || !text[0] || s_busy)
    {
      return;
    }

  push_msg(0, text);
  strncpy(s_pending, text, sizeof(s_pending) - 1);
  s_pending[sizeof(s_pending) - 1] = '\0';
  s_busy = true;
  dm_face_set(FACE_THINK, EYE_DECOR_NONE, 8);
  set_status(dm_chat_ai_available() ? "MiMo 思考中…" : "思考中…",
             dm_chat_ai_available() ? "MiMo thinking…" : "Thinking…");

  if (!s_ai_timer)
    {
      s_ai_timer = lv_timer_create(ai_worker, 80, NULL);
      lv_timer_pause(s_ai_timer);
    }
  lv_timer_resume(s_ai_timer);
}

static void send_from_input(void)
{
  const char *txt;

  if (!s_inp)
    {
      return;
    }
  txt = lv_textarea_get_text(s_inp);
  if (!txt || !txt[0])
    {
      return;
    }
  send_text(txt);
  lv_textarea_set_text(s_inp, "");
}

static void face_click(lv_event_t *e)
{
  (void)e;
  dm_face_set(FACE_HAPPY, EYE_DECOR_NONE, 6);
  push_msg(1, dm_t("我在。想聊就说话，或者打字。",
                   "I'm here. Speak or type."));
  set_status("点脸有回应", "Tap face");
}

static void ptt_press(lv_event_t *e)
{
  (void)e;
  if (s_busy)
    {
      return;
    }
  s_ptt_on = true;
  if (s_ptt_btn)
    {
      lv_obj_set_style_bg_color(s_ptt_btn, lv_color_hex(C_HEART),
                                LV_PART_MAIN);
    }
  dm_face_set(FACE_THINK, EYE_DECOR_NONE, 8);
  set_status("松开发送…", "Release to send…");
}

static void ptt_release(lv_event_t *e)
{
  (void)e;
  if (!s_ptt_on)
    {
      return;
    }
  s_ptt_on = false;
  if (s_ptt_btn)
    {
      lv_obj_set_style_bg_color(s_ptt_btn, lv_color_hex(C_BTN),
                                LV_PART_MAIN);
    }

  if (access("/dev/audio/pcm0c", F_OK) == 0)
    {
      send_text(dm_t("（语音）你好", "(voice) hello"));
    }
  else
    {
      set_status("无麦克风，请打字", "No mic — type instead");
      dm_face_set(FACE_IDLE, EYE_DECOR_NONE, 0);
      /* tap the textarea on screen; LVGL on this tree has no lv_obj_focus */
    }
}

static void send_cb(lv_event_t *e)
{
  (void)e;
  send_from_input();
}

static void clear_cb(lv_event_t *e)
{
  (void)e;
  s_msg_n = 0;
  s_busy = false;
  memset(s_pending, 0, sizeof(s_pending));
  if (s_inp)
    {
      lv_textarea_set_text(s_inp, "");
    }
  refresh_log();
  dm_face_set(FACE_IDLE, EYE_DECOR_NONE, 0);
  set_status("已清空", "Cleared");
}

void dm_create_chat(void)
{
  lv_obj_t *page = lv_obj_create(g_dm_root);
  lv_obj_t *title;
  lv_obj_t *panel;
  lv_obj_t *b;

  lv_obj_set_size(page, DM_SCR_W, DM_SCR_H);
  lv_obj_set_pos(page, 0, 0);
  lv_obj_set_style_bg_color(page, lv_color_hex(C_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(page, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(page, 0, LV_PART_MAIN);
  lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
  g_dm_pages[PAGE_CHAT] = page;

  title = dm_lbl(page, "聊天", "Chat", g_dm_font_m, C_INK);
  lv_obj_set_pos(title, 12, 6);

  s_status = dm_lbl(page, "点脸 · 打字 · 语音", "Tap / type / voice",
                    g_dm_font_s, C_DIM);
  lv_obj_set_pos(s_status, 70, 10);

  dm_face_build(page, (DM_SCR_W - 56) / 2, 32, 56);
  if (g_dm_face)
    {
      lv_obj_set_size(g_dm_face, 56, 56);
      lv_obj_add_event_cb(g_dm_face, face_click, LV_EVENT_CLICKED, NULL);
    }

  panel = lv_obj_create(page);
  lv_obj_set_size(panel, 296, 78);
  lv_obj_set_pos(panel, 12, 96);
  lv_obj_set_style_bg_color(panel, lv_color_hex(0x111111), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(panel, 12, LV_PART_MAIN);
  lv_obj_set_style_border_width(panel, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(panel, 8, LV_PART_MAIN);
  lv_obj_set_scroll_dir(panel, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_AUTO);

  s_log = dm_lbl(panel, "还没有对话", "No messages yet", g_dm_font_s,
                 C_INK);
  lv_label_set_long_mode(s_log, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(s_log, 272);

  s_inp = lv_textarea_create(page);
  lv_obj_set_size(s_inp, 170, 34);
  lv_obj_set_pos(s_inp, 12, 182);
  lv_obj_set_style_bg_color(s_inp, lv_color_hex(0x111111), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_inp, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_text_color(s_inp, lv_color_hex(C_INK), LV_PART_MAIN);
  lv_obj_set_style_radius(s_inp, 10, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_inp, 0, LV_PART_MAIN);
  lv_textarea_set_one_line(s_inp, true);
  lv_textarea_set_max_length(s_inp, 40);
  lv_textarea_set_placeholder_text(s_inp,
                                   dm_t("说点什么…", "Say something"));

  b = dm_btn(page, "发送", "Send", 52, 34, C_STAR, C_EYE, send_cb, NULL);
  lv_obj_set_pos(b, 188, 182);

  b = dm_btn(page, "清空", "Clear", 52, 34, C_BTN, C_MUTED, clear_cb, NULL);
  lv_obj_set_pos(b, 246, 182);

  s_ptt_btn = dm_btn(page, "按住说话", "Hold to talk", 296, 30, C_BTN,
                     C_ACCENT, ptt_press, NULL);
  lv_obj_set_pos(s_ptt_btn, 12, 222);
  lv_obj_add_event_cb(s_ptt_btn, ptt_release, LV_EVENT_RELEASED, NULL);
  lv_obj_add_event_cb(s_ptt_btn, ptt_release, LV_EVENT_PRESS_LOST, NULL);

  s_msg_n = 0;
  s_busy = false;
  refresh_log();
}

void dm_chat_tick(void)
{
}

#endif /* CONFIG_DESKMATE_APP */
