# Deskmate — Gemini-S1 桌搭伴侣

**Board path:** `vendor/allwinnertech/apps/deskmate/`  
This folder is the contest archive of that app.

## Version / 版本

**0.3.0**

---

## Features / 功能

### 专注主页 Focus home (default page)
- Duration presets 15 / 25 / 45 / 60 / custom
- 时长预设与自选
- Start → fade into countdown page
- 「开始专注」后淡入倒计时页
- Face: blink, star/heart/wide eyes, tap for dialogue lines
- 表情：眨眼、星星眼、爱心眼、睁大注视；点击切换文案
- Rolling `MM:SS` (only changed digits animate)
- `MM:SS` 滚轮倒计时
- Real-second timer (250ms UI tick × 4)
- 真秒计时，避免倒计时过快

### 健康页 Health (scrollable)
- Score = mood log + today’s focus minutes (weighted)
- 评分 = 心情记录 + 今日专注时长
- Recent mood list + focus stats
- 最近心情列表与专注统计
- Scrollable content area
- 内容区可上下滑动

### 记录心情（新页面）Mood log (full page)
- Tap「记录心情」→ dedicated page (no overlay)
- 点「记录心情」进入独立页面，不用遮罩浮层
- Scene: current mood / daily overall
- 场景：当前心情 / 今日整体
- 16 mood tags (multi-select) + 8 quick phrases
- 16 个心情标签多选 + 8 条快捷描述
- Save → write `/data/deskmate_mood.bin` → return to health and refresh
- 保存后写入本地存储，返回健康页并刷新
- Load from file on app start
- 启动时从文件读回记录

### Skeleton pages / 骨架页
- Chat / Supervise / Notes: navigable placeholders
- 聊天 / 监督 / 备忘：可导航占位

---

## Changelog / 更新日志

### 0.3.0
- Mood log is a **full page**, not a modal sheet
- 心情记录改为**独立全页**，不再使用半屏遮罩
- Persist moods + focus minutes to `/data/deskmate_mood.bin`
- 心情与专注分钟写入 `/data/deskmate_mood.bin`
- Reload data on startup; refresh health after save
- 启动加载；保存后刷新健康页
- Safer chip styling (null-checked labels)
- 按钮文案样式空指针保护

### 0.2.x
- Focus home + countdown + rolling digits
- 专注主页、倒计时、滚轮数字
- Health page + score blend
- 健康页与综合评分
- Touch/layout fixes for mood chips
- 心情按钮触摸与布局修复
- LCD draw-buffer alignment fix (platform)
- LCD 绘制缓冲对齐修复（平台层）
- Boot: deskmate only (no luncher_mini race)
- 开机只启动 deskmate，避免与 luncher 抢屏

---

## Build / 编译

```bash
cd /home/lrc/vela
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ -j8

cp -f nuttx/vela.bin \
  vendor/allwinnertech/lichee/board/r528s3/gemini-s1_nand/configs/nsh.fex

cd vendor/allwinnertech/lichee && source envsetup.sh
lunch_nuttx   # 2: r528s3-gemini-s1
pack
```

`CONFIG_DESKMATE_APP=y` in `nsh_minidisplay/defconfig`.  
建议关闭 `CONFIG_LUNCHER_MINI_APP`。

Log: `Deskmate 0.3.0 focus-home`
