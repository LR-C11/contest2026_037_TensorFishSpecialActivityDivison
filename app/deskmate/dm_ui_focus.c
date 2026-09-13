/****************************************************************************
 * dm_ui_focus.c — focus home (duration) + countdown run page
 ****************************************************************************/

#include "deskmate.h"
#include <stdio.h>

#ifdef CONFIG_DESKMATE_APP

static lv_obj_t *s_home_status;
static lv_obj_t *s_home_bubble;
static lv_obj_t *s_step_box;
static lv_obj_t *s_step_val;
static lv_obj_t *s_preset_btns[5];

static lv_obj_t *s_run_bubble;
static lv_obj_t *s_run_status;
static lv_obj_t *s_run_pause_btn;
static lv_obj_t *s_run_pause_lbl;
static lv_obj_t *s_start_btn;
static dm_clock_t s_clock;
static bool s_clock_ready;

static const int32_t s_presets[4] = { 15, 25, 45, 60 };

static void paint_home_status(void)
{
  char b[48];
  if (!s_home_status)
    {
      return;
    }
  lv_snprintf(b, sizeof(b), "%s · %ld min", dm_t("已选", "Selected"),
              (long)g_dm.focus_sel_min);
  lv_label_set_text(s_home_status, b);
}

static void mark_preset(int idx)
{
  int i;
  for (i = 0; i < 5; i++)
    {
      lv_obj_t *lab;
      if (!s_preset_btns[i])
        {
          continue;
        }
      lv_obj_set_style_bg_color(
          s_preset_btns[i],
          lv_color_hex(i == idx ? C_FACE : C_BTN), LV_PART_MAIN);
      lab = lv_obj_get_child(s_preset_btns[i], 0);
      if (lab)
        {
          lv_obj_set_style_text_color(
              lab, lv_color_hex(i == idx ? C_EYE : C_MUTED), LV_PART_MAIN);
        }
    }
}

static void apply_minutes(int32_t min)
{
  if (min < 1)
    {
      min = 1;
    }
  if (min > 120)
    {
      min = 120;
    }
  g_dm.focus_sel_min = min;
  g_dm.focus_total = min * 60;
  g_dm.focus_left = min * 60;
  g_dm.focus_run = false;
  g_dm.focus_finished = false;
  if (s_step_val)
    {
      char b[8];
      lv_snprintf(b, sizeof(b), "%ld", (long)min);
      lv_label_set_text(s_step_val, b);
    }
  paint_home_status();
  if (s_clock_ready)
    {
      dm_clock_set(&s_clock, g_dm.focus_left);
    }
}

static void preset_cb(lv_event_t *e)
{
  int idx = (int)(uintptr_t)lv_event_get_user_data(e);
  if (idx < 4)
    {
      if (s_step_box)
        {
          lv_obj_add_flag(s_step_box, LV_OBJ_FLAG_HIDDEN);
        }
      if (s_start_btn)
        {
          lv_obj_set_width(s_start_btn, 140);
          lv_obj_align(s_start_btn, LV_ALIGN_TOP_MID, 0, 154);
        }
      mark_preset(idx);
      apply_minutes(s_presets[idx]);
      g_dm.focus_custom_min = g_dm.focus_sel_min;
    }
  else
    {
      if (s_step_box)
        {
          lv_obj_clear_flag(s_step_box, LV_OBJ_FLAG_HIDDEN);
        }
      if (s_start_btn)
        {
          lv_obj_set_width(s_start_btn, 100);
          lv_obj_align(s_start_btn, LV_ALIGN_TOP_MID, 50, 154);
        }
      mark_preset(4);
      apply_minutes(g_dm.focus_custom_min);
    }
}

static void step_cb(lv_event_t *e)
{
  int delta = (int)(uintptr_t)lv_event_get_user_data(e);
  g_dm.focus_custom_min += delta * 5;
  if (g_dm.focus_custom_min < 5)
    {
      g_dm.focus_custom_min = 5;
    }
  if (g_dm.focus_custom_min > 120)
    {
      g_dm.focus_custom_min = 120;
    }
  apply_minutes(g_dm.focus_custom_min);
  mark_preset(4);
}

