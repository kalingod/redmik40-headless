# Project Overview

## 最终目标

把 Redmi K40 / POCO F3 (`alioth`) 逐步做成完整 Linux 手机设备：

- 可启动完整 Ubuntu 或 CentOS Stream / RHEL-family 用户态。
- 支持日常开发入口：USB 网络、SSH、稳定日志、可回 fastboot。
- 支持常用服务器能力：软件包管理、Docker/容器、Web/数据库服务、持久化数据盘。
- 逐步补齐手机硬件：显示、触摸、按键、背光/熄屏、电池/充电、热管理、Wi-Fi、蓝牙、音频、传感器、摄像头、振动、手电。
- 长期探索基带/蜂窝、GNSS、IMS/VoLTE 等更深的 Android vendor 依赖能力。

当前阶段的核心不是一步到位刷入 Ubuntu，而是建立一个可回滚、可复现、可调试的 downstream Linux 基线。

## 设备事实

| 项目 | 当前结论 |
|---|---|
| 设备 | Redmi K40 / POCO F3 |
| 型号 | M2012K11AC |
| codename | `alioth` |
| SoC | Qualcomm Snapdragon 870 / SM8250-AC |
| Android 分区模型 | Virtual A/B |
| boot 状态 | OEM unlocked，AVB 存在 |
| 当前活动槽 | `_b` |
| 当前固件基线 | Android 12 / MIUI `V13.0.7.0.SKHCNXM` / CN |
| 当前 Linux root | `/dev/block/by-name/userdata[/rootfs/ubuntu-24.04]` |
| 当前数据盘 | `/dev/block/by-name/userdata`，其中 `/rootfs/ubuntu-24.04` 是当前默认 rootfs |

固件基线的证据来自 slot `_b` 动态分区 `build.prop` 只读解析，以及本地 OTA 下载日志。不要把 Android 13 或其它区域包里的 `boot/vendor_boot/dtbo/vendor/odm/firmware` 和当前 Android 12 CN 环境混用。

## 当前架构

当前可用系统链路：

```text
normal boot from boot_b, or fastboot boot for experiments
  -> Android boot v3 image
  -> Lineage 20 downstream kernel 4.19.312
  -> custom BusyBox initramfs
  -> USB NCM gadget + DHCP server
  -> C DRM/KMS status UI
  -> switch_root 到 Ubuntu 24.04 rootfs
  -> Ubuntu systemd PID1 + SSH + service helpers
```

这条链路已经验证：

- 自编译 Lineage 20 内核可启动。
- 自制 initramfs 不依赖 OrangeFox userspace。
- USB NCM 使用 `172.16.42.2/24`，手机侧 DHCP 给主机分配 `172.16.42.1/24`。
- OpenSSH 可登录 `root@172.16.42.2`。
- DRM/KMS 状态 UI 能点亮 1080x2400 屏幕并显示系统状态。
- Ubuntu 24.04 rootfs 能接管 `/`，systemd 255 可作为 PID1。
- `systemctl is-system-running=running`，0 failed units。
- 旧 Arch/LELE OS 环境中 Docker、nginx、Valkey、PostgreSQL、Docker Compose 已验证；这些服务尚未迁移成 Ubuntu 当前主线服务。
- `userdata` 当前承载 Ubuntu rootfs，并保留后续数据盘/服务目录用途。

## 和 x86 原生 Ubuntu 裸机安装的区别

当前 K40 状态不是 PC 上常见的“Ubuntu 完整裸机安装”。x86 裸机通常是：

```text
UEFI/BIOS -> GRUB/shim -> Ubuntu generic kernel/initramfs -> Ubuntu rootfs/systemd
```

这意味着 kernel、modules、initramfs、bootloader 配置、驱动更新和用户态大多由 Ubuntu 包管理体系统一维护。

当前 K40 是：

```text
Android bootloader/fastboot -> Android boot image -> Lineage/Android downstream 4.19 kernel -> custom initramfs -> Ubuntu 24.04 rootfs/systemd
```

所以 Ubuntu 目前负责的是 `DISTRO` / userspace / systemd / apt 这一层；屏幕、UFS、USB gadget、电源、电池、输入和大量高通硬件仍依赖 Lineage/Android downstream kernel 及现有 firmware/vendor 环境。文档和监控 UI 必须把 `DISTRO` 和 `KERNEL` 分开显示，避免误解成已经切到 Ubuntu generic kernel。

## 为什么不直接刷完整 Ubuntu/CentOS

alioth 不是 PC。启动链和硬件描述由 `boot`、`vendor_boot`、`dtbo`、`vbmeta`、`super/vendor/odm`、firmware 和 vendor userspace 共同决定。单独替换一个“Ubuntu boot 镜像”通常会在显示、存储、Wi-Fi、基带或 early userspace 阶段失败。

当前工程策略是：

1. 保留现有 firmware / vendor / dtbo 环境。
2. 先用已证实可启动的 downstream kernel。
3. 用最小 initramfs 建立救援入口。
4. 再切换 rootfs 和用户态。
5. 最后逐项替换 init、服务管理、硬件适配和发行版体验。

## 当前主线选择

已停止作为主路径：

- MiCode `alioth-r-oss` GPL dump。它缺少完整 vendor DTS/DTBO，多种打包格式和 GCC 9/13 组合均黑屏。
- Mu-Silicium UEFI。它可作为长期探索方向，但不是当前最短稳定路径。
- 直接 systemd PID1。Arch systemd 260 在当前 Android 4.19 downstream kernel 上早期 mount-point 检测冻结。

当前主路径：

- `xiaomi-sm8250-devs/android_kernel_xiaomi_sm8250` `lineage-20`
- 自制 initramfs
- `switch_root` 到 Ubuntu 24.04 rootfs
- Ubuntu systemd 255 作为当前默认 PID1；initramfs helper 保留 USB/2323/status UI/回 fastboot 能力

## 源码工作区

`/vmdata/android/redmik40-lineageos/src` 是完整 LineageOS 工作区，K40 相关路径优先看：

```text
device/xiaomi/alioth
device/xiaomi/sm8250-common
kernel/xiaomi/sm8250
vendor/xiaomi/alioth
vendor/xiaomi/sm8250-common
hardware/xiaomi
```

当前可启动 boot image 的构建仍使用 `/vmdata/android/redmik40/lineage-sm8250` 下的轻量化源码镜像和产物目录。完整 LineageOS 工作区作为后续 Android vendor、硬件配置、blobs、系统镜像构建参考，不要和当前已验证 boot baseline 混淆。

## 工程原则

- 每次只改一个关键变量。
- 所有候选先用 `fastboot boot` 临时启动。
- 任何刷写分区前必须有回滚镜像、恢复路径和验证记录。
- 文档必须区分“已验证事实”“推断”“历史失败路线”。
- 当前可用基线优先于理想架构。
