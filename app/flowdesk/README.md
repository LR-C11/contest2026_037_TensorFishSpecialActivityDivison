# FlowDesk — Gemini-S1 桌搭伴侣（v0.18.1）

全新应用，**未复用**参赛仓 `desk_companion` 源码。

## 功能

### 专注
- **主页 = 专注设定**：15/25/45/60 + 自选
- **淡入倒计时页**、**滚轮倒计时** `MM:SS`
- **表情**：眨眼 / 注视 / 星星眼 / 爱心眼；点脸互动
- **完成页**：语音提示「专注完成」

### 语音聊天
- **点按录音**：点一下开始录，再点结束并发送
- 链路：`/dev/audio/pcm0c` → ASR `mimo-v2.5-asr` → LLM `mimo-v2.6-flash` → TTS `mimo-v2.5-tts` → 喇叭
- 云端：`https://token-plan-sgp.xiaomimimo.com/v1`

### 词典（BandLingo 风格）
- 本地词库约 6.5 万词（`word|definition`，a–z 分片，LRU 缓存）
- 英文查词 / 中文查释义 / 每日一词 / AI 释义
- **独立可滑动页面**：查词列表、释义详情

### 设定（6 项，可滑动）
| 项 | 说明 |
|----|------|
| Wi-Fi | 手动扫描连接（`wapi` 重试） |
| 蓝牙耳机 | A2DP Source 扫描/连接耳机 |
| App 连接 | 专属 BLE Challenge-Response 配对 |
| 音量 | 滑杆 / 静音 / 试听 + **音频输出**（自动/喇叭/耳机） |
| 播报声音 | 冰糖 / 茉莉 / 苏打 / 白桦 等多音色 |
| 关于 | 独立页：版本 + 开发者信息 |

### 主动提醒（离线优先）
- 吃药 / 喝水 / 早间简报 / 专注完成 → 预置 WAV 提示音 + 屏幕文字
- Skill：`/resource/agent/skills/`（reminder、daily-briefing、focus-coach）

### 其它
- 背单词、2048、MBTI、备忘、吃药、喝水、计算器等
- **底栏**：专注 / 聊天 / 健康 / 功能 / 设定

## 目录

```
vendor/allwinnertech/apps/flowdesk/
  deskmate.h / deskmate_main.c / dm_state.c / dm_face.c / dm_roll.c
  dm_ui_focus.c / dm_ui_shells.c / dm_ui_health.c
  dm_wifi.c / dm_ui_wifi.c          # WiFi + 设定入口
  dm_ui_btnet.c                     # 蓝牙耳机 A2DP
  dm_ui_appconn.c / dm_ble_gatt.c   # App BLE 配对
  dm_ui_chat.c / dm_chat_ai.c / dm_voice.c
  dm_dict.c / dm_ui_dict.c
  dm_audio_vol.c / dm_ui_vol.c / dm_audio_route.c
  dm_tts_voice.c / dm_ui_voice.c
  dm_ui_about.c / dm_proactive.c
  dm_ui_word.c / dm_ui_tools.c / dm_ui_life.c / dm_ui_fun.c / ...
  docs/场景说明.md
  Makefile / Make.defs / Kconfig / CMakeLists.txt
```

资源（打包进 `/resource`）：
- `dict/*.txt` — 词库
- `audio/*.wav` — 离线提示音
- `agent/skills/*.md` — Skill 定义
- `imgs/author.png` — 关于页作者头像

## 编译（WSL）

```bash
cd /home/lrc/vela
export PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin:/home/lrc/vela/prebuilts/gcc/linux-x86_64/arm-none-eabi/bin

rm -f vendor/allwinnertech/apps/flowdesk/*.o
rm -f vendor/allwinnertech/apps/libapps.a nuttx/staging/libapps.a

./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ -j8

strings nuttx/vela.bin | grep '0.18.1'
```

改过 `defconfig`（如 A2DP Source）时先：

```bash
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ distclean -j8
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ -j8
```

## 打包

```bash
cp -f nuttx/vela.bin vendor/allwinnertech/lichee/board/r528s3/gemini-s1_nand/configs/nsh.fex
cd vendor/allwinnertech/lichee && source envsetup.sh
lunch_nuttx   # 选 2: r528s3-gemini-s1
pack
```

镜像：

```text
lichee/out/r528s3/gemini-s1_nand/rtos_nuttx_r528s3-gemini-s1_uart0_128Mnand.img
```

## 运行

```bash
flowdesk &
# 或开机自启（rcS.nsh 已配置）
```

日志：`FlowDesk 0.18.1 focus-home`

## 配置

`nsh_minidisplay/defconfig`：

```
CONFIG_DESKMATE_APP=y
CONFIG_BLUETOOTH_A2DP_SOURCE=y
CONFIG_BT_A2DP_SOURCE=y
```

## 说明

- 中文优先 FreeType MiSans，否则 Montserrat
- **未改驱动**（`chips/` 保持官方）；仅本目录 + defconfig/资源
- 蓝牙耳机需耳机进入**配对模式**后再扫描连接
