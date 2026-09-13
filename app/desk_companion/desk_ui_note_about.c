/****************************************************************************
 * desk_ui_note.c / desk_ui_about.c
 ****************************************************************************/

#include "desk_companion.h"
#include <sensor/humi.h>
#include <sensor/temp.h>
#include <uORB/uORB.h>
#include <string.h>

#ifdef CONFIG_DESK_COMPANION_APP

static lv_obj_t *g_note_ta;

static void note_shown_cb(lv_event_t *e)
{
  (void)e;
  if (g_note_ta && g_kb)
    {
      lv_textarea_set_text(g_note_ta, g_note);
      lv_obj_clear_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(g_kb);
      lv_keyboard_set_textarea(g_kb, g_note_ta);
    }
}

static void note_save_cb(lv_event_t *e)
{
  (void)e;
  if (g_note_ta)
    {
      const char *t = lv_textarea_get_text(g_note_ta);
      strncpy(g_note, t ? t : "", sizeof(g_note) - 1);
      g_note[sizeof(g_note) - 1] = 0;
    }
  if (g_kb)
    lv_obj_add_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
  desk_show(PAGE_HOME);
  say("记下了，需要时叫我", "Noted. Call me anytime.");
  desk_set_face_hold(FACE_HAPPY, 25);
}

void desk_create_note(void)
{
  g_page_note = desk_page_shell(g_root, C_BG);
  desk_backbar(g_page_note, "快速备忘", "Quick Note");

  g_note_ta = lv_textarea_create(g_page_note);
  lv_obj_set_size(g_note_ta, 300, 140);
  lv_obj_set_pos(g_note_ta, 10, 44);
  lv_textarea_set_max_length(g_note_ta, 120);
  lv_textarea_set_placeholder_text(g_note_ta, T("写点什么…", "Write something…"));
  style_f(g_note_ta, g_font_m, C_INK);

  lv_obj_t *save = lv_button_create(g_page_note);
  lv_obj_set_size(save, 100, 36);
  lv_obj_set_pos(save, 110, 196);
  lv_obj_set_style_bg_color(save, lv_color_hex(C_ACCENT), LV_PART_MAIN);
  lv_obj_add_flag(save, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(save, note_save_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *sl = lbl(save, "保存", "Save", g_font_m, 0x1A1A2E);
  lv_obj_center(sl);
}

void desk_create_about(void)
{
  g_page_about = desk_page_shell(g_root, C_BG);
  desk_backbar(g_page_about, "关于 DeskMate", "About DeskMate");

  lv_obj_t *body = lv_label_create(g_page_about);
  lv_obj_set_width(body, 296);
  lv_label_set_long_mode(body, LV_LABEL_LONG_WRAP);
  lv_obj_set_pos(body, 12, 48);
  style_f(body, g_font_s, C_INK);
  lv_label_set_text(
      body,
      T("DeskMate 智能陪伴桌搭\n"
        "Gemini-S1 · OpenVela\n"
        "版本 " DESK_VER "\n\n"
        "• 点脸聊天 / 表情互动\n"
        "• 专注监督 · 快速备忘\n"
        "• Wi-Fi / 蓝牙设置\n"
        "• 温湿度环境显示\n\n"
        "本地优先，数据不上传",
        "DeskMate companion hub\n"
        "Gemini-S1 · OpenVela v" DESK_VER "\n\n"
        "• Tap face to chat\n"
        "• Focus · Notes\n"
        "• Wi-Fi / Bluetooth\n"
        "• Local sensors\n"
        "Local-first, no cloud."));
}

/* helpers used by pages */
void desk_home_tick(void);
void desk_focus_tick(void);
void desk_wifi_tick(void);
void desk_bt_tick(void);
void desk_face_anim_tick(void);
void desk_create_face_widgets(lv_obj_t *face);

void desk_tick(void)
{
  desk_face_anim_tick();
  desk_home_tick();
  desk_focus_tick();
  desk_wifi_tick();
  desk_bt_tick();

  if (g_status_txt)
    {
      char b[40];
      if (g_has_env)
        lv_snprintf(b, sizeof(b), "%.0f°C  %.0f%%", (double)g_temp_c,
                    (double)g_humi_r);
      else
        lv_snprintf(b, sizeof(b), "%s", T("待机", "Idle"));
      lv_label_set_text(g_status_txt, b);
    }
  if (g_temp_sub >= 0)
    {
      struct sensor_temp tp;
      struct sensor_humi hm;
      if (orb_copy(ORB_ID(sensor_temp), g_temp_sub, &tp) == OK)
        {
          g_temp_c = tp.temperature;
          g_has_env = true;
        }
      if (g_humi_sub >= 0 &&
          orb_copy(ORB_ID(sensor_humi), g_humi_sub, &hm) == OK)
        {
          g_humi_r = hm.humidity;
          g_has_env = true;
        }
    }
}

#endif
