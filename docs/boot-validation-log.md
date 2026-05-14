# Boot Validation Log

This file is the running log for boot images, slot-sensitive operations, rootfs changes, and validation checkpoints.

Use the templates in `experiment-protocol.md`.

## 2026-05-11 16:25 CST - local handoff baseline

Intent:

Record the current host-side handoff state without changing the device.

Host:

```text
macOS 26.3.1, Darwin 25.3.0 x86_64
repo: /Users/wuyuele/redmik40-headless
```

Files/images involved:

```text
artifacts/control/lineage-mininitramfs-boot.img
```

SHA256:

```text
19e159007fb04e009f840b09dfb33b68808ac45618403c63e80f2851e7608a23  artifacts/control/lineage-mininitramfs-boot.img
```

Commands actually run:

```text
host/resource inventory only
ping -c 1 172.16.42.2
nc -z -G 1 172.16.42.2 22
```

Observed output summary:

```text
/vmdata is not present on the current macOS host.
repo-local platform-tools are present.
172.16.42.2 responded to ping.
172.16.42.2:22 was open.
No SSH key was found inside this repository.
```

Device state after:

No intentional device changes were made.

Follow-up:

```text
Confirm whether artifacts/control/lineage-mininitramfs-boot.img is the active recommended boot image or only a control/rollback image.
Provide or document the SSH authentication path for root@172.16.42.2.
Mount/copy /vmdata resources if the old runbook paths remain authoritative.
```

## 2026-05-11 - USB live read-only probe

Intent:

Confirm the live device state without changing boot, rootfs, services, or partitions.

Commands actually run:

```text
ping -c 1 172.16.42.2
nc port checks for 22 and 2323
./tools/platform-tools/adb devices
./tools/platform-tools/fastboot devices
ssh -o BatchMode=yes root@172.16.42.2 ...
read-only commands over nc 172.16.42.2 2323
```

Observed output summary:

```text
172.16.42.2 ping succeeded.
22/tcp is open.
2323/tcp is open.
adb listed no devices.
fastboot listed no devices.
SSH batch auth failed: Permission denied (publickey,password).
2323/tcp accepted root shell commands.
```

Live device facts:

```text
kernel: Linux alioth-headless 4.19.312-perf #1 SMP PREEMPT Sun Apr 26 05:35:55 UTC 2026 aarch64
slot: androidboot.slot_suffix=_a
pid1: /var/tmp/alioth-switchroot/sh /var/tmp/alioth-switchroot/alioth-switch-init switchroot-shell
root distro: Arch Linux ARM
root mount: /dev/block/by-name/arch on /
systemd: offline, not PID1
usb0: 172.16.42.2/24
wlan0: 172.23.169.148/23
battery: 100%, Full
ubuntu rootfs: /data/rootfs/ubuntu-24.04 exists and reports Ubuntu 24.04.4 LTS
ubuntu init: /data/rootfs/ubuntu-24.04/sbin/init -> ../lib/systemd/systemd
```

Interpretation:

```text
The live device is currently in Arch/switchroot-shell state, not Ubuntu systemd state.
The documented Ubuntu baseline may be on another boot image/slot or the current switchroot sequence stopped before systemd takeover.
Do not run Ubuntu systemd service-management steps until the boot path is clarified.
```

Follow-up:

```text
Identify whether artifacts/control/lineage-mininitramfs-boot.img corresponds to this Arch/switchroot state.
Find the intended Ubuntu boot image for the current host, or inspect the switchroot script path that decides whether to enter Ubuntu systemd.
Resolve SSH authentication or continue using 2323 as a temporary rescue channel.
```

## Planned - Ubuntu-systemd-first headless panel experiment

Intent:

Move the project toward the intended mode:

```text
Ubuntu 24.04 systemd PID1
+ USB/SSH/Wi-Fi services
+ local DRM/KMS + KGSL/Vulkan status panel
+ no Arch main rootfs
+ no default desktop compositor
```

Phase 0 read-only questions:

```text
What does /var/tmp/alioth-switchroot/alioth-switch-init do?
Why does it currently stop at switchroot-shell?
What selects /dev/block/by-name/arch as root?
Is /data/rootfs/ubuntu-24.04 considered bootable by the script?
Is there an error log from a failed Ubuntu switch_root?
Does the local artifacts/control boot image match this live boot behavior?
```

Phase 0 planned commands:

```text
Read /var/tmp/alioth-switchroot/alioth-switch-init over 2323.
List /var/tmp/alioth-switchroot helper files.
Read /proc/cmdline and mount state.
Inspect top-level /data/rootfs/ubuntu-24.04 layout.
No writes.
```

Phase 1 candidate constraints:

```text
Use fastboot boot only.
Ubuntu rootfs must become /.
PID1 must be systemd.
USB NCM rescue remains available.
2323 or equivalent rescue remains available until SSH is proven.
Panel service must be managed by Ubuntu systemd or kept disabled for manual first start.
```

Rollback:

```text
Do not flash during Phase 0 or Phase 1.
If candidate boot fails, reboot/fastboot back to the known reachable image.
Before any flash, record current slot and rollback boot image path/SHA256.
```

## 2026-05-11 - Experiment 1 preparation: reproduce deployed boot_b/control image

Intent:

Prepare the first controlled reproduction experiment. The phone already has one deployed boot image in `boot_b`; the repository has one local control boot image. Determine whether they match and whether that image is the right candidate for Ubuntu-systemd-first reproduction.

Evidence collected:

```text
local image: artifacts/control/lineage-mininitramfs-boot.img
local image size: 48,373,760 bytes
local image sha256: 19e159007fb04e009f840b09dfb33b68808ac45618403c63e80f2851e7608a23

phone current slot from /proc/cmdline: _a
phone boot_a partition size: 201,326,592 bytes
phone boot_b partition size: 201,326,592 bytes
phone boot_a full partition sha256: 673468e85b13a59cd02cf3efca2f0db0963767eef1c8e3cf8027657983940ebc
phone boot_b full partition sha256: ef74a155b1b524665047c2412ca48ff019532f5b79a95b359fca1dcc244f5756

phone boot_a first 48,373,760 bytes sha256: b0b7e7bf44803150fb536a7ba45de488f1fad2f8068014899abc23b4916f275b
phone boot_b first 48,373,760 bytes sha256: 19e159007fb04e009f840b09dfb33b68808ac45618403c63e80f2851e7608a23
```

Conclusion:

```text
The repository-local control image exactly matches the boot_b image prefix.
The currently running _a image is different.
Experiment 1 should fastboot boot artifacts/control/lineage-mininitramfs-boot.img to reproduce the deployed boot_b path without flashing or changing slots.
```

Live `_a` switchroot finding:

```text
The live _a script runs from Arch rootfs.
It was invoked as boot_mode=switchroot-shell.
It starts USB NCM, DHCP, 2323 shell, Android vendor Wi-Fi helpers, sshd, and CPU fallback status UI.
It logs: now running from Arch rootfs; mode=switchroot-shell.
Its only systemd branch execs /usr/lib/systemd/systemd from the Arch root when boot_mode=switchroot-systemd.
It does not switch_root into /data/rootfs/ubuntu-24.04.
```

Experiment 1 planned command:

```bash
./tools/platform-tools/fastboot boot artifacts/control/lineage-mininitramfs-boot.img
```

Experiment 1 constraints:

```text
Do not flash.
Do not set-active.
Do not modify rootfs.
Only post-boot read-only checks.
```

Success criteria:

```text
Ubuntu 24.04 is mounted as /.
PID1 is systemd.
systemctl is not offline.
USB network is reachable.
SSH or 2323 rescue is reachable.
Panel/status UI behavior is visible in logs or service status.
```

Failure interpretation:

```text
If it still boots Arch/switchroot-shell, boot_b/control is not Ubuntu-systemd-first and initramfs switchroot logic must be modified.
If it boots Ubuntu but panel fails, keep the boot state unchanged and inspect systemd failed units and panel logs read-only.
```

### Experiment 1 execution result

Commands:

```bash
# via 2323 rescue shell
/var/tmp/alioth-switchroot/reboot-bootloader

./tools/platform-tools/fastboot getvar current-slot
shasum -a 256 artifacts/control/lineage-mininitramfs-boot.img
./tools/platform-tools/fastboot boot artifacts/control/lineage-mininitramfs-boot.img
```

Observed:

```text
fastboot current-slot: a
local image sha256: 19e159007fb04e009f840b09dfb33b68808ac45618403c63e80f2851e7608a23
Sending 'boot.img' (47240 KB): OKAY
Booting: OKAY
USB ping returned after about 5 seconds.
tcp/2323 was open.
tcp/22 was initially closed, later sshd was listening.
```

Post-boot state:

```text
kernel: Linux (none) 4.19.312-perf #1 SMP PREEMPT Sun Apr 26 05:35:55 UTC 2026 aarch64
cmdline slot suffix: androidboot.slot_suffix=_a
root distro: Arch Linux ARM
PID1: /var/tmp/alioth-switchroot/sh /var/tmp/alioth-switchroot/alioth-switch-init switchroot-shell
root mount: /dev/block/by-name/arch on /
systemd: offline
usb0: 172.16.42.2/24
Ubuntu rootfs: /data/rootfs/ubuntu-24.04 exists and reports Ubuntu 24.04.4 LTS
panel: CPU fallback status UI started; DRM/KMS display ready 1080x2400
```

Result:

```text
FAIL for Ubuntu-systemd-first reproduction.
PASS for proving the control image is bootable and preserves USB/2323 rescue.
```

Conclusion:

```text
The deployed boot_b/control image is still Arch-first switchroot-shell.
The next blocker is initramfs/switchroot policy.
Do not start with kernel changes until Ubuntu systemd takeover is attempted and produces kernel-specific failures.
```

## Planned - Experiment 2: Ubuntu-systemd-first initramfs candidate

Intent:

Create a candidate boot image that keeps the proven rescue setup but switches the main root to Ubuntu:

```text
switch_root /data/rootfs/ubuntu-24.04 /sbin/init
```

Candidate requirements:

```text
preserve USB NCM
preserve 2323 rescue shell until SSH works
mount proc/sys/dev/run/cgroup2 appropriately for systemd
populate required block and char device nodes
mount Android vendor/system/firmware/persist paths required by helper services
bind required paths into the Ubuntu rootfs
exec Ubuntu /sbin/init as PID1
log failures to /run/alioth-switchroot.log
fallback to rescue shell if switch_root fails
```

Validation:

```text
fastboot boot candidate image only
no flashing
no slot changes
read-only checks after boot
```

Kernel-change trigger:

```text
Only investigate kernel config/code changes after Experiment 2 produces a concrete systemd, mount, cgroup, device, DRM/KGSL, Wi-Fi, or audio kernel ABI failure.
```

### Experiment 2 first candidate result

Candidate:

```text
artifacts/experiments/exp2-ubuntu-systemd-first/lineage-mininitramfs-boot-exp2-ubuntu-systemd.img
sha256: 902fea6ccd762bb1bc3a68639d9479019a6b3315559e37762170909d5efdb6c4
```

Change:

```text
Patched initramfs mount_ubuntu_root() to search for Ubuntu under the current deployed Arch layout:
/mnt/arch/data/rootfs/ubuntu-24.04
```

Execution:

```text
fastboot boot candidate image succeeded at fastboot protocol level.
Device did not return the expected USB rescue path within the wait window.
User observed the phone entered recovery mode and manually returned it to fastboot.
```

Result:

```text
FAIL. Do not reuse this candidate.
```

Interpretation:

```text
The direct Ubuntu-systemd-first candidate was too aggressive for the next step.
Split the experiment: first prove switch_root into Ubuntu with a minimal PID1, then separately test systemd wrapper hold/exec.
```

### Experiment 2b candidate: Ubuntu minimal PID1

Candidate:

```text
artifacts/experiments/exp2b-ubuntu-minimal-pid1/out/lineage-mininitramfs-boot-exp2b-ubuntu-minimal.img
sha256: 777ec8f0e84c9a575da132abdfb9d03c33b8c8f10eaf8906c6931940f5fdc022
```

Ramdisk:

```text
artifacts/experiments/exp2b-ubuntu-minimal-pid1/out/ramdisk-exp2b.cpio.gz
sha256: 4467fb9d592f38eaae5954b95c9ed74bff59212d15fdf962731011603d4f66fb
```

Change:

```text
init mode: switchroot-ubuntu
rootfs config: /rootfs/ubuntu-24.04
systemd wrapper mode: hold
same EXP2 Arch-partition Ubuntu-rootfs fallback in mount_ubuntu_root()
```

Intent:

```text
Only prove that the boot image can switch_root into the Ubuntu 24.04 rootfs.
Do not execute Ubuntu systemd yet.
Use the existing minimal Ubuntu PID1 helper that starts USB/2323/sshd/status UI and then sleeps.
```

Current blocker:

```text
After recovery/control-image attempts, the device exposed 22/tcp and 2323/tcp but did not return a shell or SSH banner.
fastboot was not visible.
adb was not visible.
Attempting reboot-bootloader through 2323 did not bring fastboot back within 10 seconds.
Project operating note from user: the currently deployed image includes commands that can return to fastboot through SSH.
SSH command path should be tried before 2323.
Manual fastboot entry is a fallback only after SSH/2323/adb software paths fail.
Current local probe: ssh to root@172.16.42.2 timed out during banner exchange, so SSH command execution was not available in the current stuck state.
```

Planned command once fastboot is visible:

```bash
./tools/platform-tools/fastboot boot artifacts/experiments/exp2b-ubuntu-minimal-pid1/out/lineage-mininitramfs-boot-exp2b-ubuntu-minimal.img
```

Success criteria:

```text
rootfs reports Ubuntu 24.04
PID1 is alioth-ubuntu-init or the expected minimal Ubuntu PID1 helper, not systemd yet
USB/2323 or SSH is responsive
/run/alioth-ubuntu-init.log is readable
status UI behavior is visible or logged
```

### Experiment 2b execution result

Command:

```bash
./tools/platform-tools/fastboot boot artifacts/experiments/exp2b-ubuntu-minimal-pid1/out/lineage-mininitramfs-boot-exp2b-ubuntu-minimal.img
```

Observed:

