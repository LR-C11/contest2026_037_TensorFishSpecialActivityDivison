# Gemini-S1 SDK 补丁

FlowDesk 应用代码位于 app/flowdesk。以下补丁用于复现本机已验证的 SDK 修改；不包含整套 SDK、固件镜像或用户数据。

- lvgl-lcd-aligned-buffers.patch：修复 /dev/lcd0 打开后因绘图缓冲区未满足64字节对齐而触发断言的问题；在 SDK 的 apps/graphics/lvgl/lvgl 仓库应用。
- sdk-flowdesk-startup.patch：启用 nsh 与 nsh_minidisplay 配置的 FlowDesk，并在 rcS.nsh 中优先启动 flowdesk，避免与 luncher_mini 同时启动；在 vendor/allwinnertech 仓库应用。
- sdk-pack-failure-checks.patch：检查 Dragon 返回状态和镜像是否实际生成，并修复缺少可选 hook 时的退出状态；在 vendor/allwinnertech 仓库应用。
- sdk-ntp-direct-query.patch：移除 NTP 采样前对固定 DNS 地址的 ICMP 检查，支持仅通过 DHCP 获取 DNS 的配置，并在采样之间响应停止请求；在 SDK 的 apps 仓库应用。只更新 FlowDesk 应用目录不足以修复这个 SDK 问题。

假设竞赛仓库绝对路径为 CONTEST_DIR，SDK 根目录为 /home/lrc/vela，可先用 git apply --check 检查，再应用：

```bash
cd /home/lrc/vela
# 将下面路径替换为本竞赛仓库实际位置
CONTEST_DIR=/absolute/path/to/contest2026_037_TensorFishSpecialActivityDivison

git -C apps/graphics/lvgl/lvgl apply --check "$CONTEST_DIR/app/flowdesk/docs/patches/lvgl-lcd-aligned-buffers.patch"
git -C apps/graphics/lvgl/lvgl apply "$CONTEST_DIR/app/flowdesk/docs/patches/lvgl-lcd-aligned-buffers.patch"

git -C vendor/allwinnertech apply --check "$CONTEST_DIR/app/flowdesk/docs/patches/sdk-flowdesk-startup.patch"
git -C vendor/allwinnertech apply "$CONTEST_DIR/app/flowdesk/docs/patches/sdk-flowdesk-startup.patch"

git -C vendor/allwinnertech apply --check "$CONTEST_DIR/app/flowdesk/docs/patches/sdk-pack-failure-checks.patch"
git -C vendor/allwinnertech apply "$CONTEST_DIR/app/flowdesk/docs/patches/sdk-pack-failure-checks.patch"

git -C apps apply --check "$CONTEST_DIR/app/flowdesk/docs/patches/sdk-ntp-direct-query.patch"
git -C apps apply "$CONTEST_DIR/app/flowdesk/docs/patches/sdk-ntp-direct-query.patch"
```

当前本机 SDK 已应用这些修改，不要重复应用。补丁以当前 SDK 版本为基准，其他版本若有冲突应核对源代码后处理。WSL 打包需要能运行 SDK 的32位 x86工具；本机已使用WSL2。

本轮应用源码已通过39组宿主回归测试、Gemini-S1完整固件编译和Dragon打包。AI权限、AI操作记录和合并的生活数据读取说明见同目录其他文档；仍需刷入开发板验证页面与交互。

2026-11-07 校时修复将应用版本提升至 0.19.1，累计44组宿主回归测试通过，包含实际 SDK NTP 采样代码的测试。本次按用户要求未编译、打包或刷入固件，由用户自行编译；前述完整固件编译记录属于校时修复之前的版本。详见《校时修复-2026-11-07.md》。
