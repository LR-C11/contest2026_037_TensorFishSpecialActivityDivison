/****************************************************************************
 * dm_ui_wifi.c — Settings + WiFi independent pages
 *
 * Fixes applied:
 *   1. Scrollable AP list supporting up to 16 nearby hotspots with clear
 *      24px touch rows and signal strength display.
 *   2. Non-overlapping layout on 320x240 screen (with bottom dock hidden on
 *      PAGE_WIFI so "连接所选" / Join button is never blocked).
 *   3. Added LVGL on-screen virtual keyboard (lv_keyboard_create) on
 *      PAGE_WIFI_PW bound to s_pw_in (matching 9.5 demo lv_100ask_xz_ai_main.c)
 *      plus password show/hide toggle and Ready/Cancel event handling.
 *   4. Auto-triggers scan when opening PAGE_WIFI if list is empty.
 ****************************************************************************/

#include "deskmate.h"
#include <stdio.h>
#include <string.h>

#ifdef CONFIG_DESKMATE_APP

#define WIFI_AP_ROWS 16

static lv_obj_t *s_set_page;
static lv_obj_t *s_wifi_sub;
static lv_obj_t *s_wifi_page;
static lv_obj_t *s_cur_ssid_l;
static lv_obj_t *s_cur_ip_l;
static lv_obj_t *s_cur_state_l;
static lv_obj_t *s_tip_l;
static lv_obj_t *s_ap_sc;
static lv_obj_t *s_ap_btns[WIFI_AP_ROWS];
static lv_obj_t *s_ap_lbls[WIFI_AP_ROWS];
static lv_obj_t *s_join_btn;
static lv_obj_t *s_pw_page;
static lv_obj_t *s_pw_ssid_l;
static lv_obj_t *s_pw_in;
static lv_obj_t *s_pw_kb;
static lv_obj_t *s_conn_page;
static lv_obj_t *s_conn_ssid_l;
static lv_obj_t *s_done_page;
static lv_obj_t *s_done_icon;
static lv_obj_t *s_done_title;
static lv_obj_t *s_done_sub;

static char s_pw_buf[64];
static char s_sel_ssid[33];
static bool s_sel_open;
static bool s_need_list_refresh = true;

static lv_obj_t *mk_page(dm_page_t id)
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

static lv_obj_t *card(lv_obj_t *p, int h)
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

static void refresh_wifi_header(void)
{
  int conn = dm_wifi_connected();

  if (s_cur_ssid_l)
    {
      lv_label_set_text(s_cur_ssid_l,
                        conn ? dm_wifi_cur_ssid()
                             : dm_t("未连接", "Disconnected"));
    }
  if (s_cur_ip_l)
    {
      lv_label_set_text(s_cur_ip_l,
                        conn ? dm_wifi_cur_ip() : "—");
    }
  if (s_cur_state_l)
    {
      lv_label_set_text(s_cur_state_l,
                        dm_t(conn ? "已连接" : "离线",
                             conn ? "Online" : "Offline"));
      lv_obj_set_style_text_color(
          s_cur_state_l,
          lv_color_hex(conn ? C_OK : C_MUTED), LV_PART_MAIN);
    }
  if (s_wifi_sub)
    {
      if (conn)
        {
          lv_label_set_text(s_wifi_sub, dm_wifi_cur_ssid());
        }
      else
        {
          lv_label_set_text(s_wifi_sub, dm_t("未连接", "Disconnected"));
        }
    }
}

