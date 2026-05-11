# Boot Observability Plan

The next priority is evidence, not blind boot-image changes.

## Goal

Establish a boot logging loop that can answer:

```text
Did the kernel start?
Did /init execute?
Which initramfs step failed?
Did the device panic, watchdog, reboot, or intentionally enter recovery?
Can logs be retrieved after a failed boot?
```

## Layers

### Layer 1: pstore / ramoops

Preferred for kernel and panic logs because it does not require the rootfs to mount.

Check from a working Linux or recovery environment:

```sh
mount -t pstore pstore /sys/fs/pstore 2>/dev/null || true
ls -l /sys/fs/pstore
dmesg | grep -iE 'pstore|ramoops|oops'
zcat /proc/config.gz 2>/dev/null | grep -E 'PSTORE|RAMOOPS|PMSG'
```

Useful kernel config if a rebuild becomes necessary:

```text
CONFIG_PSTORE=y
CONFIG_PSTORE_RAM=y
CONFIG_PSTORE_CONSOLE=y
CONFIG_PSTORE_PMSG=y
CONFIG_PSTORE_FTRACE=y
CONFIG_MAGIC_SYSRQ=y
```

### Layer 2: kernel cmdline logging

LOG-1 candidate added:

```text
loglevel=8 ignore_loglevel printk.time=1 initcall_debug log_buf_len=8M panic=10 oops=panic
```

Candidate:

```text
artifacts/experiments/log1-kernel-observability/lineage-mininitramfs-boot-log1.img
sha256: cd1317909317888fab1275bd81283715a77cbd6c62cdcae124e72e3c71c11701
```

Result so far:

```text
fastboot boot protocol succeeded, but SSH/2323 did not return command output.
Do not treat TCP open as success.
Need pstore/recovery log retrieval to interpret.
```

### Layer 3: initramfs file logs

Only after ramdisk repacking is proven safe.

Target pattern:

```sh
mkdir -p /run
exec >/run/alioth-initramfs.log 2>&1
set -x
echo '[initramfs] start'
cat /proc/cmdline
dmesg -n 8
```

Then copy logs to a known ext4 root after it is safely mounted:

```text
/mnt/arch/data/bootlogs/<run-id>/
```

## LOG-0: ramdisk repack baseline

Purpose:

```text
Prove that the current macOS ramdisk unpack/repack flow can still boot the original Arch/control path.
```

Candidate:

```text
artifacts/experiments/log0-ramdisk-repack-baseline/out/lineage-mininitramfs-boot-log0-repack.img
sha256: 0a2b49834b8d7a6f13c20cb13b33b3d425facedeb87657ee755440e0ce2388bd
```

Change:

```text
Added /etc/alioth-log0-marker only.
No init mode change.
No switchroot change.
No cmdline change.
```

Success:

```text
Boots to the same Arch/control path as the original control image.
2323 or SSH returns command output.
/etc/alioth-log0-marker exists in initramfs only before switch_root, if inspected there.
```

Failure:

```text
If LOG-0 enters recovery, the ramdisk reconstruction process is not safe.
Audit cpio format, file order, symlinks, permissions, device nodes, gzip format, boot header, offsets, and padding before making any further functional changes.
```