```text
fastboot current-slot: a
candidate sha256: 777ec8f0e84c9a575da132abdfb9d03c33b8c8f10eaf8906c6931940f5fdc022
Sending 'boot.img' (47240 KB): OKAY
Booting: OKAY
TCP 22 and 2323 appeared open almost immediately, but neither SSH nor 2323 returned command output.
adb later showed device 43808be9 unauthorized.
User observed the device entered Xiaomi/MI recovery.
```

Result:

```text
FAIL. Do not reuse this candidate.
```

Interpretation:

```text
Both exp2 systemd-first and exp2b minimal-PID1 candidates trigger recovery/fallback.
This points to candidate boot image construction/ramdisk compatibility or very early init failure before the known USB/2323 Linux control path becomes usable.
The TCP-open signal after boot is not a valid success signal; success requires command output from SSH/2323 or adb shell authorization.
```

Next analysis direction:

```text
Stop booting modified candidates until the boot image reconstruction is audited.
Compare original and candidate boot headers byte-for-byte except ramdisk_size.
Verify boot v3 image size/layout/padding matches the original.
Verify ramdisk cpio format, device nodes, symlinks, permissions, and init shebang.
Prefer patching the original ramdisk with the smallest possible change and preserving cpio ordering/format if possible.
Consider adding a pre-switch diagnostic hold mode inside the original init path before any rootfs/systemd changes.
```

### LOG-0b corrected newc marker execution

Candidate:

```text
artifacts/experiments/log0b-newc-marker/lineage-mininitramfs-boot-log0b-newc-marker.img
sha256: e973f2119bd1b5ea8ff8d62243cbc5d84c2fadf88c91f1d2d5136cea5f875b6c
```

Change:

```text
Base: artifacts/control/lineage-mininitramfs-boot.img
Only appended etc/alioth-log0b-newc-marker to the initramfs.
Ramdisk keeps newc magic 070701 at offset 0.
```

Preflight:

```text
./scripts/audit_alioth_boot_image.py passed:
header_version=3
header_size=1580
cmdline=twrpfastboot=1
ramdisk first8=07070100
marker etc/alioth-log0b-newc-marker present
```

Command:

```bash
ssh -i /Users/wuyuele/.ssh/alioth_usb_ed25519 root@172.16.42.2 \
  '/var/tmp/alioth-switchroot/alioth-reboot bootloader'

./tools/platform-tools/fastboot boot \
  artifacts/experiments/log0b-newc-marker/lineage-mininitramfs-boot-log0b-newc-marker.img
```

Observed:

```text
fastboot devices showed 43808be9.
Sending boot.img OKAY.
Booting OKAY.
USB device enumerated as Alioth Linux initramfs.
Initially macOS had the NCM gadget visible at USB level but no usable IPv4 interface.
User opened/woke the Mac lid; en13 reappeared with 172.16.42.1/24.
route to 172.16.42.2 returned to en13.
2323 returned command output.
SSH returned command output.
```

Live state after LOG-0b boot:

```text
kernel: Linux 4.19.312-perf #1 SMP PREEMPT Sun Apr 26 05:35:55 UTC 2026 aarch64
slot: androidboot.slot_suffix=_a
PID1: alioth-switch-init
rootfs: Arch Linux ARM on /dev/block/sda37
run log: now running from Arch rootfs; mode=switchroot-shell
pstore: empty
```

Result:

```text
PASS for corrected newc ramdisk repack baseline.
The corrected minimal repack does not trigger recovery/fallback.
The previous LOG-0/exp2/exp2b failures are therefore consistent with bad ramdisk cpio encoding.
```

Interpretation:

```text
The local boot image packing path can work when the ramdisk remains valid newc.
The remaining blocker is not generic boot image packing; it is why the initramfs Ubuntu path falls back into Arch/switchroot-shell.
Next step: add or capture pre-switch initramfs diagnostics around mount_ubuntu_root/switch_to_ubuntu_root using a valid newc ramdisk.
```

### LOG-0b post-boot root-cause finding

Read-only dmesg from the LOG-0b boot showed the concrete fallback reason:

```text
[alioth-mininit] init mode: switchroot-ubuntu-systemd
[alioth-mininit] usb ncm ready: host should get 172.16.42.1/24 by DHCP, then nc 172.16.42.2 2323
[alioth-mininit] mounting /dev/block/by-name/userdata on /mnt/data
[alioth-mininit] failed to mount data root: mount: mounting /dev/block/by-name/userdata on /mnt/data failed: Invalid argument
[alioth-mininit] Ubuntu switch_root failed; trying Arch fallback
[alioth-mininit] mounting /dev/block/by-name/arch on /mnt/arch
[alioth-mininit] mounted Arch rootfs
[alioth-mininit] switching root to /mnt/arch with mode=switchroot-shell
```

Read-only filesystem checks after fallback showed:

```text
/dev/block/by-name/userdata -> ../sda35
/dev/block/by-name/arch -> ../sda37
/data/rootfs/ubuntu-24.04 exists under the live Arch rootfs
/data/rootfs/ubuntu-24.04 reports Ubuntu 24.04.4 LTS
```

Interpretation:

```text
The control initramfs tries to mount userdata directly as ext4 and then bind
/mnt/data/rootfs/ubuntu-24.04. On the current deployed layout, Ubuntu is under
the Arch partition at /data/rootfs/ubuntu-24.04, so the direct userdata mount
fails before Ubuntu can become PID1.
```

### EXP3 valid-newc Arch-hosted Ubuntu rootfs candidate

Candidate:

```text
artifacts/experiments/exp3-ubuntu-arch-rootfs-valid-newc/lineage-mininitramfs-boot-exp3-ubuntu-arch-rootfs.img
sha256: 261114e0f718df866326a034b5bfbc54f1e78ed07ad57365ca6d54cea5c11ffd
ramdisk sha256: acdc819b0f0c3dc7211e9f590373baeeaff3f237b47c16b4a1acbae54d88aeb2
```

Change:

```text
Keep switchroot-ubuntu-systemd and the configured /rootfs/ubuntu-24.04 path.
Try the original userdata-mounted rootfs path first.
If userdata mount/path fails, mount the Arch partition and bind:
  /mnt/arch/data/rootfs/ubuntu-24.04 -> /mnt/ubuntu
```

Preflight:

```text
./scripts/audit_alioth_boot_image.py passed:
header_version=3
header_size=1580
cmdline=twrpfastboot=1
ramdisk first8=07070100
marker etc/alioth-exp3-arch-ubuntu-rootfs present
```

Execution status:

```text
Prepared for boot; execution result recorded below.
Test only with fastboot boot. Do not flash.
After boot, if macOS sees the USB gadget but 172.16.42.2 is unreachable, do not
mutate host network configuration automatically; ask for wake/replug/UI action.
```

Planned execution:

```bash
ssh -i /Users/wuyuele/.ssh/alioth_usb_ed25519 root@172.16.42.2 \
  '/var/tmp/alioth-switchroot/alioth-reboot bootloader'

./tools/platform-tools/fastboot boot \
  artifacts/experiments/exp3-ubuntu-arch-rootfs-valid-newc/lineage-mininitramfs-boot-exp3-ubuntu-arch-rootfs.img
```

Success criteria:

```text
SSH or 2323 must return command output.
Then confirm kernel, PID1, root mount, /etc/os-release, switchroot log, and pstore.
Port-open alone is not success.
```

Execution:

```text
SSH from LOG-0b/Arch state ran /var/tmp/alioth-switchroot/alioth-reboot bootloader.
The SSH process hung after the device switched away; local hung ssh was killed.
fastboot devices showed 43808be9.
fastboot boot EXP3 succeeded:
  Sending boot.img OKAY
  Booting OKAY
```

Observed after boot:

```text
macOS route to 172.16.42.2 stayed on en13.
en13 had 172.16.42.1/24 and status active.
SSH port responded but rejected the local key.
2323 returned command output.
```

EXP3 live state from 2323:

```text
hostname: alioth-ubuntu
kernel: Linux alioth-ubuntu 4.19.312-perf #1 SMP PREEMPT Sun Apr 26 05:35:55 UTC 2026 aarch64
PID1: systemd
root distro: Ubuntu 24.04.4 LTS
root mount: /dev/block/by-name/arch[/data/rootfs/ubuntu-24.04] on /
usb0: 172.16.42.2/24
default route: via 172.16.42.1 dev usb0
pstore: empty
systemctl is-system-running: degraded
failed unit: alioth-wifi-prepare.service
```

EXP3 dmesg confirms the new fallback path worked:

```text
[alioth-mininit] failed to mount data root: mount: mounting /dev/block/by-name/userdata on /mnt/data failed: Invalid argument
[alioth-mininit] trying Arch-hosted Ubuntu rootfs fallback
[alioth-mininit] mounting /dev/block/by-name/arch on /mnt/arch
[alioth-mininit] mounted Arch rootfs
[alioth-mininit] binding Ubuntu rootfs from arch: /mnt/arch/data/rootfs/ubuntu-24.04
[alioth-mininit] switching root to /mnt/ubuntu with static systemd wrapper PID1: /lib/systemd/systemd
```

Result:

```text
PASS for the mount/rootfs/PID1 target.
EXP3 proves a valid-newc initramfs can boot Ubuntu 24.04 with systemd PID1 from
the current Arch-hosted /data/rootfs/ubuntu-24.04 layout.
```

Follow-up issues:

```text
SSH auth failed because EXP3 copied the embedded initramfs key
redmik40-alioth-usb into Ubuntu /root/.ssh/authorized_keys, while the local
test key is alioth-usb-mac. Do not silently modify phone rootfs config; either
use 2323 or boot a revised candidate that intentionally seeds the desired key.

systemd is degraded because alioth-wifi-prepare.service failed. Its log ended
after mounting modem_b firmware and then leaving /vendor/firmware_mnt untouched
because it already exists and is not a symlink.

The live Ubuntu clock read Tue May 5 04:04:10 UTC 2026 after boot, which is
stale relative to the real session date, May 12 2026.
```

### EXP3b Mac SSH key candidate

Candidate:

```text
artifacts/experiments/exp3b-ubuntu-arch-rootfs-mac-ssh-key/lineage-mininitramfs-boot-exp3b-mac-ssh-key.img
sha256: 2d1169f84dee3c0d23d5a998093ac648adf3d431a2392c971c6f6a80d0996a10
ramdisk sha256: d3b0003d34b31c74081b5c7b9976f39dfa5ce47d3cccb8691753648037025dad
```

Change from EXP3:

```text
Keep the validated Arch-hosted Ubuntu rootfs fallback.
Replace etc/alioth-ssh-authorized-key with the current Mac key:
  alioth-usb-mac
```

Preflight:

```text
./scripts/audit_alioth_boot_image.py passed:
header_version=3
header_size=1580
cmdline=twrpfastboot=1
ramdisk first8=07070100
marker etc/alioth-exp3b-mac-ssh-key present
```

Planned execution:

```bash
printf '/var/tmp/alioth-switchroot/alioth-reboot bootloader\n' | nc -w 5 172.16.42.2 2323

./tools/platform-tools/fastboot boot \
  artifacts/experiments/exp3b-ubuntu-arch-rootfs-mac-ssh-key/lineage-mininitramfs-boot-exp3b-mac-ssh-key.img
```

Success criteria:

```text
SSH with /Users/wuyuele/.ssh/alioth_usb_ed25519 must return command output.
Confirm Ubuntu 24.04, PID1 systemd, root mount, authorized_keys, failed units, and pstore.
Do not flash.
Do not mutate host network configuration automatically if the USB interface stalls.
```

Execution result:

```text
fastboot devices showed 43808be9.
fastboot boot EXP3b succeeded:
  Sending boot.img OKAY
  Booting OKAY
macOS route to 172.16.42.2 stayed on en13.
en13 had 172.16.42.1/24 and status active.
SSH with /Users/wuyuele/.ssh/alioth_usb_ed25519 returned command output.
```

EXP3b live state from SSH:

```text
hostname: alioth-ubuntu
kernel: Linux alioth-ubuntu 4.19.312-perf #1 SMP PREEMPT Sun Apr 26 05:35:55 UTC 2026 aarch64
PID1: systemd
root distro: Ubuntu 24.04.4 LTS
root mount: /dev/block/by-name/arch[/data/rootfs/ubuntu-24.04] on /
authorized_keys: alioth-usb-mac
pstore: empty
systemctl is-system-running: starting
failed unit: alioth-wifi-prepare.service
```

Result:

```text
PASS for SSH key repair and Ubuntu/systemd control path.
EXP3b is the current best temporary-boot candidate. It is still not flashed.
Remaining independent issue: alioth-wifi-prepare.service failure.
```

## 2026-05-13 10:31 CST - KGSL firmware staging smoke on current boot_a Ubuntu

Intent:

```text
Understand current GPU readiness on the persisted boot_a Ubuntu state before
attempting any DRM/KMS or Vulkan panel takeover. First check the remote build
machine for existing GPU runtime artifacts and logs, then run the lowest-risk
KGSL open/getproperty smoke on the phone.
```

Host:

```text
Mac control host: /Users/wuyuele/redmik40-headless
Remote build/reference host: lele@47.98.228.151:60022
Phone SSH: root@172.16.42.2 with /Users/wuyuele/.ssh/alioth_usb_ed25519
```

Device baseline assumed:

```text
USB route to 172.16.42.2 is en13 / Alioth Linux initramfs.
Phone is Ubuntu 24.04.4 LTS with systemd PID1.
lele-status-ui.service is active and currently runs CPU UI.
/dev/dri/card0, /dev/dri/renderD128, and /dev/kgsl-3d0 exist.
```

Files/images involved:

```text
No boot image, partition image, or flash operation.
Read-only reference paths on remote:
  /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness
  /vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/gpu-monitor-runtime-status-wifi
Phone-side temporary/helper path:
  /tmp/alioth_kgsl_getprop_probe.py
Phone-side firmware staging path:
  /lib/firmware/a650_*
```

SHA256:

```text
Remote gpu-monitor-runtime-status-wifi helper:
  3debe383f33352dabf588a8549f4e26d1976243f01077c9aea961d24f83fcfa7
Remote private KGSL Turnip ICD:
  not fetched yet in this step
```

Commands planned:

```text
1. SSH read-only inspect remote build/reference paths for GPU artifacts.
2. Copy scripts/alioth_kgsl_getprop_probe.py to /tmp on the phone.
3. Run /tmp/alioth_kgsl_getprop_probe.py before firmware staging.
4. If open fails due missing a650_sqe.fw, stage only A650 firmware by copying
   or symlinking existing /vendor/firmware/a650_* files into /lib/firmware.
5. Re-run /tmp/alioth_kgsl_getprop_probe.py.
6. Confirm lele-status-ui.service remains active.
```

