/****************************************************************************
 * desk_ui_bt.c
 ****************************************************************************/

#include "desk_companion.h"
#include <string.h>
#include <stdio.h>

#ifdef CONFIG_DESK_COMPANION_APP

#ifdef CONFIG_BLUETOOTH_SERVICE
#  include "bluetooth.h"
#  include "bt_adapter.h"
#  include "bt_addr.h"
#endif

#define MAX_BT 16
#define BT_NAME_MAX 32

#ifdef CONFIG_BLUETOOTH_SERVICE
static bt_instance_t *g_bt;
static bool g_bt_prepared;
static bool g_bt_scan_req;
typedef struct {
  char name[BT_NAME_MAX + 1];
  char addr[18];
} btd_t;
static btd_t g_btd[MAX_BT];
static int g_btn;
#endif
static lv_obj_t *g_bt_list;
static lv_obj_t *g_bt_pg_st;

#ifdef CONFIG_BLUETOOTH_SERVICE
static void on_disc_result(void *cookie, bt_discovery_result_t *r)
{
  (void)cookie;
  if (!r || g_btn >= MAX_BT)
    return;
  char addr[18] = {0};
  bt_addr_ba2str(&r->addr, addr);
  for (int i = 0; i < g_btn; i++)
    if (!strcmp(g_btd[i].addr, addr))
      return;
  strncpy(g_btd[g_btn].addr, addr, 17);
  if (r->name[0])
    strncpy(g_btd[g_btn].name, r->name, BT_NAME_MAX - 1);
  else
    snprintf(g_btd[g_btn].name, BT_NAME_MAX, "%s", addr);
  g_btn++;
}

static void on_disc_state(void *cookie, bt_discovery_state_t s)
{
  (void)cookie;
  (void)s;
}
#endif

static void rebuild_bt(void)
{
  if (!g_bt_list)
    return;
  lv_obj_clean(g_bt_list);
#ifdef CONFIG_BLUETOOTH_SERVICE
  if (!g_btn)
    {
      lv_obj_t *l = lbl(g_bt_list, "暂无设备，点扫描", "Scan for devices",
                        g_font_s, C_MUTED);
      lv_obj_center(l);
      return;
    }
  for (int i = 0; i < g_btn; i++)
    {
      lv_obj_t *b = lv_button_create(g_bt_list);
      lv_obj_set_size(b, 280, 36);
      lv_obj_set_style_bg_color(b, lv_color_hex(C_BTN), LV_PART_MAIN);
      char buf[80];
      lv_snprintf(buf, sizeof(buf), "%s\n%s", g_btd[i].name, g_btd[i].addr);
      lv_obj_t *l = lbl(b, buf, buf, g_font_s, C_INK);
      lv_obj_set_width(l, 260);
      lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
      lv_obj_center(l);
    }
#else
  lv_obj_t *l = lbl(g_bt_list, "固件未启用蓝牙", "BT not in build", g_font_s,
                    C_MUTED);
  lv_obj_center(l);
#endif
}

static void bt_scan_cb(lv_event_t *e)
{
  (void)e;
#ifdef CONFIG_BLUETOOTH_SERVICE
  if (!g_bt)
    g_bt = bluetooth_create_instance();
  if (!g_bt)
    {
      if (g_bt_pg_st)
        lv_label_set_text(g_bt_pg_st, T("服务不可用", "Unavailable"));
      return;
    }
  if (!g_bt_prepared)
    {
      static const adapter_callbacks_t cbs = {
          .on_discovery_result = on_disc_result,
          .on_discovery_state_changed = on_disc_state,
      };
      (void)bt_adapter_register_callback(g_bt, &cbs);
      g_bt_prepared = true;
    }
  if (bt_adapter_get_state(g_bt) == BT_ADAPTER_STATE_OFF)
    {
      bt_adapter_enable(g_bt);
      g_bt_scan_req = true;
      if (g_bt_pg_st)
        lv_label_set_text(g_bt_pg_st, T("正在开启…", "Starting…"));
      return;
    }
  g_btn = 0;
  memset(g_btd, 0, sizeof(g_btd));
  rebuild_bt();
  g_bt_scan_req = true;
  if (g_bt_pg_st)
    lv_label_set_text(g_bt_pg_st, T("扫描中…", "Scanning…"));
#endif
}

void desk_create_bt(void)
{
  g_page_bt = desk_page_shell(g_root, C_BG);
  desk_backbar(g_page_bt, "蓝牙", "Bluetooth");

  lv_obj_t *sc = lv_button_create(g_page_bt);
  lv_obj_set_size(sc, 120, 30);
  lv_obj_set_pos(sc, 10, 42);
  lv_obj_set_style_bg_color(sc, lv_color_hex(C_BTN), LV_PART_MAIN);
  lv_obj_add_flag(sc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(sc, bt_scan_cb, LV_EVENT_CLICKED, NULL);
  lv_obj_t *sl = lbl(sc, "开启并扫描", "Enable & Scan", g_font_s, C_ACCENT);
  lv_obj_center(sl);

  g_bt_pg_st = lbl(g_page_bt, "仅扫描，不自动配对", "Scan only", g_font_s,
                   C_MUTED);
  lv_obj_set_pos(g_bt_pg_st, 140, 48);

  g_bt_list = lv_obj_create(g_page_bt);
  lv_obj_set_size(g_bt_list, 300, 150);
  lv_obj_set_pos(g_bt_list, 10, 80);
  lv_obj_set_style_bg_color(g_bt_list, lv_color_hex(C_BTN_HI), LV_PART_MAIN);
  lv_obj_set_style_border_width(g_bt_list, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(g_bt_list, 4, LV_PART_MAIN);
  lv_obj_set_style_pad_row(g_bt_list, 3, LV_PART_MAIN);
  lv_obj_set_scroll_dir(g_bt_list, LV_DIR_VER);
  rebuild_bt();
}

void desk_bt_tick(void)
{
#ifdef CONFIG_BLUETOOTH_SERVICE
  if (g_bt && g_bt_scan_req)
    {
      if (bt_adapter_get_state(g_bt) == BT_ADAPTER_STATE_ON)
        {
          (void)bt_adapter_set_name(g_bt, "DeskMate-037");
          (void)bt_adapter_set_scan_mode(
              g_bt, BT_SCAN_MODE_CONNECTABLE_DISCOVERABLE, true);
          (void)bt_adapter_start_discovery(g_bt, 8);
          g_bt_scan_req = false;
        }
    }
  if (g_bt && bt_adapter_is_discovering(g_bt) && g_page == PAGE_BT)
    {
      static int lastn = -1;
      if (g_btn != lastn)
        {
          lastn = g_btn;
          rebuild_bt();
        }
    }
#endif
}

#endif
