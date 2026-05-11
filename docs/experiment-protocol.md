# Experiment Protocol

Use this before any action that can change boot, kernel, initramfs, rootfs, services, networking, display ownership, USB role, firmware paths, or hardware state.

## Rule

Every risky experiment needs a short written plan before execution and a result entry after execution. Use `boot-validation-log.md` as the default log unless a more specific experiment log exists.

## Risk classes

| Class | Examples | Requirement |
|---|---|---|
| Read-only | inventory commands, file hashing, passive status checks | record only if the result changes project understanding |
| Service change | enabling/disabling systemd units, Wi-Fi/audio/Bluetooth helpers | record plan, command, expected recovery |
| Display takeover | stopping status UI, running DRM/KMS, Vulkan, Wayland, compositor tests | record how DRM/KMS owner is restored |
| USB role/network | USB host/OTG/HID tests, NCM changes, DHCP changes | record how SSH is expected to recover |
| Boot image | `fastboot boot`, initramfs changes, kernel candidate | record image path, SHA256, source, rollback |
| Flash/write partition | `fastboot flash`, `dd` to block devices, dtbo/vendor/super/modem changes | explicit approval, backup path, rollback image, and no single-step irreversible change |

## Pre-experiment template

```markdown
## YYYY-MM-DD HH:MM CST - short title

Intent:

Host:

Device baseline assumed:

Files/images involved:

SHA256:

Commands planned:

Expected success signal:

Expected failure signal:

Rollback/recovery:

Stop conditions:
```

## Post-experiment template

```markdown
Result:

Commands actually run:

Observed output summary:

Device state after:

Artifacts produced:

Follow-up:
```

## Stop conditions

- Device no longer responds on the expected rescue path.
- Display is blank and there is no known remote path.
- `fastboot getvar current-slot` disagrees with the planned slot.
- A required rollback image or SSH key is missing.
- The command would write `vendor_boot`, `dtbo`, `vbmeta`, `super`, `modem`, bootloader, or firmware without a separate rollback plan.
- The status UI cannot be restored after a display experiment.

## Default safety preference

Use:

```bash
./tools/platform-tools/fastboot boot <image>
```

Avoid until explicitly planned:

```bash
./tools/platform-tools/fastboot flash <partition> <image>
```

## Current planned experiment family

The next experiment family is:

```text
Ubuntu-systemd-first boot with local DRM/KMS panel
```

Experiment intent:

```text
Make Ubuntu 24.04 systemd the main PID1.
Keep BusyBox/initramfs only as rescue and early boot glue.
Do not use Arch as the main rootfs.
Keep the built-in screen as a status panel through DRM/KMS + KGSL/Vulkan.
Do not start a full desktop stack by default.
```

Phase 0 is read-only:

```text
Inspect current switchroot logic.
Identify why current boot lands in Arch/switchroot-shell.
Map the rootfs selection mechanism.
Confirm rescue paths before any boot image modification.
```

Phase 1 must use `fastboot boot` only:

```text
Build or select a candidate boot image.
Temporarily boot it.
Validate Ubuntu PID1, network, rescue path, and panel service behavior.
Do not flash.
```

Phase 2 can consider flashing only after:

```text
candidate was booted successfully more than once
rollback image and command are documented
current slot is confirmed
SSH or 2323 rescue access is confirmed
panel failure cannot block remote recovery
```

## Experiment 1 candidate

Current candidate:

```text
artifacts/control/lineage-mininitramfs-boot.img
sha256: 19e159007fb04e009f840b09dfb33b68808ac45618403c63e80f2851e7608a23
```

Reason:

```text
The image SHA256 matches the first 48,373,760 bytes of the phone's boot_b partition.
The live phone is currently booted from _a, so testing this image with fastboot boot should reproduce the deployed boot_b path without flashing or slot changes.
```

Experiment 1 is allowed to:

```text
reboot the phone into fastboot if explicitly approved
run fastboot boot with the control image
perform read-only post-boot checks
```

Experiment 1 is not allowed to:

```text
fastboot flash
set-active slot
modify rootfs
modify boot partitions
change panel binaries or systemd services
```

Experiment 1 actual result:

```text
fastboot boot of the control image succeeded.
The device still booted Arch Linux ARM with alioth-switch-init switchroot-shell as PID1.
Ubuntu rootfs was present but not used as /.
systemctl remained offline.
CPU fallback status UI started on DRM/KMS.
```

Next experiment family:

```text
Experiment 2: Ubuntu-systemd-first initramfs candidate.
```

Experiment 2 should change only the candidate boot image/initramfs, not live partitions:

```text
unpack or reconstruct boot image
change switchroot policy to target /data/rootfs/ubuntu-24.04 /sbin/init
preserve USB NCM and 2323 rescue
boot with fastboot boot only
collect read-only post-boot evidence
```