Expected success signal:

```text
KGSL open succeeds and GETPROPERTY reports drv/dev version, derived GPU 650,
GMEM, 48-bit device bitness, and UBWC mode.
```

Expected failure signal:

```text
open(/dev/kgsl-3d0) continues to fail, dmesg reports missing firmware, or KGSL
GETPROPERTY returns ioctl errors.
```

Rollback/recovery:

```text
No service stop is planned. If firmware staging causes trouble, remove only the
new /lib/firmware/a650_* entries created in this experiment and re-run the CPU
status UI health check. Do not alter /vendor/firmware or boot partitions.
```

Stop conditions:

```text
USB/NCM route stops using en13, SSH fails, lele-status-ui.service becomes
inactive unexpectedly, or any command would write boot/vendor/dtbo/vbmeta/super
or alter host networking.
```

Result:

```text
PASS for KGSL firmware staging/getproperty on current boot_a Ubuntu.
PASS for private KGSL Turnip no-op Vulkan submit using restored runtime files.
No DRM/KMS takeover was attempted.
lele-status-ui.service remained active.
systemd remained degraded due pre-existing alioth-audio-adsp-boot.service and
fwupd-refresh.service failures.
```

Commands actually run:

```text
Remote records inspected:
  /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/kgsl-firmware-getproperty-smoke.txt
  /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-vulkaninfo-smoke.txt
  /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-noop-submit.txt
  /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-prime-kgsl-monitor-pageflip.txt
  /vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/gpu-monitor-runtime-status-wifi/deploy-default.txt

Transferred selected remote runtime/reference files to:
  artifacts/remote/gpu-runtime-20260513/

Staged phone runtime under:
  /data/experiments/gpu-runtime-20260513
  /data/experiments/mesa-kgsl-prefix/lib/aarch64-linux-gnu/libvulkan_freedreno.so

Staged A650 firmware by symlinking:
  /lib/firmware/a650_* -> /vendor/firmware/a650_*

Ran:
  /tmp/alioth_kgsl_getprop_probe.py
  LD_LIBRARY_PATH=/data/experiments/gpu-runtime-20260513/lib:/data/experiments/mesa-kgsl-prefix/lib/aarch64-linux-gnu \
  VK_ICD_FILENAMES=/data/experiments/gpu-runtime-20260513/freedreno_icd-kgsl-mesa2034-aarch64.json \
  TU_DEBUG=startup \
  /data/experiments/gpu-runtime-20260513/alioth_vulkan_noop_submit
```

Observed output summary:

```text
KGSL before staging failed exactly as old records predicted:
  request_firmware(a650_sqe.fw) failed: -2

After firmware symlinks:
  open /dev/kgsl-3d0: OK
  KGSL_PROP_VERSION: drv=3.14 dev=3.1
  KGSL_PROP_DEVICE_INFO: derived_gpu_id=650 gmem_sizebytes=1179648
  KGSL_PROP_DEVICE_BITNESS: 48
  KGSL_PROP_UBWC_MODE: 4
  overall=PASS

Private Turnip no-op submit:
  physical_device=FD650 api=1.2.131 driver=83898372 type=INTEGRATED_GPU
  queue_family[0] flags=0x7 count=1
  noop_submit=PASS queue_family=0
```

Device state after:

```text
lele-status-ui.service: active
systemctl is-system-running: degraded
failed units:
  alioth-audio-adsp-boot.service
  fwupd-refresh.service
```

Artifacts produced:

```text
Local:
  artifacts/remote/gpu-runtime-20260513/alioth_vulkan_noop_submit
  artifacts/remote/gpu-runtime-20260513/alioth_gpu_monitor_service
  artifacts/remote/gpu-runtime-20260513/libvulkan_freedreno-kgsl-mesa2034-arm64.so
  artifacts/remote/gpu-runtime-20260513/freedreno_icd-kgsl-mesa2034-aarch64.json
  artifacts/remote/gpu-runtime-20260513/lib/

Phone:
  /data/experiments/gpu-runtime-20260513/
  /data/experiments/mesa-kgsl-prefix/lib/aarch64-linux-gnu/libvulkan_freedreno.so
  /lib/firmware/a650_* symlinks
```

Follow-up:

```text
The remote records show display/pageflip tests need
/data/experiments/alioth_vulkan_glyph_text.vert.spv and
/data/experiments/alioth_vulkan_glyph_text.frag.spv. The current phone boot
does not have those SPIR-V files or glslangValidator, and the remote records
only preserve hashes/output logs, not the SPIR-V payloads. Before a DRM/KMS
takeover, either recover those SPIR-V files from an older phone/rootfs backup or
reinstall/use glslang-tools to regenerate them from the repo GLSL sources.
```

Source cross-check:

```text
Remote Android/Lineage source roots inspected:
  /vmdata/android/redmik40-lineageos/src
  /home/lele/lineageos
  /home/lele/redmik40-build/lineage-sm8250/kernel

Lineage vendor packaging confirms A650 firmware is an Android vendor artifact:
  vendor/xiaomi/sm8250-common/sm8250-common-vendor.mk copies a650_gmu.bin,
  a650_sqe.fw, and a650_zap.* into $(TARGET_COPY_OUT_VENDOR)/firmware.

Lineage ueventd confirms the Android firmware/device-node assumptions:
  rootdir/etc/ueventd.qcom.rc has firmware_directories
  /vendor/firmware_mnt/image/ and world-writable /dev/kgsl-3d0 policy for
  Android userspace.

Kernel UAPI confirms the KGSL ioctl layer used by the Ubuntu probes:
  include/uapi/linux/msm_kgsl.h defines KGSL_CONTEXT_TYPE_VK,
  KGSL_PROP_DEVICE_INFO, KGSL_PROP_VERSION, KGSL_PROP_UBWC_MODE,
  IOCTL_KGSL_DEVICE_GETPROPERTY, IOCTL_KGSL_GPUOBJ_IMPORT, and
  IOCTL_KGSL_GPU_COMMAND.

Lineage Mesa confirms this is not the stock Ubuntu DRM render-node path:
  external/mesa3d/src/freedreno/vulkan/tu_kgsl.c opens /dev/kgsl-3d0,
  reads KGSL properties, imports dma-buf objects through
  IOCTL_KGSL_GPUOBJ_IMPORT, and submits IB lists through
  IOCTL_KGSL_GPU_COMMAND.

Conclusion:
  Android proprietary EGL/GLES/Vulkan blobs and libgsl are useful references
  but not a drop-in Ubuntu/glibc path. The practical Ubuntu route remains the
  private KGSL-enabled Turnip ICD plus custom DRM/KMS scanout, not SurfaceFlinger
  or normal GBM/EGL/WSI.
```

## 2026-05-13 10:50 CST - DRM PRIME KGSL glyph pageflip smoke on current boot_a Ubuntu

Intent:

```text
Regenerate the missing glyph SPIR-V payloads from the repo GLSL sources and run
a bounded, service-restored custom GPU/KMS pageflip test on the current Ubuntu
boot without changing boot/vendor partitions or the default status UI unit.
```

Files involved:

```text
Shader sources:
  scripts/alioth_vulkan_glyph_text.vert
  scripts/alioth_vulkan_glyph_text.frag

Generated on remote through an ephemeral ubuntu:24.04 Docker container with
glslang-tools:
  artifacts/remote/gpu-runtime-20260513/alioth_vulkan_glyph_text.vert.spv
  artifacts/remote/gpu-runtime-20260513/alioth_vulkan_glyph_text.frag.spv

Phone paths:
  /data/experiments/alioth_vulkan_glyph_text.vert.spv
  /data/experiments/alioth_vulkan_glyph_text.frag.spv
  /data/experiments/gpu-runtime-20260513/alioth_gpu_monitor_service
```

SHA256:

```text
0d6f9fbc264a52e45b655eb0bbc48e8c3ae3e446512f3965239b0c5162fe5ed4  alioth_vulkan_glyph_text.vert.spv
11693586ef71adb83309fe2db879887a8052bdc1eb45c57dbd1ac7ce41ddc081  alioth_vulkan_glyph_text.frag.spv
```

Commands actually run:

```text
1. Copied repo GLSL sources to remote build host.
2. Ran glslangValidator inside remote ubuntu:24.04 Docker to produce SPIR-V.
3. Copied SPIR-V to local artifacts/remote/gpu-runtime-20260513/.
4. Copied SPIR-V to /data/experiments/ on the phone.
5. Verified service state, stopped lele-status-ui.service, ran:

   LD_LIBRARY_PATH=/data/experiments/gpu-runtime-20260513/lib:/data/experiments/mesa-kgsl-prefix/lib/aarch64-linux-gnu
   VK_ICD_FILENAMES=/data/experiments/gpu-runtime-20260513/freedreno_icd-kgsl-mesa2034-aarch64.json
   TU_DEBUG=startup
   timeout 25 /data/experiments/gpu-runtime-20260513/alioth_gpu_monitor_service 3 \
     /data/experiments/alioth_vulkan_glyph_text.vert.spv \
     /data/experiments/alioth_vulkan_glyph_text.frag.spv

6. Used a shell EXIT trap to restart lele-status-ui.service.
```

Observed output summary:

```text
pre_service=active
stopped_service=inactive
drm_target connector=29 crtc=129 mode=1080x2400@60 flips=3 service_mode=no
TU: info: Found compatible device '/dev/kgsl-3d0'.
physical_device=FD650 api=1.2.131 driver=83898372
texture_upload=PASS
descriptor_set=PASS
shader_words vert=288 frag=251
imported_fb=196 memory_type=0 flags=0x7
imported_fb=197 memory_type=0 flags=0x7
initial_set_crtc=PASS fb=196
glyph_sample_pass=3/3 frame=0
glyph_sample_pass=3/3 frame=1
glyph_sample_pass=3/3 frame=2
glyph_sample_pass=3/3 frame=3
page_flip_event seen=1
page_flip_event seen=2
page_flip_event seen=3
drm_prime_kgsl_glyph_text_pageflip=PASS submitted=3 events=3
helper_rc=0
```

Device state after:

```text
lele-status-ui.service: active
running process: /var/tmp/alioth-switchroot/alioth-status-ui-cpu
systemctl is-system-running: degraded
failed units unchanged:
  alioth-audio-adsp-boot.service
  fwupd-refresh.service
```

Result:

```text
PASS for regenerated shader payloads.
PASS for DRM PRIME buffer import into private KGSL Turnip.
PASS for glyph text rendering and 3/3 KMS page-flip events.
The default status service was restored to the existing CPU UI path.
No default GPU service deployment was attempted.
```

## 2026-05-13 10:57 CST - GPU-first status UI service cutover

Intent:

```text
Replace the default CPU-rendered local status UI path with the private KGSL
Turnip + DRM/KMS GPU monitor path to avoid the CPU UI touch-triggered screen
flash behavior. Keep a CPU fallback and a simple systemd drop-in rollback.
```

Files involved:

```text
Local wrapper source:
  scripts/alioth_gpu_status_ui_wrapper.sh

Phone deployed wrapper:
  /var/tmp/alioth-switchroot/alioth_gpu_status_ui_wrapper.sh
  /data/experiments/alioth_gpu_status_ui_wrapper.sh

Phone systemd drop-in:
  /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf

Phone unit backup:
  /data/experiments/backups/lele-status-ui.service.pre-gpu-first-20260513-025807
```

Wrapper change:

```text
The wrapper now accepts the restored 2026-05-13 runtime layout:
  /data/experiments/gpu-runtime-20260513/alioth_gpu_monitor_service
  /data/experiments/gpu-runtime-20260513/freedreno_icd-kgsl-mesa2034-aarch64.json
  /data/experiments/gpu-runtime-20260513/lib
  /data/experiments/mesa-kgsl-prefix/lib/aarch64-linux-gnu

It keeps CPU fallback through:
  /var/tmp/alioth-switchroot/alioth-status-ui-cpu
```

Pre-cutover wrapper dry-run:

```text
Stopped lele-status-ui.service, ran the wrapper under timeout for 8 seconds,
then restored the service through a shell EXIT trap.

Observed:
  starting GPU monitor service
  TU: info: Found compatible device '/dev/kgsl-3d0'
  texture_upload=PASS
  descriptor_set=PASS
  initial_set_crtc=PASS
  input_fds=6
  gpu_monitor_service_stop submitted=8 events=8 stop_requested=1
  lele-status-ui.service returned active
```

Systemd cutover:

```text
Created drop-in:
  [Service]
  ExecStart=
  ExecStart=/data/experiments/alioth_gpu_status_ui_wrapper.sh

Ran:
  systemctl daemon-reload
  systemctl restart lele-status-ui.service
```

Observed after cutover:

```text
lele-status-ui.service: active
NRestarts=0
ActiveEnterTimestamp=Wed 2026-05-13 03:00:52 UTC

Running processes:
  /bin/sh /data/experiments/alioth_gpu_status_ui_wrapper.sh
  /data/experiments/gpu-runtime-20260513/alioth_gpu_monitor_service monitor \
    /data/experiments/alioth_vulkan_glyph_text.vert.spv \
    /data/experiments/alioth_vulkan_glyph_text.frag.spv

Runtime log:
  starting GPU monitor service
  drm_target connector=29 crtc=129 mode=1080x2400@60 flips=0 service_mode=yes
  texture_upload=PASS
  descriptor_set=PASS
  imported_fb=196 memory_type=0 flags=0x7
  imported_fb=197 memory_type=0 flags=0x7
  initial_set_crtc=PASS fb=196
  input_fds=6
  monitor_vertices_refreshed frame=10 vertices=468
  kgsl_glyph_text_render frame=60 vertices=468
  page_flip_event seen=60

dmesg tail contained no new DRM/KGSL/GPU fault, timeout, or error lines.
Failed systemd units remained the pre-existing:
  alioth-audio-adsp-boot.service
  fwupd-refresh.service
```

Touch behavior note:

```text
The previous CPU UI log includes touch events and page changes immediately
before the GPU cutover. After the cutover, the active renderer is the GPU
monitor service; it keeps page-flipping through DRM/KMS and has not restarted
or fallen back to CPU in the observed window.

The GPU helper opens touch/key input devices, but the current service path only
uses key-style input for page changes. Plain touch should therefore no longer
drive the old CPU redraw path that caused visible flashing.
```

