#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

SSH_PUBKEY_FILE="${SSH_PUBKEY_FILE:-/vmdata/android/redmik40/keys/alioth_usb_ed25519.pub}"
BRIGHTNESS_HELPER_FILE="${BRIGHTNESS_HELPER_FILE:-$SCRIPT_DIR/alioth_phone_brightness.sh}"
TIME_SYNC_HELPER_FILE="${TIME_SYNC_HELPER_FILE:-$SCRIPT_DIR/alioth_http_time_sync.sh}"
TIME_SYNC_UNIT_FILE="${TIME_SYNC_UNIT_FILE:-$REPO_DIR/configs/alioth-http-time-sync.service}"
AUDIO_BOOT_HELPER_FILE="${AUDIO_BOOT_HELPER_FILE:-$SCRIPT_DIR/alioth_audio_adsp_boot.sh}"
AUDIO_BOOT_UNIT_FILE="${AUDIO_BOOT_UNIT_FILE:-$REPO_DIR/configs/alioth-audio-adsp-boot.service}"
WIFI_PREPARE_HELPER_FILE="${WIFI_PREPARE_HELPER_FILE:-$SCRIPT_DIR/alioth_wifi_prepare.sh}"
WIFI_BRINGUP_HELPER_FILE="${WIFI_BRINGUP_HELPER_FILE:-$SCRIPT_DIR/alioth_wifi_bringup.sh}"
WIFI_SCAN_HELPER_FILE="${WIFI_SCAN_HELPER_FILE:-$SCRIPT_DIR/alioth_wifi_scan.sh}"
WIFI_CONNECT_HELPER_FILE="${WIFI_CONNECT_HELPER_FILE:-$SCRIPT_DIR/alioth_wifi_connect.sh}"
WIFI_STATUS_HELPER_FILE="${WIFI_STATUS_HELPER_FILE:-$SCRIPT_DIR/alioth_wifi_status.sh}"
ANDROID_DAEMON_HELPER_FILE="${ANDROID_DAEMON_HELPER_FILE:-$SCRIPT_DIR/alioth_android_daemon.sh}"
WIFI_PREPARE_UNIT_FILE="${WIFI_PREPARE_UNIT_FILE:-$REPO_DIR/configs/alioth-wifi-prepare.service}"
WIFI_QRTR_UNIT_FILE="${WIFI_QRTR_UNIT_FILE:-$REPO_DIR/configs/alioth-qrtr-ns.service}"
WIFI_CNSS_UNIT_FILE="${WIFI_CNSS_UNIT_FILE:-$REPO_DIR/configs/alioth-cnss-daemon.service}"
WIFI_BRINGUP_UNIT_FILE="${WIFI_BRINGUP_UNIT_FILE:-$REPO_DIR/configs/alioth-wifi-bringup.service}"
WIFI_CONNECT_UNIT_FILE="${WIFI_CONNECT_UNIT_FILE:-$REPO_DIR/configs/alioth-wifi-connect.service}"
HOSTNAME="${HOSTNAME:-alioth-ubuntu}"
JOURNAL_MAX="${JOURNAL_MAX:-128M}"
JOURNAL_RUNTIME_MAX="${JOURNAL_RUNTIME_MAX:-32M}"
JOURNAL_RETENTION="${JOURNAL_RETENTION:-14day}"

usage() {
  cat >&2 <<'EOF'
usage:
  prep_ubuntu_rootfs_policy.sh apply ROOT
  prep_ubuntu_rootfs_policy.sh check ROOT

Applies or verifies the LELE Ubuntu rootfs policy. ROOT may be / for a live
Ubuntu phone rootfs, or an extracted rootfs directory such as
/mnt/userdata-parent/rootfs/ubuntu-24.04.
EOF
}

die() {
  echo "error: $*" >&2
  exit 1
}

log() {
  printf '[policy] %s\n' "$*"
}

mode="${1:-}"
root="${2:-}"
[ "$mode" = "apply" ] || [ "$mode" = "check" ] || { usage; exit 2; }
[ -n "$root" ] || { usage; exit 2; }
[ -d "$root" ] || die "ROOT does not exist: $root"
root="$(cd "$root" && pwd -P)"
[ -f "$root/etc/os-release" ] || die "ROOT does not look like a Linux rootfs: $root"

