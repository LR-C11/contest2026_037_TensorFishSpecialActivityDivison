/****************************************************************************
 * dm_ui_bt.c — Bluetooth (openvela Framework API)
 *
 * Per official docs / sample_code:
 *   bluetooth_create_instance →
 *   bt_adapter_register_callback →
 *   bt_adapter_enable / disable
 *   bt_adapter_start_discovery → on_discovery_result
 *   bt_device_create_bond + bt_device_connect
 *
 * Rules: do NOT call BT APIs from callbacks — only set flags / store results.
 ****************************************************************************/

#include "deskmate.h"

#ifdef CONFIG_DESKMATE_APP

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

#include "bluetooth.h"
#include "bt_adapter.h"
#include "bt_device.h"
#include "bt_addr.h"

#define BT_PATH "/data/deskmate_bt.txt"
#define BT_DEV_MAX 6

typedef struct
{
  char name[32];
  char mac[18];
  bt_address_t addr;
  int rssi;
  int bonded;
  int connected;
} bt_dev_t;

static bt_instance_t *s_bt_ins;
static void *s_bt_cb;
static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;

static lv_obj_t *s_bt_list;
static lv_obj_t *s_bt_sw_btn;
static lv_obj_t *s_bt_tip;
static lv_obj_t *s_bt_name;
static lv_obj_t *s_bt_mac;
static lv_obj_t *s_bt_rows[BT_DEV_MAX];
static lv_obj_t *s_bt_names[BT_DEV_MAX];
static lv_obj_t *s_bt_macs[BT_DEV_MAX];

static bt_dev_t s_bts[BT_DEV_MAX];
static int s_bt_n;
static int s_bt_sel;
static volatile int s_bt_on;
static volatile int s_bt_scanning;
static volatile int s_bt_need_paint;
static char s_local_name[32] = "Gemini-S1";
static char s_local_mac[18] = "—";

/* ---------- helpers ---------- */

static void bt_back(lv_event_t *e)
{
  (void)e;
  dm_show(PAGE_SETTINGS);
}

static void bt_addr_to_str(const bt_address_t *a, char *out, size_t n)
{
  snprintf(out, n, "%02x:%02x:%02x:%02x:%02x:%02x", a->addr[0], a->addr[1],
           a->addr[2], a->addr[3], a->addr[4], a->addr[5]);
}

