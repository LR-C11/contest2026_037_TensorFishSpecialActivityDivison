/****************************************************************************
 * deskmate.h — Deskmate shared types & API
 ****************************************************************************/

#ifndef DESKMATE_H
#define DESKMATE_H

#include <nuttx/config.h>
#include <stdbool.h>
#include <stdint.h>
#include <lvgl/lvgl.h>

#ifdef CONFIG_DESKMATE_APP

#define DM_VER "0.13.4"

#define C_BG     0x000000
#define C_FACE   0xFFFFFF
#define C_EYE    0x000000
#define C_INK    0xFFFFFF
#define C_MUTED  0x666666
#define C_DIM    0x888888
#define C_BTN    0x111111
#define C_BTN_HI 0x222222
#define C_STAR   0xFFD54F
#define C_HEART  0xFF6B8A
#define C_OK     0x6BCB77
#define C_ACCENT 0x7DD3FC
#define C_BLUSH  0xFF8A9A
#define C_TEAR   0x7DD3FC

#define DM_SCR_W 320
#define DM_SCR_H 240
#define DM_TICK_MS 250
#define DM_TICKS_PER_SEC (1000 / DM_TICK_MS)

typedef enum {
  PAGE_FOCUS_HOME = 0,
  PAGE_FOCUS_RUN,
  PAGE_FOCUS_DONE,
  PAGE_CHAT,
  PAGE_HEALTH,
  PAGE_MOOD_LOG,
  PAGE_FEATURES,
  PAGE_SUPERVISE,
  PAGE_NOTE,
  PAGE_NOTE_ADD,
  PAGE_NOTE_KB,
  PAGE_SETTINGS,
  PAGE_WIFI,
  PAGE_WIFI_PW,
  PAGE_WIFI_CONN,
  PAGE_WIFI_DONE,
  PAGE_WORD_HOME,
  PAGE_WORD_STUDY,
  PAGE_WORD_RES,
  PAGE_WORD_QMODE,
  PAGE_WORD_QUIZ,
  PAGE_WORD_WRONG,
  PAGE_WORD_LIST,
  PAGE_WORD_WSET,
  PAGE_CALC,
  PAGE_SENSORS,
  PAGE_GUESS,
  PAGE_CONVERT,
  PAGE_WATER,
  PAGE_COUNTDOWN,
  PAGE_EAT,
  PAGE_24,
  PAGE_DRAW,
  PAGE_BMI,
  PAGE_MED,
  PAGE_MED_ADD,
  PAGE_MED_KB,
  PAGE_2048,
  PAGE_MBTI,
  PAGE_MBTI_Q,
  PAGE_MBTI_RESULT,
  PAGE_COUNT
} dm_page_t;

typedef enum {
  EYE_DECOR_NONE = 0,
  EYE_DECOR_STAR,
  EYE_DECOR_HEART,
  EYE_DECOR_WIDE,
} dm_eye_decor_t;

typedef enum {
  FACE_IDLE = 0,
  FACE_FOCUS,
  FACE_HAPPY,
  FACE_LOVE,
  FACE_SURPRISE,
  FACE_DONE,
  FACE_WINK,
  FACE_SHY,
  FACE_SLEEPY,
  FACE_THINK,
  FACE_CRY,
  FACE_LAUGH,
  FACE_ANGRY,
} dm_face_t;

typedef struct {
  dm_page_t page;
  dm_face_t face;
  dm_eye_decor_t decor;
  int32_t focus_left;
  int32_t focus_total;
  int32_t focus_sel_min;
  int32_t focus_custom_min;
  bool focus_run;
  bool focus_finished;
  bool zh;
  int decor_hold;
  int look_hold;
  int look_x;
  int look_y;
  int wide_hold;
  int32_t focus_done_min; /* today completed focus minutes */
} dm_ctx_t;

/* mood bits */
#define DM_M_JOY      (1u << 0)
#define DM_M_LOVE     (1u << 1)
#define DM_M_EXCITE   (1u << 2)
#define DM_M_CONFIDENT (1u << 3)
#define DM_M_EXPECT   (1u << 4)
#define DM_M_SATISFY  (1u << 5)
#define DM_M_TOUCHED  (1u << 6)
#define DM_M_CALM     (1u << 7)
#define DM_M_THINK    (1u << 8)
#define DM_M_SURPRISE (1u << 9)
#define DM_M_SAD      (1u << 10)
#define DM_M_ANGRY    (1u << 11)
#define DM_M_TIRED    (1u << 12)
#define DM_M_ANXIOUS  (1u << 13)
#define DM_M_LONELY   (1u << 14)
#define DM_M_STRESS   (1u << 15)

#define DM_MOOD_REC_MAX 8
#define DM_QUICK_MAX 8

typedef struct {
  uint16_t mood_mask;
  uint8_t quick;    /* 0 none, 1..DM_QUICK_MAX */
  uint8_t rec_type; /* 0 current, 1 daily */
  uint8_t hour;
  uint8_t min;
} dm_mood_rec_t;

