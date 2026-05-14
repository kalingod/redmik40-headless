# Session handoff: 2026-05-11

## Critical warning

This session modified macOS host networking while debugging Alioth USB/NCM.

Do not treat the later network-dependent experiment results as clean evidence.

The host network changes included attempts around:

```text
sudo ifconfig en7 ...
sudo route ...
sudo networksetup -setmanual "Alioth Linux initramfs" 172.16.42.1 255.255.255.0 172.16.42.1
sudo ipconfig set en7 DHCP
sudo networksetup -detectnewhardware
```

The user later reported the host network was damaged and had to be recovered manually.

Future agents must not run host network mutation commands without explicit, fresh approval from the user.

High-risk host commands include:

```text
sudo networksetup
sudo ifconfig
sudo route
ipconfig set
networksetup -detectnewhardware
network service enable/disable
default route or service-order changes
```

## What should be considered invalid or unproven

Do not rely on these as clean conclusions:

```text
LOG-3b automatic UDP logging is fully proven.
2323/22 failures were only caused by host routing.
Software reboot-to-fastboot is fully stable.
The Mac host-side NCM setup is correct.
Any port-open check before host routing was verified is meaningful.
```

Reason:

```text
Those observations happened after host network mutation and route/interface confusion.
They need to be re-run in a clean host network state.
```

## Facts that are still useful, but should be revalidated

These are useful leads, not final proof:

```text
The device can expose USB NCM as "Alioth Linux initramfs".
pstore/ramoops exists and /sys/fs/pstore can expose console-ramoops-0.
2323 root shell can work when the host route is correct.
Manual launch of alioth-udp-logd from 2323 produced UDP log packets.
The generated UDP log daemon can send cmdline, PID1, mounts, pstore metadata, and kmsg tail.
The current boot path seen during this session reached Arch/switchroot-shell, not Ubuntu systemd PID1.
```

## Important artifact paths from this session

```text
artifacts/experiments/log3-udp-logging/lineage-mininitramfs-boot-log3-udp-logging.img
artifacts/experiments/log3b-udp-logging-fixed/lineage-mininitramfs-boot-log3b-udp-logging-fixed.img
artifacts/experiments/log3-udp-logging/udp-5514.log
artifacts/experiments/log3-udp-logging/2323-full-collect.txt
artifacts/experiments/log3-udp-logging/2323-udp-logd-debug.txt
```

LOG-3b image:

```text
artifacts/experiments/log3b-udp-logging-fixed/lineage-mininitramfs-boot-log3b-udp-logging-fixed.img
sha256: 0e0b64c77994713258f8b1b9719b5b02f31dedf66fd39a5a44f0f214f477afee
```

New local source files added during the session:

```text
src/alioth-diag-tcp/alioth_diag_tcp.c
src/alioth-diag-tcp/alioth_diag_raw.c
src/alioth-diag-tcp/alioth_fastbootd_raw.c
src/alioth-diag-tcp/alioth_fastboot_watchdog_raw.c
src/alioth-diag-tcp/alioth_udp_logd.c
```

## Safer restart protocol for next session

Start read-only.

Do not mutate host network.

First determine current state:

```bash
fastboot devices
adb devices
networksetup -listnetworkserviceorder
networksetup -getinfo "Alioth Linux initramfs" 2>/dev/null || true
ifconfig
route -n get 172.16.42.2
netstat -rn -f inet | grep -E '172\.16\.42|default|utun|en'
```

If the device is in fastboot, prefer temporary boot only:

```bash
fastboot boot <candidate.img>
```

If the device is already in Linux, only use read-only probes first:

```bash
ping -c 1 172.16.42.2
printf 'id; uname -a; cat /proc/cmdline; exit\n' | nc -w 8 172.16.42.2 2323
```

If host route is wrong, stop and ask the user to choose whether to manually configure the Mac network. Do not fix it automatically.

## Preferred next technical direction

The next useful direction is still evidence-first boot logging, but with less host dependency:

```text
1. Prefer pstore/ramoops and device-side file logs over live host network.
2. Use USB/NCM + UDP logging only after host route is verified read-only.
3. Keep fastboot boot temporary; do not flash.
4. Use remote compile for kernel changes.
5. Treat Arch/switchroot-shell as fallback, not final goal.
6. Target remains Ubuntu systemd PID1 + headless server + local DRM/KMS/KGSL panel.
```

## 2026-05-12 continuation findings

Read-only checks from the resumed session:

```text
172.16.42.2 is reachable over USB/NCM.
22/tcp and 2323/tcp are open.
adb devices is empty.
fastboot devices is empty.
2323 shell is root.
live rootfs is Arch Linux ARM.
PID1 is alioth-switch-init from /var/tmp/alioth-switchroot/busybox.
cmdline reports androidboot.slot_suffix=_a.
usb0 is 172.16.42.2/24.
wlan0 is 172.23.169.148/23.
battery is 100% Full, temp around 32.5 C.
```