Rollback:

```text
rm -f /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
systemctl daemon-reload
systemctl restart lele-status-ui.service

Expected rollback process:
  /var/tmp/alioth-switchroot/alioth-status-ui-cpu
```

Result:

```text
PASS for GPU-first status UI service cutover.
PASS for systemd/service stability after cutover.
Visual touch-flash improvement still needs human screen confirmation, but the
running service is no longer the CPU status UI path.
```

## 2026-05-13 11:13 CST - GPU monitor screenshot request support

Intent:

```text
Restore the panel screenshot workflow after switching from the old CPU UI to
the private KGSL Turnip GPU monitor service.
```

Change:

```text
Added screenshot request handling to:
  scripts/alioth_drm_prime_vulkan_glyph_text_pageflip.c

The GPU monitor now watches:
  /run/alioth-panel-screenshot.request

and writes:
  /run/alioth-panel-screenshot.bmp
  /run/alioth-panel-screenshot.txt

The BMP is written from the currently scanned-out DRM dumb buffer, using the
mapped BGRA/XRGB buffer as the source and emitting a 24-bit 1080x2400 BMP.
```

Build/deploy:

```text
Built on remote with redmik40-kernel-builder:bullseye and the Ubuntu 24.04
arm64 GPU monitor sysroot.

New helper:
  artifacts/remote/gpu-runtime-20260513/alioth_gpu_monitor_service.screenshot
  sha256: 0af1e6f33ff3c334e2313ff4b7a3ed5219c306c6d95553cb4372f529dd569a0e

Deployed to:
  /data/experiments/gpu-runtime-20260513/alioth_gpu_monitor_service

Previous helper was backed up in the same directory with a pre-screenshot
timestamp suffix.
```

Screenshot result:

```text
Request:
  touch /run/alioth-panel-screenshot.request

Device output:
  screenshot_write=PASS path=/run/alioth-panel-screenshot.bmp fb=196 index=0 events=0

Info file:
  width=1080
  height=2400
  pitch=4352
  fb=196
  buffer_index=0
  events=0

Local copies:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot.bmp
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot.png

Local BMP sha256:
  06833e6a2156a620f0828d2fab7043a4b7e8f7542ea3a9efcb9356aed39cdc84
```

Observed visual content:

```text
The captured panel is a black background with large pixel-font status text:
  ALIOTH RUNTIME
  SYSTEMD DEGRADED FAIL 2
  BAT 98 CHARGING
  USB0 UP
  WIFI SCAN READY
  AUDIO OFFLINING SND 0
```

Service state after:

```text
lele-status-ui.service: active
NRestarts=0
Running processes:
  /bin/sh /data/experiments/alioth_gpu_status_ui_wrapper.sh
  /data/experiments/gpu-runtime-20260513/alioth_gpu_monitor_service monitor ...
```

Result:

```text
PASS for GPU monitor screenshot support.
PASS for service stability after deploying the screenshot-capable helper.
```

## 2026-05-13 11:22 CST - GPU monitor page-mode correction

Intent:

```text
Correct the first GPU-first panel content mismatch. The initial GPU monitor was
only a simple runtime text proof, while the expected panel concept has monitor,
Wi-Fi, power, and system/status pages.
```

Change:

```text
Updated scripts/alioth_drm_prime_vulkan_glyph_text_pageflip.c:
  - expanded the rendered text model from 6 to 9 lines
  - renamed the default page to MONITOR
  - added a WIFI page with wlan0 mode/link summary
  - added POWER and SYSTEM pages
  - added /run/alioth-panel-page request handling:
      monitor/status/0
      wifi/1
      power/2
      system/details/3
      next/prev
      menu
  - kept screenshot request support

This is still not full CPU panel parity: the Wi-Fi soft keyboard, scan result
selection, and connect buttons remain CPU-panel-only for now.
```

Build/deploy:

```text
Built on remote with redmik40-kernel-builder:bullseye and the Ubuntu 24.04
arm64 GPU monitor sysroot.

New helper:
  artifacts/remote/gpu-runtime-20260513/alioth_gpu_monitor_service.pages2
  sha256: 9683e2a5727723bc1fe77b7fbca4c99ef204aa3f101b695aafcfe64d9b5be582

Deployed to:
  /data/experiments/gpu-runtime-20260513/alioth_gpu_monitor_service

Previous helpers were backed up in the same directory with pre-pages and
pre-pages2 timestamp suffixes.
```

Validation:

```text
Service state:
  lele-status-ui.service: active
  NRestarts=0

Running processes:
  /bin/sh /data/experiments/alioth_gpu_status_ui_wrapper.sh
  /data/experiments/gpu-runtime-20260513/alioth_gpu_monitor_service monitor ...

Default MONITOR page lines:
  MONITOR
  SYSTEMD DEGRADED FAIL 2
  BAT 99 CHARGING
  USB0 UP
  WIFI SCAN READY
  AUDIO OFFLINING SND 0
  GPU KGSL KMS ACTIVE
  SCREENSHOT REQUEST OK
  NAV MON WIFI PWR SYS

Wi-Fi page request:
  echo wifi > /run/alioth-panel-page

Wi-Fi page lines:
  WIFI
  WLAN SCAN READY
  MODE CLIENT-READY
  LINK NOT CONNECTED.
  SCAN FILE /RUN/PANEL WIFI
  CONNECT UI NOT PORTED
  USE CPU PANEL FOR KEYBOARD
  PAGE REQUEST WIFI OK
  NAV MON WIFI PWR SYS

Local screenshot:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-pages2-wifi.png
```

Result:

```text
PASS for GPU monitor page-mode correction.
The panel content is now closer to the expected page model, but not yet feature
equivalent to the full CPU Wi-Fi manager UI.
```

## 2026-05-13 12:05 CST - Full CPU panel parity with KMS double buffering

Intent:

```text
Restore the original CPU panel's full UI surface while testing the display-side
fix for touch-triggered flashing. The earlier KGSL monitor proof only rendered
simple text/page summaries and was not acceptable as a panel replacement.
```

Change:

```text
Updated src/alioth-status-ui-c/alioth_status_ui.c:
  - allocate two DRM dumb framebuffers instead of one
  - draw each new frame into the inactive back buffer
  - submit updates with DRM_MODE_PAGE_FLIP and wait for the flip event
  - keep SETCRTC fallback if PAGE_FLIP fails
  - preserve original monitor, Wi-Fi manager, power page, touch handling,
    soft keyboard, page request, and screenshot request logic

This is full original-panel UI parity with KMS double buffering. It is not yet
the final pure KGSL/Vulkan texture renderer.
```

Build/deploy:

```text
Built on remote with redmik40-kernel-builder:bullseye.

New binary:
  artifacts/remote/gpu-runtime-20260513/alioth-status-ui-full-db
  sha256: 0def69fdd7b8fa94529b87ebba101969bbb9811c672f26a89c22319252b09cb2

Deployed to:
  /data/experiments/alioth-status-ui-full-db

Service override:
  /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
  ExecStart=/data/experiments/alioth-status-ui-full-db

Previous GPU proof override backup:
  /data/experiments/backups/10-gpu-first.conf.pre-full-db-20260513-1204
```

Validation:

```text
Service state:
  lele-status-ui.service: active
  NRestarts=0
  MainPID=209992

Running process:
  /data/experiments/alioth-status-ui-full-db

Old proof processes:
  alioth_gpu_status_ui_wrapper: not running
  alioth_gpu_monitor_service: not running

Screenshots:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-full-db.png
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-full-db-wifi.png
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-full-db-monitor-final.png

Screenshot metadata after restoring monitor page:
  page=MONITOR
  width=1080
  height=2400
```

Rollback:

```text
To return to the previous KGSL text proof service:
  cp /data/experiments/backups/10-gpu-first.conf.pre-full-db-20260513-1204 \
    /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
  systemctl daemon-reload
  systemctl restart lele-status-ui.service

To return to the original service ExecStart with no override:
  rm -f /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
  systemctl daemon-reload
  systemctl restart lele-status-ui.service
```

Result:

```text
PASS for full UI parity and service stability after switching away from the
simplified GPU monitor.

The remaining physical validation is whether real touch input no longer causes
the visible flash. The expected improvement comes from drawing offscreen and
flipping buffers instead of redrawing directly into the active scanout buffer.
```

## 2026-05-13 14:35 CST - GPU Test page on full panel

Intent:

```text
Add a GPU Test page to the now-stable full panel without regressing the no-flash
KMS double-buffered display path.
```

Change:

```text
Updated src/alioth-status-ui-c/alioth_status_ui.c:
  - added PAGE_GPU and a fourth nav tab named GPU
  - added /run/alioth-panel-page values: gpu, test, gpu-test, 3
  - added a GPU TEST page showing KGSL node status, DRM status, KGSL sysfs
    telemetry, helper/ICD availability, and the last probe result
  - added RUN PROBE and CLEAR LOG touch buttons
  - RUN PROBE starts the existing GPU helper in safe probe mode and writes:
      /run/alioth-panel-gpu-test.log

Updated scripts/alioth_drm_prime_vulkan_glyph_text_pageflip.c:
  - added probe / kgsl-probe mode
  - probe mode does not open DRM or perform KMS takeover
  - probe mode initializes Turnip/Vulkan through KGSL, submits a GPU
    vkCmdFillBuffer operation, waits for the queue, maps the buffer, and
    verifies the filled pattern from CPU
```

Build/deploy:

```text
Panel binary:
  /data/experiments/alioth-status-ui-gpu-page-v2
  local copy: artifacts/remote/gpu-runtime-20260513/alioth-status-ui-gpu-page
  sha256: 43d8b8c6259e5f46646c06c70347a00db8382c3f329cbda4cc6764f84f9b133c

GPU helper:
  /data/experiments/gpu-runtime-20260513/alioth_gpu_monitor_service
  local copy: artifacts/remote/gpu-runtime-20260513/alioth_gpu_monitor_service_probe
  sha256: 5a11df6179bcb214f415c74ec6da822e6551c0c2be5e7e8e65835fd5b5ddb90b

Service override:
  /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
  ExecStart=/data/experiments/alioth-status-ui-gpu-page-v2

Backups:
  /data/experiments/backups/10-gpu-first.conf.pre-gpu-page-20260513-1432
  /data/experiments/gpu-runtime-20260513/alioth_gpu_monitor_service.pre-probe-20260513-1432
```

Validation:

```text
Service:
  lele-status-ui.service: active
  NRestarts=0
  MainPID=248215
  process=/data/experiments/alioth-status-ui-gpu-page-v2

GPU probe log:
  TU found compatible device /dev/kgsl-3d0
  physical_device=FD650 api=1.2.131
  required_ext VK_EXT_external_memory_dma_buf yes
  gpu_probe_submit=PASS bytes=4096 pattern=0xa5a5a5a5
  gpu_probe_verify=PASS words=1024 pattern=0xa5a5a5a5
  alioth_gpu_probe=PASS

Screenshot:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-gpu-page-v2.png
```

Result:

```text
PASS for GPU Test page and safe KGSL/Vulkan queue-submit probe.
The test page confirms GPU userspace can submit work through Turnip on KGSL
without taking over the current full panel display.
```

## 2026-05-13 15:12 CST - Full panel rendered by KGSL/Vulkan

Intent:

```text
Move the complete status panel, not only the GPU Test page, onto the proven
KGSL/Vulkan + DRM/KMS path while preserving the original CPU panel UI parity.
```

Implementation:

```text
Updated src/alioth-status-ui-c/alioth_status_ui.c:
  - added ALIOTH_GPU_RENDERER build mode
  - kept the existing panel pages, nav, buttons, status cards, bars and text
    layout logic
  - changed rect/text drawing in that mode to record clipped rectangle
    primitives instead of writing pixels directly from CPU
  - exported both DRM dumb scanout buffers as PRIME fds
  - imported those fds into Turnip/Freedreno with Vulkan external dma-buf
    memory
  - rendered each frame into the back scanout buffer with a Vulkan graphics
    pipeline, then continued using the existing KMS page-flip path

Added shaders:
  scripts/alioth_panel_solid.vert
  scripts/alioth_panel_solid.frag

Added build helper:
  artifacts/remote/gpu-runtime-20260513/build_gpu_full_panel.sh
```

Build/deploy:

```text
Panel binary:
  /data/experiments/alioth-status-ui-gpu-full-v1
  local copy: artifacts/remote/gpu-runtime-20260513/alioth-status-ui-gpu-full-v1
  sha256: b58f02c2cabb3c3469d7aee61be9bc80047f7c3d5e681ce64ef7faf4b4b4f2bd

Shaders:
  /data/experiments/alioth_panel_solid.vert.spv
  sha256: b142a1736976678cd88040d5d98e449a41b0170075d561fcaf407ff33d46c0ab
  /data/experiments/alioth_panel_solid.frag.spv
  sha256: 409fb232511ee4b880138b40418a3e7c2f7c45a8130db96b1ff21baae1ff01ea

Service override:
  /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
  ExecStart=/data/experiments/alioth-status-ui-gpu-full-v1
  LD_LIBRARY_PATH=/data/experiments/gpu-runtime-20260513/lib:/data/experiments/mesa-kgsl-prefix/lib/aarch64-linux-gnu
  VK_ICD_FILENAMES=/data/experiments/gpu-runtime-20260513/freedreno_icd-kgsl-mesa2034-aarch64.json

Rollback backup:
  /data/experiments/backups/10-gpu-first.conf.pre-gpu-full-v1-20260513
```

Live validation:

```text
Service:
  MainPID=256331
  NRestarts=0
  ExecMainStatus=0
  ActiveState=active
  SubState=running
  process=/data/experiments/alioth-status-ui-gpu-full-v1

Renderer log:
  display ready 1080x2400 pitch=4352 connector=29 crtc=129
  TU: info: Created an instance
  TU: info: Found compatible device '/dev/kgsl-3d0'.
  gpu panel physical_device=FD650 api=1.2.131
  gpu imported fb[0]=196 memory_type=0
  gpu imported fb[1]=197 memory_type=0
  gpu panel ready vertex_capacity=720000
  full panel GPU renderer enabled

Touch/page log after deployment:
  MONITOR, WIFI, POWER and GPU pages all accepted touch navigation events
  without service restart.
```

Screenshots:

