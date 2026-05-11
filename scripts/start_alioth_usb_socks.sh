#!/usr/bin/env bash
set -euo pipefail

DEVICE_IP="${DEVICE_IP:-172.16.42.2}"
SOCKS_PORT="${SOCKS_PORT:-18080}"
KEY="${KEY:-/vmdata/android/redmik40/keys/alioth_usb_ed25519}"
KNOWN_HOSTS="${KNOWN_HOSTS:-/vmdata/android/redmik40/keys/known_hosts}"
TEST_URL="${TEST_URL:-https://archlinux.org/}"

ssh_base=(
  ssh
  -i "$KEY"
  -o "UserKnownHostsFile=$KNOWN_HOSTS"
  -o StrictHostKeyChecking=no
  -o ConnectTimeout=4
  -o BatchMode=yes
)

log() {
  printf '[%s] %s\n' "$(date '+%F %T')" "$*"
}

remote() {
  "${ssh_base[@]}" "root@$DEVICE_IP" "$@"
}

ensure_device_ready() {
  log "checking SSH on root@$DEVICE_IP"
  remote 'true'

  log "enabling loopback on device"
  remote 'ip link set lo up 2>/dev/null || ifconfig lo up 2>/dev/null || true; ip addr add 127.0.0.1/8 dev lo 2>/dev/null || true'

  host_utc="$(date -u '+%Y-%m-%d %H:%M:%S')"
  log "syncing device UTC time to $host_utc"
  remote "date -u -s '$host_utc' >/dev/null; date -u"
}

remote_socks_listening() {
  remote "ss -ltn 2>/dev/null | awk '{print \$4}' | grep -qx '127.0.0.1:$SOCKS_PORT'"
}

start_forward() {
  if remote_socks_listening; then
    log "device already has 127.0.0.1:$SOCKS_PORT listening"
    return 0
  fi

  log "starting reverse dynamic SOCKS on device 127.0.0.1:$SOCKS_PORT"
  ssh \
    -fN \
    -i "$KEY" \
    -o "UserKnownHostsFile=$KNOWN_HOSTS" \
    -o StrictHostKeyChecking=no \
    -o ExitOnForwardFailure=yes \
    -R "127.0.0.1:$SOCKS_PORT" \
    "root@$DEVICE_IP"
}

verify_proxy() {
  log "verifying HTTPS through device SOCKS"
  remote "ALL_PROXY=socks5h://127.0.0.1:$SOCKS_PORT curl -fsS --connect-timeout 5 --max-time 20 '$TEST_URL' -o /dev/null -w 'http=%{http_code}\n'"
}

ensure_device_ready
start_forward
verify_proxy

cat <<EOF

Device-side internet proxy is ready.

Use on the phone:
  export ALL_PROXY=socks5h://127.0.0.1:$SOCKS_PORT
  curl https://archlinux.org/
EOF
