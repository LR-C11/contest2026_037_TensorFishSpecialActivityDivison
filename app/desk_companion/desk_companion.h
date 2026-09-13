/****************************************************************************
 * desk_companion.h — DeskMate shared types & API (v0.7 black/white focus)
 ****************************************************************************/

#ifndef DESK_COMPANION_H
#define DESK_COMPANION_H

#include <nuttx/config.h>
#include <stdbool.h>
#include <stdint.h>
#include <lvgl/lvgl.h>

#ifdef CONFIG_DESK_COMPANION_APP

#define DESK_VER "0.7.0"

#define C_BG      0x000000
#define C_FACE    0xFFFFFF
#define C_EYE     0x000000
#define C_INK     0xFFFFFF
#define C_MUTED   0x666666
#define C_DIM     0x888888
#define C_BTN      0x111111
#define C_BTN_HI   0x222222
#define C_ACCENT   0xFF8A65
#define C_ACCENT2  0x7DD3FC
#define C_OK       0x6BCB77

typedef enum {
  PAGE_HOME = 0,
  PAGE_WIFI,
  PAGE_BT,
  PAGE_NOTE,
  PAGE_ABOUT,
  PAGE_FOCUS,
} desk_page_t;

typedef enum {
  FACE_IDLE = 0,
  FACE_HAPPY,
  FACE_FOCUS,
  FACE_THINK,
  FACE_SLEEP,
  FACE_WINK,
  FACE_SHY,
  FACE_TALK,
  FACE_SAD,
  FACE_LOVE,
  FACE_SURPRISE,
  FACE_CONFUSED,
  FACE_YAWN,
  FACE_NERVOUS,
  FACE_ANGRY,
} desk_face_t;

extern const lv_font_t *g_font_s;
extern const lv_font_t *g_font_m;
extern const lv_font_t *g_font_l;
extern bool g_zh;
extern desk_page_t g_page;
extern desk_face_t g_face;
extern int32_t g_focus_left;
extern bool g_focus_run;
extern int g_temp_sub;
extern int g_humi_sub;
extern float g_temp_c;
extern float g_humi_r;
extern bool g_has_env;
extern char g_note[128];

/* focus page custom minutes */
extern int32_t g_focus_custom_min;
extern int g_focus_selected_min;
extern bool g_focus_on_custom;

extern lv_obj_t *g_root;
extern lv_obj_t *g_page_home;
extern lv_obj_t *g_page_wifi;
extern lv_obj_t *g_page_bt;
extern lv_obj_t *g_page_note;
extern lv_obj_t *g_page_about;
extern lv_obj_t *g_page_focus;
extern lv_obj_t *g_kb;
extern lv_obj_t *g_bubble;
extern lv_obj_t *g_status_txt;
extern lv_obj_t *g_focus_txt;
extern lv_obj_t *g_eye_l;
extern lv_obj_t *g_eye_r;
extern lv_obj_t *g_mouth;
extern lv_obj_t *g_face_box;
extern lv_obj_t *g_blush_l;
extern lv_obj_t *g_blush_r;
extern lv_obj_t *g_zzz;
extern lv_obj_t *g_think_dot[3];
extern lv_obj_t *g_wifi_card_st;
extern lv_obj_t *g_bt_card_st;

const char *T(const char *zh, const char *en);
void style_f(lv_obj_t *o, const lv_font_t *f, uint32_t c);
lv_obj_t *lbl(lv_obj_t *p, const char *zh, const char *en, const lv_font_t *f,
              uint32_t c);
void say(const char *zh, const char *en);
void desk_init_fonts(void);
void desk_show(desk_page_t p);
void desk_apply_face(void);
void desk_set_face_hold(desk_face_t f, int32_t ticks);
void desk_set_face(desk_face_t f);
void desk_create_home(void);
void desk_create_wifi(void);
void desk_create_bt(void);
void desk_create_note(void);
void desk_create_about(void);
void desk_create_focus(void);
void desk_create_kb(void);
void desk_tick(void);
void desk_create_face_widgets(lv_obj_t *face);
void desk_face_on_click(void);
void desk_face_anim_tick(void);
void desk_home_tick(void);
void desk_focus_tick(void);
void desk_wifi_tick(void);
void desk_bt_tick(void);
void desk_update_focus_ui(void);
void desk_update_focus_page(void);
lv_obj_t *desk_page_shell(lv_obj_t *parent, uint32_t bg);
lv_obj_t *desk_backbar(lv_obj_t *page, const char *zh, const char *en);
void desk_face_press_pulse(void);

#endif
#endif
