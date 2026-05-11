#!/bin/sh
set -eu

AP_DIR=/run/lele-ap

if [ -f "$AP_DIR/portal.pid" ]; then
  kill "$(cat "$AP_DIR/portal.pid")" 2>/dev/null || true
fi
pkill -f /usr/local/sbin/lele-wifi-portal 2>/dev/null || true
pkill hostapd 2>/dev/null || true
pkill dnsmasq 2>/dev/null || true

ip addr flush dev wlan0 2>/dev/null || true
ip link set wlan0 down 2>/dev/null || true
sleep 1
ip link set wlan0 up 2>/dev/null || true

if [ -f "$AP_DIR/nginx.was_running" ]; then
  nginx -t >/dev/null 2>&1 && nginx >/run/nginx-restart.log 2>&1 || true
fi

rm -f "$AP_DIR/portal.pid" "$AP_DIR/hostapd.pid" "$AP_DIR/dnsmasq.pid"
cat > "$AP_DIR/status" <<'EOF'
mode=client
EOF
echo "ap stopped"
