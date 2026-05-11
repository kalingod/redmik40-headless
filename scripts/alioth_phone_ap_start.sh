#!/bin/sh
set -eu

AP_DIR=/run/lele-ap
AP_SSID="${LELE_AP_SSID:-LELE-OS-SETUP}"
AP_PSK="${LELE_AP_PSK:-lele12345678}"
AP_ADDR="${LELE_AP_ADDR:-10.42.0.1}"
AP_CIDR="${LELE_AP_CIDR:-10.42.0.1/24}"
AP_RANGE="${LELE_AP_RANGE:-10.42.0.50,10.42.0.150,12h}"
PORTAL="${LELE_PORTAL_BIN:-/usr/local/sbin/lele-wifi-portal}"

mkdir -p "$AP_DIR"

if [ "${#AP_PSK}" -lt 8 ]; then
  echo "AP password must be at least 8 chars" >&2
  exit 2
fi

if pgrep -x nginx >/dev/null 2>&1; then
  touch "$AP_DIR/nginx.was_running"
  nginx -s quit 2>/dev/null || pkill nginx 2>/dev/null || true
else
  rm -f "$AP_DIR/nginx.was_running"
fi

wpa_cli -i wlan0 terminate >/dev/null 2>&1 || true
dhcpcd -k wlan0 >/dev/null 2>&1 || true
pids="$(pgrep -x dhcpcd 2>/dev/null || true)"
[ -z "$pids" ] || kill -9 $pids 2>/dev/null || true
pkill hostapd 2>/dev/null || true
pkill dnsmasq 2>/dev/null || true
if [ -f "$AP_DIR/portal.pid" ]; then
  kill "$(cat "$AP_DIR/portal.pid")" 2>/dev/null || true
fi
pkill -f "$PORTAL" 2>/dev/null || true
sleep 1

ip link set wlan0 down 2>/dev/null || true
ip addr flush dev wlan0 2>/dev/null || true
ip link set wlan0 up
ip addr add "$AP_CIDR" dev wlan0

cat > "$AP_DIR/hostapd.conf" <<EOF
interface=wlan0
driver=nl80211
ssid=$AP_SSID
country_code=CN
hw_mode=g
channel=6
ieee80211n=1
wmm_enabled=1
auth_algs=1
wpa=2
wpa_passphrase=$AP_PSK
wpa_key_mgmt=WPA-PSK
rsn_pairwise=CCMP
logger_syslog=-1
logger_syslog_level=2
EOF

hostapd -B -P "$AP_DIR/hostapd.pid" -f "$AP_DIR/hostapd.log" "$AP_DIR/hostapd.conf"

dnsmasq \
  --keep-in-foreground \
  --interface=wlan0 \
  --bind-dynamic \
  --except-interface=lo \
  --dhcp-authoritative \
  --dhcp-range="$AP_RANGE" \
  --dhcp-option=3,"$AP_ADDR" \
  --dhcp-option=6,"$AP_ADDR" \
  --address=/#/"$AP_ADDR" \
  --pid-file="$AP_DIR/dnsmasq.pid" \
  --log-facility="$AP_DIR/dnsmasq.log" \
  >/dev/null 2>&1 &

LELE_PORTAL_HOST="$AP_ADDR" LELE_PORTAL_PORT=80 nohup "$PORTAL" >"$AP_DIR/portal.out" 2>"$AP_DIR/portal.err" &
echo $! > "$AP_DIR/portal.pid"
cat > "$AP_DIR/status" <<EOF
mode=ap
ssid=$AP_SSID
password=$AP_PSK
addr=$AP_ADDR
cidr=$AP_CIDR
url=http://$AP_ADDR/
EOF
chmod 0600 "$AP_DIR/status"

sleep 1
printf 'ssid=%s\npassword=%s\nurl=http://%s/\n' "$AP_SSID" "$AP_PSK" "$AP_ADDR"
ip -brief addr show wlan0
tail -20 "$AP_DIR/hostapd.log" 2>/dev/null || true