changed=0
failed=0

mark_changed() {
  changed=$((changed + 1))
}

fail_check() {
  echo "FAIL: $*" >&2
  failed=$((failed + 1))
}

ensure_dir() {
  local path="$1" mode_bits="${2:-}" owner="${3:-}"
  if [ "$mode" = "apply" ]; then
    mkdir -p "$root/$path"
    [ -z "$owner" ] || chown "$owner" "$root/$path" 2>/dev/null || true
    [ -z "$mode_bits" ] || chmod "$mode_bits" "$root/$path"
  elif [ ! -d "$root/$path" ]; then
    fail_check "$path directory missing"
  fi
}

write_if_changed() {
  local rel="$1" content="$2" tmp
  tmp="$(mktemp)"
  printf '%s' "$content" > "$tmp"
  if [ -f "$root/$rel" ] && cmp -s "$tmp" "$root/$rel"; then
    rm -f "$tmp"
    return 0
  fi
  if [ "$mode" = "apply" ]; then
    mkdir -p "$(dirname "$root/$rel")"
    cp "$tmp" "$root/$rel"
    mark_changed
    log "wrote $rel"
  else
    fail_check "$rel differs or missing"
  fi
  rm -f "$tmp"
}

ensure_symlink_to_null() {
  local rel="$1"
  if [ -L "$root/$rel" ] && [ "$(readlink "$root/$rel")" = "/dev/null" ]; then
    return 0
  fi
  if [ "$mode" = "apply" ]; then
    rm -f "$root/$rel"
    mkdir -p "$(dirname "$root/$rel")"
    ln -s /dev/null "$root/$rel"
    mark_changed
    log "masked $rel"
  else
    fail_check "$rel is not masked"
  fi
}

ensure_symlink() {
  local rel="$1" target="$2"
  if [ -L "$root/$rel" ] && [ "$(readlink "$root/$rel")" = "$target" ]; then
    return 0
  fi
  if [ "$mode" = "apply" ]; then
    rm -f "$root/$rel"
    mkdir -p "$(dirname "$root/$rel")"
    ln -s "$target" "$root/$rel"
    mark_changed
    log "linked $rel -> $target"
  else
    fail_check "$rel is not linked to $target"
  fi
}

remove_path() {
  local rel="$1"
  [ -e "$root/$rel" ] || [ -L "$root/$rel" ] || return 0
  if [ "$mode" = "apply" ]; then
    rm -rf "$root/$rel"
    mark_changed
    log "removed $rel"
  else
    fail_check "$rel should be absent"
  fi
}

ensure_file_from_source() {
  local rel="$1" src="$2" mode_bits="${3:-0644}"

  if [ -f "$src" ]; then
    if [ "$mode" = "apply" ]; then
      mkdir -p "$(dirname "$root/$rel")"
      if [ ! -f "$root/$rel" ] || ! cmp -s "$src" "$root/$rel"; then
        cp "$src" "$root/$rel"
        mark_changed
        log "installed $rel"
      fi
      chmod "$mode_bits" "$root/$rel"
    else
      if [ ! -f "$root/$rel" ]; then
        fail_check "$rel missing"
      elif ! cmp -s "$src" "$root/$rel"; then
        fail_check "$rel differs from $src"
      fi
    fi
    return 0
  fi

  if [ "$mode" = "check" ]; then
    if [ ! -f "$root/$rel" ]; then
      fail_check "$rel missing and source unavailable: $src"
    fi
  else
    log "skipped $rel; source unavailable: $src"
  fi
}

ensure_fstab() {
  local rel="etc/fstab"
  local content="# LELE OS: root is mounted by initramfs switch_root from userdata subdir.
# Keep fstab empty for now; systemd-remount-fs must not look for cloudimg-rootfs.
"
  if [ -f "$root/$rel" ] && ! grep -q 'LABEL=cloudimg-rootfs' "$root/$rel"; then
    return 0
  fi
  write_if_changed "$rel" "$content"
}