```text
Monitor page:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-gpu-full-monitor-v1.png

GPU page:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-gpu-full-gpu-page-v1.png
  screenshot metadata: page=GPU width=1080 height=2400
```

Result:

```text
PASS for the first complete panel Vulkan renderer.

The visible full panel is now rendered by Vulkan into DRM scanout buffers.
CPU still gathers system data and builds the per-frame rectangle vertex list,
but the final per-frame panel raster/render into the display buffer is KGSL
Vulkan, not CPU pixel writes.
```

Known gaps:

```text
This is a correctness/proof v1, not the final polished UI engine.

Text is still the existing bitmap font represented as many solid rectangle
primitives, not an anti-aliased texture atlas. The renderer currently waits for
the GPU queue each frame for simple synchronization, so there is room for
fences/frame-in-flight optimization and timing metrics. The next polish path is
a texture atlas for text/icons, persistent command resources, FPS/frame timing,
and richer UI assets.
```

## 2026-05-13 15:50 CST - Android font atlas panel

Intent:

```text
Replace the blocky per-pixel text path in the full GPU panel with an
Android-style font atlas path. Keep the complete panel on Vulkan/DRM scanout.
```

Implementation:

```text
Updated src/alioth-status-ui-c/alioth_status_ui.c:
  - added stb_truetype as a small TrueType rasterizer dependency
  - loads Android/Lineage font files directly from the phone, preferring:
      /system/fonts/Roboto-Regular.ttf
      /system/fonts/MiSansVF.ttf
      /system/fonts/DroidSansMono.ttf
  - bakes ASCII glyphs for scales 1..8 into R8 Vulkan sampled images
  - records text as glyph quads with UVs instead of decomposing glyphs into
    many solid rectangles
  - keeps solid rectangles for cards, bars and borders
  - submits draw ops in original UI order so overlays and labels keep the same
    layering semantics
  - changed status lines to label/value columns instead of relying on monospaced
    padding
  - centered button/nav text using font measured width

Added:
  third_party/stb_truetype.h
  scripts/alioth_panel_text.frag

Adjusted after physical feedback:
  font atlas pixel size increased to scale*9 because scale*7/8 felt too small
  monitor page row spacing was tightened where needed to avoid clipping
```

Build/deploy:

```text
Panel binary:
  /data/experiments/alioth-status-ui-gpu-font-v1
  local copy: artifacts/remote/gpu-runtime-20260513/alioth-status-ui-gpu-font-v1
  sha256: a2c99926a0ba75a486b7cf11e33f8a32d784d61d0cd6c71f5e2d99c112f9f5d5

Shaders:
  /data/experiments/alioth_panel_solid.vert.spv
  sha256: 5bce661b3a8e7fd9d201a8b56bf1b0096cf221403ed9402285e207f571a2e022
  /data/experiments/alioth_panel_solid.frag.spv
  sha256: 0f8e68d980c8e6884e117023d61e5c2fd4afa9104b1f62541c6e81afcb64cce2
  /data/experiments/alioth_panel_text.frag.spv
  sha256: e51fd7e1ff08510dfda92b94620d99546613c2ddc0f938efe1791a8a445686b0

Service override:
  /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
  ExecStart=/data/experiments/alioth-status-ui-gpu-font-v1

Backups:
  /data/experiments/backups/10-gpu-first.conf.pre-gpu-font-v1-20260513
  /data/experiments/backups/alioth_panel_solid.vert.spv.pre-gpu-font-v1-20260513
  /data/experiments/backups/alioth_panel_solid.frag.spv.pre-gpu-font-v1-20260513
```

Live validation:

```text
Service:
  MainPID=267277
  NRestarts=0
  ExecMainStatus=0
  ActiveState=active
  SubState=running
  process=/data/experiments/alioth-status-ui-gpu-font-v1

Renderer log:
  gpu panel physical_device=FD650 api=1.2.131
  gpu font atlas ready path=/system/fonts/Roboto-Regular.ttf scales=1..8 pixel=scale*9 renderer=stb_truetype
  gpu imported fb[0]=196 memory_type=0
  gpu imported fb[1]=197 memory_type=0
  gpu panel ready vertex_capacity=720000 text_vertex_capacity=72000 font_atlas=ready
  full panel GPU renderer enabled
```

Screenshots:

```text
Monitor page:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-gpu-font-monitor-v2.png

GPU page:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-gpu-font-gpu-page-v2.png
```

Result:

```text
PASS for Android font atlas text rendering on the full GPU panel.

The panel now reuses the phone's Android font file and renders text as
anti-aliased textured glyph quads through Vulkan. This is much closer to the
modern Android rendering model than the previous per-pixel block text path,
while still avoiding Android SurfaceFlinger/HWUI dependencies.
```

Remaining polish:

```text
This does not yet implement full Android text shaping. ASCII/status text is
covered; multilingual/CJK text would need HarfBuzz/Minikin-style shaping or a
more complete text stack. The visual layout is still information-dense; future
UI work can move toward larger cards, fewer rows per page, icons, and richer
state-specific views now that the font path is no longer the blocker.
```

## 2026-05-13 16:13 CST - GPU page render demo

Intent:

```text
Make the GPU page visibly demonstrate continuous GPU rendering, not only static
status text.
```

Implementation:

```text
Updated src/alioth-status-ui-c/alioth_status_ui.c:
  - added a RENDER DEMO panel to the GPU page
  - draws animated gradient strips, grid lines, wave bars, moving particles,
    and horizontal sweep marks as Vulkan draw ops
  - shows FPS, frame time, KGSL busy percentage and clock in the demo header
  - when the GPU page is active, the main loop now uses non-blocking input poll
    and relies on KMS page-flip/vblank to pace frames
  - cached GPU diagnostics/log/sysfs text for 500 ms so file I/O does not run
    every animation frame
  - writes live demo metrics once per second to:
      /run/alioth-panel-gpu-demo.txt
```

Build/deploy:

```text
Panel binary:
  /data/experiments/alioth-status-ui-gpu-demo-v1
  local copy: artifacts/remote/gpu-runtime-20260513/alioth-status-ui-gpu-demo-v1
  sha256: 13b74c814525780304de538d3fb85f4f3b2558fc25d8e09ae2779b8722e3f04a

Service override:
  /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
  ExecStart=/data/experiments/alioth-status-ui-gpu-demo-v1

Rollback backup:
  /data/experiments/backups/10-gpu-first.conf.pre-gpu-demo-v1-20260513
```

Live validation:

```text
Service:
  MainPID=273589
  NRestarts=0
  ExecMainStatus=0
  ActiveState=active
  SubState=running
  process=/data/experiments/alioth-status-ui-gpu-demo-v1

Renderer log:
  gpu panel physical_device=FD650 api=1.2.131
  gpu font atlas ready path=/system/fonts/Roboto-Regular.ttf scales=1..8 pixel=scale*9 renderer=stb_truetype
  gpu imported fb[0]=154 memory_type=0
  gpu imported fb[1]=197 memory_type=0
  gpu panel ready vertex_capacity=720000 text_vertex_capacity=72000 font_atlas=ready
  full panel GPU renderer enabled

Live demo metrics after screenshot I/O settled:
  /run/alioth-panel-gpu-demo.txt
  fps=30
  frame_ms=33
  page=GPU
```

Screenshots:

```text
GPU render demo:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-gpu-demo-v1-final.png
```

Result:

```text
PASS for the first visible GPU render demo page.

The page now has continuous animated GPU-drawn content and live metrics. The
current measured steady rate is about 30 FPS / 33 ms on the service path. That
is enough to prove dynamic rendering, but it is not yet a 60 FPS polished UI.
The likely next performance gap is synchronization/presentation: the renderer
still uses a simple submit/wait/page-flip path rather than a pipelined
frame-in-flight model.
```

## 2026-05-13 16:33 CST - Panel power telemetry and FPS controls

Intent:

```text
Expose real PMIC/kernel power readings on the panel so GPU page frame-rate
changes can be compared against battery and USB input telemetry.
```

Implementation:

```text
Updated src/alioth-status-ui-c/alioth_status_ui.c:
  - top status line now shows time, BAT net power, USB input power
  - GPU page status line also shows current/target FPS
  - GPU page added 30 FPS / 15 FPS / 5 FPS touch controls
  - external FPS control path:
      /run/alioth-panel-gpu-fps
  - live render metrics path:
      /run/alioth-panel-gpu-demo.txt
```

Build/deploy:

```text
Panel binary:
  /data/experiments/alioth-status-ui-gpu-power-v1
  local copy: artifacts/remote/gpu-runtime-20260513/alioth-status-ui-gpu-power-v1
  sha256: 25998c7efb969893d2776445993406132ba225a34acea43be8ee195b6848deda
```

Power telemetry interpretation:

```text
BAT is battery-side net power from battery/bms voltage_now * current_now.
When plugged and full, BAT can be near zero or slightly positive/negative and
does not equal total device consumption.

USB is charger/input-side power from usb voltage_now * input_current_now. It is
better for plugged relative trend checks, but includes charging/conversion
effects and is noisy. Display resolution is 0.01 W, but actual accuracy is
PMIC/fuel-gauge dependent; compare 30-60 second averages rather than one-shot
samples.
```

Short live sample:

```text
30 FPS: fps=30 frame_ms=33 KGSL busy about 5%, USB samples roughly 1.4-2.9 W
15 FPS: fps=15 frame_ms=69 KGSL busy about 2.6%, USB samples noisy
 5 FPS: fps=5  frame_ms=198 KGSL busy about 0%, USB samples noisy
```

Result:

```text
PASS for live power/FPS visibility.
GPU busy tracks target FPS clearly. Plugged/full battery power readings are not
precise enough as one-shot GPU power numbers; use rolling averages or unplugged
controlled tests for stronger conclusions.
```

## 2026-05-13 17:08 CST - GPU UI runtime v1

Intent:

```text
Start moving from the legacy CPU-panel layout to a touch-first GPU-rendered UI
runtime with reusable visual primitives.
```

Implementation:

```text
Updated src/alioth-status-ui-c/alioth_status_ui.c:
  - added a small internal UI layer: theme, panel, metric card, info row,
    chip/segmented touch controls, and modern tab bar styling
  - rebuilt the Monitor page as touch cards and information panels
  - rebuilt the GPU page around metric cards, FPS chips, runtime/probe panels,
    and the render demo
  - removed visible instructions that depended on physical volume/power-button
    navigation; physical keys remain as a recovery/fallback path
  - kept existing Wi-Fi, Power, screenshot, page request, and GPU probe logic
```

Build/deploy:

```text
Panel binary:
  /data/experiments/alioth-status-ui-gpu-ui-runtime-v1
  local copy: artifacts/remote/gpu-runtime-20260513/alioth-status-ui-gpu-ui-runtime-v1
  final sha256: 4d3789f216b5f20f1f886abc25c1eff344257e9c802834f7c467b433418d9245

Service override:
  /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
  ExecStart=/data/experiments/alioth-status-ui-gpu-ui-runtime-v1

Local override copy:
  artifacts/remote/gpu-runtime-20260513/10-gpu-ui-runtime-v1.conf
```

Live validation:

```text
Service:
  MainPID=289090
  NRestarts=0
  ExecMainStatus=0
  ActiveState=active
  SubState=running
  process=/data/experiments/alioth-status-ui-gpu-ui-runtime-v1

GPU metrics:
  /run/alioth-panel-gpu-demo.txt
  fps=30
  frame_ms=33
  page=GPU
```

Screenshots:

```text
Monitor page:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-gpu-ui-runtime-monitor-v1-final.png

GPU page:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-gpu-ui-runtime-gpu-v1-final2.png
```

Result:

```text
PASS for the first UI-runtime refactor slice.

The panel now has a reusable foundation for modern card/layout styling and
touch-first controls while preserving the current low-level Vulkan/KMS renderer.
This is still a custom lightweight UI stack, not Skia/HWUI/LVGL yet. The next
step is to migrate Wi-Fi and Power onto the same primitives, then decide whether
text shaping/vector drawing justify bringing in a larger library.
```

## 2026-05-13 17:52 CST - GPU UI runtime v2 split

Intent:

```text
Start Phase 1 of the panel rebuild plan: split the first UI runtime module out
of the monolithic status UI source without changing visible behavior.
```

Implementation:

```text
Added:
  src/alioth-panel/ui_runtime.h

Updated:
  src/alioth-status-ui-c/alioth_status_ui.c

Change:
  - moved the UI theme and reusable UI primitives into ui_runtime.h
  - main panel source now includes "alioth-panel/ui_runtime.h"
  - kept the same rendering pipeline, pages, runtime paths, and behavior
```

Build/deploy:

```text
Panel binary:
  /data/experiments/alioth-status-ui-gpu-ui-runtime-v2-split
  local copy: artifacts/remote/gpu-runtime-20260513/alioth-status-ui-gpu-ui-runtime-v2-split
  sha256: 4d3789f216b5f20f1f886abc25c1eff344257e9c802834f7c467b433418d9245

Service override:
  /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
  ExecStart=/data/experiments/alioth-status-ui-gpu-ui-runtime-v2-split

Local override copy:
  artifacts/remote/gpu-runtime-20260513/10-gpu-ui-runtime-v2-split.conf
```

Live validation:

```text
Service:
  MainPID=300020
  NRestarts=0
  ExecMainStatus=0
  ActiveState=active
  SubState=running
  process=/data/experiments/alioth-status-ui-gpu-ui-runtime-v2-split

GPU metrics:
  /run/alioth-panel-gpu-demo.txt
  fps=30
  frame_ms=33
  page=GPU

Rescue/control checks:
  ping 172.16.42.2: OK
  SSH root@172.16.42.2 true: OK
  2323 shell true: OK
```

Screenshots:

```text
Monitor page:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-gpu-ui-runtime-v2-split-monitor.png

GPU page:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-gpu-ui-runtime-v2-split-gpu.png
```

Result:

```text
PASS for the first behavior-preserving module split.

The binary hash matches v1 because the split only moved UI runtime code into a
header module and did not change compiled behavior. This is intentional for the
first Phase 1 step. The next split should move data/page logic behind clearer
module boundaries, then Network page rebuild can start.
```

## 2026-05-13 18:56 CST - GPU UI runtime v2 types split

Intent:

```text
Continue Phase 1 of the panel rebuild plan by moving app/page/input/data-cache
type definitions out of the monolithic status UI source without changing
visible behavior.
```

