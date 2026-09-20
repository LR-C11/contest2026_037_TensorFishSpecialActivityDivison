/****************************************************************************
 * dm_wifi.c — WiFi backend aligned with official Gemini-S1 / NuttX guide
 *
 * Official connect sequence (ifconfig_cmd.md + community notes + wapi.c):
 *   ifup wlan0 first
 *   wapi mode wlan0 2          (WAPI_MODE_MANAGED)
 *   wapi psk wlan0 '<pass>' 3  (3 = WPA_ALG_CCMP)
 *   wapi essid wlan0 '<ssid>' 1 (1 = WAPI_ESSID_ON)
 *   renew wlan0 / netlib_obtain_ipv4addr
 *
 * Auto-connect: default Xiaomi AP + optional /data saved credentials.
 * Manual scan/connect UI is unchanged.
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

/* Default home AP — boot auto-connect; manual UI still works. */
#define DM_WIFI_AUTO_SSID "Xiaomi_0521_Wi-Fi5"
#define DM_WIFI_AUTO_PSK "kisslcy520"
#define DM_WIFI_SAVE_PATH "/data/deskmate_wifi.txt"

static volatile int s_drv_ready;
static volatile int s_auto_started;

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

/* NSH-safe single-quote wrap; reject embedded quotes */
static int shell_quote(const char *in, char *out, size_t outsz)
{
  size_t n;
  size_t o = 0;

  if (!in || !out || outsz < 3)
    {
      return -1;
    }
  if (strchr(in, '\'') != NULL)
    {
      return -1;
    }
  n = strlen(in);
  if (n + 3 > outsz)
    {
      return -1;
    }
  out[o++] = '\'';
  memcpy(out + o, in, n);
  o += n;
  out[o++] = '\'';
  out[o] = 0;
  return 0;
}

static void cli_sta_up(void)
{
  /* Official §9 + review: ifup FIRST, then STA mode, ifup again.
   * Do not ifdown — avoid tearing radio before psk. */
  syslog(LOG_INFO, "[deskmate-wifi] ifup → mode STA → ifup\n");
  (void)system("ifup wlan0");
  (void)system("wapi mode wlan0 2");
  (void)system("ifup wlan0");
  (void)system("wapi power_save wlan0 off");
  (void)system("wapi country wlan0 CN");
}

static int wifi_get_valid_ip(struct in_addr *addr)
{
  if (netlib_get_ipv4addr(DM_WIFI_IF, addr) != 0)
    {
      return -1;
    }
  if (addr->s_addr == 0 || addr->s_addr == 0xffffffffu)
    {
      return -1;
    }
  return 0;
}

static void wifi_cred_save(const char *ssid, const char *pass)
{
  FILE *f;

  if (!ssid || !ssid[0])
    {
      return;
    }
  f = fopen(DM_WIFI_SAVE_PATH, "w");
  if (!f)
    {
      return;
    }
  fprintf(f, "%s\n%s\n", ssid, pass ? pass : "");
  fclose(f);
}

static int wifi_cred_load(char *ssid, size_t slen, char *pass, size_t plen)
{
  FILE *f = fopen(DM_WIFI_SAVE_PATH, "r");
  char line[80];

  if (!f)
    {
      return -1;
    }
  if (!fgets(line, sizeof(line), f))
    {
      fclose(f);
      return -1;
    }
  line[strcspn(line, "\r\n")] = 0;
  if (!line[0])
    {
      fclose(f);
      return -1;
    }
  snprintf(ssid, slen, "%s", line);
  if (fgets(line, sizeof(line), f))
    {
      line[strcspn(line, "\r\n")] = 0;
      snprintf(pass, plen, "%s", line);
    }
  else
    {
      pass[0] = 0;
    }
  fclose(f);
  return 0;
}

/* Board r528_late_initialize already runs realtek_wlan_bringup().
 * App must NOT call driver init (data abort). Wait longer for wlan0:
 * board STA bringup can finish after deskmate starts. */
