/****************************************************************************
 * desk_ui_wifi.c — Wi-Fi picker page
 ****************************************************************************/

#include "desk_companion.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef CONFIG_DESK_COMPANION_APP

#define WIFI_SCAN_FILE "/tmp/desk_wifi_scan.txt"
#define WIFI_CONF_PATH "/data/etc/wifi/wapi.conf"
#define MAX_APS 24
#define SSID_MAX 32

typedef enum { WIFI_IDLE = 0, WIFI_SCANNING, WIFI_CONNECTING, WIFI_WAIT_LINK } wifi_st_t;

typedef struct {
  char ssid[SSID_MAX + 1];
  int rssi;
} ap_t;

static ap_t g_aps[MAX_APS];
static int g_apn;
static wifi_st_t g_wst;
static int g_wwait;
static char g_ssid[SSID_MAX + 1];
static lv_obj_t *g_ap_list;
static lv_obj_t *g_pwd_ta;
static lv_obj_t *g_conn_btn;
static lv_obj_t *g_wifi_pg_st;

static void wifi_st(const char *zh, const char *en)
{
  if (g_wifi_pg_st)
    lv_label_set_text(g_wifi_pg_st, T(zh, en));
  if (g_wifi_card_st)
    lv_label_set_text(g_wifi_card_st, T(zh, en));
}

static void clear_list(lv_obj_t *lst)
{
  if (lst)
    lv_obj_clean(lst);
}

static bool parse_scan(void)
{
  FILE *fp = fopen(WIFI_SCAN_FILE, "r");
  char line[256];
  g_apn = 0;
  memset(g_aps, 0, sizeof(g_aps));
  if (!fp)
    return false;
  while (fgets(line, sizeof(line), fp) && g_apn < MAX_APS)
    {
      char *p = strstr(line, "ESSID:");
      if (!p)
        continue;
      char ssid[SSID_MAX + 1] = {0};
      p = strchr(p, '"');
      if (!p)
        continue;
      p++;
      size_t i = 0;
      while (*p && *p != '"' && i < SSID_MAX)
        ssid[i++] = *p++;
      if (!ssid[0] || !strcmp(ssid, "<hidden>"))
        continue;
      int dup = 0;
      for (int k = 0; k < g_apn; k++)
        if (!strcmp(g_aps[k].ssid, ssid))
          dup = 1;
      if (dup)
        continue;
      strncpy(g_aps[g_apn].ssid, ssid, SSID_MAX);
      g_aps[g_apn].rssi = 0;
      char *sig = strstr(line, "level:");
      if (sig)
        g_aps[g_apn].rssi = atoi(sig + 6);
      g_apn++;
    }
  fclose(fp);
  return g_apn > 0;
}

static void ap_click(lv_event_t *e)
{
  const char *ssid = lv_event_get_user_data(e);
  if (!ssid)
    return;
  strncpy(g_ssid, ssid, SSID_MAX);
  wifi_st("已选网络", "Selected");
  say("输入密码然后连接", "Enter password then Connect.");
  if (g_pwd_ta)
    {
      lv_textarea_set_text(g_pwd_ta, "");
      lv_obj_clear_flag(g_pwd_ta, LV_OBJ_FLAG_HIDDEN);
    }
  if (g_conn_btn)
    lv_obj_clear_flag(g_conn_btn, LV_OBJ_FLAG_HIDDEN);
  if (g_kb)
    {
      lv_obj_clear_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(g_kb);
      lv_keyboard_set_textarea(g_kb, g_pwd_ta);
    }
}

static void rebuild_ap_list(void)
{
  clear_list(g_ap_list);
  if (!g_ap_list)
    return;
  if (!g_apn)
    {
      lv_obj_t *l = lbl(g_ap_list, "未找到网络", "No APs", g_font_s, C_MUTED);
      lv_obj_center(l);
      return;
    }
  for (int i = 0; i < g_apn; i++)
    {
      lv_obj_t *b = lv_button_create(g_ap_list);
      lv_obj_set_size(b, 280, 32);
      lv_obj_set_style_bg_color(b, lv_color_hex(C_BTN), LV_PART_MAIN);
      char *ssid = strdup(g_aps[i].ssid);
      lv_obj_add_event_cb(b, ap_click, LV_EVENT_CLICKED, ssid);
      char buf[64];
      lv_snprintf(buf, sizeof(buf), "%s  %ddBm", g_aps[i].ssid, g_aps[i].rssi);
      lv_obj_t *l = lbl(b, buf, buf, g_font_s, C_INK);
      lv_obj_center(l);
    }
}

static void wifi_connect(void)
{
  const char *psk = g_pwd_ta ? lv_textarea_get_text(g_pwd_ta) : "";
  if (!g_ssid[0])
    {
      wifi_st("请先选网络", "Select an AP");
      return;
    }
  if (!psk || !psk[0])
    {
      wifi_st("请输入密码", "Password required");
      return;
    }
  if (strchr(g_ssid, '"') || strchr(psk, '"'))
    {
      wifi_st("含非法字符", "Invalid chars");
      return;
    }
  system("mkdir -p /data/etc/wifi");
  FILE *fp = fopen(WIFI_CONF_PATH, "w");
  if (!fp)
    {
      wifi_st("写配置失败", "Write conf failed");
      return;
    }
  fprintf(fp, "{\n  \"ssid\": \"%s\",\n  \"psk\": \"%s\"\n}\n", g_ssid, psk);
  fclose(fp);
  wifi_st("正在连接", "Connecting…");
  say("正在连 Wi-Fi…", "Connecting Wi-Fi…");
  desk_set_face(FACE_THINK);
  g_wst = WIFI_CONNECTING;
  g_wwait = 0;
  system("sh /etc/wifi/start_wifi.sh > /dev/null 2>&1 &");
  if (g_kb)
    lv_obj_add_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
}