Implementation:

```text
Added:
  src/alioth-panel/app_types.h

Updated:
  src/alioth-status-ui-c/alioth_status_ui.c

Change:
  - moved cpu_sample, display/page/screen enums, input state, Wi-Fi UI state,
    Wi-Fi scan entry, GPU demo metrics and GPU page cache definitions into
    app_types.h
  - main panel source now includes "alioth-panel/app_types.h"
  - kept rendering, data collection, touch paths, screenshots, runtime files,
    and service behavior unchanged
```

Build/deploy:

```text
Panel binary:
  /data/experiments/alioth-status-ui-gpu-ui-runtime-v2-types
  local copy: artifacts/remote/gpu-runtime-20260513/alioth-status-ui-gpu-ui-runtime-v2-types
  sha256: 4d3789f216b5f20f1f886abc25c1eff344257e9c802834f7c467b433418d9245

Service override:
  /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
  ExecStart=/data/experiments/alioth-status-ui-gpu-ui-runtime-v2-types

Local override copy:
  artifacts/remote/gpu-runtime-20260513/10-gpu-ui-runtime-v2-types.conf
```

Live validation:

```text
Service:
  MainPID=316041
  NRestarts=0
  ExecMainStatus=0
  ActiveState=active
  SubState=running
  process=/data/experiments/alioth-status-ui-gpu-ui-runtime-v2-types

GPU metrics:
  /run/alioth-panel-gpu-demo.txt
  fps=30
  frame_ms=33
  page=GPU

Screenshot request output:
  Monitor: width=1080 height=2400 page=MONITOR
  GPU:     width=1080 height=2400 page=GPU fps=30 frame_ms=34

Rescue/control checks:
  route 172.16.42.2: en13
  ping 172.16.42.2: OK, 0.0% packet loss
  SSH root@172.16.42.2 with /Users/wuyuele/.ssh/alioth_usb_ed25519: OK
  2323 shell true: OK
```

Screenshots:

```text
Monitor page:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-gpu-ui-runtime-v2-types-monitor.png

GPU page:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-gpu-ui-runtime-v2-types-gpu.png
```

Result:

```text
PASS for the second behavior-preserving module split.

The binary hash still matches v1/v2-split because this step only changes the
source boundary for shared app types. This confirms Phase 1 can proceed as a
low-risk modular extraction before larger page rebuilds.

Recommended next split:
  move data-provider/cache refresh logic out of the page drawing code, then
  rebuild Network/Wi-Fi as the first non-Monitor page on top of the UI runtime.
```

## 2026-05-13 19:26 CST - GPU UI runtime v2 final page rebuild

Intent:

```text
Continue the panel rebuild beyond module extraction: introduce a page data
snapshot boundary and finish a first complete modern touch-first pass across
Monitor, Wi-Fi, Power and GPU pages.
```

Implementation:

```text
Added:
  src/alioth-panel/panel_data.h

Updated:
  src/alioth-panel/app_types.h
  src/alioth-status-ui-c/alioth_status_ui.c

Change:
  - added struct panel_data_snapshot for Monitor page data
  - moved Monitor data refresh into panel_data_refresh()
  - rebuilt Wi-Fi page with metric cards, scan list, input fields and keyboard
    on the shared UI runtime
  - rebuilt Power page with battery/input/thermal cards, live telemetry and
    touch action rows
  - removed leftover old visible touch/instruction copy from the panel
  - kept the low-level Vulkan/KMS renderer, Android font atlas, GPU demo, page
    request files, screenshots, Wi-Fi scan/connect commands and rescue paths
```

Build/deploy:

```text
Final panel binary:
  /data/experiments/alioth-status-ui-gpu-ui-runtime-v2-final
  local copy: artifacts/remote/gpu-runtime-20260513/alioth-status-ui-gpu-ui-runtime-v2-final
  sha256: 574421ae9d5174a02b6dfa6bb142d39d803dbad3ae1a9c99e5786d95ace3fe1a

Service override:
  /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
  ExecStart=/data/experiments/alioth-status-ui-gpu-ui-runtime-v2-final

Local override copy:
  artifacts/remote/gpu-runtime-20260513/10-gpu-ui-runtime-v2-final.conf
```

Intermediate deployed milestones:

```text
v2-data:
  /data/experiments/alioth-status-ui-gpu-ui-runtime-v2-data
  sha256: 97b58279f4c233c5c91c398573ffbefd4a5cfaac03859850cc41c59d53e41055

v2-wifi-clean:
  /data/experiments/alioth-status-ui-gpu-ui-runtime-v2-wifi-clean
  sha256: 701e8463884fb2c3b711c00793544b01e05e8632a98f207523feaaa0bcb263db

v2-power-clean:
  /data/experiments/alioth-status-ui-gpu-ui-runtime-v2-power-clean
  sha256: 1685cbebd4327bedf43cdeb2efb8cb330350cfc3ae195f668f1e5220bce488b8
```

Live validation:

```text
Service:
  MainPID=324878
  NRestarts=0
  ExecMainStatus=0
  ActiveState=active
  SubState=running
  process=/data/experiments/alioth-status-ui-gpu-ui-runtime-v2-final

GPU metrics after steady-state screenshot:
  /run/alioth-panel-gpu-demo.txt
  fps=30
  frame_ms=33
  page=GPU

Screenshot request output:
  Monitor: width=1080 height=2400 page=MONITOR
  Wi-Fi:   width=1080 height=2400 page=WIFI
  Power:   width=1080 height=2400 page=POWER
  GPU:     width=1080 height=2400 page=GPU fps=30 frame_ms=33

Rescue/control checks:
  route 172.16.42.2: en13
  ping 172.16.42.2: OK, 0.0% packet loss
  SSH root@172.16.42.2 with /Users/wuyuele/.ssh/alioth_usb_ed25519: OK
  2323 shell true: OK
```

Screenshots:

```text
Monitor:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-gpu-ui-runtime-v2-final-monitor.png

Wi-Fi:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-gpu-ui-runtime-v2-final-wifi.png

Power:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-gpu-ui-runtime-v2-final-power.png

GPU steady state:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-gpu-ui-runtime-v2-final-gpu-steady.png
```

Result:

```text
PASS for the first complete GPU panel UI rebuild pass.

The panel is now a custom lightweight Vulkan/KMS UI with Android font atlas text,
touch-first navigation, shared card/list/chip primitives, Wi-Fi management,
power controls, monitor dashboard, and live GPU demo. CPU code still collects
system data and builds draw ops; the visible panel frame is rendered through the
KGSL Turnip Vulkan path.
```

Remaining non-blocking gaps:

```text
This is not Android HWUI/Skia, and it still does not have full text shaping,
icon assets, accessibility, animation primitives beyond the GPU demo, or a
pipelined multi-frame Vulkan present path. Those are polish/runtime-engine
tracks rather than blockers for the current panel rebuild milestone.
```

## 2026-05-13 19:48 CST - Monitor font readability pass

Intent:

```text
Increase Monitor page readability after physical feedback that the information
rows still felt too small.
```

Implementation:

```text
Updated:
  src/alioth-panel/ui_runtime.h
  src/alioth-status-ui-c/alioth_status_ui.c

Change:
  - added ui_info_row_fit(), which uses larger value text when it fits and
    falls back for long fields
  - changed Monitor page rows to use the larger fitted row renderer
  - increased metric-card detail text when space allows
  - increased CPU core labels
  - enlarged middle Monitor panels slightly and removed the decorative charging
    animation from the Monitor Power card to avoid DNS overlap
```

Build/deploy:

```text
Panel binary:
  /data/experiments/alioth-status-ui-gpu-ui-runtime-v2-monitor-font2
  local copy: artifacts/remote/gpu-runtime-20260513/alioth-status-ui-gpu-ui-runtime-v2-monitor-font2
  sha256: ba4d3174595aff7078167c9034f2f89e34664fb0feddc2492d5f7d56c4979011

Service override:
  /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
  ExecStart=/data/experiments/alioth-status-ui-gpu-ui-runtime-v2-monitor-font2
```

Validation:

```text
Service:
  MainPID=330857
  NRestarts=0
  ExecMainStatus=0
  ActiveState=active
  SubState=running

Screenshot:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-gpu-ui-runtime-v2-monitor-font2-monitor.png

Rescue/control checks:
  ping 172.16.42.2: OK, 0.0% packet loss
  SSH root@172.16.42.2 with /Users/wuyuele/.ssh/alioth_usb_ed25519: OK
  2323 shell true: OK
```

Result:

```text
PASS. Monitor information rows are visibly larger, while long system fields
still fit without overlapping the adjacent panels.
```

## 2026-05-13 20:33 CST - 120 Hz mode, Wi-Fi list layout, and smooth list drag

Intent:

```text
Address physical feedback that Monitor fonts/layout still felt uneven, Wi-Fi
looked crowded after connection, and the Wi-Fi scan list should scroll like a
real touch list instead of jumping only after release.
```

Implementation:

```text
Updated:
  src/alioth-status-ui-c/alioth_status_ui.c
  src/alioth-panel/ui_runtime.h
  src/alioth-panel/app_types.h

Changes:
  - DRM setup now chooses the highest refresh mode exposed by the connected
    panel instead of blindly using modes[0]
  - GPU Test exposes 120/60/30/15/5 targets and reports panel_hz plus
    draw/gpu/present timing in /run/alioth-panel-gpu-demo.txt
  - Monitor rows use a fixed value font size with ellipsis truncation instead
    of per-row font downscaling
  - Wi-Fi page was rebuilt as a connection summary plus scan-list view; keyboard
    is hidden by default and appears only when editing SSID/password
  - Wi-Fi scan results now read up to WIFI_SCAN_MAX_UNIQUE entries and draw a
    scrollable list with visible range and scrollbar
  - Wi-Fi list drag uses scan_scroll_px pixel offset and updates while the
    finger is still down, so it follows movement instead of waiting for release
```

Build/deploy:

```text
Panel binary:
  /data/experiments/alioth-status-ui-gpu-ui-runtime-v2-uniformfont-pause
  local copy: artifacts/remote/gpu-runtime-20260513/alioth-status-ui-gpu-ui-runtime-v2-uniformfont-pause
  sha256: 331650b6a853fbfbafd5e3088d1197f38c2fd8ce6b7ef832f900e3f92e551758

Service:
  MainPID=339715
  NRestarts=0
  ExecMainStatus=0
  ActiveState=active
  SubState=running
  display ready 1080x2400@120 pitch=4352 connector=29 crtc=129 mode=1080x2400x120x183357cmd
```

Validation:

```text
Wi-Fi screenshots:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-wifi-smooth-default.png
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-wifi-list-scrolled.png

GPU metrics after 120 Hz mode:
  fps=57
  frame_ms=20
  target_fps=120
  panel_hz=120
  draw_ms=3
  gpu_ms=2
  present_ms=11
  total_ms=16

Route/control:
  route to 172.16.42.2 uses en13
  SSH root@172.16.42.2 with /Users/wuyuele/.ssh/alioth_usb_ed25519: OK
```

Result:

```text
PASS for deployment and layout validation. The panel is running in the 120 Hz
DRM mode, Wi-Fi scan results are a scrollable list, and the code path now
updates list offset during touch motion. Physical finger smoothness still needs
human confirmation on the device.
```

## 2026-05-13 20:37 CST - Wi-Fi keyboard activation only from input fields

Intent:

```text
Make the Wi-Fi software keyboard stay collapsed by default and avoid showing it
when selecting scan-list rows or switching the SSID/Password field selector.
```

Implementation:

```text
Updated src/alioth-status-ui-c/alioth_status_ui.c:
  - removed the always-visible Edit keyboard toggle
  - kept a Hide chip only while the keyboard is visible
  - list-row selection now fills SSID and switches the target field to password
    without showing the keyboard
  - Connect hides the keyboard after a valid connect request is launched
  - only tapping the SSID or PASSWORD input field sets keyboard_visible=true
```

Build/deploy:

```text
Panel binary:
  /data/experiments/alioth-status-ui-gpu-ui-runtime-v2-uniformfont-pause
  sha256: 2a11332d3e5666813be9c70c490ce5e7a3995fbb5d5696efd24aaf36b59dbacf

Service:
  MainPID=339878
  NRestarts=0
  ExecMainStatus=0
  ActiveState=active
  SubState=running
  display ready 1080x2400@120
```

Validation:

```text
Screenshot:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-wifi-keyboard-hidden.png

Observed:
  Wi-Fi page renders with keyboard hidden by default and the scan list remains
  visible. The code path now shows keyboard only from SSID/PASSWORD input-field
  taps; physical tap behavior still needs on-device confirmation.
```

## 2026-05-13 20:53 CST - Persist current slot and move rootfs to userdata

Intent:

```text
Keep the current working GPU panel boot path as the default path for the current
slot, convert the 192G userdata partition into useful Linux storage, and run a
native compile benchmark on the phone.
```

Boot persistence:

```text
Current slot after normal reboot:
  androidboot.slot_suffix=_a

Current boot image match:
  phone boot_a first 48,427,008 bytes:
    11ddc5806f285757d3d110fbe17a6ba35a0a4f1b0375d9a6a1a718250453ebe5
  local image:
    artifacts/experiments/exp4-touch-wifi-panel-persist/lineage-mininitramfs-boot-touch-wifi-panel.img
    sha256 11ddc5806f285757d3d110fbe17a6ba35a0a4f1b0375d9a6a1a718250453ebe5

Result:
  current slot _a already contains the working EXP4 touch/Wi-Fi/panel boot image.
  A normal systemctl reboot returned to Ubuntu without fastboot boot.
```

Storage change:

```text
userdata outer partition:
  /dev/block/by-name/userdata
  GPT name: userdata
  size: 192.9 GiB partition, 188.8G ext4 filesystem after formatting

Before formatting:
  userdata contained a stale nested GPT/PMBR signature and was not a usable
  ext4 mount target for the initramfs.

Action:
  mkfs.ext4 -F -L USERDATA192 /dev/block/by-name/userdata
  copied the live Ubuntu rootfs to /rootfs/ubuntu-24.04 on userdata

Validated after normal reboot:
  /      /dev/block/by-name/userdata[/rootfs/ubuntu-24.04] ext4 188.8G 10.4G 168.8G
  /data  /dev/block/by-name/arch[/data]                    ext4 31.4G  6.8G  23G
  /work  lives on the userdata-backed rootfs
  /data/work -> /work
```

