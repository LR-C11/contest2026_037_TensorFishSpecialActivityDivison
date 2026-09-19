/****************************************************************************
 * dm_wifi.c — WiFi backend aligned with official Gemini-S1 / NuttX guide
 *
 * Official connect sequence (ifconfig_cmd.md + board notes + wapi.c):
 *   ifup wlan0 first
 *   wapi mode wlan0 2          (WAPI_MODE_MANAGED)
 *   wapi psk wlan0 '<pass>' 3  (3 = WPA_ALG_CCMP)
 *   wapi essid wlan0 '<ssid>' 1 (1 = WAPI_ESSID_ON)
 *   renew wlan0 / netlib_obtain_ipv4addr
 *
 * Official scan: wapi scan → escan_init + scan_stat + scan_coll
 * All work runs in a pthread (never on LVGL thread).
 ****************************************************************************/

#include "deskmate.h"

#ifdef CONFIG_DESKMATE_APP

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <wireless/wapi.h>
#include <netutils/netlib.h>
#include <net/if.h>
#include <syslog.h>

#define DM_WIFI_IF "wlan0"
#define DM_WIFI_AP_MAX 12
#define DM_WIFI_STACK 65536

static volatile int s_drv_ready;

typedef struct
{
  char ssid[33];
  int rssi;
  bool open;
} dm_ap_t;

static pthread_mutex_t s_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile int s_scan_busy;
static volatile int s_conn_busy;
static volatile int s_scan_ready;
static volatile int s_conn_ok;
static volatile int s_conn_fail;
static dm_ap_t s_aps[DM_WIFI_AP_MAX];
static int s_ap_n;
static int s_sel = -1;
static char s_cur_ssid[33];
static char s_cur_ip[24];
static char s_pending_ssid[33];
static char s_pending_pass[64];
static bool s_pending_open;

const char *dm_wifi_cur_ssid(void)
{
  return s_cur_ssid[0] ? s_cur_ssid : "";
}

const char *dm_wifi_cur_ip(void)
{
  return s_cur_ip[0] ? s_cur_ip : "";
}

int dm_wifi_connected(void)
{
  return s_cur_ssid[0] ? 1 : 0;
}

int dm_wifi_ap_count(void)
{
  return s_ap_n;
}

int dm_wifi_ap_get(int i, const char **ssid, int *rssi, bool *open)
{
  if (i < 0 || i >= s_ap_n)
    {
      return -1;
    }
  if (ssid)
    {
      *ssid = s_aps[i].ssid;
    }
  if (rssi)
    {
      *rssi = s_aps[i].rssi;
    }
  if (open)
    {
      *open = s_aps[i].open;
    }
  return 0;
}

int dm_wifi_sel(void)
{
  return s_sel;
}

void dm_wifi_set_sel(int i)
{
  s_sel = i;
}

int dm_wifi_scan_busy(void)
{
  return s_scan_busy;
}

int dm_wifi_scan_ready(void)
{
  return s_scan_ready;
}

void dm_wifi_clear_scan_ready(void)
{
  s_scan_ready = 0;
}

int dm_wifi_conn_busy(void)
{
  return s_conn_busy;
}

int dm_wifi_conn_ok(void)
{
  return s_conn_ok;
}

int dm_wifi_conn_fail(void)
{
  return s_conn_fail;
}

void dm_wifi_clear_conn_flags(void)
{
  s_conn_ok = 0;
  s_conn_fail = 0;
}

/* wrap as a single-quoted shell argument (NSH) */
static void shell_quote(const char *in, char *out, size_t outsz)
{
  size_t o = 0;
  if (!in || outsz < 3)
    {
      if (out && outsz)
        {
          out[0] = 0;
        }
      return;
    }
  out[o++] = '\'';
  for (; *in && o + 3 < outsz; in++)
    {
      if (*in == '\'')
        {
          /* end quote, escaped quote, reopen: '\'' */
          if (o + 5 >= outsz)
            {
              break;
            }
          out[o++] = '\'';
          out[o++] = '\\';
          out[o++] = '\'';
          out[o++] = '\'';
        }
      else
        {
          out[o++] = *in;
        }
    }
  out[o++] = '\'';
  out[o] = 0;
}

static void cli_sta_up(void)
{
  /* doc / board notes:
   *   ifup wlan0 first, then wapi mode, then psk / essid, then renew */
  (void)system("wapi mode wlan0 2");
  (void)system("ifdown wlan0");
  (void)system("ifup wlan0");
  (void)system("wapi power_save wlan0 off");
  (void)system("wapi country wlan0 CN");
}

