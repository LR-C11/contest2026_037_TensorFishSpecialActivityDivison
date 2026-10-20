---
feature: deskmate-focus
status: in-progress
updated: 2026-09-13
branch: wsl-apps/deskmate
commits: 
---

# Deskmate 专注主页与倒计时互动

## Report

## [S1] Problem
Gemini-S1 桌搭需要一个以「专注」为默认主页的本地伴侣应用：设定时长后淡入倒计时页；倒计时页有会眨眼的表情、星星/爱心眼、睁眼注视，点击可文案互动；下方 MM:SS 数值变化要有滚轮感。聊天/健康/监督/备忘本回合只做可导航骨架，不实现完整业务。

## [S2] Design

### 落点
- 新应用目录：`vendor/allwinnertech/apps/deskmate/`（全新源码，不复用参赛仓 desk_companion）
- 构建接入：应用内 `Kconfig`/`Make.defs`/`Makefile`/`CMakeLists.txt`；`nsh_minidisplay/defconfig` 打开 `CONFIG_DESKMATE_APP=y`
- 不改动驱动、板级、luncher_mini 源码；启动时 `killall luncher_mini` 避免抢屏

### 页面模型
| 页面 | 角色 |
|------|------|
| `PAGE_FOCUS_HOME` | 默认主页：时长预设/自选 + 开始 |
| `PAGE_FOCUS_RUN` | 倒计时：表情 + 字幕 + 滚轮 MM:SS |
| `PAGE_CHAT` | 骨架 |
| `PAGE_HEALTH` | 骨架 |
| `PAGE_SUPERVISE` | 骨架 |
| `PAGE_NOTE` | 骨架 |

底栏：专注 / 聊天 / 健康 / 监督 / 备忘。倒计时页隐藏底栏，提供返回/结束。

### 专注流程
1. 主页显示几何白脸（眨眼、轻注视）+ 预设 `15/25/45/60/自选`
2. 点「开始专注」→ 主页淡出，倒计时页淡入（约 320ms）
3. 倒计时页：
   - 表情：自动眨眼；随机注视；周期性「星星眼」「爱心眼」「睁大看」
   - 点击表情 → 切换鼓励文案（字幕）
   - 表情与字幕下方：`MM:SS` 滚轮倒计时
   - 提供 暂停/继续、结束返回
4. 归零 → 完成表情 + 完成文案

### 滚轮数字
- 四位独立数字窗 + 冒号
- 每窗裁剪高度 ≈ 字高；变化时旧字上移出窗、新字从下入窗（`lv_anim`，约 160ms）
- 只滚动发生变化的位

### 表情系统
- 白色圆脸 + 黑色几何眼/嘴（不用 Unicode 表情字形）
- `dm_face_tick`：眨眼状态机（开/半/闭）、idle 注视偏移
- 眼饰：`FACE_EYE_NONE | STAR | HEART | WIDE`，用小几何形叠加

### 视觉
- 底 `#000000`，脸 `#FFFFFF`，眼嘴 `#000000`，弱化字 `#666666`/`#888888`
- 字体：优先 FreeType `/data/font/MiSans-Regular.ttf`，否则 Montserrat 内置

### 接口（模块）
- `deskmate.h`：颜色、页面、表情、全局 ctx、API
- `dm_state.c`：字体、`dm_show()`、`dm_say()`、tick 路由
- `dm_face.c`：脸控件 + 眨眼/眼饰
- `dm_roll.c`：`dm_clock_*` 滚轮
- `dm_ui_focus.c`：主页 + 倒计时
- `dm_ui_*.c`：骨架页

## [S3] Out of Scope
- 真实 LLM 闲聊、摄像头/健康算法、监督打断策略、备忘持久化
- WiFi/蓝牙设置页
- 参赛仓 desk_companion 源码复用
- 非 nsh_minidisplay 板型

## Tasks
- [ ] T1: 创建 deskmate 骨架与构建文件 — acceptance: 目录含 Makefile/Make.defs/Kconfig/CMakeLists 与空实现可编译结构 (covers: S2)
- [ ] T2: 实现专注主页+倒计时（表情/滚轮/互动）— acceptance: 代码支持开始淡入、眨眼、星心眼、点击文案、MM:SS 滚轮 (covers: S2)
- [ ] T3: 实现多能力骨架页 — acceptance: chat/health/supervise/note 可导航占位 (covers: S2)
- [ ] T4: defconfig 接入并编译 — acceptance: `nsh_minidisplay` 能编出 deskmate 或确认阻塞原因 (covers: S2)
