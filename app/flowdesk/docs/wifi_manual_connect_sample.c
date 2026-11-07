/*
 * wifi_manual_connect.c — 评审修正版
 *
 * 对照《WiFi 适配驱动指南》§9 与板级实测：
 *   ifup wlan0  →  wapi mode 2  →  wapi psk 3  →  wapi essid 1  →  renew
 *
 * 相对原稿的修复见文件末尾注释。
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <errno.h>

#define WIFI_IFNAME       "wlan0"
#define WIFI_INPUT_MAX    128
#define WIFI_CMD_MAX      256
#define WIFI_RETRY_COUNT  5
#define WIFI_RETRY_DELAY  3

static void trim_newline(char *text)
{
  size_t len;

  if (text == NULL)
    {
      return;
    }
  len = strlen(text);
  while (len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r'))
    {
      text[--len] = '\0';
    }
}

static int is_blank(const char *text)
{
  if (text == NULL || *text == '\0')
    {
      return 1;
    }
  while (*text != '\0')
    {
      if (!isspace((unsigned char)*text))
        {
          return 0;
        }
      text++;
    }
  return 1;
}

/*
 * NSH 对 '"'"' 拼接支持不完整。
 * 策略：一般字符直接用单引号包起来；含单引号则拒绝，提示改密码/SSID。
 */
static int shell_quote_safe(char *out, size_t out_size, const char *input)
{
  size_t used;
  size_t n;

  if (out == NULL || input == NULL || out_size < 3)
    {
      return -1;
    }
  if (strchr(input, '\'') != NULL)
    {
      return -1;
    }

  n = strlen(input);
  if (n + 3 > out_size)
    {
      return -1;
    }

  used = 0;
  out[used++] = '\'';
  memcpy(out + used, input, n);
  used += n;
  out[used++] = '\'';
  out[used] = '\0';
  return (int)used;
}

static int run_command(const char *command)
{
  int ret;

  printf("\n[WiFi] $ %s\n", command);
  ret = system(command);
  if (ret != 0)
    {
      printf("[WiFi] 命令返回：%d (errno=%d %s)\n",
             ret, errno, strerror(errno));
      return -1;
    }
  return 0;
}

/* 等待板级 bringup 注册 wlan0（最多 ~10s） */
static int wait_wlan0(int timeout_100ms)
{
  int i;

  for (i = 0; i < timeout_100ms; i++)
    {
      if (if_nametoindex(WIFI_IFNAME) != 0)
        {
          printf("[WiFi] %s 已就绪\n", WIFI_IFNAME);
          return 0;
        }
      usleep(100 * 1000);
    }

  printf("[WiFi] 错误：没有 %s。请先确认板级 SDIO/probe 成功\n",
         WIFI_IFNAME);
  printf("[WiFi] 串口应有: [rtl-sdio] sdio_probe ok / netdev registered\n");
  return -1;
}

static int ifup_wlan0(void)
{
  char command[WIFI_CMD_MAX];

  /* 官方与社区一致：必须先 ifup */
  snprintf(command, sizeof(command), "ifup %s", WIFI_IFNAME);
  return run_command(command);
}

static int set_station_mode(void)
{
  char command[WIFI_CMD_MAX];

  snprintf(command, sizeof(command), "wapi mode %s 2", WIFI_IFNAME);
  return run_command(command);
}

static int scan_wifi(void)
{
  char command[WIFI_CMD_MAX];

  snprintf(command, sizeof(command), "wapi scan %s", WIFI_IFNAME);
  /* 扫描失败不阻断手动输入 SSID */
  if (run_command(command) < 0)
    {
      printf("[WiFi] 扫描失败，仍可手动输入 SSID\n");
    }
  return 0;
}

static int connect_wifi(const char *ssid, const char *password)
{
  char qssid[WIFI_INPUT_MAX + 8];
  char qpass[WIFI_INPUT_MAX + 8];
  char command[WIFI_CMD_MAX];

  if (shell_quote_safe(qpass, sizeof(qpass), password) < 0)
    {
      printf("[WiFi] 密码含单引号或过长，请改用不含 ' 的密码\n");
      return -1;
    }
  if (shell_quote_safe(qssid, sizeof(qssid), ssid) < 0)
    {
      printf("[WiFi] SSID 含单引号或过长\n");
      return -1;
    }

  /* 分步执行，避免 NSH 对 && 支持不完整；psk 失败可单独看到 */
  snprintf(command, sizeof(command),
           "wapi psk %s %s 3", WIFI_IFNAME, qpass);
  if (run_command(command) < 0)
    {
      return -1;
    }

  /* 可选：显式 WPA2 版本（文档 §1.3.2） */
  snprintf(command, sizeof(command),
           "wapi psk %s %s 3 2", WIFI_IFNAME, qpass);
  (void)run_command(command); /* 失败可忽略，有的栈只吃 3 参数形式 */

  snprintf(command, sizeof(command),
           "wapi essid %s %s 1", WIFI_IFNAME, qssid);
  return run_command(command);
}

static int renew_ip(void)
{
  char command[WIFI_CMD_MAX];

  snprintf(command, sizeof(command), "renew %s", WIFI_IFNAME);
  return run_command(command);
}

