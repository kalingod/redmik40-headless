# Runbook

这份文档只放可执行流程。背景和路线见 `project-overview.md`，当前状态见 `current-state.md`，当前主机可用资源见 `environment.md`。

执行任何会改变设备、boot image、rootfs、服务或网络状态的步骤前，先按 `experiment-protocol.md` 把计划写进 `boot-validation-log.md`；执行后再把实际结果补回去。

## 当前接手主机约定

当前接手环境是 macOS checkout：

```text
/Users/wuyuele/redmik40-headless
```

历史文档中的 `/vmdata/android/redmik40/...` 是旧 Linux 工作站/实验盘路径；当前主机未挂载 `/vmdata` 时，不要直接复制执行这些命令。

优先使用仓库内 platform-tools，避免 PATH 指到其它 adb/fastboot：

```bash
./tools/platform-tools/adb version
./tools/platform-tools/fastboot --version
```

当前仓库内可见的 control boot image：

```text
artifacts/control/lineage-mininitramfs-boot.img
sha256: 19e159007fb04e009f840b09dfb33b68808ac45618403c63e80f2851e7608a23
```

这个镜像当前只应视为本地 control artifact；在确认它和设备当前默认 boot 完全一致之前，不要把它写成“推荐默认镜像”。

## LineageOS 源码获取策略

当前阶段不需要默认全量 clone LineageOS，也不继续维护旧 Linux 工作站上的大源码树作为前置条件。这个仓库的主线目标是维护已验证 boot/rootfs/服务/状态面板链路；日常 runbook、脚本、systemd unit、状态 UI 调整不需要完整 Android tree。

优先级建议：

1. 不下载源码：只整理文档、设备只读探测、SSH/systemd/Wi-Fi/audio/status UI 运维。
2. 最小源码参考：需要查 alioth/sm8250 设备树、kernel config、DTS、Wi-Fi/audio/blobs 路径、boot 打包参数时，只拉 kernel + device tree + common device tree。
3. 远端缓存编译：如果需要高算力，优先请求 36 核机器在远端保留完整源码和构建缓存，本机只传 manifest、patch、配置和小脚本。
4. 完整 LineageOS repo：只有在必须本地复现完整 Android/Lineage build、重新抽取/匹配 vendor blobs、构建完整 system/vendor 相关产物、或系统性比对 HAL/sepolicy/rootdir 时才需要。

最小参考源码建议放在仓库外，避免把大型源码混进这个控制仓库：

```text
/Users/wuyuele/lineage-src/alioth-minimal
```

文档中已验证 baseline 对应的轻量源码集合是：

```text
kernel:        xiaomi-sm8250-devs/android_kernel_xiaomi_sm8250, lineage-20
device alioth: xiaomi-sm8250-devs/android_device_xiaomi_alioth, lineage-20
device common: xiaomi-sm8250-devs/android_device_xiaomi_sm8250-common, lineage-20
```

如果只是查代码，使用 shallow clone 即可。执行前先确认网络和远端分支仍存在：

```bash
mkdir -p /Users/wuyuele/lineage-src/alioth-minimal
cd /Users/wuyuele/lineage-src/alioth-minimal

git ls-remote --heads https://github.com/xiaomi-sm8250-devs/android_kernel_xiaomi_sm8250 lineage-20
git ls-remote --heads https://github.com/xiaomi-sm8250-devs/android_device_xiaomi_alioth lineage-20
git ls-remote --heads https://github.com/xiaomi-sm8250-devs/android_device_xiaomi_sm8250-common lineage-20

git clone --depth=1 --branch lineage-20 https://github.com/xiaomi-sm8250-devs/android_kernel_xiaomi_sm8250 kernel_xiaomi_sm8250
git clone --depth=1 --branch lineage-20 https://github.com/xiaomi-sm8250-devs/android_device_xiaomi_alioth device_xiaomi_alioth
git clone --depth=1 --branch lineage-20 https://github.com/xiaomi-sm8250-devs/android_device_xiaomi_sm8250-common device_xiaomi_sm8250-common
```

不要把完整 `repo sync` 当成前置步骤。全量 LineageOS tree 通常体积大、耗时长、权限和分支状态复杂；当前文档已指出旧完整工作区在 `/vmdata/android/redmik40-lineageos/src`，但这个路径当前主机不可见，也不再作为当前接手流程的依赖。

公网带宽约 5 MB/s 时，不要把“本机下载全量源码再上传远端编译”作为默认流程。远端编译应遵守：

```text
源码和 ccache/sstate/build output 留在远端
本机只上传 manifest、local_manifests、patch、defconfig、脚本
远端只回传 boot.img、Image、dtb/dtbo、modules、config、日志摘要、SHA256
大日志先压缩，能 grep 摘要就不拉全量
不要反复同步 out/、.repo/、prebuilts/、vendor 大目录
```

如果需要让 36 核机器参与，建议先准备一个很小的 build request：

```markdown
目标：
源码分支/commit：
需要应用的 patch：
构建命令：
期望产物：
回传文件清单：
验证方式：
```

需要全量 clone 的触发条件：

```text
需要 lunch/brunch 完整构建 LineageOS
需要重新生成 vendor/system 相关产物
需要系统性比对 Android rootdir/init rc/HAL/sepolicy
需要重新抽取 proprietary blobs 并验证版权/版本匹配
需要把当前 downstream boot baseline 从源码完整复现出来
```

不需要全量 clone 的任务：

```text
整理 runbook/current-state/resources
验证 USB NCM/SSH/systemd 状态
调整手机侧 shell helper 或 systemd unit
改 GPU status UI 的 C helper
做 fastboot boot 临时验证已有 boot image
查单个 DTS/kernel config/BoardConfig 片段
```

可以清理或降级为历史引用的内容：

```text
旧 Linux 工作站上的完整源码路径
不可见的 /vmdata 源码树
没有对应本地 artifact 的历史 out/ 目录
只为一次性实验保留、且已被文档结论吸收的大型源码副本
```

## 检查设备是否在线

```bash
ping -c 1 172.16.42.2
./tools/platform-tools/adb devices
./tools/platform-tools/fastboot devices
ip -br addr
```

如果三者都看不到设备，先确认 USB 连接、手机是否在 fastboot、是否已经启动到当前 Linux 镜像。

当前接手探测发现 `22/tcp` 和 `2323/tcp` 均开放，但 SSH 缺少可用认证；`2323/tcp` 是可用 root 备用 shell。只读探测可用：

```bash
nc -G 2 -w 5 172.16.42.2 2323
```

当前 live 状态不是 Ubuntu systemd 默认态，而是 Arch Linux ARM + `alioth-switch-init switchroot-shell`，Ubuntu 24.04 rootfs 仅挂载在 `/data/rootfs/ubuntu-24.04`。在恢复或确认 Ubuntu PID1 前，不要直接执行依赖 `systemctl` 管理手机主系统的步骤。

## 目标运行模式：Ubuntu headless + 本机 Panel

目标不是彻底不用屏幕，而是不跑完整手机/桌面 UI：

```text
Ubuntu 24.04 systemd 作为 PID1
SSH / Wi-Fi / 时间同步 / audio ADSP 等作为 systemd service
屏幕作为固定状态 Panel
Panel 直接使用 DRM/KMS + KGSL/Vulkan
默认不启动 Wayland/X11/compositor
```

目标启动链：

```text
Android bootloader / fastboot
-> Android boot image
-> minimal BusyBox initramfs rescue
-> USB NCM + optional 2323 rescue shell
-> mount vendor/firmware/persist and Ubuntu rootfs
-> switch_root /data/rootfs/ubuntu-24.04 /sbin/init
-> Ubuntu systemd
-> alioth helper services
-> alioth panel service
```

Panel 应作为 Ubuntu systemd 服务管理，概念上类似：

```text
alioth-panel.service
-> alioth_gpu_status_ui_wrapper.sh
-> alioth_gpu_monitor_service
-> DRM/KMS pageflip
-> KGSL Vulkan glyph/text renderer
```

Panel 显示范围：

```text
USB/Wi-Fi IP
SSH 状态
电池/充电
温度
负载/内存/磁盘
systemd failed units
关键服务状态
可选菜单：reboot / fastboot / brightness / Wi-Fi
```

## 预实验：定位为什么没有进入 Ubuntu PID1

这个阶段只读，不改设备、不重启、不刷写。

从当前 2323 root shell 读取 switchroot 逻辑：

```bash
{
  printf 'echo SWITCHROOT_READ_START\n'
  printf 'sed -n "1,260p" /var/tmp/alioth-switchroot/alioth-switch-init\n'
  printf 'find /var/tmp/alioth-switchroot -maxdepth 2 -type f -o -type l | sed -n "1,120p"\n'
  printf 'find /data/rootfs/ubuntu-24.04 -maxdepth 1 -type f -o -type l -o -type d | sed -n "1,80p"\n'
  printf 'cat /proc/cmdline\n'
  printf 'mount | sed -n "1,80p"\n'
  printf 'echo SWITCHROOT_READ_END\n'
  printf 'exit\n'
} | nc -G 2 -w 5 172.16.42.2 2323
```

需要回答：

```text
当前 initramfs 是根据什么选择 Arch rootfs？
是否存在 Ubuntu switch_root 分支？
Ubuntu 分支是否失败后回落到 shell？
失败信号写在哪里？
需要修改 boot image、initramfs 脚本，还是只需要改启动参数/标记文件？
```

在这些问题回答前，不做 `fastboot flash`，也不在 live root 上改 `/var/tmp/alioth-switchroot`。

## 实验准备：Ubuntu-systemd-first boot profile

预实验完成后，准备一个只通过 `fastboot boot` 验证的候选镜像。候选目标：

