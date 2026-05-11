#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

OUT_DIR="${OUT_DIR:-/vmdata/android/redmik40/kernel-build}"
CONTAINER_NAME="${CONTAINER_NAME:-redmik40-kernel-build}"
LOG_FILE="${LOG_FILE:-$OUT_DIR/build.log}"
INTERVAL="${INTERVAL:-5}"
ONCE=0

usage() {
  cat <<EOF
Usage: $(basename "$0") [--once] [--interval SECONDS]

Environment:
  OUT_DIR=$OUT_DIR
  CONTAINER_NAME=$CONTAINER_NAME
  LOG_FILE=$LOG_FILE

Examples:
  scripts/monitor_kernel_build.sh
  scripts/monitor_kernel_build.sh --once
  INTERVAL=2 scripts/monitor_kernel_build.sh
EOF
}

while [ $# -gt 0 ]; do
  case "$1" in
    --once)
      ONCE=1
      shift
      ;;
    --interval)
      INTERVAL="${2:?missing interval}"
      shift 2
      ;;
    -h|--help|help)
      usage
      exit 0
      ;;
    *)
      usage >&2
      exit 2
      ;;
  esac
done

stat_mtime() {
  stat -f %m "$1" 2>/dev/null || stat -c %Y "$1" 2>/dev/null || echo 0
}

duration() {
  local seconds="$1"
  if [ "$seconds" -lt 0 ]; then
    seconds=0
  fi
  printf '%02d:%02d:%02d' "$((seconds / 3600))" "$(((seconds % 3600) / 60))" "$((seconds % 60))"
}

line_count() {
  if [ -f "$1" ]; then
    wc -l < "$1" | tr -d ' '
  else
    echo 0
  fi
}

object_count() {
  if [ -d "$OUT_DIR/build" ]; then
    find "$OUT_DIR/build" -type f \( -name '*.o' -o -name '*.a' -o -name '*.ko' \) 2>/dev/null | wc -l | tr -d ' '
  else
    echo 0
  fi
}

latest_build_line() {
  if [ -f "$LOG_FILE" ]; then
    grep -E '^[[:space:]]*(AR|AS|CC|CHK|CALL|GEN|HOSTCC|HOSTLD|LD|LDS|LEX|MODPOST|OBJCOPY|UPD|YACC)[[:space:]]+' "$LOG_FILE" | tail -1 || true
  fi
}

error_summary() {
  if [ -f "$LOG_FILE" ]; then
    grep -nE '([Ee]rror:|fatal:|undefined reference|No rule to make target|make(\[[0-9]+\])?: \*\*\*)' "$LOG_FILE" | tail -12 || true
  fi
}

container_status() {
  if ! command -v docker >/dev/null 2>&1; then
    echo "docker: missing"
    return
  fi
  if docker inspect "$CONTAINER_NAME" >/dev/null 2>&1; then
    docker inspect --format 'container: {{.Name}} status={{.State.Status}} running={{.State.Running}} exit={{.State.ExitCode}} started={{.State.StartedAt}} finished={{.State.FinishedAt}}' "$CONTAINER_NAME"
  else
    echo "container: $CONTAINER_NAME not found"
  fi
}

print_status() {
  local now log_age log_lines objects
  now="$(date '+%F %T')"
  log_lines="$(line_count "$LOG_FILE")"
  objects="$(object_count)"
  if [ -f "$LOG_FILE" ]; then
    log_age="$(duration "$(($(date +%s) - $(stat_mtime "$LOG_FILE")))")"
  else
    log_age="n/a"
  fi

  if [ "$ONCE" -eq 0 ] && [ -t 1 ]; then
    printf '\033[H\033[2J'
  fi

  echo "[$now] MiCode alioth kernel build monitor"
  echo "$(container_status)"
  echo "out: $OUT_DIR"
  echo "log: $LOG_FILE"
  echo "log_lines: $log_lines  log_idle: $log_age  objects/libs/modules: $objects"

  if [ -f "$OUT_DIR/artifacts/Image" ]; then
    ls -lh "$OUT_DIR/artifacts/Image"
  else
    echo "artifact: Image not created yet"
  fi

  local latest
  latest="$(latest_build_line)"
  if [ -n "$latest" ]; then
    echo
    echo "latest build step:"
    echo "$latest"
  fi

  local errors
  errors="$(error_summary)"
  if [ -n "$errors" ]; then
    echo
    echo "recent errors:"
    echo "$errors"
  fi

  if [ -f "$LOG_FILE" ]; then
    echo
    echo "last 25 log lines:"
    tail -25 "$LOG_FILE"
  fi
}

while true; do
  print_status
  if [ "$ONCE" -eq 1 ]; then
    break
  fi
  sleep "$INTERVAL"
done
