/****************************************************************************
 * desk_state.c — shared helpers & navigation
 ****************************************************************************/

#include "desk_companion.h"
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>

#ifdef CONFIG_DESK_COMPANION_APP

const lv_font_t *g_font_s;
const lv_font_t *g_font_m;
const lv_font_t *g_font_l;
bool g_zh;
desk_page_t g_page = PAGE_HOME;
desk_face_t g_face = FACE_IDLE;
int32_t g_focus_left = 25 * 60;
bool g_focus_run = true;
int g_temp_sub = -1;
int g_humi_sub = -1;
float g_temp_c;
float g_humi_r;
bool g_has_env;
char g_note[128];
int32_t g_focus_custom_min = 30;
int g_focus_selected_min = 25;
bool g_focus_on_custom;

lv_obj_t *g_root;
lv_obj_t *g_page_home;
lv_obj_t *g_page_wifi;
lv_obj_t *g_page_bt;
lv_obj_t *g_page_note;
lv_obj_t *g_page_about;
lv_obj_t *g_page_focus;
lv_obj_t *g_kb;
lv_obj_t *g_bubble;
lv_obj_t *g_status_txt;
lv_obj_t *g_focus_txt;
lv_obj_t *g_eye_l;
lv_obj_t *g_eye_r;
lv_obj_t *g_mouth;
lv_obj_t *g_face_box;
lv_obj_t *g_blush_l;
lv_obj_t *g_blush_r;
lv_obj_t *g_zzz;
lv_obj_t *g_think_dot[3];
lv_obj_t *g_wifi_card_st;
lv_obj_t *g_bt_card_st;

const char *T(const char *zh, const char *en)
{
  return g_zh ? zh : en;
}

void style_f(lv_obj_t *o, const lv_font_t *f, uint32_t c)
{
  lv_obj_set_style_text_font(o, f, LV_PART_MAIN);
  lv_obj_set_style_text_color(o, lv_color_hex(c), LV_PART_MAIN);
}

lv_obj_t *lbl(lv_obj_t *p, const char *zh, const char *en, const lv_font_t *f,
              uint32_t c)
{
  lv_obj_t *o = lv_label_create(p);
  lv_label_set_text(o, T(zh, en));
  style_f(o, f, c);
  return o;
}

void say(const char *zh, const char *en)
{
  if (g_bubble)
    lv_label_set_text(g_bubble, T(zh, en));
}

static const lv_font_t *try_ft(const char *path, uint16_t sz)
{
#ifdef LV_USE_FREETYPE
  return lv_freetype_font_create(path, LV_FREETYPE_FONT_RENDER_MODE_BITMAP,
                                 sz, LV_FREETYPE_FONT_STYLE_NORMAL);
#else
  (void)path;
  (void)sz;
  return NULL;
#endif
}

void desk_init_fonts(void)
{
  g_font_s = try_ft("/data/font/MiSans-Regular.ttf", 12);
  g_font_m = try_ft("/data/font/MiSans-Regular.ttf", 16);
  g_font_l = try_ft("/data/font/MiSans-Regular.ttf", 20);
  if (!g_font_s)
    g_font_s = try_ft("/resource/fonts/MiSans-Normal.ttf", 12);
  if (!g_font_m)
    g_font_m = try_ft("/resource/fonts/MiSans-Normal.ttf", 16);
  if (!g_font_l)
    g_font_l = try_ft("/resource/fonts/MiSans-Normal.ttf", 20);
  g_zh = g_font_s && g_font_m && g_font_l;
  if (!g_zh)
    {
      g_font_s = &lv_font_montserrat_12;
      g_font_m = &lv_font_montserrat_16;
      g_font_l = &lv_font_montserrat_20;
    }
}

void desk_create_kb(void)
{
  if (g_kb)
    return;
  g_kb = lv_keyboard_create(lv_layer_top());
  lv_obj_set_size(g_kb, 310, 96);
  lv_obj_align(g_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_mode(g_kb, LV_KEYBOARD_MODE_TEXT_LOWER);
  lv_obj_add_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
}

void desk_show(desk_page_t p)
{
  g_page = p;
  lv_obj_t *pages[] = {g_page_home, g_page_wifi, g_page_bt, g_page_note,
                       g_page_about, g_page_focus};
  desk_page_t ids[] = {PAGE_HOME, PAGE_WIFI, PAGE_BT, PAGE_NOTE, PAGE_ABOUT,
                       PAGE_FOCUS};
  for (int i = 0; i < 6; i++)
    {
      if (!pages[i])
        continue;
      if (p == ids[i])
        {
          lv_obj_clear_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
          lv_obj_move_foreground(pages[i]);
        }
      else
        {
          lv_obj_add_flag(pages[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
  if (g_kb && p != PAGE_WIFI && p != PAGE_NOTE)
    lv_obj_add_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
}

/* page shell: full 320x240 child of g_root */
lv_obj_t *desk_page_shell(lv_obj_t *parent, uint32_t bg)
{
  lv_obj_t *pg = lv_obj_create(parent);
  lv_obj_set_size(pg, 320, 240);
  lv_obj_set_pos(pg, 0, 0);
  lv_obj_set_style_bg_color(pg, lv_color_hex(bg), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(pg, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(pg, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(pg, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(pg, 0, LV_PART_MAIN);
  lv_obj_clear_flag(pg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(pg, LV_OBJ_FLAG_HIDDEN);
  return pg;
}

static void back_home_cb(lv_event_t *e)
{
  (void)e;
  desk_show(PAGE_HOME);
}

lv_obj_t *desk_backbar(lv_obj_t *page, const char *zh, const char *en)
{
  lv_obj_t *bar = lv_obj_create(page);
  lv_obj_set_size(bar, 320, 34);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, lv_color_hex(C_BG), LV_PART_MAIN);
  lv_obj_set_style_border_width(bar, 0, LV_PART_MAIN);
  lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *bk = lv_button_create(bar);
  lv_obj_set_size(bk, 36, 26);
  lv_obj_align(bk, LV_ALIGN_LEFT_MID, 6, 0);
  lv_obj_set_style_bg_color(bk, lv_color_hex(C_BTN), LV_PART_MAIN);
  lv_obj_add_flag(bk, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(bk, back_home_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *bl = lbl(bk, "←", "<", g_font_m, C_INK);
  lv_obj_center(bl);

  lv_obj_t *t = lbl(bar, zh, en, g_font_m, C_INK);
  lv_obj_center(t);
  return bar;
}

static void pulse_end(lv_timer_t *t)
{
  (void)t;
  if (g_face_box)
    {
      lv_obj_set_style_transform_scale_x(g_face_box, 256, LV_PART_MAIN);
      lv_obj_set_style_transform_scale_y(g_face_box, 256, LV_PART_MAIN);
    }
}

void desk_face_press_pulse(void)
{
  if (!g_face_box)
    return;
  lv_obj_set_style_transform_pivot_x(g_face_box, 44, LV_PART_MAIN);
  lv_obj_set_style_transform_pivot_y(g_face_box, 44, LV_PART_MAIN);
  lv_obj_set_style_transform_scale_x(g_face_box, 240, LV_PART_MAIN);
  lv_obj_set_style_transform_scale_y(g_face_box, 240, LV_PART_MAIN);
  {
    lv_timer_t *tm = lv_timer_create(pulse_end, 180, NULL);
    if (tm)
      lv_timer_set_repeat_count(tm, 1);
  }
}

#endif