ensure_sshd_policy() {
  local rel="etc/ssh/sshd_config.d/99-lele-root.conf"
  local content="PermitRootLogin prohibit-password
PasswordAuthentication no
PubkeyAuthentication yes
UsePAM no
"
  write_if_changed "$rel" "$content"
  if [ "$mode" = "apply" ]; then
    chmod 0644 "$root/$rel"
  fi
}

ensure_authorized_key() {
  [ -f "$SSH_PUBKEY_FILE" ] || die "SSH public key missing: $SSH_PUBKEY_FILE"
  local key rel="root/.ssh/authorized_keys"
  key="$(cat "$SSH_PUBKEY_FILE")"$'\n'
  write_if_changed "$rel" "$key"
  if [ "$mode" = "apply" ]; then
    chmod 0700 "$root/root/.ssh"
    chmod 0600 "$root/$rel"
  fi
}

ensure_inet_policy() {
  local group_file="$root/etc/group" passwd_file="$root/etc/passwd" tmp
  [ -f "$group_file" ] || die "missing /etc/group"
  [ -f "$passwd_file" ] || die "missing /etc/passwd"

  if [ "$mode" = "apply" ]; then
    tmp="$(mktemp)"
    awk -F: '
      BEGIN { OFS = FS; seen = 0 }
      $1 == "inet" {
        seen = 1
        $3 = "3003"
        if ($4 !~ /(^|,)root(,|$)/) $4 = ($4 ? $4 ",root" : "root")
        if ($4 !~ /(^|,)_apt(,|$)/) $4 = ($4 ? $4 ",_apt" : "_apt")
      }
      { print }
      END {
        if (!seen) print "inet", "x", "3003", "root,_apt"
      }
    ' "$group_file" > "$tmp"
    if ! cmp -s "$tmp" "$group_file"; then
      cp "$tmp" "$group_file"
      mark_changed
      log "updated etc/group inet policy"
    fi
    rm -f "$tmp"

    tmp="$(mktemp)"
    awk -F: 'BEGIN { OFS = FS } $1 == "_apt" { $4 = "3003" } { print }' "$passwd_file" > "$tmp"
    if ! cmp -s "$tmp" "$passwd_file"; then
      cp "$tmp" "$passwd_file"
      mark_changed
      log "updated _apt primary gid"
    fi
    rm -f "$tmp"
  else
    if ! awk -F: '$1 == "inet" && $3 == "3003" && $4 ~ /(^|,)root(,|$)/ && $4 ~ /(^|,)_apt(,|$)/ { ok = 1 } END { exit ok ? 0 : 1 }' "$group_file"; then
      fail_check "inet group policy missing"
    fi
    if ! awk -F: '$1 == "_apt" && $4 == "3003" { ok = 1 } END { exit ok ? 0 : 1 }' "$passwd_file"; then
      fail_check "_apt primary gid is not 3003"
    fi
  fi
}

ensure_systemd_masks() {
  local unit
  ensure_dir "etc/systemd/system"
  for unit in \
    systemd-networkd-wait-online.service \
    multipathd.service \
    multipathd.socket \
    systemd-rfkill.socket \
    lele-minitcpsh.service; do
    ensure_symlink_to_null "etc/systemd/system/$unit"
  done
  remove_path "etc/systemd/system/multi-user.target.wants/lele-minitcpsh.service"
}

