/*
 * dm_ble_gatt.c — FlowDesk BLE GATT server + LE advertising
 *
 * Service:  12345678-1234-5678-1234-56789abcdef0
 * CHAR_AUTH  ...def1  write (Pair ACK) + notify (Pair Request challenge)
 * CHAR_CMD   ...def2  write (Pair Request / Heartbeat)
 * CHAR_STATUS ...def3 notify (Net On/Off)
 *
 * Frame: [CMD:1][LEN:1][DATA:0..32], multi-byte little-endian.
 * Pairing: response = challenge XOR hash(PAIR_KEY), hash = h*31 + k[i].
 */

#include "deskmate.h"
#include "dm_ble_gatt.h"
#include "dm_bt_protocol.h"

#ifdef CONFIG_DESKMATE_APP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <syslog.h>

#include "bluetooth.h"
#include "bt_adapter.h"
#include "bt_gatts.h"
#include "bt_le_advertiser.h"
#include "advertiser_data.h"

/* 128-bit UUIDs in openvela / BLE wire order (reverse of big-endian string)
 * 12345678-1234-5678-1234-56789abcdef0  ->  f0 de bc 9a 78 56 34 12 ...
 * ...def1 AUTH, ...def2 CMD, ...def3 STATUS */
#define DM_UUID_SVC \
  0xf0, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12, \
  0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12
#define DM_UUID_AUTH \
  0xf1, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12, \
  0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12
#define DM_UUID_CMD \
  0xf2, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12, \
  0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12
#define DM_UUID_STATUS \
  0xf3, 0xde, 0xbc, 0x9a, 0x78, 0x56, 0x34, 0x12, \
  0x78, 0x56, 0x34, 0x12, 0x78, 0x56, 0x34, 0x12

static const bt_uuid_t k_svc_uuid = BT_UUID_DECLARE_128(DM_UUID_SVC);

enum {
  ATTR_SVC = 1,
  ATTR_AUTH,
  ATTR_AUTH_CCCD,
  ATTR_CMD,
  ATTR_CMD_CCCD,
  ATTR_STATUS,
  ATTR_STATUS_CCCD,
};

static bt_instance_t *s_ins;
static void *s_adapter_cb;
static gatts_handle_t s_srv;
static bt_advertiser_t *s_adv;
static volatile dm_ble_state_t s_state = DM_BLE_IDLE;
static dm_ble_state_cb_t s_state_cb;

static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static bt_address_t s_peer;
static volatile int s_has_peer;
static volatile int s_auth_cccd;
static volatile int s_cmd_cccd;
static volatile int s_status_cccd;
static volatile uint32_t s_challenge;
static volatile int s_paired;
static volatile int s_pairing;

static void set_state(dm_ble_state_t st)
{
  if (s_state == st)
    {
      return;
    }
  s_state = st;
  if (s_state_cb)
    {
      s_state_cb(st);
    }
}

dm_ble_state_t dm_ble_state(void)
{
  return s_state;
}

bool dm_ble_is_paired(void)
{
  return s_paired ? true : false;
}

int dm_ble_paired_addr(char *buf, int buflen)
{
  if (!buf || buflen < 18 || !s_has_peer)
    {
      return -1;
    }
  snprintf(buf, buflen, "%02x:%02x:%02x:%02x:%02x:%02x",
           s_peer.addr[5], s_peer.addr[4], s_peer.addr[3],
           s_peer.addr[2], s_peer.addr[1], s_peer.addr[0]);
  return 0;
}

void dm_ble_set_state_cb(dm_ble_state_cb_t cb)
{
  s_state_cb = cb;
}

/* ---- protocol helpers (LE) ---- */

static void put_u32_le(uint8_t *p, uint32_t v)
{
  p[0] = (uint8_t)(v & 0xff);
  p[1] = (uint8_t)((v >> 8) & 0xff);
  p[2] = (uint8_t)((v >> 16) & 0xff);
  p[3] = (uint8_t)((v >> 24) & 0xff);
}