static int show_ip_config(void)
{
  /* NuttX 常用 ifconfig；文档截图里写过 ipconfig，两者都试 */
  if (run_command("ifconfig") == 0)
    {
      return 0;
    }
  return run_command("ipconfig");
}

static int verify_connected(void)
{
  if (run_command("wapi show wlan0") < 0)
    {
      printf("[WiFi] wapi show 失败\n");
    }
  return show_ip_config();
}

int main(int argc, char *argv[])
{
  char ssid[WIFI_INPUT_MAX];
  char password[WIFI_INPUT_MAX];
  int retry;
  int got_ip = 0;

  (void)argc;
  (void)argv;

  printf("========================================\n");
  printf(" NuttX WiFi 手动连接（修正版）\n");
  printf(" 顺序: ifup → mode → psk → essid → renew\n");
  printf("========================================\n");

  /* 0) 板级必须已注册 wlan0 */
  if (wait_wlan0(100) < 0)
    {
      return EXIT_FAILURE;
    }

  /* 1) 先 ifup（官方/群内：这一步必须最先） */
  if (ifup_wlan0() < 0)
    {
      printf("[WiFi] ifup 失败，继续尝试 mode/scan\n");
    }

  /* 2) STA 模式 */
  if (set_station_mode() < 0)
    {
      printf("[WiFi] 无法设置 STA 模式。\n");
      return EXIT_FAILURE;
    }

  /* 再 ifup 一次，确保 mode 之后接口仍 up */
  (void)ifup_wlan0();

  /* 3) 扫描（失败不退出） */
  (void)scan_wifi();

  printf("\n请输入目标 WiFi 的 SSID：");
  fflush(stdout);
  if (fgets(ssid, sizeof(ssid), stdin) == NULL)
    {
      printf("\n[WiFi] 读取 SSID 失败。\n");
      return EXIT_FAILURE;
    }
  trim_newline(ssid);
  if (is_blank(ssid))
    {
      printf("[WiFi] SSID 不能为空。\n");
      return EXIT_FAILURE;
    }

  printf("请输入 WiFi 密码：");
  fflush(stdout);
  if (fgets(password, sizeof(password), stdin) == NULL)
    {
      printf("\n[WiFi] 读取密码失败。\n");
      return EXIT_FAILURE;
    }
  trim_newline(password);
  if (is_blank(password))
    {
      printf("[WiFi] 密码不能为空（开放网络请另写 open 流程）。\n");
      return EXIT_FAILURE;
    }

  /* 4) psk → essid */
  if (connect_wifi(ssid, password) < 0)
    {
      printf("[WiFi] 设置 SSID 或密码失败。\n");
      return EXIT_FAILURE;
    }

  /* 5) 等 4 次握手 / 关联，再 DHCP（原稿 2s 偏短） */
  printf("\n[WiFi] 等待关联与 DHCP...\n");
  sleep(3);

  for (retry = 0; retry < WIFI_RETRY_COUNT; retry++)
    {
      if (renew_ip() == 0)
        {
          got_ip = 1;
          break;
        }
      printf("[WiFi] renew 未成功，%ds 后重试 (%d/%d)\n",
             WIFI_RETRY_DELAY, retry + 1, WIFI_RETRY_COUNT);
      sleep(WIFI_RETRY_DELAY);
    }

  if (verify_connected() < 0)
    {
      printf("[WiFi] 无法读取网络配置。\n");
      return EXIT_FAILURE;
    }

  if (!got_ip)
    {
      printf("[WiFi] 警告：renew 多次失败。请检查密码/AP/信号。\n");
      printf("[WiFi] 期望串口出现: WPA/WPA2 handshake done\n");
      return EXIT_FAILURE;
    }

  printf("\n[WiFi] 连接流程完成，请检查 wlan0 是否有 IP（非 0.0.0.0）。\n");
  return EXIT_SUCCESS;
}

/*
 * ===================== 原稿问题清单 =====================
 * 1) 缺少 ifup wlan0 —— 官方要求最先执行；你们板上尤其关键。
 * 2) 未等待 wlan0 注册 —— sdio_probe 失败时 ifconfig 只有 lo，
 *    本程序会一直报 mode/scan 失败却说不清原因。
 * 3) psk 与 essid 写在同一条 system("... && ...") —— NSH 对 &&
 *    支持不完整时会整条失败；且无法单独定位 psk/essid 谁错了。
 * 4) essid 后只等 2s 就 renew —— WPA2 四次握手常需 3s+，
 *    建议 essid 后 sleep(3)，再循环 renew。
 * 5) ipconfig —— NuttX 更常见是 ifconfig；已两者都试。
 * 6) shell_quote 用 '"'"' 拼接 —— NSH 未必支持；
 *    已改为「不含单引号才接受」的保守策略。
 * 7) 扫描失败直接 EXIT —— 扫描挂了仍应允许手输 SSID。
 * 8) 无 wapi show / 无 IP 校验 —— renew 成功≠已拿到地址。
 *
 * ===================== 与当前板子的关系 =====================
 * 若串口仍有:
 *   [rtl-sdio] sdio_probe ret=-1
 *   ifconfig 仅有 lo
 * 则本程序任何版本都会失败 —— 先解决板级 SDIO/probe，
 * 再用本手动流程连 Xiaomi_0521_Wi-Fi5 / kisslcy520。
 */