Panel state after reboot:

```text
lele-status-ui.service:
  enabled
  active
  MainPID=6477
  NRestarts=0

GPU metrics sample:
  page=GPU
  paused=1
  target_fps=5
  panel_hz=120
```

Native compile benchmark:

```text
Toolchain installed:
  gcc/g++ 13.3.0
  GNU Make 4.3
  build-essential

Benchmark:
  generated 384 C++17 translation units
  workdir: /work/builds/compile-bench-20260513-384
  result artifact:
    artifacts/remote/compile-bench-20260513/compile-bench-384-userdata-result.txt

Results:
  -j1 elapsed=27.78 user=24.11 sys=5.59 maxrss_kb=38032
  -j2 elapsed=15.24 user=27.03 sys=5.46 maxrss_kb=38048
  -j4 elapsed=8.85  user=31.31 sys=6.00 maxrss_kb=38040
  -j8 elapsed=7.19  user=48.91 sys=6.64 maxrss_kb=38048
  checksum=c46c58f011bfb02f

Thermal:
  representative cpu-1-3-usr rose from 40.7C to 67.8C during the benchmark.
```

Result:

```text
PASS. The phone now boots the current _a Ubuntu/GPU-panel path by default from
the 192G userdata rootfs, has a persistent large workspace at /work, and can run
native parallel C++ builds with useful scaling up to -j8.
```

## 2026-05-13 21:08 CST - Monitor CPU core class labels

Intent:

```text
Make the Monitor CPU grid identify which C0-C7 entries are small, big, and
prime cores.
```

Implementation:

```text
Updated src/alioth-status-ui-c/alioth_status_ui.c:
  - added cpu_core_capacity_hint() using /sys/devices/system/cpu/cpu*/cpu_capacity
  - falls back to cpuinfo_max_freq and then alioth index order if capacity is missing
  - renders each CPU row as Cn + colored class chip + percent + bar
  - adds Monitor legend: L small, B big, P prime
```

Live topology used on alioth:

```text
C0 cap=313  max=1804800  -> L
C1 cap=313  max=1804800  -> L
C2 cap=313  max=1804800  -> L
C3 cap=313  max=1804800  -> L
C4 cap=777  max=2419200  -> B
C5 cap=777  max=2419200  -> B
C6 cap=777  max=2419200  -> B
C7 cap=1024 max=3187200  -> P
```

Build/deploy:

```text
Built natively on the phone after installing libvulkan1/libvulkan-dev headers
from Ubuntu arm64 packages copied through the Mac host.

Panel binary:
  /data/experiments/alioth-status-ui-gpu-ui-runtime-v2-core-tags
  local copy: artifacts/remote/gpu-runtime-20260513/alioth-status-ui-gpu-ui-runtime-v2-core-tags
  sha256: 7883e552b906e1a78c4c984c542b7c98be01192b870ee4ebf3bbe2c0a0b370a4

Service override:
  /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
  ExecStart=/data/experiments/alioth-status-ui-gpu-ui-runtime-v2-core-tags
```

Validation:

```text
Service:
  MainPID=13924
  NRestarts=0
  ActiveState=active
  SubState=running

Screenshot:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-monitor-core-tags.png
```

Result:

```text
PASS. Monitor now marks C0-C3 as L, C4-C6 as B, and C7 as P.
```

## 2026-05-14 02:25 CST - Battery card detail spacing fix

Intent:

```text
Fix the Monitor Battery card after the detail line appeared visually wrong.
The raw telemetry was valid, but the 100% progress bar overlapped the detail
text.
```

Raw telemetry at the time:

```text
battery:
  capacity=100
  status=Full
  voltage_now=4416980
  current_now=3906

usb:
  voltage_now=5148138
  input_current_now=308495
  online=1
```

Implementation:

```text
Updated:
  src/alioth-panel/ui_runtime.h
  src/alioth-panel/panel_data.h
  src/alioth-status-ui-c/alioth_status_ui.c

Changes:
  - ui_metric_card() now moves the detail line upward when a progress bar is
    present, avoiding overlap.
  - Battery detail now includes battery status, for example:
    Full +0.02W 5mA
  - battery_line() now formats current as mA instead of the old MA suffix.
```

Build/deploy:

```text
Panel binary:
  /data/experiments/alioth-status-ui-gpu-ui-runtime-v2-battery-fix
  local copy: artifacts/remote/gpu-runtime-20260513/alioth-status-ui-gpu-ui-runtime-v2-battery-fix
  sha256: 338cb52510ea2bf9b9ff4ba856376ad7cbffc21d82ec99da9eae1d6744f210b8

Service override:
  /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
  ExecStart=/data/experiments/alioth-status-ui-gpu-ui-runtime-v2-battery-fix
```

Validation:

```text
Service:
  MainPID=22095
  NRestarts=0
  ActiveState=active
  SubState=running

Screenshot:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-current-20260514-battery-fix.png
```

Result:

```text
PASS. The Battery card now displays `Full +0.02W 5mA` above the 100% bar
without overlap. USB input remains around 1.55W.
```

## 2026-05-14 10:59 CST - Remove GPU render demo from panel

Intent:

```text
Remove the GPU render demo from the production panel now that the GPU path has
been proven. Keep the GPU page, but make it a practical renderer/status page.
```

Implementation:

```text
Updated:
  src/alioth-status-ui-c/alioth_status_ui.c
  src/alioth-panel/app_types.h
  docs/current-state.md
  docs/boot-validation-log.md

Changes:
  - Removed the visible render-demo animation from the GPU page.
  - Removed the old FPS target and pause request controls from the production
    input path.
  - Renamed the runtime metrics path to:
    /run/alioth-panel-gpu-metrics.txt
  - GPU page now shows panel rate, KGSL busy, USB/battery power, renderer paths,
    probe status, and draw/GPU/present timing.
  - Frame timing copy now separates active render time from the 1s status wake
    interval, so a 1s sample period is not mistaken for a slow GPU frame.
  - Fixed KGSL busy formatting when the sysfs value already includes `%`.
  - Rounded the low-rate status metrics so the 1Hz status page reports `fps=1`
    instead of truncating to zero.
```

Build/deploy:

```text
Panel binary:
  /data/experiments/alioth-status-ui-gpu-ui-runtime-v2-gpu-status
  local copy: artifacts/remote/gpu-runtime-20260513/alioth-status-ui-gpu-ui-runtime-v2-gpu-status
  sha256: 6c7fab74e22d5d8fd11a5ab77a2ce02b896f4f7273ca86063cbfa93bc0c46dac

Service override:
  /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
  ExecStart=/data/experiments/alioth-status-ui-gpu-ui-runtime-v2-gpu-status
```

Validation:

```text
Service:
  MainPID=22822
  NRestarts=0
  ActiveState=active
  SubState=running

Screenshot:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-gpu-status-final-20260514.png

Runtime metrics:
  fps=1
  frame_ms=1033
  page=GPU
  panel_hz=120
  draw_ms=7
  gpu_ms=6
  present_ms=19
  total_ms=1033
```

Result:

```text
PASS. GPU render demo is gone from the production panel. The GPU tab is now a
status/diagnostics page for the Vulkan renderer and direct scanout path.
```

## 2026-05-14 11:03 CST - Wi-Fi password remember support

Intent:

```text
Make the touch Wi-Fi page remember passwords when Connect is pressed and
auto-fill the saved password when the same SSID is selected again.
```

Implementation:

```text
Updated:
  src/alioth-status-ui-c/alioth_status_ui.c
  docs/current-state.md
  docs/boot-validation-log.md

Changes:
  - Reused /etc/alioth-wifi-default as the persistent Wi-Fi credential store.
    This matches alioth-wifi-connect, which already supports repeated
    ssid/password line pairs.
  - Wi-Fi page startup now reads both SSID and password from the saved config,
    not only the SSID.
  - Selecting a scanned SSID now looks up a matching saved password and
    auto-fills it. If no saved entry exists, the old password field is cleared
    to avoid accidentally connecting with another network's password.
  - If an SSID is typed manually and the password field is empty, Connect also
    looks up the saved password before launching alioth-wifi-connect.
  - Connect updates the selected SSID/password pair at the top of
    /etc/alioth-wifi-default and preserves older saved networks below it.
  - Added a Saved / Not saved chip in the Wi-Fi control row.
  - Passwords are masked on the panel with `*`; the real value stays in memory
    for connect requests.
```

Build/deploy:

```text
Panel binary:
  /data/experiments/alioth-status-ui-gpu-ui-runtime-v2-wifi-save
  local copy: artifacts/remote/gpu-runtime-20260513/alioth-status-ui-gpu-ui-runtime-v2-wifi-save
  sha256: 9919eb459fd74911fb9bb5328f2a57ca184fc99bbbd6a64feca85cd4b75dcf1d

Service override:
  /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
  ExecStart=/data/experiments/alioth-status-ui-gpu-ui-runtime-v2-wifi-save
```

Validation:

```text
Service:
  MainPID=23408
  NRestarts=0
  ActiveState=active
  SubState=running

Saved config check, password redacted:
  ssid[1]=Didi-Guest
  psk[1]=11 chars

Screenshot:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-wifi-save-20260514.png
```

Result:

```text
PASS. Wi-Fi page auto-fills the saved SSID/password, shows Saved, and masks the
password field on screen.
```

## 2026-05-14 11:59 CST - Behavior-preserving panel paths split

Intent:

```text
Start the panel module split without changing runtime behavior. The first split
keeps the deployed UI stable while moving shared runtime paths and limits out of
the 5k-line main source file.
```

Implementation:

```text
Updated:
  src/alioth-status-ui-c/alioth_status_ui.c
  src/alioth-panel/app_types.h

Added:
  src/alioth-panel/panel_paths.h

Changes:
  - Moved Wi-Fi field limits, saved-network count, scan limits, navigation
    height, screenshot paths, page request path, GPU metrics path, GPU probe
    helper paths, Vulkan ICD path, LD_LIBRARY_PATH, shader paths, and font atlas
    limits into panel_paths.h.
  - app_types.h now includes panel_paths.h instead of carrying local Wi-Fi size
    fallback macros.
  - alioth_status_ui.c includes panel_paths.h and no longer owns those runtime
    path constants.
```

Build/deploy:

```text
Build dir:
  /work/panel-build-paths-20260514

Native build command:
  gcc -O2 -std=gnu11 -Wall -Wextra -Wno-format-truncation
      -Wno-unused-function -Wno-sign-compare -Wno-unused-parameter
      -DALIOTH_GPU_RENDERER=1 -I. -Isrc
      -o alioth-status-ui-gpu-ui-runtime-v2-paths
      src/alioth-status-ui-c/alioth_status_ui.c -lvulkan -lm

Panel binary:
  /data/experiments/alioth-status-ui-gpu-ui-runtime-v2-paths
  local copy: artifacts/remote/gpu-runtime-20260513/alioth-status-ui-gpu-ui-runtime-v2-paths
  sha256: 9919eb459fd74911fb9bb5328f2a57ca184fc99bbbd6a64feca85cd4b75dcf1d

Service override:
  /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
  ExecStart=/data/experiments/alioth-status-ui-gpu-ui-runtime-v2-paths
```

Validation:

```text
Service:
  MainPID=24521
  NRestarts=0
  ActiveState=active
  SubState=running

Screenshot:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-v2-paths-monitor.png
  width=1080 height=2400 page=MONITOR

Runtime metrics:
  fps=1
  frame_ms=1033
  page=MONITOR
  panel_hz=120
  draw_ms=11
  gpu_ms=7
  present_ms=14
  total_ms=1033
```

Result:

```text
PASS. This was a behavior-preserving source organization step. The produced
binary is byte-identical to v2-wifi-save, which confirms no generated code
changed while the source boundary moved into src/alioth-panel/panel_paths.h.
```

## 2026-05-14 12:03 CST - Monitor system health card

Intent:

```text
Make the Monitor page answer "is the system healthy?" directly instead of
requiring SSH and systemctl to notice failed units.
```

Implementation:

```text
Updated:
  src/alioth-status-ui-c/alioth_status_ui.c
  src/alioth-panel/app_types.h
  src/alioth-panel/panel_data.h
  src/alioth-panel/ui_runtime.h

Changes:
  - Added service_health_line(), which reads systemd NFailedUnits and the first
    failed unit names through systemctl.
  - Cached the health probe for 5 seconds so the panel does not spawn systemctl
    on every frame.
  - Added services_value, services_detail, and failed_units to the panel data
    snapshot.
  - Replaced the top-right Monitor Renderer card with a Health card.
  - Added the failed service summary into the Runtime section.
  - Moved renderer detail into the Hardware section.
  - Made ui_metric_card() ellipsize detail text so long service names cannot
    overflow compact cards.
```

Build/deploy:

```text
Build dir:
  /work/panel-build-health-20260514

Panel binary:
  /data/experiments/alioth-status-ui-gpu-ui-runtime-v2-health
  local copy: artifacts/remote/gpu-runtime-20260513/alioth-status-ui-gpu-ui-runtime-v2-health
  sha256: 29a0d3f493390ecf53047c79bd581d8d01f173a81f512b81920d03ec035091fe

Service override:
  /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
  ExecStart=/data/experiments/alioth-status-ui-gpu-ui-runtime-v2-health
```

Validation:

```text
Service:
  MainPID=24911
  NRestarts=0
  ActiveState=active
  SubState=running

Screenshot:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-v2-health-monitor.png
  width=1080 height=2400 page=MONITOR

Runtime metrics:
  fps=1
  frame_ms=1050
  page=MONITOR
  panel_hz=120
  draw_ms=23
  gpu_ms=7
  present_ms=19
  total_ms=1050

Current failed units surfaced by the panel:
  alioth-audio-adsp-boot.service
  fwupd-refresh.service
```

Result:

```text
PASS. Monitor now exposes system health as a first-class panel signal. The
failed units are still present; this change makes them visible rather than
silently hidden behind SSH diagnostics.
```

## 2026-05-14 12:05 CST - Disable non-essential fwupd refresh noise

Intent:

```text
After adding the Monitor Health card, classify the reported failed units so the
panel does not train the user to ignore noisy failures.
```

Diagnosis:

