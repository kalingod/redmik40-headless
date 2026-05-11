#!/bin/sh
# prep_ubuntu_headless.sh - Configure Ubuntu 24.04 rootfs as headless server
# Usage: prep_ubuntu_headless.sh apply ROOT
#        prep_ubuntu_headless.sh check ROOT
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
MODE="${1:-}"
ROOT="${2:-}"

[ "$MODE" = "apply" ] || [ "$MODE" = "check" ] || { echo "usage: $0 apply|check ROOT" >&2; exit 2; }
[ -n "$ROOT" ] || { echo "usage: $0 apply|check ROOT" >&2; exit 2; }
[ -d "$ROOT" ] || { echo "error: ROOT does not exist: $ROOT" >&2; exit 1; }
ROOT="$(cd "$ROOT" && pwd -P)"

log() { printf '[headless] %s\n' "$*"; }
fail() { echo "FAIL: $*" >&2; exit 1; }

changed=0
write_if_absent() {
  local rel="$1" content="$2" mode_bits="${3:-0644}"
  if [ -f "$ROOT/$rel" ]; then
    return 0
  fi
  if [ "$MODE" = "apply" ]; then
    mkdir -p "$(dirname "$ROOT/$rel")"
    printf '%s' "$content" > "$ROOT/$rel"
    chmod "$mode_bits" "$ROOT/$rel"
    changed=$((changed + 1))
    log "wrote $rel"
  else
    fail "$rel missing"
  fi
}

ensure_symlink_to_null() {
  local rel="$1"
  if [ -L "$ROOT/$rel" ] && [ "$(readlink "$ROOT/$rel")" = "/dev/null" ]; then
    return 0
  fi
  if [ "$MODE" = "apply" ]; then
    rm -f "$ROOT/$rel"
    mkdir -p "$(dirname "$ROOT/$rel")"
    ln -s /dev/null "$ROOT/$rel"
    changed=$((changed + 1))
    log "masked $rel"
  else
    fail "$rel not masked"
  fi
}

ensure_dir() {
  local rel="$1" mode_bits="${2:-0755}"
  if [ "$MODE" = "apply" ]; then
    mkdir -p "$ROOT/$rel"
    [ -z "$mode_bits" ] || chmod "$mode_bits" "$ROOT/$rel"
  elif [ ! -d "$ROOT/$rel" ]; then
    fail "$rel directory missing"
  fi
}

# ---- 1. SSH ----
log "configuring SSH"
ensure_dir "etc/ssh/sshd_config.d" "0755"
ensure_dir "root/.ssh" "0700"
write_if_absent "etc/ssh/sshd_config.d/01-headless.conf" \
"PermitRootLogin prohibit-password
PasswordAuthentication no
PubkeyAuthentication yes
UsePAM no
"

# ---- 2. Hostname ----
log "configuring hostname"
write_if_absent "etc/hostname" "alioth-headless\n"
[ "$MODE" = "apply" ] && [ ! -e "$ROOT/etc/machine-id" ] && : > "$ROOT/etc/machine-id"

# ---- 3. fstab ----
log "configuring fstab"
write_if_absent "etc/fstab" "# Headless Ubuntu - root mounted by initramfs switch_root
"

# ---- 4. Network - systemd-networkd for usb0 ----
log "configuring network"
ensure_dir "etc/systemd/network" "0755"
write_if_absent "etc/systemd/network/20-usb0-ncm.network" \
"[Match]
Name=usb0

[Network]
Address=172.16.42.2/24
Gateway=172.16.42.1
DNS=8.8.8.8
DNS=1.1.1.1
"
write_if_absent "etc/systemd/network/20-wlan0-dhcp.network" \
"[Match]
Name=wlan0

[Network]
DHCP=yes
"

# ---- 5. systemd masks (headless) ----
log "masking unwanted services"
ensure_dir "etc/systemd/system"
for unit in \
  sleep.target \
  suspend.target \
  hibernate.target \
  hybrid-sleep.target \
  systemd-networkd-wait-online.service \
  multipathd.service \
  multipathd.socket \
  systemd-rfkill.socket; do
  ensure_symlink_to_null "etc/systemd/system/$unit"
done

# ---- 6. Logind - ignore power key, no idle action ----
log "configuring logind"
ensure_dir "etc/systemd/logind.conf.d"
write_if_absent "etc/systemd/logind.conf.d/01-headless.conf" \
"[Login]
HandlePowerKey=ignore
HandleSuspendKey=ignore
HandleHibernateKey=ignore
HandleLidSwitch=ignore
IdleAction=ignore
"

# ---- 7. Journal persistent ----
log "configuring journal"
ensure_dir "var/log/journal" "2755"
ensure_dir "etc/systemd/journald.conf.d"
write_if_absent "etc/systemd/journald.conf.d/01-headless.conf" \
"[Journal]
Storage=persistent
SystemMaxUse=128M
RuntimeMaxUse=32M
MaxRetentionSec=14day
"

# ---- 8. Enable key services ----
log "enabling services"
ensure_dir "etc/systemd/system/multi-user.target.wants"
for svc in \
  ssh.service \
  systemd-networkd.service \
  systemd-resolved.service; do
  if [ "$MODE" = "apply" ]; then
    ln -sf "/usr/lib/systemd/system/$svc" "$ROOT/etc/systemd/system/multi-user.target.wants/$svc" 2>/dev/null || \
    ln -sf "/lib/systemd/system/$svc" "$ROOT/etc/systemd/system/multi-user.target.wants/$svc" 2>/dev/null || true
  fi
done

# ---- 9. Rootfs marker ----
log "writing rootfs marker"
write_if_absent "etc/headless-rootfs-id" "ubuntu-24.04-headless-server\n"

if [ "$MODE" = "check" ]; then
  log "check passed for $ROOT"
else
  log "apply complete for $ROOT; changed=$changed"
fi
