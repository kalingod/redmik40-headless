#!/bin/sh
set -eu

LOG="${ALIOTH_WIFI_LOG:-/run/alioth-wifi-bringup.log}"
FW_MNT="${ALIOTH_WIFI_FW_MNT:-/run/alioth-firmware-mnt}"
BLOCK_DIR="${ALIOTH_WIFI_BLOCK_DIR:-/run/alioth-block}"
RUNTIME="${ALIOTH_CNSS_RUNTIME:-/data/experiments/android-cnss-runtime}"

log() {
  mkdir -p /run
  printf '[%s] alioth-wifi-prepare: %s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ')" "$*" | tee -a "$LOG"
}

make_block_node() {
  devno="$1"
  name="$2"
  major="${devno%:*}"
  minor="${devno#*:}"
  mkdir -p "$BLOCK_DIR"
  rm -f "$BLOCK_DIR/$name"
  mknod "$BLOCK_DIR/$name" b "$major" "$minor"
  printf '%s\n' "$BLOCK_DIR/$name"
}

find_modem_b_node() {
  for uevent in /sys/class/block/*/uevent; do
    [ -f "$uevent" ] || continue
    if grep -qx 'PARTNAME=modem_b' "$uevent"; then
      block_dir="${uevent%/uevent}"
      devno="$(cat "$block_dir/dev")"
      make_block_node "$devno" modem_b
      return 0
    fi
  done

  if [ -r /sys/class/block/sde30/dev ]; then
    devno="$(cat /sys/class/block/sde30/dev)"
    make_block_node "$devno" modem_b
    return 0
  fi

  return 1
}

ensure_symlink() {
  target="$1"
  link="$2"
  if [ -L "$link" ] || [ ! -e "$link" ]; then
    ln -sfn "$target" "$link"
  else
    log "$link exists and is not a symlink; leaving it untouched"
    return 1
  fi
}

mkdir -p "$FW_MNT" /lib/firmware /lib/firmware/wlan/qca_cld /vendor /firmware

if ! mountpoint -q "$FW_MNT"; then
  modem_node="$(find_modem_b_node)" || {
    log "cannot find modem_b block device"
    exit 2
  }
  mount -o ro "$modem_node" "$FW_MNT"
  log "mounted modem_b firmware at $FW_MNT from $modem_node"
else
  log "$FW_MNT already mounted"
fi

ensure_symlink "$FW_MNT/image/qca6390" /lib/firmware/qca6390
ensure_symlink "$FW_MNT" /vendor/firmware_mnt
ensure_symlink "$FW_MNT/image" /firmware/image

if [ ! -s /lib/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini ]; then
  if [ -s /data/experiments/wifi-fw-overlay/image/wlan/qca_cld/WCNSS_qcom_cfg.ini ]; then
    cp /data/experiments/wifi-fw-overlay/image/wlan/qca_cld/WCNSS_qcom_cfg.ini \
      /lib/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini
    chmod 0644 /lib/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini
    log "restored WCNSS_qcom_cfg.ini from /data experiment overlay"
  else
    log "missing /lib/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini"
    exit 3
  fi
fi

for required in \
  "$RUNTIME/apex/com.android.runtime/bin/linker64" \
  "$RUNTIME/vendor/bin/qrtr-ns" \
  "$RUNTIME/vendor/bin/cnss-daemon" \
  /lib/firmware/qca6390/amss20.bin \
  /lib/firmware/qca6390/bd_k11a.elf \
  /lib/firmware/qca6390/m3.bin \
  /lib/firmware/qca6390/regdb.bin \
  /lib/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini; do
  if [ ! -e "$required" ]; then
    log "required file missing: $required"
    exit 4
  fi
done

printf '\n' > /sys/module/firmware_class/parameters/path 2>/dev/null || true

log "firmware paths ready"
if [ -s /lib/firmware/wlan/qca_cld/wlan_mac.bin ]; then
  log "wlan_mac.bin present: $(sha256sum /lib/firmware/wlan/qca_cld/wlan_mac.bin 2>/dev/null)"
fi
ls -l /lib/firmware/qca6390 /vendor/firmware_mnt /firmware/image \
  /lib/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini \
  /lib/firmware/wlan/qca_cld/wlan_mac.bin 2>&1 | tee -a "$LOG"
