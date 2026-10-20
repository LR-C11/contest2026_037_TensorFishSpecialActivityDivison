# contest2026_037_TensorFishSpecialActivityDivison

## 作品名称 / Work Name

**FlowDesk 心流桌伴** — openvela 智能桌搭陪伴终端 / OpenVela desk companion terminal

**Version:** 0.18.1  
**Track / 赛道:** AI 硬件产品创新 / AI Hardware Product Innovation  
**Team / 队伍:** TensorFishSpecialActivityDivison (037)  
**Author / 作者:** LR-C11

---

## 一、作品简介 / Introduction

**中文：**  
FlowDesk 针对桌搭产品「功能单一、缺少陪伴」的问题，基于 **Gemini-S1（全志 R528 / OpenVela）** 实现多功能桌面终端。本地提供专注计时、词典（6.5 万词）、健康情绪、备忘、吃药/喝水提醒、背单词、2048、MBTI 等；语音聊天点按录音，接入小米 **MiMo**（ASR / LLM / TTS），回复可从 4Ω3W 喇叭或蓝牙耳机播出；断网时自动本地回复与离线提示音。界面为 LVGL 320×240 + 底部五栏导航，吉祥物贯穿专注与聊天。

**English:**  
FlowDesk is a multi-function desk companion on **Gemini-S1 (R528 / OpenVela)**. On-device: focus timer, dictionary (65k words), mood/health, notes, med/water reminders, word study, 2048, MBTI. Voice chat (tap-to-talk) uses Xiaomi **MiMo** ASR/LLM/TTS with speaker or BT headset output; offline falls back to local replies and alert tones. LVGL 320×240 UI with a 5-tab dock and shared mascot.

---

## 二、功能一览 / Feature Overview

### 导航 / Dock

专注 Focus · 聊天 Chat · 健康 Health · 功能 Features · 设定 Settings

### 详细功能 / Detailed Features

#### 专注 Focus
- 自选时长 Custom duration: 15 / 25 / 45 / 60 + custom
- 进行页 Running: 滚动 MM:SS 时钟 + 吉祥物
- 完成页 Done: 恭喜、表情、**语音播报** Voice notice
- 暂停/结束 Pause/End

#### 聊天 Chat
- **点按录音** Tap-to-talk: 点一下开始录，再点结束并发送
- 链路 Pipeline: 录音 → ASR `mimo-v2.5-asr` → LLM `mimo-v2.6-flash` → TTS `mimo-v2.5-tts` → 喇叭/耳机
- 云端 MiMo（OpenAI 兼容）
- 离线本地人格回复 Offline persona replies
- 自称 **openvela小助手**

#### 健康 Health
- 情绪综合分（多因子）Multi-factor mood score
- 心情记录 Mood log
- 吃药 / 喝水提醒 Med / water reminders（到点可主动播报）

#### 词典 Dictionary（独立可滑动页）
- 本地词库约 6.5 万词（a–z 分片 + LRU）
- 英文查词 / 中文查释义 / 每日一词 / AI 释义
- Scrollable search results & detail pages

#### 功能页 Features（完整清单）

**学习与效率 Study**
| 中文 | English |
|------|---------|
| 词典（查词 / 每日一词 / AI） | Dictionary (lookup / daily / AI) |
| 备忘（列表/添加/拼音/持久化） | Notes (list/add/pinyin/persist) |
| 背单词 1000+（学/练/测/错题） | Word Memo 1000+ (study/quiz/wrong) |
| 计算器 | Calculator |
| 倒数日 | Countdown |

**生活健康 Life & Health**
| 中文 | English |
|------|---------|
| 吃药提醒（每天 1/2/3 次 + 主动语音） | Med reminders + proactive voice |
| 喝水提醒打卡 | Water check-in |
| 吃什么随机 | What to eat |
| BMI 计算 | BMI |
| 环境传感（温湿度/光照） | Sensors (temp/humi/light) |

