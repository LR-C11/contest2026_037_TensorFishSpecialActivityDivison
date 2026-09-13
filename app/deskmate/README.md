# Deskmate — Gemini-S1 桌搭伴侣

源码对应板端路径：`vendor/allwinnertech/apps/deskmate/`  
（WSL 开发树中同名目录；本目录为参赛仓归档副本。）

## 功能（v0.2.1）

### 专注主页（默认首页）
- 时长预设 15 / 25 / 45 / 60 / 自选
- 「开始专注」→ 页面淡入倒计时
- 倒计时：表情（眨眼/星星眼/爱心眼/睁大看）+ 点击文案互动
- `MM:SS` 滚轮数字（仅变化位滚动）
- 真秒计时（250ms UI tick，4 次 = 1 秒）

### 健康页（可滑动）
- 情绪综合分 = **手动心情记录 + 今日专注时长** 加权
- 最近心情列表、环境/专注统计
- 「记录心情」半屏面板（可滑动）：
  - 场景：当前心情 / 今日整体
  - 心情标签多选（16）
  - 快捷描述（8，无自由输入）
  - 取消 / 保存

### 骨架页
- 聊天 / 监督 / 备忘：可导航占位

## 文件

| 文件 | 作用 |
|------|------|
| `deskmate_main.c` | 入口、LVGL 初始化 |
| `dm_state.c` | 字体、页面切换 |
| `dm_face.c` | 几何表情与互动文案 |
| `dm_roll.c` | 滚轮倒计时 |
| `dm_ui_focus.c` | 专注主页 + 倒计时 |
| `dm_ui_health.c` | 健康页 + 心情记录 |
| `dm_ui_shells.c` | 骨架页 + 底栏 |

## 编译打包（OpenVela / Gemini-S1）

```bash
cd /home/lrc/vela
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ -j8

cp -f nuttx/vela.bin \
  vendor/allwinnertech/lichee/board/r528s3/gemini-s1_nand/configs/nsh.fex

cd vendor/allwinnertech/lichee && source envsetup.sh
lunch_nuttx   # 2: r528s3-gemini-s1
pack
```

产物：`out/r528s3/gemini-s1_nand/rtos_nuttx_r528s3-gemini-s1_uart0_128Mnand.img`

## 配置

`nsh_minidisplay/defconfig`：

```
CONFIG_DESKMATE_APP=y
```

建议关闭 `CONFIG_LUNCHER_MINI_APP`，rcS 只启动 deskmate，避免抢 LCD。

中文：FreeType 读 `/data/font/MiSans-Regular.ttf`（UDISK 字体包）。

## 运行

```bash
deskmate &
```

日志：`Deskmate 0.2.1 focus-home`
