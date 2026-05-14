#!/bin/sh
set -eu

CONFIG_PATH="${ALIOTH_WIFI_CONFIG:-/etc/alioth-wifi-default}"
RUN_DIR="${ALIOTH_WIFI_RUN_DIR:-/run/alioth-wifi-client}"
WPA_DIR="${ALIOTH_WPA_CTRL_DIR:-/run/wpa_supplicant}"
STATE="${ALIOTH_WIFI_STATE:-/run/alioth-wifi-state}"
PREFER_ROUTE="${ALIOTH_WIFI_PREFER_ROUTE:-yes}"
DNS_SERVERS="${ALIOTH_WIFI_DNS:-223.5.5.5 1.1.1.1}"

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

iface_ipv4_addr() {
  iface="$1"
  ip -o -4 addr show dev "$iface" scope global 2>/dev/null |
    awk '{ split($4, a, "/"); print a[1]; exit }'
}

stop_legacy_dhcpcd() {
  iface="$1"

  if command -v dhcpcd >/dev/null 2>&1; then
    dhcpcd -4 -k "$iface" >/dev/null 2>&1 || true
  fi
}

networkd_manages_iface() {
  iface="$1"

  command -v networkctl >/dev/null 2>&1 || return 1
  systemctl is-active --quiet systemd-networkd 2>/dev/null || return 1
  networkctl status "$iface" --no-pager 2>/dev/null | grep -q 'Network File:'
}

wait_networkd_ipv4() {
  iface="$1"
  ipaddr=""

  if ! networkd_manages_iface "$iface"; then
    return 1
  fi

  networkctl reconfigure "$iface" >/dev/null 2>&1 || true
  networkctl renew "$iface" >/dev/null 2>&1 || true
  for i in $(seq 1 35); do
    ipaddr="$(iface_ipv4_addr "$iface")"
    if [ -n "$ipaddr" ] && ip route show default dev "$iface" 2>/dev/null | grep -q '^default '; then
      echo "networkd_ipv4=$ipaddr poll=$i"
      return 0
    fi
    sleep 1
  done

  echo "networkd ipv4 timeout for $iface" >&2
  return 1
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
  stop_legacy_dhcpcd "$iface"
  ip -4 addr flush dev "$iface" scope global 2>/dev/null || true
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

  if ! wait_networkd_ipv4 "$iface"; then
    return 1
  fi
  wpa_cli -i "$iface" status 2>/dev/null || true
  ip -brief addr show "$iface" 2>/dev/null || true
  ip route show default 2>/dev/null || true
  return 0
}

wifi_failure_reason() {
  iface="$1"
  log="$RUN_DIR/wpa_supplicant-$iface.log"

  if [ -r "$log" ] && tail -120 "$log" | grep -q 'WRONG_KEY\|pre-shared key may be incorrect'; then
    printf 'wrong-key\n'
    return 0
  fi
  if command -v wpa_cli >/dev/null 2>&1; then
    state="$(wpa_cli -i "$iface" status 2>/dev/null | sed -n 's/^wpa_state=//p' | head -n 1)"
    [ -n "$state" ] && {
      printf 'wpa-%s\n' "$state"
      return 0
    }
  fi
  printf 'connect-failed\n'
}

prefer_wifi_route() {
  iface="$1"
  [ "$PREFER_ROUTE" = "yes" ] || return 0

  gateway="$(ip route show default dev "$iface" 2>/dev/null | awk '/^default / { for (i = 1; i <= NF; i++) if ($i == "via") { print $(i + 1); exit } }')"
  [ -n "$gateway" ] || return 0

  ip route replace default via "$gateway" dev "$iface" metric 50 2>/dev/null || true

  while ip route show default 2>/dev/null | grep -q '^default via 172\.16\.42\.1 dev usb0'; do
    ip route del default via 172.16.42.1 dev usb0 2>/dev/null ||
      ip route del default via 172.16.42.1 dev usb0 proto static 2>/dev/null ||
      break
  done
  if ip link show usb0 >/dev/null 2>&1; then
    ip route replace default via 172.16.42.1 dev usb0 metric 5000 2>/dev/null || true
  fi
}

