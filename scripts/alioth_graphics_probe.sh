#!/usr/bin/env bash
set -u

EXECUTE=0
TEST_NAME="modetest"
DURATION_SEC=12
LOG_FILE="/tmp/alioth-graphics-probe.log"
UI_SERVICE="lele-status-ui.service"
RESTORE_NEEDED=0

usage() {
  cat <<'USAGE'
Usage:
  alioth_graphics_probe.sh [--dry-run] [--execute] [--test NAME] [--duration-sec N] [--log FILE]

Tests:
  modetest       Run modetest -c -p
  eglinfo        Run eglinfo -B -p surfaceless
  kmscube        Run kmscube on /dev/dri/card0 with a bounded frame count
  weston-pixman  Run Weston DRM backend with pixman renderer
  weston-pixman-openvt
                 Run Weston pixman through openvt on tty1
  weston-pixman-openvt-switch
                 Run Weston pixman through openvt on tty1 and switch to it
  weston-pixman-builtin-novt
                 Run Weston pixman with libseat builtin backend and SEATD_VTBOUND=0
  weston-gl      Run Weston DRM backend with GL renderer

Default is dry-run. --execute is required before stopping lele-status-ui.
For live tests, prefer launching on the phone through systemd-run.
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
    --test)
      shift
      TEST_NAME="${1:-modetest}"
      ;;
    --duration-sec)
      shift
      DURATION_SEC="${1:-12}"
      ;;
    --log)
      shift
      LOG_FILE="${1:-/tmp/alioth-graphics-probe.log}"
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

if [ "$DURATION_SEC" -lt 3 ]; then
  DURATION_SEC=3
fi
if [ "$DURATION_SEC" -gt 90 ]; then
  DURATION_SEC=90
fi

case "$TEST_NAME" in
  modetest|eglinfo|kmscube|weston-pixman|weston-pixman-openvt|weston-pixman-openvt-switch|weston-pixman-builtin-novt|weston-gl)
    ;;
  *)
    echo "unknown --test: $TEST_NAME" >&2
    usage >&2
    exit 2
    ;;
esac

log() {
  printf '%s %s\n' "$(date -Ins 2>/dev/null || date)" "$*" | tee -a "$LOG_FILE"
}

section() {
  printf '\n===== %s =====\n' "$*" | tee -a "$LOG_FILE"
}

