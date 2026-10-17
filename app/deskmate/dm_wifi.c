/****************************************************************************
 * dm_wifi.c — WiFi backend aligned with official Gemini-S1 / NuttX guide
 *
 * Official connect sequence (Gemini-S1 docs + 9.5 wifi_manager.c):
 *   ifup wlan0
 *   wapi mode wlan0 2          (WAPI_MODE_MANAGED / STA)
 *   wapi psk wlan0 <pass> 3 2  (3 = WPA_ALG_CCMP, 2 = WPA2)
 *   wapi essid wlan0 <ssid> 1  (1 = WAPI_ESSID_ON)
 *   renew wlan0
 *
 * Fixes applied:
 *   1. Do not write s_cur_ssid when DHCP fails (!got_ip); dm_wifi_connected()
 *      checks both s_cur_ssid and s_cur_ip so failed auto-connect never fakes
 *      "Connected" state.
 *   2. Manual scan/connect sets s_manual_override = 1, immediately interrupting
 *      background boot auto-connect so clicking "Scan" is never blocked by
 *      s_conn_busy.
 *   3. Robust wapi scan parser: skips non-MAC header/log lines, preserves SSIDs
 *      containing spaces, deduplicates BSSIDs, and sorts by RSSI descending.
 *   4. Avoids double DHCP calls in the same loop iteration.
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

#define DM_WIFI_IF        "wlan0"
#define DM_WIFI_AP_MAX    24
#define DM_WIFI_STACK     65536

/* Optional fallback AP when no saved credentials exist */
#define DM_WIFI_AUTO_SSID "Xiaomi_0521_Wi-Fi5"
#define DM_WIFI_AUTO_PSK  "kisslcy520"
#define DM_WIFI_SAVE_PATH "/data/deskmate_wifi.txt"

static volatile int s_drv_ready;
static volatile int s_auto_started;
static volatile int s_manual_override;
static volatile int s_conn_is_auto;

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
  return (s_cur_ssid[0] != 0 && s_cur_ip[0] != 0) ? 1 : 0;
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

/* NSH-safe single-quote wrap; reject embedded single quotes */
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
  syslog(LOG_INFO, "[deskmate-wifi] ifup + mode STA\n");
  (void)system("ifup wlan0");
  (void)system("wapi mode wlan0 2");
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

/* Check wapi show wlan0 like 9.5 wifi_manager.c get_wifi_status() */
static int wifi_is_associated(char *assoc_ssid, size_t ssid_len)
{
  FILE *fp;
  char buf[256];
  int associated = 1;

  if (assoc_ssid && ssid_len > 0)
    {
      assoc_ssid[0] = 0;
    }

  (void)system("wapi show wlan0 > /tmp/wifi_status.txt");
  fp = fopen("/tmp/wifi_status.txt", "r");
  if (!fp)
    {
      return 0;
    }

  while (fgets(buf, sizeof(buf), fp) != NULL)
    {
      buf[strcspn(buf, "\r\n")] = 0;
      if (strstr(buf, "AP: 00:00:00:00:00:00") != NULL)
        {
          associated = 0;
        }
      if (assoc_ssid && ssid_len > 0)
        {
          char *p = strstr(buf, "ESSID:");
          if (p)
            {
              p += 6;
              while (*p == ' ' || *p == '"')
                {
                  p++;
                }
              snprintf(assoc_ssid, ssid_len, "%s", p);
              assoc_ssid[strcspn(assoc_ssid, "\"\r\n")] = 0;
            }
        }
    }

  fclose(fp);
  return associated;
}

static void wifi_cred_save(const char *ssid, const char *pass)
{
  FILE *f;

  if (!ssid || !ssid[0])
    {
      return;
    }
  /* Reference save_wifi_config(): /data/wifi.cfg */
  f = fopen("/data/wifi.cfg", "w");
  if (f)
    {
      fprintf(f, "SSID=\"%s\"\n", ssid);
      fprintf(f, "PASSWORD=\"%s\"\n", pass ? pass : "");
      fclose(f);
    }
  /* Keep deskmate's own file as a mirror */
  f = fopen(DM_WIFI_SAVE_PATH, "w");
  if (f)
    {
      fprintf(f, "%s\n%s\n", ssid, pass ? pass : "");
      fclose(f);
    }
}