The live Arch-side `/var/tmp/alioth-switchroot/alioth-switch-init` only contains the
Arch `switchroot-shell` / `switchroot-systemd` path. It has no Ubuntu
`switchroot-ubuntu` dispatch branch. `/run/alioth-switchroot.log` confirms this
boot ran:

```text
now running from Arch rootfs; mode=switchroot-shell
```

Offline boot-image audit found a concrete construction problem in the failed
modified candidates:

```text
artifacts/control/lineage-mininitramfs-boot.img
  ramdisk cpio starts with 070701, passes newc audit

artifacts/experiments/log3b-udp-logging-fixed/lineage-mininitramfs-boot-log3b-udp-logging-fixed.img
  ramdisk cpio starts with 070701, passes newc audit

artifacts/experiments/log0-ramdisk-repack-baseline/out/lineage-mininitramfs-boot-log0-repack.img
artifacts/experiments/exp2-ubuntu-systemd-first/lineage-mininitramfs-boot-exp2-ubuntu-systemd.img
artifacts/experiments/exp2b-ubuntu-minimal-pid1/out/lineage-mininitramfs-boot-exp2b-ubuntu-minimal.img
  ramdisk cpio starts with 00070701, fails expected newc magic at offset 0
```

This makes LOG-0 / exp2 / exp2b invalid as boot candidates. Their failure is
consistent with ramdisk reconstruction corruption or incompatible cpio encoding,
not with a proven Ubuntu/systemd kernel or userspace failure.

New local verifier:

```bash
./scripts/audit_alioth_boot_image.py <boot.img> [...]
```

Run this verifier before any future `fastboot boot` candidate. A candidate must
pass boot header, gzip ramdisk, and `070701`/`070702` newc checks before it is
safe enough to test on-device.

Current SSH control path:

```bash
ssh -i /Users/wuyuele/.ssh/alioth_usb_ed25519 \
  -o UserKnownHostsFile=/tmp/alioth_usb_known_hosts_test \
  -o StrictHostKeyChecking=no \
  root@172.16.42.2
```

This was verified read-only on 2026-05-12 and returns the Arch/switchroot-shell
state. The matching public key comment is `alioth-usb-mac`.

LOG-0b corrected repack candidate:

```text
artifacts/experiments/log0b-newc-marker/lineage-mininitramfs-boot-log0b-newc-marker.img
sha256: e973f2119bd1b5ea8ff8d62243cbc5d84c2fadf88c91f1d2d5136cea5f875b6c
```

LOG-0b is based on `artifacts/control/lineage-mininitramfs-boot.img` and only
appends `etc/alioth-log0b-newc-marker` to the ramdisk. It passes the new verifier:

```text
header_version=3
header_size=1580
cmdline=twrpfastboot=1
ramdisk first8=07070100
marker=etc/alioth-log0b-newc-marker
```

LOG-0b was booted on 2026-05-12 with `fastboot boot` only; it was not flashed.
Result:

```text
fastboot boot protocol succeeded.
USB gadget enumerated as Alioth Linux initramfs.
Mac NCM IPv4 did not come back until the user opened/woke the Mac lid.
After wake, en13 returned with 172.16.42.1/24 and route to 172.16.42.2.
2323 returned command output.
SSH returned command output.
Live rootfs is still Arch Linux ARM.
PID1 is alioth-switch-init.
Run log ends with: now running from Arch rootfs; mode=switchroot-shell.
pstore is empty.
```

Interpretation:

```text
LOG-0b proves a corrected newc ramdisk repack can boot the control path.
Do not use old LOG-0 / exp2 / exp2b as failure evidence; their ramdisks were malformed.
The remaining problem is why the Ubuntu path falls back into Arch/switchroot-shell.
Next evidence target is valid-newc pre-switch diagnostics around mount_ubuntu_root/switch_to_ubuntu_root.
```

LOG-0b post-boot dmesg found the precise fallback cause:

```text
[alioth-mininit] init mode: switchroot-ubuntu-systemd
[alioth-mininit] mounting /dev/block/by-name/userdata on /mnt/data
[alioth-mininit] failed to mount data root: mount: mounting /dev/block/by-name/userdata on /mnt/data failed: Invalid argument
[alioth-mininit] Ubuntu switch_root failed; trying Arch fallback
[alioth-mininit] mounting /dev/block/by-name/arch on /mnt/arch
[alioth-mininit] mounted Arch rootfs
[alioth-mininit] switching root to /mnt/arch with mode=switchroot-shell
```

Read-only checks showed `/dev/block/by-name/userdata -> ../sda35`,
`/dev/block/by-name/arch -> ../sda37`, and Ubuntu exists under the live Arch
root at `/data/rootfs/ubuntu-24.04`.

