/****************************************************************************
 * desk_ui_home.c — black bg, centered white face, bottom dock
 ****************************************************************************/

#include "desk_companion.h"
#include <stdint.h>

#ifdef CONFIG_DESK_COMPANION_APP

void desk_create_face_widgets(lv_obj_t *face);
void desk_face_on_click(void);

static void face_cb(lv_event_t *e)
{
  (void)e;
  desk_face_press_pulse();
  desk_face_on_click();
}

static void dock_cb(lv_event_t *e)
{
  desk_page_t p = (desk_page_t)(uintptr_t)lv_event_get_user_data(e);
  desk_show(p);
}

static lv_obj_t *dock_btn(lv_obj_t *parent, const char *zh, const char *en,
                          lv_event_cb_t cb, void *ud)
{
  lv_obj_t *b = lv_button_create(parent);
  lv_obj_remove_style_all(b);
  lv_obj_set_size(b, 68, 36);
  lv_obj_set_style_radius(b, 10, LV_PART_MAIN);
  lv_obj_set_style_bg_color(b, lv_color_hex(C_BTN), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(b, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(b, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(b, 0, LV_PART_MAIN);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);
  lv_obj_t *t = lbl(b, zh, en, g_font_s, C_DIM);
  lv_obj_center(t);
  return b;
}

static void focus_click(lv_event_t *e)
{
  (void)e;
  desk_show(PAGE_FOCUS);
}

void desk_update_focus_ui(void)
{
  /* home no longer shows focus timer; page does */
}

void desk_create_home(void)
{
  g_page_home = desk_page_shell(g_root, C_BG);
  lv_obj_clear_flag(g_page_home, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(g_page_home);

  /* top bar */
  lv_obj_t *brand = lbl(g_page_home, "DeskMate", "DeskMate", g_font_s,
                        C_MUTED);
  lv_obj_set_pos(brand, 12, 8);
  g_status_txt = lbl(g_page_home, "—", "—", g_font_s, C_MUTED);
  lv_obj_align(g_status_txt, LV_ALIGN_TOP_RIGHT, -12, 8);

  /* centered stage */
  lv_obj_t *stage = lv_obj_create(g_page_home);
  lv_obj_set_size(stage, 320, 170);
  lv_obj_set_pos(stage, 0, 28);
  lv_obj_set_style_bg_opa(stage, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(stage, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(stage, 0, LV_PART_MAIN);
  lv_obj_clear_flag(stage, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_clear_flag(stage, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t *face = lv_obj_create(stage);
  lv_obj_set_size(face, 88, 88);
  lv_obj_center(face);
  lv_obj_set_y(face, -12);
  lv_obj_set_style_radius(face, 100, LV_PART_MAIN);
  lv_obj_set_style_bg_color(face, lv_color_hex(C_FACE), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(face, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(face, 0, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(face, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(face, 0, LV_PART_MAIN);
  lv_obj_clear_flag(face, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(face, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(face, face_cb, LV_EVENT_CLICKED, NULL);
  desk_create_face_widgets(face);

  g_bubble = lv_label_create(stage);
  lv_obj_set_width(g_bubble, 280);
  lv_label_set_long_mode(g_bubble, LV_LABEL_LONG_WRAP);
  lv_obj_set_style_text_align(g_bubble, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  style_f(g_bubble, g_font_s, C_DIM);
  lv_label_set_text(g_bubble,
                    T("嗨，我在呢\n点我聊天", "Hi, I'm here.\nTap to chat"));
  lv_obj_align(g_bubble, LV_ALIGN_BOTTOM_MID, 0, -4);

  /* dock */
  lv_obj_t *d = lv_obj_create(g_page_home);
  lv_obj_set_size(d, 300, 42);
  lv_obj_set_pos(d, 10, 190);
  lv_obj_set_style_bg_opa(d, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(d, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(d, 0, LV_PART_MAIN);
  lv_obj_set_flex_flow(d, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(d, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER,
                        LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(d, LV_OBJ_FLAG_SCROLLABLE);

  dock_btn(d, "专注", "Focus", focus_click, NULL);
  dock_btn(d, "Wi-Fi", "Wi-Fi", dock_cb, (void *)(uintptr_t)PAGE_WIFI);
  dock_btn(d, "蓝牙", "BT", dock_cb, (void *)(uintptr_t)PAGE_BT);
  dock_btn(d, "备忘", "Note", dock_cb, (void *)(uintptr_t)PAGE_NOTE);

  desk_set_face(FACE_IDLE);
  desk_apply_face();
}

void desk_home_tick(void)
{
  /* countdown lives in desk_focus_tick */
}

#endif