static void wifi_unquote(char *s)
{
  size_t n;

  if (!s || s[0] != '"')
    {
      return;
    }
  n = strlen(s);
  if (n >= 2 && s[n - 1] == '"')
    {
      memmove(s, s + 1, n - 2);
      s[n - 2] = 0;
    }
}

static int wifi_cred_load_cfg(char *ssid, size_t slen, char *pass,
                              size_t plen)
{
  FILE *f = fopen("/data/wifi.cfg", "r");
  char line[96];
  int got_ssid = 0;
  int got_pass = 0;

  if (!f)
    {
      return -1;
    }
  while (fgets(line, sizeof(line), f))
    {
      line[strcspn(line, "\r\n")] = 0;
      if (strncmp(line, "SSID=", 5) == 0)
        {
          snprintf(ssid, slen, "%s", line + 5);
          wifi_unquote(ssid);
          got_ssid = 1;
        }
      else if (strncmp(line, "PASSWORD=", 9) == 0)
        {
          snprintf(pass, plen, "%s", line + 9);
          wifi_unquote(pass);
          got_pass = 1;
        }
    }
  fclose(f);

  if (!got_ssid || !ssid[0])
    {
      return -1;
    }
  if (!got_pass)
    {
      pass[0] = 0;
    }
  syslog(LOG_INFO, "[deskmate-wifi] creds from /data/wifi.cfg ssid=%s\n",
         ssid);
  return 0;
}

