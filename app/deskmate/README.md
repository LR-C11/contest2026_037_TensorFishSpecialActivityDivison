# Deskmate — OpenVela 桌搭陪伴终端

**Version / 版本：** `0.13.2`  
**Board / 开发板：** Gemini-S1（Allwinner R528 / OpenVela）  
**Author / 作者：** LR-C11 · carsonlinrui@gmail.com

Deskmate 是一款面向桌面场景的智能陪伴终端：本地优先完成专注、健康、学习与工具类功能，在联网时接入小米 **MiMo** 大模型提供对话能力；断网时自动降级，保证核心体验始终可用。

Deskmate is a desk companion terminal. It handles focus, health, study, and utility features locally first, and connects to Xiaomi **MiMo** for chat when online. Offline, it falls back automatically so core features stay available.

---

## 1. 主导航 / Main Dock

底部五栏 / Bottom navigation (5 tabs):

| 栏目 | English | 说明 |
|------|---------|------|
| 专注 | Focus | 计时专注，吉祥物陪伴 |
| 聊天 | Chat | 表情 + 回答 + 语音入口 |
| 健康 | Health | 情绪综合分与关怀建议 |
| 功能 | Features | 全部子功能入口列表 |
| 设定 | Settings | WiFi 与系统相关设置 |

---

## 2. 功能完整列表 / Complete Feature List

### 2.1 专注 / Focus

| 功能点 | English | 说明 |
|--------|---------|------|
| 自选时长 | Custom duration | 1 / 5 / 10 / 15 / 20 / 25… 分钟可调 |
| 专注进行页 | Focus running | 倒计时 + 吉祥物陪伴与表情 |
| 完成恭喜页 | Done screen | 结束后祝贺文案与表情反馈 |
| 暂停/结束 | Pause / End | 支持中途暂停或结束 |
| 滚动时钟 | Rolling clock | MM:SS 滚动显示剩余时间 |
| 与健康联动 | Health link | 完成时长计入健康/情绪相关统计 |

**English:** Pick 1/5/10… minutes, run with the mascot, see a congratulation page when done; focus minutes feed into health stats.

---

### 2.2 聊天 / Chat（0.13.2 简化 UI）

| 功能点 | English | 说明 |
|--------|---------|------|
| 居中吉祥物 | Center mascot | 与专注页同款几何小脸，可点击互动 |
| 回答显示 | Answer display | 表情下方显示最近一条回答 |
| 按住说话 | Hold-to-talk | 底部唯一主控件，避开导航栏 |
| 云端 MiMo | Cloud MiMo | 联网时走 OpenAI 兼容接口（mimo-v2.5-pro） |
| 离线兜底 | Offline fallback | 无网时本地人格回复，界面不空白 |
| 自称 | Persona | **openvela小助手** / openvela Assistant |
| 无输入框 | No text box | 0.13.2 起去掉输入框、发送、清空 |

**English:** One face in the center, answer underneath, hold-to-talk at the bottom. Online → MiMo; offline → local replies. Assistant name: **openvela小助手**.

MiMo 接口 / Endpoint:

- Base: `https://token-plan-sgp.xiaomimimo.com/v1`（OpenAI-compatible）
- Auth: `Authorization: Bearer <API_KEY>`（公开仓占位符 / placeholder in public repo）
- Model: `mimo-v2.5-pro`（可扩展 asr / tts）

---

### 2.3 健康 / Health

| 功能点 | English | 说明 |
|--------|---------|------|
| 情绪综合分 | Mood score | 多因子本地加权（情绪、专注、喝水、吃药等） |
| 心情记录 | Mood log | 标签化记录当前/当日状态 |
| 关怀建议 | Care tips | 分数偏低时给出暖心提示 |
| 统计入口 | Stats | 展示与专注等数据的汇总 |

**English:** A multi-factor mood score computed on-device; log moods and get gentle suggestions when the score is low.

---

### 2.4 功能页完整清单 / Features Page — Full List

#### 学习与效率 / Study & Productivity

