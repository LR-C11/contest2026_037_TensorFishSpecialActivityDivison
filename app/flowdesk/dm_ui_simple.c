/****************************************************************************
 * dm_ui_simple.c — app-layer simplified home, cards and control center
 ****************************************************************************/

#include "deskmate.h"
#include "dm_audio_vol.h"
#include "dm_tts_voice.h"

#ifdef CONFIG_DESKMATE_APP

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <syslog.h>

#define SIMPLE_MODE_PATH "/data/flowdesk_simple_mode.txt"

static int s_simple_mode;
static lv_obj_t *s_home_time;
static lv_obj_t *s_home_date;
static lv_obj_t *s_cc_sub[4];
static lv_obj_t *s_overview_sub[4];
static lv_obj_t *s_watch_sub[4];

typedef struct
{
  dm_page_t target;
  int axis;                  /* 0 = horizontal, 1 = vertical */
  int direction;             /* -1 = left/up */
  int origin_x;
  int origin_y;
  int press_x;
  int press_y;
  int moved;
  int opened_on_release;
} simple_drag_state_t;

static simple_drag_state_t s_drag_handles[2];
static int s_drag_handle_count;

static void simple_drag_cb(lv_event_t *e);
static lv_obj_t *simple_drag_handle(lv_obj_t *parent, int x, int y,
                                    const char *text, dm_page_t target,
                                    int axis, int direction);
static lv_obj_t *simple_route_button(lv_obj_t *parent, int x, int y,
                                     int w, int h, const char *zh,
                                     const char *en, dm_page_t target);
static void card_route_cb(lv_event_t *e);
static void desk_companion_cb(lv_event_t *e);
static void refresh_simple_text(void);

void dm_simple_mode_init(void)
{
  FILE *f = fopen(SIMPLE_MODE_PATH, "r");
  int mode = 0;

  if (f)
    {
      if (fscanf(f, "%d", &mode) == 1)
        {
          s_simple_mode = mode ? 1 : 0;
        }
      fclose(f);
    }
}

int dm_simple_mode_enabled(void)
{
  return s_simple_mode;
}

int dm_simple_mode_toggle(void)
{
  FILE *f;
  int next = !s_simple_mode;
  int failed = 0;

  f = fopen(SIMPLE_MODE_PATH, "w");
  if (!f)
    {
      syslog(LOG_WARNING, "[dm-simple] unable to save display mode\n");
      return s_simple_mode;
    }

  if (fprintf(f, "%d\n", next) < 0)
    {
      failed = 1;
    }
  if (fclose(f) != 0)
    {
      failed = 1;
    }
  if (failed)
    {
      syslog(LOG_WARNING, "[dm-simple] display mode write failed\n");
      return s_simple_mode;
    }

  s_simple_mode = next;
  return s_simple_mode;
}

static const char *weekday_name(int wday)
{
  static const char *const names[] =
    { "周日", "周一", "周二", "周三", "周四", "周五", "周六" };

  if (wday < 0 || wday > 6)
    {
      return "";
    }
  return names[wday];
}

static void make_date_text(char *out, size_t cap, const struct tm *tmv)
{
  if (!tmv || tmv->tm_year + 1900 < 2024)
    {
      snprintf(out, cap, "%s", "日期未同步");
      return;
    }
  snprintf(out, cap, "%04d-%02d-%02d  %s", tmv->tm_year + 1900,
           tmv->tm_mon + 1, tmv->tm_mday, weekday_name(tmv->tm_wday));
}