static uint32_t get_u32_le(const uint8_t *p)
{
  return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
         ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint32_t key_hash(void)
{
  uint32_t h = 0;
  int i;
  for (i = 0; i < DM_BT_PAIR_KEY_LEN; i++)
    {
      h = h * 31u + (uint8_t)DM_BT_PAIR_KEY[i];
    }
  return h;
}

static int send_notify(uint16_t attr, const uint8_t *data, uint16_t len)
{
  bt_status_t ret;
  if (!s_srv || !s_has_peer)
    {
      return -1;
    }
  pthread_mutex_lock(&s_lock);
  ret = bt_gatts_notify(s_srv, &s_peer, attr, (uint8_t *)data, len);
  pthread_mutex_unlock(&s_lock);
  return (ret == BT_STATUS_SUCCESS) ? 0 : -1;
}

static int send_pair_req(uint32_t challenge)
{
  uint8_t buf[6];
  buf[0] = DM_BT_CMD_PAIR_REQ;
  buf[1] = 4;
  put_u32_le(buf + 2, challenge);
  return send_notify(ATTR_AUTH, buf, 6);
}

static int send_pair_fail(void)
{
  uint8_t buf[2] = { DM_BT_CMD_PAIR_FAIL, 0 };
  return send_notify(ATTR_AUTH, buf, 2);
}

static void start_pairing(void)
{
  uint32_t ch = (uint32_t)rand() ^ (uint32_t)(uintptr_t)&s_challenge;
  s_challenge = ch;
  s_pairing = 1;
  s_paired = 0;
  set_state(DM_BLE_PAIRING);
  if (send_pair_req(ch) != 0)
    {
      syslog(LOG_WARNING, "[ble-gatt] pair req notify failed\n");
    }
}

/* ---- attr callbacks ---- */

static uint16_t auth_on_read(void *srv, bt_address_t *addr, uint16_t attr,
                             uint32_t req)
{
  uint8_t buf[2] = { 0, 0 };
  (void)srv;
  (void)addr;
  (void)attr;
  bt_gatts_response(srv, addr, req, buf, 2);
  return 0;
}

static uint16_t auth_on_write(void *srv, bt_address_t *addr, uint16_t attr,
                              const uint8_t *value, uint16_t length,
                              uint16_t offset)
{
  (void)srv;
  (void)attr;
  (void)offset;

  if (length >= 2 && value[0] == DM_BT_CMD_PAIR_ACK && value[1] >= 4 &&
      length >= 6)
    {
      uint32_t resp = get_u32_le(value + 2);
      uint32_t expect = s_challenge ^ key_hash();
      syslog(LOG_INFO, "[ble-gatt] pair ack resp=0x%x expect=0x%x\n",
             (unsigned)resp, (unsigned)expect);
      if (s_pairing && resp == expect)
        {
          s_paired = 1;
          s_pairing = 0;
          pthread_mutex_lock(&s_lock);
          if (addr)
            {
              s_peer = *addr;
              s_has_peer = 1;
            }
          pthread_mutex_unlock(&s_lock);
          set_state(DM_BLE_PAIRED);
          /* confirm with Pair ACK notify so App side is consistent */
          {
            uint8_t ack[2] = { DM_BT_CMD_PAIR_ACK, 0 };
            send_notify(ATTR_AUTH, ack, 2);
          }
        }
      else
        {
          s_paired = 0;
          s_pairing = 0;
          send_pair_fail();
          set_state(DM_BLE_CONNECTED);
        }
      return length;
    }

  if (length >= 2 && value[0] == DM_BT_CMD_PAIR_REQ)
    {
      start_pairing();
      return length;
    }

  return length;
}

static uint16_t cmd_on_write(void *srv, bt_address_t *addr, uint16_t attr,
                             const uint8_t *value, uint16_t length,
                             uint16_t offset)
{
  (void)srv;
  (void)attr;
  (void)offset;

  if (addr)
    {
      pthread_mutex_lock(&s_lock);
      s_peer = *addr;
      s_has_peer = 1;
      pthread_mutex_unlock(&s_lock);
    }

  if (length >= 1)
    {
      uint8_t cmd = value[0];
      if (cmd == DM_BT_CMD_PAIR_REQ)
        {
          start_pairing();
        }
      else if (cmd == DM_BT_CMD_HEARTBEAT)
        {
          /* keep-alive, ignore */
        }
      else if (cmd == DM_BT_CMD_PAIR_ACK && length >= 6)
        {
          /* App may write ACK on AUTH or CMD */
          auth_on_write(srv, addr, ATTR_AUTH, value, length, offset);
        }
    }
  return length;
}

static uint16_t status_on_read(void *srv, bt_address_t *addr, uint16_t attr,
                               uint32_t req)
{
  uint8_t buf[2] = { DM_BT_CMD_NET_OFF, 0 };
  (void)attr;
  bt_gatts_response(srv, addr, req, buf, 2);
  return 0;
}

static uint16_t cccd_on_write(void *srv, bt_address_t *addr, uint16_t attr,
                              const uint8_t *value, uint16_t length,
                              uint16_t offset)
{
  (void)srv;
  (void)addr;
  (void)offset;
  if (length < 1)
    {
      return length;
    }
  if (attr == ATTR_AUTH_CCCD)
    {
      s_auth_cccd = value[0];
      if (s_auth_cccd && !s_paired)
        {
          start_pairing();
        }
    }
  else if (attr == ATTR_CMD_CCCD)
    {
      s_cmd_cccd = value[0];
    }
  else if (attr == ATTR_STATUS_CCCD)
    {
      s_status_cccd = value[0];
    }
  return length;
}

static gatt_attr_db_t s_attr_db[] = {
  GATT_H_PRIMARY_SERVICE(BT_UUID_DECLARE_128(DM_UUID_SVC), ATTR_SVC),
  GATT_H_CHARACTERISTIC_USER_RSP(BT_UUID_DECLARE_128(DM_UUID_AUTH),
                                 GATT_PROP_WRITE | GATT_PROP_NOTIFY,
                                 GATT_PERM_WRITE | GATT_PERM_READ,
                                 auth_on_read, auth_on_write, ATTR_AUTH),
  GATT_H_CCCD(GATT_PERM_READ | GATT_PERM_WRITE, cccd_on_write, ATTR_AUTH_CCCD),
  GATT_H_CHARACTERISTIC_USER_RSP(BT_UUID_DECLARE_128(DM_UUID_CMD),
                                 GATT_PROP_WRITE | GATT_PROP_NOTIFY,
                                 GATT_PERM_WRITE, NULL, cmd_on_write, ATTR_CMD),
  GATT_H_CCCD(GATT_PERM_READ | GATT_PERM_WRITE, cccd_on_write, ATTR_CMD_CCCD),
  GATT_H_CHARACTERISTIC_USER_RSP(BT_UUID_DECLARE_128(DM_UUID_STATUS),
                                 GATT_PROP_READ | GATT_PROP_NOTIFY,
                                 GATT_PERM_READ, status_on_read, NULL,
                                 ATTR_STATUS),
  GATT_H_CCCD(GATT_PERM_READ | GATT_PERM_WRITE, cccd_on_write, ATTR_STATUS_CCCD),
};

static gatt_srv_db_t s_srv_db = {
  .attr_db = s_attr_db,
  .attr_num = sizeof(s_attr_db) / sizeof(gatt_attr_db_t),
};

/* ---- gatts callbacks ---- */

static void on_connected(void *srv, bt_address_t *addr)
{
  (void)srv;
  if (addr)
    {
      pthread_mutex_lock(&s_lock);
      s_peer = *addr;
      s_has_peer = 1;
      pthread_mutex_unlock(&s_lock);
    }
  syslog(LOG_INFO, "[ble-gatt] connected\n");
  if (!s_paired)
    {
      set_state(DM_BLE_CONNECTED);
    }
}

static void on_disconnected(void *srv, bt_address_t *addr)
{
  (void)srv;
  (void)addr;
  syslog(LOG_INFO, "[ble-gatt] disconnected\n");
  s_has_peer = 0;
  s_auth_cccd = 0;
  s_pairing = 0;
  s_paired = 0;
  s_challenge = 0;
  if (s_adv)
    {
      set_state(DM_BLE_ADVERTISING);
    }
  else
    {
      set_state(DM_BLE_IDLE);
    }
}

static void on_attr_table_added(void *srv, gatt_status_t status,
                                uint16_t attr)
{
  (void)srv;
  syslog(LOG_INFO, "[ble-gatt] attr table added handle=%u status=%d\n",
         (unsigned)attr, (int)status);
}

static void on_attr_table_removed(void *srv, gatt_status_t status,
                                  uint16_t attr)
{
  (void)srv;
  (void)status;
  (void)attr;
}

static void on_notify_complete(void *srv, bt_address_t *addr,
                               gatt_status_t status, uint16_t attr)
{
  (void)srv;
  (void)addr;
  (void)attr;
  if (status != GATT_STATUS_SUCCESS)
    {
      syslog(LOG_WARNING, "[ble-gatt] notify fail status=%d\n", (int)status);
    }
}

static void on_mtu_changed(void *srv, bt_address_t *addr, uint32_t mtu)
{
  (void)srv;
  (void)addr;
  syslog(LOG_INFO, "[ble-gatt] mtu=%u\n", (unsigned)mtu);
}

static void on_phy_read(void *s, bt_address_t *a, ble_phy_type_t t,
                        ble_phy_type_t r)
{
  (void)s;
  (void)a;
  (void)t;
  (void)r;
}

static void on_phy_upd(void *s, bt_address_t *a, gatt_status_t st,
                       ble_phy_type_t t, ble_phy_type_t r)
{
  (void)s;
  (void)a;
  (void)st;
  (void)t;
  (void)r;
}

static void on_conn_param(void *s, bt_address_t *a, uint16_t i, uint16_t l,
                          uint16_t t)
{
  (void)s;
  (void)a;
  (void)i;
  (void)l;
  (void)t;
}

static gatts_callbacks_t s_gatts_cbs = {
  sizeof(s_gatts_cbs),
  on_connected,
  on_disconnected,
  on_attr_table_added,
  on_attr_table_removed,
  on_notify_complete,
  on_mtu_changed,
  on_phy_read,
  on_phy_upd,
  on_conn_param,
};

/* ---- advertising ---- */

static void on_adv_start(bt_advertiser_t *adv, uint8_t adv_id, uint8_t status)
{
  (void)adv;
  syslog(LOG_INFO, "[ble-gatt] adv start id=%u status=%u\n",
         (unsigned)adv_id, (unsigned)status);
  if (status == 0)
    {
      set_state(DM_BLE_ADVERTISING);
    }
}

static void on_adv_stop(bt_advertiser_t *adv, uint8_t adv_id)
{
  (void)adv;
  (void)adv_id;
}

static advertiser_callback_t s_adv_cbs = {
  .size = sizeof(advertiser_callback_t),
  .on_advertising_start = on_adv_start,
  .on_advertising_stopped = on_adv_stop,
};

static int start_adv(void)
{
  ble_adv_params_t params;
  advertiser_data_t *adv_data;
  advertiser_data_t *scan_rsp;
  uint8_t *p_adv;
  uint8_t *p_rsp;
  uint16_t adv_len = 0;
  uint16_t rsp_len = 0;

  if (!s_ins)
    {
      return -1;
    }
  if (s_adv)
    {
      return 0;
    }

  memset(&params, 0, sizeof(params));
  params.adv_type = BT_LE_ADV_IND;
  params.own_addr_type = BT_LE_ADDR_TYPE_PUBLIC;
  params.tx_power = 0;
  params.interval = 160; /* 100ms */
  params.duration = 0;
  params.channel_map = 7;
  params.filter_policy = BT_LE_ADV_FILTER_WHITE_LIST_FOR_NONE;

  adv_data = advertiser_data_new();
  if (!adv_data)
    {
      return -1;
    }
  advertiser_data_set_flags(adv_data,
                            BT_AD_FLAG_GENERAL_DISCOVERABLE |
                            BT_AD_FLAG_BREDR_NOT_SUPPORT);
  advertiser_data_add_service_uuid(adv_data, &k_svc_uuid);
  p_adv = advertiser_data_build(adv_data, &adv_len);

  scan_rsp = advertiser_data_new();
  if (scan_rsp)
    {
      advertiser_data_set_name(scan_rsp, "FlowDesk-S1");
      p_rsp = advertiser_data_build(scan_rsp, &rsp_len);
    }
  else
    {
      p_rsp = NULL;
    }

  s_adv = bt_le_start_advertising(s_ins, &params,
                                  p_adv, adv_len,
                                  p_rsp, rsp_len,
                                  &s_adv_cbs);

  advertiser_data_free(adv_data);
  if (scan_rsp)
    {
      advertiser_data_free(scan_rsp);
    }

  if (!s_adv)
    {
      syslog(LOG_ERR, "[ble-gatt] start advertising failed\n");
      set_state(DM_BLE_ERROR);
      return -1;
    }
  set_state(DM_BLE_ADVERTISING);
  return 0;
}

static void stop_adv(void)
{
  if (s_ins && s_adv)
    {
      bt_le_stop_advertising(s_ins, s_adv);
      s_adv = NULL;
    }
}

/* ---- adapter ---- */

static void adapter_state_cb(void *cookie, bt_adapter_state_t state)
{
  (void)cookie;
  syslog(LOG_INFO, "[ble-gatt] adapter state=%d\n", (int)state);
  if (state == BT_ADAPTER_STATE_ON)
    {
      /* register service after adapter is up */
      if (!s_srv)
        {
          bt_status_t ret = bt_gatts_register_service(s_ins, &s_srv,
                                                     &s_gatts_cbs);
          if (ret == BT_STATUS_SUCCESS && s_srv)
            {
              bt_gatts_add_attr_table(s_srv, &s_srv_db);
              start_adv();
            }
          else
            {
              syslog(LOG_ERR, "[ble-gatt] register service fail %d\n",
                     (int)ret);
              set_state(DM_BLE_ERROR);
            }
        }
      else if (!s_adv)
        {
          start_adv();
        }
    }
}

static adapter_callbacks_t s_adapter_cbs = {
  .on_adapter_state_changed = adapter_state_cb,
};

int dm_ble_start(void)
{
  if (s_state != DM_BLE_IDLE && s_state != DM_BLE_ERROR)
    {
      return 0;
    }

  set_state(DM_BLE_STARTING);
  s_paired = 0;
  s_pairing = 0;
  s_has_peer = 0;

  if (!s_ins)
    {
      s_ins = bluetooth_create_instance();
      if (!s_ins)
        {
          syslog(LOG_ERR, "[ble-gatt] create instance fail\n");
          set_state(DM_BLE_ERROR);
          return -1;
        }
    }

  if (!s_adapter_cb)
    {
      s_adapter_cb = bt_adapter_register_callback(s_ins, &s_adapter_cbs);
    }

  if (bt_adapter_enable(s_ins) != BT_STATUS_SUCCESS)
    {
      /* may already be on — try register/adv directly */
      adapter_state_cb(NULL, BT_ADAPTER_STATE_ON);
    }
  return 0;
}

void dm_ble_stop(void)
{
  stop_adv();
  if (s_srv)
    {
      bt_gatts_unregister_service(s_srv);
      s_srv = NULL;
    }
  s_paired = 0;
  s_pairing = 0;
  s_has_peer = 0;
  s_auth_cccd = 0;
  set_state(DM_BLE_IDLE);
}

#endif /* CONFIG_DESKMATE_APP */
