# Remote Build Executor Plan

This is the task brief for another AI executor. The executor should follow the
steps as written and avoid inventing new infrastructure unless a step is blocked.

## Goal

Provide a reliable remote build workflow for Redmi K40 alioth kernel and boot
image work.

The local macOS host remains the control plane. The remote build machine keeps
the large source trees, build outputs, and ccache hot. The phone is only a test
target. The executor should move small inputs to the build machine and fetch
small final artifacts back.

## Non-Negotiable Rules

- Do not flash the phone. Device tests use `fastboot boot` only.
- Do not mutate macOS host network configuration. No automatic `networksetup`,
  `ifconfig`, `route`, or DHCP/network-service changes.
- Do not clone or sync full Android/LineageOS source trees onto the Mac unless
  the user explicitly asks.
- Do not upload full source trees from the Mac to the remote machine.
- Do not delete remote source or cache directories.
- Do not use stale port-open checks as success. Success requires command output
  or build artifacts with checksums.
- Keep each experiment in a named output/artifact directory. Never overwrite the
  current known-good artifact without a new name.

## Known Hosts And Paths

Remote SSH endpoint:

```text
ssh -p 60022 lele@47.98.228.151
```

Remote workspace:

```text
/home/lele/redmik40-build/lineage-sm8250
```

Remote kernel source:

```text
/home/lele/redmik40-build/lineage-sm8250/kernel
```

Remote source policy:

```text
origin   https://github.com/kalingod/android_kernel_xiaomi_sm8250.git
upstream https://github.com/xiaomi-sm8250-devs/android_kernel_xiaomi_sm8250
branch   lineage-20-headless
```

Remote artifact root:

```text
/home/lele/redmik40-build/lineage-sm8250/artifacts
```

Local artifact root:

```text
artifacts/remote
```

Builder image expected by existing scripts:

```text
redmik40-kernel-builder:bullseye
```

## Existing Local Tools

Use these instead of rewriting from scratch unless they are missing a required
capability:

```text
scripts/build_lineage_sm8250_alioth_kernel.sh
scripts/build_lineage_mininitramfs_boot.sh
scripts/repack_alioth_boot_ramdisk.py
scripts/audit_alioth_boot_image.py
scripts/fetch_remote_kernel_artifacts.sh
scripts/validate_alioth_kernel_candidate.sh
docker/kernel-builder/Dockerfile
```

Important limitation:

```text
scripts/validate_alioth_kernel_candidate.sh uses an OrangeFox ramdisk and is
only a kernel/recovery smoke path. It is not valid evidence for Ubuntu systemd,
ModemManager, Wi-Fi, audio, KGSL, or normal rootfs behavior.
```

## Architecture

The workflow has four lanes:

```text
Mac control repo
  -> writes patch/config/build request
  -> sends small request to remote

Remote build machine
  -> applies patch to persistent kernel tree
  -> builds with persistent out directory and ccache
  -> writes named artifact directory

Mac artifact intake
  -> fetches only Image, dtb/dtbo, config, SHA256SUMS, build log
  -> audits or packs boot image locally

Phone validation
  -> fastboot boot only
  -> verify with SSH/2323 command output
```

## Build Request Contract

Every build request should have a unique artifact name:

```text
<topic>-<date-or-task-id>
```

Examples:

```text
kernel-lineage20-alioth-j36
wifi-prepare-debug-20260512
panel-lineage-20-headless-7
```

Required request fields:

```text
artifact_name
branch
base_commit
patch_or_config_fragment
clean_build yes/no
jobs
expected_outputs
reason_for_build
```

Default values:

```text
branch=lineage-20-headless
clean_build=no
jobs=36
SYSTEMD_FRIENDLY_CONFIG=yes
```

## Remote Build Command Pattern

The executor should first inspect, not mutate:

```bash
ssh -p 60022 lele@47.98.228.151 '
  set -e
  cd /home/lele/redmik40-build/lineage-sm8250/kernel
  git status --short
  git branch --show-current
  git rev-parse HEAD
  git remote -v
'
```

If the working tree is dirty, do not reset it. Report the dirty files and ask
for direction unless the dirty files are exactly the patch set the executor was
asked to use.

For a normal kernel build, run on the remote:

```bash
cd /home/lele/redmik40-build/lineage-sm8250
OUT_DIR=/home/lele/redmik40-build/lineage-sm8250/out/<artifact_name> \
ARTIFACT_DIR=/home/lele/redmik40-build/lineage-sm8250/artifacts/<artifact_name> \
SYSTEMD_FRIENDLY_CONFIG=yes \
CLEAN_BUILD=no \
JOBS=36 \
bash scripts/build_lineage_sm8250_alioth_kernel.sh
```

If the remote script is not present, the executor should copy the local script
to the remote workspace rather than hand-typing a new build flow.

Required remote outputs:

```text
artifacts/<artifact_name>/Image
artifacts/<artifact_name>/config
artifacts/<artifact_name>/SHA256SUMS
optional: alioth*.dtb, alioth*.dtbo, Image.gz
optional but preferred: build.log, git-rev.txt, build-env.txt
```

## Artifact Fetch

Fetch by SSH/SCP, not browser download:

```bash
scripts/fetch_remote_kernel_artifacts.sh <artifact_name>
```

Manual equivalent:

```bash
mkdir -p artifacts/remote/<artifact_name>
scp -P 60022 \
  -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/tmp/redmik40_remote_known_hosts \
  lele@47.98.228.151:/home/lele/redmik40-build/lineage-sm8250/artifacts/<artifact_name>/* \
  artifacts/remote/<artifact_name>/
```

Immediately verify:

