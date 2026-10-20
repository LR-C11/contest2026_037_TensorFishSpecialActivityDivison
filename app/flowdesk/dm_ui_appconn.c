/*
 * dm_ui_appconn.c — App Connect page (BLE GATT pairing)
 * Board advertises FlowDesk service; Android App scans/connects and
 * completes Challenge-Response pairing (see dm_ble_gatt.c / BleProtocol.kt).
 */

#include "deskmate.h"
#include "dm_ble_gatt.h"

#ifdef CONFIG_DESKMATE_APP

#include <stdio.h>
#include <string.h>

static lv_obj_t *s_app_page;
static lv_obj_t *s_app_status_l;
static lv_obj_t *s_app_tip_l;
static lv_obj_t *s_app_addr_l;
static lv_obj_t *s_start_btn;
static lv_obj_t *s_stop_btn;
static lv_obj_t *s_pair_overlay;
static lv_obj_t *s_pair_arc;
static lv_obj_t *s_pair_label;
static lv_anim_t s_arc_anim;
static volatile int s_pair_anim_active;
static volatile int s_anim_done_flag; /* 1 success 2 fail */

static void app_back(lv_event_t *e)
{
  (void)e;
  dm_show(PAGE_SETTINGS);
}

static void app_set_tip(const char *zh, const char *en)
{
  if (s_app_tip_l)
    {
      lv_label_set_text(s_app_tip_l, dm_t(zh, en));
    }
}

static void pair_arc_anim_cb(void *var, int32_t v)
{
  lv_arc_set_value((lv_obj_t *)var, v);
}

static void pair_overlay_close_cb(lv_timer_t *t)
{
  lv_timer_del(t);
  if (s_pair_overlay)
    {
      lv_obj_del(s_pair_overlay);
      s_pair_overlay = NULL;
      s_pair_arc = NULL;
      s_pair_label = NULL;
      s_pair_anim_active = 0;
    }
}

