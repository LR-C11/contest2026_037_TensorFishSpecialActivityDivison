# Deskmate — Gemini-S1 桌搭伴侣

**Version:** 0.4.9  
**Board path:** `vendor/allwinnertech/apps/deskmate/`

## 底栏（5）

专注 | 聊天 | 健康 | 功能 | 设定

- **专注**：时长预设 → 淡入倒计时；表情眨眼/星星眼/爱心眼/点击互动；`MM:SS` 滚轮；真秒计时
- **健康**：可滑动；评分 = 心情记录 + 今日专注；独立「记录心情」页（多选标签 + 快捷句）写入 `/data/deskmate_mood.bin`
- **功能**：监督 / 备忘 独立页入口
- **设定**：Wi-Fi 独立页（扫描 / 密码 / 连接）

## Wi-Fi

对齐官方流程 + 板级 bringup：

```text
set_sdio_param(1,3,NULL)
sdio_initialize(1)
realtek_wl_sdio_init
realtek_wl_initialize(RTW_MODE_STA=1)  // 必须 STA，0=NONE 不会 wifi_on
wapi mode/psk/essid/renew
```

后台线程执行，不阻塞 LVGL。

## 编译

```bash
cd /home/lrc/vela
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ -j8
cp -f nuttx/vela.bin \
  vendor/allwinnertech/lichee/board/r528s3/gemini-s1_nand/configs/nsh.fex
cd vendor/allwinnertech/lichee && source envsetup.sh && lunch_nuttx && pack
```

`CONFIG_DESKMATE_APP=y`；建议关闭 `CONFIG_LUNCHER_MINI_APP`。

日志：`Deskmate 0.4.9`
