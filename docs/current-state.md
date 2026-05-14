# Current State

Last refreshed: 2026-05-14 13:03 CST.

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
| Android slot | `_a` current default; older records may mention `_b` |
| Userspace | Ubuntu 24.04.4 LTS |
| PID1 | systemd 255 |
| Kernel | Lineage/Android downstream `4.19.312-perf` |
| Rootfs | `/dev/block/by-name/userdata[/rootfs/ubuntu-24.04]` |
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

Read-only probes over USB network after a normal reboot on 2026-05-13 found
that the live device now matches the intended Ubuntu systemd baseline and boots
from the 192G userdata-backed rootfs.

| Item | Observed live state |
|---|---|
| USB IP | `172.16.42.2` reachable |
| SSH port | `22/tcp` open |
| SSH auth | `root@172.16.42.2` with `/Users/wuyuele/.ssh/alioth_usb_ed25519` works |
| Kernel | `Linux alioth-ubuntu 4.19.312-perf #1 SMP PREEMPT Mon May 11 14:11:54 UTC 2026 aarch64` |
| Kernel slot suffix | `androidboot.slot_suffix=_a` |
| Root distro | Ubuntu 24.04.4 LTS |
| Root mount | `/dev/block/by-name/userdata[/rootfs/ubuntu-24.04]` mounted on `/`, 188.8G total / 168.8G free |
| Data compatibility mount | `/dev/block/by-name/arch[/data]` mounted on `/data`, 31.4G total / 23G free |
| Large workspace | `/work` on the userdata rootfs; `/data/work -> /work` |
| PID1 | systemd 255 |
| Panel service | `lele-status-ui.service` is enabled and active, GPU-first path |
| Panel binary | `/data/experiments/alioth-status-ui-gpu-ui-runtime-v2-cpu-tuning` |
| System health | `1` failed unit currently visible on Monitor: `alioth-audio-adsp-boot.service`; `fwupd-refresh.timer` is disabled as non-essential headless noise |
| Build toolchain | `gcc/g++ 13.3.0`, `GNU Make 4.3`, `build-essential` installed |
| Thermal guard | `alioth-thermal-guard.service` enabled; monitors battery/CPU/GPU/PMIC and auto-throttles Ubuntu before kernel hard trip points |
| Compile benchmark | 384 generated C++17 files on `/work`: `-j1 27.78s`, `-j2 15.24s`, `-j4 8.85s`, `-j8 7.19s` |
| Local current boot image match | phone `boot_a` first 48,427,008 bytes matches `artifacts/experiments/exp4-touch-wifi-panel-persist/lineage-mininitramfs-boot-touch-wifi-panel.img` |

Interpretation:

```text
The phone now boots the intended Ubuntu/systemd path by default from current
slot _a. The 192G userdata partition has been reformatted as ext4 and carries
the live Ubuntu rootfs. The old Arch partition remains mounted at /data for
compatibility with existing experiment paths.

The current GPU page is a renderer/status page rather than the earlier visual
render demo. It shows the 120Hz panel mode, KGSL/DRM/ICD state, draw/GPU/present
timing, power telemetry, and the runtime paths used by the Vulkan renderer.

The Wi-Fi page now reads and updates saved SSID/password pairs in
`/etc/alioth-wifi-default`. Saved passwords are used for auto-fill and
connection, but the panel masks the password field on screen.

The `v2-paths` binary was byte-identical to `v2-wifi-save` and exists as the
first behavior-preserving source organization step: runtime paths, Wi-Fi limits,
screenshot paths, and GPU runtime paths now live in
`src/alioth-panel/panel_paths.h`.

The `v2-health` binary builds on that baseline and makes systemd health visible
on the Monitor page. It adds a top-level Health card, caches `NFailedUnits` for
short intervals, and shows failed-unit names in the Runtime section.

The `v2-health-nav` binary keeps those health features and fixes Monitor
touch hitboxes for the current layout: Battery/USB route to Power, Network
routes to Wi-Fi, Hardware routes to GPU, and the Health card no longer uses the
old Renderer/GPU hitbox.

The `v2-power-v1` binary keeps the Monitor/Wi-Fi/GPU behavior and
rebuilds the Power page into a touch-first control page. It shows real panel
backlight setpoint from `/sys/class/backlight/panel0-backlight/brightness`,
exposes screen-mode chips, exposes brightness preset/step controls, keeps
dangerous power actions behind the existing confirmation flow, and leaves
backlight unchanged until the user touches a brightness control.

The `v2-logs-v1` binary adds a fifth `LOGS` tab. The tab shows systemd
failed-unit count, key service states, Wi-Fi and audio runtime summaries, panel
metrics, and a short panel log tail. The Monitor Health card now routes to this
page, so failed service details are visible from the device panel itself.

The current `v2-cpu-tuning` binary adds CPU diagnosis and governor control.
Monitor now shows the active governor, cluster frequency summary, and hottest
thread/core so a C7 spike can be attributed from the panel. Power now has
`Eco / Auto / Perf` controls wired to
`powersave / schedutil / performance` cpufreq governors.

The phone now has a small Ubuntu-side thermal guard because the Android
LineageOS/Xiaomi user-space thermal stack is not running under systemd Ubuntu.
The guard writes `/run/alioth-thermal-guard.status`, logs to
`/var/log/alioth-thermal-guard.log`, switches to `schedutil` at warm thresholds,
switches to `powersave` at hot/critical thresholds, and powers off only at the
shutdown thresholds. Current thresholds: warm battery 40C / CPU 60C / GPU 60C /
PMIC 65C; hot battery 42C / CPU 70C / GPU 70C / PMIC 80C; shutdown battery 55C /
CPU 105C / GPU 105C / PMIC 115C. The post-compile live state was cooled back to
`schedutil`.

`fwupd-refresh.timer` has been disabled because LVFS metadata refresh is not a
useful default path for this Android-kernel phone rootfs and it was generating
repeated failed-unit noise. The remaining `alioth-audio-adsp-boot.service`
failure is real: the ADSP audio card did not appear and should stay visible
until the audio boot sequence is repaired.
```

## Switchroot finding

Historical finding before the 2026-05-13 userdata reformat:

```text
The Ubuntu-first control initramfs tried to mount /dev/block/by-name/userdata as
ext4 at /mnt/data and failed with Invalid argument because userdata contained a
stale nested GPT/PMBR signature instead of a usable ext4 filesystem.

EXP3/EXP3b worked around that by mounting the Arch partition and binding
/mnt/arch/data/rootfs/ubuntu-24.04 into /mnt/ubuntu.
```

Current state after the 2026-05-13 persistence pass:

```text
userdata is now ext4 and contains /rootfs/ubuntu-24.04, so the current _a boot
image takes the direct userdata rootfs path. The Arch-hosted Ubuntu fallback is
still useful as a recovery concept, but it is no longer the normal live path.
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