void dm_update_run_clock(void)
{
  if (!s_clock_ready)
    {
      return;
    }
  dm_clock_set(&s_clock, g_dm.focus_left);
}

void dm_focus_start(void)
{
  static const char *zh[] = {
    "开始专注，我陪着你",
    "好，计时开始",
    "冲，我在旁边",
  };
  static const char *en[] = {
    "Focus started. I'm with you.",
    "Okay, clock is running.",
    "Go — I'm right here.",
  };
  static int n;

  g_dm.focus_left = g_dm.focus_total;
  if (g_dm.focus_left <= 0)
    {
      g_dm.focus_left = g_dm.focus_sel_min * 60;
      g_dm.focus_total = g_dm.focus_left;
    }
  g_dm.focus_run = true;
  g_dm.focus_finished = false;

  dm_face_attach(g_dm_pages[PAGE_FOCUS_RUN], (DM_SCR_W - 84) / 2, 28);
  if (g_dm_face)
    {
      lv_obj_set_size(g_dm_face, 84, 84);
    }
  dm_face_set(FACE_FOCUS, EYE_DECOR_WIDE, 6);
  g_dm_bubble = s_run_bubble;
  dm_say(zh[n % 3], en[n % 3]);
  n++;
  if (s_run_status)
    {
      lv_label_set_text(s_run_status, dm_t("专注中", "Focusing"));
    }
  if (s_run_pause_lbl)
    {
      lv_label_set_text(s_run_pause_lbl, dm_t("暂停", "Pause"));
    }
  dm_update_run_clock();
  dm_show(PAGE_FOCUS_RUN);
}

void dm_focus_end(void)
{
  int32_t done = (g_dm.focus_total - g_dm.focus_left) / 60;

  if (done > 0)
    {
      dm_health_add_focus_min(done);
      dm_health_refresh();
    }

  g_dm.focus_run = false;
  g_dm.focus_finished = false;
  g_dm.focus_left = g_dm.focus_total;
  dm_face_attach(g_dm_pages[PAGE_FOCUS_HOME], (DM_SCR_W - 72) / 2, 28);
  if (g_dm_face)
    {
      lv_obj_set_size(g_dm_face, 72, 72);
    }
  dm_face_set(FACE_IDLE, EYE_DECOR_NONE, 0);
  g_dm_bubble = s_home_bubble;
  dm_say("已结束，休息一下", "Session ended. Rest a bit.");
  dm_show(PAGE_FOCUS_HOME);
  paint_home_status();
}

void dm_focus_toggle_pause(void)
{
  if (g_dm.focus_finished)
    {
      /* restart same duration */
      apply_minutes(g_dm.focus_sel_min);
      dm_focus_start();
      return;
    }

  g_dm.focus_run = !g_dm.focus_run;
  if (s_run_pause_lbl)
    {
      lv_label_set_text(s_run_pause_lbl,
                        g_dm.focus_run ? dm_t("暂停", "Pause")
                                       : dm_t("继续", "Resume"));
    }
  if (s_run_status)
    {
      lv_label_set_text(s_run_status,
                        g_dm.focus_run ? dm_t("专注中", "Focusing")
                                       : dm_t("已暂停", "Paused"));
    }
  if (g_dm.focus_run)
    {
      static const char *zh[] = { "继续冲", "回来啦，继续", "好，接着来" };
      static const char *en[] = { "Back to it", "Welcome back", "On again" };
      static int k;
      dm_face_set(FACE_FOCUS, EYE_DECOR_NONE, 0);
      dm_say(zh[k % 3], en[k % 3]);
      k++;
    }
  else
    {
      static const char *zh[] = {
        "先暂停，准备好了再继续",
        "歇一下也行，我等你",
        "暂停中，不着急",
      };
      static const char *en[] = {
        "Paused. Ready when you are.",
        "Take a beat. I'll wait.",
        "Paused — no rush.",
      };
      static int k;
      dm_face_set(FACE_IDLE, EYE_DECOR_NONE, 0);
      dm_say(zh[k % 3], en[k % 3]);
      k++;
    }
}