static lv_obj_t *simple_page(dm_page_t id, uint32_t color)
{
  lv_obj_t *page = lv_obj_create(g_dm_root);
  lv_obj_set_size(page, DM_SCR_W, DM_SCR_H);
  lv_obj_set_pos(page, 0, 0);
  lv_obj_set_style_bg_color(page, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(page, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(page, 0, LV_PART_MAIN);
  lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(page, LV_OBJ_FLAG_CLICKABLE);
  g_dm_pages[id] = page;
  return page;
}

static lv_obj_t *simple_label(lv_obj_t *parent, const char *text,
                              const lv_font_t *font, uint32_t color,
                              int width, lv_text_align_t align)
{
  lv_obj_t *label = lv_label_create(parent);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, lv_color_hex(color), LV_PART_MAIN);
  if (width > 0)
    {
      lv_obj_set_width(label, width);
    }
  lv_obj_set_style_text_align(label, align, LV_PART_MAIN);
  lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
  return label;
}

static lv_obj_t *simple_card(lv_obj_t *parent, int x, int y, int w, int h,
                             const char *title, const char *subtitle,
                             dm_page_t target, uint32_t color,
                             lv_obj_t **subtitle_out)
{
  lv_obj_t *card = lv_obj_create(parent);
  lv_obj_t *name;
  lv_obj_t *sub;

  lv_obj_set_size(card, w, h);
  lv_obj_set_pos(card, x, y);
  lv_obj_set_style_bg_color(card, lv_color_hex(color), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(card, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(card, lv_color_hex(0x30343d), LV_PART_MAIN);
  lv_obj_set_style_border_width(card, 1, LV_PART_MAIN);
  lv_obj_set_style_radius(card, 14, LV_PART_MAIN);
  lv_obj_set_style_pad_all(card, 8, LV_PART_MAIN);
  lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(card, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(card, card_route_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)target);

  name = simple_label(card, title, g_dm_font_s, C_INK, w - 18,
                      LV_TEXT_ALIGN_LEFT);
  lv_obj_set_pos(name, 0, 1);
  sub = simple_label(card, subtitle, g_dm_font_s, C_DIM, w - 18,
                     LV_TEXT_ALIGN_LEFT);
  lv_label_set_long_mode(sub, LV_LABEL_LONG_DOT);
  lv_obj_set_pos(sub, 0, h > 60 ? 30 : 26);
  if (subtitle_out)
    {
      *subtitle_out = sub;
    }
  return card;
}

static void card_route_cb(lv_event_t *e)
{
  dm_page_t target = (dm_page_t)(uintptr_t)lv_event_get_user_data(e);

  if (target == PAGE_FOCUS_RUN)
    {
      if (!g_dm.focus_run || g_dm.focus_finished)
        {
          if (dm_focus_start_minutes(25) != 0)
            {
              return;
            }
        }
      dm_show(PAGE_FOCUS_RUN);
      return;
    }
  dm_show(target);
}

static lv_obj_t *simple_drag_handle(lv_obj_t *parent, int x, int y,
                                    const char *text, dm_page_t target,
                                    int axis, int direction)
{
  lv_obj_t *button;
  lv_obj_t *label;
  simple_drag_state_t *state;

  if (s_drag_handle_count >=
      (int)(sizeof(s_drag_handles) / sizeof(s_drag_handles[0])))
    {
      return NULL;
    }

  state = &s_drag_handles[s_drag_handle_count++];
  memset(state, 0, sizeof(*state));
  state->target = target;
  state->axis = axis;
  state->direction = direction;
  state->origin_x = x;
  state->origin_y = y;

  button = lv_button_create(parent);
  lv_obj_set_size(button, 46, 46);
  lv_obj_set_pos(button, x, y);
  lv_obj_set_style_radius(button, 12, LV_PART_MAIN);
  lv_obj_set_style_bg_color(button, lv_color_hex(0x334255), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(button, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_color(button, lv_color_hex(0x687b92), LV_PART_MAIN);
  lv_obj_set_style_border_width(button, 1, LV_PART_MAIN);
  lv_obj_set_style_shadow_width(button, 8, LV_PART_MAIN);
  lv_obj_set_style_shadow_opa(button, LV_OPA_30, LV_PART_MAIN);
  lv_obj_clear_flag(button, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(button, simple_drag_cb, LV_EVENT_ALL, state);
  label = lv_label_create(button);
  lv_label_set_text(label, text);
  lv_obj_set_style_text_font(label, g_dm_font_m, LV_PART_MAIN);
  lv_obj_set_style_text_color(label, lv_color_hex(C_INK), LV_PART_MAIN);
  lv_obj_center(label);
  lv_obj_clear_flag(label, LV_OBJ_FLAG_CLICKABLE);
  return button;
}

static void simple_drag_cb(lv_event_t *e)
{
  simple_drag_state_t *state = lv_event_get_user_data(e);
  lv_obj_t *object = lv_event_get_target(e);
  lv_event_code_t code = lv_event_get_code(e);
  lv_indev_t *indev;
  lv_point_t point;
  int dx;
  int dy;

  if (!state || !object)
    {
      return;
    }

  if (code == LV_EVENT_CLICKED)
    {
      if (state->moved || state->opened_on_release)
        {
          state->moved = 0;
          state->opened_on_release = 0;
          return;
        }
      dm_show(state->target);
      return;
    }

  if (code == LV_EVENT_PRESSED)
    {
      indev = lv_indev_active();
      if (!indev)
        {
          return;
        }
      lv_indev_get_point(indev, &point);
      state->press_x = point.x;
      state->press_y = point.y;
      state->moved = 0;
      state->opened_on_release = 0;
      return;
    }

  if (code != LV_EVENT_PRESSING && code != LV_EVENT_RELEASED)
    {
      return;
    }

  indev = lv_indev_active();
  if (!indev)
    {
      return;
    }
  lv_indev_get_point(indev, &point);
  dx = point.x - state->press_x;
  dy = point.y - state->press_y;
  if (dx > 8 || dx < -8 || dy > 8 || dy < -8)
    {
      state->moved = 1;
    }

  if (code == LV_EVENT_PRESSING)
    {
      int x = state->origin_x + dx;
      int y = state->origin_y + dy;
      int max_x = DM_SCR_W - lv_obj_get_width(object);
      int max_y = DM_SCR_H - lv_obj_get_height(object);
      if (x < 0) x = 0;
      if (y < 0) y = 0;
      if (x > max_x) x = max_x;
      if (y > max_y) y = max_y;
      lv_obj_set_pos(object, x, y);
      return;
    }

  if ((state->axis == 0 &&
       ((state->direction < 0 && dx <= -36) ||
        (state->direction > 0 && dx >= 36))) ||
      (state->axis == 1 &&
       ((state->direction < 0 && dy <= -36) ||
        (state->direction > 0 && dy >= 36))))
    {
      state->opened_on_release = 1;
    }

  lv_obj_set_pos(object, state->origin_x, state->origin_y);
  if (state->opened_on_release)
    {
      dm_show(state->target);
    }
}

static lv_obj_t *simple_route_button(lv_obj_t *parent, int x, int y,
                                     int w, int h, const char *zh,
                                     const char *en, dm_page_t target)
{
  lv_obj_t *button = dm_btn(parent, zh, en, w, h, 0x303844, C_INK,
                            card_route_cb, (void *)(uintptr_t)target);
  lv_obj_set_pos(button, x, y);
  lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
  return button;
}

static void desk_companion_cb(lv_event_t *e)
{
  (void)e;
  if (!dm_simple_mode_enabled())
    {
      dm_show(PAGE_FOCUS_HOME);
      return;
    }

  (void)dm_simple_mode_toggle();
  if (!dm_simple_mode_enabled())
    {
      dm_show(PAGE_FOCUS_HOME);
    }
}

static void refresh_simple_text(void)
{
  time_t now = time(NULL);
  struct tm tmv;
  char time_buf[16];
  char date_buf[48];
  char status[72];
  int valid = localtime_r(&now, &tmv) != NULL &&
              tmv.tm_year + 1900 >= 2024;

  if (valid)
    {
      snprintf(time_buf, sizeof(time_buf), "%02d:%02d",
               tmv.tm_hour, tmv.tm_min);
      make_date_text(date_buf, sizeof(date_buf), &tmv);
    }
  else
    {
      snprintf(time_buf, sizeof(time_buf), "--:--");
      snprintf(date_buf, sizeof(date_buf), "日期未同步");
    }

  if (s_home_time) lv_label_set_text(s_home_time, time_buf);
  if (s_home_date) lv_label_set_text(s_home_date, date_buf);

  if (s_cc_sub[0])
    {
      if (!dm_wifi_enabled())
        {
          snprintf(status, sizeof(status), "%s", "已关闭");
        }
      else if (dm_wifi_connected())
        {
          snprintf(status, sizeof(status), "%s", dm_wifi_cur_ssid());
        }
      else
        {
          snprintf(status, sizeof(status), "%s", "已开启 · 未连接");
        }
      lv_label_set_text(s_cc_sub[0], status);
    }
  if (s_cc_sub[1]) lv_label_set_text(s_cc_sub[1], date_buf);
  if (s_cc_sub[2])
    {
      snprintf(status, sizeof(status), "媒体 %d%% · 闹钟 %d%%",
               dm_vol_get(), dm_vol_alarm_get());
      lv_label_set_text(s_cc_sub[2], status);
    }
  if (s_cc_sub[3])
    {
      snprintf(status, sizeof(status), "当前：%s", dm_voice_profile_name());
      lv_label_set_text(s_cc_sub[3], status);
    }

  if (s_overview_sub[0])
    {
      if (g_dm.focus_run && !g_dm.focus_finished)
        {
          int left = g_dm.focus_left < 0 ? 0 : g_dm.focus_left;
          snprintf(status, sizeof(status), "剩余 %d:%02d",
                   left / 60, left % 60);
        }
      else
        {
          snprintf(status, sizeof(status), "开启 25 分钟专注");
        }
      lv_label_set_text(s_overview_sub[0], status);
    }
  if (s_overview_sub[1])
    {
      snprintf(status, sizeof(status), "今日喝水 %d 杯", dm_water_today_cups());
      lv_label_set_text(s_overview_sub[1], status);
    }
  if (s_overview_sub[2])
    {
      lv_label_set_text(s_overview_sub[2], "语音添加 / 到时提醒");
    }
  if (s_overview_sub[3])
    {
      lv_label_set_text(s_overview_sub[3], "查看或设置响铃");
    }

  if (s_watch_sub[0])
    {
      snprintf(status, sizeof(status), "今日完成 %ld 分钟",
               (long)g_dm.focus_done_min);
      lv_label_set_text(s_watch_sub[0], status);
    }
  if (s_watch_sub[1])
    {
      snprintf(status, sizeof(status), "喝水 %d 杯 · 记录心情",
               dm_water_today_cups());
      lv_label_set_text(s_watch_sub[1], status);
    }
  if (s_watch_sub[2])
    {
      lv_label_set_text(s_watch_sub[2], "快速记录一条备忘");
    }
  if (s_watch_sub[3])
    {
      lv_label_set_text(s_watch_sub[3], valid ? "日期已同步" : "请校准设备时间");
    }
}

void dm_create_simple_pages(void)
{
  lv_obj_t *page;
  lv_obj_t *label;
  lv_obj_t *button;

  page = simple_page(PAGE_SIMPLE_HOME, 0x080A0E);
  s_home_date = simple_label(page, "日期未同步", g_dm_font_s, C_DIM,
                             300, LV_TEXT_ALIGN_CENTER);
  lv_obj_align(s_home_date, LV_ALIGN_TOP_MID, 0, 42);
  s_home_time = simple_label(page, "--:--", g_dm_font_xl, C_INK,
                             300, LV_TEXT_ALIGN_CENTER);
  lv_obj_align(s_home_time, LV_ALIGN_CENTER, 0, -3);
  lv_obj_set_style_text_letter_space(s_home_time, 2, LV_PART_MAIN);
  (void)simple_drag_handle(page, 10, 168, "控", PAGE_CONTROL_CENTER,
                           1, -1);
  (void)simple_drag_handle(page, 264, 168, "卡", PAGE_SIMPLE_OVERVIEW,
                           0, -1);
  label = simple_label(page, "拖「控」↑ / 「卡」←，点按也可打开",
                       g_dm_font_s, C_DIM, 300, LV_TEXT_ALIGN_CENTER);
  lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -5);

  page = simple_page(PAGE_SIMPLE_OVERVIEW, 0x0A0D12);
  label = simple_label(page, "今日概览", g_dm_font_m, C_INK, 180,
                       LV_TEXT_ALIGN_LEFT);
  lv_obj_set_pos(label, 14, 8);
  (void)simple_route_button(page, 202, 6, 48, 24, "首页", "Home",
                            PAGE_SIMPLE_HOME);
  (void)simple_route_button(page, 256, 6, 54, 24, "手表", "Watch",
                            PAGE_WATCH_CARDS);
  simple_card(page, 14, 42, 142, 64, "专注", "开启 25 分钟专注",
              PAGE_FOCUS_RUN, 0x18232C, &s_overview_sub[0]);
  simple_card(page, 164, 42, 142, 64, "健康", "今日记录", PAGE_HEALTH,
              0x20202A, &s_overview_sub[1]);
  simple_card(page, 14, 116, 142, 64, "待办", "语音添加 / 到时提醒",
              PAGE_TODO, 0x232019, &s_overview_sub[2]);
  simple_card(page, 164, 116, 142, 64, "闹钟", "查看或设置响铃",
              PAGE_ALARM, 0x271C20, &s_overview_sub[3]);
  (void)simple_route_button(page, 84, 188, 152, 28, "控制中心", "Control",
                            PAGE_CONTROL_CENTER);

  page = simple_page(PAGE_CONTROL_CENTER, 0x101319);
  label = simple_label(page, "控制中心", g_dm_font_m, C_INK, 140,
                       LV_TEXT_ALIGN_LEFT);
  lv_obj_set_pos(label, 14, 7);
  (void)simple_route_button(page, 160, 5, 48, 24, "首页", "Home",
                            PAGE_SIMPLE_HOME);
  (void)simple_route_button(page, 212, 5, 48, 24, "卡片", "Cards",
                            PAGE_SIMPLE_OVERVIEW);
  button = dm_btn(page, "设定", "Set", 50, 24, 0x303642,
                  C_INK, NULL, NULL);
  lv_obj_set_pos(button, 264, 5);
  lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(button, card_route_cb, LV_EVENT_CLICKED,
                      (void *)(uintptr_t)PAGE_SETTINGS);
  simple_card(page, 14, 43, 142, 60, "Wi-Fi", "读取状态…", PAGE_WIFI,
              0x18232C, &s_cc_sub[0]);
  simple_card(page, 164, 43, 142, 60, "日期与时间", "读取日期…",
              PAGE_TIME_SET, 0x20202A, &s_cc_sub[1]);
  simple_card(page, 14, 113, 142, 60, "音量", "读取音量…", PAGE_VOLUME,
              0x232019, &s_cc_sub[2]);
  simple_card(page, 164, 113, 142, 60, "播报声音", "读取音色…",
              PAGE_VOICE_SEL, 0x271C20, &s_cc_sub[3]);
  button = dm_btn(page, "切换为桌伴模式", "Desk companion mode",
                  190, 28, 0x455366, C_INK, desk_companion_cb, NULL);
  lv_obj_set_pos(button, 65, 180);
  lv_obj_add_flag(button, LV_OBJ_FLAG_CLICKABLE);
  label = simple_label(page, "点选卡片进入对应设置 · ↓ 返回", g_dm_font_s,
                       C_DIM, 300, LV_TEXT_ALIGN_CENTER);
  lv_obj_align(label, LV_ALIGN_BOTTOM_MID, 0, -8);

  page = simple_page(PAGE_WATCH_CARDS, 0x090C12);
  label = simple_label(page, "卡片", g_dm_font_m, C_INK, 145,
                       LV_TEXT_ALIGN_LEFT);
  lv_obj_set_pos(label, 14, 7);
  (void)simple_route_button(page, 176, 6, 48, 24, "首页", "Home",
                            PAGE_SIMPLE_HOME);
  (void)simple_route_button(page, 230, 6, 80, 24, "快捷应用", "Apps",
                            PAGE_WATCH_APPS);
  simple_card(page, 14, 40, 142, 62, "专注记录", "今日完成 0 分钟",
              PAGE_HEALTH, 0x1B2631, &s_watch_sub[0]);
  simple_card(page, 164, 40, 142, 62, "健康", "喝水与心情",
              PAGE_HEALTH, 0x20202A, &s_watch_sub[1]);
  simple_card(page, 14, 112, 142, 62, "备忘", "快速记录一条备忘",
              PAGE_NOTE, 0x232019, &s_watch_sub[2]);
  simple_card(page, 164, 112, 142, 62, "日期", "检查设备时间",
              PAGE_TIME_SET, 0x271C20, &s_watch_sub[3]);
  (void)simple_route_button(page, 14, 188, 142, 28, "今日概览", "Overview",
                            PAGE_SIMPLE_OVERVIEW);
  (void)simple_route_button(page, 164, 188, 142, 28, "控制中心", "Control",
                            PAGE_CONTROL_CENTER);

  page = simple_page(PAGE_WATCH_APPS, 0x0A0D12);
  label = simple_label(page, "快捷卡片", g_dm_font_m, C_INK, 170,
                       LV_TEXT_ALIGN_LEFT);
  lv_obj_set_pos(label, 14, 7);
  (void)simple_route_button(page, 194, 6, 48, 24, "卡片", "Cards",
                            PAGE_WATCH_CARDS);
  (void)simple_route_button(page, 248, 6, 62, 24, "首页", "Home",
                            PAGE_SIMPLE_HOME);
  simple_card(page, 14, 40, 142, 48, "闹钟", "管理闹钟", PAGE_ALARM,
              0x271C20, NULL);
  simple_card(page, 164, 40, 142, 48, "录音", "录制 / 回放", PAGE_RECORDER,
              0x18232C, NULL);
  simple_card(page, 14, 96, 142, 48, "专注", "开始 25 分钟", PAGE_FOCUS_RUN,
              0x20202A, NULL);
  simple_card(page, 164, 96, 142, 48, "健康", "今日概览", PAGE_HEALTH,
              0x232019, NULL);
  simple_card(page, 14, 152, 142, 48, "聊天", "语音助手", PAGE_CHAT,
              0x1B2631, NULL);
  simple_card(page, 164, 152, 142, 48, "备忘", "查看备忘", PAGE_NOTE,
              0x271C20, NULL);
  (void)simple_route_button(page, 14, 208, 142, 24, "今日概览", "Overview",
                            PAGE_SIMPLE_OVERVIEW);
  (void)simple_route_button(page, 164, 208, 142, 24, "控制中心", "Control",
                            PAGE_CONTROL_CENTER);

  refresh_simple_text();
}

void dm_simple_mode_tick(void)
{
  static int ticks;

  if (++ticks < DM_TICKS_PER_SEC)
    {
      return;
    }
  ticks = 0;
  refresh_simple_text();
}

#endif /* CONFIG_DESKMATE_APP */
