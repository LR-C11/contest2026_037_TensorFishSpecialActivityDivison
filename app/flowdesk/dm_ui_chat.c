/****************************************************************************
 * dm_ui_chat.c — Chat UI: one face + answer + hold-to-talk only
 *
 * Layout (dock top = 194):
 *   y=4    title
 *   y=32   mascot 76px centered
 *   y=120  latest answer (wrapped)
 *   y=168  PTT  (ends 192, above dock)
 ****************************************************************************/

#include "deskmate.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef CONFIG_DESKMATE_APP

#define CHAT_LINE_MAX 80
#define CHAT_FACE_SZ 76

static lv_obj_t *s_ans;
static lv_obj_t *s_ptt_btn;
static bool s_ptt_on;
static bool s_busy;
static lv_timer_t *s_ai_timer;
static char s_pending[CHAT_LINE_MAX];
static char s_last_ans[CHAT_LINE_MAX];

static void set_ans(const char *text)
{
  if (!text)
    {
      text = "";
    }
  strncpy(s_last_ans, text, sizeof(s_last_ans) - 1);
  s_last_ans[sizeof(s_last_ans) - 1] = '\0';
  if (s_ans)
    {
      lv_label_set_text(s_ans, s_last_ans[0] ? s_last_ans
                                             : dm_t("点我，或按住说话",
                                                    "Tap me or hold to talk"));
    }
}

static void ai_worker(lv_timer_t *t)
{
  const char *reply;

  (void)t;
  if (!s_busy)
    {
      return;
    }

  reply = dm_chat_ai_ask(s_pending);
  set_ans(reply ? reply : "……");
  dm_face_set(FACE_HAPPY, EYE_DECOR_NONE, 8);
  s_busy = false;
  if (s_ai_timer)
    {
      lv_timer_pause(s_ai_timer);
    }
}

static void ask_ai(const char *text)
{
  if (!text || !text[0] || s_busy)
    {
      return;
    }

  strncpy(s_pending, text, sizeof(s_pending) - 1);
  s_pending[sizeof(s_pending) - 1] = '\0';
  s_busy = true;
  dm_face_set(FACE_THINK, EYE_DECOR_NONE, 10);
  set_ans(dm_t("思考中…", "Thinking…"));

  if (!s_ai_timer)
    {
      s_ai_timer = lv_timer_create(ai_worker, 80, NULL);
      lv_timer_pause(s_ai_timer);
    }
  lv_timer_resume(s_ai_timer);
}

static void face_click(lv_event_t *e)
{
  (void)e;
  if (s_busy)
    {
      return;
    }
  dm_face_set(FACE_HAPPY, EYE_DECOR_HEART, 8);
  ask_ai(dm_t("陪我聊聊天", "Chat with me"));
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
  set_ans(dm_t("我在听…", "Listening…"));
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
      ask_ai(dm_t("（语音）你好", "(voice) hello"));
    }
  else
    {
      /* no mic: still demo the answer path */
      ask_ai(dm_t("你好", "hello"));
    }
}

void dm_create_chat(void)
{
  lv_obj_t *page = lv_obj_create(g_dm_root);
  lv_obj_t *title;

  lv_obj_set_size(page, DM_SCR_W, DM_SCR_H);
  lv_obj_set_pos(page, 0, 0);
  lv_obj_set_style_bg_color(page, lv_color_hex(C_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(page, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(page, 0, LV_PART_MAIN);
  lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
  g_dm_pages[PAGE_CHAT] = page;

  title = dm_lbl(page, "聊天", "Chat", g_dm_font_m, C_INK);
  lv_obj_set_pos(title, 12, 4);

  /* one face, center */
  dm_face_build(page, (DM_SCR_W - CHAT_FACE_SZ) / 2, 32, CHAT_FACE_SZ);
  if (g_dm_face)
    {
      lv_obj_set_size(g_dm_face, CHAT_FACE_SZ, CHAT_FACE_SZ);
      lv_obj_add_event_cb(g_dm_face, face_click, LV_EVENT_CLICKED, NULL);
    }

  /* answer under face */
  s_ans = dm_lbl(page, "点我，或按住说话", "Tap me or hold to talk",
                 g_dm_font_s, C_INK);
  lv_label_set_long_mode(s_ans, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(s_ans, 288);
  lv_obj_set_style_text_align(s_ans, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_pos(s_ans, 16, 120);

  /* only control: hold to talk */
  s_ptt_btn = dm_btn(page, "按住说话", "Hold to talk", 296, 26, C_BTN,
                     C_ACCENT, ptt_press, NULL);
  lv_obj_set_pos(s_ptt_btn, 12, 166);
  lv_obj_add_event_cb(s_ptt_btn, ptt_release, LV_EVENT_RELEASED, NULL);
  lv_obj_add_event_cb(s_ptt_btn, ptt_release, LV_EVENT_PRESS_LOST, NULL);

  s_busy = false;
  s_ptt_on = false;
  s_last_ans[0] = '\0';
}

void dm_chat_tick(void)
{
}

#endif /* CONFIG_DESKMATE_APP */