static void refresh_ap_list(void)
{
  int n = dm_wifi_ap_count();
  int sel = dm_wifi_sel();
  int i;

  if (sel >= 0 && sel < n)
    {
      const char *ssid = NULL;
      bool open = false;
      if (dm_wifi_ap_get(sel, &ssid, NULL, &open) == 0 && ssid)
        {
          strncpy(s_sel_ssid, ssid, sizeof(s_sel_ssid) - 1);
          s_sel_ssid[sizeof(s_sel_ssid) - 1] = 0;
          s_sel_open = open;
        }
    }

  for (i = 0; i < WIFI_AP_ROWS; i++)
    {
      const char *ssid = NULL;
      int rssi = 0;
      bool open = false;

      if (!s_ap_btns[i])
        {
          continue;
        }

      if (i < n && dm_wifi_ap_get(i, &ssid, &rssi, &open) == 0 && ssid)
        {
          char line[56];
          lv_obj_clear_flag(s_ap_btns[i], LV_OBJ_FLAG_HIDDEN);
          lv_snprintf(line, sizeof(line), "%s%s (%ddBm)",
                      open ? "  " : "* ", ssid, rssi);
          if (s_ap_lbls[i])
            {
              lv_label_set_text(s_ap_lbls[i], line);
              lv_obj_set_style_text_color(
                  s_ap_lbls[i],
                  lv_color_hex(i == sel ? C_ACCENT : C_INK),
                  LV_PART_MAIN);
            }
          lv_obj_set_style_bg_color(
              s_ap_btns[i],
              lv_color_hex(i == sel ? 0x0d3a4a : 0x1a1a1a),
              LV_PART_MAIN);
        }
      else
        {
          lv_obj_add_flag(s_ap_btns[i], LV_OBJ_FLAG_HIDDEN);
        }
    }

  if (s_join_btn)
    {
      if (sel >= 0 && sel < n)
        {
          lv_obj_set_style_bg_opa(s_join_btn, LV_OPA_COVER, LV_PART_MAIN);
        }
      else
        {
          lv_obj_set_style_bg_opa(s_join_btn, LV_OPA_50, LV_PART_MAIN);
        }
    }
}

static void set_tip(const char *zh, const char *en)
{
  if (s_tip_l)
    {
      lv_label_set_text(s_tip_l, dm_t(zh, en));
    }
}

static void open_wifi(lv_event_t *e)
{
  (void)e;
  refresh_wifi_header();
  refresh_ap_list();
  s_need_list_refresh = true;
  dm_show(PAGE_WIFI);

  if (dm_wifi_scan_busy())
    {
      set_tip("正在扫描附近热点…", "Scanning nearby APs…");
    }
  else if (dm_wifi_ap_count() == 0)
    {
      if (dm_wifi_start_scan() == 0)
        {
          set_tip("正在扫描附近热点…", "Scanning nearby APs…");
        }
      else
        {
          set_tip("点右上角「扫描」搜索热点", "Tap Scan for nearby APs");
        }
    }
  else
    {
      set_tip("点选热点后点「连接所选」", "Select AP then tap Join");
    }
}

static void back_set(lv_event_t *e)
{
  (void)e;
  refresh_wifi_header();
  dm_show(PAGE_SETTINGS);
}

static void back_wifi(lv_event_t *e)
{
  (void)e;
  refresh_wifi_header();
  dm_show(PAGE_WIFI);
}

static void scan_cb(lv_event_t *e)
{
  (void)e;
  if (dm_wifi_start_scan() == 0)
    {
      set_tip("正在扫描附近热点…", "Scanning nearby APs…");
    }
  else
    {
      set_tip("当前正忙，请稍候重试", "Busy, try again shortly");
    }
}

static void ap_click(lv_event_t *e)
{
  int i = (int)(uintptr_t)lv_event_get_user_data(e);
  const char *ssid = NULL;
  bool open = false;

  if (i < 0 || i >= dm_wifi_ap_count())
    {
      return;
    }
  dm_wifi_set_sel(i);
  dm_wifi_ap_get(i, &ssid, NULL, &open);
  if (ssid)
    {
      strncpy(s_sel_ssid, ssid, sizeof(s_sel_ssid) - 1);
      s_sel_ssid[sizeof(s_sel_ssid) - 1] = 0;
      s_sel_open = open;
    }
  refresh_ap_list();
}

