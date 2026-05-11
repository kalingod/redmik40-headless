#!/bin/sh
set -eu

CONFIG_PATH="${ALIOTH_WIFI_CONFIG:-/etc/alioth-wifi-default}"
RUN_DIR="${ALIOTH_WIFI_RUN_DIR:-/run/alioth-wifi-client}"
WPA_DIR="${ALIOTH_WPA_CTRL_DIR:-/run/wpa_supplicant}"

usage() {
  cat >&2 <<'EOF'
usage:
  alioth-wifi-connect <ssid> [password]
  ALIOTH_WIFI_SSID=<ssid> ALIOTH_WIFI_PSK=<password> alioth-wifi-connect
  or create /etc/alioth-wifi-default as repeated ssid/password line pairs
EOF
}

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

write_open_network() {
  ssid="$1"
  escaped_ssid="$(printf '%s' "$ssid" | sed 's/\\/\\\\/g; s/"/\\"/g')"
  cat <<EOF
ctrl_interface=$WPA_DIR
update_config=0
network={
    ssid="$escaped_ssid"
    key_mgmt=NONE
}
EOF
}

write_psk_network() {
  ssid="$1"
  psk="$2"
  {
    printf 'ctrl_interface=%s\n' "$WPA_DIR"
    printf 'update_config=0\n'
    wpa_passphrase "$ssid" <<EOF
$psk
EOF
  } | sed '/^[[:space:]]*#psk=/d'
}

try_connect() {
  ssid="$1"
  psk="${2:-}"
  iface="$3"

  if [ -z "$ssid" ]; then
    return 1
  fi

  mkdir -p "$RUN_DIR" "$WPA_DIR"
  chmod 0700 "$RUN_DIR"
  conf="$RUN_DIR/wpa-$iface.conf"
  log="$RUN_DIR/wpa_supplicant-$iface.log"

  if [ -n "$psk" ]; then
    if [ "${#psk}" -lt 8 ]; then
      echo "password for SSID '$ssid' is shorter than 8 chars" >&2
      return 1
    fi
    write_psk_network "$ssid" "$psk" > "$conf"
  else
    write_open_network "$ssid" > "$conf"
  fi
  chmod 0600 "$conf"

  echo "connecting iface=$iface ssid=$ssid"
  ip link set "$iface" up 2>/dev/null || true
  wpa_cli -i "$iface" terminate >/dev/null 2>&1 || true
  sleep 1
  rm -f "$WPA_DIR/$iface"

  wpa_supplicant -B -i "$iface" -c "$conf" -D nl80211 -O "$WPA_DIR" -f "$log"

  connected=0
  for i in $(seq 1 45); do
    state="$(wpa_cli -i "$iface" status 2>/dev/null | sed -n 's/^wpa_state=//p' | head -n 1)"
    [ -n "$state" ] || state="UNKNOWN"
    echo "wpa_state=$state poll=$i"
    if [ "$state" = "COMPLETED" ]; then
      connected=1
      break
    fi
    sleep 1
  done

  if [ "$connected" -ne 1 ]; then
    echo "association failed for SSID '$ssid'" >&2
    tail -80 "$log" >&2 || true
    return 1
  fi

  dhcpcd -4 -q -w -t 30 "$iface"
  dhcp_rc=$?
  echo "dhcpcd_rc=$dhcp_rc"
  wpa_cli -i "$iface" status 2>/dev/null || true
  ip -brief addr show "$iface" 2>/dev/null || true
  ip route show default 2>/dev/null || true
  return "$dhcp_rc"
}

systemctl start alioth-wifi-bringup.service >/dev/null 2>&1 || true

iface="${ALIOTH_WIFI_IFACE:-$(find_managed_iface || true)}"
if [ -z "$iface" ]; then
  echo "no managed Wi-Fi interface found" >&2
  exit 1
fi

if [ "$#" -ge 1 ]; then
  try_connect "$1" "${2:-}" "$iface"
  exit $?
fi

if [ -n "${ALIOTH_WIFI_SSID:-}" ]; then
  try_connect "$ALIOTH_WIFI_SSID" "${ALIOTH_WIFI_PSK:-}" "$iface"
  exit $?
fi

if [ -s "$CONFIG_PATH" ]; then
  rc=1
  while IFS= read -r ssid; do
    IFS= read -r psk || psk=""
    [ -n "$ssid" ] || continue
    if try_connect "$ssid" "$psk" "$iface"; then
      rc=0
      break
    fi
  done < "$CONFIG_PATH"
  exit "$rc"
fi

usage
exit 2
