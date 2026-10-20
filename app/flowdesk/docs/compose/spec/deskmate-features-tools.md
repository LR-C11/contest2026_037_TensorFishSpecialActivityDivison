---
feature: deskmate-features-tools
status: in-progress
updated: 2026-09-16
branch: wsl-apps/deskmate
commits:
---

# Deskmate 功能页扩展工具集

## Report

## [S1] Problem
功能页需要一批可即开即用的小工具，每项**独立页面**（独立 `PAGE_*`）。第一批：计算器/传感/猜数/换算/喝水/倒数日。第二批追加：随机决定、24 点、画板涂鸦、秒表、BMI。

## [S2] Design

### 落点
- 代码目录：`vendor/allwinnertech/apps/deskmate/`（WSL `~/vela`）
- 新增源文件：
  - `dm_ui_tools.c` — 计算器 / 传感器 / 猜数字 / 单位换算
  - `dm_ui_life.c` — 喝水提醒 / 倒数日（含文件持久化）
- 修改：`deskmate.h`（页枚举、API）、`dm_ui_shells.c`（功能列表扩容、dock 归属）、`deskmate_main.c`（create 调用）、`Makefile`/`CMakeLists.txt`
- 不改动 WiFi / 单词 / 专注 / 健康既有逻辑

### 页面模型（均隐藏底栏，左上角 ← 返回功能页）

| 页面 | 功能 | 持久化 |
|------|------|--------|
| `PAGE_CALC` | 四则运算计算器 | 无 |
| `PAGE_SENSORS` | 温湿度 + 光照 | 无 |
| `PAGE_GUESS` | 1–100 猜数字 | 无 |
| `PAGE_CONVERT` | 长度/重量/温度换算 | 无 |
| `PAGE_WATER` | 喝水打卡与间隔提醒 | `/data/deskmate_water.txt` |
| `PAGE_COUNTDOWN` | 倒数日列表 | `/data/deskmate_countdown.txt` |
| `PAGE_EAT` | 吃什么（随机选餐） | 无 |
| `PAGE_24` | 24 点 | 无 |
| `PAGE_DRAW` | 画板涂鸦 | 无 |
| `PAGE_BMI` | BMI 计算 | 无 |

功能页滚动列表顺序：监督 → 备忘 → 背单词 → 计算器 → 环境传感 → 猜数字 → 单位换算 → 喝水提醒 → 倒数日 → 吃什么 → 24点 → 画板 → BMI。

### 第二批契约

**吃什么** `PAGE_EAT`
- 4 组×8 道菜名轮换；「开始」80ms 轮换，再点停住；「换一组」

**24 点** `PAGE_24`
- 发 4 个 1–13；「提示」穷举算式或「无解」；「下一局」重发

**画板** `PAGE_DRAW`
- 300×160 canvas；清屏；线宽 细/中/粗；强调色

**BMI** `PAGE_BMI`
- 身高 100–220、体重 20–200 ±1；标签 偏瘦/正常/偏胖/肥胖

第二批源文件：`dm_ui_fun.c`。版本 0.6.1。

### 各页契约

**计算器**
- 显示区：当前表达式（小字）+ 结果（大字右对齐）
- 键盘严格 4 列：
  `C  ⌫  ±  ÷`
  `7  8  9  ×`
  `4  5  6  −`
  `1  2  3  +`
  `0（占两格）  .  =`
- 无括号；连续运算符替换前一个；`=` 求值并冻结显示；`C` 清零；`⌫` 删一位；`±` 变号
- 除零显示 `ERR`；结果用最多 10 位有效数字

**环境传感**
- 三卡：温度 °C、湿度 %、光照 lux
- 每 2s 尝试读一次；驱动不可用时卡片显示「未检测到」（灰字），不伪造数据
- 底部「刷新」按钮手动触发一次

**猜数字**
- 每局随机 1–100；显示当前猜测值、次数、提示（太大/太小/猜对了）
- 增减按钮 ±1 / ±10，中间「猜」提交
- 猜中后吉祥物文案 + 「再来一局」

**单位换算**
- 顶部三段：长度 / 重量 / 温度
- 上下两行数值 + 可点单位名循环切换
- 长度：mm/cm/m/km/in/ft；重量：g/kg/lb/oz；温度：°C/°F/K
- 改一侧实时算另一侧；提供交换按钮

**喝水提醒**
- 今日进度 `n/目标`（默认目标 8 杯）
- 间隔 chips：30 / 45 / 60 / 90 分钟
- 倒计时到下次提醒（到点文案）；用绝对到期时间戳，离开页面仍按墙钟走
- 「喝了一杯」+1 并重置间隔；「今日清零」
- 保存：`cups interval due_epoch day` 一行文本，跨重启恢复（按日重置）

**倒数日**
- 列表最多 8 条：事件名 + 剩余天数（大数字）
- 添加：短名（预设：生日/考试/旅行/纪念日/自定义编号）+ 年月日步进器
- 删除当前选中；过期显示「已过」
- 文件格式每行：`YYYY-MM-DD name`

### Dock 归属
上述 6 页在 `dm_dock_highlight` 中归到 `PAGE_FEATURES`；进入子页时隐藏 dock。

### 视觉
- 延续黑底 `#000`、白字、弱化 `#666/#888`、圆角卡片 `#111`
- 强调色：`C_ACCENT #7DD3FC`（传感器/换算）、`C_OK #6BCB77`（成功）、`C_STAR #FFD54F`（提醒）

## [S3] Out of Scope
- 真实 LLM、摄像头、复杂日历编辑
- 计算器科学函数 / 括号
- 多目标喝水、多用户
- 传感器驱动本身的移植（只读已有或显示不可用）

## Tasks
- [ ] T1: 扩展 `deskmate.h` 页面枚举与 API — acceptance: 新 PAGE_* 与 create/tick 声明齐全 (covers: S2)
- [ ] T2: 实现 `dm_ui_tools.c`（计算器/传感/猜数/换算）— acceptance: 四页可导航且核心逻辑正确 (covers: S2)
- [ ] T3: 实现 `dm_ui_life.c`（喝水/倒数日+持久化）— acceptance: 打卡与日期可保存重载 (covers: S2)
- [ ] T4: 功能列表扩容 + dock 归属 + main 创建 — acceptance: 功能页九项可点进各自独立页 (covers: S2)
- [ ] T5: HTML 交互预览 — acceptance: 320×240 模拟器可点通 6 页 (covers: S2)
- [ ] T6: 编译与板端冒烟 — acceptance: 编译通过；日志 Deskmate 版本号更新 (covers: S2)
