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

find_part_node() {
  partname="$1"
  fallback="${2:-}"
  for uevent in /sys/class/block/*/uevent; do
    [ -f "$uevent" ] || continue
    if grep -qx "PARTNAME=$partname" "$uevent"; then
      block_dir="${uevent%/uevent}"
      devno="$(cat "$block_dir/dev")"
      make_block_node "$devno" "$partname"
      return 0
    fi
  done

  if [ -n "$fallback" ] && [ -r "/sys/class/block/$fallback/dev" ]; then
    devno="$(cat "/sys/class/block/$fallback/dev")"
    make_block_node "$devno" "$partname"
    return 0
  fi

  return 1
}

find_modem_b_node() {
  find_part_node modem_b sde30
}

ensure_firmware_path() {
  target="$1"
  link="$2"
  parent="$(dirname "$link")"
  mkdir -p "$parent"

  if mountpoint -q "$link" 2>/dev/null; then
    log "$link is already a mountpoint"
    return 0
  fi

  if [ -L "$link" ] || [ ! -e "$link" ]; then
    if ln -sfn "$target" "$link" 2>/tmp/alioth-wifi-link.err; then
      log "linked $link -> $target"
      return 0
    fi
    log "failed to link $link -> $target: $(cat /tmp/alioth-wifi-link.err 2>/dev/null)"
    return 1
  fi

  if [ -d "$link" ]; then
    if mount --bind "$target" "$link" 2>/tmp/alioth-wifi-bind.err; then
      log "bind-mounted $target on existing directory $link"
      return 0
    fi
    log "failed to bind-mount $target on $link: $(cat /tmp/alioth-wifi-bind.err 2>/dev/null)"
    return 1
  fi

  log "$link exists and is neither a symlink nor directory; leaving it untouched"
  return 1
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

ensure_firmware_path "$FW_MNT/image/qca6390" /lib/firmware/qca6390
ensure_firmware_path "$FW_MNT" /vendor/firmware_mnt || true
ensure_firmware_path "$FW_MNT/image" /firmware/image || true

mkdir -p /mnt/vendor/persist
if ! mountpoint -q /mnt/vendor/persist 2>/dev/null; then
  if persist_node="$(find_part_node persist "")"; then
    mount -o ro "$persist_node" /mnt/vendor/persist 2>/dev/null && \
      log "mounted persist at /mnt/vendor/persist from $persist_node" || true
  fi
fi

if [ ! -s /lib/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini ]; then
  for cfg in \
    /data/experiments/wifi-fw-overlay/image/wlan/qca_cld/WCNSS_qcom_cfg.ini \
    /vendor/etc/wifi/qca6390/WCNSS_qcom_cfg.ini \
    /vendor/firmware/wlan/qca_cld/qca6390/WCNSS_qcom_cfg.ini \
    /vendor/etc/wifi/WCNSS_qcom_cfg.ini \
    /vendor/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini; do
    [ -s "$cfg" ] || continue
    cp "$cfg" /lib/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini
    chmod 0644 /lib/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini
    log "restored WCNSS_qcom_cfg.ini from $cfg"
    break
  done
fi

if [ ! -s /lib/firmware/wlan/qca_cld/wlan_mac.bin ]; then
  for mac in \
    /mnt/vendor/persist/wlan_mac.bin \
    /data/experiments/wifi-fw-overlay/image/wlan/qca_cld/wlan_mac.bin \
    /vendor/firmware/wlan/qca_cld/qca6390/wlan_mac.bin \
    /vendor/firmware/wlan/qca_cld/wlan_mac.bin; do
    [ -s "$mac" ] || continue
    cp "$mac" /lib/firmware/wlan/qca_cld/wlan_mac.bin
    chmod 0644 /lib/firmware/wlan/qca_cld/wlan_mac.bin
    log "restored wlan_mac.bin from $mac"
    break
  done
fi

ensure_android_runtime_apex() {
  [ -x /apex/com.android.runtime/bin/linker64 ] && return 0
  apex_src=/system/apex/com.android.runtime.apex
  work=/run/alioth-apex-runtime
  root="$work/root"
  payload="$work/apex_payload.img"

  [ -f "$apex_src" ] || return 1
  command -v python3 >/dev/null 2>&1 || return 1
  command -v debugfs >/dev/null 2>&1 || return 1

  rm -rf "$work"
  mkdir -p "$root" /apex/com.android.runtime
  python3 - "$apex_src" "$work" <<'PY'
import sys
import zipfile

apex, out_dir = sys.argv[1], sys.argv[2]
with zipfile.ZipFile(apex) as zf:
    zf.extract("apex_payload.img", out_dir)
PY
  debugfs -R "rdump / $root" "$payload" >/tmp/alioth-apex-debugfs.out 2>/tmp/alioth-apex-debugfs.err || {
    log "failed to extract Android runtime APEX: $(cat /tmp/alioth-apex-debugfs.err 2>/dev/null)"
    return 1
  }
  if mountpoint -q /apex/com.android.runtime 2>/dev/null; then
    log "Android runtime APEX is already mounted"
  elif mount --bind "$root" /apex/com.android.runtime 2>/tmp/alioth-apex-bind.err; then
    log "materialized Android runtime APEX from $apex_src"
  else
    log "failed to bind Android runtime APEX: $(cat /tmp/alioth-apex-bind.err 2>/dev/null)"
    return 1
  fi
  [ -x /apex/com.android.runtime/bin/linker64 ]
}

ensure_android_runtime_apex || true

android_linker=
android_qrtr=
android_cnss=
if [ -x /vendor/bin/qrtr-ns ] && [ -x /vendor/bin/cnss-daemon ]; then
  if [ -x /system/bin/linker64 ]; then
    android_linker=/system/bin/linker64
  elif [ -x /apex/com.android.runtime/bin/linker64 ]; then
    android_linker=/apex/com.android.runtime/bin/linker64
  fi
  if [ -n "$android_linker" ]; then
    android_qrtr=/vendor/bin/qrtr-ns
    android_cnss=/vendor/bin/cnss-daemon
  fi
fi
if [ -z "$android_linker" ]; then
  android_linker="$RUNTIME/apex/com.android.runtime/bin/linker64"
  android_qrtr="$RUNTIME/vendor/bin/qrtr-ns"
  android_cnss="$RUNTIME/vendor/bin/cnss-daemon"
fi

for required in \
  "$android_linker" \
  "$android_qrtr" \
  "$android_cnss" \
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
