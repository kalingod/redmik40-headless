#!/bin/sh
set -eu

BRIGHTNESS_DIR="${ALIOTH_BRIGHTNESS_DIR:-/sys/class/backlight/panel0-backlight}"
BRIGHTNESS_FILE="$BRIGHTNESS_DIR/brightness"
MAX_FILE="$BRIGHTNESS_DIR/max_brightness"
ACTUAL_FILE="$BRIGHTNESS_DIR/actual_brightness"
BL_POWER_FILE="$BRIGHTNESS_DIR/bl_power"

usage() {
  cat >&2 <<'EOF'
usage:
  lele-brightness status
  lele-brightness get
  lele-brightness max
  lele-brightness set <1..max>
  lele-brightness percent <1..100>
  lele-brightness dim [seconds] [percent]
EOF
}

die() {
  echo "lele-brightness: $*" >&2
  exit 1
}

is_uint() {
  case "${1:-}" in
    ''|*[!0-9]*) return 1 ;;
    *) return 0 ;;
  esac
}

require_target() {
  [ -d "$BRIGHTNESS_DIR" ] || die "missing brightness dir: $BRIGHTNESS_DIR"
  [ -r "$BRIGHTNESS_FILE" ] || die "missing readable brightness file: $BRIGHTNESS_FILE"
  [ -w "$BRIGHTNESS_FILE" ] || die "brightness file is not writable: $BRIGHTNESS_FILE"
  [ -r "$MAX_FILE" ] || die "missing readable max_brightness file: $MAX_FILE"
}

current_value() {
  cat "$BRIGHTNESS_FILE"
}

max_value() {
  cat "$MAX_FILE"
}

actual_value() {
  if [ -r "$ACTUAL_FILE" ]; then
    cat "$ACTUAL_FILE"
  else
    echo "n/a"
  fi
}

bl_power_value() {
  if [ -r "$BL_POWER_FILE" ]; then
    cat "$BL_POWER_FILE"
  else
    echo "n/a"
  fi
}

set_value() {
  value="$1"
  max="$2"
  is_uint "$value" || die "value must be an integer"
  [ "$value" -ge 1 ] || die "refusing to write 0; use a value in 1..$max"
  [ "$value" -le "$max" ] || die "value $value exceeds max $max"
  echo "$value" > "$BRIGHTNESS_FILE"
}

percent_to_value() {
  percent="$1"
  max="$2"
  is_uint "$percent" || die "percent must be an integer"
  [ "$percent" -ge 1 ] || die "percent must be in 1..100"
  [ "$percent" -le 100 ] || die "percent must be in 1..100"
  value=$((max * percent / 100))
  [ "$value" -lt 1 ] && value=1
  [ "$value" -gt "$max" ] && value="$max"
  echo "$value"
}

status() {
  printf 'path=%s\n' "$BRIGHTNESS_DIR"
  printf 'brightness=%s\n' "$(current_value)"
  printf 'actual_brightness=%s\n' "$(actual_value)"
  printf 'max_brightness=%s\n' "$(max_value)"
  printf 'bl_power=%s\n' "$(bl_power_value)"
}

cmd="${1:-status}"
case "$cmd" in
  status)
    require_target
    status
    ;;
  get)
    require_target
    current_value
    ;;
  max)
    require_target
    max_value
    ;;
  set)
    require_target
    [ "$#" -eq 2 ] || { usage; exit 2; }
    max="$(max_value)"
    set_value "$2" "$max"
    current_value
    ;;
  percent)
    require_target
    [ "$#" -eq 2 ] || { usage; exit 2; }
    max="$(max_value)"
    value="$(percent_to_value "$2" "$max")"
    set_value "$value" "$max"
    current_value
    ;;
  dim)
    require_target
    seconds="${2:-2}"
    percent="${3:-33}"
    is_uint "$seconds" || die "seconds must be an integer"
    [ "$seconds" -ge 1 ] || die "seconds must be >= 1"
    [ "$seconds" -le 30 ] || die "seconds must be <= 30"
    max="$(max_value)"
    orig="$(current_value)"
    is_uint "$orig" || die "current brightness is not an integer: $orig"
    value="$(percent_to_value "$percent" "$max")"
    if [ "$value" -ge "$orig" ]; then
      value=$((orig / 3))
      [ "$value" -lt 1 ] && value=1
      [ "$value" -ge "$orig" ] && value=$((orig - 1))
      [ "$value" -lt 1 ] && value=1
    fi
    restore() {
      echo "$orig" > "$BRIGHTNESS_FILE" 2>/dev/null || true
    }
    trap restore EXIT INT TERM
    set_value "$value" "$max"
    printf 'dimmed=%s seconds=%s orig=%s\n' "$(current_value)" "$seconds" "$orig"
    sleep "$seconds"
    restore
    trap - EXIT INT TERM
    printf 'restored=%s\n' "$(current_value)"
    ;;
  -h|--help|help)
    usage
    ;;
  *)
    usage
    exit 2
    ;;
esac