```text
保留 USB NCM rescue
保留 2323 rescue 或等价 fallback
默认 switch_root 到 /data/rootfs/ubuntu-24.04 /sbin/init
Ubuntu systemd 成为 PID1
sshd 可登录
panel service 可启动或明确 disabled 等待手动启动
失败时保留可恢复路径
```

候选镜像验证顺序：

```text
fastboot boot candidate.img
ping 172.16.42.2
SSH 或 2323 进入
cat /etc/os-release
cat /proc/1/comm
systemctl is-system-running
systemctl --failed --no-pager
ip -br addr
检查 panel service 是否按预期运行或处于 disabled/manual 状态
```

成功标准：

```text
PID1 是 systemd
rootfs 是 Ubuntu 24.04
USB rescue path 可用
没有不可解释的 failed units
不依赖 Arch rootfs 承担主系统
屏幕 panel 路径不需要桌面栈
```

失败时不刷写，直接长按/fastboot 回退到已知镜像。

## 实验 1：复现已部署 boot_b/control 镜像

目的：

```text
确认仓库本地 control boot image 是否能复现之前记录的 Ubuntu systemd + Panel 路径。
```

已知证据：

```text
当前 live slot: _a
当前 live root: Arch Linux ARM on /dev/block/by-name/arch
当前 live PID1: alioth-switch-init switchroot-shell
boot_a prefix hash over 48,373,760 bytes: b0b7e7bf44803150fb536a7ba45de488f1fad2f8068014899abc23b4916f275b
boot_b prefix hash over 48,373,760 bytes: 19e159007fb04e009f840b09dfb33b68808ac45618403c63e80f2851e7608a23
local control image sha256: 19e159007fb04e009f840b09dfb33b68808ac45618403c63e80f2851e7608a23
```

结论：

```text
artifacts/control/lineage-mininitramfs-boot.img == phone boot_b image prefix
artifacts/control/lineage-mininitramfs-boot.img != current live boot_a image
```

实验方式：

```text
只用 fastboot boot 临时启动本地 control image。
不 flash。
不切 slot。
不改 boot_a/boot_b。
```

执行前记录：

```bash
./tools/platform-tools/fastboot devices
./tools/platform-tools/fastboot getvar current-slot
shasum -a 256 artifacts/control/lineage-mininitramfs-boot.img
```

临时启动：

```bash
./tools/platform-tools/fastboot boot artifacts/control/lineage-mininitramfs-boot.img
```

启动后验证：

```bash
ping -c 1 172.16.42.2
nc -z -G 1 172.16.42.2 22
nc -z -G 1 172.16.42.2 2323
```

如果 2323 可用，先只读确认：

```bash
{
  printf 'cat /etc/os-release 2>/dev/null | sed -n "1,8p"\n'
  printf 'cat /proc/1/comm 2>/dev/null\n'
  printf 'readlink /proc/1/exe 2>/dev/null || true\n'
  printf 'findmnt / 2>/dev/null || mount | sed -n "1,12p"\n'
  printf 'systemctl is-system-running 2>/dev/null || true\n'
  printf 'systemctl --failed --no-pager 2>/dev/null | sed -n "1,20p"\n'
  printf 'ip -br addr 2>/dev/null | sed -n "1,20p"\n'
  printf 'tail -80 /run/alioth-status-ui.log 2>/dev/null || true\n'
  printf 'tail -80 /run/alioth-status-ui-wrapper.log 2>/dev/null || true\n'
  printf 'exit\n'
} | nc -G 2 -w 8 172.16.42.2 2323
```

成功标准：

```text
rootfs 是 Ubuntu 24.04
PID1 是 systemd
systemctl 可用且不是 offline
USB 网络可达
SSH 或 2323 至少一个救援入口可用
Panel/status UI 日志存在，或服务状态明确可解释
```

如果仍进入 Arch：

```text
说明 boot_b/control 镜像也不是 Ubuntu-systemd-first，下一步需要改 initramfs switchroot 逻辑。
```

如果进入 Ubuntu 但 Panel 没起来：

```text
先保持系统不变，只读看 systemd failed units、panel wrapper 日志、DRM/KGSL 节点。
不要直接启动显示实验覆盖当前屏幕状态。
```

### 实验 1 实际结果

执行结果：

```text
fastboot current-slot: a
fastboot boot artifacts/control/lineage-mininitramfs-boot.img: OK
USB ping returned after about 5 seconds
tcp/2323 open
tcp/22 was initially closed, later sshd was listening
rootfs remained Arch Linux ARM
PID1 remained alioth-switch-init switchroot-shell
systemctl remained offline
Ubuntu 24.04 rootfs remained mounted at /data/rootfs/ubuntu-24.04, not /
CPU fallback status UI opened DRM/KMS 1080x2400
```

结论：

```text
实验 1 没有复现 Ubuntu systemd + Panel。
control/boot_b 镜像仍是 Arch-first switchroot-shell 路径。
下一步不应先改内核，而应先制作 Ubuntu-systemd-first initramfs/switchroot 候选。
```

## 实验 2：Ubuntu-systemd-first initramfs 候选

目的：

```text
修改或重建 boot image 的 initramfs 逻辑，让它默认 switch_root 到 /data/rootfs/ubuntu-24.04 /sbin/init。
```

实验 2 仍然只允许：

```text
制作候选 boot image
fastboot boot 候选镜像
启动后只读验证
```

实验 2 不允许：

```text
fastboot flash
set-active
修改 boot_a/boot_b
破坏当前 Arch rescue 路径
```

候选 initramfs 行为：

```text
启动 USB NCM rescue
启动 2323 rescue shell，至少在 Ubuntu SSH 验证前保留
创建 /dev、/proc、/sys、/run、/sys/fs/cgroup 必要挂载
创建 block/char 节点
挂载 Android vendor/system/firmware/persist 必要路径
确认 /data/rootfs/ubuntu-24.04/sbin/init 存在
bind-mount 必要的 /dev /proc /sys /run /vendor /system /odm /product /system_ext /firmware
exec switch_root /data/rootfs/ubuntu-24.04 /sbin/init
失败则留在 rescue shell，并把原因写入 /run/alioth-switchroot.log
```

候选成功标准：

```text
cat /etc/os-release 显示 Ubuntu 24.04
cat /proc/1/comm 显示 systemd
findmnt / 显示 /data/rootfs/ubuntu-24.04 或对应块设备作为 /
systemctl is-system-running 不再是 offline
USB NCM 可达
2323 或 SSH 至少一个入口可用
Panel service 可解释：已启动、disabled/manual，或有明确 failed log
```

如果 Ubuntu systemd 启动失败，再判断是否需要内核配置/代码修改：

```text
cgroup / namespace / devtmpfs / tmpfs / overlay / loop / binderfs / dm / ext4 等内核能力缺失
systemd 早期 mount API 失败
udev/devtmpfs 行为异常
DRM/KGSL 节点缺失
Wi-Fi/audio/vendor helper 依赖的内核 ABI 不存在
```

没有这些具体失败证据前，不把“改内核”作为第一动作。

### 实验 2 当前结果和降级策略

第一版 systemd-first 候选：

```text
artifacts/experiments/exp2-ubuntu-systemd-first/lineage-mininitramfs-boot-exp2-ubuntu-systemd.img
sha256: 902fea6ccd762bb1bc3a68639d9479019a6b3315559e37762170909d5efdb6c4
```

结果：

```text
fastboot boot 协议层成功
设备没有恢复预期 USB 救援通道
用户观察到进入 recovery
该候选停止使用
```

降级后的实验 2b：

```text
artifacts/experiments/exp2b-ubuntu-minimal-pid1/out/lineage-mininitramfs-boot-exp2b-ubuntu-minimal.img
sha256: 777ec8f0e84c9a575da132abdfb9d03c33b8c8f10eaf8906c6931940f5fdc022
```

实验 2b 目标：

```text
只验证 switch_root 到 Ubuntu rootfs
不启动 systemd
用 minimal Ubuntu PID1 helper 保留 USB/2323/sshd/status UI
```

执行条件：

```text
必须先看到 fastboot devices
```

执行命令：

```bash
./tools/platform-tools/fastboot boot artifacts/experiments/exp2b-ubuntu-minimal-pid1/out/lineage-mininitramfs-boot-exp2b-ubuntu-minimal.img
```

当前设备控制面注意：

```text
 recovery 后的当前状态曾出现 22/tcp 和 2323/tcp 开放但不返回 shell/SSH banner。
 adb 不可见，fastboot 不可见。
 但项目约定：当前状态通常可以通过软件路径直接回到 fastboot。
 手动进入 fastboot 只作为软件路径失效后的兜底。
```

软件回 fastboot 优先级：

```text
1. 如果 SSH 可执行命令，优先用 SSH 运行部署镜像内置的 reboot-bootloader/alioth-reboot bootloader。
2. 如果 2323 shell 可执行命令，再运行 /var/tmp/alioth-switchroot/reboot-bootloader。
3. 如果 adb 可见，使用 adb reboot bootloader。
4. 只有软件路径不可用时，才手动进入 fastboot。
```

SSH 回 fastboot 参考命令：

```bash
ssh root@172.16.42.2 'command -v reboot-bootloader >/dev/null 2>&1 && exec reboot-bootloader; command -v alioth-reboot >/dev/null 2>&1 && exec alioth-reboot bootloader; [ -x /var/tmp/alioth-switchroot/reboot-bootloader ] && exec /var/tmp/alioth-switchroot/reboot-bootloader; [ -x /var/tmp/alioth-switchroot/alioth-reboot ] && exec /var/tmp/alioth-switchroot/alioth-reboot bootloader; reboot bootloader'
```

注意：

```text
端口 open 不等于 SSH 可执行命令。
如果 ssh 卡在 banner exchange，说明当前不是可用 SSH 控制面，应先恢复到部署镜像的正常 SSH 状态。
```

