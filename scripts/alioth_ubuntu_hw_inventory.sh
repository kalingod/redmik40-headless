#!/usr/bin/env bash
set -euo pipefail

DEVICE_IP="${DEVICE_IP:-172.16.42.2}"
SSH_KEY="${SSH_KEY:-/vmdata/android/redmik40/keys/alioth_usb_ed25519}"
KNOWN_HOSTS="${KNOWN_HOSTS:-/tmp/alioth_ubuntu2404_known_hosts}"
OUT_DIR="${OUT_DIR:-/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-hw-abi-inventory}"
REPORT="${REPORT:-$OUT_DIR/report.txt}"

mkdir -p "$OUT_DIR"

ssh_cmd=(
  ssh
  -i "$SSH_KEY"
  -o "UserKnownHostsFile=$KNOWN_HOSTS"
  -o StrictHostKeyChecking=no
  -o ConnectTimeout=5
  "root@$DEVICE_IP"
)

printf '[inventory] writing %s\n' "$REPORT" >&2

"${ssh_cmd[@]}" 'bash -s' >"$REPORT" <<'REMOTE'
set +e

section() {
  printf '\n===== %s =====\n' "$1"
}

run() {
  section "$1"
  shift
  "$@" 2>&1 || true
}

shrun() {
  section "$1"
  shift
  bash -lc "$*" 2>&1 || true
}

read_one() {
  local path="$1"
  if [ -r "$path" ]; then
    printf '%s=' "$path"
    tr '\0' ' ' <"$path" 2>/dev/null || cat "$path" 2>/dev/null
    printf '\n'
  fi
}

read_value() {
  tr -d '\000' <"$1" 2>/dev/null
}

section "timestamp"
date -Is
cat /proc/sys/kernel/random/boot_id 2>/dev/null || true
cut -d' ' -f1 /proc/uptime 2>/dev/null || true

section "distro kernel pid1"
cat /etc/os-release 2>/dev/null || true
printf 'lele-rootfs-id='
cat /etc/lele-rootfs-id 2>/dev/null || true
printf 'uname='
uname -a 2>/dev/null || true
printf 'pid1='
tr '\0' ' ' </proc/1/cmdline 2>/dev/null || true
printf '\n'
systemctl --version 2>/dev/null | sed -n '1,4p'
systemctl is-system-running 2>/dev/null || true
systemctl --failed --no-pager 2>/dev/null || true

section "cmdline slot"
cat /proc/cmdline 2>/dev/null || true
grep -o 'androidboot.slot_suffix=[^ ]*' /proc/cmdline 2>/dev/null || true

section "mounts rootfs block"
findmnt -R / 2>/dev/null || true
mount | sed -n '1,140p'
df -hT 2>/dev/null || true
lsblk -o NAME,MAJ:MIN,SIZE,TYPE,FSTYPE,LABEL,MOUNTPOINTS 2>/dev/null || true
ls -l /dev/block/by-name 2>/dev/null | sed -n '1,160p'

section "drm kms"
ls -l /dev/dri 2>/dev/null || true
for d in /sys/class/drm/*; do
  [ -e "$d" ] || continue
  echo "-- $d"
  for f in dev status enabled dpms modes mode name; do
    [ -r "$d/$f" ] && printf '%s=%s\n' "$f" "$(read_value "$d/$f")"
  done
done

section "framebuffer graphics"
ls -l /dev/fb* /dev/graphics 2>/dev/null || true
for d in /sys/class/graphics/*; do
  [ -e "$d" ] || continue
  echo "-- $d"
  for f in name modes mode virtual_size stride blank state; do
    [ -r "$d/$f" ] && printf '%s=%s\n' "$f" "$(read_value "$d/$f")"
  done
done

section "input evdev"
ls -l /dev/input 2>/dev/null || true
cat /proc/bus/input/devices 2>/dev/null || true
for e in /sys/class/input/event*; do
  [ -e "$e" ] || continue
  echo "-- $e"
  read_one "$e/dev"
  read_one "$e/device/name"
  read_one "$e/device/phys"
  read_one "$e/device/uevent"
done

section "power supply"
ls -l /sys/class/power_supply 2>/dev/null || true
for p in /sys/class/power_supply/*; do
  [ -e "$p" ] || continue
  echo "-- $p"
  for f in type status online present capacity health charge_type usb_type voltage_now current_now power_now temp technology manufacturer model_name; do
    [ -r "$p/$f" ] && printf '%s=%s\n' "$f" "$(read_value "$p/$f")"
  done
done

section "thermal cpu"
cat /sys/devices/system/cpu/online 2>/dev/null || true
for c in /sys/devices/system/cpu/cpu[0-9]*; do
  [ -e "$c" ] || continue
  name="${c##*/}"
  freq=""
  [ -r "$c/cpufreq/scaling_cur_freq" ] && freq="$(read_value "$c/cpufreq/scaling_cur_freq")"
  gov=""
  [ -r "$c/cpufreq/scaling_governor" ] && gov="$(read_value "$c/cpufreq/scaling_governor")"
  [ -n "$freq$gov" ] && printf '%s freq=%s governor=%s\n' "$name" "$freq" "$gov"
