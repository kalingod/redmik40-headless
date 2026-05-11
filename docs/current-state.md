# Current State

Last refreshed: 2026-05-11 16:25 CST.

This file is the short handoff entry. Long background remains in `project-overview.md`; detailed hardware status remains in `hardware-abi-status.md`; host-local resource availability remains in `environment.md`.

## Project position

The active direction is a Redmi K40 / POCO F3 (`alioth`) headless Linux server:

```text
Android bootloader / fastboot
-> Android boot image
-> Lineage/Android downstream 4.19 kernel
-> custom BusyBox initramfs
-> USB NCM rescue network
-> switch_root to Ubuntu 24.04 rootfs
-> Ubuntu systemd + SSH + hardware helper services
-> DRM/KMS + KGSL Vulkan status panel
```

This is not an Ubuntu generic-kernel bare-metal installation. Describe it as Ubuntu userspace/systemd on a Lineage/Android downstream kernel.

The intended product mode is not "no display at all". It is:

```text
Ubuntu server/headless mode
+ built-in screen as a fixed local status panel
+ no phone UI
+ no desktop compositor by default
+ no Wayland/X11 dependency for the panel
```

Target ownership:

```text
Ubuntu systemd PID1 = main operating system
BusyBox/initramfs = rescue and early boot only
Arch rootfs = historical fallback, not the desired main path
DRM/KMS + KGSL/Vulkan status UI = local panel service
```

## Current stable baseline from project docs

| Area | State |
|---|---|
| Device | Redmi K40 / POCO F3, codename `alioth` |
| SoC | Snapdragon 870 / SM8250-AC |
| Android slot | `_b` in the documented baseline |
| Userspace | Ubuntu 24.04.4 LTS |
| PID1 | systemd 255 |
| Kernel | Lineage/Android downstream `4.19.312-perf` |
| Rootfs | `userdata[/rootfs/ubuntu-24.04]` in the documented baseline |
| USB network | phone `172.16.42.2/24`, host `172.16.42.1/24` |
| SSH | expected on `root@172.16.42.2` |
| Display | raw DRM/KMS proven |
| GPU panel | private KGSL Turnip Vulkan + DRM/KMS page-flip proven and deployed as GPU-first status UI |
| Wi-Fi | QCA6390 bring-up and scan baseline proven |
| Audio | ADSP boot, ALSA enumeration, and first speaker playback proven |
| Touch | evdev/libinput live touch path proven |
| Bluetooth | HCI baseline only on temporary kernel path; radio scan remains blocked |
| Modem | not ready |

## Current local observation

On 2026-05-11 16:25 CST from this repository checkout:

| Item | Observation |
|---|---|
| Host | macOS 26.3.1 on Darwin 25.3.0 x86_64 |
| Repository path | `/Users/wuyuele/redmik40-headless` |
| `/vmdata` | missing on this host |
| Local control boot image | `artifacts/control/lineage-mininitramfs-boot.img`, 46 MiB |
| Local control boot image SHA256 | `19e159007fb04e009f840b09dfb33b68808ac45618403c63e80f2851e7608a23` |
| Repo-local adb | `tools/platform-tools/adb`, version `37.0.0-14910828` |
| Repo-local fastboot | `tools/platform-tools/fastboot`, version `37.0.0-14910828` |
| Device ping | `172.16.42.2` responded |
| Device SSH port | `172.16.42.2:22` was open |
| Local SSH key | no key found inside this repository |

The external boot image paths documented under `/vmdata/android/redmik40/...` were not visible from the current macOS host. Treat those paths as historical or remote-host resources until mounted or copied locally.

## Current live device observation

Read-only probes over USB network and the TCP shell on 2026-05-11 found that the live device does not match the documented Ubuntu systemd baseline.

| Item | Observed live state |
|---|---|
| USB IP | `172.16.42.2` reachable |
| SSH port | `22/tcp` open |
| SSH auth | non-interactive `root@172.16.42.2` failed with `Permission denied (publickey,password)` |
| Backup shell | `2323/tcp` open as root shell |
| Kernel | `Linux alioth-headless 4.19.312-perf #1 SMP PREEMPT Sun Apr 26 05:35:55 UTC 2026 aarch64` |
| Kernel slot suffix | `androidboot.slot_suffix=_a` |
| Root distro | Arch Linux ARM |
| Root mount | `/dev/block/by-name/arch` mounted on `/` |
| PID1 | `/var/tmp/alioth-switchroot/sh /var/tmp/alioth-switchroot/alioth-switch-init switchroot-shell` |
| systemd | not PID1; `systemctl is-system-running` returned `offline` |
| Ubuntu rootfs | present at `/data/rootfs/ubuntu-24.04`, reports Ubuntu 24.04.4 LTS |
| Ubuntu init | `/data/rootfs/ubuntu-24.04/sbin/init -> ../lib/systemd/systemd` |
| Network | `usb0=172.16.42.2/24`, `wlan0=172.23.169.148/23` |
| Battery | `100%`, `Full` |
| Local control image match | `artifacts/control/lineage-mininitramfs-boot.img` matches the first 48,373,760 bytes of phone `boot_b`, not current live `boot_a` |

Interpretation:

```text
The phone is currently in an Arch/switchroot-shell state, not the expected Ubuntu systemd state.
Ubuntu 24.04 exists on disk and is partially bind-mounted, but it has not taken over as PID1.
The active Android slot is _a, while older baseline docs often refer to _b.
The repository-local control boot image is deployed on boot_b, while the phone is currently booted from boot_a.
```

