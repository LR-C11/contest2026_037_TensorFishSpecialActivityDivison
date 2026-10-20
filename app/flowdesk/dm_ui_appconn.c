/*
 * dm_ui_appconn.c — App Connect page (BLE connect to Android app)
 * Connects to GeminiS1 BLE Android app via BLE
 */

#include "deskmate.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#define APP_DEV_ROWS  6
static volatile int s_app_scan_done;

static lv_obj_t *app_page_new(dm_page_t id)
{
  lv_obj_t *page = lv_obj_create(g_dm_root);
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

static lv_obj_t *app_card(lv_obj_t *p, int h)
{
  lv_obj_t *c = lv_obj_create(p);
  lv_obj_set_width(c, 304);
  lv_obj_set_height(c, h);
  lv_obj_set_style_bg_color(c, lv_color_hex(0x111111), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(c, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(c, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_all(c, 8, LV_PART_MAIN);
  lv_obj_clear_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  return c;
}

static lv_obj_t *s_app_page;
static lv_obj_t *s_app_status_l;
static lv_obj_t *s_app_tip_l;
static lv_obj_t *s_app_sc;
static lv_obj_t *s_app_devs[APP_DEV_ROWS];
static volatile int s_app_scan_busy;
static volatile int s_app_sel = -1;
static char s_app_addrs[APP_DEV_ROWS][18];
static char s_app_names[APP_DEV_ROWS][24];
static int s_app_n;

static lv_obj_t *s_pair_anim_arc;
static lv_obj_t *s_pair_anim_label;
static lv_obj_t *s_pair_overlay;
static lv_anim_t s_arc_anim;
static volatile int s_pair_anim_active;

/* ── 配对动画 ─────────────────────────────────────────── */
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
    }
}

static void pair_anim_create(void)
{
  /* Full-screen overlay */
  s_pair_overlay = lv_obj_create(s_app_page);
  lv_obj_set_size(s_pair_overlay, 320, 240);
  lv_obj_set_pos(s_pair_overlay, 0, 0);
  lv_obj_set_style_bg_color(s_pair_overlay, lv_color_hex(0x0D0D1A), 0);
  lv_obj_set_style_bg_opa(s_pair_overlay, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(s_pair_overlay, 0, 0);
  lv_obj_set_style_radius(s_pair_overlay, 0, 0);
  lv_obj_clear_flag(s_pair_overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_move_foreground(s_pair_overlay);

  /* Spinning arc */
  s_pair_anim_arc = lv_arc_create(s_pair_overlay);
  lv_obj_set_size(s_pair_anim_arc, 120, 120);
  lv_obj_center(s_pair_anim_arc);
  lv_obj_set_style_arc_color(s_pair_anim_arc, lv_color_hex(0x2A2A4A), LV_PART_MAIN);
  lv_obj_set_style_arc_color(s_pair_anim_arc, lv_color_hex(0x66CCFF), LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(s_pair_anim_arc, 8, LV_PART_MAIN);
  lv_obj_set_style_arc_width(s_pair_anim_arc, 8, LV_PART_INDICATOR);
  lv_arc_set_bg_angles(s_pair_anim_arc, 0, 360);
  lv_arc_set_value(s_pair_anim_arc, 25);
  lv_obj_remove_flag(s_pair_anim_arc, LV_OBJ_FLAG_CLICKABLE);

  /* Label */
  s_pair_anim_label = lv_label_create(s_pair_overlay);
  lv_label_set_text(s_pair_anim_label, "配对中...");
  lv_obj_set_style_text_color(s_pair_anim_label, lv_color_hex(0xFFFFFF), 0);
  lv_obj_set_style_text_font(s_pair_anim_label, g_dm_font_m, 0);
  lv_obj_align_to(s_pair_anim_label, s_pair_anim_arc, LV_ALIGN_OUT_BOTTOM_MID, 0, 12);

  /* Spin animation */
  lv_anim_init(&s_arc_anim);
  lv_anim_set_var(&s_arc_anim, s_pair_anim_arc);
  lv_anim_set_exec_cb(&s_arc_anim, pair_arc_anim_cb);
  lv_anim_set_time(&s_arc_anim, 800);
  lv_anim_set_repeat_count(&s_arc_anim, LV_ANIM_REPEAT_INFINITE);
  lv_anim_set_playback_time(&s_arc_anim, 800);
  lv_anim_set_values(&s_arc_anim, 25, 75);
  lv_anim_start(&s_arc_anim);

  s_pair_anim_active = 1;
}

static void pair_anim_success(void)
{
  if (!s_pair_anim_active) return;
  lv_anim_del(s_pair_anim_arc, pair_arc_anim_cb);
  lv_arc_set_value(s_pair_anim_arc, 100);
  lv_obj_set_style_arc_color(s_pair_anim_arc, lv_color_hex(0x44FF88), LV_PART_INDICATOR);
  lv_label_set_text(s_pair_anim_label, "配对成功!");
  lv_obj_set_style_text_color(s_pair_anim_label, lv_color_hex(0x44FF88), 0);
  s_pair_anim_active = 0;

  /* Auto close after 1.5s */
  lv_timer_create(pair_overlay_close_cb, 1500, NULL);
}

static void pair_anim_fail(void)
{
  if (!s_pair_anim_active) return;
  lv_anim_del(s_pair_anim_arc, pair_arc_anim_cb);
  lv_arc_set_value(s_pair_anim_arc, 100);
  lv_obj_set_style_arc_color(s_pair_anim_arc, lv_color_hex(0xFF4444), LV_PART_INDICATOR);
  lv_label_set_text(s_pair_anim_label, "配对失败");
  lv_obj_set_style_text_color(s_pair_anim_label, lv_color_hex(0xFF4444), 0);
  s_pair_anim_active = 0;

  lv_timer_create(pair_overlay_close_cb, 1500, NULL);
}

static void app_back(lv_event_t *e) { (void)e; dm_show(PAGE_SETTINGS); }

static void app_set_tip(const char *zh, const char *en)
{
  lv_label_set_text(s_app_tip_l, dm_t(zh, en));
}

static void app_update_list(void)
{
  int i;
  for (i = 0; i < APP_DEV_ROWS; i++)
    {
      if (i < s_app_n)
        {
          char line[64];
          snprintf(line, sizeof(line), "%s  [%s]", s_app_names[i], s_app_addrs[i]);
          lv_label_set_text(lv_obj_get_child(s_app_devs[i], 0), line);
          lv_obj_clear_flag(s_app_devs[i], LV_OBJ_FLAG_HIDDEN);
        }
      else
        lv_obj_add_flag(s_app_devs[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void app_dev_click(lv_event_t *e)
{
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  if (idx >= 0 && idx < s_app_n)
    {
      s_app_sel = idx;
      char msg[80];
      snprintf(msg, sizeof(msg), "已选择: %s", s_app_names[idx]);
      app_set_tip(msg, msg);
    }
}

static void *app_scan_thread(void *arg)
{
  (void)arg;
  system("bttool scan > /tmp/app_scan.txt 2>&1");
  usleep(3 * 1000 * 1000);

  FILE *fp = fopen("/tmp/app_scan.txt", "r");
  s_app_n = 0;
  s_app_sel = -1;
  if (fp)
    {
      char line[128];
      while (fgets(line, sizeof(line), fp) && s_app_n < APP_DEV_ROWS)
        {
          char addr[18];
          char name[24];
          addr[0] = 0; name[0] = 0;
          if (sscanf(line, " %17[0-9a-fA-F:]", addr) == 1 && strlen(addr) == 17)
            {
              char *n = strstr(line, " ");
              if (n) { snprintf(name, sizeof(name), "%.23s", n+1); name[strcspn(name,"\r\n")] = 0; }
              if (!name[0]) snprintf(name, sizeof(name), "Device-%02d", s_app_n);
              snprintf(s_app_addrs[s_app_n], 18, "%s", addr);
              snprintf(s_app_names[s_app_n], 24, "%s", name);
              s_app_n++;
            }
        }
      fclose(fp);
    }

  s_app_scan_busy = 0;
  /* Do not touch LVGL from this thread; dm_appconn_tick() updates the UI. */
  s_app_scan_done = 1;
  return NULL;
}

void dm_appconn_tick(void)
{
  if (!s_app_scan_done)
    {
      return;
    }
  s_app_scan_done = 0;
  app_set_tip(s_app_n > 0 ? "扫描完成，点选设备后连接" : "未发现设备，请打开 GeminiS1 BLE App",
              s_app_n > 0 ? "Scan done, tap to select" : "No device, open GeminiS1 BLE App");
  app_update_list();
}

static void app_scan(lv_event_t *e)
{
  (void)e;
  if (s_app_scan_busy) return;
  s_app_scan_busy = 1;
  s_app_n = 0; s_app_sel = -1;
  app_update_list();
  app_set_tip("正在扫描附近设备...", "Scanning...");

  pthread_t th;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, 8192);
  if (pthread_create(&th, &attr, app_scan_thread, NULL) == 0)
    pthread_detach(th);
  pthread_attr_destroy(&attr);
}

static void app_connect(lv_event_t *e)
{
  (void)e;
  if (s_app_sel < 0 || s_app_sel >= s_app_n)
    { app_set_tip("请先选择设备", "Please select a device"); return; }

  char cmd[128];
  pair_anim_create();
  if (s_app_status_l)
    {
      lv_label_set_text(s_app_status_l, dm_t("连接中", "Connecting"));
    }
  snprintf(cmd, sizeof(cmd), "bttool connect PAN %s 1 1", s_app_addrs[s_app_sel]);
  system(cmd);
  sleep(2);
  system("renew bt-pan");
  pair_anim_success();
  if (s_app_status_l)
    {
      lv_label_set_text(s_app_status_l, "bt-pan");
    }
  app_set_tip("连接完成，请检查IP", "Connected, check IP");
}

static void app_disconnect(lv_event_t *e)
{
  (void)e;
  if (s_app_sel >= 0 && s_app_sel < s_app_n)
    {
      char cmd[128];
      snprintf(cmd, sizeof(cmd), "bttool disconnect PAN %s", s_app_addrs[s_app_sel]);
      system(cmd);
    }
  app_set_tip("已断开", "Disconnected");
}

void dm_create_app_connect(void)
{
  lv_obj_t *title, *back, *scan, *c_cur, *c_list, *lab, *conn, *disc;
  int i;

  s_app_page = app_page_new(PAGE_APP_CONNECT);
  back = dm_btn(s_app_page, "←", "<", 36, 24, C_BTN, C_MUTED, app_back, NULL);
  lv_obj_set_pos(back, 8, 6);
  title = dm_lbl(s_app_page, "App 连接", "App Connect", g_dm_font_m, C_INK);
  lv_obj_set_pos(title, 50, 8);
  scan = dm_btn(s_app_page, "扫描", "Scan", 64, 24, C_BTN_HI, C_ACCENT, app_scan, NULL);
  lv_obj_set_pos(scan, 248, 6);

  c_cur = app_card(s_app_page, 42);
  lv_obj_align(c_cur, LV_ALIGN_TOP_MID, 0, 34);
  s_app_status_l = dm_lbl(c_cur, "未连接", "Disconnected", g_dm_font_s, C_INK);
  lv_obj_set_pos(s_app_status_l, 0, 0);

  c_list = app_card(s_app_page, 104);
  lv_obj_align(c_list, LV_ALIGN_TOP_MID, 0, 80);
  lab = dm_lbl(c_list, "附近设备 (上下滑动选择)", "Nearby devices (scroll)", g_dm_font_s, C_MUTED);
  lv_obj_set_pos(lab, 0, 0);

  s_app_sc = lv_obj_create(c_list);
  lv_obj_set_size(s_app_sc, 288, 70);
  lv_obj_set_pos(s_app_sc, 0, 18);
  lv_obj_set_style_bg_opa(s_app_sc, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_app_sc, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_app_sc, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(s_app_sc, 4, LV_PART_MAIN);
  lv_obj_set_scroll_dir(s_app_sc, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(s_app_sc, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_flex_flow(s_app_sc, LV_FLEX_FLOW_COLUMN);

  for (i = 0; i < APP_DEV_ROWS; i++)
    {
      s_app_devs[i] = dm_btn(s_app_sc, "", "", 280, 24, 0x1a1a1a, C_INK, app_dev_click, (void*)(intptr_t)i);
      lv_obj_set_style_radius(s_app_devs[i], 6, LV_PART_MAIN);
      lv_obj_add_flag(s_app_devs[i], LV_OBJ_FLAG_HIDDEN);
    }

  conn = dm_btn(s_app_page, "连接", "Connect", 130, 28, C_BTN_HI, C_ACCENT, app_connect, NULL);
  lv_obj_set_pos(conn, 20, 188);
  disc = dm_btn(s_app_page, "断开", "Disconnect", 130, 28, C_BTN, C_MUTED, app_disconnect, NULL);
  lv_obj_set_pos(disc, 170, 188);

  s_app_tip_l = dm_lbl(s_app_page, "请先在手机打开 GeminiS1 BLE App",
                       "Open GeminiS1 BLE App on phone first", g_dm_font_s, C_MUTED);
  lv_obj_set_pos(s_app_tip_l, 12, 220);

  app_update_list();
}
