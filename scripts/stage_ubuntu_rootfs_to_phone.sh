#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

DEVICE_IP="${DEVICE_IP:-172.16.42.2}"
HOST_IP="${HOST_IP:-172.16.42.1}"
HTTP_PORT="${HTTP_PORT:-8088}"
SSH_KEY="${SSH_KEY:-/vmdata/android/redmik40/keys/alioth_usb_ed25519}"
KNOWN_HOSTS="${KNOWN_HOSTS:-/tmp/alioth_ubuntu_stage_known_hosts}"
ROOTFS_NAME="${ROOTFS_NAME:-ubuntu-24.04}"
ROOTFS_TARBALL="${ROOTFS_TARBALL:-/vmdata/android/redmik40/rootfs-cache/ubuntu-24.04/noble-server-cloudimg-arm64-root.tar.xz}"
ROOTFS_SHA256="${ROOTFS_SHA256:-3c8f36e427583571cb5536b339355b9d9b60c233eb7aed5e51c41b3ceba5accf}"
POLICY_SCRIPT="${POLICY_SCRIPT:-$REPO_DIR/scripts/prep_ubuntu_rootfs_policy.sh}"
SSH_PUBKEY_FILE="${SSH_PUBKEY_FILE:-/vmdata/android/redmik40/keys/alioth_usb_ed25519.pub}"
BRIGHTNESS_HELPER_FILE="${BRIGHTNESS_HELPER_FILE:-$REPO_DIR/scripts/alioth_phone_brightness.sh}"
TIME_SYNC_HELPER_FILE="${TIME_SYNC_HELPER_FILE:-$REPO_DIR/scripts/alioth_http_time_sync.sh}"
TIME_SYNC_UNIT_FILE="${TIME_SYNC_UNIT_FILE:-$REPO_DIR/configs/alioth-http-time-sync.service}"
AUDIO_BOOT_HELPER_FILE="${AUDIO_BOOT_HELPER_FILE:-$REPO_DIR/scripts/alioth_audio_adsp_boot.sh}"
AUDIO_BOOT_UNIT_FILE="${AUDIO_BOOT_UNIT_FILE:-$REPO_DIR/configs/alioth-audio-adsp-boot.service}"
REPLACE="${REPLACE:-0}"
CHECK_ONLY="${CHECK_ONLY:-0}"

die() {
  echo "error: $*" >&2
  exit 1
}

log() {
  printf '[stage] %s\n' "$*"
}

require_file() {
  [ -f "$1" ] || die "missing file: $1"
}

ssh_phone() {
  ssh -i "$SSH_KEY" \
    -o UserKnownHostsFile="$KNOWN_HOSTS" \
    -o StrictHostKeyChecking=no \
    -o ConnectTimeout=8 \
    root@"$DEVICE_IP" "$@"
}

scp_to_phone() {
  scp -i "$SSH_KEY" \
    -o UserKnownHostsFile="$KNOWN_HOSTS" \
    -o StrictHostKeyChecking=no \
    "$1" root@"$DEVICE_IP":"$2"
}

start_http() {
  local dir
  dir="$(dirname "$ROOTFS_TARBALL")"
  (cd "$dir" && python3 -m http.server "$HTTP_PORT" --bind "$HOST_IP" >/tmp/alioth-rootfs-http.log 2>&1) &
  http_pid=$!
}

stop_http() {
  local pid="${1:-}"
  [ -n "$pid" ] || return 0
  kill "$pid" >/dev/null 2>&1 || true
  wait "$pid" >/dev/null 2>&1 || true
}

