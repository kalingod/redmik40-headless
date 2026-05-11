#!/bin/sh
set -u

PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

LOG_FILE="${ALIOTH_HTTP_TIME_SYNC_LOG:-/run/alioth-http-time-sync.log}"
MIN_EPOCH="${ALIOTH_HTTP_TIME_SYNC_MIN_EPOCH:-1767225600}" # 2026-01-01T00:00:00Z
MAX_FUTURE_SKEW="${ALIOTH_HTTP_TIME_SYNC_MAX_FUTURE_SKEW:-31622400}" # 366 days
SET_THRESHOLD="${ALIOTH_HTTP_TIME_SYNC_SET_THRESHOLD:-2}"
URLS="${ALIOTH_HTTP_TIME_SYNC_URLS:-http://connectivitycheck.ubuntu.com/ http://archive.ubuntu.com/ubuntu/ http://1.1.1.1/}"

mkdir -p "$(dirname "$LOG_FILE")" 2>/dev/null || true

log() {
  msg="$*"
  printf '%s %s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ' 2>/dev/null || echo unknown-time)" "$msg" >>"$LOG_FILE" 2>/dev/null || true
  logger -t alioth-http-time-sync -- "$msg" 2>/dev/null || true
}

abs_delta() {
  a="$1"
  b="$2"
  if [ "$a" -ge "$b" ]; then
    echo $((a - b))
  else
    echo $((b - a))
  fi
}

already_synchronized() {
  command -v timedatectl >/dev/null 2>&1 || return 1
  [ "$(timedatectl show -p NTPSynchronized --value 2>/dev/null || true)" = "yes" ]
}

extract_http_date() {
  url="$1"
  command -v curl >/dev/null 2>&1 || return 1
  curl -fsSI --connect-timeout 4 --max-time 8 "$url" 2>/dev/null \
    | tr -d '\r' \
    | awk 'BEGIN { IGNORECASE = 1 } /^Date:[[:space:]]*/ { sub(/^Date:[[:space:]]*/, ""); print; exit }'
}

parse_epoch() {
  header="$1"
  date -u -d "$header" +%s 2>/dev/null
}

set_clock() {
  epoch="$1"
  date -u -s "@$epoch" >/dev/null 2>&1
}

if already_synchronized; then
  log "NTP already synchronized; no HTTP Date correction needed"
  exit 0
fi

now="$(date -u +%s 2>/dev/null || echo 0)"
[ "$now" -ge 0 ] 2>/dev/null || now=0
max_epoch=$((now + MAX_FUTURE_SKEW))

for url in $URLS; do
  header="$(extract_http_date "$url" || true)"
  if [ -z "$header" ]; then
    log "no HTTP Date header from $url"
    continue
  fi

  epoch="$(parse_epoch "$header" || true)"
  if [ -z "$epoch" ] || ! [ "$epoch" -eq "$epoch" ] 2>/dev/null; then
    log "ignored unparsable HTTP Date from $url: $header"
    continue
  fi

  if [ "$epoch" -lt "$MIN_EPOCH" ]; then
    log "ignored stale HTTP Date from $url: $header epoch=$epoch"
    continue
  fi

  if [ "$now" -ge "$MIN_EPOCH" ] && [ "$epoch" -gt "$max_epoch" ]; then
    log "ignored far-future HTTP Date from $url: $header epoch=$epoch now=$now"
    continue
  fi

  delta="$(abs_delta "$epoch" "$now")"
  if [ "$delta" -le "$SET_THRESHOLD" ]; then
    log "clock already close enough via $url: header='$header' now=$now epoch=$epoch delta=${delta}s"
    exit 0
  fi

  if set_clock "$epoch"; then
    after="$(date -u +%s 2>/dev/null || echo 0)"
    log "set UTC from $url: header='$header' old=$now new=$after target=$epoch delta=${delta}s"
    exit 0
  fi

  log "date -u -s failed for $url: header='$header' epoch=$epoch"
done

log "no usable HTTP Date source; leaving clock unchanged"
exit 0
