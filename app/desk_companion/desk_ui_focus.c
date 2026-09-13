/****************************************************************************
 * desk_ui_focus.c — focus page: presets + custom minutes
 ****************************************************************************/

#include "desk_companion.h"
#include <stdio.h>

#ifdef CONFIG_DESK_COMPANION_APP

static lv_obj_t *g_timer_lbl;
static lv_obj_t *g_status_lbl;
static lv_obj_t *g_start_txt;
static lv_obj_t *g_step_box;
static lv_obj_t *g_step_val;
static lv_obj_t *g_preset_btns[5];
static const int32_t g_presets[4] = {15, 25, 45, 60};

static void paint_timer(void)
{
  char b[16];

  lv_snprintf(b, sizeof(b), "%02ld:%02ld", (long)(g_focus_left / 60),
              (long)(g_focus_left % 60));
  if (g_timer_lbl)
    lv_label_set_text(g_timer_lbl, b);

  if (g_timer_lbl)
    {
      uint32_t col = C_INK;
      if (g_focus_left == 0)
        col = C_OK;
      else if (!g_focus_run && g_focus_left < (int32_t)g_focus_selected_min * 60)
        col = C_DIM;
      lv_obj_set_style_text_color(g_timer_lbl, lv_color_hex(col), LV_PART_MAIN);
    }

  if (g_start_txt)
    {
      if (g_focus_left == 0)
        lv_label_set_text(g_start_txt, T("再来一段", "Again"));
      else if (g_focus_run)
        lv_label_set_text(g_start_txt, T("暂停", "Pause"));
      else
        lv_label_set_text(g_start_txt, T("继续", "Resume"));
    }
}

static void set_status(const char *zh, const char *en)
{
  if (g_status_lbl)
    lv_label_set_text(g_status_lbl, T(zh, en));
}

static void mark_preset(int idx)
{
  int i;

  for (i = 0; i < 5; i++)
    {
      if (!g_preset_btns[i])
        continue;
      if (i == idx)
        {
          lv_obj_set_style_bg_color(g_preset_btns[i], lv_color_hex(C_FACE),
                                    LV_PART_MAIN);
          lv_obj_set_style_text_color(g_preset_btns[i], lv_color_hex(C_EYE),
                                      LV_PART_MAIN);
        }
      else
        {
          lv_obj_set_style_bg_color(g_preset_btns[i], lv_color_hex(C_BTN),
                                    LV_PART_MAIN);
          lv_obj_set_style_text_color(g_preset_btns[i], lv_color_hex(C_MUTED),
                                      LV_PART_MAIN);
        }
    }
}

static void apply_minutes(int32_t min)
{
  g_focus_selected_min = (int)min;
  g_focus_left = min * 60;
  g_focus_run = false;
  if (g_step_val)
    {
      char b[8];
      lv_snprintf(b, sizeof(b), "%ld", (long)min);
      lv_label_set_text(g_step_val, b);
    }
  paint_timer();
}

static void preset_cb(lv_event_t *e)
{
  int idx = (int)(uintptr_t)lv_event_get_user_data(e);

  if (idx < 4)
    {
      g_focus_on_custom = false;
      if (g_step_box)
        lv_obj_add_flag(g_step_box, LV_OBJ_FLAG_HIDDEN);
      mark_preset(idx);
      apply_minutes(g_presets[idx]);
      set_status("选好时长，点开始", "Pick duration, tap start");
    }
  else
    {
      g_focus_on_custom = true;
      if (g_step_box)
        lv_obj_clear_flag(g_step_box, LV_OBJ_FLAG_HIDDEN);
      mark_preset(4);
      apply_minutes(g_focus_custom_min);
      set_status("用 − / + 调分钟", "Use - / + to adjust");
    }
}

static void step_cb(lv_event_t *e)
{
  int delta = (int)(uintptr_t)lv_event_get_user_data(e);

  g_focus_custom_min += delta * 5;
  if (g_focus_custom_min < 5)
    g_focus_custom_min = 5;
  if (g_focus_custom_min > 120)
    g_focus_custom_min = 120;
  apply_minutes(g_focus_custom_min);
  set_status("已调整时长", "Duration updated");
}

static void start_cb(lv_event_t *e)
{
  (void)e;
  if (g_focus_left == 0)
    apply_minutes(g_focus_selected_min);
  g_focus_run = !g_focus_run;
  if (g_focus_run)
    {
      set_status("专注中，别分心", "Focusing. Don't drift.");
      desk_set_face(FACE_FOCUS);
    }
  else
    {
      set_status("已暂停", "Paused");
      desk_set_face(FACE_IDLE);
    }
  paint_timer();
}

