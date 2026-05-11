#!/bin/sh
set -eu

TORCH_NODE=${LELE_TORCH_NODE:-/sys/class/leds/led:torch_0}
SWITCH_NODE=${LELE_TORCH_SWITCH_NODE:-/sys/class/leds/led:switch_0}
SAFE_MAX=${LELE_TORCH_SAFE_MAX:-120}
DEFAULT_LEVEL=${LELE_TORCH_DEFAULT_LEVEL:-25}

log() {
  printf '[lele-torch] %s\n' "$*"
}

need_node() {
  if [ ! -w "$TORCH_NODE/brightness" ] || [ ! -w "$SWITCH_NODE/brightness" ]; then
    log "missing writable torch sysfs nodes"
    log "torch=$TORCH_NODE switch=$SWITCH_NODE"
    exit 1
  fi
}

clamp_level() {
  level="${1:-$DEFAULT_LEVEL}"
  case "$level" in
    *[!0-9]*|'') level="$DEFAULT_LEVEL" ;;
  esac

  max_brightness=$(cat "$TORCH_NODE/max_brightness" 2>/dev/null || printf '%s' "$SAFE_MAX")
  [ "$level" -gt 0 ] || level=1
  [ "$level" -le "$max_brightness" ] || level="$max_brightness"
  [ "$level" -le "$SAFE_MAX" ] || level="$SAFE_MAX"
  printf '%s' "$level"
}

off_all() {
  echo 0 > /sys/class/leds/flashlight/brightness 2>/dev/null || true
  echo 0 > /sys/class/leds/led:switch_0/brightness 2>/dev/null || true
  echo 0 > /sys/class/leds/led:switch_1/brightness 2>/dev/null || true
  echo 0 > /sys/class/leds/led:torch_0/brightness 2>/dev/null || true
  echo 0 > /sys/class/leds/led:torch_1/brightness 2>/dev/null || true
}

torch_on() {
  need_node
  level=$(clamp_level "${1:-}")
  off_all
  echo "$level" > "$TORCH_NODE/brightness"
  echo 1 > "$SWITCH_NODE/brightness"
  log "on level=$level torch=$TORCH_NODE switch=$SWITCH_NODE"
}

torch_off() {
  off_all
  log "off"
}

torch_status() {
  printf 'torch_node=%s\n' "$TORCH_NODE"
  printf 'switch_node=%s\n' "$SWITCH_NODE"
  for d in /sys/class/leds/flashlight "$TORCH_NODE" "$SWITCH_NODE"; do
    [ -d "$d" ] || continue
    printf '%s brightness=' "$d"
    cat "$d/brightness" 2>/dev/null || true
    printf '%s max_brightness=' "$d"
    cat "$d/max_brightness" 2>/dev/null || true
  done
  dmesg | grep -E 'flashv2|qpnp_flash' | tail -12 || true
}

usage() {
  cat <<'USAGE'
usage: lele-torch COMMAND [args]

commands:
  on [level]          turn torch on, default level 25, capped at 120
  off                 turn torch off
  timer [sec] [level] turn torch on briefly, then always turn it off
  blink [count] [level]
  status              print torch sysfs state and recent driver messages
  test                same as: timer 2 25
USAGE
}

cmd="${1:-status}"
shift || true

case "$cmd" in
  on)
    torch_on "${1:-}"
    ;;
  off)
    torch_off
    ;;
  timer)
    seconds="${1:-2}"
    level="${2:-$DEFAULT_LEVEL}"
    case "$seconds" in
      *[!0-9]*|'') seconds=2 ;;
    esac
    [ "$seconds" -le 30 ] || seconds=30
    trap torch_off EXIT INT TERM
    torch_on "$level"
    sleep "$seconds"
    torch_off
    trap - EXIT INT TERM
    ;;
  blink)
    count="${1:-3}"
    level="${2:-$DEFAULT_LEVEL}"
    case "$count" in
      *[!0-9]*|'') count=3 ;;
    esac
    [ "$count" -le 20 ] || count=20
    trap torch_off EXIT INT TERM
    i=0
    while [ "$i" -lt "$count" ]; do
      torch_on "$level"
      sleep 0.35
      torch_off
      sleep 0.25
      i=$((i + 1))
    done
    trap - EXIT INT TERM
    ;;
  status)
    torch_status
    ;;
  test)
    trap torch_off EXIT INT TERM
    torch_on "$DEFAULT_LEVEL"
    sleep 2
    torch_off
    trap - EXIT INT TERM
    ;;
  -h|--help|help)
    usage
    ;;
  *)
    usage >&2
    exit 2
    ;;
esac