static void pair_anim_create(void)
{
  if (s_pair_overlay)
    {
      return;
    }

  s_pair_overlay = lv_obj_create(s_app_page);
  lv_obj_set_size(s_pair_overlay, 320, 240);
  lv_obj_set_pos(s_pair_overlay, 0, 0);
  lv_obj_set_style_bg_color(s_pair_overlay, lv_color_hex(0x0D0D1A), 0);
  lv_obj_set_style_bg_opa(s_pair_overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_pair_overlay, 0, 0);
  lv_obj_set_style_radius(s_pair_overlay, 0, 0);
  lv_obj_clear_flag(s_pair_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_move_foreground(s_pair_overlay);

  s_pair_arc = lv_arc_create(s_pair_overlay);
  lv_obj_set_size(s_pair_arc, 110, 110);
  lv_obj_center(s_pair_arc);
  lv_obj_set_style_arc_color(s_pair_arc, lv_color_hex(0x2A2A4A), LV_PART_MAIN);
  lv_obj_set_style_arc_color(s_pair_arc, lv_color_hex(0x66CCFF),
                             LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(s_pair_arc, 8, LV_PART_MAIN);
  lv_obj_set_style_arc_width(s_pair_arc, 8, LV_PART_INDICATOR);
  lv_arc_set_bg_angles(s_pair_arc, 0, 360);
  lv_arc_set_value(s_pair_arc, 25);
  lv_obj_remove_flag(s_pair_arc, LV_OBJ_FLAG_CLICKABLE);

  s_pair_label = lv_label_create(s_pair_overlay);
  lv_label_set_text(s_pair_label, dm_t("配对中…", "Pairing…"));
  lv_obj_set_style_text_color(s_pair_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(s_pair_label, g_dm_font_m, 0);
  lv_obj_align_to(s_pair_label, s_pair_arc, LV_ALIGN_OUT_BOTTOM_MID, 0, 12);

  lv_anim_init(&s_arc_anim);
  lv_anim_set_var(&s_arc_anim, s_pair_arc);
  lv_anim_set_exec_cb(&s_arc_anim, pair_arc_anim_cb);
  lv_anim_set_time(&s_arc_anim, 800);
  lv_anim_set_repeat_count(&s_arc_anim, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_playback_time(&s_arc_anim, 800);
  lv_anim_set_values(&s_arc_anim, 25, 75);
  lv_anim_start(&s_arc_anim);

  s_pair_anim_active = 1;
}

static void pair_anim_finish(int ok)
{
  if (!s_pair_anim_active || !s_pair_arc)
    {
      return;
    }
  lv_anim_del(s_pair_arc, pair_arc_anim_cb);
  lv_arc_set_value(s_pair_arc, 100);
  if (ok)
    {
      lv_obj_set_style_arc_color(s_pair_arc, lv_color_hex(0x44FF88),
                                 LV_PART_INDICATOR);
      lv_label_set_text(s_pair_label, dm_t("配对成功!", "Paired!"));
      lv_obj_set_style_text_color(s_pair_label, lv_color_hex(0x44FF88), 0);
    }
  else
    {
      lv_obj_set_style_arc_color(s_pair_arc, lv_color_hex(0xFF4444),
                                 LV_PART_INDICATOR);
      lv_label_set_text(s_pair_label, dm_t("配对失败", "Pair failed"));
      lv_obj_set_style_text_color(s_pair_label, lv_color_hex(0xFF4444), 0);
    }
  s_pair_anim_active = 0;
  lv_timer_create(pair_overlay_close_cb, 1500, NULL);
}

static void refresh_status(void)
{
  dm_ble_state_t st = dm_ble_state();
  const char *zh = "未连接";
  const char *en = "Idle";
  char addr[24];

  switch (st)
    {
    case DM_BLE_STARTING:
      zh = "正在启动…";
      en = "Starting…";
      break;
    case DM_BLE_ADVERTISING:
      zh = "广播中，等待 App";
      en = "Advertising, wait for App";
      break;
    case DM_BLE_CONNECTED:
      zh = "已连接，等待配对";
      en = "Connected, wait for pair";
      break;
    case DM_BLE_PAIRING:
      zh = "配对中…";
      en = "Pairing…";
      break;
    case DM_BLE_PAIRED:
      zh = "已配对";
      en = "Paired";
      break;
    case DM_BLE_ERROR:
      zh = "启动失败";
      en = "Error";
      break;
    default:
      break;
    }

  if (s_app_status_l)
    {
      lv_label_set_text(s_app_status_l, dm_t(zh, en));
    }

  if (s_app_addr_l)
    {
      if (dm_ble_paired_addr(addr, sizeof(addr)) == 0)
        {
          lv_label_set_text(s_app_addr_l, addr);
        }
      else
        {
          lv_label_set_text(s_app_addr_l, "—");
        }
    }

  if (s_start_btn)
    {
      if (st == DM_BLE_IDLE || st == DM_BLE_ERROR)
        {
          lv_obj_set_style_bg_opa(s_start_btn, LV_OPA_COVER, LV_PART_MAIN);
        }
      else
        {
          lv_obj_set_style_bg_opa(s_start_btn, LV_OPA_40, LV_PART_MAIN);
        }
    }
}

static void on_ble_state(dm_ble_state_t st)
{
  /* only update labels; animation handled in tick */
  (void)st;
}

static void app_start(lv_event_t *e)
{
  (void)e;
  if (dm_ble_state() != DM_BLE_IDLE && dm_ble_state() != DM_BLE_ERROR)
    {
      app_set_tip("已在运行", "Already running");
      return;
    }
  if (dm_ble_start() == 0)
    {
      app_set_tip("已开始广播，请打开 App 扫描",
                  "Advertising, open App to scan");
    }
  else
    {
      app_set_tip("启动失败，请检查蓝牙", "Start failed, check BT");
    }
  refresh_status();
}

static void app_stop(lv_event_t *e)
{
  (void)e;
  dm_ble_stop();
  app_set_tip("已停止", "Stopped");
  refresh_status();
}

void dm_appconn_tick(void)
{
  static int s_pairing_latched;

  refresh_status();

  if (s_anim_done_flag)
    {
      int ok = (s_anim_done_flag == 1);
      s_anim_done_flag = 0;
      pair_anim_finish(ok);
    }

  if (dm_ble_state() == DM_BLE_PAIRING && !s_pairing_latched)
    {
      s_pairing_latched = 1;
      pair_anim_create();
    }
  else if (dm_ble_state() == DM_BLE_PAIRED && s_pairing_latched)
    {
      s_pairing_latched = 0;
      s_anim_done_flag = 1;
    }
  else if (dm_ble_state() == DM_BLE_CONNECTED && s_pairing_latched)
    {
      /* pairing aborted */
      s_pairing_latched = 0;
      s_anim_done_flag = 2;
    }
}

void dm_create_app_connect(void)
{
  lv_obj_t *title, *back, *c_cur, *lab;

  s_app_page = lv_obj_create(g_dm_root);
  lv_obj_set_size(s_app_page, DM_SCR_W, DM_SCR_H);
  lv_obj_set_pos(s_app_page, 0, 0);
  lv_obj_set_style_bg_color(s_app_page, lv_color_hex(C_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(s_app_page, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_app_page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_app_page, 0, LV_PART_MAIN);
  lv_obj_clear_flag(s_app_page, LV_OBJ_FLAG_SCROLLABLE);
  g_dm_pages[PAGE_APP_CONNECT] = s_app_page;

  back = dm_btn(s_app_page, "←", "<", 36, 24, C_BTN, C_MUTED, app_back, NULL);
  lv_obj_set_pos(back, 8, 6);
  title = dm_lbl(s_app_page, "App 连接", "App Connect", g_dm_font_m, C_INK);
  lv_obj_set_pos(title, 50, 8);

  c_cur = lv_obj_create(s_app_page);
  lv_obj_set_size(c_cur, 304, 88);
  lv_obj_set_pos(c_cur, 8, 38);
  lv_obj_set_style_bg_color(c_cur, lv_color_hex(0x111111), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(c_cur, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(c_cur, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(c_cur, 12, LV_PART_MAIN);
  lv_obj_set_style_pad_all(c_cur, 10, LV_PART_MAIN);
  lv_obj_clear_flag(c_cur, LV_OBJ_FLAG_SCROLLABLE);

  lab = dm_lbl(c_cur, "状态", "Status", g_dm_font_s, C_MUTED);
  lv_obj_set_pos(lab, 0, 0);
  s_app_status_l = dm_lbl(c_cur, "未连接", "Idle", g_dm_font_m, C_INK);
  lv_obj_set_pos(s_app_status_l, 0, 20);
  s_app_addr_l = dm_lbl(c_cur, "—", "—", g_dm_font_s, C_ACCENT);
  lv_obj_set_pos(s_app_addr_l, 0, 48);

  s_start_btn = dm_btn(s_app_page, "开始广播", "Advertise", 130, 30, C_BTN_HI,
                       C_ACCENT, app_start, NULL);
  lv_obj_set_pos(s_start_btn, 20, 140);
  s_stop_btn = dm_btn(s_app_page, "停止", "Stop", 130, 30, C_BTN, C_MUTED,
                      app_stop, NULL);
  lv_obj_set_pos(s_stop_btn, 170, 140);

  s_app_tip_l = dm_lbl(s_app_page,
                       "打开手机 App 扫描并连接 FlowDesk-S1",
                       "Open App, scan & connect FlowDesk-S1",
                       g_dm_font_s, C_MUTED);
  lv_obj_set_pos(s_app_tip_l, 12, 190);

  {
    lv_obj_t *hint = dm_lbl(s_app_page,
                            "配对协议: Challenge-Response / GEMINI-S1",
                            "Pairing: Challenge-Response",
                            g_dm_font_s, C_DIM);
    lv_obj_set_pos(hint, 12, 212);
  }

  dm_ble_set_state_cb(on_ble_state);
  refresh_status();
}

#endif /* CONFIG_DESKMATE_APP */