## 从 fastboot 启动当前推荐镜像

当前活动槽 `_b` 的 `boot_b` 已在 2026-05-05 16:08 CST 验证为 Ubuntu 24.04 GPU-first monitor 推荐镜像。正常开机应默认进入 Ubuntu 24.04 LTS systemd。

临时启动同一镜像：

```bash
fastboot boot /vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/lineage-mininitramfs-boot.img
```

启动后主机应通过 USB DHCP 拿到 `172.16.42.1/24`，手机为 `172.16.42.2/24`。

如需重新写入当前默认 boot：

```bash
fastboot getvar current-slot
fastboot flash boot_b /vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/lineage-mininitramfs-boot.img
fastboot reboot
```

只在确认当前 slot 是 `_b` 时使用上面的 `boot_b` 命令；如果以后切到 `_a`，应改为 `boot_a`。

回滚到旧 LELE OS/Arch 监控镜像：

```bash
fastboot getvar current-slot
fastboot flash boot_b /vmdata/android/redmik40/lineage-sm8250/switchroot-shell-ui-docker-screen/lineage-mininitramfs-boot.img
fastboot reboot
```

## SSH 登录

```bash
ssh -i /vmdata/android/redmik40/keys/alioth_usb_ed25519 \
  -o UserKnownHostsFile=/tmp/alioth_ubuntu2404_known_hosts \
  -o StrictHostKeyChecking=no \
  root@172.16.42.2
```

备用 TCP shell：

```bash
nc 172.16.42.2 2323
```

## 屏幕菜单

当前 GPU monitor 由 `lele-status-ui.service` 启动，helper 是：

```text
/data/experiments/alioth_gpu_monitor_service
sha256: 282206b88851f98c85b70985555cde6664d0ea680cf73e71118742a6537f5791
```

按键行为：

```text
VOLUP:        切换普通信息页；在菜单里选择下一项
VOLDOWN:      代码支持上一项，但合成注入验证不足
POWER:        从普通页打开菜单；在菜单里确认动作
```

当前菜单顺序：

```text
CANCEL
UBUNTU DESKTOP
REBOOT SYSTEM
FASTBOOT
```

`UBUNTU DESKTOP` 会启动 `alioth-wayland-labwc-session.service`，临时进入 labwc/wlroots desktop-smoke 会话，退出后恢复 GPU monitor。`REBOOT SYSTEM` 和 `FASTBOOT` 都是带确认的菜单动作；当前 helper 已复验 `CANCEL`、`UBUNTU DESKTOP`、`REBOOT SYSTEM` 和 `FASTBOOT`。

## 手机侧基础检查

```sh
uname -a
cat /etc/os-release
tr '\0' ' ' </proc/1/cmdline; echo
systemctl is-system-running
systemctl --failed --no-pager
ip -br addr
df -h /
ss -lntp
tail -80 /run/alioth-status-ui.log
tail -80 /tmp/alioth-status-ui-c.log
```

## Thermal Guard

Ubuntu does not start Android init services such as Xiaomi `mi_thermald` or the
QTI thermal profile stack. Keep `alioth-thermal-guard.service` enabled before
long CPU/GPU/compile runs:

```sh
systemctl status alioth-thermal-guard.service --no-pager
cat /run/alioth-thermal-guard.status
journalctl -u alioth-thermal-guard.service -n 80 --no-pager
```

Installed paths:

```text
/usr/local/sbin/alioth-thermal-guard
/etc/systemd/system/alioth-thermal-guard.service
/run/alioth-thermal-guard.status
/var/log/alioth-thermal-guard.log
```

Default policy:

```text
warm:     battery 40C / CPU 60C / GPU 60C / PMIC 65C -> schedutil
hot:      battery 42C / CPU 70C / GPU 70C / PMIC 80C -> powersave
critical: battery 48C / CPU 85C / GPU 85C / PMIC 95C -> powersave
shutdown: battery 55C / CPU 105C / GPU 105C / PMIC 115C -> systemctl poweroff
```

## Ubuntu 硬件 ABI 盘点

主机侧执行只读盘点：

```bash
scripts/alioth_ubuntu_hw_inventory.sh
```

默认输出：

```text
/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-hw-abi-inventory/report.txt
```

当前摘要见 `docs/hardware-abi-status.md`。插 USB 扩展、鼠标或键盘前先写单独实验计划；当前 USB 口承担 NCM/SSH，切到 host/OTG 角色可能导致 `usb0` 断开。

## 图形栈基线

DRM/Mesa 只读盘点：

```text
/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/report.txt
```

图形测试工具安装记录：

```text
/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/install-graphics-tools.txt
```

当前已安装 `modetest`、`kmscube`、`eglinfo`、`glxinfo`、`weston`、`weston-simple-egl`、`weston-simple-dmabuf-egl`、`wayland-info`、`seatd`、`seatd-launch`、`cage`、`grim`、`wev` 和 wlroots 运行库。`card0-DSI-1` 连接并启用，模式为 1080x2400 60/90/120 Hz；`/dev/dri/card0` 和 `/dev/dri/renderD128` 均可由 root 打开。

已完成的 runtime 记录：

```text
modetest: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/graphics-probe-modetest.txt
eglinfo:  /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/graphics-probe-eglinfo.txt
KGSL:     /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/kgsl-node-egl-retry.txt
Weston:   /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/graphics-probe-weston-pixman-openvt-switch.txt
Weston builtin non-VT: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/graphics-probe-weston-pixman-builtin-novt.txt
Mesa/KGSL strace: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-strace.txt
Mesa driver matrix: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-driver-selection-matrix.txt
Mesa targets: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-gallium-targets.txt
libgallium inspection: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/libgallium-host-target-inspection.txt
Android/Lineage graphics userspace inventory: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/android-lineage-graphics-userspace-inventory.txt
KGSL open-path inventory: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/kgsl-open-path-inventory.txt
KGSL firmware getproperty smoke: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/kgsl-firmware-getproperty-smoke.txt
Mesa/Vulkan loader inventory: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-vulkan-loader-inventory.txt
Mesa/Vulkan tools smoke: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-vulkan-tools-smoke.txt
Pulled Ubuntu freedreno ICD: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/libvulkan_freedreno-ubuntu-arm64.so
Mesa KGSL build feasibility: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-build-feasibility.txt
Mesa KGSL Meson setup: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-meson-setup.txt
Mesa KGSL private build: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-build.txt
Mesa KGSL private build git_sha1 retry: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-build-gitsha1-retry.txt
Mesa KGSL private build libdrm include retry: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-build-libdrm-include-retry.txt
Mesa KGSL private build define user retry: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-build-define-user-retry.txt
Private KGSL freedreno ICD library: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/libvulkan_freedreno-kgsl-mesa2034-arm64.so
Private KGSL vulkaninfo smoke: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-vulkaninfo-smoke.txt
Private KGSL no-op submit: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-noop-submit.txt
Private KGSL compute smoke: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-compute-smoke.txt
Private KGSL image clear smoke: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-image-clear-smoke.txt
Private KGSL offscreen triangle smoke: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-offscreen-triangle-smoke.txt
Private KGSL sampled texture smoke: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-textured-quad-smoke.txt
Private KGSL visible staged present first attempt: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-visible-staged-present-smoke.txt
Private KGSL visible staged present retry: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-visible-staged-present-smoke-retry1.txt
Private KGSL dmabuf probe: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-dmabuf-probe.txt
Private KGSL dmabuf probe retry: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-dmabuf-probe-retry1.txt
Private KGSL dmabuf export/import proof: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-dmabuf-export-import.txt
DRM PRIME fd -> private KGSL import proof: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-prime-to-kgsl-vulkan-import.txt
DRM PRIME KGSL clear readback proof: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-prime-kgsl-clear-readback.txt
DRM PRIME KGSL clear present proof: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-prime-kgsl-clear-present.txt
Private KGSL Vulkan WSI/display probe: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/private-kgsl-vulkan-wsi-display-probe.txt
Private KGSL Vulkan WSI/display probe retry1: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/private-kgsl-vulkan-wsi-display-probe-retry1.txt
Private KGSL Vulkan WSI/display probe retry2: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/private-kgsl-vulkan-wsi-display-probe-retry2.txt
Private EGL/GBM/Gallium runtime inventory: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-egl-gbm-glonly-runtime-inventory.txt
MSM DRM ioctl capability probe: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-msm-ioctl-probe.txt
CPU-only DRM page-flip smoke: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-pageflip-smoke.txt
DRM PRIME KGSL double-buffer page-flip clear: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-prime-kgsl-pageflip-clear.txt
DRM PRIME KGSL direct triangle page-flip retry1: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-prime-kgsl-triangle-pageflip-retry1.txt
DRM PRIME KGSL direct textured page-flip: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-prime-kgsl-textured-pageflip.txt
DRM PRIME KGSL glyph text page-flip retry1: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-prime-kgsl-glyph-text-pageflip-retry1.txt
DRM PRIME KGSL dynamic monitor page-flip: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-prime-kgsl-monitor-pageflip.txt
GPU monitor service boot verify: /vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/gpu-monitor-service-boot-verify.txt
GPU monitor runtime-status smoke: /vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/gpu-monitor-runtime-status/smoke.txt
GPU monitor runtime-status default deploy: /vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/gpu-monitor-runtime-status/deploy-default.txt
```