static int ensure_driver_wait(int attempts_100ms)
{
  int i;

  if (s_drv_ready && if_nametoindex(DM_WIFI_IF) != 0)
    {
      return 0;
    }

  for (i = 0; i < attempts_100ms; i++)
    {
      if (if_nametoindex(DM_WIFI_IF) != 0)
        {
          s_drv_ready = 1;
          syslog(LOG_INFO, "[deskmate-wifi] %s ready\n", DM_WIFI_IF);
          return 0;
        }
      usleep(100 * 1000);
    }

  s_drv_ready = 0;
  syslog(LOG_ERR,
         "[deskmate-wifi] %s missing — board bringup not ready yet\n",
         DM_WIFI_IF);
  return -1;
}

static int ensure_driver(void)
{
  /* ~20s first wait; auto thread retries later */
  return ensure_driver_wait(200);
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

/* Official connect — ifup → mode → psk(ccmp=3) → essid(1) → wait → renew */
static void *conn_thread(void *arg)
{
  char cmd[192];
  char qssid[80];
  char qpass[160];
  int ret;
  int i;
  int got_ip = 0;
  struct in_addr addr;
  char try_ssid[33];
  char try_pass[64];

  (void)arg;

  strncpy(try_ssid, s_pending_ssid, sizeof(try_ssid) - 1);
  try_ssid[sizeof(try_ssid) - 1] = 0;
  strncpy(try_pass, s_pending_pass, sizeof(try_pass) - 1);
  try_pass[sizeof(try_pass) - 1] = 0;

  if (try_ssid[0] == 0 || if_nametoindex(DM_WIFI_IF) == 0)
    {
      syslog(LOG_ERR, "[deskmate-wifi] connect abort: no ssid/wlan0\n");
      s_conn_busy = 0;
      s_conn_fail = 1;
      return NULL;
    }

  if (ensure_driver() < 0)
    {
      s_conn_busy = 0;
      s_conn_fail = 1;
      return NULL;
    }

  /* 1) ifup + STA mode (separate system calls — no NSH &&) */
  cli_sta_up();

  /* 2) psk — WPA_ALG_CCMP = 3 (guide §1.3.2 / §9.2) */
  if (!s_pending_open && s_pending_pass[0])
    {
      if (shell_quote(s_pending_pass, qpass, sizeof(qpass)) < 0)
        {
          s_conn_busy = 0;
          s_conn_fail = 1;
          return NULL;
        }
      snprintf(cmd, sizeof(cmd), "wapi psk %s %s 3", DM_WIFI_IF, qpass);
      syslog(LOG_INFO, "[deskmate-wifi] %s\n", cmd);
      (void)system(cmd);

      /* optional explicit WPA2 version — ignore failure */
      snprintf(cmd, sizeof(cmd), "wapi psk %s %s 3 2", DM_WIFI_IF, qpass);
      (void)system(cmd);
    }

  /* 3) essid — flag 1 = connect */
  if (shell_quote(s_pending_ssid, qssid, sizeof(qssid)) < 0)
    {
      s_conn_busy = 0;
      s_conn_fail = 1;
      return NULL;
    }
  snprintf(cmd, sizeof(cmd), "wapi essid %s %s 1", DM_WIFI_IF, qssid);
  syslog(LOG_INFO, "[deskmate-wifi] %s\n", cmd);
  (void)system(cmd);

  /* 4) WPA2 4-way handshake / associate */
  sleep(3);

  /* 5) DHCP renew — retry loop (review: 2s too short once is not enough) */
  for (i = 0; i < 5; i++)
    {
      (void)system("renew wlan0");
      ret = netlib_obtain_ipv4addr(DM_WIFI_IF);
      if (ret >= 0 && wifi_get_valid_ip(&addr) == 0)
        {
          got_ip = 1;
          break;
        }
      syslog(LOG_WARNING, "[deskmate-wifi] renew retry %d\n", i + 1);
      sleep(3);
    }

  pthread_mutex_lock(&s_lock);
  strncpy(s_cur_ssid, try_ssid, sizeof(s_cur_ssid) - 1);
  s_cur_ssid[sizeof(s_cur_ssid) - 1] = 0;
  if (got_ip)
    {
      strncpy(s_cur_ip, inet_ntoa(addr), sizeof(s_cur_ip) - 1);
      s_cur_ip[sizeof(s_cur_ip) - 1] = 0;
    }
  else
    {
      s_cur_ip[0] = 0;
    }
  pthread_mutex_unlock(&s_lock);

  if (!got_ip)
    {
      syslog(LOG_ERR,
             "[deskmate-wifi] connect fail: no IP after renew "
             "(ssid=%s wlan0=%d)\n",
             try_ssid, (int)if_nametoindex(DM_WIFI_IF));
      s_conn_busy = 0;
      s_conn_fail = 1;
      return NULL;
    }

  syslog(LOG_INFO, "[deskmate-wifi] connected ssid=%s ip=%s\n",
         s_cur_ssid, s_cur_ip);

  (void)system("wapi save_config wlan0");
  wifi_cred_save(try_ssid, try_pass);

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

/* Boot auto-connect with retry until wlan0 appears.
 * Manual scan/connect UI remains fully available. */
static void *auto_thread(void *arg)
{
  char ssid[33];
  char pass[64];
  int round;

  (void)arg;

  ssid[0] = 0;
  pass[0] = 0;
  if (wifi_cred_load(ssid, sizeof(ssid), pass, sizeof(pass)) != 0 ||
      !ssid[0])
    {
      snprintf(ssid, sizeof(ssid), "%s", DM_WIFI_AUTO_SSID);
      snprintf(pass, sizeof(pass), "%s", DM_WIFI_AUTO_PSK);
    }

  syslog(LOG_INFO,
         "[deskmate-wifi] auto-connect ssid=%s — wait wlan0 first\n", ssid);

  for (round = 0; round < 8; round++)
    {
      /* Board bringup must finish first — do not call wapi before wlan0 */
      if (ensure_driver_wait(300) < 0)
        {
          syslog(LOG_WARNING,
                 "[deskmate-wifi] wlan0 not ready, round=%d "
                 "(waiting for board SDIO probe)\n", round);
          sleep(5);
          continue;
        }

      if (dm_wifi_connected())
        {
          syslog(LOG_INFO, "[deskmate-wifi] already connected\n");
          return NULL;
        }

      dm_wifi_clear_conn_flags();
      if (dm_wifi_connect(ssid, pass) != 0)
        {
          syslog(LOG_WARNING, "[deskmate-wifi] connect start busy/fail\n");
          sleep(5);
          continue;
        }

      /* conn_thread: ifup→psk→essid→3s→renew×5 */
      sleep(20);
      if (dm_wifi_connected())
        {
          syslog(LOG_INFO, "[deskmate-wifi] auto connected ssid=%s ip=%s\n",
                 dm_wifi_cur_ssid(), dm_wifi_cur_ip());
          dm_wifi_clear_conn_flags();
          return NULL;
        }

      syslog(LOG_WARNING, "[deskmate-wifi] auto connect not ready, retry\n");
      dm_wifi_clear_conn_flags();
      sleep(5);
    }

  syslog(LOG_ERR,
         "[deskmate-wifi] auto-connect gave up; use manual UI after wlan0\n");
  return NULL;
}

void dm_wifi_auto_start(void)
{
  pthread_t th;
  pthread_attr_t attr;

  if (s_auto_started)
    {
      return;
    }
  s_auto_started = 1;

  pthread_attr_init(&attr);
  pthread_attr_setstacksize(&attr, DM_WIFI_STACK);
  if (pthread_create(&th, &attr, auto_thread, NULL) == 0)
    {
      pthread_detach(th);
    }
  pthread_attr_destroy(&attr);
}

/* dm_wifi_tick lives in dm_ui_wifi.c */

#endif /* CONFIG_DESKMATE_APP */