| 功能 | English | 说明 |
|------|---------|------|
| 备忘 | Notes | 列表 / 添加 / 删除；拼音中文输入；持久化到 `/data/deskmate_note.txt` |
| 背单词 | Word Memo | 1000+ 词库；学习 / 复习 / 测验 / 错题本；随机题库 |
| 计算器 | Calculator | 四则运算 |
| 倒数日 | Countdown | 记录目标日，显示剩余天数 |
| 监督 | Supervise | 盯住一件事，减少打扰 |

**English:** Notes with pinyin input and file persistence; word book (1000+) with study/quiz/wrong-book; calculator; countdown days; supervise task.

#### 健康与生活 / Health & Life

| 功能 | English | 说明 |
|------|---------|------|
| 吃药提醒 | Med reminders | 独立拼音键盘；每天次数 1/2/3；打卡提醒 |
| 喝水提醒 | Water | 打卡 + 间隔提醒 |
| 吃什么 | What to eat | 多组菜单随机帮你选一顿 |
| BMI 计算 | BMI | 身高体重简易评估 |
| 环境传感 | Sensors | 温湿度 / 光照（uORB 传感器） |

**English:** Meds with keyboard and 1–3×/day; water check-in; random meal picker; BMI; temp/humidity/light sensors.

#### 游戏与测试 / Games & Tests

| 功能 | English | 说明 |
|------|---------|------|
| 2048 | 2048 | 滑动/方向键合并；分数与最佳分；标准合并算法（单测覆盖） |
| 猜数字 | Guess | 1–100 小游戏 |
| 24 点 | Make 24 | 四张牌凑 24 |
| 画板涂鸦 | Draw | 触摸随手画 |
| MBTI | MBTI | **4 套题库 × 每套 52 题**；四选一 **2×2**；每次随机一套；E/I·S/N·T/F·J/P 各 13 题（奇数无平局）；结果含类型码与四维百分比 |

**English:** 2048 with proper merge logic; guess number; 24-game; doodle pad; MBTI with **4 banks × 52 items**, 4-choice 2×2 layout, random bank each run, four-dimension percentages.

#### 设置 / Settings

| 功能 | English | 说明 |
|------|---------|------|
| WiFi 扫描 | WiFi scan | 列出周边 AP 与信号 |
| 手动连接 | Manual connect | 输入 SSID / 密码连接 |
| 自动连接 | Auto connect | 开机尝试连接已配置热点 |
| 连接状态 | Status | 显示 SSID / IP / 进度 |

**English:** Scan APs, manual SSID/password connect, auto-connect at boot, status display.

---

### 2.5 吉祥物 / Mascot

| 能力 | English | 说明 |
|------|---------|------|
| 几何小脸 | Geometric face | 白色圆形脸，眨眼动画 |
| 多表情 | Expressions | 专注 / 开心 / 思考 / 爱心 / 困倦等 |
| 可点击 | Tappable | 聊天页点击有回应 |
| 页面复用 | Shared | 专注页与聊天页共用同一吉祥物 |

**English:** Shared blinking geometric mascot across Focus and Chat, multiple expressions, tappable in chat.

---

## 3. 系统能力 / openvela Integration

| 方向 | English | 落地情况 |
|------|---------|----------|
| 图形 | Graphics | **是** — LVGL + NuttX；SPI LCD / 触摸；完整 UI 与吉祥物 |
| AI | AI | **是** — 网络栈 + MiMo HTTP 对话；离线降级；预留 ASR/TTS |
| 多媒体 | Multimedia | **部分** — 音频栈调研完成；PTT UI 就绪，麦克风依赖板级配置 |

**English:** Graphics fully on LVGL; AI via MiMo HTTP + offline fallback; multimedia partially integrated (PTT UI ready).

---

## 4. 自定义 Skill / Custom Skill

| 项 | 内容 |
|----|------|
| 路径 Path | `/data/agent/skills/desk-companion/SKILL.md` |
| 名称 Name | `desk-companion`（桌边陪伴 / desk companion） |
| 触发 Trigger | 「陪我聊会儿」「有点累」「现在做什么好」；结合专注/喝水/吃药状态给建议 |
| 策略 Policy | 离线可用本地规则；健康仅提醒、不作医疗判断 |

