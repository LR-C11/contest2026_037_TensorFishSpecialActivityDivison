/****************************************************************************
 * dm_wifi.c — WiFi backend aligned with official Gemini-S1 / NuttX guide
 *
 * Official connect sequence:
 *   wapi mode wlan0 2
 *   wapi psk wlan0 "<pass>" 3      (CCMP / WPA2)
 *   wapi essid wlan0 "<ssid>" 1
 *   renew wlan0
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
#include <nuttx/sdio.h>
#include <arch/chip/realtek_wlan.h>
#include <syslog.h>

#define DM_WIFI_IF "wlan0"
#define DM_WIFI_AP_MAX 12
#define DM_WIFI_STACK 65536

/* board/driver bringup (same as wifi_test / realtek_wlan_bringup) */
FAR struct sdio_dev_s *sdio_initialize(int sdcno);
void set_sdio_param(int sdcno, int cd_mode,
                    void (*card_detect_cb)(uint32_t present, uint16_t sdc_id));

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

/* escape for single-quoted shell argument */
static void shell_quote(const char *in, char *out, size_t outsz)
{
  size_t o = 0;
  if (!in)
    {
      out[0] = 0;
      return;
    }
  for (; *in && o + 2 < outsz; in++)
    {
      if (*in == '\'')
        {
          if (o + 4 >= outsz)
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
  out[o] = 0;
}

static void cli_sta_up(void)
{
  /* official: wapi mode wlan0 2 (WAPI_MODE_MANAGED) */
  (void)system("wapi mode wlan0 2");
  (void)system("wapi power_save wlan0 off");
  (void)system("wapi country wlan0 CN");
}

/* Align with chips/r528/r528_wlan.c realtek_wlan_bringup:
 *   set_sdio_param(1,3,NULL); sdio_initialize(1);
 *   realtek_wl_sdio_init(); realtek_wl_initialize(mode)
 * Official boot uses mode=NONE(0) and never wifi_on.
 * We use STA(1) so wifi_on runs. */
#define DM_RTW_MODE_STA 1

static int ensure_driver(void)
{
  FAR struct sdio_dev_s *sdio;
  int ret;

  if (s_drv_ready)
    {
      return 0;
    }

  syslog(LOG_INFO, "[deskmate-wifi] bringup sdc1...\n");
  set_sdio_param(1, 3, NULL);
  sdio = sdio_initialize(1);
  if (sdio == NULL)
    {
      syslog(LOG_ERR, "[deskmate-wifi] sdio_initialize failed\n");
      return -1;
    }
  ret = realtek_wl_sdio_init(sdio);
  if (ret != 0)
    {
      syslog(LOG_ERR, "[deskmate-wifi] sdio_init failed %d\n", ret);
      return ret;
    }
  syslog(LOG_INFO, "[deskmate-wifi] realtek_wl_initialize STA\n");
  ret = realtek_wl_initialize(DM_RTW_MODE_STA);
  if (ret < 0)
    {
      syslog(LOG_ERR, "[deskmate-wifi] initialize failed %d\n", ret);
      return ret;
    }
  s_drv_ready = 1;
  syslog(LOG_INFO, "[deskmate-wifi] bringup ok\n");
  return 0;
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

/* official CLI connect — proven on this board */
static void *conn_thread(void *arg)
{
  char cmd[192];
  char qssid[80];
  char qpass[160];
  int ret;
  struct in_addr addr;

  (void)arg;

  if (ensure_driver() < 0)
    {
      s_conn_busy = 0;
      s_conn_fail = 1;
      return NULL;
    }

  cli_sta_up();

  if (!s_pending_open && s_pending_pass[0])
    {
      shell_quote(s_pending_pass, qpass, sizeof(qpass));
      snprintf(cmd, sizeof(cmd), "wapi psk wlan0 %s 3", qpass);
      (void)system(cmd);
    }

  shell_quote(s_pending_ssid, qssid, sizeof(qssid));
  snprintf(cmd, sizeof(cmd), "wapi essid wlan0 %s 1", qssid);
  (void)system(cmd);

  /* wait 4-way handshake then official renew */
  sleep(4);
  (void)system("renew wlan0");
  sleep(2);

  ret = netlib_get_ipv4addr(DM_WIFI_IF, &addr);
  pthread_mutex_lock(&s_lock);
  if (ret == 0 && addr.s_addr != 0)
    {
      strncpy(s_cur_ssid, s_pending_ssid, sizeof(s_cur_ssid) - 1);
      s_cur_ssid[sizeof(s_cur_ssid) - 1] = 0;
      strncpy(s_cur_ip, inet_ntoa(addr), sizeof(s_cur_ip) - 1);
      s_cur_ip[sizeof(s_cur_ip) - 1] = 0;
    }
  else
    {
      /* link may be up without DHCP yet — still treat as connected if essid set */
      strncpy(s_cur_ssid, s_pending_ssid, sizeof(s_cur_ssid) - 1);
      s_cur_ssid[sizeof(s_cur_ssid) - 1] = 0;
      s_cur_ip[0] = 0;
    }
  pthread_mutex_unlock(&s_lock);

  if (s_cur_ip[0] == 0 || strcmp(s_cur_ip, "0.0.0.0") == 0)
    {
      /* one more DHCP try */
      (void)system("renew wlan0");
      sleep(2);
      if (netlib_get_ipv4addr(DM_WIFI_IF, &addr) == 0)
        {
          strncpy(s_cur_ip, inet_ntoa(addr), sizeof(s_cur_ip) - 1);
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