capture_state() {
  name="$1"
  section "$name"
  log "mode=$([ "$EXECUTE" -eq 1 ] && echo execute || echo dry-run)"
  log "test=$TEST_NAME duration_sec=$DURATION_SEC"
  uname -a 2>&1 | tee -a "$LOG_FILE"
  systemctl is-system-running 2>&1 | tee -a "$LOG_FILE"
  systemctl --failed --no-pager 2>&1 | tee -a "$LOG_FILE"
  systemctl is-active ssh lele-usb-net lele-usb-dhcpd "$UI_SERVICE" 2>&1 | tee -a "$LOG_FILE"
  ps -eo pid,ppid,stat,user,group,args | grep -E 'alioth-status-ui|lele-status-ui|PID' | grep -v grep | tee -a "$LOG_FILE"
  ls -la /dev/dri /dev/input 2>&1 | tee -a "$LOG_FILE"
  for f in /sys/class/drm/card0-*/status /sys/class/drm/card0-*/enabled /sys/class/drm/card0-*/modes /sys/class/drm/card0-*/dpms; do
    [ -e "$f" ] || continue
    printf -- '--- %s ---\n' "$f" | tee -a "$LOG_FILE"
    cat "$f" 2>&1 | tee -a "$LOG_FILE"
  done
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

restore_ui() {
  [ "$RESTORE_NEEDED" -eq 1 ] || return 0
  section restore_status_ui
  run_cmd systemctl start "$UI_SERVICE" || true
  if [ "$EXECUTE" -eq 1 ]; then
    sleep 2
  fi
  capture_state restored_state
}

trap restore_ui EXIT INT TERM

build_test_command() {
  case "$TEST_NAME" in
    modetest)
      TEST_CMD=(timeout --kill-after=3 "$DURATION_SEC" modetest -c -p)
      ;;
    eglinfo)
      TEST_CMD=(timeout --kill-after=3 "$DURATION_SEC" eglinfo -B -p surfaceless)
      ;;
    kmscube)
      frame_count=$((DURATION_SEC * 30))
      if [ "$frame_count" -lt 60 ]; then
        frame_count=60
      fi
      TEST_CMD=(timeout --kill-after=3 "$DURATION_SEC" kmscube -D /dev/dri/card0 -c "$frame_count")
      ;;
    weston-pixman)
      WESTON_LOG="${LOG_FILE}.weston.log"
      TEST_CMD=(timeout --kill-after=3 "$DURATION_SEC" weston --backend=drm --renderer=pixman --drm-device=/dev/dri/card0 --current-mode --continue-without-input --idle-time=0 --no-config --socket=wayland-alioth-probe --log="$WESTON_LOG")
      ;;
    weston-pixman-openvt)
      WESTON_LOG="${LOG_FILE}.weston.log"
      TEST_CMD=(openvt -c 1 -f -w -- timeout --kill-after=3 "$DURATION_SEC" weston --backend=drm --renderer=pixman --drm-device=/dev/dri/card0 --current-mode --continue-without-input --idle-time=0 --no-config --socket=wayland-alioth-probe --log="$WESTON_LOG")
      ;;
    weston-pixman-openvt-switch)
      WESTON_LOG="${LOG_FILE}.weston.log"
      TEST_CMD=(openvt -c 1 -f -s -w -- timeout --kill-after=3 "$DURATION_SEC" weston --backend=drm --renderer=pixman --drm-device=/dev/dri/card0 --current-mode --continue-without-input --idle-time=0 --no-config --socket=wayland-alioth-probe --log="$WESTON_LOG")
      ;;
    weston-pixman-builtin-novt)
      WESTON_LOG="${LOG_FILE}.weston.log"
      TEST_CMD=(env LIBSEAT_BACKEND=builtin SEATD_VTBOUND=0 timeout --kill-after=3 "$DURATION_SEC" weston --backend=drm --renderer=pixman --drm-device=/dev/dri/card0 --current-mode --continue-without-input --idle-time=0 --no-config --socket=wayland-alioth-probe --log="$WESTON_LOG")
      ;;
    weston-gl)
      WESTON_LOG="${LOG_FILE}.weston.log"
      TEST_CMD=(timeout --kill-after=3 "$DURATION_SEC" weston --backend=drm --renderer=gl --drm-device=/dev/dri/card0 --current-mode --continue-without-input --idle-time=0 --no-config --socket=wayland-alioth-probe --log="$WESTON_LOG")
      ;;
  esac
}

main() {
  : >"$LOG_FILE"
  section start
  log "script=$0"
  log "execute=$EXECUTE"
  log "test=$TEST_NAME"
  log "log_file=$LOG_FILE"

  if [ ! -e /dev/dri/card0 ]; then
    log "missing /dev/dri/card0"
    exit 1
  fi
  if ! systemctl status "$UI_SERVICE" >/dev/null 2>&1; then
    log "missing service $UI_SERVICE"
    exit 1
  fi

  build_test_command
  log "test_command=${TEST_CMD[*]}"
  capture_state baseline

  section stop_status_ui
  RESTORE_NEEDED=1
  run_cmd systemctl stop "$UI_SERVICE" || true
  if [ "$EXECUTE" -eq 1 ]; then
    sleep 1
  fi
  capture_state after_ui_stop

  section run_graphics_test
  if [ "$TEST_NAME" = "weston-pixman" ] || [ "$TEST_NAME" = "weston-pixman-openvt" ] || [ "$TEST_NAME" = "weston-pixman-openvt-switch" ] || [ "$TEST_NAME" = "weston-pixman-builtin-novt" ] || [ "$TEST_NAME" = "weston-gl" ]; then
    export XDG_RUNTIME_DIR=/run/alioth-graphics-probe
    if [ "$EXECUTE" -eq 1 ]; then
      mkdir -p "$XDG_RUNTIME_DIR"
      chmod 0700 "$XDG_RUNTIME_DIR"
    fi
    log "XDG_RUNTIME_DIR=$XDG_RUNTIME_DIR"
  fi
  run_cmd "${TEST_CMD[@]}"
  test_rc=$?
  log "graphics_test_rc=$test_rc"

  restore_ui
  RESTORE_NEEDED=0
  section done
  exit "$test_rc"
}

main "$@"