extern dm_ctx_t g_dm;
extern const lv_font_t *g_dm_font_s;
extern const lv_font_t *g_dm_font_m;
extern const lv_font_t *g_dm_font_l;
extern const lv_font_t *g_dm_font_xl;

extern lv_obj_t *g_dm_root;
extern lv_obj_t *g_dm_pages[PAGE_COUNT];
extern lv_obj_t *g_dm_dock;
extern lv_obj_t *g_dm_face;
extern lv_obj_t *g_dm_bubble;
extern lv_obj_t *g_dm_eye_l;
extern lv_obj_t *g_dm_eye_r;
extern lv_obj_t *g_dm_mouth;
extern lv_obj_t *g_dm_decor_l;
extern lv_obj_t *g_dm_decor_r;
extern lv_obj_t *g_dm_blush_l;
extern lv_obj_t *g_dm_blush_r;

/* roll clock */
typedef struct {
  lv_obj_t *clip;
  lv_obj_t *cur;
  int digit;
} dm_digit_t;

typedef struct {
  lv_obj_t *box;
  dm_digit_t d[4];
} dm_clock_t;

const char *dm_t(const char *zh, const char *en);
void dm_style(lv_obj_t *o, const lv_font_t *f, uint32_t c);
lv_obj_t *dm_lbl(lv_obj_t *p, const char *zh, const char *en,
                 const lv_font_t *f, uint32_t c);
lv_obj_t *dm_btn(lv_obj_t *p, const char *zh, const char *en, int w, int h,
                 uint32_t bg, uint32_t fg, lv_event_cb_t cb, void *ud);
void dm_say(const char *zh, const char *en);
void dm_init_fonts(void);
void dm_show(dm_page_t p);
void dm_tick(void);

/* face */
void dm_face_build(lv_obj_t *parent, int x, int y, int size);
void dm_face_attach(lv_obj_t *page, int x, int y);
void dm_face_set(dm_face_t f, dm_eye_decor_t decor, int decor_hold);
void dm_face_tick(void);
void dm_face_on_click(lv_event_t *e);
void dm_face_repaint(void);
void dm_word_face_click(void);

/* pages */
void dm_create_focus_home(void);
void dm_create_focus_run(void);
void dm_create_focus_done(void);
void dm_focus_show_done(int32_t minutes);
void dm_create_chat(void);
void dm_create_health(void);
void dm_create_2048(void);
void dm_create_mbti(void);
void dm_chat_tick(void);
int dm_chat_ai_available(void);
const char *dm_chat_ai_ask(const char *user_text);
const char *dm_chat_local_reply(const char *user_text);
void dm_create_supervise(void);
void dm_create_dock(void);
void dm_dock_highlight(dm_page_t p);
void dm_focus_home_tick(void);
void dm_focus_run_tick(void);
void dm_focus_start(void);
void dm_focus_end(void);
void dm_focus_toggle_pause(void);
void dm_update_run_clock(void);

/* health */
void dm_health_add_focus_min(int32_t min);
void dm_health_add_game_ms(int ms);
int dm_water_today_cups(void);
int dm_med_today_stats(int *sched_out, int *taken_out);
int dm_word_today_n(void);
int dm_sensor_last_th(float *t_c, float *h_pct);
void dm_health_refresh(void);
int dm_health_score(void);

/* settings / wifi */
void dm_create_settings(void);
void dm_create_features(void);
void dm_create_wifi(void);
void dm_create_word(void);
void dm_create_tools(void);
void dm_create_life(void);
void dm_create_fun(void);
void dm_create_med(void);
void dm_create_note(void);
void dm_med_kb_open(void);
void dm_tools_tick(void);
void dm_life_tick(void);
void dm_fun_tick(void);
void dm_med_tick(void);
void dm_wifi_tick(void);
int dm_wifi_start_scan(void);
int dm_wifi_connect(const char *ssid, const char *pass);
void dm_wifi_auto_start(void);
const char *dm_wifi_cur_ssid(void);
const char *dm_wifi_cur_ip(void);
int dm_wifi_connected(void);
int dm_wifi_ap_count(void);
int dm_wifi_ap_get(int i, const char **ssid, int *rssi, bool *open);
int dm_wifi_sel(void);
void dm_wifi_set_sel(int i);
int dm_wifi_scan_busy(void);
int dm_wifi_scan_ready(void);
void dm_wifi_clear_scan_ready(void);
int dm_wifi_conn_busy(void);
int dm_wifi_conn_ok(void);
int dm_wifi_conn_fail(void);
void dm_wifi_clear_conn_flags(void);

/* roll */
void dm_clock_create(dm_clock_t *c, lv_obj_t *parent, int x, int y, int w,
                     int h, const lv_font_t *font);
void dm_clock_set(dm_clock_t *c, int32_t seconds_left);
void dm_clock_refresh_now(dm_clock_t *c);

#endif
#endif /* DESKMATE_H */
