/*
 * dm_ui_btnet.c — Bluetooth PAN network page
 * Scan / connect / disconnect via bttool PAN commands
 */

#include "deskmate.h"
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#define BT_NET_ROWS  8

static lv_obj_t *bt_page_new(dm_page_t id)
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

static lv_obj_t *bt_card(lv_obj_t *p, int h)
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

static lv_obj_t *s_btnet_page;
static lv_obj_t *s_bt_cur_l;
static lv_obj_t *s_bt_state_l;
static lv_obj_t *s_bt_sc;
static lv_obj_t *s_bt_devs[BT_NET_ROWS];
static lv_obj_t *s_bt_tip_l;
static volatile int s_bt_scan_busy;
static volatile int s_bt_sel = -1;
static char s_bt_addrs[BT_NET_ROWS][18];
static char s_bt_names[BT_NET_ROWS][24];
static int s_bt_n;
static volatile int s_bt_scan_done;

/* Forward */
static void bt_back(lv_event_t *e);
static void bt_scan(lv_event_t *e);
static void bt_connect(lv_event_t *e);
static void bt_disconnect(lv_event_t *e);
static void *bt_scan_thread(void *arg);

static void bt_set_tip(const char *zh, const char *en)
{
  lv_label_set_text(s_bt_tip_l, dm_t(zh, en));
}

