# Session handoff: 2026-05-12

## Current state

The touch/Wi-Fi/status panel runtime changes have now been persisted into
`boot_a`.

Verified after flashing and rebooting:

```text
slot: _a
/var/tmp/alioth-switchroot/alioth-status-ui-cpu:
  e1bee54958982e2d481cfff7c6a28de4ae5b73d98a5066c7cec3e8f315087127
/usr/local/sbin/alioth-wifi-scan:
  28073a3f4ad6c05ce1f08b10d3480dcce5c1fbf0f1773bd0f7889b8d4d22c50d
lele-status-ui.service: active
SSH over USB/NCM: working at 172.16.42.2
```

This fixes the earlier problem where replacing
`/var/tmp/alioth-switchroot/alioth-status-ui-cpu` only changed the live
unpacked ramdisk and was lost after reboot.

## Persisted boot artifact

```text
artifacts/experiments/exp4-touch-wifi-panel-persist/lineage-mininitramfs-boot-touch-wifi-panel.img
sha256: 11ddc5806f285757d3d110fbe17a6ba35a0a4f1b0375d9a6a1a718250453ebe5
size: 46M
```

Repacked ramdisk:

```text
artifacts/experiments/exp4-touch-wifi-panel-persist/ramdisk.cpio.gz
sha256: 3e6f2594c45d3cfda85965842c6b1791e7c4bb853ad9ea6b4f40b95755ab3eec
size: 1.7M
```

The image was based on the previously working image:

```text
artifacts/experiments/exp4-panel-menu-autoboot/lineage-mininitramfs-boot-panel-menu.img
sha256: 1243d8dc0f32434213a2298d1a3a48987712352ef9137a01aa37517f7c2ce351
```

Only the ramdisk was changed. The boot header, kernel, and cmdline were
preserved by `scripts/repack_alioth_boot_ramdisk.py`.

Audit passed before flashing:

```text
header_version=3
header_size=1580
cmdline=twrpfastboot=1
ramdisk first8=07070100
newc entries=99
```

## Files replaced inside the persisted ramdisk

```text
ramdisk/bin/alioth-status-ui-cpu
  e1bee54958982e2d481cfff7c6a28de4ae5b73d98a5066c7cec3e8f315087127

ramdisk/etc/alioth-rootfs-overlay/usr/local/sbin/alioth-wifi-scan
  28073a3f4ad6c05ce1f08b10d3480dcce5c1fbf0f1773bd0f7889b8d4d22c50d
```

## Flash result

Device entered fastboot successfully:

```text
fastboot devices -l:
  43808be9 fastboot usb:338690048X
fastboot getvar current-slot:
  current-slot: a
fastboot getvar product:
  product: alioth
```

Flashed command:

```bash
tools/platform-tools/fastboot flash boot_a \
  artifacts/experiments/exp4-touch-wifi-panel-persist/lineage-mininitramfs-boot-touch-wifi-panel.img
```

Result:

```text
Sending 'boot_a' (47292 KB) OKAY
Writing 'boot_a' OKAY
Finished. Total time: 1.278s
```

Then:

```bash
tools/platform-tools/fastboot reboot
```

SSH returned after reboot and verified the persisted panel hash.

## Current panel capabilities

The currently persisted CPU-rendered panel includes:

```text
Monitor page with status, Wi-Fi, battery, thermal, memory, load, and storage.
Wi-Fi page with scan results, SSID selection, plaintext password field, soft keyboard, connect, clear, and rescan.
Power page with reboot, bootloader, and poweroff actions.
Bottom navigation between Monitor, Wi-Fi, and Power pages.
Touch input through the fts_ts event device.
Screenshot request through /run/alioth-panel-screenshot.request.
```

Known behavior:

```text
The panel uses CPU framebuffer rendering for now.
GPU panel work remains optional; CPU rendering is good enough for this service panel.
Poweroff may still be limited by device/USB/PMIC behavior, especially when USB is attached.
Do not format or repurpose the large userdata partition without explicit confirmation.
```

## Useful commands for the next session

SSH:

```bash
ssh -i /Users/wuyuele/.ssh/alioth_usb_ed25519 \
  -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/tmp/alioth_exp4_known_hosts \
  root@172.16.42.2
```

Verify persisted panel:

```bash
ssh -i /Users/wuyuele/.ssh/alioth_usb_ed25519 \
  -o BatchMode=yes \
  -o ConnectTimeout=6 \
  -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/tmp/alioth_exp4_known_hosts \
  root@172.16.42.2 \
  'sha256sum /var/tmp/alioth-switchroot/alioth-status-ui-cpu /usr/local/sbin/alioth-wifi-scan; systemctl is-active lele-status-ui.service'
```

Request a panel screenshot:

```bash
ssh -i /Users/wuyuele/.ssh/alioth_usb_ed25519 \
  -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/tmp/alioth_exp4_known_hosts \
  root@172.16.42.2 \
  'touch /run/alioth-panel-screenshot.request'
```

The screenshot path on device is:

```text
/run/alioth-panel-screenshot.bmp
```

## Next work

Recommended next steps:

```text
1. Keep this image as the current known-good persisted baseline.
2. Improve panel layout and touch hit areas incrementally.
3. Add better Wi-Fi error display after failed connect attempts.
4. Decide separately whether to use the large userdata partition for /srv.
5. If using userdata for /srv, ask for explicit approval before formatting or mounting.
```