static void scan_cb(lv_event_t *e)
{
  (void)e;
  wifi_st("扫描中…", "Scanning…");
  clear_list(g_ap_list);
  g_wst = WIFI_SCANNING;
  g_wwait = 0;
  system("sh -c 'wapi scan wlan0; sleep 2; wapi scan_results wlan0 > "
         WIFI_SCAN_FILE " 2>&1' >/dev/null 2>&1 &");
}

static void conn_cb(lv_event_t *e)
{
  (void)e;
  wifi_connect();
}

void desk_create_wifi(void)
{
  g_page_wifi = desk_page_shell(g_root, C_BG);
  desk_backbar(g_page_wifi, "Wi-Fi 设置", "Wi-Fi Setup");

  lv_obj_t *scan = lv_button_create(g_page_wifi);
  lv_obj_set_size(scan, 100, 30);
  lv_obj_set_pos(scan, 10, 42);
  lv_obj_set_style_bg_color(scan, lv_color_hex(C_BTN), LV_PART_MAIN);
  lv_obj_add_flag(scan, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(scan, scan_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *sl = lbl(scan, "扫描网络", "Scan", g_font_s, C_ACCENT2);
  lv_obj_center(sl);

  g_wifi_pg_st = lbl(g_page_wifi, "点扫描找家里 Wi-Fi", "Scan for home Wi-Fi",
                     g_font_s, C_MUTED);
  lv_obj_set_pos(g_wifi_pg_st, 120, 48);

  g_ap_list = lv_obj_create(g_page_wifi);
  lv_obj_set_size(g_ap_list, 300, 100);
  lv_obj_set_pos(g_ap_list, 10, 80);
  lv_obj_set_style_bg_color(g_ap_list, lv_color_hex(C_BTN_HI), LV_PART_MAIN);
  lv_obj_set_style_border_width(g_ap_list, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_ap_list, 4, LV_PART_MAIN);
  lv_obj_set_style_pad_row(g_ap_list, 3, LV_PART_MAIN);
  lv_obj_set_scroll_dir(g_ap_list, LV_DIR_VER);

  g_pwd_ta = lv_textarea_create(g_page_wifi);
  lv_obj_set_size(g_pwd_ta, 188, 32);
  lv_obj_set_pos(g_pwd_ta, 10, 190);
  lv_textarea_set_one_line(g_pwd_ta, true);
  lv_textarea_set_password_mode(g_pwd_ta, true);
  lv_textarea_set_placeholder_text(g_pwd_ta, T("密码", "Password"));
  style_f(g_pwd_ta, g_font_s, C_INK);
  lv_obj_add_flag(g_pwd_ta, LV_OBJ_FLAG_HIDDEN);

  g_conn_btn = lv_button_create(g_page_wifi);
  lv_obj_set_size(g_conn_btn, 96, 32);
  lv_obj_set_pos(g_conn_btn, 210, 190);
  lv_obj_set_style_bg_color(g_conn_btn, lv_color_hex(C_OK), LV_PART_MAIN);
  lv_obj_add_flag(g_conn_btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(g_conn_btn, conn_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_add_flag(g_conn_btn, LV_OBJ_FLAG_HIDDEN);
  lv_obj_t *cl = lbl(g_conn_btn, "连接", "Connect", g_font_s, 0x0B1220);
  lv_obj_center(cl);
}

void desk_wifi_tick(void)
{
  switch (g_wst)
    {
      case WIFI_SCANNING:
        g_wwait++;
        if (g_wwait >= 8)
          {
            parse_scan();
            rebuild_ap_list();
            wifi_st(g_apn ? "找到网络" : "未找到网络",
                    g_apn ? "APs found" : "No APs");
            g_wst = WIFI_IDLE;
          }
        break;
      case WIFI_CONNECTING:
        g_wwait++;
        if (g_wwait >= 3)
          {
            g_wst = WIFI_WAIT_LINK;
            wifi_st("检查连接", "Checking");
          }
        break;
      case WIFI_WAIT_LINK:
        {
          FILE *fp;
          char line[160];
          int up = 0;
#ifdef CONFIG_SYSTEM_SYSTEM
          system("wapi show wlan0 > /tmp/desk_wifi_status.txt 2>&1");
#endif
          fp = fopen("/tmp/desk_wifi_status.txt", "r");
          if (fp)
            {
              while (fgets(line, sizeof(line), fp))
                {
                  if (strstr(line, "AP: 00:00:00:00:00:00") ||
                      strstr(line, "not connected"))
                    {
                      up = 0;
                      break;
                    }
                  if (strstr(line, "AP:"))
                    up = 1;
                }
              fclose(fp);
            }
          if (up)
            {
              wifi_st("已连接", "Connected");
              say("网络通了，我在呢", "Network up. I'm here.");
              desk_set_face_hold(FACE_HAPPY, 25);
            }
          else
            {
              wifi_st("未连接", "Not connected");
              say("密码或信号再检查下", "Check password / signal.");
            }
          g_wst = WIFI_IDLE;
        }
        break;
      default:
        break;
    }
}

#endif