static int wifi_cred_load(char *ssid, size_t slen, char *pass, size_t plen)
{
  FILE *f = fopen(DM_WIFI_SAVE_PATH, "r");
  char line[96];

  if (!f)
    {
      return wifi_cred_load_cfg(ssid, slen, pass, plen);
    }
  if (!fgets(line, sizeof(line), f))
    {
      fclose(f);
      return wifi_cred_load_cfg(ssid, slen, pass, plen);
    }
  line[strcspn(line, "\r\n")] = 0;
  if (!line[0])
    {
      fclose(f);
      return wifi_cred_load_cfg(ssid, slen, pass, plen);
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
  return ensure_driver_wait(200);
}

/* Parse /tmp/wifi_scan_result.txt:
 *   bssid / frequency / signal level / encode / ssid
 *   74:39:89:f8:f0:ae       5240    -59     0803    Programmers7
 *   d2:ad:08:f8:eb:71       2462    -88     0803    DIRECT-71-HP Smart Tank 750
 *
 * Preserves spaces inside SSIDs, skips hidden (empty) SSIDs and non-MAC lines,
 * deduplicates same-SSID entries (keeping strongest RSSI), and sorts by RSSI.
 */
static int collect_aps_from_txt(void)
{
  FILE *fp;
  char line[256];
  int n = 0;
  int i;
  int j;

  fp = fopen("/tmp/wifi_scan_result.txt", "r");
  if (!fp)
    {
      return -1;
    }

  pthread_mutex_lock(&s_lock);
  s_ap_n = 0;

  while (fgets(line, sizeof(line), fp) && n < DM_WIFI_AP_MAX)
    {
      char *p = line;
      char *tok_bssid;
      char *tok_freq;
      char *tok_rssi;
      char *tok_enc;
      char *ssid_start;
      size_t ssid_len;
      int rssi;
      bool is_open;
      int dup_idx = -1;

      line[strcspn(line, "\r\n")] = 0;

      while (*p == ' ' || *p == '\t')
        {
          p++;
        }
      if (!*p || strncmp(p, "bssid", 5) == 0)
        {
          continue;
        }

      /* Field 0: BSSID (must contain ':') */
      tok_bssid = p;
      while (*p && *p != ' ' && *p != '\t')
        {
          p++;
        }
      if (!*p)
        {
          continue;
        }
      *p++ = 0;
      if (strchr(tok_bssid, ':') == NULL)
        {
          continue;
        }

      /* Field 1: frequency */
      while (*p == ' ' || *p == '\t')
        {
          p++;
        }
      tok_freq = p;
      while (*p && *p != ' ' && *p != '\t')
        {
          p++;
        }
      if (!*p || !*tok_freq)
        {
          continue;
        }
      *p++ = 0;

      /* Field 2: signal level (RSSI) */
      while (*p == ' ' || *p == '\t')
        {
          p++;
        }
      tok_rssi = p;
      while (*p && *p != ' ' && *p != '\t')
        {
          p++;
        }
      if (!*p || !*tok_rssi)
        {
          continue;
        }
      *p++ = 0;
      rssi = atoi(tok_rssi);

      /* Field 3: encode (0000 = open, otherwise encrypted) */
      while (*p == ' ' || *p == '\t')
        {
          p++;
        }
      tok_enc = p;
      while (*p && *p != ' ' && *p != '\t')
        {
          p++;
        }
      if (*p)
        {
          *p++ = 0;
        }
      if (!*tok_enc)
        {
          continue;
        }
      is_open = (strcmp(tok_enc, "0000") == 0);

      /* Field 4+: entire remainder of line is SSID (may contain spaces) */
      while (*p == ' ' || *p == '\t')
        {
          p++;
        }
      ssid_start = p;
      ssid_len = strlen(ssid_start);
      while (ssid_len > 0 &&
             (ssid_start[ssid_len - 1] == ' ' ||
              ssid_start[ssid_len - 1] == '\t'))
        {
          ssid_start[--ssid_len] = 0;
        }
      if (ssid_len == 0)
        {
          /* Hidden AP with blank SSID */
          continue;
        }

      /* Deduplicate by SSID: keep strongest RSSI */
      for (i = 0; i < n; i++)
        {
          if (strncmp(s_aps[i].ssid, ssid_start, sizeof(s_aps[i].ssid) - 1) == 0)
            {
              dup_idx = i;
              break;
            }
        }
      if (dup_idx >= 0)
        {
          if (rssi > s_aps[dup_idx].rssi)
            {
              s_aps[dup_idx].rssi = rssi;
              s_aps[dup_idx].open = is_open;
            }
          continue;
        }

      strncpy(s_aps[n].ssid, ssid_start, sizeof(s_aps[n].ssid) - 1);
      s_aps[n].ssid[sizeof(s_aps[n].ssid) - 1] = 0;
      s_aps[n].rssi = rssi;
      s_aps[n].open = is_open;
      n++;
    }

  /* Sort APs by RSSI descending (strongest signal first) */
  for (i = 0; i < n - 1; i++)
    {
      for (j = i + 1; j < n; j++)
        {
          if (s_aps[j].rssi > s_aps[i].rssi)
            {
              dm_ap_t tmp = s_aps[i];
              s_aps[i] = s_aps[j];
              s_aps[j] = tmp;
            }
        }
    }

  s_ap_n = n;
  if (s_sel >= s_ap_n)
    {
      s_sel = (s_ap_n > 0) ? 0 : -1;
    }
  pthread_mutex_unlock(&s_lock);
  fclose(fp);
  return n;
}

static void *scan_thread(void *arg)
{
  int i;
  int n;

  (void)arg;

  /* Wait briefly if an auto-connect thread is yielding */
  for (i = 0; i < 50 && s_conn_busy; i++)
    {
      usleep(100 * 1000);
    }

  if (ensure_driver() < 0)
    {
      s_scan_busy = 0;
      s_scan_ready = 1;
      return NULL;
    }

  /* Only bring interface up in STA mode if not already connected */
  if (!dm_wifi_connected())
    {
      cli_sta_up();
      usleep(200 * 1000);
    }

  /* Match 9.5 demo get_wifi.sh: wapi scan wlan0 > /tmp/wifi_scan_result.txt */
  (void)system("wapi scan wlan0 > /tmp/wifi_scan_result.txt");
  usleep(300 * 1000);

  n = collect_aps_from_txt();
  if (n <= 0)
    {
      usleep(500 * 1000);
      (void)system("wapi scan wlan0 > /tmp/wifi_scan_result.txt");
      usleep(300 * 1000);
      n = collect_aps_from_txt();
    }

  syslog(LOG_INFO, "[deskmate-wifi] wapi scan -> %d aps\n", n);
  for (i = 0; i < s_ap_n; i++)
    {
      syslog(LOG_INFO, "[deskmate-wifi]  ap %s rssi=%d open=%d\n",
             s_aps[i].ssid, s_aps[i].rssi, (int)s_aps[i].open);
    }

  s_scan_busy = 0;
  s_scan_ready = 1;
  return NULL;
}

int dm_wifi_start_scan(void)
{
  pthread_t th;
  pthread_attr_t attr;
  int ret;

  if (s_scan_busy)
    {
      return 0;
    }

  /* Interrupt any background boot auto-connect so manual scan is never blocked */
  s_manual_override = 1;
  if (s_conn_busy && !s_conn_is_auto)
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

/* Official connect — ifup → mode 2 → psk(3 2) → essid(1) → wait → renew */
static void *conn_thread(void *arg)
{
  char cmd[192];
  char qssid[80];
  char qpass[160];
  int i;
  int got_ip = 0;
  int is_auto = s_conn_is_auto;
  struct in_addr addr;
  char try_ssid[33];
  char try_pass[64];
  bool try_open;

  (void)arg;

  strncpy(try_ssid, s_pending_ssid, sizeof(try_ssid) - 1);
  try_ssid[sizeof(try_ssid) - 1] = 0;
  strncpy(try_pass, s_pending_pass, sizeof(try_pass) - 1);
  try_pass[sizeof(try_pass) - 1] = 0;
  try_open = s_pending_open;

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

  if (is_auto && s_manual_override)
    {
      s_conn_busy = 0;
      return NULL;
    }

  /* Save credentials to /data/wifi.cfg first (compatible with 9.5 wifi_manager) */
  if (!is_auto)
    {
      wifi_cred_save(try_ssid, try_pass);
    }

  /* 1) ifup + STA mode */
  cli_sta_up();

  /* 2) psk — WPA_ALG_CCMP = 3, WPA2 = 2 */
  if (!try_open && try_pass[0])
    {
      if (shell_quote(try_pass, qpass, sizeof(qpass)) < 0)
        {
          s_conn_busy = 0;
          s_conn_fail = 1;
          return NULL;
        }
      snprintf(cmd, sizeof(cmd), "wapi psk %s %s 3 2", DM_WIFI_IF, qpass);
      syslog(LOG_INFO, "[deskmate-wifi] %s\n", cmd);
      (void)system(cmd);
    }

  /* 3) essid — flag 1 = connect */
  if (shell_quote(try_ssid, qssid, sizeof(qssid)) < 0)
    {
      s_conn_busy = 0;
      s_conn_fail = 1;
      return NULL;
    }
  snprintf(cmd, sizeof(cmd), "wapi essid %s %s 1", DM_WIFI_IF, qssid);
  syslog(LOG_INFO, "[deskmate-wifi] %s\n", cmd);
  (void)system(cmd);

  /* 4) Wait for WPA2 4-way handshake / association (interruptible if auto) */
  for (i = 0; i < 25; i++)
    {
      if (is_auto && s_manual_override)
        {
          s_conn_busy = 0;
          return NULL;
        }
      usleep(100 * 1000);
    }

  /* 5) DHCP renew — single DHCP call per attempt */
  for (i = 0; i < 4; i++)
    {
      if (is_auto && s_manual_override)
        {
          s_conn_busy = 0;
          return NULL;
        }

      (void)system("renew wlan0");
      if (wifi_get_valid_ip(&addr) == 0)
        {
          got_ip = 1;
          break;
        }

      if (is_auto && s_manual_override)
        {
          s_conn_busy = 0;
          return NULL;
        }

      if (netlib_obtain_ipv4addr(DM_WIFI_IF) >= 0 &&
          wifi_get_valid_ip(&addr) == 0)
        {
          got_ip = 1;
          break;
        }

      syslog(LOG_WARNING, "[deskmate-wifi] renew retry %d\n", i + 1);
      sleep(1);
    }

  /* Fix: Only set s_cur_ssid when got_ip is true! */
  pthread_mutex_lock(&s_lock);
  if (got_ip)
    {
      strncpy(s_cur_ssid, try_ssid, sizeof(s_cur_ssid) - 1);
      s_cur_ssid[sizeof(s_cur_ssid) - 1] = 0;
      strncpy(s_cur_ip, inet_ntoa(addr), sizeof(s_cur_ip) - 1);
      s_cur_ip[sizeof(s_cur_ip) - 1] = 0;
    }
  else
    {
      s_cur_ssid[0] = 0;
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
      if (!is_auto)
        {
          s_conn_fail = 1;
        }
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

static int wifi_start_connect_internal(const char *ssid, const char *pass,
                                       int is_auto)
{
  pthread_t th;
  pthread_attr_t attr;
  int ret;
  int i;

  if (!ssid || !ssid[0])
    {
      return -1;
    }

  if (!is_auto)
    {
      s_manual_override = 1;
      for (i = 0; i < 50 && (s_scan_busy || (s_conn_busy && s_conn_is_auto)); i++)
        {
          usleep(100 * 1000);
        }
    }

  if (s_scan_busy || s_conn_busy)
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
  s_pending_open = (pass == NULL || s_pending_pass[0] == 0);

  s_conn_ok = 0;
  s_conn_fail = 0;
  s_conn_is_auto = is_auto;
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

int dm_wifi_connect(const char *ssid, const char *pass)
{
  return wifi_start_connect_internal(ssid, pass, 0);
}

/* Boot auto-connect: checks if already connected, otherwise tries saved
 * credentials once or twice and immediately yields if user opens manual scan. */
static void *auto_thread(void *arg)
{
  char ssid[33];
  char pass[64];
  struct in_addr addr;
  int round;
  int i;

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

  for (round = 0; round < 2 && !s_manual_override; round++)
    {
      if (ensure_driver_wait(300) < 0)
        {
          if (s_manual_override)
            {
              return NULL;
            }
          sleep(2);
          continue;
        }

      /* If board or wifi_manager already obtained an IP, adopt it directly */
      if (wifi_get_valid_ip(&addr) == 0)
        {
          char assoc_ssid[33];
          wifi_is_associated(assoc_ssid, sizeof(assoc_ssid));
          pthread_mutex_lock(&s_lock);
          snprintf(s_cur_ssid, sizeof(s_cur_ssid), "%s",
                   assoc_ssid[0] ? assoc_ssid : ssid);
          snprintf(s_cur_ip, sizeof(s_cur_ip), "%s", inet_ntoa(addr));
          pthread_mutex_unlock(&s_lock);
          syslog(LOG_INFO, "[deskmate-wifi] already connected ssid=%s ip=%s\n",
                 s_cur_ssid, s_cur_ip);
          return NULL;
        }

      if (dm_wifi_connected() || s_manual_override)
        {
          return NULL;
        }

      dm_wifi_clear_conn_flags();
      if (wifi_start_connect_internal(ssid, pass, 1) != 0)
        {
          sleep(2);
          continue;
        }

      /* Wait for conn_thread to finish or manual override */
      for (i = 0; i < 180 && s_conn_busy && !s_manual_override; i++)
        {
          usleep(100 * 1000);
        }

      if (dm_wifi_connected())
        {
          syslog(LOG_INFO, "[deskmate-wifi] auto connected ssid=%s ip=%s\n",
                 dm_wifi_cur_ssid(), dm_wifi_cur_ip());
          dm_wifi_clear_conn_flags();
          return NULL;
        }

      dm_wifi_clear_conn_flags();
      if (s_manual_override)
        {
          return NULL;
        }
      sleep(2);
    }

  syslog(LOG_INFO,
         "[deskmate-wifi] auto-connect finished; manual UI ready\n");
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

#endif /* CONFIG_DESKMATE_APP */
