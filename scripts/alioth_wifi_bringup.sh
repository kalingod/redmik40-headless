#!/bin/sh
set -eu

LOG="${ALIOTH_WIFI_LOG:-/run/alioth-wifi-bringup.log}"
STATE="${ALIOTH_WIFI_STATE:-/run/alioth-wifi-state}"

log() {
  mkdir -p /run
  printf '[%s] alioth-wifi-bringup: %s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ')" "$*" | tee -a "$LOG"
}

find_managed_iface() {
  if command -v iw >/dev/null 2>&1; then
    iw dev 2>/dev/null | awk '
      $1 == "Interface" { iface = $2; next }
      $1 == "type" && $2 == "managed" && iface != "" { print iface; exit }
    ' | head -n 1
    return 0
  fi

  for path in /sys/class/net/wlp* /sys/class/net/wlan*; do
    [ -e "$path" ] || continue
    basename "$path"
    return 0
  done
}

wait_for_process() {
  pattern="$1"
  name="$2"
  for _ in $(seq 1 20); do
    if pgrep -f "$pattern" >/dev/null 2>&1; then
      log "$name is running"
      return 0
    fi
    sleep 1
  done
  log "$name is not running"
  return 1
}

wait_for_calibration() {
  for i in $(seq 1 100); do
    if dmesg | tail -n 600 | grep -q 'Calibration completed successfully'; then
      log "calibration completed at poll $i"
      return 0
    fi
    if dmesg | tail -n 300 | grep -q 'Calibration timed out\|cold boot calibration failed'; then
      log "calibration failure seen at poll $i"
      return 1
    fi
    sleep 1
  done
  log "calibration wait timed out"
  return 1
}

create_wlan_node() {
  devno="$(cat /sys/class/wlan/wlan/dev 2>/dev/null || cat /sys/devices/virtual/wlan/wlan/dev 2>/dev/null || true)"
  if [ -z "$devno" ]; then
    log "cannot find qcwlanstate char device"
    return 1
  fi
  major="${devno%:*}"
  minor="${devno#*:}"
  rm -f /dev/wlan
  mknod /dev/wlan c "$major" "$minor"
  chmod 0660 /dev/wlan
  log "created /dev/wlan c $major $minor"
}

write_state() {
  iface="$1"
  {
    printf 'mode=client-ready\n'
    printf 'iface=%s\n' "$iface"
    printf 'boot_id=%s\n' "$(cat /proc/sys/kernel/random/boot_id 2>/dev/null || true)"
    printf 'updated_utc=%s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
    ip -brief addr show "$iface" 2>/dev/null | sed 's/^/ip=/'
    if command -v iw >/dev/null 2>&1; then
      iw dev "$iface" link 2>/dev/null | sed 's/^/iw_link=/'
    fi
  } > "$STATE"
}

log "starting"
/usr/local/sbin/alioth-wifi-prepare

iface="$(find_managed_iface || true)"
if [ -n "$iface" ]; then
  ip link set "$iface" up 2>/dev/null || true
  log "managed interface already present: $iface"
  write_state "$iface"
  exit 0
fi

wait_for_process 'vendor/bin/qrtr-ns' qrtr-ns
wait_for_process 'vendor/bin/cnss-daemon' cnss-daemon

fs_ready_node=""
for node in /sys/kernel/cnss/fs_ready /sys/devices/platform/soc/*qcom,cnss-qca6390/fs_ready /sys/devices/platform/soc/*cnss*/fs_ready; do
  [ -e "$node" ] || continue
  fs_ready_node="$node"
  break
done

if [ -z "$fs_ready_node" ]; then
  log "cannot find CNSS fs_ready node"
  exit 2
fi

log "writing fs_ready via $fs_ready_node"
printf '1\n' > "$fs_ready_node"
wait_for_calibration || {
  dmesg | grep -iE 'cnss|wlan|qca|wlfw|calibration|firmware|qmi|qrtr|failed|timeout' | tail -220 >> "$LOG" 2>&1 || true
  exit 3
}

sleep 3
if ! pgrep -f 'vendor/bin/cnss-daemon' >/dev/null 2>&1; then
  log "cnss-daemon is not running after calibration; asking systemd to restart it"
  systemctl restart alioth-cnss-daemon.service 2>>"$LOG" || true
  wait_for_process 'vendor/bin/cnss-daemon' cnss-daemon
fi

create_wlan_node
log "writing ON to /dev/wlan"
printf 'ON\n' > /dev/wlan

for i in $(seq 1 90); do
  iface="$(find_managed_iface || true)"
  if [ -n "$iface" ]; then
    ip link set "$iface" up 2>/dev/null || true
    log "managed interface ready: $iface at poll $i"
    write_state "$iface"
    exit 0
  fi
  sleep 1
done

log "no managed Wi-Fi interface appeared"
dmesg | grep -iE 'cnss|wlan|qca|wlfw|calibration|firmware|qmi|qrtr|failed|timeout' | tail -260 >> "$LOG" 2>&1 || true
exit 4