Do not apply Ubuntu-systemd runbook steps until the boot path is clarified.

## Switchroot finding

The live `_a` switchroot script is Arch-first. It does not currently contain a path that `switch_root`s into `/data/rootfs/ubuntu-24.04`.

Observed script behavior:

```text
setup Android logical partitions and vendor firmware mounts
start USB NCM, DHCP, 2323 shell
start Android qrtr-ns and cnss-daemon helpers for Wi-Fi
start sshd from the Arch root if present
start CPU fallback status UI
log "now running from Arch rootfs; mode=switchroot-shell"
if boot_mode=switchroot-systemd, exec /usr/lib/systemd/systemd from the Arch root
otherwise loop on a console shell
```

Implication:

```text
The current live _a image cannot become the desired Ubuntu-systemd-first boot without changing initramfs/switchroot logic or selecting another boot image.
The local control image matching boot_b is the first candidate to reproduce the previously documented Ubuntu path.
```

## Desired near-term state

The next target is to make the live boot path match this layout:

```text
Android bootloader
-> boot.img
-> minimal BusyBox initramfs rescue
-> USB NCM rescue path available
-> mount Android vendor/firmware and Ubuntu rootfs
-> switch_root /data/rootfs/ubuntu-24.04 /sbin/init
-> Ubuntu systemd PID1
-> sshd, Wi-Fi, time sync, audio ADSP helpers
-> alioth panel service on DRM/KMS + KGSL/Vulkan
```

The panel is a headless-server status appliance display, not a desktop:

```text
show: IP, USB, Wi-Fi, SSH, battery, charging, temperature, load, memory, disk, systemd failed count, key service states
control: optional power/menu actions through hardware keys or touch
avoid: full desktop session, persistent compositor, Android phone UI
```

The initial experiment must be read-only and answer:

```text
Why did this boot land in Arch/switchroot-shell instead of Ubuntu systemd?
Is artifacts/control/lineage-mininitramfs-boot.img this exact boot path or a different image?
What condition or script selects Arch vs Ubuntu?
What files need to change for a temporary fastboot boot into Ubuntu systemd first?
```

Answers from Phase 0:

```text
It landed in Arch because the live _a switchroot script is designed to run from Arch rootfs and was invoked with boot_mode=switchroot-shell.
artifacts/control/lineage-mininitramfs-boot.img is not the live _a image. It matches deployed boot_b by prefix hash.
The _a script has an Arch systemd branch, not an Ubuntu switch_root branch.
The next reproduction target is boot_b/control-image via temporary fastboot boot.
```

Experiment 1 result:

```text
fastboot boot artifacts/control/lineage-mininitramfs-boot.img succeeded.
USB network returned after about 5 seconds.
2323 rescue shell was reachable.
The system still booted Arch Linux ARM on /dev/block/by-name/arch.
PID1 was still alioth-switch-init switchroot-shell.
Ubuntu 24.04 rootfs was present at /data/rootfs/ubuntu-24.04 but not PID1.
systemctl returned offline.
The CPU fallback status UI started and opened DRM/KMS at 1080x2400.
```

Interpretation:

```text
The local control image/boot_b path does not reproduce Ubuntu-systemd-first mode.
The immediate blocker is initramfs/switchroot policy, not yet a proven kernel blocker.
Kernel config/code changes may still be needed later, but only after Ubuntu systemd takeover is attempted and fails with concrete symptoms.
```

Experiment 2 status:

```text
exp2 systemd-first modified ramdisk candidate: fastboot boot OK at protocol layer, then device entered recovery/fallback.
exp2b Ubuntu minimal-PID1 candidate: fastboot boot OK at protocol layer, then device entered MI/Xiaomi recovery.
Neither candidate produced a usable SSH/2323 Linux shell.
```

Current conclusion:

```text
Do not continue booting modified candidates blindly.
The next blocker is likely candidate boot image/ramdisk reconstruction compatibility or very early init failure, before kernel feature debugging is useful.
Audit the boot image and ramdisk construction first.
```

## Default operational stance

- Prefer `fastboot boot` over `fastboot flash`.
- Do not flash `vendor_boot`, `dtbo`, `vbmeta`, `super`, `modem`, bootloader, or firmware without an explicit rollback plan.
- Keep USB NCM/SSH as the rescue path.
- Do not run display experiments while the GPU status UI owns DRM/KMS unless the plan explicitly stops and restores it.
- Do not switch USB role/OTG casually; it can drop the current network path.
- Keep Ubuntu distro/userspace facts separate from kernel/firmware/vendor facts.

## Next useful setup work

1. Copy or mount the required `/vmdata/android/redmik40` resources onto the current macOS host, or update all workflows to use repository-local artifacts.
2. Add the SSH key expected by the runbook, or document the current authentication method.
3. Decide whether `artifacts/control/lineage-mininitramfs-boot.img` is the active recommended boot image or only a rollback/control image.
4. Determine why the live boot path stops at `alioth-switch-init switchroot-shell` instead of switching to Ubuntu systemd.
5. Prepare a temporary `fastboot boot` experiment for Ubuntu-systemd-first mode after the switchroot logic is understood.
6. Keep this file updated before changing boot, rootfs, network, GPU panel, Wi-Fi, audio, Bluetooth, or modem state.
