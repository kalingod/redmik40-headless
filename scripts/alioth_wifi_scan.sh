#!/bin/sh
set -eu

find_managed_iface() {
  if command -v iw >/dev/null 2>&1; then
    iw dev 2>/dev/null | awk '
      $1 == "Interface" { iface = $2; next }
      $1 == "type" && $2 == "managed" && iface != "" { print iface; exit }
    ' | head -n 1
    return 0
  fi

  for path in /sys/class/net/wlan* /sys/class/net/wlp*; do
    [ -e "$path" ] || continue
    basename "$path"
    return 0
  done
}

systemctl start alioth-wifi-bringup.service >/dev/null 2>&1 || true

iface="${ALIOTH_WIFI_IFACE:-$(find_managed_iface || true)}"
if [ -z "$iface" ]; then
  echo "no managed Wi-Fi interface found" >&2
  exit 1
fi

ip link set "$iface" up 2>/dev/null || true

if [ "${ALIOTH_WIFI_RAW_SCAN:-0}" = "1" ]; then
  exec iw dev "$iface" scan
fi

iw dev "$iface" scan | awk -v iface="$iface" '
  function emit() {
    if (bssid == "") return
    if (ssid == "") ssid = "<hidden>"
    printf "%-18s %7s  %5s  %s\n", bssid, signal, freq, ssid
  }
  BEGIN {
    printf "iface=%s\n", iface
    printf "%-18s %7s  %5s  %s\n", "BSSID", "SIGNAL", "FREQ", "SSID"
  }
  /^BSS / {
    emit()
    bssid = $2
    sub(/\(.*/, "", bssid)
    signal = ""
    freq = ""
    ssid = ""
    next
  }
  /^[ \t]*signal:/ {
    signal = $2
    next
  }
  /^[ \t]*freq:/ {
    freq = $2
    next
  }
  /^[ \t]*SSID:/ {
    sub(/^[ \t]*SSID:[ \t]*/, "")
    ssid = $0
    next
  }
  END { emit() }
'