prune_extra_ipv4_addrs() {
  iface="$1"
  preferred="$(ip route show default dev "$iface" 2>/dev/null | awk '/^default / && / proto dhcp / { for (i = 1; i <= NF; i++) if ($i == "src") { print $(i + 1); exit } }')"
  if [ -z "$preferred" ] && command -v wpa_cli >/dev/null 2>&1; then
    preferred="$(wpa_cli -i "$iface" status 2>/dev/null | sed -n 's/^ip_address=//p' | head -n 1)"
  fi
  [ -n "$preferred" ] || return 0

  ip -o -4 addr show dev "$iface" scope global 2>/dev/null | awk '{ print $4 }' |
    while IFS= read -r cidr; do
      addr="${cidr%/*}"
      [ "$addr" = "$preferred" ] && continue
      ip addr del "$cidr" dev "$iface" 2>/dev/null || true
    done
}

prefer_wifi_dns() {
  iface="$1"
  tmp="$RUN_DIR/resolv.conf"
  : > "$tmp"

  for dns in $DNS_SERVERS; do
    printf 'nameserver %s\n' "$dns" >> "$tmp"
  done
  awk '/^nameserver / { print }' /etc/resolv.conf 2>/dev/null |
    while read -r _ dns; do
      case " $DNS_SERVERS " in
        *" $dns "*) continue ;;
      esac
      printf 'nameserver %s\n' "$dns"
    done >> "$tmp"

  if [ -s "$tmp" ]; then
    cp -n /etc/resolv.conf /etc/resolv.conf.before-alioth-wifi 2>/dev/null || true
    cp "$tmp" /etc/resolv.conf 2>/dev/null || true
  fi

  if command -v resolvectl >/dev/null 2>&1; then
    resolvectl dns "$iface" $DNS_SERVERS >/dev/null 2>&1 || true
    resolvectl default-route "$iface" yes >/dev/null 2>&1 || true
    resolvectl domain "$iface" '~.' >/dev/null 2>&1 || true
  fi
}

write_state_mode() {
  iface="$1"
  ssid="$2"
  mode="$3"
  reason="${4:-}"
  {
    printf 'mode=%s\n' "$mode"
    printf 'iface=%s\n' "$iface"
    printf 'ssid=%s\n' "$ssid"
    [ -n "$reason" ] && printf 'reason=%s\n' "$reason"
    printf 'boot_id=%s\n' "$(cat /proc/sys/kernel/random/boot_id 2>/dev/null || true)"
    printf 'updated_utc=%s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ')"
    ip -brief addr show "$iface" 2>/dev/null | sed 's/^/ip=/'
    ip route show default 2>/dev/null | sed 's/^/default_route=/'
    if command -v wpa_cli >/dev/null 2>&1; then
      wpa_cli -i "$iface" status 2>/dev/null | sed 's/^/wpa=/'
    fi
  } > "$STATE"
}

write_state() {
  write_state_mode "$1" "$2" "client-connected" ""
}

connect_and_finalize() {
  ssid="$1"
  psk="${2:-}"
  iface="$3"

  write_state_mode "$iface" "$ssid" "client-connecting" "starting"
  if ! try_connect "$ssid" "$psk" "$iface"; then
    write_state_mode "$iface" "$ssid" "client-failed" "$(wifi_failure_reason "$iface")"
    return 1
  fi
  prune_extra_ipv4_addrs "$iface"
  prefer_wifi_route "$iface"
  prefer_wifi_dns "$iface"
  write_state "$iface" "$ssid"
}

systemctl start alioth-wifi-bringup.service >/dev/null 2>&1 || true

iface="${ALIOTH_WIFI_IFACE:-$(find_managed_iface || true)}"
if [ -z "$iface" ]; then
  echo "no managed Wi-Fi interface found" >&2
  exit 1
fi

if [ "$#" -ge 1 ]; then
  connect_and_finalize "$1" "${2:-}" "$iface"
  exit $?
fi

if [ -n "${ALIOTH_WIFI_SSID:-}" ]; then
  connect_and_finalize "$ALIOTH_WIFI_SSID" "${ALIOTH_WIFI_PSK:-}" "$iface"
  exit $?
fi

if [ -s "$CONFIG_PATH" ]; then
  rc=1
  while IFS= read -r ssid; do
    IFS= read -r psk || psk=""
    [ -n "$ssid" ] || continue
    if connect_and_finalize "$ssid" "$psk" "$iface"; then
      rc=0
      break
    fi
  done < "$CONFIG_PATH"
  exit "$rc"
fi

usage
exit 2
