# Deskmate — Gemini-S1 桌搭伴侣（专注主页）

全新应用，**未复用**参赛仓 `desk_companion` 源码。

## 功能

- **主页 = 专注设定**：15/25/45/60/自选时长，点「开始专注」
- **淡入倒计时页**：约 320ms 页面淡入
- **表情**：自动眨眼、注视；周期性星星眼 / 爱心眼 / 睁大看
- **点脸互动**：切换鼓励文案 + 轻微缩放脉冲
- **滚轮倒计时**：`MM:SS` 逐位上下滚动（只滚变化的数字）
- **骨架页**：聊天 / 健康 / 监督 / 备忘（可导航占位）
- **底栏**：专注 / 聊天 / 健康 / 监督 / 备忘（倒计时页隐藏）

## 目录

```
vendor/allwinnertech/apps/deskmate/
  deskmate.h
  deskmate_main.c
  dm_state.c
  dm_face.c
  dm_roll.c
  dm_ui_focus.c
  dm_ui_shells.c
  Makefile / Make.defs / Kconfig / CMakeLists.txt
```

## 编译（WSL）

```bash
cd /home/lrc/vela

# 若 defconfig 已含 CONFIG_DESKMATE_APP=y，直接编：
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ -j8

# 若刚改过 Kconfig/多文件，建议 distclean 后再编：
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ distclean -j8
./build.sh vendor/allwinnertech/boards/r528/r528s3-gemini-s1/configs/nsh_minidisplay/ -j8
```

打包（记得手动拷贝，否则可能是旧固件）：

```bash
cp -f nuttx/vela.bin vendor/allwinnertech/lichee/board/r528s3/gemini-s1_nand/configs/nsh.fex
cd vendor/allwinnertech/lichee && source envsetup.sh
lunch_nuttx   # 选 2: r528s3-gemini-s1
pack
```

## 运行

```bash
killall luncher_mini
deskmate &
```

日志应出现：`Deskmate 0.1.0 focus-home`

## 配置

`nsh_minidisplay/defconfig` 已加入：

```
CONFIG_DESKMATE_APP=y
```

栈默认 64KB。中文优先 FreeType MiSans（`/data/font` 或 `/resource/fonts`），否则 Montserrat 内置字体（中文可能缺字，数字正常）。

## 说明

- 启动时会 `killall luncher_mini`，避免双 LVGL 抢屏
- 未改驱动 / 板级 / luncher 源码；仅新增本目录 + defconfig 一行