static void reset_cb(lv_event_t *e)
{
  (void)e;
  g_focus_run = false;
  apply_minutes(g_focus_selected_min);
  set_status("已重置", "Reset");
  desk_set_face(FACE_IDLE);
}

static lv_obj_t *mk_btn(lv_obj_t *p, const char *zh, const char *en, int w,
                        int h, uint32_t bg, uint32_t fg, lv_event_cb_t cb,
                        void *ud)
{
  lv_obj_t *b = lv_button_create(p);

  lv_obj_remove_style_all(b);
  lv_obj_set_size(b, w, h);
  lv_obj_set_style_radius(b, h / 2, LV_PART_MAIN);
  lv_obj_set_style_bg_color(b, lv_color_hex(bg), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(b, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  if (cb)
    lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
  {
    lv_obj_t *t = lbl(b, zh, en, g_font_s, fg);
    lv_obj_center(t);
  }
  return b;
}

void desk_update_focus_page(void)
{
  paint_timer();
}

void desk_create_focus(void)
{
  static const char *zh[] = {"15分", "25分", "45分", "60分", "自选"};
  static const char *en[] = {"15m", "25m", "45m", "60m", "Custom"};
  int x = 14;
  int i;
  lv_obj_t *minus;
  lv_obj_t *plus;
  lv_obj_t *rst;

  g_page_focus = desk_page_shell(g_root, C_BG);
  desk_backbar(g_page_focus, "专注", "Focus");

  g_timer_lbl = lv_label_create(g_page_focus);
  lv_label_set_text(g_timer_lbl, "25:00");
  style_f(g_timer_lbl, g_font_l, C_INK);
  lv_obj_set_style_text_font(g_timer_lbl, g_font_l, LV_PART_MAIN);
  lv_obj_align(g_timer_lbl, LV_ALIGN_TOP_MID, 0, 40);

  for (i = 0; i < 5; i++)
    {
      int w = (i == 4) ? 58 : 48;
      g_preset_btns[i] =
          mk_btn(g_page_focus, zh[i], en[i], w, 30, C_BTN, C_MUTED, preset_cb,
                 (void *)(uintptr_t)i);
      lv_obj_set_pos(g_preset_btns[i], x, 76);
      x += w + 6;
    }
  mark_preset(1);

  g_step_box = lv_obj_create(g_page_focus);
  lv_obj_set_size(g_step_box, 160, 40);
  lv_obj_set_pos(g_step_box, 80, 114);
  lv_obj_set_style_bg_opa(g_step_box, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(g_step_box, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_step_box, 0, LV_PART_MAIN);
  lv_obj_clear_flag(g_step_box, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(g_step_box, LV_OBJ_FLAG_HIDDEN);

  minus = mk_btn(g_step_box, "−", "-", 36, 36, C_BTN_HI, C_INK, step_cb,
                 (void *)(uintptr_t)-1);
  lv_obj_set_pos(minus, 0, 2);
  g_step_val = lbl(g_step_box, "30", "30", g_font_m, C_INK);
  lv_obj_align(g_step_val, LV_ALIGN_CENTER, 0, 0);
  plus = mk_btn(g_step_box, "+", "+", 36, 36, C_BTN_HI, C_INK, step_cb,
                (void *)(uintptr_t)1);
  lv_obj_set_pos(plus, 120, 2);

  {
    lv_obj_t *start = mk_btn(g_page_focus, "开始专注", "Start", 120, 40,
                             C_FACE, C_EYE, start_cb, NULL);
    lv_obj_set_pos(start, 50, 162);
    g_start_txt = lv_obj_get_child(start, 0);
  }

  rst = mk_btn(g_page_focus, "重置", "Reset", 70, 40, C_BTN, C_MUTED, reset_cb,
               NULL);
  lv_obj_set_pos(rst, 185, 162);

  g_status_lbl = lv_label_create(g_page_focus);
  lv_label_set_text(g_status_lbl, T("选好时长，点开始", "Pick duration, tap start"));
  style_f(g_status_lbl, g_font_s, C_MUTED);
  lv_obj_align(g_status_lbl, LV_ALIGN_BOTTOM_MID, 0, -10);

  g_focus_custom_min = 30;
  apply_minutes(25);
  mark_preset(1);
  set_status("选好时长，点开始", "Pick duration, tap start");
}

void desk_focus_tick(void)
{
  if (g_focus_run && g_focus_left > 0)
    {
      g_focus_left--;
      paint_timer();
    }
  if (g_focus_left == 0 && g_focus_run)
    {
      g_focus_run = false;
      paint_timer();
      set_status("专注完成，做得很好", "Focus done. Well done!");
      desk_set_face_hold(FACE_HAPPY, 30);
    }
}

#endif