static void bt_update_list(void)
{
  int i;
  for (i = 0; i < BT_NET_ROWS; i++)
    {
      if (i < s_bt_n)
        {
          char line[64];
          snprintf(line, sizeof(line), "%s  [%s]", s_bt_names[i], s_bt_addrs[i]);
          lv_label_set_text(lv_obj_get_child(s_bt_devs[i], 0), line);
          lv_obj_clear_flag(s_bt_devs[i], LV_OBJ_FLAG_HIDDEN);
        }
      else
        {
          lv_obj_add_flag(s_bt_devs[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void bt_dev_click(lv_event_t *e)
{
  int idx = (int)(intptr_t)lv_event_get_user_data(e);
  if (idx >= 0 && idx < s_bt_n)
    {
      s_bt_sel = idx;
      char msg[80];
      snprintf(msg, sizeof(msg), "已选择: %s", s_bt_names[idx]);
      bt_set_tip(msg, msg);
    }
}

/* Scan thread — runs bttool scan in background */
static void *bt_scan_thread(void *arg)
{
  (void)arg;
  /* Use bttool to scan BT devices */
  system("bttool scan > /tmp/bt_scan.txt 2>&1");
  usleep(3 * 1000 * 1000); /* wait 3s for scan */

  /* Parse results */
  FILE *fp = fopen("/tmp/bt_scan.txt", "r");
  s_bt_n = 0;
  s_bt_sel = -1;
  if (fp)
    {
      char line[128];
      while (fgets(line, sizeof(line), fp) && s_bt_n < BT_NET_ROWS)
        {
          /* Look for MAC address pattern XX:XX:XX:XX:XX:XX */
          char *p = strstr(line, ":");
          if (p && p > line)
            {
              /* Try to extract MAC */
              char addr[18];
              char name[24];
              addr[0] = 0;
              name[0] = 0;
              if (sscanf(line, " %17[0-9a-fA-F:]", addr) == 1 &&
                  strlen(addr) == 17)
                {
                  /* Extract name if present */
                  char *n = strstr(line, " ");
                  if (n)
                    {
                      snprintf(name, sizeof(name), "%.23s", n + 1);
                      /* trim */
                      name[strcspn(name, "\r\n")] = 0;
                    }
                  if (!name[0])
                    snprintf(name, sizeof(name), "Device-%02d", s_bt_n);
                  snprintf(s_bt_addrs[s_bt_n], 18, "%s", addr);
                  snprintf(s_bt_names[s_bt_n], 24, "%s", name);
                  s_bt_n++;
                }
            }
        }
      fclose(fp);
    }

  s_bt_scan_busy = 0;
  /* Do not touch LVGL from this thread; dm_btnet_tick() updates the UI. */
  s_bt_scan_done = 1;
  return NULL;
}

void dm_btnet_tick(void)
{
  if (!s_bt_scan_done)
    {
      return;
    }
  s_bt_scan_done = 0;
  if (s_bt_n > 0)
    {
      bt_set_tip("扫描完成，点选设备", "Scan done, tap to select");
    }
  else
    {
      bt_set_tip("未发现设备，请确认手机已开蓝牙共享",
                 "No devices, enable BT tethering on phone");
    }
  bt_update_list();
}

static void bt_scan(lv_event_t *e)
{
  (void)e;
  if (s_bt_scan_busy)
    return;
  s_bt_scan_busy = 1;
  s_bt_n = 0;
  s_bt_sel = -1;
  bt_update_list();
  bt_set_tip("正在扫描附近蓝牙设备...", "Scanning nearby BT devices...");

  pthread_t th;
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, 8192);
  if (pthread_create(&th, &attr, bt_scan_thread, NULL) == 0)
    pthread_detach(th);
  pthread_attr_destroy(&attr);
}

static void bt_connect(lv_event_t *e)
{
  (void)e;
  if (s_bt_sel < 0 || s_bt_sel >= s_bt_n)
    {
      bt_set_tip("请先选择设备", "Please select a device");
      return;
    }

  char cmd[128];
  char msg[80];
  snprintf(msg, sizeof(msg), "正在连接 %s...", s_bt_names[s_bt_sel]);
  bt_set_tip(msg, msg);

  /* bttool connect PAN <addr> <dstrole> <srcrole> */
  snprintf(cmd, sizeof(cmd), "bttool connect PAN %s 1 1", s_bt_addrs[s_bt_sel]);
  system(cmd);
  sleep(2);

  /* Try to get IP */
  system("renew bt-pan");

  bt_set_tip("连接完成，请检查IP", "Connected, check IP");
}

static void bt_disconnect(lv_event_t *e)
{
  (void)e;
  if (s_bt_sel >= 0 && s_bt_sel < s_bt_n)
    {
      char cmd[128];
      snprintf(cmd, sizeof(cmd), "bttool disconnect PAN %s", s_bt_addrs[s_bt_sel]);
      system(cmd);
    }
  bt_set_tip("已断开", "Disconnected");
}

static void bt_back(lv_event_t *e)
{
  (void)e;
  dm_show(PAGE_SETTINGS);
}

void dm_create_btnet(void)
{
  lv_obj_t *title, *back, *scan, *c_cur, *c_list, *lab, *conn, *disc;
  int i;

  s_btnet_page = bt_page_new(PAGE_BT_NET);
  back = dm_btn(s_btnet_page, "←", "<", 36, 24, C_BTN, C_MUTED, bt_back, NULL);
  lv_obj_set_pos(back, 8, 6);
  title = dm_lbl(s_btnet_page, "蓝牙网络", "BT Network", g_dm_font_m, C_INK);
  lv_obj_set_pos(title, 50, 8);
  scan = dm_btn(s_btnet_page, "扫描", "Scan", 64, 24, C_BTN_HI, C_ACCENT,
                bt_scan, NULL);
  lv_obj_set_pos(scan, 248, 6);

  /* Current status */
  c_cur = bt_card(s_btnet_page, 42);
  lv_obj_align(c_cur, LV_ALIGN_TOP_MID, 0, 34);
  s_bt_cur_l = dm_lbl(c_cur, "未连接", "Disconnected", g_dm_font_s, C_INK);
  lv_obj_set_pos(s_bt_cur_l, 0, 0);
  s_bt_state_l = dm_lbl(c_cur, "离线", "Offline", g_dm_font_s, C_MUTED);
  lv_obj_align(s_bt_state_l, LV_ALIGN_RIGHT_MID, 0, 0);

  /* Device list scrollable */
  c_list = bt_card(s_btnet_page, 104);
  lv_obj_align(c_list, LV_ALIGN_TOP_MID, 0, 80);
  lab = dm_lbl(c_list, "附近设备 (上下滑动选择)", "Nearby devices (scroll)",
               g_dm_font_s, C_MUTED);
  lv_obj_set_pos(lab, 0, 0);

  s_bt_sc = lv_obj_create(c_list);
  lv_obj_set_size(s_bt_sc, 288, 70);
  lv_obj_set_pos(s_bt_sc, 0, 18);
  lv_obj_set_style_bg_opa(s_bt_sc, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_bt_sc, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_bt_sc, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(s_bt_sc, 4, LV_PART_MAIN);
  lv_obj_set_scroll_dir(s_bt_sc, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(s_bt_sc, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_flex_flow(s_bt_sc, LV_FLEX_FLOW_COLUMN);

  for (i = 0; i < BT_NET_ROWS; i++)
    {
      s_bt_devs[i] = dm_btn(s_bt_sc, "", "", 280, 24, 0x1a1a1a, C_INK,
                            bt_dev_click, (void *)(intptr_t)i);
      lv_obj_set_style_radius(s_bt_devs[i], 6, LV_PART_MAIN);
      lv_obj_add_flag(s_bt_devs[i], LV_OBJ_FLAG_HIDDEN);
    }

  /* Connect / Disconnect buttons */
  conn = dm_btn(s_btnet_page, "连接", "Connect", 130, 28, C_BTN_HI, C_ACCENT,
                bt_connect, NULL);
  lv_obj_set_pos(conn, 20, 188);
  disc = dm_btn(s_btnet_page, "断开", "Disconnect", 130, 28, C_BTN, C_MUTED,
                bt_disconnect, NULL);
  lv_obj_set_pos(disc, 170, 188);

  /* Tip */
  s_bt_tip_l = dm_lbl(s_btnet_page, "手机开启「蓝牙网络共享」后点击扫描",
                      "Enable BT tethering on phone, then scan",
                      g_dm_font_s, C_MUTED);
  lv_obj_set_pos(s_bt_tip_l, 12, 220);

  bt_update_list();
}