```text
Before cleanup:
  NFailedUnits=2
  alioth-audio-adsp-boot.service failed
  fwupd-refresh.service failed

fwupd-refresh.service:
  Ubuntu default LVFS metadata refresh via fwupd-refresh.timer.
  Not a meaningful default path for this Android downstream kernel phone rootfs.
  It was periodically failing and polluting the health signal.

alioth-audio-adsp-boot.service:
  Real hardware/service issue.
  /run/alioth-audio-adsp-boot.log shows ADSP stayed OFFLINING and no ALSA card
  appeared.
  /proc/asound/cards reports no soundcards.
```

Action:

```text
systemctl disable --now fwupd-refresh.timer
systemctl reset-failed fwupd-refresh.service fwupd-refresh.timer
```

Validation:

```text
NFailedUnits=1
Remaining failed unit:
  alioth-audio-adsp-boot.service

fwupd-refresh.timer:
  disabled

Panel:
  /data/experiments/alioth-status-ui-gpu-ui-runtime-v2-health
  MainPID=24911
  NRestarts=0
  ActiveState=active
  SubState=running

Screenshot:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-v2-health-monitor-fwupd-disabled.png

Runtime metrics:
  fps=1
  frame_ms=1049
  page=MONITOR
  panel_hz=120
  draw_ms=26
  gpu_ms=7
  present_ms=15
  total_ms=1049
```

Result:

```text
PASS. The Monitor Health card now reports one real remaining failure instead of
two mixed failure/noise items. Audio remains intentionally visible as unresolved.
```

## 2026-05-14 12:09 CST - Monitor touch hitbox refresh

Intent:

```text
Keep Monitor touch behavior aligned with the current layout after replacing the
old Renderer card with the Health card.
```

Implementation:

```text
Updated:
  src/alioth-status-ui-c/alioth_status_ui.c

Changes:
  - Battery card and USB input card route to Power.
  - Network panel routes to Wi-Fi.
  - Hardware panel routes to GPU.
  - Removed the obsolete top-right Renderer/GPU hitbox so the Health card no
    longer jumps to GPU.
```

Build/deploy:

```text
Build dir:
  /work/panel-build-health-nav-20260514

Panel binary:
  /data/experiments/alioth-status-ui-gpu-ui-runtime-v2-health-nav
  local copy: artifacts/remote/gpu-runtime-20260513/alioth-status-ui-gpu-ui-runtime-v2-health-nav
  sha256: d0116950257c7fc6c35f4dfa634edd15364a33dcb4b70ddc254793d3a599a336

Service override:
  /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
  ExecStart=/data/experiments/alioth-status-ui-gpu-ui-runtime-v2-health-nav
```

Validation:

```text
Service:
  MainPID=25427
  NRestarts=0
  ActiveState=active
  SubState=running

Page request smoke:
  MONITOR, WIFI, POWER, and GPU all rendered after /run/alioth-panel-page
  requests. Physical finger hitbox confirmation remains a human-on-device check.

Final Monitor screenshot:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-v2-health-nav-monitor.png
  width=1080 height=2400 page=MONITOR

Runtime metrics:
  fps=1
  frame_ms=1042
  page=MONITOR
  panel_hz=120
  draw_ms=16
  gpu_ms=6
  present_ms=19
  total_ms=1042
```

Result:

```text
PASS. Monitor touch routing now matches the current layout and no longer treats
the Health card as the old Renderer/GPU card.
```

## 2026-05-14 12:50 CST - Power page touch-first rebuild v1

Intent:

```text
Turn Power from an action-list page into a practical device control page for
battery, USB input, thermal state, screen mode, real backlight brightness, and
confirmed boot/power actions.
```

Implementation:

```text
Updated:
  src/alioth-status-ui-c/alioth_status_ui.c
  src/alioth-panel/panel_paths.h

Changes:
  - Added backlight sysfs path constants for panel0-backlight.
  - Added helpers to read brightness, max_brightness, actual_brightness, and
    bl_power.
  - Added bounded backlight writes through brightness percent controls.
  - Replaced the old Thermal top card with a Backlight metric card.
  - Expanded Live power rows to include Battery, Charge, Thermal, and Screen.
  - Added Display controls panel with Normal, Low, Night, Lamp, and Off chips.
  - Added brightness controls: -, 25%, 50%, 75%, +.
  - Moved power actions lower and kept the existing confirm-before-run flow.
  - Avoided showing actual_brightness=0 as primary truth because this kernel
    reports actual_brightness as 0 while the panel is visibly on; the UI now
    labels the writable brightness value as the sysfs setpoint.
```

Build/deploy:

```text
Build dir:
  /work/panel-build-power-v1-20260514

Panel binary:
  /data/experiments/alioth-status-ui-gpu-ui-runtime-v2-power-v1
  local copy: artifacts/remote/gpu-runtime-20260513/alioth-status-ui-gpu-ui-runtime-v2-power-v1
  sha256: aa39fb53ce44ae53d780a19cb1181d77ff15bdc73ad4392e7e557281bfef5668

Service override:
  /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
  ExecStart=/data/experiments/alioth-status-ui-gpu-ui-runtime-v2-power-v1
```

Validation:

```text
Service:
  MainPID=27857
  NRestarts=0
  ActiveState=active
  SubState=running

Screenshot:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-v2-power-v1-power.png
  width=1080 height=2400 page=POWER screen=UI_POWER_MENU

Runtime metrics after screenshot I/O settled:
  fps=1
  frame_ms=1033
  page=POWER
  panel_hz=120
  draw_ms=12
  gpu_ms=6
  present_ms=14
  total_ms=1033

Backlight at validation:
  brightness=536
  max_brightness=2047
```

Result:

```text
PASS. Power is now a touch-first control page. This validation did not
intentionally change brightness; the new brightness controls will write sysfs
only when touched on the device.
```

## 2026-05-14 13:03 CST - Services and logs tab v1

Intent:

```text
Make Monitor Health actionable by adding a device-visible details page for
systemd service state, failed units, runtime summaries, and panel logs.
```

Implementation:

```text
Updated:
  src/alioth-status-ui-c/alioth_status_ui.c
  src/alioth-panel/app_types.h

Changes:
  - Added PAGE_LOGS as a fifth app page and nav tab.
  - Added /run/alioth-panel-page support for logs/services/service/4.
  - Monitor Health card now routes to LOGS.
  - Added a Services / Logs page with:
      System failed count
      Panel service state
      Audio state
      Key service list
      Wi-Fi runtime summary
      Audio runtime summary
      Panel metrics
      Panel log tail
  - Service details are cached for 5 seconds to avoid spawning systemctl every
    frame.
  - Fixed systemctl show parsing to read ActiveState, SubState, and Result by
    property name rather than relying on --value ordering.
```

Build/deploy:

```text
Build dir:
  /work/panel-build-logs-v1-20260514

Panel binary:
  /data/experiments/alioth-status-ui-gpu-ui-runtime-v2-logs-v1
  local copy: artifacts/remote/gpu-runtime-20260513/alioth-status-ui-gpu-ui-runtime-v2-logs-v1
  sha256: 9102fda8ef21ce9ceb9a8be8d96b32275f30b8e43005e78b8d2c3f0cc077b1fc

Service override:
  /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
  ExecStart=/data/experiments/alioth-status-ui-gpu-ui-runtime-v2-logs-v1
```

Validation:

```text
Service:
  MainPID=28626
  NRestarts=0
  ActiveState=active
  SubState=running

Five-page request smoke:
  monitor, wifi, power, gpu, and logs all rendered through /run/alioth-panel-page.

Screenshot:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-v2-logs-v1-logs.png
  width=1080 height=2400 page=LOGS

Runtime metrics:
  fps=1
  frame_ms=1025
  page=LOGS
  panel_hz=120
  draw_ms=2
  gpu_ms=7
  present_ms=15
  total_ms=1025

Remaining failed unit:
  alioth-audio-adsp-boot.service
```

Result:

```text
PASS. The panel now has five tabs and exposes service health details without
SSH. The remaining Health failure is still the real ADSP audio issue.
```

## 2026-05-14 05:05 CST - CPU hotspot diagnosis and governor controls

Problem:

```text
The user reported C7, the prime core, was almost always at 100%.
```

Live diagnosis:

```text
Immediate cause:
  PID 23982, grep -R con_mode /sys/module /proc/sys
  PSR=7
  CPU around 84-92%
  elapsed over 1 hour

After killing that stale diagnostic grep:
  C7 average idle: 100.00%
  all CPU average idle: 99.38%
  panel process CPU: around 1-2%

Interpretation:
  The spike was not GPU panel rendering. It was a leftover recursive grep over
  procfs/sysfs from the Wi-Fi monitor-mode investigation. The scheduler placed
  the single busy thread on C7 because C7 is the highest-capacity prime core.
```

Implementation:

```text
Updated:
  src/alioth-status-ui-c/alioth_status_ui.c
  src/alioth-panel/app_types.h
  src/alioth-panel/panel_data.h

Changes:
  - Added a lightweight /proc thread sampler with cached per-thread CPU deltas.
  - Monitor CPU section now shows hottest thread/core.
  - Monitor CPU section now shows active governor and cluster frequency summary.
  - Power page now includes Eco / Auto / Perf governor controls.
  - Governor controls map to powersave / schedutil / performance.
  - Monitor CPU panel touch now routes to Power for tuning.
```

Build/deploy:

```text
Build dir:
  /work/panel-build-cpu-tuning-20260514

Panel binary:
  /data/experiments/alioth-status-ui-gpu-ui-runtime-v2-cpu-tuning
  local copy: artifacts/remote/gpu-runtime-20260513/alioth-status-ui-gpu-ui-runtime-v2-cpu-tuning
  sha256: 615c3ea3dc1bfceef363796cfa03aed2b40ac7d5944db75be88cf25abf60a51e

Service override:
  /etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
  ExecStart=/data/experiments/alioth-status-ui-gpu-ui-runtime-v2-cpu-tuning
```

Validation:

```text
Service:
  MainPID=30627
  NRestarts=0
  ExecMainStatus=0
  ActiveState=active
  SubState=running

Governor write smoke:
  powersave: C0/C4/C7 all powersave
  schedutil: C0/C4/C7 all schedutil
  performance: C0/C4/C7 restored to performance

Final live governor:
  C0 performance 1804800
  C4 performance 2419200
  C7 performance 3187200

Runtime metrics:
  fps=1
  frame_ms=1032
  page=POWER
  panel_hz=120
  draw_ms=10
  gpu_ms=6
  present_ms=15
  total_ms=1032

CPU idle proof after cleanup:
  all average idle: 99.12%
  C7 average idle: 100.00%

Screenshots:
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-v2-cpu-tuning-monitor.png
  artifacts/remote/gpu-runtime-20260513/alioth-panel-screenshot-v2-cpu-tuning-power.png
```

Result:

```text
PASS. The panel now exposes the next C7 spike cause directly on Monitor and
provides touch controls for CPU governor tuning. The phone was restored to the
performance governor after validation.
```

## 2026-05-14 thermal guard after native kernel compile heat test

Context:

```text
The phone became physically hot during a native Ubuntu kernel compile benchmark
(`linux-source-6.8.0`, ARCH=arm64, make -j8 Image). The build was interrupted
before completion and all build processes were stopped. CPU governors were
temporarily switched to powersave for cooldown.
```

Live thermal evidence after interruption:

```text
Immediate sample:
  battery/BMS: 40.7C
  cpu_therm:   about 43.9C
  CPU zones:   up to about 44.6C
  PMIC zones:  up to about 46.7C
  Wi-Fi therm: about 44.0C

Cooldown sample after guard install:
  battery: 36.0C
  cpu:     39.1C
  gpu:     38.4C
  pmic:    41.8C
  wifi:    38.8C
```

LineageOS / Android thermal stack finding:

```text
The vendor partition contains Android thermal components:
  /vendor/bin/mi_thermald
  /vendor/bin/thermal-engine
  /vendor/bin/hw/android.hardware.thermal@2.0-service.qti
  /vendor/etc/thermal-*.conf

LineageOS common device sources package android.hardware.thermal-service.qti and
keep Xiaomi thermal profile support in XiaomiParts. XiaomiParts writes profile
values to /sys/class/thermal/thermal_message/sconfig, for example default=0,
dialer=8, gaming=9, benchmark=10, browser=11, camera=12, streaming=14.

In Ubuntu systemd mode those Android init services are not started. The kernel
thermal zones and cooling devices remain visible, but the Android user-space
thermal policy layer is absent.
```

Kernel thermal trip evidence:

```text
Live sysfs / LineageOS kernel DTS agree on these key trip points:
  CPU user_space zones: 115C / 125C passive notifications
  CPU little step zones: 110C passive
  CPU big/perf step zones: 75C and 110C passive
  GPU step zone: 95C passive
  PMIC zones: 95C / 115C / 145C passive

No exposed Linux thermal zone showed a critical trip point in the current live
sysfs sample, so Ubuntu should not rely on the kernel-only path as the first
safety mechanism for long-running server workloads.
```

Implemented protection:

```text
Added:
  scripts/alioth_thermal_guard.sh
  configs/alioth-thermal-guard.service

Installed on phone:
  /usr/local/sbin/alioth-thermal-guard
  /etc/systemd/system/alioth-thermal-guard.service

Service:
  alioth-thermal-guard.service enabled and active
  status file: /run/alioth-thermal-guard.status
  log file:    /var/log/alioth-thermal-guard.log

Policy:
  warm:     battery 40C / CPU 60C / GPU 60C / PMIC 65C -> schedutil
  hot:      battery 42C / CPU 70C / GPU 70C / PMIC 80C -> powersave
  critical: battery 48C / CPU 85C / GPU 85C / PMIC 95C -> powersave
  shutdown: battery 55C / CPU 105C / GPU 105C / PMIC 115C -> systemctl poweroff
```

Validation:

```text
systemctl show alioth-thermal-guard.service:
  ActiveState=active
  SubState=running
  NRestarts=0

/run/alioth-thermal-guard.status:
  action=normal
  reason=within_limits
  battery=36.0C
  cpu=39.1C
  gpu=38.4C
  pmic=41.8C
  wifi=38.8C

Panel service remained active:
  lele-status-ui.service ActiveState=active SubState=running NRestarts=0

Final governor:
  policy0 schedutil
  policy4 schedutil
  policy7 schedutil
```

Result:

```text
PASS. The overheating compile run was stopped, the device cooled, and Ubuntu now
has an always-on conservative thermal guard before further server/compile/GPU
stress tests.
```