EXP3 candidate prepared but not booted:

```text
artifacts/experiments/exp3-ubuntu-arch-rootfs-valid-newc/lineage-mininitramfs-boot-exp3-ubuntu-arch-rootfs.img
sha256: 261114e0f718df866326a034b5bfbc54f1e78ed07ad57365ca6d54cea5c11ffd
ramdisk sha256: acdc819b0f0c3dc7211e9f590373baeeaff3f237b47c16b4a1acbae54d88aeb2
```

EXP3 keeps `switchroot-ubuntu-systemd`, tries the original userdata-mounted
rootfs path first, then falls back to mounting the Arch partition and binding
`/mnt/arch/data/rootfs/ubuntu-24.04` to `/mnt/ubuntu`. It passes
`./scripts/audit_alioth_boot_image.py` with newc magic `07070100` and marker
`etc/alioth-exp3-arch-ubuntu-rootfs`.

EXP3 was booted on 2026-05-12 with `fastboot boot` only; it was not flashed.
Result:

```text
fastboot boot protocol succeeded.
macOS en13 kept 172.16.42.1/24 and route to 172.16.42.2.
2323 returned command output.
hostname is alioth-ubuntu.
PID1 is systemd.
root distro is Ubuntu 24.04.4 LTS.
root mount is /dev/block/by-name/arch[/data/rootfs/ubuntu-24.04] on /.
systemctl is-system-running returned degraded.
failed unit: alioth-wifi-prepare.service.
pstore is empty.
```

EXP3 dmesg confirmed the intended fallback:

```text
userdata mount on /mnt/data still fails with Invalid argument.
initramfs then logs: trying Arch-hosted Ubuntu rootfs fallback.
it mounts /dev/block/by-name/arch on /mnt/arch.
it binds /mnt/arch/data/rootfs/ubuntu-24.04.
it switches root to /mnt/ubuntu with static systemd wrapper PID1.
```

SSH caveat:

```text
SSH port is up, but auth failed. EXP3 copied the embedded initramfs key
redmik40-alioth-usb into Ubuntu /root/.ssh/authorized_keys. The local Mac key
used in this session is alioth-usb-mac. Use 2323 for now, or explicitly approve
booting a revised candidate that seeds the desired SSH key.
```

EXP3b was then prepared and booted with `fastboot boot` only:

```text
artifacts/experiments/exp3b-ubuntu-arch-rootfs-mac-ssh-key/lineage-mininitramfs-boot-exp3b-mac-ssh-key.img
sha256: 2d1169f84dee3c0d23d5a998093ac648adf3d431a2392c971c6f6a80d0996a10
ramdisk sha256: d3b0003d34b31c74081b5c7b9976f39dfa5ce47d3cccb8691753648037025dad
```

EXP3b keeps the EXP3 Arch-hosted Ubuntu fallback and replaces
`etc/alioth-ssh-authorized-key` with `alioth-usb-mac`. Result:

```text
fastboot boot protocol succeeded.
macOS en13 kept 172.16.42.1/24 and route to 172.16.42.2.
SSH with /Users/wuyuele/.ssh/alioth_usb_ed25519 returned command output.
hostname is alioth-ubuntu.
PID1 is systemd.
root distro is Ubuntu 24.04.4 LTS.
root mount is /dev/block/by-name/arch[/data/rootfs/ubuntu-24.04] on /.
authorized_keys contains alioth-usb-mac.
failed unit remains alioth-wifi-prepare.service.
pstore is empty.
```

Clock caveat:

```text
Live Ubuntu date after boot was Tue May 5 04:04:10 UTC 2026, stale relative to
the actual session date of 2026-05-12.
```

Operational note:

```text
After fastboot boot, macOS may show the Alioth USB NCM gadget at USB level while
172.16.42.2 is unreachable. In this session the user opening/waking the Mac lid
caused en13 and 172.16.42.1/24 to return. Do not mutate host network
configuration automatically; ask the user to perform wake/replug/network UI
actions first.
```

## First prompt for tomorrow

Use this as the first message to the AI:

```text
接手 /Users/wuyuele/redmik40-headless。先读 docs/session-handoff-2026-05-11.md。注意：上轮 AI 修改过 macOS 主机网络，导致用户网络异常，所以所有网络相关结论都要降级为未验证；不要自动执行 sudo networksetup、ifconfig、route、ipconfig set、detectnewhardware，也不要改网络服务顺序。先只读确认 fastboot/adb/当前路由/当前 Alioth Linux initramfs 网络服务状态。目标是继续 Redmi K40 alioth headless Linux 实验，后续内核改动尽量走远端编译；所有 boot 实验只用 fastboot boot，不 flash。优先建立可靠日志闭环：pstore/ramoops、2323 只读采集、必要时 UDP 日志，但必须先证明 host route 没污染。
```
