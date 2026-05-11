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