**English:** Runtime skill `desk-companion` for companionship prompts; reads focus/med hints; offline-safe; no medical claims.

---

## 5. 构建与运行 / Build & Run

```bash
# OpenVela workspace
cd /path/to/openvela
export PATH=$PWD/prebuilts/gcc/linux-x86_64/arm-none-eabi/bin:$PATH

./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ -j8

# Check version / 校验版本
strings nuttx/vela.bin | grep Deskmate
# expect / 期望: Deskmate 0.13.2

# Copy into pack tree / 同步打包输入
cp -f nuttx/vela.bin \
  vendor/allwinnertech/lichee/board/r528s3/gemini-s1_nand/configs/nsh.fex

# Pack PhoenixSuit image / 打包镜像
cd vendor/allwinnertech/lichee
source envsetup.sh
lunch_nuttx    # choose 2 → r528s3-gemini-s1
pack
```

镜像 / Image:

```text
.../lichee/out/r528s3/gemini-s1_nand/rtos_nuttx_r528s3-gemini-s1_uart0_128Mnand.img
```

烧录 / Flash: PhoenixSuit 选镜像 → 板子下电 → 点烧录 → 上电触发。

**English:** Build `nsh_minidisplay`, verify `Deskmate 0.13.2`, copy `nsh.fex`, `lunch_nuttx` + `pack`, flash with PhoenixSuit.

---

## 6. 目录结构 / Source Layout

```text
app/deskmate/          # 本作品应用 / this app
├── deskmate_main.c    # 入口 LVGL bring-up
├── deskmate.h         # 版本、页面 ID、颜色与 API
├── dm_state.c         # 页面调度 / Dock / tick
├── dm_face.c          # 吉祥物 / mascot
├── dm_ui_focus.c      # 专注 / Focus
├── dm_ui_chat.c       # 聊天 UI / Chat UI
├── dm_chat_ai.c       # MiMo + 离线 AI / cloud & offline AI
├── dm_ui_health.c     # 健康 / Health
├── dm_ui_note.c       # 备忘 / Notes
├── dm_ui_med.c        # 吃药 / Meds
├── dm_ui_word.c       # 背单词 / Word memo
├── dm_ui_2048.c       # 2048
├── dm_ui_mbti.c       # MBTI 4×52
├── dm_ui_tools.c      # 计算器/传感/猜数字/换算等
├── dm_ui_life.c       # 喝水/倒数日/吃什么等
├── dm_ui_fun.c        # 24点/画板/BMI 等
├── dm_ui_wifi.c       # WiFi UI
├── dm_wifi.c          # WiFi 连接逻辑
├── dm_pinyin.c        # 拼音输入 / pinyin IME
└── dm_ui_shells.c     # Dock 与功能列表 / dock & list
```

---

## 7. 离线与降级 / Offline & Degradation

| 场景 | English | 行为 |
|------|---------|------|
| 无 WiFi | No WiFi | 专注/备忘/吃药/游戏/MBTI **全部可用**；聊天走本地回复 |
| 请求失败/超时 | Fail/timeout | 自动回退本地回复（约 20s 上限） |
| WiFi 驱动失败 | Driver fail | 应用不崩溃，等待/降级；手动 WiFi UI 保留 |

**English:** All local features work offline; chat falls back to local replies; WiFi driver failure does not crash the app.

---

## 8. 说明 / Notes

- 公开仓库中 `dm_chat_ai.c` 的 API Key 为占位符 `YOUR_MIMO_API_KEY`，真机密钥由设备侧配置。  
  In the public repo the API key is a placeholder; configure it on-device.
- 无线：RTL8733 SDIO 在部分硬件上可能出现 `sdio_probe` 失败，软件已做探测增强与业务降级。  
  RTL8733 SDIO probe may fail on some hardware; software adds bringup logs and graceful degradation.

---

© openvela AI Contest 2026 · Team 037 TensorFishSpecialActivityDivison