static void join_cb(lv_event_t *e)
{
  int sel = dm_wifi_sel();
  const char *ssid = NULL;
  bool open = false;

  (void)e;
  if (sel < 0 || sel >= dm_wifi_ap_count())
    {
      set_tip("请先扫描并点选一个热点", "Select an AP first");
      return;
    }

  if (dm_wifi_ap_get(sel, &ssid, NULL, &open) == 0 && ssid)
    {
      strncpy(s_sel_ssid, ssid, sizeof(s_sel_ssid) - 1);
      s_sel_ssid[sizeof(s_sel_ssid) - 1] = 0;
      s_sel_open = open;
    }

  if (s_sel_open)
    {
      s_pw_buf[0] = 0;
      if (dm_wifi_connect(s_sel_ssid, NULL) == 0)
        {
          if (s_conn_ssid_l)
            {
              lv_label_set_text(s_conn_ssid_l, s_sel_ssid);
            }
          dm_show(PAGE_WIFI_CONN);
        }
      return;
    }

  s_pw_buf[0] = 0;
  if (s_pw_in)
    {
      lv_textarea_set_text(s_pw_in, "");
    }
  if (s_pw_ssid_l)
    {
      lv_label_set_text(s_pw_ssid_l, s_sel_ssid);
    }
  if (s_pw_kb && s_pw_in)
    {
      lv_keyboard_set_textarea(s_pw_kb, s_pw_in);
    }
  dm_show(PAGE_WIFI_PW);
}

static void pw_toggle_vis(lv_event_t *e)
{
  (void)e;
  if (s_pw_in)
    {
      bool cur = lv_textarea_get_password_mode(s_pw_in);
      lv_textarea_set_password_mode(s_pw_in, !cur);
    }
}

static void pw_ok(lv_event_t *e)
{
  const char *pw;
  (void)e;

  pw = s_pw_in ? lv_textarea_get_text(s_pw_in) : "";
  if (pw)
    {
      strncpy(s_pw_buf, pw, sizeof(s_pw_buf) - 1);
      s_pw_buf[sizeof(s_pw_buf) - 1] = 0;
    }
  else
    {
      s_pw_buf[0] = 0;
    }

  /* Prevent sending empty password for an encrypted AP */
  if (!s_sel_open && s_pw_buf[0] == 0)
    {
      if (s_pw_ssid_l)
        {
          char msg[48];
          lv_snprintf(msg, sizeof(msg), "%s (%s)", s_sel_ssid,
                      dm_t("请输入密码", "Enter password"));
          lv_label_set_text(s_pw_ssid_l, msg);
        }
      return;
    }

  if (dm_wifi_connect(s_sel_ssid, s_pw_buf) == 0)
    {
      if (s_conn_ssid_l)
        {
          lv_label_set_text(s_conn_ssid_l, s_sel_ssid);
        }
      dm_show(PAGE_WIFI_CONN);
    }
}

/* Match 9.5 demo ta_event_cb: handle LV_EVENT_READY / LV_EVENT_CANCEL from lv_keyboard */
static void pw_ta_event_cb(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *ta = lv_event_get_target(e);
  lv_obj_t *kb = (lv_obj_t *)lv_event_get_user_data(e);

  if (code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED)
    {
      if (kb != NULL)
        {
          lv_keyboard_set_textarea(kb, ta);
        }
    }
  else if (code == LV_EVENT_READY)
    {
      pw_ok(e);
    }
  else if (code == LV_EVENT_CANCEL)
    {
      back_wifi(e);
    }
}

static void done_ok(lv_event_t *e)
{
  (void)e;
  dm_wifi_clear_conn_flags();
  refresh_wifi_header();
  dm_show(PAGE_WIFI);
}

/* ---------- pages ---------- */