当前结论：`modetest` 可打开 `MSM Snapdragon DRM` 并枚举 DSI/CRTC/plane；Mesa EGL 只能走 `llvmpipe`。临时创建 `/dev/kgsl-3d0` 并安装 `libdrm-freedreno1` 后仍没有 Adreno renderer；进一步 `strace` 和驱动选择矩阵证明当前 EGL/GBM 路径没有打开 `/dev/kgsl-3d0`，也没有加载 `kgsl_dri.so`/`msm_dri.so`。`MESA_LOADER_DRIVER_OVERRIDE=kgsl/msm/freedreno` 仍是 `llvmpipe`，`GALLIUM_DRIVER=kgsl/msm/freedreno` 会让 EGL 初始化失败。Android/Lineage 可工作的图形路径是 Qualcomm Android 用户态：Adreno EGL/GLES/Vulkan blob -> `libgsl.so` -> KGSL ioctl，并接入 QCOM gralloc/HWComposer/mapper/composer/display HAL；这些库依赖 Android/Bionic/HIDL/ION/libsync，不是 Ubuntu glibc 下的可直接复用库。A650 firmware 暂存到 `/lib/firmware` 后，Ubuntu helper 已可打开 `/dev/kgsl-3d0` 并读取 KGSL properties。Ubuntu `mesa-vulkan-drivers` 已安装 freedreno/Turnip ICD，但 stock `libvulkan_freedreno.so` 拒绝当前 `/dev/dri/renderD128 (msm_drm)`，默认 `vulkaninfo` 只剩 `llvmpipe`，强制 freedreno ICD 失败；拉出的库也没有 `/dev/kgsl-3d0`/`tu_kgsl` 证据。本地 Lineage Mesa `20.3.4` 已以 `-Dfreedreno-kgsl=true` 在手机 private prefix 构建出 KGSL Turnip ICD；强制私有 ICD 后，`vulkaninfo` 枚举 `FD650`，no-surface 空提交到达 `IOCTL_KGSL_GPU_COMMAND`，tiny compute shader 能写入并回读 `0x5a17c0de`，image clear/copy 能回读 RGBA `00 ff 00 ff` 且 0 mismatch，offscreen render-pass triangle 能回读红色中心像素和 `red=1352 black=2744 other=0`。CPU-staged visible smoke 已把 KGSL 渲染结果转为 XRGB raw，并通过 DRM dumb buffer 在 connector 29 / CRTC 129 / 1080x2400@60 上显示 5 秒，随后 status UI 恢复。KGSL 分配向外 export dma-buf 被 `tu_bo_export_dmabuf()` stub 阻断，但 DRM dumb PRIME fd -> KGSL import/write/readback/scanout 已通过：全屏 1080x2400 DRM-owned buffer 由 KGSL 清色，KMS 扫出同一 fb 5 秒。Private KHR_display WSI 会在 `vkEnumeratePhysicalDevices()` 段错误，因此不跑 direct-display `vkcube`。仍不等于 EGL/GBM、Vulkan WSI swapchain 或桌面 compositor 可用。Weston DRM backend 已尝试 pixman、显式 `/dev/dri/card0`、临时 VT 节点、`openvt`、`openvt -s` 和 libseat builtin non-VT，仍报 `could not open DRM device '/dev/dri/card0'`。

补充结论：private EGL/GBM/Gallium GL-only Mesa 已可 build/install，但 runtime 不能创建 freedreno DRI screen/GBM device；`kgsl_dri.so` 在这份 Mesa 中只是 `msm` alias，实际没有 Gallium KGSL winsys。MSM DRM ioctl probe 也确认当前 `msm_drm` 节点不提供 GPU params/IOVA。KMS page-flip 已单独通过，`scripts/alioth_drm_prime_vulkan_pageflip_clear.c` 已证明两个 DRM PRIME buffer 可以由 KGSL Vulkan 逐帧写入并 KMS page-flip，18/18 个事件通过；`scripts/alioth_drm_prime_vulkan_triangle_pageflip.c` 又证明 render pass/graphics pipeline 可以直接画入 imported scanout buffer，每帧 5/5 个样本通过，18/18 个 page-flip event 通过；`scripts/alioth_drm_prime_vulkan_textured_pageflip.c` 进一步证明 sampled texture UI 内容可直接画入 scanout buffer，每帧 8/8 个样本通过，18/18 个 page-flip event 通过；`scripts/alioth_drm_prime_vulkan_glyph_text_pageflip.c` 已证明 glyph atlas + vertex-buffer text quads 可直接画入 scanout buffer，每帧 3/3 个文字/背景样本通过，12/12 个 page-flip event 通过，并已扩展为 runtime-status GPU 监控页。当前默认 `lele-status-ui.service` 已是 GPU-first wrapper，优先运行 `/data/experiments/alioth_gpu_monitor_service`，首屏显示 systemd failed count、电池、USB、Wi-Fi 和 audio ADSP/SND 状态（WIFI 行会按 wlan0 条件显示 `UP`/`SCAN READY`/`LINK`/`DOWN`），失败时回退原 CPU UI。旧 helper 备份在 `/data/experiments/alioth_gpu_monitor_service.pre-runtime-status-20260509`。

注意：当前 GPU monitor service 占用 DRM/KMS 和 KGSL。运行任何显示接管实验前必须写单独实验计划，并用 wrapper 临时停止 `lele-status-ui`，测试结束后恢复它。不要直接手工长时间运行 compositor。Weston 启动变体、stock Ubuntu Mesa/Vulkan 环境变量变体、private KHR_display direct-display 变体、以及 private EGL/GBM/Gallium loader/env 变体暂时停止重复测试；private KGSL Turnip ICD 已可 custom double-buffer direct scanout、直接 triangle render-pass page-flip、headless sampled texture rendering、direct textured page-flip、glyph text page-flip、runtime-status GPU 监控页、以及默认 GPU-first monitor service。下一步优先把 monitor/launcher 接入更多输入和实际操作，而不是重复底层 present smoke；仍不替换 `/usr` Mesa。

## 临时 Wayland 会话

当前已有一个可重复的非加速标准 Wayland 路径：

```text
compositor: cage / wlroots, labwc / wlroots
renderer:   pixman
output:     DSI-1 1080x2400
clients:    weston-simple-shm, weston-terminal
clients:    foot, swaybg, fuzzel, wlr-randr (labwc desktop-smoke)
screenshot: grim / zwlr_screencopy
launcher:   /usr/local/sbin/alioth-wayland-cage-session
unit:       alioth-wayland-cage-session.service (disabled)
launcher:   /usr/local/sbin/alioth-wayland-labwc-session
unit:       alioth-wayland-labwc-session.service (disabled)
```

手动运行一次 8 秒终端会话并抓图：

```bash
ssh -i /vmdata/android/redmik40/keys/alioth_usb_ed25519 \
  -o UserKnownHostsFile=/tmp/alioth_ubuntu2404_known_hosts \
  -o StrictHostKeyChecking=no \
  root@172.16.42.2 \
  '/usr/local/sbin/alioth-wayland-cage-session --duration 8 --mode terminal --screenshot /run/alioth-wlroots-test/manual-terminal.png'
```

通过 systemd 运行：

```bash
ssh -i /vmdata/android/redmik40/keys/alioth_usb_ed25519 \
  -o UserKnownHostsFile=/tmp/alioth_ubuntu2404_known_hosts \
  -o StrictHostKeyChecking=no \
  root@172.16.42.2 \
  'systemctl start alioth-wayland-cage-session.service'
```

运行一次 labwc/window-manager desktop-smoke 会话：

```bash
ssh -i /vmdata/android/redmik40/keys/alioth_usb_ed25519 \
  -o UserKnownHostsFile=/tmp/alioth_ubuntu2404_known_hosts \
  -o StrictHostKeyChecking=no \
  root@172.16.42.2 \
  'systemctl start alioth-wayland-labwc-session.service'
```

拉取最近截图：

```bash
scp -i /vmdata/android/redmik40/keys/alioth_usb_ed25519 \
  -o UserKnownHostsFile=/tmp/alioth_ubuntu2404_known_hosts \
  -o StrictHostKeyChecking=no \
  root@172.16.42.2:/run/alioth-wlroots-test/systemd-terminal.png \
  /vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/systemd-terminal.png
```

labwc 最近截图在：

```text
/run/alioth-labwc-test/desktop-smoke-systemd.png
```

已知问题：

```text
Virtual-1 connector 会产生 wlroots swapchain failed 日志，但 DSI-1 输出、Wayland client、grim 截图均已验证可用。
当前是 pixman/software renderer，不是 EGL/GBM/硬件加速桌面。
Wayland seat 已暴露 keyboard touch；2026-05-10 的 A7 run 已在实验链路中确认 libinput 与 labwc compositor/`wl_seat` touch 可用，并记录了 `wev_capture=1`，会话恢复正常。
不过 `wev-events.log` 在当前合成注入路径为空，因此客户端触控交互仍需一次真实物理手指验证。
```

USB role/HID 只读盘点：

```text
/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-usb-role-hid-readiness/report.txt
```

当前 USB 基线：ConfigFS gadget `alioth` 绑定 `a600000.dwc3`，函数为 `ncm.usb0`，`usb0=172.16.42.2/24`。没有 `/sys/class/usb_role`，但 `/sys/class/typec/port0/data_role` 显示 `host [device]`，对应属性 root 可写；真实 USB hub/HID 测试会大概率断开 SSH，必须先准备本地屏幕/按键确认或手动重启/fastboot 回退路径。

已准备但尚未执行真实切换的 helper：

```sh
/tmp/alioth_usb_host_probe.sh --dry-run --duration-sec 10
systemd-run --unit=alioth-usb-host-probe --collect /tmp/alioth_usb_host_probe.sh --execute --duration-sec 45
```

第二条会停止当前 USB NCM gadget、尝试切到 Type-C host/source，等待计时后切回 device/sink 并重启 USB 网络服务。只有在 USB-C hub/键盘/鼠标准备好、并接受 SSH 临时断开时才执行。

## 输入设备基线

当前 Ubuntu rootfs 已安装 `evtest`、`evemu-tools` 和 `libinput-tools`。最近一次非交互报告：