done
for t in /sys/class/thermal/thermal_zone*; do
  [ -e "$t" ] || continue
  type="$(read_value "$t/type")"
  temp="$(read_value "$t/temp")"
  printf '%s type=%s temp=%s\n' "${t##*/}" "$type" "$temp"
done | sed -n '1,120p'

section "usb host gadget"
lsusb 2>/dev/null || true
ls -l /sys/bus/usb/devices 2>/dev/null || true
for d in /sys/bus/usb/devices/*; do
  [ -e "$d" ] || continue
  echo "-- $d"
  for f in busnum devnum idVendor idProduct manufacturer product serial version speed bDeviceClass bDeviceSubClass authorized; do
    [ -r "$d/$f" ] && printf '%s=%s\n' "$f" "$(read_value "$d/$f")"
  done
done
find /config/usb_gadget /sys/kernel/config/usb_gadget -maxdepth 4 -type f 2>/dev/null | sort | while read -r f; do
  case "$f" in
    *UDC|*idVendor|*idProduct|*functions/*|*configs/*)
      printf '%s=%s\n' "$f" "$(read_value "$f")"
      ;;
  esac
done

section "network"
ip -br link 2>/dev/null || true
ip -br addr 2>/dev/null || true
ip route 2>/dev/null || true
resolvectl status 2>/dev/null | sed -n '1,160p' || cat /etc/resolv.conf 2>/dev/null || true
iw dev 2>/dev/null || true
rfkill list 2>/dev/null || true
for n in /sys/class/net/*; do
  [ -e "$n" ] || continue
  echo "-- $n"
  for f in operstate address mtu type carrier speed; do
    [ -r "$n/$f" ] && printf '%s=%s\n' "$f" "$(read_value "$n/$f")"
  done
done

section "audio video media bluetooth sensors"
ls -l /dev/snd /dev/video* /dev/media* /dev/rfkill /dev/hci* /dev/ttyHS* /dev/ttyMSM* /dev/qrtr* /dev/wwan* /dev/iio* /dev/input/js* 2>/dev/null || true
find /sys/class/sound /sys/class/video4linux /sys/class/media /sys/class/bluetooth /sys/bus/iio/devices -maxdepth 3 2>/dev/null | sort | sed -n '1,220p'

section "firmware module clues"
ls -d /lib/firmware /vendor/firmware* /mnt/vendor/firmware* /firmware /bt_firmware /dsp 2>/dev/null || true
find /lib/firmware /vendor /mnt/vendor /firmware /bt_firmware /dsp -maxdepth 2 -type f 2>/dev/null | sed -n '1,220p'
lsmod 2>/dev/null || true

section "services sockets"
systemctl --type=service --state=running --no-pager 2>/dev/null || true
ss -lntup 2>/dev/null || true
journalctl --disk-usage 2>/dev/null || true

section "dmesg focused"
dmesg 2>/dev/null | grep -Ei 'drm|dsi|panel|input|touch|gpio-keys|qpnp|battery|charger|power_supply|usb|dwc|ncm|rndis|wifi|wlan|cnss|bluetooth|bt_|audio|alsa|snd|camera|cam_|v4l|video|media|qmi|qrtr|modem|ipa|firmware|failed|error' | tail -n 260

section "journal warnings"
journalctl -b -p warning..alert --no-pager 2>/dev/null | tail -n 160
REMOTE

printf '[inventory] done: %s\n' "$REPORT" >&2
printf '%s\n' "$REPORT"