ensure_phone_helpers() {
  ensure_dir "usr/local/sbin" "0755"
  ensure_file_from_source "usr/local/sbin/lele-brightness" "$BRIGHTNESS_HELPER_FILE" "0755"
  ensure_file_from_source "usr/local/sbin/alioth-http-time-sync" "$TIME_SYNC_HELPER_FILE" "0755"
  ensure_file_from_source "etc/systemd/system/alioth-http-time-sync.service" "$TIME_SYNC_UNIT_FILE" "0644"
  ensure_file_from_source "usr/local/sbin/alioth-audio-adsp-boot" "$AUDIO_BOOT_HELPER_FILE" "0755"
  ensure_file_from_source "etc/systemd/system/alioth-audio-adsp-boot.service" "$AUDIO_BOOT_UNIT_FILE" "0644"
  ensure_file_from_source "usr/local/sbin/alioth-wifi-prepare" "$WIFI_PREPARE_HELPER_FILE" "0755"
  ensure_file_from_source "usr/local/sbin/alioth-wifi-bringup" "$WIFI_BRINGUP_HELPER_FILE" "0755"
  ensure_file_from_source "usr/local/sbin/alioth-wifi-scan" "$WIFI_SCAN_HELPER_FILE" "0755"
  ensure_file_from_source "usr/local/sbin/alioth-wifi-connect" "$WIFI_CONNECT_HELPER_FILE" "0755"
  ensure_file_from_source "usr/local/sbin/alioth-wifi-status" "$WIFI_STATUS_HELPER_FILE" "0755"
  ensure_file_from_source "usr/local/sbin/alioth-android-daemon" "$ANDROID_DAEMON_HELPER_FILE" "0755"
  ensure_file_from_source "etc/systemd/system/alioth-wifi-prepare.service" "$WIFI_PREPARE_UNIT_FILE" "0644"
  ensure_file_from_source "etc/systemd/system/alioth-qrtr-ns.service" "$WIFI_QRTR_UNIT_FILE" "0644"
  ensure_file_from_source "etc/systemd/system/alioth-cnss-daemon.service" "$WIFI_CNSS_UNIT_FILE" "0644"
  ensure_file_from_source "etc/systemd/system/alioth-wifi-bringup.service" "$WIFI_BRINGUP_UNIT_FILE" "0644"
  ensure_file_from_source "etc/systemd/system/alioth-wifi-connect.service" "$WIFI_CONNECT_UNIT_FILE" "0644"
  ensure_dir "etc/systemd/system/multi-user.target.wants" "0755"
  ensure_symlink "etc/systemd/system/multi-user.target.wants/alioth-http-time-sync.service" "../alioth-http-time-sync.service"
  ensure_symlink "etc/systemd/system/multi-user.target.wants/alioth-audio-adsp-boot.service" "../alioth-audio-adsp-boot.service"
  ensure_symlink "etc/systemd/system/multi-user.target.wants/alioth-wifi-bringup.service" "../alioth-wifi-bringup.service"
  ensure_symlink "etc/systemd/system/multi-user.target.wants/alioth-wifi-connect.service" "../alioth-wifi-connect.service"
}

ensure_journal_policy() {
  local rel="etc/systemd/journald.conf.d/99-lele-persistent.conf"
  local content="[Journal]
Storage=persistent
SystemMaxUse=$JOURNAL_MAX
RuntimeMaxUse=$JOURNAL_RUNTIME_MAX
MaxRetentionSec=$JOURNAL_RETENTION
"
  ensure_dir "var/log/journal" "2755" "root:systemd-journal"
  ensure_dir "etc/systemd/journald.conf.d" "0755"
  write_if_changed "$rel" "$content"
  if [ "$mode" = "apply" ]; then
    chmod 0644 "$root/$rel"
  fi
}

ensure_hostname_machine_id() {
  write_if_changed "etc/hostname" "$HOSTNAME"$'\n'
  if [ "$mode" = "apply" ]; then
    [ -e "$root/etc/machine-id" ] || : > "$root/etc/machine-id"
  elif [ ! -e "$root/etc/machine-id" ]; then
    fail_check "etc/machine-id missing"
  fi
}

ensure_marker() {
  local pretty rel="etc/lele-rootfs-id"
  [ -f "$root/$rel" ] && return 0
  pretty="$(sed -n 's/^PRETTY_NAME=//p' "$root/etc/os-release" | tr -d '"')"
  write_if_changed "$rel" "ubuntu policy rootfs: $pretty"$'\n'
}

ensure_fstab
ensure_dir "etc/ssh/sshd_config.d" "0755"
ensure_dir "root/.ssh" "0700"
ensure_sshd_policy
ensure_authorized_key
ensure_inet_policy
ensure_systemd_masks
ensure_phone_helpers
ensure_journal_policy
ensure_hostname_machine_id
ensure_marker

if [ "$mode" = "check" ]; then
  if [ "$failed" -gt 0 ]; then
    die "$failed policy check(s) failed for $root"
  fi
  log "check passed for $root"
else
  log "apply complete for $root; changed=$changed"
fi