```text
/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-input-baseline/report.txt
```

快速检查：

```sh
cat /proc/bus/input/devices
libinput list-devices
timeout 2 evtest /dev/input/event2
```

当前基线：`event2 fts_ts` 可被 `libinput` 识别为 touch，并已捕获实时触摸轨迹；`event1 qpnp_pon` 提供 Power/VolumeDown，`event5 gpio-keys` 提供 VolumeUp，`event4 aw8697_haptic` 暴露 EV_FF。触控在 `A7` 实验中已证实到 `libinput`/compositor/`wl_seat` 级，桌面/app 手势体验仍待物理触控验证。

2026-05-09 追加验证：

```text
/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/input-touch/report.txt
```

`libinput debug-events --device /dev/input/event2` 已捕获 `TOUCH_DOWN`/`TOUCH_MOTION`/`TOUCH_FRAME`，`evtest /dev/input/event2` 已捕获 `ABS_MT_POSITION_X/Y` 实时坐标。USB 鼠标键盘仍未在手机 host 模式下测试。

### 可复现的 LabWC 触控验证（推荐）

将实验脚本同步到手机 `/tmp` 并执行：

```bash
scp -i /vmdata/android/redmik40/keys/alioth_usb_ed25519 \
  -o UserKnownHostsFile=/tmp/alioth_ubuntu2404_known_hosts \
  -o StrictHostKeyChecking=no \
  scripts/alioth_labwc_touch_smoke.sh \
  root@172.16.42.2:/tmp/alioth-labwc-touch-smoke.sh

ssh -i /vmdata/android/redmik40/keys/alioth_usb_ed25519 \
  -o UserKnownHostsFile=/tmp/alioth_ubuntu2404_known_hosts \
  -o StrictHostKeyChecking=no \
  root@172.16.42.2 \
  'chmod +x /tmp/alioth-labwc-touch-smoke.sh && /tmp/alioth-labwc-touch-smoke.sh --runtime-dir /run/alioth-labwc-touch --duration 18 --screenshot /tmp/alioth-labwc-touch.png'
```

默认会做三件事：

- `timeout 12s libinput debug-events --device /dev/input/event2`（默认事件窗口）
- 两次合成 `evemu-event` 触控（默认 `(4300,9600)` 和 `(7800,17800)`）
- `alioth-wayland-labwc-session --mode desktop-smoke` 会话并自动恢复 `lele-status-ui`

产物目录会保存在：

```text
/run/alioth-labwc-touch/baseline.txt
/run/alioth-labwc-touch/session.log
/run/alioth-labwc-touch/libinput-events.log
/run/alioth-labwc-touch/touch-inject.log
/run/alioth-labwc-touch/summary.txt
/run/alioth-labwc-touch/post.txt
```

`A7` 最新落地（AI2）已在以下目录留存，包含 `--capture-wev` 结果：

```text
/vmdata/android/redmik40/experiments/2026-05-10-ai2-ui-touch-smoke/alioth-ai2-labwc-touch-smoke
/vmdata/android/redmik40/experiments/2026-05-10-ai2-ui-touch-smoke/alioth-ai2-labwc-touch-smoke.png
```

`/run/alioth-ai2-labwc-touch-smoke` / `summary.txt` 结果要点：

- `wev_capture=1`
- `session_rc=0`
- `libinput_rc=124`（12s 超时窗口）
- `wev-events.log` 仍为空（当前 `wev` 仍偏向 window-focus 场景）
- UI 成功恢复：`lele-status-ui=active`，`alioth-wayland-labwc-session=inactive`

## 显示亮度

当前背光控制路径：

```text
/sys/class/backlight/panel0-backlight/brightness
max_brightness: 2047
```

2026-05-09 bounded dim/restore 报告：

```text
/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/brightness/dim-restore-retry.txt
```

已验证写入 `536 -> 178 -> 536` 并恢复成功。`actual_brightness` 当前一直读为 `0`，不要把它当作可见亮度的权威读回。

手机侧 helper 已部署：

```sh
lele-brightness status
lele-brightness get
lele-brightness set 512
lele-brightness percent 25
lele-brightness dim 2 33
```

`dim` 会保存原值、短暂降低亮度并自动恢复；如果按百分比计算出来不低于当前亮度，会自动改用当前亮度约三分之一，避免“dim”变成升亮。

触摸/haptic smoke 记录：

```text
/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-touch-haptic-smoke/report.txt
```

当前 haptic 注意点：`aw8697_haptic` 虽然暴露 EV_FF，但 `FF_RUMBLE` upload 返回 `EFAULT`；downstream aw8697 驱动的简单播放路径是 `FF_CONSTANT`。`scripts/alioth_haptic_pulse.py --effect constant` 已能 upload/playback，播放后的 `EVIOCRMFF` cleanup 返回 `EINVAL`，helper 将其记录为 warning。另有 `/sys/bus/i2c/devices/2-005a/{duration,activate,effect_id,gain,vmax}` 等 sysfs 控制面。继续尝试振动时只做单次短脉冲，不要做循环播放。

aw8697 控制路径只读报告：

```text
/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-aw8697-path/report.txt
```

## 恢复手机侧出网

当前 Ubuntu 24.04 默认通过 USB NCM 使用主机侧 `172.16.42.1` 作为网关，DNS 由 systemd-resolved 配置。先验证：

```sh
ip route
resolvectl status usb0
apt-get update -qq
```

旧 LELE OS/Arch 环境需要反向 SOCKS 时，主机侧执行：

```bash
scripts/start_alioth_usb_socks.sh
```

旧环境手机侧新登录 shell 会自动设置：

```sh
ALL_PROXY=socks5h://127.0.0.1:18080
```

当前 shell 可手动补：

```sh
export ALL_PROXY=socks5h://127.0.0.1:18080
curl https://archlinux.org/
```

## 数据盘和服务

这一节是旧 LELE OS/Arch 服务栈流程；当前默认 Ubuntu 24.04 还没有把 Docker/nginx/PostgreSQL 迁入主线 systemd。需要回到旧镜像或后续迁移服务时再使用。

旧环境手机侧执行：

```sh
lele-data-mount
lele-docker-start
lele-services-start
```

验证：

```sh
docker info
docker run --rm lele/busybox-test:local /bin/busybox uname -a
curl http://127.0.0.1/
valkey-cli ping
pg_isready -h /run/postgresql
```

主机侧验证 nginx：

```bash
curl http://172.16.42.2/
```

## Ubuntu 24.04 default systemd

当前 Ubuntu systemd 主线是 Ubuntu 24.04 LTS，不是 26.04。26.04 的 systemd 259 可成为 PID1，但启动服务时在当前 4.19 kernel 上触发 executor `ENOSYS`。

主机侧缓存：

```text
/vmdata/android/redmik40/rootfs-cache/ubuntu-24.04/noble-server-cloudimg-arm64-root.tar.xz
sha256: 3c8f36e427583571cb5536b339355b9d9b60c233eb7aed5e51c41b3ceba5accf
```

手机侧 rootfs：

```text
/data/rootfs/ubuntu-24.04
```

当前默认 boot image：

```text
/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/lineage-mininitramfs-boot.img
sha256: 19e159007fb04e009f840b09dfb33b68808ac45618403c63e80f2851e7608a23
```

重建当前默认 boot image：

```bash
OUT_DIR=/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service \
INIT_MODE=switchroot-ubuntu-systemd \
UBUNTU_ROOTFS_PATH=/rootfs/ubuntu-24.04 \
UBUNTU_SYSTEMD_WRAPPER_MODE=exec \
STATUS_UI_WRAPPER_BIN=$PWD/scripts/alioth_gpu_status_ui_wrapper.sh \
bash scripts/build_lineage_mininitramfs_boot.sh
```

临时启动：

```bash
fastboot boot /vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/lineage-mininitramfs-boot.img
```

SSH 登录：

```bash
ssh -i /vmdata/android/redmik40/keys/alioth_usb_ed25519 \
  -o UserKnownHostsFile=/tmp/alioth_ubuntu2404_known_hosts \
  -o StrictHostKeyChecking=no \
  root@172.16.42.2
```

已验证：

```text
root: /dev/block/by-name/userdata[/rootfs/ubuntu-24.04] ext4
pid1: /lib/systemd/systemd, systemd 255
service spawn: minimal /bin/true systemd unit OK
ssh: root@172.16.42.2 OK after host keys + authorized_keys are present
fallback shell: tcp/2323 OK
status UI: lele-status-ui.service active with /dev/dri/card0/renderD128 and /dev/input/event* created
UI: header shows `UBUNTU 24.04`; BOOT panel separates `KERNEL` from `DISTRO`; POWER panel shows charge rate and poweroff confirmation text
input: event1 qpnp_pon and event5 gpio-keys opened by status UI; event2 fts_ts live touch events proven through libinput/evtest
network: root and _apt use inet gid 3003; usb0 DNS via systemd-resolved
apt: apt-get update OK
time: build-epoch floor + HTTP Date refinement OK; `alioth-http-time-sync.service` enabled and verified; NTP not yet synchronized
audio: `alioth-audio-adsp-boot.service` enabled; ADSP ONLINE; `kona-mtp-snd-card` visible; `/dev/snd` recreated from sysfs; `aplay -l` and `alsactl info 0` work
journal: persistent, capped at 128M, journalctl --list-boots works
state: running, 0 failed units
```

下一次重建 rootfs 或 boot image 时应持久化：