static void bt_paint(void)
{
  int i;
  char line[40];

  if (!g_dm_pages[PAGE_BT] || g_dm.page != PAGE_BT)
    {
      return;
    }

  if (s_bt_sw_btn)
    {
      lv_obj_t *lab = lv_obj_get_child(s_bt_sw_btn, 0);
      if (lab)
        {
          lv_label_set_text(lab, s_bt_on ? "蓝牙：开" : "蓝牙：关");
        }
      lv_obj_set_style_bg_color(s_bt_sw_btn,
                                lv_color_hex(s_bt_on ? C_OK : C_BTN),
                                LV_PART_MAIN);
    }

  if (s_bt_name)
    {
      if (s_bt_on)
        {
          lv_label_set_text(s_bt_name, s_local_name);
          lv_obj_set_style_text_color(s_bt_name, lv_color_hex(C_INK),
                                      LV_PART_MAIN);
        }
      else
        {
          lv_label_set_text(s_bt_name, "—");
          lv_obj_set_style_text_color(s_bt_name, lv_color_hex(C_MUTED),
                                      LV_PART_MAIN);
        }
    }
  if (s_bt_mac)
    {
      if (s_bt_on)
        {
          lv_label_set_text(s_bt_mac, s_local_mac);
        }
      else
        {
          lv_label_set_text(s_bt_mac, "地址 —");
        }
    }

  if (s_bt_tip)
    {
      if (s_bt_scanning)
        {
          lv_label_set_text(s_bt_tip, "扫描中…");
        }
      else if (!s_bt_on)
        {
          lv_label_set_text(s_bt_tip, "请先打开蓝牙");
        }
      else if (s_bt_n == 0)
        {
          lv_label_set_text(s_bt_tip, "点扫描发现设备");
        }
      else
        {
          lv_label_set_text(s_bt_tip, "点列表选中，再点连接");
        }
    }

  pthread_mutex_lock(&s_lock);
  for (i = 0; i < BT_DEV_MAX; i++)
    {
      if (!s_bt_rows[i])
        {
          continue;
        }
      if (i < s_bt_n)
        {
          lv_obj_clear_flag(s_bt_rows[i], LV_OBJ_FLAG_HIDDEN);
          lv_obj_set_style_bg_color(
              s_bt_rows[i],
              lv_color_hex(i == s_bt_sel ? 0x0d3a4a : C_BTN), LV_PART_MAIN);
          if (s_bt_names[i])
            {
              snprintf(line, sizeof(line), "%s  %d", s_bts[i].name,
                       s_bts[i].rssi);
              lv_label_set_text(s_bt_names[i], line);
            }
          if (s_bt_macs[i])
            {
              snprintf(line, sizeof(line), "%s%s", s_bts[i].mac,
                       s_bts[i].connected ? " ·已连接"
                                          : (s_bts[i].bonded ? " ·已配对"
                                                             : ""));
              lv_label_set_text(s_bt_macs[i], line);
            }
        }
      else
        {
          lv_obj_add_flag(s_bt_rows[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
  pthread_mutex_unlock(&s_lock);
}

/* ---------- callbacks: flags only, no BT API ---------- */

static void on_adapter_state(void *cookie, bt_adapter_state_t state)
{
  (void)cookie;
  if (state == BT_ADAPTER_STATE_ON)
    {
      s_bt_on = 1;
    }
  else if (state == BT_ADAPTER_STATE_OFF)
    {
      s_bt_on = 0;
      pthread_mutex_lock(&s_lock);
      s_bt_n = 0;
      pthread_mutex_unlock(&s_lock);
    }
  s_bt_need_paint = 1;
}

static void on_discovery_state(void *cookie, bt_discovery_state_t state)
{
  (void)cookie;
  s_bt_scanning = (state == BT_DISCOVERY_STATE_STARTED) ? 1 : 0;
  s_bt_need_paint = 1;
}

static void on_discovery_result(void *cookie, bt_discovery_result_t *result)
{
  int i;
  (void)cookie;
  if (!result)
    {
      return;
    }
  pthread_mutex_lock(&s_lock);
  /* update existing or append */
  for (i = 0; i < s_bt_n; i++)
    {
      if (memcmp(s_bts[i].addr.addr, result->addr.addr,
                 sizeof(bt_address_t)) == 0)
        {
          break;
        }
    }
  if (i >= BT_DEV_MAX)
    {
      pthread_mutex_unlock(&s_lock);
      return;
    }
  if (i >= s_bt_n)
    {
      s_bt_n = i + 1;
      memset(&s_bts[i], 0, sizeof(s_bts[i]));
    }
  memcpy(&s_bts[i].addr, &result->addr, sizeof(bt_address_t));
  bt_addr_to_str(&result->addr, s_bts[i].mac, sizeof(s_bts[i].mac));
  snprintf(s_bts[i].name, sizeof(s_bts[i].name), "%s",
           result->name[0] ? result->name : "未知设备");
  s_bts[i].rssi = result->rssi;
  pthread_mutex_unlock(&s_lock);
  s_bt_need_paint = 1;
}

static void on_bond_state(void *cookie, bt_address_t *addr,
                          bt_transport_t transport, bond_state_t state,
                          bool is_ctkd)
{
  int i;
  (void)cookie;
  (void)transport;
  (void)is_ctkd;
  if (!addr)
    {
      return;
    }
  pthread_mutex_lock(&s_lock);
  for (i = 0; i < s_bt_n; i++)
    {
      if (memcmp(&s_bts[i].addr, addr, sizeof(bt_address_t)) == 0)
        {
          s_bts[i].bonded = (state == BOND_STATE_BONDED) ? 1 : 0;
          break;
        }
    }
  pthread_mutex_unlock(&s_lock);
  s_bt_need_paint = 1;
}

static void on_conn_state(void *cookie, bt_address_t *addr,
                          bt_transport_t transport, connection_state_t state)
{
  int i;
  (void)cookie;
  (void)transport;
  if (!addr)
    {
      return;
    }
  pthread_mutex_lock(&s_lock);
  for (i = 0; i < s_bt_n; i++)
    {
      if (memcmp(&s_bts[i].addr, addr, sizeof(bt_address_t)) == 0)
        {
          s_bts[i].connected =
              (state == CONNECTION_STATE_CONNECTED) ? 1 : 0;
          break;
        }
    }
  pthread_mutex_unlock(&s_lock);
  s_bt_need_paint = 1;
}

static const adapter_callbacks_t s_adapter_cbs = {
  .on_adapter_state_changed = on_adapter_state,
  .on_discovery_state_changed = on_discovery_state,
  .on_discovery_result = on_discovery_result,
  .on_bond_state_changed = on_bond_state,
  .on_connection_state_changed = on_conn_state,
};

/* ---------- init / user actions ---------- */

static int bt_insure(void)
{
  if (s_bt_ins)
    {
      return 0;
    }
  s_bt_ins = bluetooth_create_instance();
  if (!s_bt_ins)
    {
      return -1;
    }
  s_bt_cb = bt_adapter_register_callback(s_bt_ins, &s_adapter_cbs);
  if (!s_bt_cb)
    {
      return -1;
    }
  return 0;
}

static void bt_read_local(void)
{
  bt_address_t a;
  if (!s_bt_ins)
    {
      return;
    }
  bt_adapter_get_name(s_bt_ins, s_local_name, sizeof(s_local_name) - 1);
  bt_adapter_get_address(s_bt_ins, &a);
  bt_addr_to_str(&a, s_local_mac, sizeof(s_local_mac));
}

static volatile int s_bt_enabling;

static void *bt_enable_worker(void *arg)
{
  int i;
  (void)arg;
  for (i = 0; i < 10; i++)
    {
      if (bt_adapter_enable(s_bt_ins) == BT_STATUS_SUCCESS)
        {
          s_bt_on = 1;
          bt_read_local();
          s_bt_need_paint = 1;
          s_bt_enabling = 0;
          return NULL;
        }
      sleep(1);
    }
  s_bt_enabling = 0;
  s_bt_need_paint = 1;
  return NULL;
}

static void bt_sw_cb(lv_event_t *e)
{
  pthread_t th;
  pthread_attr_t attr;
  (void)e;
  if (bt_insure() < 0)
    {
      if (s_bt_tip)
        {
          lv_label_set_text(s_bt_tip, "实例创建失败");
        }
      return;
    }
  if (!s_bt_on)
    {
      if (s_bt_enabling)
        {
          return;
        }
      s_bt_enabling = 1;
      if (s_bt_tip)
        {
          lv_label_set_text(s_bt_tip, "正在开启…");
        }
      pthread_attr_init(&attr);
      pthread_attr_setstacksize(&attr, 65536);
      if (pthread_create(&th, &attr, bt_enable_worker, NULL) == 0)
        {
          pthread_detach(th);
        }
      else
        {
          s_bt_enabling = 0;
        }
      pthread_attr_destroy(&attr);
    }
  else
    {
      (void)bt_adapter_disable(s_bt_ins);
      s_bt_on = 0;
      pthread_mutex_lock(&s_lock);
      s_bt_n = 0;
      pthread_mutex_unlock(&s_lock);
    }
  bt_paint();
}

static void bt_scan_cb(lv_event_t *e)
{
  (void)e;
  if (!s_bt_on || bt_insure() < 0)
    {
      bt_paint();
      return;
    }
  pthread_mutex_lock(&s_lock);
  s_bt_n = 0;
  s_bt_sel = 0;
  pthread_mutex_unlock(&s_lock);
  s_bt_scanning = 1;
  bt_paint();
  /* timeout 10 * 1.28s ≈ 12s window */
  (void)bt_adapter_start_discovery(s_bt_ins, 10);
}

static void bt_conn_cb(lv_event_t *e)
{
  bt_address_t addr;
  (void)e;
  if (!s_bt_on || bt_insure() < 0)
    {
      return;
    }
  pthread_mutex_lock(&s_lock);
  if (s_bt_sel < 0 || s_bt_sel >= s_bt_n)
    {
      pthread_mutex_unlock(&s_lock);
      return;
    }
  memcpy(&addr, &s_bts[s_bt_sel].addr, sizeof(addr));
  pthread_mutex_unlock(&s_lock);

  /* docs: create_bond then connect */
  if (bt_device_create_bond(s_bt_ins, &addr, BT_TRANSPORT_BREDR) !=
      BT_STATUS_SUCCESS)
    {
      /* may already be bonded — still try connect */
    }
  (void)bt_device_connect(s_bt_ins, &addr);
  if (s_bt_tip)
    {
      lv_label_set_text(s_bt_tip, "已发起配对/连接");
    }
}

static void bt_del_cb(lv_event_t *e)
{
  int i;
  (void)e;
  if (s_bt_sel < 0 || s_bt_sel >= s_bt_n)
    {
      return;
    }
  pthread_mutex_lock(&s_lock);
  for (i = s_bt_sel; i < s_bt_n - 1; i++)
    {
      s_bts[i] = s_bts[i + 1];
    }
  s_bt_n--;
  if (s_bt_sel >= s_bt_n)
    {
      s_bt_sel = s_bt_n > 0 ? s_bt_n - 1 : 0;
    }
  pthread_mutex_unlock(&s_lock);
  bt_paint();
}

static void bt_row_cb(lv_event_t *e)
{
  s_bt_sel = (int)(intptr_t)lv_event_get_user_data(e);
  bt_paint();
}

void dm_create_bt(void)
{
  lv_obj_t *page;
  lv_obj_t *lab;
  lv_obj_t *b;
  lv_obj_t *row;
  int i;

  page = lv_obj_create(g_dm_root);
  lv_obj_set_size(page, DM_SCR_W, DM_SCR_H);
  lv_obj_set_pos(page, 0, 0);
  lv_obj_set_style_bg_color(page, lv_color_hex(C_BG), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(page, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_border_width(page, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(page, 0, LV_PART_MAIN);
  lv_obj_clear_flag(page, LV_OBJ_FLAG_SCROLLABLE);
  g_dm_pages[PAGE_BT] = page;

  b = dm_btn(page, "←", "<", 36, 24, C_BTN, C_MUTED, bt_back, NULL);
  lv_obj_set_pos(b, 8, 6);
  lab = dm_lbl(page, "蓝牙", "Bluetooth", g_dm_font_m, C_INK);
  lv_obj_set_pos(lab, 50, 8);

  s_bt_sw_btn = dm_btn(page, "蓝牙：关", "BT Off", 100, 32, C_BTN, C_INK,
                       bt_sw_cb, NULL);
  lv_obj_set_pos(s_bt_sw_btn, 8, 36);

  b = dm_btn(page, "扫描", "Scan", 96, 32, 0x0d3a4a, C_ACCENT, bt_scan_cb,
             NULL);
  lv_obj_set_pos(b, 112, 36);

  s_bt_name = dm_lbl(page, "—", "—", g_dm_font_m, C_INK);
  lv_obj_set_pos(s_bt_name, 8, 74);
  s_bt_mac = dm_lbl(page, "地址 —", "Addr —", g_dm_font_s, C_MUTED);
  lv_obj_set_pos(s_bt_mac, 8, 96);
  s_bt_tip = dm_lbl(page, "请先打开蓝牙", "Turn BT on", g_dm_font_s, C_STAR);
  lv_obj_set_pos(s_bt_tip, 8, 116);

  s_bt_list = lv_obj_create(page);
  lv_obj_set_size(s_bt_list, 304, 76);
  lv_obj_set_pos(s_bt_list, 8, 136);
  lv_obj_set_style_bg_opa(s_bt_list, LV_OPA_TRANSP, LV_PART_MAIN);
  lv_obj_set_style_border_width(s_bt_list, 0, LV_PART_MAIN);
  lv_obj_set_style_pad_all(s_bt_list, 0, LV_PART_MAIN);
  lv_obj_set_scroll_dir(s_bt_list, LV_DIR_VER);
  lv_obj_set_scrollbar_mode(s_bt_list, LV_SCROLLBAR_MODE_AUTO);

  for (i = 0; i < BT_DEV_MAX; i++)
    {
      row = lv_obj_create(s_bt_list);
      lv_obj_set_size(row, 296, 36);
      lv_obj_set_pos(row, 0, i * 42);
      lv_obj_set_style_bg_color(row, lv_color_hex(C_BTN), LV_PART_MAIN);
      lv_obj_set_style_bg_opa(row, LV_OPA_COVER, LV_PART_MAIN);
      lv_obj_set_style_radius(row, 8, LV_PART_MAIN);
      lv_obj_set_style_border_width(row, 0, LV_PART_MAIN);
      lv_obj_set_style_pad_all(row, 2, LV_PART_MAIN);
      lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
      lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
      lv_obj_add_event_cb(row, bt_row_cb, LV_EVENT_CLICKED,
                          (void *)(intptr_t)i);
      s_bt_names[i] = dm_lbl(row, "", "", g_dm_font_s, C_INK);
      lv_obj_set_pos(s_bt_names[i], 6, 2);
      s_bt_macs[i] = dm_lbl(row, "", "", g_dm_font_s, C_MUTED);
      lv_obj_set_pos(s_bt_macs[i], 6, 18);
      s_bt_rows[i] = row;
      lv_obj_add_flag(row, LV_OBJ_FLAG_HIDDEN);
    }

  b = dm_btn(page, "连接", "Connect", 144, 32, C_OK, C_EYE, bt_conn_cb,
             NULL);
  lv_obj_set_pos(b, 8, 204);
  b = dm_btn(page, "删除", "Unpair", 144, 32, C_BTN, C_INK, bt_del_cb,
             NULL);
  lv_obj_set_pos(b, 168, 204);

  /* create instance early so enable is instant */
  (void)bt_insure();
}

void dm_bt_tick(void)
{
  if (g_dm.page != PAGE_BT)
    {
      return;
    }
  if (s_bt_need_paint)
    {
      s_bt_need_paint = 0;
      bt_paint();
    }
}

#endif /* CONFIG_DESKMATE_APP */