require_file "$ROOTFS_TARBALL"
require_file "$POLICY_SCRIPT"
require_file "$SSH_KEY"
require_file "$SSH_PUBKEY_FILE"
require_file "$BRIGHTNESS_HELPER_FILE"
require_file "$TIME_SYNC_HELPER_FILE"
require_file "$TIME_SYNC_UNIT_FILE"
require_file "$AUDIO_BOOT_HELPER_FILE"
require_file "$AUDIO_BOOT_UNIT_FILE"
command -v sha256sum >/dev/null 2>&1 || die "sha256sum missing"
command -v python3 >/dev/null 2>&1 || die "python3 missing"
command -v ssh >/dev/null 2>&1 || die "ssh missing"
command -v scp >/dev/null 2>&1 || die "scp missing"

actual_sha="$(sha256sum "$ROOTFS_TARBALL" | awk '{print $1}')"
[ "$actual_sha" = "$ROOTFS_SHA256" ] || die "tarball sha mismatch: got $actual_sha expected $ROOTFS_SHA256"
log "host tarball sha OK: $actual_sha"

log "checking phone connectivity"
ssh_phone 'cat /etc/lele-rootfs-id 2>/dev/null || cat /etc/os-release | sed -n "1,3p"; ip route | sed -n "1,5p"' | sed 's/^/[phone] /'

scp_to_phone "$POLICY_SCRIPT" /tmp/prep_ubuntu_rootfs_policy.sh
scp_to_phone "$SSH_PUBKEY_FILE" /tmp/alioth-stage-authorized-key.pub
scp_to_phone "$BRIGHTNESS_HELPER_FILE" /tmp/alioth_phone_brightness.sh
scp_to_phone "$TIME_SYNC_HELPER_FILE" /tmp/alioth_http_time_sync.sh
scp_to_phone "$TIME_SYNC_UNIT_FILE" /tmp/alioth-http-time-sync.service
scp_to_phone "$AUDIO_BOOT_HELPER_FILE" /tmp/alioth_audio_adsp_boot.sh
scp_to_phone "$AUDIO_BOOT_UNIT_FILE" /tmp/alioth-audio-adsp-boot.service
ssh_phone 'chmod +x /tmp/prep_ubuntu_rootfs_policy.sh'

remote_probe='
set -e
ROOTFS_NAME="$1"
REPLACE="$2"
CHECK_ONLY="$3"
prepare_base() {
  if [ -d /data/rootfs ] && mountpoint -q /data 2>/dev/null; then
    echo /data/rootfs
    return 0
  fi
  mkdir -p /mnt/userdata-parent
  if ! mountpoint -q /mnt/userdata-parent; then
    if [ -b /dev/block/by-name/userdata ]; then
      dev=/dev/block/by-name/userdata
    else
      majmin="$(while read -r _ _ mm _ mp _; do [ "$mp" = "/" ] && { echo "$mm"; break; }; done < /proc/self/mountinfo)"
      [ -n "$majmin" ] || exit 11
      maj="${majmin%:*}"
      min="${majmin#*:}"
      dev=/dev/alioth-userdata
      [ -b "$dev" ] || mknod "$dev" b "$maj" "$min"
    fi
    mount -t ext4 -o rw,noatime "$dev" /mnt/userdata-parent
  fi
  mkdir -p /mnt/userdata-parent/rootfs
  echo /mnt/userdata-parent/rootfs
}
base="$(prepare_base)"
target="$base/$ROOTFS_NAME"
tmp="$base/.$ROOTFS_NAME.staging"
archive="$base/.$ROOTFS_NAME.tar.xz"
echo "base=$base"
echo "target=$target"
if [ -e "$target" ] && [ "$REPLACE" != "1" ]; then
  echo "target_exists=1"
  if [ "$CHECK_ONLY" = "1" ]; then
    exit 0
  fi
  exit 23
fi
echo "target_exists=0"
if [ "$CHECK_ONLY" = "1" ]; then
  exit 0
fi
'

log "probing phone target"
probe_out="$(ssh_phone "bash -s -- '$ROOTFS_NAME' '$REPLACE' '$CHECK_ONLY'" <<<"$remote_probe")"
printf '%s\n' "$probe_out" | sed 's/^/[phone] /'

if [ "$CHECK_ONLY" = "1" ]; then
  log "CHECK_ONLY=1 complete; no extraction performed"
  exit 0