**游戏与测试 Games & Tests**
| 中文 | English |
|------|---------|
| 2048 | 2048 |
| 猜数字 1–100 | Guess number |
| 24 点 | Make 24 |
| 画板涂鸦 | Doodle |
| MBTI：4 套 × 52 题 | MBTI: 4 banks × 52 |

**设定 Settings（6 项，可滑动）**
| 中文 | English |
|------|---------|
| Wi-Fi 扫描 / 连接 / 开关 | Wi-Fi scan / connect / switch |
| 蓝牙耳机（A2DP Source） | BT headset (A2DP source) |
| App 连接（BLE 专属配对） | App connect (dedicated BLE pair) |
| 音量 + 音频输出（喇叭/耳机/自动） | Volume + output channel |
| 播报声音（多音色） | TTS voice picker |
| 关于（独立页） | About (standalone page) |

#### 吉祥物 Mascot
- 几何白色小脸，眨眼 Geometric blinking face
- 多表情 Multiple expressions

#### 自定义 Skill / 主动能力
- Skills: `reminder` / `daily-briefing` / `focus-coach`（`/resource/agent/skills/`）
- 主动：吃药、喝水、早间简报、专注完成 → **离线 WAV 提示 + 屏幕文字**，联网可 TTS

---

## 三、技术要点 / Technical Highlights

| 项 Item | 说明 Description |
|---------|------------------|
| 图形 Graphics | LVGL on OpenVela/NuttX；SPI LCD 320×240 + 触摸 |
| 语音 Voice | `/dev/audio/pcm0c` 麦 + `pcm0p` 喇叭 4Ω3W；MiMo ASR/TTS |
| AI | MiMo HTTPS；离线关键词人格 |
| 蓝牙 BT | BLE 配对（Challenge-Response）+ A2DP 耳机输出 |
| 词典 Dict | 本地分片词库 + 云端 AI 释义 |
| 降级 Degradation | 无网功能可用；离线提示音 + 本地聊天 |

MiMo 接口 / Endpoint:

```text
https://token-plan-sgp.xiaomimimo.com/v1
Models: mimo-v2.6-flash / mimo-v2.5-asr / mimo-v2.5-tts
Auth: Authorization: Bearer <API_KEY>
```

---

## 四、目录结构 / Repository Layout

```text
app/flowdesk/     # FlowDesk 应用源码 / application source
logs/             # AI Coding 日志 / AI coding session logs
board/            # 板级相关（如有）/ board material (if any)
```

应用详细说明见：[`app/flowdesk/README.md`](app/flowdesk/README.md)。

---

## 五、运行方式 / How to Run

```bash
cd <openvela-root>
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ -j8
strings nuttx/vela.bin | grep '0.18.1'

cp -f nuttx/vela.bin \
  vendor/allwinnertech/lichee/board/r528s3/gemini-s1_nand/configs/nsh.fex
cd vendor/allwinnertech/lichee
source envsetup.sh && lunch_nuttx   # 2 → r528s3-gemini-s1
pack
```

镜像 / Image: `out/r528s3/gemini-s1_nand/rtos_nuttx_r528s3-gemini-s1_uart0_128Mnand.img`  
烧录 / Flash: PhoenixSuit。

---

## 六、AI Coding 说明 / AI Coding

本作品使用 AI 辅助进行方案拆解、LVGL 界面、词典/语音/蓝牙等实现与文档整理。对话日志见 `logs/` 目录。

AI assisted design, LVGL UI, dictionary/voice/BT features, and docs. Session logs are under `logs/`.

---

## 七、选题方向 / Direction

**AI 硬件产品创新** — 以健康干预与情感陪伴为核心，提供可离线使用的桌面终端，并接入大赛 MiMo 云能力（语音对话、TTS 播报、主动提醒）。

**AI Hardware Innovation** — Offline-first desk companion with contest MiMo cloud (voice chat, TTS, proactive notices).