```text
mask/disable: systemd-networkd-wait-online.service, multipathd.service, multipathd.socket, systemd-rfkill.socket
ensure: ssh host keys or first-boot key generation
ensure: /root/.ssh/authorized_keys contains /vmdata/android/redmik40/keys/alioth_usb_ed25519.pub
ensure: lele-minitcpsh.service is masked; wrapper-level 2323 is the fallback shell
ensure: lele-status-ui.service runs lele-drm-nodes before starting; helper creates DRM and input event nodes
ensure: /usr/local/sbin/lele-brightness is installed from scripts/alioth_phone_brightness.sh if brightness helper is expected
ensure: /usr/local/sbin/alioth-http-time-sync and alioth-http-time-sync.service are installed from scripts/alioth_http_time_sync.sh and configs/alioth-http-time-sync.service
ensure: /usr/local/sbin/alioth-audio-adsp-boot and alioth-audio-adsp-boot.service are installed from scripts/alioth_audio_adsp_boot.sh and configs/alioth-audio-adsp-boot.service
```

rootfs policy helper:

```bash
scripts/prep_ubuntu_rootfs_policy.sh check /path/to/ubuntu-rootfs
scripts/prep_ubuntu_rootfs_policy.sh apply /path/to/ubuntu-rootfs
scripts/prep_ubuntu_rootfs_policy.sh check /path/to/ubuntu-rootfs
```

在手机 live Ubuntu 内验证当前 rootfs：

```sh
SSH_PUBKEY_FILE=/root/.ssh/authorized_keys /tmp/prep_ubuntu_rootfs_policy.sh check /
```

host-side staging helper:

```bash
CHECK_ONLY=1 scripts/stage_ubuntu_rootfs_to_phone.sh

ROOTFS_NAME=ubuntu-24.04-stage-test \
scripts/stage_ubuntu_rootfs_to_phone.sh
```

默认 `ROOTFS_NAME=ubuntu-24.04`，脚本会拒绝覆盖已存在目标，除非显式设置 `REPLACE=1`。不要对当前工作 rootfs 使用 `REPLACE=1`，直到有明确回滚方案。

当前该 rootfs 已是默认启动目标。回滚目标是旧 LELE OS/Arch boot image，不是替换 userdata rootfs。

## 时间同步

当前 rootfs 已启用 best-effort HTTP Date 校时服务：

```sh
systemctl status alioth-http-time-sync.service --no-pager
cat /run/alioth-http-time-sync.log
```

该服务只做一次 oneshot：优先保持已同步的 NTP；如果 `systemd-timesyncd` 仍未同步，则从 HTTP `Date` header 取 UTC，校验时间下限后用 `date -u -s` 修正明显偏差。没有网络或没有可用 header 时也退出 0，避免产生 failed unit。NTP 仍未真正同步，后续如果要调 timesyncd，仍按单独实验记录。

## 音频 ADSP/ALSA 基线

当前音频只验证到 ADSP/ASoC/ALSA 枚举，不代表扬声器、听筒、耳机或麦克风 route 已可用。不要直接运行 `speaker-test`、`aplay` 播放或 `amixer set`，除非先按实验协议写好 route、音量和停止条件。

只读检查：

```sh
systemctl status alioth-audio-adsp-boot.service --no-pager
cat /run/alioth-audio-state
cat /proc/asound/cards
cat /proc/asound/pcm
find /dev/snd -maxdepth 1 -type c | wc -l
aplay -l
arecord -l
alsactl info 0
amixer -c 0 controls | sed -n '1,160p'
```

已验证的服务行为：

```text
firmware path: /vendor/firmware_mnt/image
ADSP trigger:  echo 1 > /sys/kernel/boot_adsp/boot
card:          kona-mtp-snd-card
/dev/snd:      120 character nodes recreated from /sys/class/sound/*/dev
tools:         alsa-utils installed; aplay/arecord/alsactl enumeration works
```

如需手动重跑当前 helper：

```sh
systemctl restart alioth-audio-adsp-boot.service
cat /run/alioth-audio-adsp-boot.log
cat /run/alioth-audio-state
```

`amixer -c 0 scontrols` 当前返回 `Mixer sysdefault:0 load error: No such device`，但 `amixer -c 0 controls` 可以列出 5333 个低层控件。这是 simple-mixer 抽象缺失，不等于控制面不可访问。

当前 route 研究产物：

```text
/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/audio-route-inventory/report.txt
/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/audio-route-inventory/focused-routes.txt
```

只读解析显示 Android `deep-buffer-playback speaker` 主要会打开 `TERT_TDM_RX_0 Audio Mixer MultiMedia1=1`，而 `speaker` 基础路径还涉及 `TERT_TDM_RX_0`、`PCM Source=DSP`、`ASP/DSP RX` 和 RCV 相关控件。2026-05-09 fixed 5 秒 1 kHz 测试已确认 live 外放路径实际是 TERT-MI2S / CS35L41：`aplay_beep_rc=0`，dmesg 有 CS35L41 unmute/mute，用户反馈声音可接受，测试后所有捕获的 mixer 值已恢复。WSA base speaker amp 控件在当前 live profile 不存在，dmesg 也提示使用 `tert_mi2s_rx_cs35l41_dai_links`。

## Ubuntu 26.04 rootfs candidate

Ubuntu 26.04 只作为候选 rootfs 保留；当前默认启动系统是 Ubuntu 24.04 LTS。26.04 的 systemd 259 已确认与当前 4.19 downstream kernel 的 service spawn 路径不兼容，除非后续重编 systemd 或内核，否则不作为主线。

手机侧准备 `/data`：

```sh
lele-data-mount
```

主机侧缓存：

```text
/vmdata/android/redmik40/rootfs-cache/ubuntu-26.04/resolute-server-cloudimg-arm64-root.tar.xz
sha256: ef56b62a89f38909f60e181c9ee4494f42c2780f947b13bbb96f07e382e6f341
```

手机侧 rootfs：

```text
/data/rootfs/ubuntu-26.04
```

chroot 验证示例：

```sh
ROOT=/data/rootfs/ubuntu-26.04
date -u -s 'YYYY-MM-DD HH:MM:SS'
cp /etc/resolv.conf "$ROOT/etc/resolv.conf"
mount -t proc proc "$ROOT/proc"
mount --rbind /sys "$ROOT/sys"
mount --rbind /dev "$ROOT/dev"
mount --rbind /run "$ROOT/run"
chroot "$ROOT" /bin/bash -lc 'cat /etc/os-release; uname -a; dpkg --print-architecture; apt-get update'
umount -R "$ROOT/run" "$ROOT/dev" "$ROOT/sys" "$ROOT/proc"
```

2026-05-04 15:40 CST 已完成一次临时 `fastboot boot` + `switch_root` smoke test。该测试没有刷写分区，Ubuntu 会话中已验证：

```text
root: /dev/block/by-name/userdata[/rootfs/ubuntu-26.04] ext4
pid1: /var/tmp/alioth-switchroot/alioth-ubuntu-init
ssh: root@172.16.42.2 OK
fallback shell: tcp/2323 OK
status UI: DRM/KMS OK
apt: apt 3.2.0 arm64 present
egress: temporary host NAT/default route works for root processes
apt: default apt update/install works after Android inet GID policy
dev: standalone tmpfs /dev and fixed devpts in latest image
cgroup: cgroup2 mounted in latest image
```

复现实验 boot image：

```bash
OUT_DIR=/vmdata/android/redmik40/experiments/2026-05-04-ubuntu-switchroot-dev-cgroup-route \
INIT_MODE=switchroot-ubuntu \
UBUNTU_ROOTFS_PATH=/rootfs/ubuntu-26.04 \
KERNEL_IMG=/vmdata/android/redmik40/lineage-sm8250/artifacts/kernel-lineage20-alioth-docker/Image \
bash scripts/build_lineage_mininitramfs_boot.sh build
```

临时启动：

```bash
fastboot boot /vmdata/android/redmik40/experiments/2026-05-04-ubuntu-switchroot-dev-cgroup-route/lineage-mininitramfs-boot.img
```

Ubuntu rootfs 使用自己的 SSH host key。为了避免和默认 Arch/LELE OS 的 `known_hosts` 冲突，临时 Ubuntu 会话建议使用单独文件：

```bash
ssh -i /vmdata/android/redmik40/keys/alioth_usb_ed25519 \
  -o UserKnownHostsFile=/tmp/alioth_ubuntu_known_hosts \
  -o StrictHostKeyChecking=no \
  root@172.16.42.2
```

不要把该 rootfs 设为默认启动。26.04 的非 chroot 和 systemd PID1 已证明，但 systemd 259 的服务启动路径在当前 4.19 kernel 上失败，当前主线已转向 Ubuntu 24.04 LTS。

当前临时 Ubuntu 出网基线：host 侧用 NetworkManager active device shared mode 给 `enx7229ea144ff3` 临时启用 `172.16.42.1/24` 共享/NAT，phone 侧临时路由为 `default via 172.16.42.1 dev usb0`。root 进程可以解析 DNS 和访问 HTTP。

Ubuntu rootfs 当前采用 Android downstream 网络权限组策略：

```text
inet:x:3003:_apt,root
_apt primary gid: 3003
root supplementary gid: 3003
```

这个策略已验证默认 `apt-get update`、`apt-get install --no-install-recommends socat`、`ping 8.8.8.8`、`_apt` DNS/TCP 均可用。不要再把 `APT::Sandbox::User=root` 当成常规方案；它只保留为诊断 workaround。

回滚当前 host/phone 临时出网状态：

```bash
nmcli device reapply enx7229ea144ff3

ssh -i /vmdata/android/redmik40/keys/alioth_usb_ed25519 \
  -o UserKnownHostsFile=/tmp/alioth_ubuntu_known_hosts \
  -o StrictHostKeyChecking=no \
  root@172.16.42.2 'ip route del default via 172.16.42.1 dev usb0 2>/dev/null || true'
```

早期 `ping`/APT 沙箱曾受 Android downstream 网络权限组限制；当前记录的修正策略是 `inet` GID `3003`，`root` supplementary gid `3003`，`_apt` primary gid `3003`。