fi

http_pid=""
trap 'stop_http "$http_pid"' EXIT
start_http
sleep 1
log "temporary HTTP server pid=$http_pid url=http://$HOST_IP:$HTTP_PORT/$(basename "$ROOTFS_TARBALL")"

remote_stage='
set -e
ROOTFS_NAME="$1"
REPLACE="$2"
ROOTFS_SHA256="$3"
URL="$4"
prepare_base() {
  if [ -d /data/rootfs ] && mountpoint -q /data 2>/dev/null; then
    echo /data/rootfs
    return 0
  fi
  mkdir -p /mnt/userdata-parent
  if ! mountpoint -q /mnt/userdata-parent; then
    if [ -b /dev/block/by-name/userdata ]; then
      dev=/dev/block/by-name/userdata
    else
      majmin="$(while read -r _ _ mm _ mp _; do [ "$mp" = "/" ] && { echo "$mm"; break; }; done < /proc/self/mountinfo)"
      [ -n "$majmin" ] || exit 11
      maj="${majmin%:*}"
      min="${majmin#*:}"
      dev=/dev/alioth-userdata
      [ -b "$dev" ] || mknod "$dev" b "$maj" "$min"
    fi
    mount -t ext4 -o rw,noatime "$dev" /mnt/userdata-parent
  fi
  mkdir -p /mnt/userdata-parent/rootfs
  echo /mnt/userdata-parent/rootfs
}
base="$(prepare_base)"
target="$base/$ROOTFS_NAME"
tmp="$base/.$ROOTFS_NAME.staging"
archive="$base/.$ROOTFS_NAME.tar.xz"
if [ -e "$target" ]; then
  [ "$REPLACE" = "1" ] || exit 23
  rm -rf "$target.old"
  mv "$target" "$target.old"
fi
rm -rf "$tmp" "$archive"
mkdir -p "$tmp"
curl -fL --connect-timeout 10 -o "$archive" "$URL"
echo "$ROOTFS_SHA256  $archive" | sha256sum -c -
tar -xJf "$archive" -C "$tmp"
SSH_PUBKEY_FILE=/tmp/alioth-stage-authorized-key.pub BRIGHTNESS_HELPER_FILE=/tmp/alioth_phone_brightness.sh TIME_SYNC_HELPER_FILE=/tmp/alioth_http_time_sync.sh TIME_SYNC_UNIT_FILE=/tmp/alioth-http-time-sync.service AUDIO_BOOT_HELPER_FILE=/tmp/alioth_audio_adsp_boot.sh AUDIO_BOOT_UNIT_FILE=/tmp/alioth-audio-adsp-boot.service /tmp/prep_ubuntu_rootfs_policy.sh apply "$tmp"
SSH_PUBKEY_FILE=/tmp/alioth-stage-authorized-key.pub BRIGHTNESS_HELPER_FILE=/tmp/alioth_phone_brightness.sh TIME_SYNC_HELPER_FILE=/tmp/alioth_http_time_sync.sh TIME_SYNC_UNIT_FILE=/tmp/alioth-http-time-sync.service AUDIO_BOOT_HELPER_FILE=/tmp/alioth_audio_adsp_boot.sh AUDIO_BOOT_UNIT_FILE=/tmp/alioth-audio-adsp-boot.service /tmp/prep_ubuntu_rootfs_policy.sh check "$tmp"
rm -f "$archive"
sync
mv "$tmp" "$target"
sync
echo "staged=$target"
cat "$target/etc/lele-rootfs-id" 2>/dev/null || true
du -sh "$target"
'

url="http://$HOST_IP:$HTTP_PORT/$(basename "$ROOTFS_TARBALL")"
log "staging rootfs on phone"
ssh_phone "bash -s -- '$ROOTFS_NAME' '$REPLACE' '$ROOTFS_SHA256' '$url'" <<<"$remote_stage"
log "stage complete"