```bash
shasum -a 256 -c artifacts/remote/<artifact_name>/SHA256SUMS
```

If `SHA256SUMS` uses remote-relative paths, the executor should still verify
the listed file hashes manually and record the exact command used.

## Boot Image Production

For current Ubuntu/systemd validation, use the mininitramfs boot-image path, not
the OrangeFox smoke path.

Current best temporary boot baseline:

```text
artifacts/experiments/exp3b-ubuntu-arch-rootfs-mac-ssh-key/lineage-mininitramfs-boot-exp3b-mac-ssh-key.img
```

Current validated behavior:

```text
Ubuntu 24.04.4 LTS
PID1 systemd
root /dev/block/by-name/arch[/data/rootfs/ubuntu-24.04]
SSH key alioth-usb-mac works
remaining failed unit: alioth-wifi-prepare.service
```

When replacing only the kernel, preserve the validated initramfs logic unless
the task explicitly targets initramfs behavior.

Recommended build:

```bash
KERNEL_IMG=artifacts/remote/<artifact_name>/Image \
OUT_DIR=artifacts/experiments/<boot_artifact_name> \
SSH_PUBKEY_FILE=/Users/wuyuele/.ssh/alioth_usb_ed25519.pub \
INIT_MODE=switchroot-ubuntu-systemd \
UBUNTU_ROOTFS_PATH=/rootfs/ubuntu-24.04 \
bash scripts/build_lineage_mininitramfs_boot.sh build
```

If only repacking a known-good ramdisk, use:

```bash
./scripts/repack_alioth_boot_ramdisk.py \
  artifacts/control/lineage-mininitramfs-boot.img \
  artifacts/experiments/<candidate>/ramdisk \
  artifacts/experiments/<candidate>/lineage-mininitramfs-boot-<candidate>.img \
  --out-ramdisk artifacts/experiments/<candidate>/ramdisk-<candidate>.cpio.gz
```

Every boot image candidate must pass:

```bash
./scripts/audit_alioth_boot_image.py \
  artifacts/experiments/<candidate>/lineage-mininitramfs-boot-<candidate>.img
```

Required audit properties:

```text
Android boot v3 header
header_size=1580
cmdline=twrpfastboot=1
gzip ramdisk decodes
ramdisk first bytes are 070701 or 070702
candidate marker is present
```

## Phone Validation

Before boot:

```bash
route -n get 172.16.42.2
ssh -i /Users/wuyuele/.ssh/alioth_usb_ed25519 \
  -o BatchMode=yes \
  -o ConnectTimeout=5 \
  -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/dev/null \
  root@172.16.42.2 'hostname; ps -p 1 -o comm=; head -n 5 /etc/os-release'
```

To reboot current Ubuntu/systemd into bootloader:

```bash
ssh -i /Users/wuyuele/.ssh/alioth_usb_ed25519 \
  -o BatchMode=yes \
  -o ConnectTimeout=5 \
  -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/dev/null \
  root@172.16.42.2 '/var/tmp/alioth-switchroot/alioth-reboot bootloader'
```

If SSH is unavailable but 2323 works:

```bash
printf '/var/tmp/alioth-switchroot/alioth-reboot bootloader\n' | nc -w 5 172.16.42.2 2323
```

Boot:

```bash
./tools/platform-tools/fastboot devices -l
./tools/platform-tools/fastboot boot artifacts/experiments/<candidate>/lineage-mininitramfs-boot-<candidate>.img
```

After boot, verify with command output:

```bash
ssh -i /Users/wuyuele/.ssh/alioth_usb_ed25519 \
  -o BatchMode=yes \
  -o ConnectTimeout=8 \
  -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/dev/null \
  root@172.16.42.2 '
    hostname
    uname -a
    echo PID1=$(ps -p 1 -o comm=)
    head -n 5 /etc/os-release
    findmnt /
    systemctl is-system-running || true
    systemctl --failed --no-pager || true
    ls -la /sys/fs/pstore 2>/dev/null || true
    dmesg | grep alioth-mininit | tail -n 60
  '
```

If macOS sees the USB gadget but `172.16.42.2` is unreachable, stop and ask the
user to wake/replug/open the Network UI. Do not run host network mutations.

## Documentation Requirements

Before a risky test, append the plan to:

```text
docs/boot-validation-log.md
```

After execution, append:

```text
candidate path
SHA256
exact command
fastboot result
route/interface observation
SSH or 2323 command output summary
rootfs/PID1/systemd state
pstore state
remaining failures
```

Also update if the state changes:

```text
docs/current-state.md
docs/session-handoff-2026-05-11.md
docs/resources.md
```

## Executor Checklist

1. Read `docs/current-state.md`, `docs/boot-validation-log.md`, and this file.
2. Confirm the exact requested kernel/initramfs change.
3. Inspect local and remote git status before editing.
4. Apply only the requested patch/config change.
5. Build remotely using persistent source/cache.
6. Fetch only the named artifact directory.
7. Verify checksums.
8. Build or repack a boot image using the validated Ubuntu mininitramfs path.
9. Audit the boot image.
10. Write the boot plan to `docs/boot-validation-log.md`.
11. `fastboot boot` only.
12. Verify with SSH/2323 command output.
13. Record results and remaining issues.

## Stop Conditions

Stop and ask the user if any of these happen:

```text
remote kernel tree has unexpected dirty changes
remote branch is not lineage-20-headless and the task did not ask for that
build requires a clean reset or deleting cache/source
candidate boot image fails audit
phone enters recovery or no rescue path returns
macOS USB network route/interface disappears
any command would flash, erase, set_active, or mutate host network
```