## AP 配置模式

查看状态：

```sh
lele-ap-status
```

关闭 AP 并尝试恢复 Wi-Fi client：

```sh
lele-ap-stop
alioth-wifi-connect
lele-services-start
```

AP 模式信息：

```text
SSID: LELE-OS-SETUP
Password: lele12345678
Portal: http://10.42.0.1/
```

AP 模式下 portal 占用 80 端口，nginx 停止是预期行为。

## 屏幕和按键

状态 UI 当前行为：

- 单按电源键：切换黑屏/显示。
- 双按电源键：切换纯色 lamp 模式。
- 音量上/下：循环 `normal`、`low`、`night`。
- 长按电源键：打开 `POWER OFF?` 确认界面，不会直接关机。
- 关机确认打开时：再次按电源键确认关机；不确认则 10 秒超时自动取消。
- 音量键始终只作为显示模式/亮度控制，不作为关机控制。
- 触摸事件已过滤，避免误触导致亮度变化。

状态文件：

```sh
cat /run/lele-display-mode
tail -80 /run/alioth-status-ui.log
tail -80 /tmp/alioth-status-ui-c.log
```

亮度 helper：

```sh
lele-brightness status
lele-brightness dim 2 33
lele-brightness percent 25
```

## 手电

只使用 helper，不直接碰 `flashlight` aggregate 节点：

```sh
lele-torch status
lele-torch test
lele-torch on 25
lele-torch off
```

已知安全通道是 `led:torch_0` + `led:switch_0`。`torch_1/switch_1` 和 aggregate `flashlight` 曾触发 `led short/open fault detected`。

## 回 fastboot

手机侧：

```sh
reboot-bootloader
```

主机侧通过 SSH：

```bash
ssh -i /vmdata/android/redmik40/keys/alioth_usb_ed25519 \
  -o UserKnownHostsFile=/tmp/alioth_ubuntu2404_known_hosts \
  root@172.16.42.2 \
  '/var/tmp/alioth-switchroot/reboot-bootloader'
```

如果当前状态是 Ubuntu USB NCM/SSH 在线，Codex 可以直接执行上面的 SSH 命令切到 fastboot；不需要等待人工确认。若 `/vmdata/android/redmik40/locks/phone.lock` 是上一个 agent 异常退出留下的 stale lock，可以删除后继续。

## 编译 Lineage 内核

当前已验证 boot baseline 使用轻量化源码目录：

```bash
OUT_DIR=/vmdata/android/redmik40/lineage-sm8250/out/kernel-lineage20-alioth-docker \
ARTIFACT_DIR=/vmdata/android/redmik40/lineage-sm8250/artifacts/kernel-lineage20-alioth-docker \
SYSTEMD_FRIENDLY_CONFIG=yes JOBS=36 \
bash scripts/build_lineage_sm8250_alioth_kernel.sh
```

完整 LineageOS 工作区在：

```text
/vmdata/android/redmik40-lineageos/src
```

K40 相关目录：

```text
device/xiaomi/alioth
device/xiaomi/sm8250-common
kernel/xiaomi/sm8250
vendor/xiaomi/alioth
vendor/xiaomi/sm8250-common
```

该完整工作区主要用于查 device tree、vendor blobs、sepolicy、HAL 配置和后续整包构建。当前 Ubuntu boot image 的生产流程以本项目 `scripts/build_lineage_mininitramfs_boot.sh` 加 `/vmdata/android/redmik40/lineage-sm8250` 中已验证的 Lineage kernel 产物为准。

## pmOS/community mainline route

E70-E72 已证明 community postmarketOS alioth 路线可以在主机上构建 6.19.6 kernel/initramfs/boot image 和 split rootfs，但还没有做手机启动验证。当前产物在：

```text
/vmdata/android/redmik40/experiments/20260510-pmos-pmbootstrap-community/export
```

可用产物：

```text
boot.img
initramfs
vmlinuz
dtbs/sm8250-xiaomi-alioth.dtb
xiaomi-alioth-boot.img
xiaomi-alioth-root.img
```

注意：`boot.img` 的 cmdline 依赖 `pmOS_boot` 和 `pmOS_root` UUID。只执行 `fastboot boot boot.img` 不是完整系统启动，若手机没有匹配 rootfs，大概率停在 initramfs。不要使用该导出目录里的 `vendor_boot.img`、`dtbo.img`、合并 `xiaomi-alioth.img` 或 recovery zip symlink；这些不是 E72 的有效产物。

后续重新跑 pmbootstrap 时，`pmbootstrap -w` 应放在本机 SSD/高速盘，最后只把 export、日志、SHA256 和必要 distfiles 归档到 `/vmdata`。

## 构建当前 Ubuntu initramfs boot image

```bash
OUT_DIR=/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service \
INIT_MODE=switchroot-ubuntu-systemd \
UBUNTU_ROOTFS_PATH=/rootfs/ubuntu-24.04 \
UBUNTU_SYSTEMD_WRAPPER_MODE=exec \
STATUS_UI_WRAPPER_BIN=$PWD/scripts/alioth_gpu_status_ui_wrapper.sh \
bash scripts/build_lineage_mininitramfs_boot.sh
```

启动构建产物：

```bash
fastboot boot /vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/lineage-mininitramfs-boot.img
```

## 验证候选内核的 Ubuntu 用户态能力

需要验证 ModemManager、Wi-Fi、音频、GPU monitor、systemd 或 Ubuntu 服务时，必须用候选内核重建当前 Ubuntu mininitramfs boot image，而不是使用 OrangeFox ramdisk：

```bash
KERNEL_IMG=/vmdata/android/redmik40/lineage-sm8250/artifacts/<candidate>/Image \
OUT_DIR=/vmdata/android/redmik40/experiments/<run-id>/ubuntu-boot \
INIT_MODE=switchroot-ubuntu-systemd \
UBUNTU_ROOTFS_PATH=/rootfs/ubuntu-24.04 \
UBUNTU_SYSTEMD_WRAPPER_MODE=exec \
STATUS_UI_WRAPPER_BIN=$PWD/scripts/alioth_gpu_status_ui_wrapper.sh \
bash scripts/build_lineage_mininitramfs_boot.sh
```

然后只做临时启动：

```bash
fastboot boot /vmdata/android/redmik40/experiments/<run-id>/ubuntu-boot/lineage-mininitramfs-boot.img
```

`scripts/validate_alioth_kernel_candidate.sh` 会把候选内核塞进 OrangeFox ramdisk，只能作为 kernel/recovery quick smoke。它不能作为 `mmcli`、`/dev/qmi*`、systemd 服务或 Ubuntu rootfs 的验证证据。

## Modem 当前推进边界

2026-05-10 的 E17/E18 已经验证：即使用 `qmiwwan2` 候选内核临时启动真实 Ubuntu 24.04 rootfs，并确认 `CONFIG_USB_NET_QMI_WWAN=y`、`CONFIG_USB_WDM=y`、`CONFIG_RMNET_PERF=y`、`CONFIG_RMNET_SHS=y`，Ubuntu 仍没有 `/dev/qmi*`、`/dev/cdc-wdm*`、`/dev/wwan*`、`/dev/rmnet*`，`mmcli -L` 仍为 `No modems were found`。当前不要把下一步继续放在盲目 kernel config 组合上。

2026-05-10 的 E47/E48 已经验证：当前 `_b` 槽 `dtbo_b` 被有意保持为 E46 最小 RMTS UIO 补丁；配合 E45 临时 boot image 后，系统暴露 `/sys/class/uio/uio0`，临时创建设备节点 `/dev/uio0` 后 `rmt_storage` 可以打开并 mmap RMTS UIO，达到 `Done with init now waiting for messages!`，且所有 E34 备份的 modem/EFS 分区写计数保持 0。Ubuntu 仍不会自动创建 `/dev/uio0`，所以后续 helper smoke 需要临时 `mknod` 并在结束后删除。

E49 已经验证 `rmt_storage`、`tftp_server`、`pd-mapper` 三个前置 helper 可一起运行 15 秒，结束后无残留进程、无临时节点残留、E34 备份分区写计数仍为 0。E49 的 QRTR 采集用错了 Ubuntu PATH 中不存在的 `qrtr-lookup`，后续 QRTR 表必须通过 Android linker 调用 staged runtime 的：

```bash
/data/experiments/android-modem-runtime/apex/com.android.runtime/bin/linker64 \
  /data/experiments/android-modem-runtime/vendor/bin/qrtr-lookup
```

E50 已修正 QRTR 采集：Android-linker `qrtr-lookup` 能返回服务表，能看到 MODEM DIAG C/D/DATA 相关条目，但仍看不到标准 WDS/DMS/NAS。`soc:qcom,mdm0` / `esoc0` 当前是 `SDX55M`、`PCIe`、`DRIVER=ext-mdm`/`mdm-4x`，`subsys10` 仍是 `OFFLINING`。Android init 的下一层是 `vendor.mdm_launcher -> init.mdm.sh -> vendor.mdm_helper`，所以不要直接跳到 `qcrild`/`netmgrd`。