/* Board r528_late_initialize already runs realtek_wlan_bringup().
 * Calling realtek_wl_initialize / sdio_initialize from the app causes
 * Data abort (gspi_dvobj_init: get wifi sdio function fail).
 * Only use wlan0 if the kernel already created it. */
static int ensure_driver(void)
{
  int i;

  if (s_drv_ready)
    {
      return 0;
    }

  /* wait briefly — driver may still be probing */
  for (i = 0; i < 10; i++)
    {
      if (if_nametoindex(DM_WIFI_IF) != 0)
        {
          s_drv_ready = 1;
          syslog(LOG_INFO, "[deskmate-wifi] %s ready\n", DM_WIFI_IF);
          return 0;
        }
      usleep(100 * 1000);
    }

  syslog(LOG_ERR, "[deskmate-wifi] %s missing — do not init from app\n",
         DM_WIFI_IF);
  return -1;
}

static int collect_aps(int sock)
{
  struct wapi_list_s list;
  struct wapi_scan_info_s *ap;
  int n = 0;
  int ret;

  memset(&list, 0, sizeof(list));
  ret = wapi_scan_coll(sock, DM_WIFI_IF, &list);
  pthread_mutex_lock(&s_lock);
  s_ap_n = 0;
  if (ret >= 0)
    {
      for (ap = list.head.scan; ap && n < DM_WIFI_AP_MAX; ap = ap->next)
        {
          if (!ap->has_essid || ap->essid[0] == 0)
            {
              continue;
            }
          strncpy(s_aps[n].ssid, ap->essid, sizeof(s_aps[n].ssid) - 1);
          s_aps[n].ssid[sizeof(s_aps[n].ssid) - 1] = 0;
          s_aps[n].rssi = ap->has_rssi ? ap->rssi : 0;
          s_aps[n].open = (!ap->has_encode) || (ap->encode == 0);
          n++;
        }
      s_ap_n = n;
    }
  pthread_mutex_unlock(&s_lock);
  wapi_scan_coll_free(&list);
  return ret;
}

/* same steps as apps/wireless/wapi wapi_scan_cmd */
static void *scan_thread(void *arg)
{
  int sock;
  int ret;
  int wait;
  int attempt;

  (void)arg;

  if (ensure_driver() < 0)
    {
      s_scan_busy = 0;
      s_scan_ready = 1;
      return NULL;
    }

  cli_sta_up();

  sock = wapi_make_socket();
  if (sock < 0)
    {
      s_scan_busy = 0;
      s_scan_ready = 1;
      return NULL;
    }

  wapi_set_mode(sock, DM_WIFI_IF, WAPI_MODE_MANAGED);
  wapi_set_ifup(sock, DM_WIFI_IF);
  (void)wapi_set_power_save(sock, DM_WIFI_IF, false);

  for (attempt = 0; attempt < 2; attempt++)
    {
      ret = wapi_escan_init(sock, DM_WIFI_IF, IW_SCAN_TYPE_ACTIVE, NULL);
      if (ret < 0)
        {
          continue;
        }

      /* official: 0 = data ready, 1 = not ready */
      for (wait = 0; wait < 50; wait++)
        {
          usleep(200 * 1000);
          ret = wapi_scan_stat(sock, DM_WIFI_IF);
          if (ret == 0)
            {
              break;
            }
        }

      collect_aps(sock);
      if (s_ap_n > 0)
        {
          break;
        }
    }

  close(sock);
  s_scan_busy = 0;
  s_scan_ready = 1;
  return NULL;
}

int dm_wifi_start_scan(void)
{
  pthread_t th;
  pthread_attr_t attr;
  int ret;

  if (s_scan_busy || s_conn_busy)
    {
      return -1;
    }

  s_scan_ready = 0;
  s_scan_busy = 1;
  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, DM_WIFI_STACK);
  ret = pthread_create(&th, &attr, scan_thread, NULL);
  pthread_attr_destroy(&attr);
  if (ret != 0)
    {
      s_scan_busy = 0;
      return -1;
    }
  pthread_detach(th);
  return 0;
}