void dm_create_settings(void)
{
  lv_obj_t *title;
  lv_obj_t *c1;
  lv_obj_t *c2;
  lv_obj_t *name;
  lv_obj_t *arrow;

  s_set_page = mk_page(PAGE_SETTINGS);
  title = dm_lbl(s_set_page, "设定", "Settings", g_dm_font_m, C_INK);
  lv_obj_set_pos(title, 12, 8);

  c1 = card(s_set_page, 56);
  lv_obj_align(c1, LV_ALIGN_TOP_MID, 0, 40);
  lv_obj_add_flag(c1, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(c1, open_wifi, LV_EVENT_CLICKED, NULL);
  name = dm_lbl(c1, "Wi-Fi", "Wi-Fi", g_dm_font_s, C_INK);
  lv_obj_set_pos(name, 0, 2);
  s_wifi_sub = dm_lbl(c1, "未连接", "Disconnected", g_dm_font_s, C_MUTED);
  lv_obj_set_pos(s_wifi_sub, 0, 22);
  arrow = dm_lbl(c1, "›", "›", g_dm_font_m, C_MUTED);
  lv_obj_align(arrow, LV_ALIGN_RIGHT_MID, -4, 0);

  c2 = card(s_set_page, 48);
  lv_obj_align(c2, LV_ALIGN_TOP_MID, 0, 104);
  name = dm_lbl(c2, "关于", "About", g_dm_font_s, C_INK);
  lv_obj_set_pos(name, 0, 2);
  {
    lv_obj_t *v = dm_lbl(c2, "心流桌伴 " DM_VER, "心流桌伴 " DM_VER,
                         g_dm_font_s, C_MUTED);
    lv_obj_set_pos(v, 0, 20);
  }
}

void dm_create_wifi(void)
{
  lv_obj_t *title;
  lv_obj_t *back;
  lv_obj_t *scan;
  lv_obj_t *c_cur;
  lv_obj_t *c_list;
  lv_obj_t *lab;
  int i;

  s_wifi_page = mk_page(PAGE_WIFI);
  back = dm_btn(s_wifi_page, "←", "<", 36, 24, C_BTN, C_MUTED, back_set,
                NULL);
  lv_obj_set_pos(back, 8, 6);
  title = dm_lbl(s_wifi_page, "Wi-Fi", "Wi-Fi", g_dm_font_m, C_INK);
  lv_obj_set_pos(title, 50, 8);
  scan = dm_btn(s_wifi_page, "扫描", "Scan", 64, 24, C_BTN_HI, C_ACCENT,
                scan_cb, NULL);
  lv_obj_set_pos(scan, 248, 6);

  /* Current connection status card (y = 34..76) */
  c_cur = card(s_wifi_page, 42);
  lv_obj_align(c_cur, LV_ALIGN_TOP_MID, 0, 34);
  s_cur_ssid_l = dm_lbl(c_cur, "未连接", "Disconnected", g_dm_font_s, C_INK);
  lv_obj_set_pos(s_cur_ssid_l, 0, 0);
  s_cur_ip_l = dm_lbl(c_cur, "—", "—", g_dm_font_s, C_MUTED);
  lv_obj_set_pos(s_cur_ip_l, 0, 14);
  s_cur_state_l = dm_lbl(c_cur, "离线", "Offline", g_dm_font_s, C_MUTED);
  lv_obj_align(s_cur_state_l, LV_ALIGN_RIGHT_MID, 0, 0);

  /* Nearby networks scrollable card (y = 80..184) */
  c_list = card(s_wifi_page, 104);
  lv_obj_align(c_list, LV_ALIGN_TOP_MID, 0, 80);
  lab = dm_lbl(c_list, "附近网络 (上下滑动选择)", "Nearby APs (scroll)",
               g_dm_font_s, C_MUTED);
  lv_obj_set_pos(lab, 0, 0);

  s_ap_sc = lv_obj_create(c_list);
  lv_obj_set_size(s_ap_sc, 288, 70);
  lv_obj_set_pos(s_ap_sc, 0, 18);
  lv_obj_set_style_bg_opa(s_ap_sc, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_ap_sc, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_ap_sc, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_row(s_ap_sc, 4, LV_PART_MAIN);
  lv_obj_set_scroll_dir(s_ap_sc, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(s_ap_sc, LV_SCROLLBAR_MODE_AUTO);
  lv_obj_set_flex_flow(s_ap_sc, LV_FLEX_FLOW_COLUMN);

  for (i = 0; i < WIFI_AP_ROWS; i++)
    {
      s_ap_btns[i] = dm_btn(s_ap_sc, "", "", 280, 24, 0x1a1a1a, C_INK,
                            ap_click, (void *)(uintptr_t)i);
      lv_obj_set_style_radius(s_ap_btns[i], 6, LV_PART_MAIN);
      if (lv_obj_get_child_count(s_ap_btns[i]) > 0)
        {
          s_ap_lbls[i] = lv_obj_get_child(s_ap_btns[i], 0);
          lv_obj_align(s_ap_lbls[i], LV_ALIGN_LEFT_MID, 8, 0);
        }
      lv_obj_add_flag(s_ap_btns[i], LV_OBJ_FLAG_HIDDEN);
    }

  /* Tip label (y = 188) and Join button (y = 206..234) — no overlap! */
  s_tip_l = dm_lbl(s_wifi_page, "点「扫描」搜索附近热点", "Tap Scan",
                   g_dm_font_s, C_DIM);
  lv_obj_set_style_text_align(s_tip_l, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_align(s_tip_l, LV_ALIGN_TOP_MID, 0, 187);

  s_join_btn = dm_btn(s_wifi_page, "连接所选", "Join Selected", 136, 28,
                      C_FACE, C_EYE, join_cb, NULL);
  lv_obj_align(s_join_btn, LV_ALIGN_TOP_MID, 0, 206);

  /* ---------- Password page with on-screen LVGL keyboard ---------- */
  s_pw_page = mk_page(PAGE_WIFI_PW);
  back = dm_btn(s_pw_page, "←", "<", 36, 24, C_BTN, C_MUTED, back_wifi,
                NULL);
  lv_obj_set_pos(back, 8, 6);
  title = dm_lbl(s_pw_page, "输入密码", "Password", g_dm_font_s, C_INK);
  lv_obj_set_pos(title, 50, 10);

  {
    lv_obj_t *vis = dm_btn(s_pw_page, "显/隐", "Show", 52, 24, C_BTN_HI,
                           C_DIM, pw_toggle_vis, NULL);
    lv_obj_set_pos(vis, 174, 6);

    lv_obj_t *ok = dm_btn(s_pw_page, "连接", "Join", 78, 24, C_FACE, C_EYE,
                          pw_ok, NULL);
    lv_obj_set_pos(ok, 234, 6);
  }

  {
    lv_obj_t *c = card(s_pw_page, 68);
    lv_obj_align(c, LV_ALIGN_TOP_MID, 0, 34);
    s_pw_ssid_l = dm_lbl(c, "SSID", "SSID", g_dm_font_s, C_ACCENT);
    lv_obj_set_pos(s_pw_ssid_l, 0, 0);

    s_pw_in = lv_textarea_create(c);
    lv_obj_set_size(s_pw_in, 288, 34);
    lv_obj_set_pos(s_pw_in, 0, 18);
    lv_textarea_set_one_line(s_pw_in, true);
    lv_textarea_set_password_mode(s_pw_in, false);
    lv_textarea_set_max_length(s_pw_in, 63);
    lv_obj_set_style_bg_color(s_pw_in, lv_color_hex(0x0a0a0a), LV_PART_MAIN);
    lv_obj_set_style_text_color(s_pw_in, lv_color_hex(C_INK), LV_PART_MAIN);
    lv_obj_set_style_border_width(s_pw_in, 1, LV_PART_MAIN);
    lv_obj_set_style_border_color(s_pw_in, lv_color_hex(0x3a3a3a),
                                  LV_PART_MAIN);
    lv_obj_set_style_text_font(s_pw_in, g_dm_font_s, LV_PART_MAIN);
  }

  /* Create on-screen LVGL keyboard (aligned with 9.5 demo lv_100ask_xz_ai_main.c) */
  s_pw_kb = lv_keyboard_create(s_pw_page);
  lv_obj_set_size(s_pw_kb, DM_SCR_W, 132);
  lv_obj_align(s_pw_kb, LV_ALIGN_BOTTOM_MID, 0, 0);
  lv_keyboard_set_textarea(s_pw_kb, s_pw_in);
  lv_obj_add_event_cb(s_pw_in, pw_ta_event_cb, LV_EVENT_ALL, s_pw_kb);

  /* ---------- Connecting page ---------- */
  s_conn_page = mk_page(PAGE_WIFI_CONN);
  title = dm_lbl(s_conn_page, "连接中", "Connecting", g_dm_font_s, C_INK);
  lv_obj_set_pos(title, 12, 8);
  s_conn_ssid_l = dm_lbl(s_conn_page, "", "", g_dm_font_l, C_INK);
  lv_obj_set_style_text_align(s_conn_ssid_l, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN);
  lv_obj_align(s_conn_ssid_l, LV_ALIGN_TOP_MID, 0, 80);
  {
    lv_obj_t *h = dm_lbl(s_conn_page, "认证并获取 IP…", "Auth + DHCP…",
                         g_dm_font_s, C_DIM);
    lv_obj_set_style_text_align(h, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
    lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 120);
  }

  /* ---------- Result page ---------- */
  s_done_page = mk_page(PAGE_WIFI_DONE);
  s_done_icon = dm_lbl(s_done_page, "OK", "OK", g_dm_font_l, C_OK);
  lv_obj_set_style_text_align(s_done_icon, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN);
  lv_obj_align(s_done_icon, LV_ALIGN_TOP_MID, 0, 60);
  s_done_title = dm_lbl(s_done_page, "已连接", "Connected", g_dm_font_m,
                        C_INK);
  lv_obj_set_style_text_align(s_done_title, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN);
  lv_obj_align(s_done_title, LV_ALIGN_TOP_MID, 0, 100);
  s_done_sub = dm_lbl(s_done_page, "", "", g_dm_font_s, C_MUTED);
  lv_obj_set_style_text_align(s_done_sub, LV_TEXT_ALIGN_CENTER,
                              LV_PART_MAIN);
  lv_obj_align(s_done_sub, LV_ALIGN_TOP_MID, 0, 128);
  {
    lv_obj_t *ok = dm_btn(s_done_page, "返回 Wi-Fi", "Back", 120, 30,
                          C_FACE, C_EYE, done_ok, NULL);
    lv_obj_align(ok, LV_ALIGN_TOP_MID, 0, 160);
  }
}

void dm_wifi_tick(void)
{
  if (g_dm.page == PAGE_SETTINGS || g_dm.page == PAGE_WIFI)
    {
      refresh_wifi_header();
    }

  if (g_dm.page == PAGE_WIFI)
    {
      if (dm_wifi_scan_ready())
        {
          dm_wifi_clear_scan_ready();
          s_need_list_refresh = true;
          refresh_ap_list();
          if (dm_wifi_ap_count() > 0)
            {
              char b[48];
              lv_snprintf(b, sizeof(b), "%s %d %s",
                          dm_t("找到", "Found"), dm_wifi_ap_count(),
                          dm_t("个热点，点选后连接", "APs, tap to select"));
              set_tip(b, b);
            }
          else
            {
              set_tip("未找到热点，可再点「扫描」", "No APs, tap Scan again");
            }
        }
    }

  if (g_dm.page == PAGE_WIFI_CONN)
    {
      if (dm_wifi_conn_ok())
        {
          dm_wifi_clear_conn_flags();
          refresh_wifi_header();
          if (s_done_title)
            {
              lv_label_set_text(s_done_title, dm_t("已连接", "Connected"));
              lv_obj_set_style_text_color(s_done_title, lv_color_hex(C_OK),
                                          LV_PART_MAIN);
            }
          if (s_done_icon)
            {
              lv_label_set_text(s_done_icon, "OK");
              lv_obj_set_style_text_color(s_done_icon, lv_color_hex(C_OK),
                                          LV_PART_MAIN);
            }
          if (s_done_sub)
            {
              lv_label_set_text(s_done_sub, dm_wifi_cur_ip());
            }
          dm_show(PAGE_WIFI_DONE);
        }
      else if (dm_wifi_conn_fail())
        {
          dm_wifi_clear_conn_flags();
          refresh_wifi_header();
          if (s_done_title)
            {
              lv_label_set_text(s_done_title,
                                dm_t("连接失败", "Failed"));
              lv_obj_set_style_text_color(s_done_title, lv_color_hex(C_HEART),
                                          LV_PART_MAIN);
            }
          if (s_done_icon)
            {
              lv_label_set_text(s_done_icon, "X");
              lv_obj_set_style_text_color(s_done_icon, lv_color_hex(C_HEART),
                                          LV_PART_MAIN);
            }
          if (s_done_sub)
            {
              lv_label_set_text(s_done_sub,
                                dm_t("请检查密码或重试", "Check password"));
            }
          dm_show(PAGE_WIFI_DONE);
        }
    }
}

#endif /* CONFIG_DESKMATE_APP */