E51 已完成 `mdm_helper`、`libmdmdetect.so`、`libmdmimgload.so` staging 和 Android linker 依赖验证。E52 已完成第一次 bounded `mdm_helper` 执行：临时 `/dev/esoc-0`、`/dev/subsys_esoc0`、`/dev/uio0`、by-name 节点和 helper bundle 都可用；`mdm_helper` 会打开 `/dev/esoc-0`，一个 ESOC ioctl 成功，然后阻塞在另一个 ESOC ioctl 直到 timeout。E53 源码审计确认 E52 卡住的是 `ESOC_WAIT_FOR_REQ`，缺少的是 `/dev/subsys_esoc0` open 触发 `subsystem_get()`/`mdm_subsys_powerup()`。E54 证明该触发能进入 SDX55 MHI 但缺 `/lib/firmware/sdx55m`；E56 增加 firmware alias；E57 到达 Sahara 并暴露缺 `/vendor/bin/ks`；E58 staged 可逆 `/vendor/bin/ks` wrapper 和 `ks.real`；E59 reboot-clean；E60 证明 `rmt_storage`、`tftp_server`、`pd-mapper`、`mdm_helper`、`/dev/subsys_esoc0` 和 `ks` 能把 SDX55 临时拉到 `ONLINE`，暴露 `QMI0/QMI1/EFS/DUN/SAHARA/RMNET_CTL/IP_HW0/IPCR` 等 MHI 设备，并通过 Android-linker QRTR 看到 WDS/DMS/NAS 等标准 modem 服务。E60 所有 E34 备份分区写计数为 0，`mdmddr`/`msadp` 辅助写计数也为 0。

E60 当前 blocker 不是 ESOC/MHI/Sahara 第一阶段，而是第二次 `ks` 调用。第一次 `/vendor/bin/ks -o ... -p /dev/mhi_0306_02.01.00_pipe_2 ...` 成功进入 wrapper，再由 staged Android linker 执行 `ks.real`，并成功传输数据；第二次 `execve("/vendor/bin/ks", ["/vendor/bin/ks", "-m", "-p", "/dev/mhi_0306_02.01.00_pipe_10", "-w", "/dev/block/bootdevice/by-name/", "-t", "-1", "-l", "-g", "mdm1"], NULL)` 没有环境变量，导致 E58 动态链接 wrapper 无法找到 `libc.so`/`libdl.so`：

```text
CANNOT LINK EXECUTABLE "/vendor/bin/ks": library "libc.so" not found: needed by main executable
```

**E61 已解决此 blocker**：创建了可逆的 `/system/lib64/libc.so` 和 `/system/lib64/libdl.so` 符号链接指向 staged Android runtime bootstrap 库。`env -i /vendor/bin/ks --help` 现在可以正常运行。使用 `env -i strace -f` 确认在没有 `LD_LIBRARY_PATH` 的情况下 `execve` 传入了 0 个环境变量，linker 通过 `/system/lib64` 找到了 `libc.so`/`libdl.so`。

**E66 里程碑：首次从 Ubuntu 用户态通过 kernel QRTR 与 modem 成功 QMI 通信。** `qmicli -d qrtr://3 --dms-get-ids` 返回真实 IMEI `862951063155536`、MEID `99001855225272`、ESN `804EC186`。`--nas-get-signal-strength`、`--dms-get-manufacturer`、`--dms-get-software-version` 均可工作。kernel QRTR (`AF_QIPCRTR`) 正确桥接 MHI QMI 通道。

**关键约束**：每次 modem trigger 实验后，MHI `0306_02.01.00` 残留 sysfs 条目会阻止下一次 trigger。因此每次 trigger 前必须先 reboot-clean（fastboot boot E45）。

回滚：`rm -f /system/lib64/libc.so /system/lib64/libdl.so /system/lib64/.alioth-e61-marker && rmdir /system/lib64 2>/dev/null`

更新后的下一步是先做 reboot-clean（E62）清掉 E60 残留的 `/sys/bus/mhi/devices/0306_02.01.00`，然后按实验协议写 E63 计划并重新运行 bounded trigger。

当前 RMTS/UIO 实验现场：

```bash
fastboot boot /vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e45-ubuntu-boot-rmtfs-uio/lineage-mininitramfs-boot.img
```

RMTS/UIO `dtbo_b` 回滚：

```bash
fastboot flash dtbo_b /vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e43-dtbo-live-backup/dtbo_b-live.img
fastboot boot /vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/lineage-mininitramfs-boot.img
```

当前 modem 主线暂停在 E60 后的 handoff 点：

- 当前手机 SSH 可达；`subsys10=OFFLINE`、`crash_count=0`，无 helper/radio/`ks`/strace 进程，无临时 `/dev/uio0`、`/dev/esoc-0`、`/dev/subsys_esoc0`、`/dev/mhi*` 节点。
- 仍有 residual `/sys/bus/mhi/devices/0306_02.01.00`，下一次 trigger 前先 reboot-clean。
- 不要启动完整 `qcrild`/`netmgrd`/radio framework；先修复 `ks -m` 的无环境动态链接问题。
- 如果后续重跑 modem trigger，继续保持短时 timeout、临时 devnode、write-counter telemetry、E34 分区写计数和清理流程。

## 当前 Ubuntu initramfs 候选实验状态

实验 2 第一版：

```text
artifacts/experiments/exp2-ubuntu-systemd-first/lineage-mininitramfs-boot-exp2-ubuntu-systemd.img
sha256: 902fea6ccd762bb1bc3a68639d9479019a6b3315559e37762170909d5efdb6c4
result: fastboot boot 协议层成功，但设备进入 recovery/fallback；停止使用
```

实验 2b：

```text
artifacts/experiments/exp2b-ubuntu-minimal-pid1/out/lineage-mininitramfs-boot-exp2b-ubuntu-minimal.img
sha256: 777ec8f0e84c9a575da132abdfb9d03c33b8c8f10eaf8906c6931940f5fdc022
result: fastboot boot 协议层成功，但设备进入 MI/Xiaomi recovery；停止使用
```

结论：

```text
不要继续盲目 boot 修改过的候选。
TCP 22/2323 open 不能作为成功信号；必须拿到 SSH/2323 命令输出或 adb shell。
下一步先做离线 boot image/ramdisk 构造审计。
```

离线审计重点：

```text
原始 boot v3 header 与候选 header 对比，除 ramdisk_size 外不应有非预期差异
kernel offset、ramdisk offset、padding、总大小与原始镜像一致性
ramdisk cpio newc 格式、文件顺序、symlink、权限、设备节点
init shebang 和 /bin/busybox 路径
尽量用最小 patch 修改原始 ramdisk，或先加 pre-switch diagnostic hold 模式
```

先用仓库内离线审计脚本检查候选镜像。检查失败时不要上机 boot：

```bash
./scripts/audit_alioth_boot_image.py \
  artifacts/control/lineage-mininitramfs-boot.img \
  artifacts/experiments/<candidate>/lineage-mininitramfs-boot-*.img
```

已确认的坏候选特征：

```text
LOG-0 / exp2 / exp2b 的 ramdisk 解压后以 00070701 开头，不是预期的 newc 070701/070702。
这类候选必须视为 ramdisk 重构失败，不能作为 Ubuntu/systemd 故障证据。
```

修正版最小重包候选：

```text
artifacts/experiments/log0b-newc-marker/lineage-mininitramfs-boot-log0b-newc-marker.img
sha256: e973f2119bd1b5ea8ff8d62243cbc5d84c2fadf88c91f1d2d5136cea5f875b6c
change: only append etc/alioth-log0b-newc-marker
audit: passes Android boot v3 + gzip + newc cpio checks
result: fastboot boot 已验证可以回到 Arch/control 路径；重包流程本身成立
```

LOG-0b 后续只读 dmesg 已确认 Ubuntu path fallback 的直接原因：

```text
/dev/block/by-name/userdata 按 ext4 挂载到 /mnt/data 失败：Invalid argument
随后 initramfs 走 Arch fallback，挂载 /dev/block/by-name/arch 到 /mnt/arch
当前 Ubuntu rootfs 实际在 Arch root 的 /data/rootfs/ubuntu-24.04
```

EXP3 候选：

```text
artifacts/experiments/exp3-ubuntu-arch-rootfs-valid-newc/lineage-mininitramfs-boot-exp3-ubuntu-arch-rootfs.img
sha256: 261114e0f718df866326a034b5bfbc54f1e78ed07ad57365ca6d54cea5c11ffd
ramdisk sha256: acdc819b0f0c3dc7211e9f590373baeeaff3f237b47c16b4a1acbae54d88aeb2
change: 先试原 userdata rootfs；失败时挂载 Arch 并 bind /mnt/arch/data/rootfs/ubuntu-24.04 到 /mnt/ubuntu
audit: passes Android boot v3 + gzip + newc cpio checks; marker etc/alioth-exp3-arch-ubuntu-rootfs
result: fastboot boot 已验证进入 Ubuntu 24.04.4 LTS，PID1=systemd，root=/dev/block/by-name/arch[/data/rootfs/ubuntu-24.04]
```

EXP3 后续问题：

```text
systemctl is-system-running=degraded；失败单元是 alioth-wifi-prepare.service。
SSH 端口已起，但 /root/.ssh/authorized_keys 被 initramfs 嵌入 key redmik40-alioth-usb 覆盖；
当前 Mac 使用的是 alioth-usb-mac，所以 SSH 公钥认证失败。未获明确许可前不要直接改手机 rootfs 配置。
如果继续验证，先用 2323；需要 SSH 时再明确生成/boot 会写入目标 key 的修正版候选。
```

本机 Docker kernel 编译准备见：

```text
docs/kernel-build-local.md
docker/kernel-builder/Dockerfile
scripts/docker_kernel_shell.sh
scripts/docker_kernel_build.sh
```

## 安全提醒

- 日常测试使用 `fastboot boot`。
- 不要随手执行 `fastboot flash`、`fastboot erase`、`fastboot set_active`。
- 当前 `boot_b` 已被有意改成 Ubuntu 24.04 LTS 默认启动系统，`images/boot_b.img` 是本地保留的原 stock boot 备份。
- 当前 `dtbo_b` 已在 E47 被有意改成 E46 RMTS UIO 最小补丁；回滚命令见上面的 modem 章节。
- 如果必须刷写，先把 boot、vendor_boot、dtbo、vbmeta、super metadata 和当前启动镜像都备份并写入验证记录。