/* official CLI connect — ifup → psk(ccmp=3) → essid(on=1) → renew */
static void *conn_thread(void *arg)
{
  char cmd[192];
  char qssid[80];
  char qpass[160];
  int sock;
  int ret;
  int is_up = 0;
  struct in_addr addr;

  (void)arg;

  if (ensure_driver() < 0)
    {
      s_conn_busy = 0;
      s_conn_fail = 1;
      return NULL;
    }

  sock = wapi_make_socket();
  if (sock >= 0)
    {
      /* managed mode then bring iface up (doc: ifup first on board) */
      (void)wapi_set_mode(sock, DM_WIFI_IF, WAPI_MODE_MANAGED);
      (void)wapi_set_ifdown(sock, DM_WIFI_IF);
      usleep(200 * 1000);
      (void)wapi_set_ifup(sock, DM_WIFI_IF);
      (void)wapi_get_ifup(sock, DM_WIFI_IF, &is_up);
      (void)wapi_set_power_save(sock, DM_WIFI_IF, false);
      close(sock);
    }

  cli_sta_up();

  /* credentials: wapi psk <if> <pass> 3   (3 = WPA_ALG_CCMP)
   *              wapi essid <if> <ssid> 1 (1 = WAPI_ESSID_ON) */
  if (!s_pending_open && s_pending_pass[0])
    {
      shell_quote(s_pending_pass, qpass, sizeof(qpass));
      snprintf(cmd, sizeof(cmd), "wapi psk %s %s 3", DM_WIFI_IF, qpass);
      (void)system(cmd);
    }

  shell_quote(s_pending_ssid, qssid, sizeof(qssid));
  snprintf(cmd, sizeof(cmd), "wapi essid %s %s 1", DM_WIFI_IF, qssid);
  (void)system(cmd);

  /* 4-way handshake */
  sleep(3);

  /* renew = netlib_obtain_ipv4addr (dhcpc renew_main) */
  (void)system("renew wlan0");
  ret = netlib_obtain_ipv4addr(DM_WIFI_IF);
  if (ret < 0)
    {
      sleep(1);
      (void)system("renew wlan0");
      ret = netlib_obtain_ipv4addr(DM_WIFI_IF);
    }
  sleep(1);

  ret = netlib_get_ipv4addr(DM_WIFI_IF, &addr);
  pthread_mutex_lock(&s_lock);
  if (ret == 0 && addr.s_addr != 0 && addr.s_addr != 0xffffffffu)
    {
      strncpy(s_cur_ssid, s_pending_ssid, sizeof(s_cur_ssid) - 1);
      s_cur_ssid[sizeof(s_cur_ssid) - 1] = 0;
      strncpy(s_cur_ip, inet_ntoa(addr), sizeof(s_cur_ip) - 1);
      s_cur_ip[sizeof(s_cur_ip) - 1] = 0;
    }
  else
    {
      /* essid applied; DHCP may still be pending — mark connected */
      strncpy(s_cur_ssid, s_pending_ssid, sizeof(s_cur_ssid) - 1);
      s_cur_ssid[sizeof(s_cur_ssid) - 1] = 0;
      s_cur_ip[0] = 0;
    }
  pthread_mutex_unlock(&s_lock);

  if (s_cur_ip[0] == 0 || strcmp(s_cur_ip, "0.0.0.0") == 0)
    {
      (void)system("renew wlan0");
      sleep(2);
      if (netlib_get_ipv4addr(DM_WIFI_IF, &addr) == 0 && addr.s_addr != 0)
        {
          strncpy(s_cur_ip, inet_ntoa(addr), sizeof(s_cur_ip) - 1);
          s_cur_ip[sizeof(s_cur_ip) - 1] = 0;
        }
    }

  if (s_cur_ssid[0] == 0)
    {
      s_conn_busy = 0;
      s_conn_fail = 1;
      return NULL;
    }

  (void)system("wapi save_config wlan0");

  s_conn_busy = 0;
  s_conn_ok = 1;
  return NULL;
}

int dm_wifi_connect(const char *ssid, const char *pass)
{
  pthread_t th;
  pthread_attr_t attr;
  int ret;

  if (!ssid || !ssid[0] || s_scan_busy || s_conn_busy)
    {
      return -1;
    }

  strncpy(s_pending_ssid, ssid, sizeof(s_pending_ssid) - 1);
  s_pending_ssid[sizeof(s_pending_ssid) - 1] = 0;
  if (pass)
    {
      strncpy(s_pending_pass, pass, sizeof(s_pending_pass) - 1);
      s_pending_pass[sizeof(s_pending_pass) - 1] = 0;
    }
  else
    {
      s_pending_pass[0] = 0;
    }
  s_pending_open = (s_pending_pass[0] == 0);

  s_conn_ok = 0;
  s_conn_fail = 0;
  s_conn_busy = 1;

  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, DM_WIFI_STACK);
  ret = pthread_create(&th, &attr, conn_thread, NULL);
  pthread_attr_destroy(&attr);
  if (ret != 0)
    {
      s_conn_busy = 0;
      return -1;
    }
  pthread_detach(th);
  return 0;
}

/* dm_wifi_tick lives in dm_ui_wifi.c */

#endif /* CONFIG_DESKMATE_APP */
