#!/usr/bin/env bash
set -u

EXECUTE=0
DURATION_SEC=45
LOG_FILE="/tmp/alioth-usb-host-probe.log"
TYPEC_PORT="/sys/class/typec/port0"
GADGET_DIR="/sys/kernel/config/usb_gadget/alioth"
UDC_NAME="a600000.dwc3"
RESTORE_NEEDED=0

usage() {
  cat <<'USAGE'
Usage:
  alioth_usb_host_probe.sh [--dry-run] [--execute] [--duration-sec N] [--log FILE]

Default is dry-run. --execute is required before any sysfs/service write.

Future live test should be launched on the phone through systemd so it survives
USB SSH disconnect, for example:

  systemd-run --unit=alioth-usb-host-probe --collect \
    /tmp/alioth_usb_host_probe.sh --execute --duration-sec 45
USAGE
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --dry-run)
      EXECUTE=0
      ;;
    --execute)
      EXECUTE=1
      ;;
    --duration-sec)
      shift
      DURATION_SEC="${1:-45}"
      ;;
    --log)
      shift
      LOG_FILE="${1:-/tmp/alioth-usb-host-probe.log}"
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
  shift
done

case "$DURATION_SEC" in
  ''|*[!0-9]*)
    echo "--duration-sec must be a positive integer" >&2
    exit 2
    ;;
esac

if [ "$DURATION_SEC" -lt 5 ]; then
  DURATION_SEC=5
fi
if [ "$DURATION_SEC" -gt 180 ]; then
  DURATION_SEC=180
fi

log() {
  printf '%s %s\n' "$(date -Ins 2>/dev/null || date)" "$*" | tee -a "$LOG_FILE"
}

section() {
  printf '\n===== %s =====\n' "$*" | tee -a "$LOG_FILE"
}

read_attr() {
  attr="$1"
  if [ -e "$attr" ]; then
    printf '%s=' "$attr" | tee -a "$LOG_FILE"
    cat "$attr" 2>&1 | tee -a "$LOG_FILE"
  else
    log "missing $attr"
  fi
}

current_choice() {
  attr="$1"
  [ -r "$attr" ] || return 1
  sed -n 's/.*\[\([^]]*\)\].*/\1/p' "$attr" 2>/dev/null | head -n 1
}

run_cmd() {
  log "+ $*"
  if [ "$EXECUTE" -eq 1 ]; then
    "$@" >>"$LOG_FILE" 2>&1
    rc=$?
    log "rc=$rc cmd=$*"
    return "$rc"
  fi
  log "dry-run skip command"
  return 0
}

write_attr() {
  attr="$1"
  value="$2"
  log "write $attr <- $value"
  if [ ! -e "$attr" ]; then
    log "skip missing attr $attr"
    return 0
  fi
  if [ "$EXECUTE" -eq 1 ]; then
    printf '%s' "$value" >"$attr" 2>>"$LOG_FILE"
    rc=$?
    log "rc=$rc write_attr=$attr"
    return "$rc"
  fi
  log "dry-run skip write"
  return 0
}

capture_state() {
  name="$1"
  section "$name"
  log "mode=$([ "$EXECUTE" -eq 1 ] && echo execute || echo dry-run)"
  log "duration_sec=$DURATION_SEC"
  uname -a 2>&1 | tee -a "$LOG_FILE"
  read_attr "$TYPEC_PORT/data_role"
  read_attr "$TYPEC_PORT/power_role"
  read_attr "$TYPEC_PORT/port_type"
  read_attr "$TYPEC_PORT/preferred_role"
  read_attr "$GADGET_DIR/UDC"
  read_attr "/sys/class/udc/$UDC_NAME/state"
  read_attr "/sys/class/udc/$UDC_NAME/function"
  ip addr show usb0 2>&1 | tee -a "$LOG_FILE"
  lsusb 2>&1 | tee -a "$LOG_FILE"
  find /sys/bus/usb/devices -maxdepth 1 -mindepth 1 -printf '%p -> %l\n' 2>&1 | tee -a "$LOG_FILE"
  cat /proc/bus/input/devices 2>&1 | tee -a "$LOG_FILE"
}

restore_device_mode() {
  [ "$RESTORE_NEEDED" -eq 1 ] || return 0
  section restore_device_mode
  write_attr "$TYPEC_PORT/data_role" "device" || true
  write_attr "$TYPEC_PORT/power_role" "sink" || true
  if [ "$EXECUTE" -eq 1 ]; then
    sleep 2
  fi
  write_attr "$GADGET_DIR/UDC" "$UDC_NAME" || true
  run_cmd systemctl restart lele-usb-net || true
  run_cmd systemctl restart lele-usb-dhcpd || true
  if [ "$EXECUTE" -eq 1 ]; then
    sleep 3
  fi
  capture_state restored_state
}

trap restore_device_mode EXIT INT TERM

main() {
  : >"$LOG_FILE"
  section start
  log "script=$0"
  log "execute=$EXECUTE"
  log "log_file=$LOG_FILE"
  log "data_role_current=$(current_choice "$TYPEC_PORT/data_role" || true)"
  log "power_role_current=$(current_choice "$TYPEC_PORT/power_role" || true)"

  if [ ! -d "$TYPEC_PORT" ]; then
    log "missing Type-C port path: $TYPEC_PORT"
    exit 1
  fi
  if [ ! -d "$GADGET_DIR" ]; then
    log "missing gadget path: $GADGET_DIR"
    exit 1
  fi

  capture_state baseline

  section enter_host_mode
  RESTORE_NEEDED=1
  run_cmd systemctl stop lele-usb-dhcpd || true
  run_cmd systemctl stop lele-usb-net || true
  write_attr "$GADGET_DIR/UDC" "" || true
  write_attr "$TYPEC_PORT/power_role" "source" || true
  write_attr "$TYPEC_PORT/data_role" "host" || true

  section host_observation_window
  log "sleeping ${DURATION_SEC}s before restore"
  if [ "$EXECUTE" -eq 1 ]; then
    sleep "$DURATION_SEC"
  else
    log "dry-run skip sleep"
  fi
  capture_state host_observation

  restore_device_mode
  RESTORE_NEEDED=0
  section done
}

main "$@"
