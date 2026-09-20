# contest2026_037_TensorFishSpecialActivityDivison

## 作品名称 / Work Name

**心流桌伴** — openvela 智能桌搭陪伴终端 / OpenVela desk companion terminal

**Version:** 0.13.2  
**Track / 赛道:** AI 硬件产品创新 / AI Hardware Product Innovation  
**Team / 队伍:** TensorFishSpecialActivityDivison (037)  
**Author / 作者:** LR-C11 · carsonlinrui@gmail.com

---

## 一、作品简介 / Introduction

**中文：**  
心流桌伴针对桌搭产品“功能单一、缺少陪伴”的问题，基于 **Gemini-S1（全志 R528 / OpenVela）** 实现一款多功能桌面终端。本地集成专注计时、健康情绪综合分、备忘、吃药与喝水提醒、背单词、2048、MBTI 等能力；联网时接入小米 **MiMo** 大模型进行自然对话，助手自称 **openvela小助手**；断网时自动降级为本地回复。界面以 LVGL 单屏 + 底部五栏导航组织，吉祥物贯穿专注与聊天场景。

**English:**  
心流桌伴 is a multi-function desk companion on **Gemini-S1 (R528 / OpenVela)**. On-device features include focus timer, multi-factor mood score, notes, med/water reminders, word study, 2048, and MBTI. Online chat uses Xiaomi **MiMo** (assistant name: **openvela小助手**); offline falls back to local replies. UI is LVGL with a 5-tab dock and a shared mascot on Focus and Chat.

---

## 二、功能一览 / Feature Overview

### 导航 / Dock

专注 Focus · 聊天 Chat · 健康 Health · 功能 Features · 设定 Settings

### 详细功能 / Detailed Features

#### 专注 Focus
- 自选时长 Custom duration: 1 / 5 / 10 / 15 / 20 / 25… 分钟
- 进行页 Running: 倒计时 + 吉祥物
- 完成页 Done: 恭喜与表情
- 暂停/结束 Pause/End
- 滚动 MM:SS 时钟 Rolling clock
- 完成时长计入健康统计 Feeds health stats

#### 聊天 Chat
- 居中吉祥物，可点击 Center tappable mascot
- 表情下方显示回答 Answer under the face
- 底部「按住说话」Hold-to-talk (bottom, above dock)
- 云端 MiMo（OpenAI 兼容，mimo-v2.5-pro）
- 离线本地人格回复 Offline persona replies
- 自称 **openvela小助手**
- 简化 UI：无输入框/发送/清空 No text box/send/clear

#### 健康 Health
- 情绪综合分（多因子本地计算）Multi-factor mood score
- 心情记录 Mood log
- 低分关怀建议 Care tips when score is low

#### 功能页 Features（完整清单）

**学习与效率 Study**
| 中文 | English |
|------|---------|
| 备忘（列表/添加/拼音/持久化） | Notes (list/add/pinyin/persist) |
| 背单词 1000+（学/练/测/错题） | Word Memo 1000+ (study/quiz/wrong) |
| 计算器 | Calculator |
| 倒数日 | Countdown |
| 监督 | Supervise |

**生活健康 Life & Health**
| 中文 | English |
|------|---------|
| 吃药提醒（键盘，每天 1/2/3 次） | Med reminders (keyboard, 1–3×/day) |
| 喝水提醒打卡 | Water check-in |
| 吃什么随机 | What to eat |
| BMI 计算 | BMI |
| 环境传感（温湿度/光照） | Sensors (temp/humi/light) |

**游戏与测试 Games & Tests**
| 中文 | English |
|------|---------|
| 2048（滑动合并、最佳分、标准算法） | 2048 (swipe merge, best score, standard algorithm) |
| 猜数字 1–100 | Guess number |
| 24 点 | Make 24 |
| 画板涂鸦 | Doodle |
| MBTI：4 套 × 52 题，四选一 2×2，随机抽套，四维百分比 | MBTI: 4 banks × 52, 4-choice 2×2, random bank, % per dimension |

**设定 Settings**
| 中文 | English |
|------|---------|
| WiFi 扫描 / 手动连接 / 自动连接 / 状态 | Scan / manual / auto connect / status |

#### 吉祥物 Mascot
- 几何白色小脸，眨眼 Geometric blinking face
- 多表情 Multiple expressions
- 专注页与聊天页共用 Shared on Focus & Chat

#### 自定义 Skill
- 名称 `desk-companion`
- 路径 `/data/agent/skills/desk-companion/SKILL.md`
- 场景：陪伴对话、结合专注/吃药/喝水给建议
- Scene: companionship + status-aware hints; offline-safe

---

## 三、技术要点 / Technical Highlights

| 项 Item | 说明 Description |
|---------|------------------|
| 图形 Graphics | LVGL on OpenVela/NuttX；SPI LCD 320×240 + 触摸 |
| AI | MiMo HTTPS Chat Completions；离线关键词人格 |
| 开发板 Board | Gemini-S1 only（R528，128MB） |
| 无线 Wireless | RTL8733 SDIO；自动/手动 WiFi；驱动 bringup 增强 |
| 降级 Degradation | 无网功能全开；聊天自动本地回复 |

MiMo 接口 / Endpoint:

```text
https://token-plan-sgp.xiaomimimo.com/v1
Model: mimo-v2.5-pro
Auth: Authorization: Bearer <API_KEY>
```

---

## 四、目录结构 / Repository Layout

```text
app/deskmate/     # Deskmate 应用源码 / application source
logs/             # AI Coding 日志 / AI coding session logs
board/            # 板级相关（如有）/ board material (if any)
```

应用详细说明见：[`app/deskmate/README.md`](app/deskmate/README.md)（中英双语完整功能列表）。

---

## 五、运行方式 / How to Run

```bash
cd <openvela-root>
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ -j8
strings nuttx/vela.bin | grep Deskmate   # Deskmate 0.13.2

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

本作品使用 AI 辅助进行方案拆解、LVGL 界面与 2048/MBTI 逻辑实现、WiFi 驱动排查与文档整理。对话日志见 `logs/` 目录。

AI assisted architecture, LVGL UI, 2048/MBTI logic, WiFi bringup, and docs. Session logs are under `logs/`.

---

## 七、选题方向 / Direction

**AI 硬件产品创新** — 以健康干预与情感陪伴为核心，将碎片化桌搭需求整合为可离线使用的桌面终端，并接入大赛 MiMo 云能力。

**AI Hardware Innovation** — Health-oriented, offline-first desk companion integrated with contest MiMo cloud services.