static void start_cb(lv_event_t *e)
{
  (void)e;
  dm_focus_start();
}

static void pause_cb(lv_event_t *e)
{
  (void)e;
  dm_focus_toggle_pause();
}

static void end_cb(lv_event_t *e)
{
  (void)e;
  dm_focus_end();
}

static lv_obj_t *mk_page(lv_obj_t *parent, dm_page_t id)
{
  lv_obj_t *page = lv_obj_create(parent);
  lv_obj_set_size(page, DM_SCR_W, DM_SCR_H);
  lv_obj_set_pos(page, 0, 0);
  lv_obj_set_style_bg_color(page, lv_color_hex(C_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(page, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(page, 0, LV_PART_MAIN);
  lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
  g_dm_pages[id] = page;
  return page;
}

void dm_create_focus_home(void)
{
  static const char *zh[] = { "15分", "25分", "45分", "60分", "自选" };
  static const char *en[] = { "15m", "25m", "45m", "60m", "Custom" };
  lv_obj_t *page = mk_page(g_dm_root, PAGE_FOCUS_HOME);
  lv_obj_t *title = dm_lbl(page, "专注", "Focus", g_dm_font_m, C_INK);
  lv_obj_t *minus;
  lv_obj_t *plus;
  int x = 18;
  int i;

  lv_obj_set_pos(title, 12, 8);

  s_home_status = lv_label_create(page);
  lv_label_set_text(s_home_status, "Selected · 25 min");
  dm_style(s_home_status, g_dm_font_s, C_MUTED);
  lv_obj_align(s_home_status, LV_ALIGN_TOP_RIGHT, -12, 12);

  dm_face_build(page, (DM_SCR_W - 68) / 2, 24, 68);

  s_home_bubble = lv_label_create(page);
  lv_label_set_long_mode(s_home_bubble, LV_LABEL_LONG_DOT);
  lv_obj_set_width(s_home_bubble, 300);
  lv_label_set_text(s_home_bubble,
                    dm_t("今天想专注多久？点脸可以互动",
                         "How long today? Tap the face"));
  dm_style(s_home_bubble, g_dm_font_s, C_DIM);
  lv_obj_set_style_text_align(s_home_bubble, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN);
  lv_obj_align(s_home_bubble, LV_ALIGN_TOP_MID, 0, 98);
  g_dm_bubble = s_home_bubble;

  for (i = 0; i < 5; i++)
    {
      int w = (i == 4) ? 58 : 48;
      s_preset_btns[i] =
          dm_btn(page, zh[i], en[i], w, 28, C_BTN, C_MUTED, preset_cb,
                 (void *)(uintptr_t)i);
      lv_obj_set_pos(s_preset_btns[i], x, 118);
      x += w + 6;
    }

  s_step_box = lv_obj_create(page);
  lv_obj_set_size(s_step_box, 150, 34);
  lv_obj_set_pos(s_step_box, 14, 154);
  lv_obj_set_style_bg_opa(s_step_box, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_step_box, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_step_box, 0, LV_PART_MAIN);
  lv_obj_clear_flag(s_step_box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(s_step_box, LV_OBJ_FLAG_HIDDEN);

  minus = dm_btn(s_step_box, "-", "-", 32, 32, C_BTN_HI, C_INK, step_cb,
                 (void *)(uintptr_t)-1);
  lv_obj_set_pos(minus, 0, 1);
  s_step_val = dm_lbl(s_step_box, "30", "30", g_dm_font_m, C_INK);
  lv_obj_align(s_step_val, LV_ALIGN_CENTER, 0, 0);
  plus = dm_btn(s_step_box, "+", "+", 32, 32, C_BTN_HI, C_INK, step_cb,
                (void *)(uintptr_t)1);
  lv_obj_set_pos(plus, 118, 1);

  s_start_btn = dm_btn(page, "开始专注", "Start focus", 140, 34, C_FACE,
                       C_EYE, start_cb, NULL);
  lv_obj_align(s_start_btn, LV_ALIGN_TOP_MID, 0, 154);

  mark_preset(1);
  apply_minutes(25);
}

void dm_create_focus_run(void)
{
  lv_obj_t *page = mk_page(g_dm_root, PAGE_FOCUS_RUN);
  lv_obj_t *title = dm_lbl(page, "倒计时", "Countdown", g_dm_font_s, C_MUTED);
  lv_obj_t *endb;

  lv_obj_set_pos(title, 12, 8);

  endb = dm_btn(page, "结束", "End", 52, 26, C_BTN, C_MUTED, end_cb, NULL);
  lv_obj_set_pos(endb, 256, 6);

  /* shared face is attached when entering this page */

  s_run_bubble = lv_label_create(page);
  lv_label_set_long_mode(s_run_bubble, LV_LABEL_LONG_DOT);
  lv_obj_set_width(s_run_bubble, 300);
  lv_label_set_text(s_run_bubble, dm_t("准备好就开始", "Ready when you are"));
  dm_style(s_run_bubble, g_dm_font_s, C_INK);
  lv_obj_set_style_text_align(s_run_bubble, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN);
  lv_obj_align(s_run_bubble, LV_ALIGN_TOP_MID, 0, 116);

  s_run_status = dm_lbl(page, "专注中", "Focusing", g_dm_font_s, C_DIM);
  lv_obj_set_style_text_align(s_run_status, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN);
  lv_obj_align(s_run_status, LV_ALIGN_TOP_MID, 0, 134);

  dm_clock_create(&s_clock, page, 30, 148, 260, 50, g_dm_font_xl);
  s_clock_ready = true;
  dm_clock_set(&s_clock, g_dm.focus_left);

  s_run_pause_btn = dm_btn(page, "暂停", "Pause", 110, 34, C_BTN, C_INK,
                           pause_cb, NULL);
  lv_obj_align(s_run_pause_btn, LV_ALIGN_TOP_MID, 0, 204);
  s_run_pause_lbl = lv_obj_get_child(s_run_pause_btn, 0);
}

void dm_focus_home_tick(void)
{
}

void dm_focus_run_tick(void)
{
  static int s_sec_acc;

  if (!(g_dm.focus_run && !g_dm.focus_finished && g_dm.focus_left > 0))
    {
      s_sec_acc = 0;
      return;
    }

  /* UI tick is 250ms — only count down once per real second */
  s_sec_acc++;
  if (s_sec_acc < DM_TICKS_PER_SEC)
    {
      return;
    }
  s_sec_acc = 0;

  g_dm.focus_left--;
  if (g_dm.page == PAGE_FOCUS_RUN)
    {
      dm_update_run_clock();
    }

  if (g_dm.focus_left == 0)
    {
      g_dm.focus_run = false;
      g_dm.focus_finished = true;
      dm_health_add_focus_min(g_dm.focus_total / 60);
      dm_health_refresh();
      dm_face_set(FACE_DONE, EYE_DECOR_HEART, 20);
      if (g_dm.page == PAGE_FOCUS_RUN)
        {
          dm_say("专注完成，做得很好", "Done. Well done!");
          if (s_run_status)
            {
              lv_label_set_text(s_run_status, dm_t("完成", "Complete"));
            }
          if (s_run_pause_lbl)
            {
              lv_label_set_text(s_run_pause_lbl, dm_t("再来一段", "Again"));
            }
        }
    }
  else if (g_dm.focus_left == 60 && g_dm.page == PAGE_FOCUS_RUN)
    {
      dm_face_set(FACE_FOCUS, EYE_DECOR_STAR, 6);
      dm_say("还有一分钟", "One minute left");
    }
}

#endif /* CONFIG_DESKMATE_APP */
