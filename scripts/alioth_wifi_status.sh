#!/bin/sh
set -eu

printf '=== alioth wifi state ===\n'
cat /run/alioth-wifi-state 2>/dev/null || true

printf '\n=== services ===\n'
systemctl --no-pager --plain status \
  alioth-wifi-prepare.service \
  alioth-qrtr-ns.service \
  alioth-cnss-daemon.service \
  alioth-wifi-bringup.service 2>/dev/null | sed -n '1,220p' || true

printf '\n=== processes ===\n'
ps -ef | grep -E 'qrtr-ns|cnss-daemon|wpa_supplicant' | grep -v grep || true

printf '\n=== firmware paths ===\n'
ls -l /lib/firmware/qca6390 /vendor/firmware_mnt /firmware/image \
  /lib/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini \
  /lib/firmware/wlan/qca_cld/wlan_mac.bin 2>/dev/null || true
mount | grep -E 'alioth-firmware|firmware_mnt|modem' || true

printf '\n=== links ===\n'
ip -brief addr show 2>/dev/null | grep -E 'usb0|wlan|wlp|p2p|wifi-aware' || true

printf '\n=== iw ===\n'
if command -v iw >/dev/null 2>&1; then
  iw dev 2>&1 || true
  iw reg get 2>&1 | sed -n '1,80p' || true
  iface="$(iw dev 2>/dev/null | awk '$1 == "Interface" { iface = $2; next } $1 == "type" && $2 == "managed" { print iface; exit }')"
  if [ -n "${iface:-}" ]; then
    iw dev "$iface" link 2>&1 || true
    if [ "${ALIOTH_WIFI_SCAN:-0}" = "1" ]; then
      timeout 35 iw dev "$iface" scan 2>&1 | sed -n '1,160p' || true
    fi
  fi
else
  echo "iw missing"
fi

printf '\n=== recent log ===\n'
tail -180 /run/alioth-wifi-bringup.log 2>/dev/null || true
