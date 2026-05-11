#!/bin/sh
set -eu

AP_DIR=/run/lele-ap

ip -brief addr show wlan0 2>/dev/null || true
iw dev wlan0 info 2>/dev/null || true
printf '\nprocesses:\n'
ps -ef | grep -E '[h]ostapd|[d]nsmasq|[l]ele-wifi-portal|[w]pa_supplicant|[d]hcpcd' || true
printf '\nportal:\n'
curl -fsS --max-time 3 http://10.42.0.1/ | sed -n '1,8p' || true
printf '\nhostapd log:\n'
tail -40 "$AP_DIR/hostapd.log" 2>/dev/null || true
printf '\ndnsmasq log:\n'
tail -40 "$AP_DIR/dnsmasq.log" 2>/dev/null || true
printf '\nportal log:\n'
tail -40 "$AP_DIR/portal.log" 2>/dev/null || true
