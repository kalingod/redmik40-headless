#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

BASE_BOOT_IMG="${BASE_BOOT_IMG:-$REPO_DIR/images/orangefox-arch-chroot.img}"
KERNEL_IMG="${KERNEL_IMG:-/vmdata/android/redmik40/lineage-sm8250/artifacts/kernel-lineage20-alioth/Image}"
OUT_DIR="${OUT_DIR:-/vmdata/android/redmik40/lineage-sm8250/mininitramfs}"
BUSYBOX_APK_URL="${BUSYBOX_APK_URL:-https://dl-cdn.alpinelinux.org/alpine/edge/main/aarch64/busybox-static-1.37.0-r31.apk}"
ADB_BIN="${ADB_BIN:-adb}"
FASTBOOT_BIN="${FASTBOOT_BIN:-fastboot}"
BUILDER_IMAGE="${BUILDER_IMAGE:-redmik40-kernel-builder:bullseye}"
MINITCPSH_SRC="${MINITCPSH_SRC:-$REPO_DIR/src/minitcpsh/minitcpsh.c}"
USB_DHCPD_SRC="${USB_DHCPD_SRC:-$REPO_DIR/src/alioth-usb-dhcpd/alioth_usb_dhcpd.c}"
USB_DHCPD_BIN="${USB_DHCPD_BIN:-}"
REBOOT_TOOL_SRC="${REBOOT_TOOL_SRC:-$REPO_DIR/src/alioth-reboot/alioth_reboot.c}"
REBOOT_TOOL_BIN="${REBOOT_TOOL_BIN:-}"
STATUS_UI_SRC="${STATUS_UI_SRC:-$REPO_DIR/src/alioth-status-ui-c/alioth_status_ui.c}"
STATUS_UI_BIN="${STATUS_UI_BIN:-}"
STATUS_UI_WRAPPER_BIN="${STATUS_UI_WRAPPER_BIN:-}"
INIT_MODE="${INIT_MODE:-shell}"
UBUNTU_ROOTFS_PATH="${UBUNTU_ROOTFS_PATH:-/rootfs/ubuntu-26.04}"
UBUNTU_SYSTEMD_WRAPPER_MODE="${UBUNTU_SYSTEMD_WRAPPER_MODE:-exec}"
SSH_PUBKEY_FILE="${SSH_PUBKEY_FILE:-/vmdata/android/redmik40/keys/alioth_usb_ed25519.pub}"
NCM_DEVICE_IP="${NCM_DEVICE_IP:-172.16.42.2}"
NCM_HOST_IP="${NCM_HOST_IP:-172.16.42.1}"
NCM_PORT="${NCM_PORT:-2323}"
DEFAULT_WIFI_SSID="${DEFAULT_WIFI_SSID:-${ALIOTH_WIFI_SSID:-}}"
DEFAULT_WIFI_PSK="${DEFAULT_WIFI_PSK:-${ALIOTH_WIFI_PSK:-}}"
DEFAULT_WIFI_CONFIG="${DEFAULT_WIFI_CONFIG:-}"

die() {
  echo "error: $*" >&2
  exit 1
}

log() {
  printf '[%s] %s\n' "$(date '+%F %T')" "$*"
}

require_cmd() {
  command -v "$1" >/dev/null 2>&1 || die "missing command: $1"
}

require_file() {
  [ -f "$1" ] || die "missing file: $1"
}

adb_state() {
  "$ADB_BIN" devices 2>/dev/null | awk 'NR > 1 && NF >= 2 {print $2; exit}'
}

find_ncm_iface() {
  ip -br link 2>/dev/null | awk '$1 ~ /^enx/ {print $1; exit}'
}

configure_host_ncm() {
  require_cmd ip
  local iface con deadline
  iface="$(find_ncm_iface || true)"
  [ -n "$iface" ] || return 1
  if ip -br addr show "$iface" 2>/dev/null | grep -q "$NCM_HOST_IP/24"; then
    return 0
  fi
  if command -v nmcli >/dev/null 2>&1; then
    con="$(nmcli -t -f NAME,DEVICE con show --active 2>/dev/null | awk -F: -v i="$iface" '$2==i {print $1; exit}')"
    if [ -z "$con" ]; then
      con="alioth-usb-ncm"
      nmcli con add type ethernet ifname "$iface" con-name "$con" \
        ipv4.method auto ipv4.never-default yes ipv6.method ignore >/dev/null 2>&1 || return 1
    else
      nmcli con mod "$con" ipv4.method auto ipv4.addresses "" ipv4.gateway "" ipv4.dns "" \
        ipv4.never-default yes ipv6.method ignore >/dev/null 2>&1 || return 1
    fi
    nmcli con up "$con" >/dev/null 2>&1 || return 1
    deadline=$((SECONDS + 15))
    while [ "$SECONDS" -lt "$deadline" ]; do
      ip -br addr show "$iface" 2>/dev/null | grep -q "$NCM_HOST_IP/24" && return 0
      sleep 1
    done

    # Compatibility fallback for older test images without the device-side DHCP server.
    nmcli con mod "$con" ipv4.method manual ipv4.addresses "$NCM_HOST_IP/24" \
      ipv4.gateway "" ipv4.dns "" ipv4.never-default yes ipv6.method ignore >/dev/null 2>&1 || return 1
    nmcli con up "$con" >/dev/null 2>&1 || return 1
  else
    ip link set "$iface" up 2>/dev/null || true
    ip addr replace "$NCM_HOST_IP/24" dev "$iface" 2>/dev/null || return 1
  fi
}

ncm_shell_ready() {
  command -v nc >/dev/null 2>&1 || return 1
  ping -c 1 -W 1 "$NCM_DEVICE_IP" >/dev/null 2>&1
}

reboot_ncm_to_bootloader() {
  command -v nc >/dev/null 2>&1 || return 1
  configure_host_ncm || true
  ncm_shell_ready || return 1
  log "rebooting device to bootloader through USB NCM shell"
  {
    printf '%s\n' 'if [ -x /bin/reboot-bootloader ]; then /bin/reboot-bootloader;'
    printf '%s\n' 'elif [ -x /bin/alioth-reboot ]; then /bin/alioth-reboot bootloader;'
    printf '%s\n' 'elif [ -x /var/tmp/alioth-switchroot/reboot-bootloader ]; then /var/tmp/alioth-switchroot/reboot-bootloader;'
    printf '%s\n' 'elif [ -x /var/tmp/alioth-switchroot/alioth-reboot ]; then /var/tmp/alioth-switchroot/alioth-reboot bootloader;'
    printf '%s\n' 'else reboot -f; fi'
  } | nc -w 3 "$NCM_DEVICE_IP" "$NCM_PORT" >/dev/null 2>&1 || true
  wait_fastboot 120
}

wait_fastboot() {
  local timeout="${1:-90}" deadline
  deadline=$((SECONDS + timeout))
  while [ "$SECONDS" -lt "$deadline" ]; do
    if "$FASTBOOT_BIN" devices 2>/dev/null | awk 'NF >= 2 {found=1} END {exit !found}'; then
      "$FASTBOOT_BIN" devices -l
      return 0
    fi
    sleep 1
  done
  die "timed out waiting for fastboot"
}

ensure_fastboot() {
  if "$FASTBOOT_BIN" devices 2>/dev/null | awk 'NF >= 2 {found=1} END {exit !found}'; then
    return 0
  fi

  local state
  state="$(adb_state || true)"
  if [ -n "$state" ]; then
    log "rebooting device from adb state '$state' to bootloader"
    "$ADB_BIN" reboot bootloader
    wait_fastboot 120
    return 0
  fi

  if reboot_ncm_to_bootloader; then
    return 0
  fi

  die "device is not visible in fastboot, adb, or USB NCM shell"
}

write_init_script() {
  local path="$1"
  cat > "$path" <<'INIT'
#!/bin/busybox sh
export PATH=/bin:/sbin:/usr/bin:/usr/sbin

log() {
  echo "[alioth-mininit] $*" >/dev/kmsg 2>/dev/null || true
  echo "[alioth-mininit] $*" >/dev/console 2>/dev/null || true
}

mount_once() {
  type="$1"
  source="$2"
  target="$3"
  opts="${4:-}"
  mkdir -p "$target"
  grep -q " $target " /proc/mounts 2>/dev/null && return 0
  if [ -n "$opts" ]; then
    mount -t "$type" -o "$opts" "$source" "$target" 2>/dev/null || return 1
  else
    mount -t "$type" "$source" "$target" 2>/dev/null || return 1
  fi
}

seed_dev_nodes_at() {
  target="$1"
  mkdir -p "$target/block/by-name" "$target/pts"
  if [ ! -c "$target/null" ]; then
    rm -f "$target/null"
    mknod -m 0666 "$target/null" c 1 3 || true
  fi
  for spec in \
    "console 0600 5 1" \
    "null 0666 1 3" \
    "zero 0666 1 5" \
    "full 0666 1 7" \
    "random 0666 1 8" \
    "urandom 0666 1 9" \
    "kmsg 0600 1 11" \
    "tty 0666 5 0" \
    "ptmx 0666 5 2"; do
    set -- $spec
    name="$1"; mode="$2"; maj="$3"; min="$4"
    [ -c "$target/$name" ] || rm -f "$target/$name" 2>"$target/null" || true
    [ -c "$target/$name" ] || mknod -m "$mode" "$target/$name" c "$maj" "$min" 2>"$target/null" || true
  done
  rm -rf "$target/fd" "$target/stdin" "$target/stdout" "$target/stderr" 2>"$target/null" || true
  ln -s /proc/self/fd "$target/fd" 2>"$target/null" || true
  ln -s /proc/self/fd/0 "$target/stdin" 2>"$target/null" || true
  ln -s /proc/self/fd/1 "$target/stdout" 2>"$target/null" || true
  ln -s /proc/self/fd/2 "$target/stderr" 2>"$target/null" || true
}

seed_dev_nodes() {
  seed_dev_nodes_at /dev
}

mount_dev_tree() {
  root="$1"
  devdir="$root/dev"
  mkdir -p "$devdir" "$devdir/pts"
  grep -q " $devdir " /proc/mounts 2>/dev/null || mount -t tmpfs -o mode=0755 tmpfs "$devdir" 2>/tmp/dev-mount.err || {
    log "failed to mount tmpfs dev at $devdir: $(cat /tmp/dev-mount.err 2>/dev/null)"
    return 1
  }
  seed_dev_nodes_at "$devdir"
  if grep -q " $devdir/pts " /proc/mounts 2>/dev/null; then
    mount -o remount,mode=0620,ptmxmode=0666,gid=5 "$devdir/pts" 2>/dev/null || true
  else
    mount -t devpts -o mode=0620,ptmxmode=0666,gid=5 devpts "$devdir/pts" 2>/tmp/devpts-mount.err || {
      log "failed to mount devpts at $devdir/pts: $(cat /tmp/devpts-mount.err 2>/dev/null)"
      return 1
    }
  fi
}

create_char_node() {
  target="$1"
  majmin="$2"
  maj="${majmin%:*}"
  min="${majmin#*:}"
  case "$maj:$min" in
    *:*) ;;
    *) return 0 ;;
  esac
  dir="${target%/*}"
  [ "$dir" = "$target" ] || mkdir -p "$dir"
  [ -c "$target" ] || mknod -m 0600 "$target" c "$maj" "$min" 2>/dev/null || true
}

populate_char_nodes() {
  mkdir -p /dev/dri /dev/input /dev/graphics
  for sysdev in /sys/class/drm/*; do
    [ -e "$sysdev/dev" ] || continue
    name="${sysdev##*/}"
    create_char_node "/dev/dri/$name" "$(cat "$sysdev/dev" 2>/dev/null || true)"
  done
  for sysdev in /sys/class/input/event*; do
    [ -e "$sysdev/dev" ] || continue
    name="${sysdev##*/}"
    create_char_node "/dev/input/$name" "$(cat "$sysdev/dev" 2>/dev/null || true)"
  done
  for sysdev in /sys/class/graphics/fb*; do
    [ -e "$sysdev/dev" ] || continue
    name="${sysdev##*/}"
    majmin="$(cat "$sysdev/dev" 2>/dev/null || true)"
    create_char_node "/dev/graphics/$name" "$majmin"
    create_char_node "/dev/$name" "$majmin"
  done
}

populate_block_nodes() {
  mkdir -p /dev/block/by-name
  for sysdev in /sys/class/block/*; do
    [ -e "$sysdev/dev" ] || continue
    name="${sysdev##*/}"
    majmin="$(cat "$sysdev/dev" 2>/dev/null || true)"
    maj="${majmin%:*}"
    min="${majmin#*:}"
    case "$maj:$min" in
      *:*) ;;
      *) continue ;;
    esac
    [ -b "/dev/block/$name" ] || mknod -m 0600 "/dev/block/$name" b "$maj" "$min" 2>/dev/null || true
    partname="$(grep '^PARTNAME=' "$sysdev/uevent" 2>/dev/null | cut -d= -f2- | head -n 1)"
    if [ -n "$partname" ] && [ -b "/dev/block/$name" ]; then
      ln -sf "../$name" "/dev/block/by-name/$partname" 2>/dev/null || true
    fi
  done
}

setup_loopback() {
  ip link set lo up 2>/dev/null || ifconfig lo up 2>/dev/null || true
  ip addr add 127.0.0.1/8 dev lo 2>/dev/null || true
}

start_status_ui() {
  [ -x /bin/alioth-status-ui ] || return 0
  if [ -s /run/alioth-status-ui.pid ] && kill -0 "$(cat /run/alioth-status-ui.pid)" 2>/dev/null; then
    return 0
  fi
  (
    for i in $(seq 1 30); do
      populate_char_nodes
      [ -c /dev/dri/card0 ] && break
      sleep 1
    done
    exec /bin/alioth-status-ui
  ) >/tmp/alioth-status-ui.log 2>&1 &
  echo "$!" >/run/alioth-status-ui.pid
  log "status UI starting"
}

wait_arch_block() {
  for i in $(seq 1 60); do
    populate_block_nodes
    [ -b /dev/block/by-name/arch ] && echo /dev/block/by-name/arch && return 0
    [ -b /dev/block/sda37 ] && echo /dev/block/sda37 && return 0
    sleep 1
  done
  return 1
}

wait_data_block() {
  for i in $(seq 1 60); do
    populate_block_nodes
    [ -b /dev/block/by-name/userdata ] && echo /dev/block/by-name/userdata && return 0
    [ -b /dev/block/sda35 ] && echo /dev/block/sda35 && return 0
    sleep 1
  done
  return 1
}

mount_data_root() {
  dev="$(wait_data_block || true)"
  [ -n "$dev" ] || {
    log "userdata block device not found"
    return 1
  }
  mkdir -p /mnt/data
  grep -q " /mnt/data " /proc/mounts 2>/dev/null && return 0
  log "mounting $dev on /mnt/data"
  mount -t ext4 -o rw,noatime "$dev" /mnt/data 2>/tmp/data-mount.err && {
    log "mounted data root"
    return 0
  }
  log "failed to mount data root: $(cat /tmp/data-mount.err 2>/dev/null)"
  return 1
}

ubuntu_root_source() {
  rel="$(cat /etc/alioth-ubuntu-rootfs 2>/dev/null || echo /rootfs/ubuntu-26.04)"
  case "$rel" in
    /data/*)
      echo "/mnt${rel}"
      ;;
    /*)
      echo "/mnt/data${rel}"
      ;;
    *)
      echo "/mnt/data/${rel}"
      ;;
  esac
}

mount_ubuntu_root() {
  mount_data_root || return 1
  src="$(ubuntu_root_source)"
  [ -d "$src/bin" ] && [ -f "$src/etc/os-release" ] || {
    log "Ubuntu rootfs missing or incomplete: $src"
    return 1
  }
  mkdir -p /mnt/ubuntu
  grep -q " /mnt/ubuntu " /proc/mounts 2>/dev/null || mount --bind "$src" /mnt/ubuntu 2>/tmp/ubuntu-bind.err || {
    log "failed to bind Ubuntu rootfs: $(cat /tmp/ubuntu-bind.err 2>/dev/null)"
    return 1
  }
  mkdir -p /mnt/ubuntu/dev /mnt/ubuntu/proc /mnt/ubuntu/sys /mnt/ubuntu/run
  mount_dev_tree /mnt/ubuntu || return 1
  mount_once proc proc /mnt/ubuntu/proc || true
  mount_once sysfs sysfs /mnt/ubuntu/sys || true
  mkdir -p /mnt/ubuntu/sys/fs/cgroup
  grep -q " /mnt/ubuntu/sys/fs/cgroup " /proc/mounts 2>/dev/null || \
    mount -t cgroup2 -o nsdelegate cgroup2 /mnt/ubuntu/sys/fs/cgroup 2>/tmp/cgroup2-mount.err || \
    mount -t cgroup2 cgroup2 /mnt/ubuntu/sys/fs/cgroup 2>/tmp/cgroup2-mount.err || \
    log "failed to mount cgroup2 for Ubuntu: $(cat /tmp/cgroup2-mount.err 2>/dev/null)"
  mount_once tmpfs tmpfs /mnt/ubuntu/run mode=0755 || true
  return 0
}

setup_usb_ncm() {
  start_dhcp="${1:-yes}"
  mount_once configfs configfs /sys/kernel/config || {
    log "configfs unavailable"
    return 1
  }

  udc="$(ls /sys/class/udc 2>/dev/null | head -n 1)"
  [ -n "$udc" ] || {
    log "no UDC found"
    return 1
  }

  g=/sys/kernel/config/usb_gadget/alioth
  mkdir -p "$g"
  cd "$g" || return 1
  echo "" > UDC 2>/dev/null || true
  echo 0x18d1 > idVendor
  echo 0xd001 > idProduct
  echo 0x0200 > bcdUSB
  echo 0x0100 > bcdDevice
  mkdir -p strings/0x409 configs/c.1/strings/0x409
  echo alioth-mininitramfs > strings/0x409/serialnumber
  echo Xiaomi > strings/0x409/manufacturer
  echo "Alioth Linux initramfs" > strings/0x409/product
  echo "NCM network shell" > configs/c.1/strings/0x409/configuration
  mkdir -p functions/ncm.usb0 2>/dev/null || {
    log "ncm.usb0 function unavailable"
    cd /
    return 1
  }
  echo 02:00:00:00:42:02 > functions/ncm.usb0/dev_addr 2>/dev/null || true
  echo 02:00:00:00:42:01 > functions/ncm.usb0/host_addr 2>/dev/null || true
  ln -sf functions/ncm.usb0 configs/c.1/ncm.usb0
  echo "$udc" > UDC 2>/dev/null || log "failed to bind UDC $udc"
  cd /

  for i in $(seq 1 20); do
    ip link set usb0 up 2>/dev/null && break
    ifconfig usb0 up 2>/dev/null && break
    sleep 1
  done
  ip addr add 172.16.42.2/24 dev usb0 2>/dev/null || ifconfig usb0 172.16.42.2 netmask 255.255.255.0 up 2>/dev/null || true
  if [ "$start_dhcp" = "yes" ] && [ -x /bin/alioth-usb-dhcpd ]; then
    /bin/alioth-usb-dhcpd usb0 172.16.42.2 172.16.42.1 >/tmp/alioth-usb-dhcpd.log 2>&1 &
  fi
  log "usb ncm ready: host should get 172.16.42.1/24 by DHCP, then nc 172.16.42.2 2323"
}

start_initramfs_tcp_shell() {
  [ -x /bin/minitcpsh ] || return 0
  /bin/minitcpsh 2323 /bin/sh >/tmp/minitcpsh.log 2>&1 &
  log "initramfs tcp shell ready on 172.16.42.2:2323"
}

mount_arch_root() {
  dev="$(wait_arch_block || true)"
  [ -n "$dev" ] || {
    log "arch block device not found"
    return 1
  }
  mkdir -p /mnt/arch
  log "mounting $dev on /mnt/arch"
  mount -t ext4 -o rw,noatime "$dev" /mnt/arch 2>/tmp/arch-mount.err && {
    log "mounted Arch rootfs"
    mkdir -p /mnt/arch/dev /mnt/arch/proc /mnt/arch/sys /mnt/arch/run
    mount --bind /dev /mnt/arch/dev 2>/dev/null || true
    mount_once proc proc /mnt/arch/proc || true
    mount_once sysfs sysfs /mnt/arch/sys || true
    mount_once tmpfs tmpfs /mnt/arch/run mode=0755 || true
    return 0
  }
  log "failed to mount Arch rootfs: $(cat /tmp/arch-mount.err 2>/dev/null)"
  return 1
}

write_enter_arch() {
  cat > /bin/enter-arch <<'EOF'
#!/bin/sh
mkdir -p /mnt/arch/dev /mnt/arch/proc /mnt/arch/sys /mnt/arch/run
mount --bind /dev /mnt/arch/dev 2>/dev/null || true
mount -t proc proc /mnt/arch/proc 2>/dev/null || true
mount -t sysfs sysfs /mnt/arch/sys 2>/dev/null || true
mount -t tmpfs tmpfs /mnt/arch/run 2>/dev/null || true
if [ "$#" -gt 0 ]; then
  exec chroot /mnt/arch /bin/sh -lc "$*"
fi
exec chroot /mnt/arch /bin/bash -l 2>/dev/null || exec chroot /mnt/arch /bin/sh -l
EOF
  chmod 0755 /bin/enter-arch
}

write_enter_ubuntu() {
  cat > /bin/enter-ubuntu <<'EOF'
#!/bin/sh
mkdir -p /mnt/data /mnt/ubuntu
dev=""
for candidate in /dev/block/by-name/userdata /dev/block/sda35; do
  [ -b "$candidate" ] && dev="$candidate" && break
done
[ -n "$dev" ] || { echo "userdata block device not found" >&2; exit 1; }
grep -q " /mnt/data " /proc/mounts 2>/dev/null || mount -t ext4 -o rw,noatime "$dev" /mnt/data || exit 1
rel="$(cat /etc/alioth-ubuntu-rootfs 2>/dev/null || echo /rootfs/ubuntu-26.04)"
case "$rel" in
  /data/*) root="/mnt${rel}" ;;
  /*) root="/mnt/data${rel}" ;;
  *) root="/mnt/data/${rel}" ;;
esac
[ -d "$root/bin" ] || { echo "Ubuntu rootfs not found: $root" >&2; exit 1; }
grep -q " /mnt/ubuntu " /proc/mounts 2>/dev/null || mount --bind "$root" /mnt/ubuntu || exit 1
mkdir -p /mnt/ubuntu/dev /mnt/ubuntu/proc /mnt/ubuntu/sys /mnt/ubuntu/run
grep -q " /mnt/ubuntu/dev " /proc/mounts 2>/dev/null || mount -t tmpfs -o mode=0755 tmpfs /mnt/ubuntu/dev || exit 1
mkdir -p /mnt/ubuntu/dev/pts
if [ ! -c /mnt/ubuntu/dev/null ]; then
  rm -f /mnt/ubuntu/dev/null
  mknod -m 0666 /mnt/ubuntu/dev/null c 1 3 || true
fi
for spec in \
  "console 0600 5 1" \
  "kmsg 0600 1 11" \
  "null 0666 1 3" \
  "zero 0666 1 5" \
  "full 0666 1 7" \
  "random 0666 1 8" \
  "urandom 0666 1 9" \
  "tty 0666 5 0" \
  "ptmx 0666 5 2"; do
  set -- $spec
  name="$1"; mode="$2"; maj="$3"; min="$4"
  [ -c "/mnt/ubuntu/dev/$name" ] || rm -f "/mnt/ubuntu/dev/$name" 2>/mnt/ubuntu/dev/null || true
  [ -c "/mnt/ubuntu/dev/$name" ] || mknod -m "$mode" "/mnt/ubuntu/dev/$name" c "$maj" "$min" 2>/mnt/ubuntu/dev/null || true
done
rm -rf /mnt/ubuntu/dev/fd /mnt/ubuntu/dev/stdin /mnt/ubuntu/dev/stdout /mnt/ubuntu/dev/stderr 2>/mnt/ubuntu/dev/null || true
ln -s /proc/self/fd /mnt/ubuntu/dev/fd 2>/mnt/ubuntu/dev/null || true
ln -s /proc/self/fd/0 /mnt/ubuntu/dev/stdin 2>/mnt/ubuntu/dev/null || true
ln -s /proc/self/fd/1 /mnt/ubuntu/dev/stdout 2>/mnt/ubuntu/dev/null || true
ln -s /proc/self/fd/2 /mnt/ubuntu/dev/stderr 2>/mnt/ubuntu/dev/null || true
grep -q " /mnt/ubuntu/dev/pts " /proc/mounts 2>/dev/null || mount -t devpts -o mode=0620,ptmxmode=0666,gid=5 devpts /mnt/ubuntu/dev/pts 2>/mnt/ubuntu/dev/null || true
mount -t proc proc /mnt/ubuntu/proc 2>/dev/null || true
mount -t sysfs sysfs /mnt/ubuntu/sys 2>/dev/null || true
mkdir -p /mnt/ubuntu/sys/fs/cgroup
grep -q " /mnt/ubuntu/sys/fs/cgroup " /proc/mounts 2>/dev/null || mount -t cgroup2 -o nsdelegate cgroup2 /mnt/ubuntu/sys/fs/cgroup 2>/mnt/ubuntu/dev/null || mount -t cgroup2 cgroup2 /mnt/ubuntu/sys/fs/cgroup 2>/mnt/ubuntu/dev/null || true
mount -t tmpfs tmpfs /mnt/ubuntu/run 2>/dev/null || true
if [ "$#" -gt 0 ]; then
  exec chroot /mnt/ubuntu /bin/sh -lc "$*"
fi
exec chroot /mnt/ubuntu /bin/bash -l 2>/dev/null || exec chroot /mnt/ubuntu /bin/sh -l
EOF
  chmod 0755 /bin/enter-ubuntu
}

switch_to_arch_root() {
  mode="$1"
  [ -d /mnt/arch/bin ] || {
    log "cannot switch_root: /mnt/arch does not look like a rootfs"
    return 1
  }

  helper=/mnt/arch/var/tmp/alioth-switchroot
  mkdir -p "$helper" /mnt/arch/dev /mnt/arch/proc /mnt/arch/sys /mnt/arch/run
  cp /bin/minitcpsh "$helper/minitcpsh"
  chmod 0755 "$helper/minitcpsh"
  cp /bin/alioth-usb-dhcpd "$helper/alioth-usb-dhcpd"
  chmod 0755 "$helper/alioth-usb-dhcpd"
  cp /bin/busybox "$helper/busybox"
  chmod 0755 "$helper/busybox"
  for app in sh mount mkdir mknod rm ip ifconfig sleep true false; do
    ln -sf busybox "$helper/$app"
  done
  cp /bin/alioth-reboot "$helper/alioth-reboot"
  chmod 0755 "$helper/alioth-reboot"
  cat > "$helper/reboot-bootloader" <<'EOF'
#!/var/tmp/alioth-switchroot/sh
exec /var/tmp/alioth-switchroot/alioth-reboot bootloader
EOF
  chmod 0755 "$helper/reboot-bootloader"
  if [ -x /bin/alioth-status-ui ]; then
    cp /bin/alioth-status-ui "$helper/alioth-status-ui"
    chmod 0755 "$helper/alioth-status-ui"
  fi
  if [ -x /bin/alioth-status-ui-cpu ]; then
    cp /bin/alioth-status-ui-cpu "$helper/alioth-status-ui-cpu"
    chmod 0755 "$helper/alioth-status-ui-cpu"
  fi
  if [ -f /etc/alioth-wifi-default ]; then
    mkdir -p /mnt/arch/etc
    cp /etc/alioth-wifi-default /mnt/arch/etc/alioth-wifi-default
    chmod 0600 /mnt/arch/etc/alioth-wifi-default 2>/dev/null || true
  fi
  cat > "$helper/alioth-switch-init" <<'EOF'
#!/var/tmp/alioth-switchroot/sh
helper=/var/tmp/alioth-switchroot
export PATH=$helper:/usr/local/sbin:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin
boot_mode="${1:-switchroot-shell}"

log() {
  mkdir -p /run 2>/dev/null || true
  printf '[%s] %s\n' "$(cut -d' ' -f1 /proc/uptime 2>/dev/null || echo 0)" "$*" >>/run/alioth-switchroot.log 2>/dev/null || true
  echo "[alioth-switchroot] $*" >/dev/kmsg 2>/dev/null || true
  echo "[alioth-switchroot] $*" >/dev/console 2>/dev/null || true
}

log_cmd() {
  desc="$1"
  shift
  log "BEGIN $desc"
  {
    printf '\n### %s\n' "$desc"
    printf '$'
    for arg in "$@"; do printf ' %s' "$arg"; done
    printf '\n'
    "$@"
    rc=$?
    printf 'rc=%s\n' "$rc"
  } >>/run/alioth-switchroot.log 2>&1
  log "END $desc rc=$rc"
  return "$rc"
}

snapshot_wifi_debug() {
  label="$1"
  {
    printf '\n===== %s =====\n' "$label"
    date 2>/dev/null || true
    printf '\n--- uptime ---\n'
    cat /proc/uptime 2>/dev/null || true
    printf '\n--- cmdline ---\n'
    cat /proc/cmdline 2>/dev/null || true
    printf '\n--- mounts ---\n'
    mount 2>/dev/null | grep -E 'android_| /system | /vendor | /odm | /product | /system_ext |firmware_mnt|persist|/apex' || true
    printf '\n--- device nodes ---\n'
    ls -l /dev/null /dev/wlan /dev/mapper/control /dev/block/by-name/super 2>/dev/null || true
    printf '\n--- android paths ---\n'
    ls -ld /system /vendor /odm /product /system_ext /apex/com.android.runtime /vendor/firmware_mnt /firmware 2>/dev/null || true
    ls -l /vendor/bin/cnss-daemon /vendor/bin/qrtr-ns /vendor/etc/qmi_fw.conf /system/bin/linker64 2>/dev/null || true
    printf '\n--- qca6390 firmware ---\n'
    ls -l /vendor/firmware_mnt/image/qca6390/amss20.bin /vendor/firmware_mnt/image/qca6390/m3.bin /vendor/firmware_mnt/image/qca6390/bd_k11a.elf /vendor/firmware_mnt/image/qca6390/regdb.bin /mnt/vendor/persist/wlan_mac.bin 2>/dev/null || true
    printf '\n--- qcacld firmware config ---\n'
    ls -ld /vendor/firmware/wlan /vendor/firmware/wlan/qca_cld /lib/firmware/wlan /lib/firmware/wlan/qca_cld 2>/dev/null || true
    ls -l /vendor/etc/wifi/qca6390/WCNSS_qcom_cfg.ini /vendor/firmware/wlan/qca_cld/qca6390/WCNSS_qcom_cfg.ini /lib/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini /lib/firmware/wlan/qca_cld/wlan_mac.bin 2>/dev/null || true
    ls -lL /lib/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini /lib/firmware/wlan/qca_cld/wlan_mac.bin 2>/dev/null || true
    printf '\n--- processes ---\n'
    ps -ef 2>/dev/null | grep -E 'cnss|qrtr|qmi' | grep -v grep || true
    printf '\n--- net links ---\n'
    ip -br link 2>/dev/null || true
    iw dev 2>/dev/null || true
    printf '\n--- qrtr proc ---\n'
    cat /proc/net/qrtr 2>/dev/null || true
    printf '\n--- cnss daemon log ---\n'
    tail -120 /run/cnss-daemon.log 2>/dev/null || true
    printf '\n--- qrtr-ns log ---\n'
    tail -120 /run/qrtr-ns.log 2>/dev/null || true
    printf '\n--- dmesg cnss tail ---\n'
    dmesg 2>/dev/null | grep -iE 'alioth-switchroot|cnss|wlan|qca|wlfw|calibration|firmware|qmi|qrtr|timeout|recovery|ASSERT|failed' | tail -220 || true
  } >>/run/alioth-wifi-debug.log 2>&1
}

mkdir -p /dev /dev/pts
for spec in \
  "console 0600 5 1" \
  "kmsg 0600 1 11" \
  "null 0666 1 3" \
  "zero 0666 1 5" \
  "full 0666 1 7" \
  "random 0666 1 8" \
  "urandom 0666 1 9" \
  "tty 0666 5 0" \
  "ptmx 0666 5 2"; do
  set -- $spec
  name="$1"; mode="$2"; maj="$3"; min="$4"
  [ -c "/dev/$name" ] || rm -f "/dev/$name" || true
  [ -c "/dev/$name" ] || mknod -m "$mode" "/dev/$name" c "$maj" "$min" || true
done

mount -t proc proc /proc 2>/dev/null || true
mount -t sysfs sysfs /sys 2>/dev/null || true
mount -t tmpfs tmpfs /run 2>/dev/null || true
mkdir -p /dev/pts /sys/kernel/config
mount -t devpts devpts /dev/pts 2>/dev/null || true
mount -t configfs configfs /sys/kernel/config 2>/dev/null || true

seed_dev_nodes() {
  mkdir -p /dev /dev/pts
  for spec in \
    "console 0600 5 1" \
    "kmsg 0600 1 11" \
    "null 0666 1 3" \
    "zero 0666 1 5" \
    "full 0666 1 7" \
    "random 0666 1 8" \
    "urandom 0666 1 9" \
    "tty 0666 5 0" \
    "ptmx 0666 5 2"; do
    set -- $spec
    name="$1"; mode="$2"; maj="$3"; min="$4"
    [ -c "/dev/$name" ] || rm -f "/dev/$name" 2>/dev/null || true
    [ -c "/dev/$name" ] || mknod -m "$mode" "/dev/$name" c "$maj" "$min" 2>/dev/null || true
  done
  ln -sf /proc/self/fd /dev/fd 2>/dev/null || true
  ln -sf /proc/self/fd/0 /dev/stdin 2>/dev/null || true
  ln -sf /proc/self/fd/1 /dev/stdout 2>/dev/null || true
  ln -sf /proc/self/fd/2 /dev/stderr 2>/dev/null || true
}

create_char_node() {
  target="$1"
  majmin="$2"
  maj="${majmin%:*}"
  min="${majmin#*:}"
  case "$maj:$min" in
    *:*) ;;
    *) return 0 ;;
  esac
  dir="${target%/*}"
  [ "$dir" = "$target" ] || mkdir -p "$dir"
  [ -c "$target" ] || mknod -m 0600 "$target" c "$maj" "$min" 2>/dev/null || true
}

populate_char_nodes() {
  mkdir -p /dev/dri /dev/input /dev/graphics
  for sysdev in /sys/class/drm/*; do
    [ -e "$sysdev/dev" ] || continue
    name="${sysdev##*/}"
    create_char_node "/dev/dri/$name" "$(cat "$sysdev/dev" 2>/dev/null || true)"
  done
  for sysdev in /sys/class/input/event*; do
    [ -e "$sysdev/dev" ] || continue
    name="${sysdev##*/}"
    create_char_node "/dev/input/$name" "$(cat "$sysdev/dev" 2>/dev/null || true)"
  done
  for sysdev in /sys/class/graphics/fb*; do
    [ -e "$sysdev/dev" ] || continue
    name="${sysdev##*/}"
    majmin="$(cat "$sysdev/dev" 2>/dev/null || true)"
    create_char_node "/dev/graphics/$name" "$majmin"
    create_char_node "/dev/$name" "$majmin"
  done
}

create_block_node() {
  target="$1"
  majmin="$2"
  maj="${majmin%:*}"
  min="${majmin#*:}"
  case "$maj:$min" in
    *:*) ;;
    *) return 0 ;;
  esac
  dir="${target%/*}"
  [ "$dir" = "$target" ] || mkdir -p "$dir"
  [ -b "$target" ] || mknod -m 0600 "$target" b "$maj" "$min" 2>/dev/null || true
}

populate_block_nodes() {
  mkdir -p /dev/block/by-name
  for sysdev in /sys/class/block/*; do
    [ -e "$sysdev/dev" ] || continue
    name="${sysdev##*/}"
    create_block_node "/dev/block/$name" "$(cat "$sysdev/dev" 2>/dev/null || true)"
    partname="$(grep '^PARTNAME=' "$sysdev/uevent" 2>/dev/null | cut -d= -f2- | head -n 1)"
    if [ -n "$partname" ] && [ -b "/dev/block/$name" ]; then
      ln -sf "../$name" "/dev/block/by-name/$partname" 2>/dev/null || true
    fi
  done
}

mount_once() {
  type="$1"
  source="$2"
  target="$3"
  opts="${4:-}"
  mkdir -p "$target"
  grep -q " $target " /proc/mounts 2>/dev/null && return 0
  if [ -n "$opts" ]; then
    mount -t "$type" -o "$opts" "$source" "$target" 2>/dev/null || return 1
  else
    mount -t "$type" "$source" "$target" 2>/dev/null || return 1
  fi
}

bind_mount_once() {
  source="$1"
  target="$2"
  [ -e "$source" ] || return 1
  mkdir -p "$target"
  grep -q " $target " /proc/mounts 2>/dev/null && return 0
  mount --bind "$source" "$target" 2>/dev/null || return 1
}

active_slot_suffix() {
  suffix="$(tr ' ' '\n' </proc/cmdline 2>/dev/null | sed -n 's/^androidboot.slot_suffix=//p' | head -n 1)"
  case "$suffix" in
    _a|_b) echo "$suffix" ;;
    a|b) echo "_$suffix" ;;
    *) echo "_a" ;;
  esac
}

setup_loop_nodes() {
  [ -c /dev/loop-control ] || mknod -m 0660 /dev/loop-control c 10 237 2>/dev/null || true
  for i in 0 1 2 3 4 5 6 7; do
    [ -b "/dev/loop$i" ] || mknod -m 0660 "/dev/loop$i" b 7 "$i" 2>/dev/null || true
  done
}

dm_node_for_name() {
  dm_name="$1"
  mkdir -p /dev/mapper
  for sysdev in /sys/class/block/dm-*; do
    [ -e "$sysdev/dev" ] || continue
    [ "$(cat "$sysdev/dm/name" 2>/dev/null || true)" = "$dm_name" ] || continue
    majmin="$(cat "$sysdev/dev" 2>/dev/null || true)"
    maj="${majmin%:*}"
    min="${majmin#*:}"
    [ -b "/dev/mapper/$dm_name" ] || mknod -m 0600 "/dev/mapper/$dm_name" b "$maj" "$min" 2>/dev/null || true
    return 0
  done
  return 1
}

map_logical_partition() {
  part="$1"
  target="$2"
  slot="$3"
  dm_name="android_$part"

  [ -b /dev/block/by-name/super ] || return 1
  command -v lpdump >/dev/null 2>&1 || return 1
  command -v dmsetup >/dev/null 2>&1 || return 1
  mkdir -p /dev/mapper
  [ -c /dev/mapper/control ] || mknod -m 0600 /dev/mapper/control c 10 236 2>/dev/null || true

  if ! [ -b "/dev/mapper/$dm_name" ]; then
    table="$(
      lpdump --slot="$slot" /dev/block/by-name/super 2>/dev/null |
        awk -v name="$part" '
          $1 == "Name:" && $2 == name { in_part = 1; next }
          in_part && $1 == "Name:" { exit }
          in_part && $2 == ".." && $4 == "linear" {
            printf "%s %s linear /dev/block/by-name/%s %s\n", $1, ($3 - $1 + 1), $5, $6
          }
        '
    )"
    [ -n "$table" ] || return 1
    dmsetup remove "$dm_name" >/dev/null 2>&1 || true
    printf '%s\n' "$table" | dmsetup create "$dm_name" >/dev/null 2>&1 || return 1
    dmsetup mknodes >/dev/null 2>&1 || true
    dm_node_for_name "$dm_name" || true
  fi

  [ -b "/dev/mapper/$dm_name" ] || return 1
  mount_once ext4 "/dev/mapper/$dm_name" "$target" ro
}

setup_android_mounts() {
  populate_block_nodes
  suffix="$(active_slot_suffix)"
  slot=0
  [ "$suffix" = "_b" ] && slot=1

  log "active slot suffix=$suffix slot=$slot"
  map_logical_partition "system$suffix" /mnt/android_system "$slot" && log "mounted system$suffix" || log "android system mount skipped"
  map_logical_partition "system_ext$suffix" /mnt/android_system_ext "$slot" && log "mounted system_ext$suffix" || log "android system_ext mount skipped"
  map_logical_partition "product$suffix" /mnt/android_product "$slot" && log "mounted product$suffix" || log "android product mount skipped"
  map_logical_partition "odm$suffix" /mnt/android_odm "$slot" && log "mounted odm$suffix" || log "android odm mount skipped"
  map_logical_partition "vendor$suffix" /mnt/android_vendor "$slot" && log "mounted vendor$suffix" || log "android vendor mount skipped"

  bind_mount_once /mnt/android_system/system /system && log "bound /system" || log "bind /system skipped"
  bind_mount_once /mnt/android_vendor /vendor && log "bound /vendor" || log "bind /vendor skipped"
  bind_mount_once /mnt/android_odm /odm && log "bound /odm" || log "bind /odm skipped"
  bind_mount_once /mnt/android_product /product && log "bound /product" || log "bind /product skipped"
  bind_mount_once /mnt/android_system_ext /system_ext && log "bound /system_ext" || log "bind /system_ext skipped"
}

setup_qcom_wifi_firmware() {
  populate_block_nodes
  suffix="$(active_slot_suffix)"
  modem=""
  for candidate in "/dev/block/by-name/modem$suffix" /dev/block/by-name/modem_b /dev/block/by-name/modem_a /dev/block/by-name/modem; do
    [ -b "$candidate" ] && modem="$candidate" && break
  done

  if [ -n "$modem" ]; then
    mount_once vfat "$modem" /vendor/firmware_mnt ro,shortname=lower || log "modem firmware mount skipped"
  else
    log "modem firmware block not found"
  fi

  [ -b /dev/block/by-name/persist ] && mount_once ext4 /dev/block/by-name/persist /mnt/vendor/persist ro || true

  if [ -d /vendor/firmware_mnt ]; then
    rm -f /firmware 2>/dev/null || true
    mkdir -p /firmware /lib/firmware/qca6390
    mountpoint -q /firmware 2>/dev/null || mount --bind /vendor/firmware_mnt /firmware 2>/dev/null || true
    if [ -d /vendor/firmware_mnt/image/qca6390 ]; then
      mountpoint -q /lib/firmware/qca6390 2>/dev/null || mount --bind /vendor/firmware_mnt/image/qca6390 /lib/firmware/qca6390 2>/dev/null || true
    fi
  fi

  if [ -d /vendor/firmware/wlan/qca_cld ] || [ -d /vendor/etc/wifi ]; then
    if mountpoint -q /lib/firmware/wlan/qca_cld 2>/dev/null; then
      umount /lib/firmware/wlan/qca_cld 2>/dev/null || true
    fi
    rm -rf /lib/firmware/wlan/qca_cld 2>/dev/null || true
    mkdir -p /lib/firmware/wlan/qca_cld

    for cfg in \
      /vendor/etc/wifi/qca6390/WCNSS_qcom_cfg.ini \
      /vendor/firmware/wlan/qca_cld/qca6390/WCNSS_qcom_cfg.ini \
      /vendor/etc/wifi/WCNSS_qcom_cfg.ini \
      /vendor/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini; do
      [ -f "$cfg" ] || continue
      cp -L "$cfg" /lib/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini 2>/dev/null && break
    done

    for mac in \
      /mnt/vendor/persist/wlan_mac.bin \
      /vendor/firmware/wlan/qca_cld/qca6390/wlan_mac.bin \
      /vendor/firmware/wlan/qca_cld/wlan_mac.bin; do
      [ -f "$mac" ] || continue
      cp -L "$mac" /lib/firmware/wlan/qca_cld/wlan_mac.bin 2>/dev/null && break
    done

    if [ -d /vendor/firmware/wlan/qca_cld/qca6390 ]; then
      mkdir -p /lib/firmware/wlan/qca_cld/qca6390
      cp -aL /vendor/firmware/wlan/qca_cld/qca6390/. /lib/firmware/wlan/qca_cld/qca6390/ 2>/dev/null || true
    fi
    if [ -d /vendor/firmware/wlan/qca_cld/qca6490 ]; then
      mkdir -p /lib/firmware/wlan/qca_cld/qca6490
      cp -aL /vendor/firmware/wlan/qca_cld/qca6490/. /lib/firmware/wlan/qca_cld/qca6490/ 2>/dev/null || true
    fi
  fi

  if [ -f /mnt/vendor/persist/wlan_mac.bin ]; then
    ln -sfn /mnt/vendor/persist/wlan_mac.bin /lib/firmware/wlan_mac.bin 2>/dev/null || true
  fi

  if [ -e /sys/devices/virtual/wlan/wlan/dev ]; then
    create_char_node /dev/wlan "$(cat /sys/devices/virtual/wlan/wlan/dev 2>/dev/null || true)"
    chmod 0660 /dev/wlan 2>/dev/null || true
  fi

  log_cmd "firmware listing" ls -l /vendor/firmware_mnt/image/qca6390/amss20.bin /vendor/firmware_mnt/image/qca6390/m3.bin /vendor/firmware_mnt/image/qca6390/bd_k11a.elf /vendor/firmware_mnt/image/qca6390/regdb.bin /mnt/vendor/persist/wlan_mac.bin /vendor/etc/wifi/qca6390/WCNSS_qcom_cfg.ini /vendor/firmware/wlan/qca_cld/qca6390/WCNSS_qcom_cfg.ini /lib/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini /lib/firmware/wlan/qca_cld/wlan_mac.bin || true
}

setup_android_runtime_apex() {
  [ -f /mnt/android_system/system/apex/com.android.runtime.apex ] || return 1
  [ -x /apex/com.android.runtime/bin/linker64 ] && return 0
  command -v bsdtar >/dev/null 2>&1 || return 1

  setup_loop_nodes
  mkdir -p /apex/com.android.runtime /run/apex-runtime
  rm -f /run/apex-runtime/apex_payload.img 2>/dev/null || true
  bsdtar -xf /mnt/android_system/system/apex/com.android.runtime.apex -C /run/apex-runtime apex_payload.img >/dev/null 2>&1 || return 1
  mount_once ext4 /run/apex-runtime/apex_payload.img /apex/com.android.runtime ro,loop || return 1
  log "mounted Android runtime APEX"
}

android_env_and_exec() {
  binary="$1"
  shift
  export ANDROID_ROOT=/system
  export ANDROID_DATA=/data
  export APEX_ROOT=/apex
  export LD_LIBRARY_PATH=/vendor/lib64:/odm/lib64:/system_ext/lib64:/product/lib64:/system/lib64:/apex/com.android.runtime/lib64/bionic
  exec /system/bin/linker64 "$binary" "$@"
}

start_android_daemon() {
  name="$1"
  binary="$2"
  shift 2
  [ -x /system/bin/linker64 ] || return 1
  [ -x "$binary" ] || return 1
  if [ -s "/run/$name.pid" ] && kill -0 "$(cat "/run/$name.pid")" 2>/dev/null; then
    return 0
  fi

  log "starting $name: $binary $*"
  ( android_env_and_exec "$binary" "$@" ) >"/run/$name.log" 2>&1 &
  echo "$!" >"/run/$name.pid"
  sleep 1
  if kill -0 "$(cat "/run/$name.pid")" 2>/dev/null; then
    log "$name running pid=$(cat "/run/$name.pid")"
    return 0
  fi
  log "$name exited early"
  tail -80 "/run/$name.log" >>/run/alioth-switchroot.log 2>/dev/null || true
  return 1
}

setup_android_vendor_runtime() {
  mkdir -p /data/vendor/wifi/sockets /data/vendor/wifi/hostapd/ctrl /data/vendor/wifi/wpa/sockets /data/vendor/tombstones /dev/socket/qmux_radio /linkerconfig
  chmod 0770 /data/vendor/wifi /data/vendor/wifi/sockets /data/vendor/wifi/hostapd /data/vendor/wifi/hostapd/ctrl /data/vendor/wifi/wpa /data/vendor/wifi/wpa/sockets 2>/dev/null || true
  chmod 2770 /dev/socket/qmux_radio 2>/dev/null || true
  log_cmd "android vendor runtime paths" ls -ld /system /vendor /odm /product /system_ext /apex/com.android.runtime /data/vendor/wifi /data/vendor/wifi/sockets /dev/socket/qmux_radio /vendor/etc/qmi_fw.conf || true
}

start_qrtr_services() {
  start_android_daemon qrtr-ns /vendor/bin/qrtr-ns -f || log "qrtr-ns startup skipped"
  if [ -x /system/bin/qmiproxy ]; then
    start_android_daemon qmiproxy /system/bin/qmiproxy || log "qmiproxy startup skipped"
  elif [ -x /vendor/bin/qmiproxy ]; then
    start_android_daemon qmiproxy /vendor/bin/qmiproxy || log "qmiproxy startup skipped"
  else
    log "qmiproxy not present in mounted Android partitions"
  fi
}

start_cnss_daemon() {
  start_android_daemon cnss-daemon /vendor/bin/cnss-daemon -n -l
}

mark_cnss_fs_ready() {
  for node in /sys/devices/platform/soc/*qcom,cnss-qca6390/fs_ready /sys/devices/platform/soc/*cnss*/fs_ready; do
    [ -w "$node" ] || continue
    echo 1 >"$node" 2>/dev/null || true
    log "cnss fs_ready set via $node"
    return 0
  done
  log "cnss fs_ready node not found"
  return 1
}

write_wifi_helpers() {
  mkdir -p /usr/local/sbin
  cat > /usr/local/sbin/alioth-wifi-on <<'WIFI_ON'
#!/bin/sh
set -u
LOG=/run/alioth-wifi-debug.log
log() {
  printf '[%s] alioth-wifi-on: %s\n' "$(cut -d' ' -f1 /proc/uptime 2>/dev/null || echo 0)" "$*" >>"$LOG"
  echo "[alioth-wifi-on] $*" >/dev/kmsg 2>/dev/null || true
}
snapshot() {
  {
    printf '\n===== alioth-wifi-on %s =====\n' "$1"
    date 2>/dev/null || true
    printf '\n--- links ---\n'
    ip -br link 2>/dev/null || true
    iw dev 2>/dev/null || true
    printf '\n--- mounts ---\n'
    mount 2>/dev/null | grep -E ' /system | /vendor | /odm | /product | /system_ext |firmware_mnt|persist|/apex' || true
    printf '\n--- processes ---\n'
    ps -ef 2>/dev/null | grep -E 'cnss|qrtr|qmi' | grep -v grep || true
    printf '\n--- daemon logs ---\n'
    tail -80 /run/qrtr-ns.log 2>/dev/null || true
    tail -120 /run/cnss-daemon.log 2>/dev/null || true
    printf '\n--- qcacld firmware config ---\n'
    ls -ld /vendor/firmware/wlan/qca_cld /lib/firmware/wlan/qca_cld 2>/dev/null || true
    ls -l /vendor/etc/wifi/qca6390/WCNSS_qcom_cfg.ini /vendor/firmware/wlan/qca_cld/qca6390/WCNSS_qcom_cfg.ini /lib/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini /lib/firmware/wlan/qca_cld/wlan_mac.bin 2>/dev/null || true
    ls -lL /lib/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini /lib/firmware/wlan/qca_cld/wlan_mac.bin 2>/dev/null || true
    printf '\n--- cnss dmesg ---\n'
    dmesg 2>/dev/null | grep -iE 'cnss|wlan|qca|wlfw|calibration|firmware|qmi|qrtr|timeout|recovery|ASSERT|failed' | tail -220 || true
  } >>"$LOG" 2>&1
}

if [ ! -e /dev/wlan ]; then
  log "/dev/wlan is missing"
  exit 1
fi
snapshot before-on
log "writing ON to /dev/wlan"
printf 'ON\n' >/dev/wlan
rc=$?
log "write ON rc=$rc"
snapshot after-on-write
[ "$rc" -eq 0 ] || exit "$rc"
ip link set wlan0 up 2>/dev/null || true
for i in $(seq 1 20); do
  ip link show wlan0 >/dev/null 2>&1 && break
  sleep 1
done
snapshot final
iw dev 2>/dev/null || ip -br link
WIFI_ON
  chmod 0755 /usr/local/sbin/alioth-wifi-on

  cat > /usr/local/sbin/alioth-wifi-scan <<'WIFI_SCAN'
#!/bin/sh
set -u
if ! ip link show wlan0 >/dev/null 2>&1; then
  /usr/local/sbin/alioth-wifi-on >/dev/null 2>&1 || true
fi
ip link set wlan0 up 2>/dev/null || true
iw dev wlan0 scan 2>/tmp/alioth-wifi-scan.err | awk '
  /^BSS / { bss = $2; sub(/\(.*/, "", bss); sig = ""; ssid = ""; next }
  /^[ \t]*signal:/ { sig = $2 " " $3; next }
  /^[ \t]*SSID:/ {
    ssid = $0
    sub(/^[ \t]*SSID: /, "", ssid)
    if (ssid != "")
      printf "%-18s %s\n", sig, ssid
  }
'
rc=$?
if [ "$rc" -ne 0 ]; then
  cat /tmp/alioth-wifi-scan.err >&2
fi
exit "$rc"
WIFI_SCAN
  chmod 0755 /usr/local/sbin/alioth-wifi-scan

  cat > /usr/local/sbin/alioth-wifi-connect <<'WIFI_CONNECT'
#!/bin/sh
set -eu

setup_resolv_conf() {
  resolv=/etc/resolv.conf
  if [ -L "$resolv" ]; then
    link="$(readlink "$resolv" 2>/dev/null || true)"
    case "$link" in
      /*) resolv="$link" ;;
      "") ;;
      *) resolv="/etc/$link" ;;
    esac
  fi
  mkdir -p "${resolv%/*}"
  router="$(ip route show default 2>/dev/null | awk '/ dev wlan0 / { print $3; found=1; exit } NR == 1 { first=$3 } END { if (!found && first != "") print first }')"
  {
    [ -n "$router" ] && printf 'nameserver %s\n' "$router"
    printf 'nameserver 223.5.5.5\n'
    printf 'nameserver 1.1.1.1\n'
  } >"$resolv"
}

write_net_status() {
  ssid="$1"
  state="$2"
  dhcp_rc="$3"
  ipv4="$(ip -4 -o addr show dev wlan0 2>/dev/null | awk 'NR == 1 { print $4 }')"
  gateway="$(ip route show default 2>/dev/null | awk '/ dev wlan0 / { print $3; found=1; exit } NR == 1 { first=$3 } END { if (!found && first != "") print first }')"
  dns="$(awk '/^nameserver[ \t]+/ { printf "%s%s", sep, $2; sep=" " }' /etc/resolv.conf 2>/dev/null || true)"
  {
    printf 'ssid=%s\n' "$ssid"
    printf 'wpa_state=%s\n' "$state"
    printf 'ipv4=%s\n' "$ipv4"
    printf 'gateway=%s\n' "$gateway"
    printf 'dns=%s\n' "$dns"
    printf 'dhcp_rc=%s\n' "$dhcp_rc"
    printf 'uptime=%s\n' "$(cut -d' ' -f1 /proc/uptime 2>/dev/null || echo 0)"
  } >/run/alioth-net-status
}

network_ready() {
  ip -4 -o addr show dev wlan0 2>/dev/null | grep -q ' inet ' &&
    ip route show default 2>/dev/null | grep -q ' dev wlan0 '
}

current_wifi_ssid() {
  iw dev wlan0 link 2>/dev/null | sed -n 's/^[[:space:]]*SSID: //p' | head -n 1
}

connected_to() {
  [ "$(current_wifi_ssid)" = "$1" ] && network_ready
}

connect_one() {
  ssid="$1"
  psk="$2"

  if ! ip link show wlan0 >/dev/null 2>&1; then
    /usr/local/sbin/alioth-wifi-on >/dev/null 2>&1 || true
  fi
  ip link set wlan0 up 2>/dev/null || true

  if connected_to "$ssid"; then
    setup_resolv_conf
    write_net_status "$ssid" "COMPLETED" 0
    ip -br addr show wlan0
    return 0
  fi

  mkdir -p /run/wpa_supplicant
  conf=/run/alioth-wpa-wlan0.conf
  if [ -n "$psk" ]; then
    {
      printf 'ctrl_interface=/run/wpa_supplicant\n'
      printf 'update_config=0\n'
      wpa_passphrase "$ssid" "$psk" | sed '/^[ \t]*#psk=/d'
    } >"$conf"
  else
    {
      printf 'ctrl_interface=/run/wpa_supplicant\n'
      printf 'update_config=0\n'
      printf 'network={\n'
      printf '\tssid="%s"\n' "$ssid"
      printf '\tkey_mgmt=NONE\n'
      printf '}\n'
    } >"$conf"
  fi
  chmod 0600 "$conf"

  wpa_cli -i wlan0 terminate >/dev/null 2>&1 || true
  sleep 1
  wpa_supplicant -B -i wlan0 -c "$conf" -D nl80211,wext -O /run/wpa_supplicant -f /run/wpa_supplicant-wlan0.log

  state=""
  for i in $(seq 1 25); do
    state="$(wpa_cli -i wlan0 status 2>/dev/null | sed -n 's/^wpa_state=//p' | head -n 1)"
    if [ "$state" != "COMPLETED" ] && [ "$(current_wifi_ssid)" = "$ssid" ]; then
      state="COMPLETED"
    fi
    [ "$state" = "COMPLETED" ] && break
    sleep 1
  done

  wpa_cli -i wlan0 status 2>/dev/null | tee /run/alioth-wpa-status.log
  if [ "$state" != "COMPLETED" ]; then
    write_net_status "$ssid" "${state:-FAILED}" 1
    return 1
  fi

  dhcp_rc=0
  dhcpcd -4 -q -w -t 25 wlan0 || dhcp_rc=$?
  setup_resolv_conf
  if network_ready; then
    dhcp_rc=0
  fi
  write_net_status "$ssid" "$state" "$dhcp_rc"
  ip -br addr show wlan0
  return "$dhcp_rc"
}

if [ "$#" -gt 0 ] || [ -n "${ALIOTH_WIFI_SSID:-}" ]; then
  connect_one "${1:-${ALIOTH_WIFI_SSID:-}}" "${2:-${ALIOTH_WIFI_PSK:-}}"
  exit $?
fi

if [ -f /etc/alioth-wifi-default ]; then
  while IFS= read -r ssid; do
    IFS= read -r psk || psk=""
    [ -n "$ssid" ] || continue
    if connected_to "$ssid"; then
      setup_resolv_conf
      write_net_status "$ssid" "COMPLETED" 0
      ip -br addr show wlan0
      exit 0
    fi
  done </etc/alioth-wifi-default

  rc=1
  while IFS= read -r ssid; do
    IFS= read -r psk || psk=""
    [ -n "$ssid" ] || continue
    connect_one "$ssid" "$psk" && exit 0
    rc=$?
  done </etc/alioth-wifi-default
  exit "$rc"
fi

echo "usage: alioth-wifi-connect <ssid> [password]" >&2
echo "or: ALIOTH_WIFI_SSID=<ssid> ALIOTH_WIFI_PSK=<password> alioth-wifi-connect" >&2
echo "or create /etc/alioth-wifi-default as repeated ssid/password line pairs" >&2
exit 2
WIFI_CONNECT
  chmod 0755 /usr/local/sbin/alioth-wifi-connect

  cat > /usr/local/sbin/alioth-wifi-status <<'WIFI_STATUS'
#!/bin/sh
LOG=/run/alioth-wifi-debug.log
{
  printf '\n===== alioth-wifi-status =====\n'
  date 2>/dev/null || true
  printf '\n--- links ---\n'
  ip -br link
  ip -br addr show wlan0 2>/dev/null || true
  iw dev 2>/dev/null || true
  printf '\n--- wpa ---\n'
  wpa_cli -i wlan0 status 2>/dev/null || true
  printf '\n--- mounts ---\n'
  mount | grep -E 'android_| /system | /vendor | /odm | /product | /system_ext |firmware_mnt|persist|/apex' || true
  printf '\n--- paths ---\n'
  ls -l /dev/wlan /dev/null /vendor/bin/cnss-daemon /vendor/bin/qrtr-ns /vendor/etc/qmi_fw.conf /system/bin/linker64 /vendor/firmware_mnt/image/qca6390/amss20.bin /vendor/etc/wifi/qca6390/WCNSS_qcom_cfg.ini /vendor/firmware/wlan/qca_cld/qca6390/WCNSS_qcom_cfg.ini /lib/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini /lib/firmware/wlan/qca_cld/wlan_mac.bin 2>/dev/null || true
  ls -lL /lib/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini /lib/firmware/wlan/qca_cld/wlan_mac.bin 2>/dev/null || true
  printf '\n--- processes ---\n'
  ps -ef | grep -E 'cnss|qrtr|qmi' | grep -v grep || true
  printf '\n--- daemon logs ---\n'
  tail -120 /run/qrtr-ns.log 2>/dev/null || true
  tail -160 /run/cnss-daemon.log 2>/dev/null || true
  printf '\n--- switchroot log ---\n'
  tail -160 /run/alioth-switchroot.log 2>/dev/null || true
  printf '\n--- cnss dmesg ---\n'
  dmesg | grep -iE 'alioth-switchroot|cnss|wlan|qca|wlfw|calibration|firmware|qmi|qrtr|timeout|recovery|ASSERT|failed' | tail -260
} | tee -a "$LOG"
exit 0
ip -br link
iw dev 2>/dev/null || true
ps -ef | grep -v grep | grep cnss-daemon || true
dmesg | grep -iE 'cnss|wlan|qca|wlfw|calibration|firmware' | tail -120
WIFI_STATUS
  chmod 0755 /usr/local/sbin/alioth-wifi-status
}

auto_start_wifi() {
  [ -f /etc/alioth-wifi-default ] || return 0
  (
    sleep 3
    /usr/local/sbin/alioth-wifi-connect >/run/alioth-wifi-connect.log 2>&1 || true
  ) &
  log "default Wi-Fi auto-connect queued"
}

setup_systemd_mounts() {
  mkdir -p /sys/fs/cgroup
  mount -t cgroup2 cgroup2 /sys/fs/cgroup 2>/dev/null || true
}

seed_dev_nodes
populate_block_nodes
ip link set lo up 2>/dev/null || ifconfig lo up 2>/dev/null || true
ip addr add 127.0.0.1/8 dev lo 2>/dev/null || true
ip link set usb0 up 2>/dev/null || ifconfig usb0 up 2>/dev/null || true
ip addr add 172.16.42.2/24 dev usb0 2>/dev/null || ifconfig usb0 172.16.42.2 netmask 255.255.255.0 up 2>/dev/null || true
"$helper/alioth-usb-dhcpd" usb0 172.16.42.2 172.16.42.1 >/run/alioth-usb-dhcpd.log 2>&1 &
MINITCPSH_SHELL="$helper/sh" "$helper/minitcpsh" 2323 "$helper/sh" >/run/minitcpsh.log 2>&1 &

setup_android_mounts
setup_qcom_wifi_firmware
setup_android_runtime_apex || log "Android runtime APEX startup skipped"
setup_android_vendor_runtime
snapshot_wifi_debug before-vendor-daemons
start_qrtr_services
start_cnss_daemon || log "cnss-daemon startup skipped"
snapshot_wifi_debug after-vendor-daemons
sleep 2
mark_cnss_fs_ready
snapshot_wifi_debug after-fs-ready
write_wifi_helpers
auto_start_wifi

if [ -x /bin/sshd ]; then
  mkdir -p /run/sshd
  /bin/sshd -D -e >/run/sshd.log 2>&1 &
fi

if [ -x "$helper/alioth-status-ui" ]; then
  populate_char_nodes
  "$helper/alioth-status-ui" >/run/alioth-status-ui.log 2>&1 &
fi

log "now running from Arch rootfs; mode=$boot_mode"

if [ "$boot_mode" = "switchroot-systemd" ] && [ -x /usr/lib/systemd/systemd ]; then
  setup_systemd_mounts
  exec /usr/lib/systemd/systemd --unit=multi-user.target --show-status=yes --log-target=console
fi

while true; do
  /bin/bash -l </dev/console >/dev/console 2>&1 || /bin/sh -l </dev/console >/dev/console 2>&1 || "$helper/sh" -l </dev/console >/dev/console 2>&1 || true
  sleep 1
done
EOF
  chmod 0755 "$helper/alioth-switch-init"

  log "switching root to /mnt/arch with mode=$mode"
  exec switch_root /mnt/arch /var/tmp/alioth-switchroot/alioth-switch-init "$mode"
}

write_ubuntu_systemd_units() {
  helper=/mnt/ubuntu/var/tmp/alioth-switchroot
  unitdir=/mnt/ubuntu/etc/systemd/system
  wants="$unitdir/multi-user.target.wants"
  mkdir -p "$unitdir" "$wants"

  if grep -q 'LABEL=cloudimg-rootfs' /mnt/ubuntu/etc/fstab 2>/dev/null; then
    cat > /mnt/ubuntu/etc/fstab <<'EOF'
# LELE OS: root is mounted by initramfs switch_root from userdata subdir.
# Keep fstab empty for now; systemd-remount-fs must not look for cloudimg-rootfs.
EOF
  fi

  if [ -f /mnt/ubuntu/etc/group ]; then
    if grep -q '^inet:' /mnt/ubuntu/etc/group 2>/dev/null; then
      awk -F: 'BEGIN { OFS = FS } $1 == "inet" { $3 = "3003"; if ($4 !~ /(^|,)root(,|$)/) $4 = ($4 ? $4 ",root" : "root"); if ($4 !~ /(^|,)_apt(,|$)/) $4 = ($4 ? $4 ",_apt" : "_apt") } { print }' /mnt/ubuntu/etc/group >/tmp/ubuntu-group.alioth && cp /tmp/ubuntu-group.alioth /mnt/ubuntu/etc/group
    else
      echo 'inet:x:3003:root,_apt' >> /mnt/ubuntu/etc/group
    fi
  fi
  if [ -f /mnt/ubuntu/etc/passwd ] && grep -q '^_apt:' /mnt/ubuntu/etc/passwd 2>/dev/null; then
    awk -F: 'BEGIN { OFS = FS } $1 == "_apt" { $4 = "3003" } { print }' /mnt/ubuntu/etc/passwd >/tmp/ubuntu-passwd.alioth && cp /tmp/ubuntu-passwd.alioth /mnt/ubuntu/etc/passwd
  fi
  mkdir -p /mnt/ubuntu/var/log/journal /mnt/ubuntu/etc/systemd/journald.conf.d
  chown root:systemd-journal /mnt/ubuntu/var/log/journal 2>/dev/null || true
  chmod 2755 /mnt/ubuntu/var/log/journal 2>/dev/null || true
  cat > /mnt/ubuntu/etc/systemd/journald.conf.d/99-lele-persistent.conf <<'EOF'
[Journal]
Storage=persistent
SystemMaxUse=128M
RuntimeMaxUse=32M
MaxRetentionSec=14day
EOF

  for masked_unit in \
    systemd-networkd-wait-online.service \
    multipathd.service \
    multipathd.socket \
    systemd-rfkill.socket; do
    ln -sf /dev/null "$unitdir/$masked_unit"
  done

  cat > "$helper/lele-usb-net-up" <<'EOF'
#!/var/tmp/alioth-switchroot/sh
helper=/var/tmp/alioth-switchroot
export PATH=$helper:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
ip link set lo up 2>/dev/null || ifconfig lo up 2>/dev/null || true
ip addr add 127.0.0.1/8 dev lo 2>/dev/null || true
for i in $(seq 1 30); do
  ip link set usb0 up 2>/dev/null && break
  ifconfig usb0 up 2>/dev/null && break
  sleep 1
done
ip addr add 172.16.42.2/24 dev usb0 2>/dev/null || ifconfig usb0 172.16.42.2 netmask 255.255.255.0 up 2>/dev/null || true
ip route replace default via 172.16.42.1 dev usb0 2>/dev/null || true
if command -v resolvectl >/dev/null 2>&1; then
  for i in $(seq 1 20); do
    resolvectl dns usb0 1.1.1.1 8.8.8.8 2>/dev/null &&
      resolvectl default-route usb0 yes 2>/dev/null &&
      resolvectl domain usb0 '~.' 2>/dev/null &&
      break
    sleep 1
  done
fi
http_date_bin=
if [ -x /usr/bin/date ]; then
  http_date_bin=/usr/bin/date
elif command -v date >/dev/null 2>&1; then
  http_date_bin="$(command -v date)"
fi
if command -v curl >/dev/null 2>&1 && [ -n "$http_date_bin" ]; then
  for url in http://ports.ubuntu.com/ubuntu-ports/ http://archive.ubuntu.com/ubuntu/; do
    http_date="$(curl -fsI --max-time 15 "$url" 2>/dev/null | tr -d '\r' | sed -n 's/^[Dd][Aa][Tt][Ee]:[[:space:]]*//p' | head -n 1)"
    [ -n "$http_date" ] || continue
    if "$http_date_bin" -u -s "$http_date" >/run/lele-http-time-sync.log 2>&1; then
      echo "set from $url: $http_date" >>/run/lele-http-time-sync.log 2>&1 || true
      break
    fi
  done
fi
if command -v systemctl >/dev/null 2>&1; then
  systemctl try-restart --no-block systemd-timesyncd.service >/dev/null 2>&1 || true
fi
EOF
  chmod 0755 "$helper/lele-usb-net-up"

  cat > "$helper/lele-drm-nodes" <<'EOF'
#!/var/tmp/alioth-switchroot/sh
mkdir -p /dev/dri /dev/input 2>/dev/null || true
for name in card0 renderD128; do
  devfile="/sys/class/drm/$name/dev"
  [ -r "$devfile" ] || continue
  mm="$(cat "$devfile" 2>/dev/null || true)"
  maj="${mm%:*}"
  min="${mm#*:}"
  [ -n "$maj" ] && [ -n "$min" ] || continue
  rm -f "/dev/dri/$name"
  mknod "/dev/dri/$name" c "$maj" "$min" 2>/dev/null || true
  chown root:video "/dev/dri/$name" 2>/dev/null || true
  chmod 0660 "/dev/dri/$name" 2>/dev/null || true
done
if [ -r /sys/class/kgsl/kgsl-3d0/dev ]; then
  mm="$(cat /sys/class/kgsl/kgsl-3d0/dev 2>/dev/null || true)"
  maj="${mm%:*}"
  min="${mm#*:}"
  if [ -n "$maj" ] && [ -n "$min" ]; then
    rm -f /dev/kgsl-3d0
    mknod /dev/kgsl-3d0 c "$maj" "$min" 2>/dev/null || true
    chown root:video /dev/kgsl-3d0 2>/dev/null || true
    chmod 0660 /dev/kgsl-3d0 2>/dev/null || true
  fi
fi
for sysdev in /sys/class/input/event*; do
  [ -e "$sysdev" ] || continue
  name="${sysdev##*/}"
  mm="$(cat "$sysdev/dev" 2>/dev/null || true)"
  maj="${mm%:*}"
  min="${mm#*:}"
  [ -n "$maj" ] && [ -n "$min" ] || continue
  rm -f "/dev/input/$name"
  mknod "/dev/input/$name" c "$maj" "$min" 2>/dev/null || true
  chown root:input "/dev/input/$name" 2>/dev/null || true
  chmod 0660 "/dev/input/$name" 2>/dev/null || true
done
exit 0
EOF
  chmod 0755 "$helper/lele-drm-nodes"

  cat > "$unitdir/lele-usb-net.service" <<'EOF'
[Unit]
Description=LELE USB NCM network setup
DefaultDependencies=no
After=systemd-udevd.service systemd-resolved.service
Before=network-pre.target ssh.service
Wants=network-pre.target systemd-resolved.service

[Service]
Type=oneshot
RemainAfterExit=yes
ExecStart=/var/tmp/alioth-switchroot/lele-usb-net-up

[Install]
WantedBy=multi-user.target
EOF

  cat > "$unitdir/lele-usb-dhcpd.service" <<'EOF'
[Unit]
Description=LELE USB NCM host DHCP helper
After=lele-usb-net.service
Requires=lele-usb-net.service

[Service]
ExecStart=/var/tmp/alioth-switchroot/alioth-usb-dhcpd usb0 172.16.42.2 172.16.42.1
Restart=always
RestartSec=1

[Install]
WantedBy=multi-user.target
EOF

  cat > "$unitdir/lele-minitcpsh.service" <<'EOF'
[Unit]
Description=LELE fallback TCP shell
After=lele-usb-net.service
Requires=lele-usb-net.service

[Service]
Environment=MINITCPSH_SHELL=/bin/sh
ExecStart=/var/tmp/alioth-switchroot/minitcpsh 2323 /bin/sh
Restart=always
RestartSec=1

[Install]
WantedBy=multi-user.target
EOF
  # The systemd wrapper starts the emergency TCP shell before execing systemd.
  # A second systemd-managed minitcpsh races for the same port and degrades boot.
  rm -f "$unitdir/lele-minitcpsh.service" "$wants/lele-minitcpsh.service"
  ln -sf /dev/null "$unitdir/lele-minitcpsh.service"

  cat > "$unitdir/lele-status-ui.service" <<'EOF'
[Unit]
Description=LELE DRM/KMS status UI
After=systemd-udevd.service

[Service]
ExecStartPre=/var/tmp/alioth-switchroot/lele-drm-nodes
ExecStart=/var/tmp/alioth-switchroot/alioth-status-ui
Restart=always
RestartSec=1
StandardOutput=append:/run/alioth-status-ui.log
StandardError=append:/run/alioth-status-ui.log

[Install]
WantedBy=multi-user.target
EOF

  ln -sf ../lele-usb-net.service "$wants/lele-usb-net.service"
  ln -sf ../lele-usb-dhcpd.service "$wants/lele-usb-dhcpd.service"
  rm -f "$wants/lele-minitcpsh.service"
  if [ -x "$helper/alioth-status-ui" ]; then
    ln -sf ../lele-status-ui.service "$wants/lele-status-ui.service"
  fi
  if [ -f /mnt/ubuntu/lib/systemd/system/ssh.service ]; then
    ln -sf /lib/systemd/system/ssh.service "$wants/ssh.service"
  elif [ -f /mnt/ubuntu/usr/lib/systemd/system/ssh.service ]; then
    ln -sf /usr/lib/systemd/system/ssh.service "$wants/ssh.service"
  fi

  mkdir -p /mnt/ubuntu/etc
  [ -s /mnt/ubuntu/etc/machine-id ] || : > /mnt/ubuntu/etc/machine-id
  echo alioth-ubuntu > /mnt/ubuntu/etc/hostname
}

switch_to_ubuntu_root() {
  mount_ubuntu_root || return 1
  boot_mode="${1:-switchroot-ubuntu}"
  [ -x /mnt/ubuntu/bin/sh ] || {
    log "cannot switch_root: /mnt/ubuntu lacks /bin/sh"
    return 1
  }

  helper=/mnt/ubuntu/var/tmp/alioth-switchroot
  mkdir -p "$helper" /mnt/ubuntu/dev /mnt/ubuntu/proc /mnt/ubuntu/sys /mnt/ubuntu/run
  cp /bin/minitcpsh "$helper/minitcpsh"
  chmod 0755 "$helper/minitcpsh"
  cp /bin/alioth-usb-dhcpd "$helper/alioth-usb-dhcpd"
  chmod 0755 "$helper/alioth-usb-dhcpd"
  cp /bin/busybox "$helper/busybox"
  chmod 0755 "$helper/busybox"
  for app in sh mount mkdir mknod rm ip ifconfig sleep true false cat grep awk sed cut date ps tail head uname chmod chown ln kill seq; do
    ln -sf busybox "$helper/$app"
  done
  cp /bin/alioth-reboot "$helper/alioth-reboot"
  chmod 0755 "$helper/alioth-reboot"
  cat > "$helper/reboot-bootloader" <<'EOF'
#!/var/tmp/alioth-switchroot/sh
exec /var/tmp/alioth-switchroot/alioth-reboot bootloader
EOF
  chmod 0755 "$helper/reboot-bootloader"
  if [ -x /bin/alioth-status-ui ]; then
    cp /bin/alioth-status-ui "$helper/alioth-status-ui"
    chmod 0755 "$helper/alioth-status-ui"
  fi
  if [ -x /bin/alioth-status-ui-cpu ]; then
    cp /bin/alioth-status-ui-cpu "$helper/alioth-status-ui-cpu"
    chmod 0755 "$helper/alioth-status-ui-cpu"
  fi
  mkdir -p /mnt/ubuntu/etc/ssh/sshd_config.d
  cat > /mnt/ubuntu/etc/ssh/sshd_config.d/99-lele-root.conf <<'EOF'
PermitRootLogin prohibit-password
PasswordAuthentication no
PubkeyAuthentication yes
UsePAM no
EOF
  chmod 0644 /mnt/ubuntu/etc/ssh/sshd_config.d/99-lele-root.conf 2>/dev/null || true
  if [ -f /etc/alioth-build-epoch ]; then
    cp /etc/alioth-build-epoch /mnt/ubuntu/etc/alioth-build-epoch 2>/dev/null || true
  fi
  if [ -f /etc/alioth-systemd-wrapper-mode ]; then
    cp /etc/alioth-systemd-wrapper-mode /mnt/ubuntu/etc/alioth-systemd-wrapper-mode 2>/dev/null || true
  fi
  if [ -s /etc/alioth-ssh-authorized-key ]; then
    mkdir -p /mnt/ubuntu/root/.ssh
    cp /etc/alioth-ssh-authorized-key /mnt/ubuntu/root/.ssh/authorized_keys
    chmod 0700 /mnt/ubuntu/root/.ssh
    chmod 0600 /mnt/ubuntu/root/.ssh/authorized_keys
  fi
  cat > "$helper/alioth-systemd-init" <<'EOF'
#!/var/tmp/alioth-switchroot/sh
helper=/var/tmp/alioth-switchroot
export PATH=$helper:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

log() {
  mkdir -p /run 2>/dev/null || true
  printf '[%s] %s\n' "$(cut -d' ' -f1 /proc/uptime 2>/dev/null || echo 0)" "$*" >>/run/alioth-systemd-init.log 2>/dev/null || true
  echo "[alioth-systemd] $*" >/dev/kmsg 2>/dev/null || true
  echo "[alioth-systemd] $*" >/dev/console 2>/dev/null || true
}

start_emergency_usb_shell() {
  mkdir -p /run 2>/dev/null || true
  ip link set usb0 up 2>/dev/null || true
  ip addr add 172.16.42.2/24 dev usb0 2>/dev/null || true
  alioth-usb-dhcpd usb0 172.16.42.2 172.16.42.1 >/run/alioth-usb-dhcpd.log 2>&1 &
  echo "$!" >/run/alioth-early-usb-dhcpd.pid 2>/dev/null || true
  minitcpsh 2323 "$helper/sh" >/run/alioth-minitcpsh.log 2>&1 &
  log "emergency USB shell requested on 172.16.42.2:2323"
}

stop_early_usb_dhcpd() {
  pid="$(cat /run/alioth-early-usb-dhcpd.pid 2>/dev/null || true)"
  case "$pid" in
    ''|*[!0-9]*)
      return 0
      ;;
  esac
  if kill -0 "$pid" 2>/dev/null; then
    log "stopping early USB DHCP helper before systemd service takeover: pid=$pid"
    kill "$pid" 2>/dev/null || true
    sleep 1
  fi
  rm -f /run/alioth-early-usb-dhcpd.pid 2>/dev/null || true
}

systemd_init="${1:-/lib/systemd/systemd}"
if [ ! -x "$systemd_init" ] && [ -x /usr/lib/systemd/systemd ]; then
  systemd_init=/usr/lib/systemd/systemd
fi
wrapper_mode="$(cat /etc/alioth-systemd-wrapper-mode 2>/dev/null || echo exec)"

log "wrapper PID1 starting; mode=$wrapper_mode requested systemd=$systemd_init"
if [ -f /etc/alioth-build-epoch ]; then
  now="$(date +%s 2>/dev/null || echo 0)"
  build_epoch="$(cat /etc/alioth-build-epoch 2>/dev/null || echo 0)"
  if [ "$build_epoch" -gt 1700000000 ] 2>/dev/null && [ "$now" -lt "$build_epoch" ] 2>/dev/null; then
    if date -u -s "@$build_epoch" >/run/alioth-build-epoch-date.log 2>&1; then
      log "set clock to build epoch: $build_epoch"
    else
      log "failed to set clock to build epoch: $build_epoch"
    fi
  else
    log "clock already at or after build epoch: now=$now build_epoch=$build_epoch"
  fi
fi
start_emergency_usb_shell
if [ "$wrapper_mode" = "hold" ]; then
  log "holding before systemd exec; touch /run/alioth-start-systemd to continue"
  while [ ! -e /run/alioth-start-systemd ]; do
    sleep 1
  done
  log "systemd trigger observed"
fi

if [ ! -x "$systemd_init" ]; then
  log "systemd binary is not executable inside Ubuntu root"
else
  log "execing Ubuntu systemd PID1: $systemd_init"
  stop_early_usb_dhcpd
  exec "$systemd_init" --unit=multi-user.target --show-status=yes --log-target=console
  rc=$?
  log "Ubuntu systemd exec failed rc=$rc"
fi

while true; do
  sleep 60
done
EOF
  chmod 0755 "$helper/alioth-systemd-init"

  if [ "$boot_mode" = "switchroot-ubuntu-systemd" ]; then
    systemd_init=
    if [ -x /mnt/ubuntu/lib/systemd/systemd ]; then
      systemd_init=/lib/systemd/systemd
    elif [ -x /mnt/ubuntu/usr/lib/systemd/systemd ]; then
      systemd_init=/usr/lib/systemd/systemd
    fi
    [ -n "$systemd_init" ] || {
      log "cannot start Ubuntu systemd: systemd binary not found"
      return 1
    }
    write_ubuntu_systemd_units
    if [ ! -e /mnt/ubuntu/etc/ssh/ssh_host_ed25519_key ]; then
      if [ -x /mnt/ubuntu/usr/bin/ssh-keygen ] || [ -x /mnt/ubuntu/bin/ssh-keygen ]; then
        /bin/busybox chroot /mnt/ubuntu ssh-keygen -A >/tmp/ubuntu-ssh-keygen.log 2>&1 || true
        while IFS= read -r line; do
          log "Ubuntu ssh-keygen: $line"
        done </tmp/ubuntu-ssh-keygen.log
      fi
    fi
    /bin/busybox chroot /mnt/ubuntu "$systemd_init" --version >/tmp/ubuntu-systemd-version.log 2>&1
    rc=$?
    log "Ubuntu systemd preflight --version rc=$rc"
    while IFS= read -r line; do
      log "Ubuntu systemd preflight: $line"
    done </tmp/ubuntu-systemd-version.log
    log "switching root to /mnt/ubuntu with static systemd wrapper PID1: $systemd_init"
    exec switch_root /mnt/ubuntu /var/tmp/alioth-switchroot/alioth-systemd-init
    rc=$?
    log "Ubuntu systemd exec switch_root failed before replacement rc=$rc"
    return "$rc"
  fi

  cat > "$helper/alioth-ubuntu-init" <<'EOF'
#!/var/tmp/alioth-switchroot/sh
helper=/var/tmp/alioth-switchroot
export PATH=$helper:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

log() {
  mkdir -p /run 2>/dev/null || true
  printf '[%s] %s\n' "$(cut -d' ' -f1 /proc/uptime 2>/dev/null || echo 0)" "$*" >>/run/alioth-ubuntu-init.log 2>/dev/null || true
  echo "[alioth-ubuntu] $*" >/dev/kmsg 2>/dev/null || true
  echo "[alioth-ubuntu] $*" >/dev/console 2>/dev/null || true
}

seed_dev_nodes() {
  mkdir -p /dev /dev/pts /dev/dri /dev/input /dev/graphics
  if [ ! -c /dev/null ]; then
    rm -f /dev/null
    mknod -m 0666 /dev/null c 1 3 || true
  fi
  for spec in \
    "console 0600 5 1" \
    "kmsg 0600 1 11" \
    "null 0666 1 3" \
    "zero 0666 1 5" \
    "full 0666 1 7" \
    "random 0666 1 8" \
    "urandom 0666 1 9" \
    "tty 0666 5 0" \
    "ptmx 0666 5 2"; do
    set -- $spec
    name="$1"; mode="$2"; maj="$3"; min="$4"
    [ -c "/dev/$name" ] || rm -f "/dev/$name" 2>/dev/null || true
    [ -c "/dev/$name" ] || mknod -m "$mode" "/dev/$name" c "$maj" "$min" 2>/dev/null || true
  done
  rm -rf /dev/fd /dev/stdin /dev/stdout /dev/stderr 2>/dev/null || true
  ln -s /proc/self/fd /dev/fd 2>/dev/null || true
  ln -s /proc/self/fd/0 /dev/stdin 2>/dev/null || true
  ln -s /proc/self/fd/1 /dev/stdout 2>/dev/null || true
  ln -s /proc/self/fd/2 /dev/stderr 2>/dev/null || true
}

create_char_node() {
  target="$1"
  majmin="$2"
  maj="${majmin%:*}"
  min="${majmin#*:}"
  case "$maj:$min" in
    *:*) ;;
    *) return 0 ;;
  esac
  dir="${target%/*}"
  [ "$dir" = "$target" ] || mkdir -p "$dir"
  [ -c "$target" ] || mknod -m 0600 "$target" c "$maj" "$min" 2>/dev/null || true
}

populate_char_nodes() {
  mkdir -p /dev/dri /dev/input /dev/graphics
  for sysdev in /sys/class/drm/*; do
    [ -e "$sysdev/dev" ] || continue
    name="${sysdev##*/}"
    create_char_node "/dev/dri/$name" "$(cat "$sysdev/dev" 2>/dev/null || true)"
  done
  for sysdev in /sys/class/input/event*; do
    [ -e "$sysdev/dev" ] || continue
    name="${sysdev##*/}"
    create_char_node "/dev/input/$name" "$(cat "$sysdev/dev" 2>/dev/null || true)"
  done
  for sysdev in /sys/class/graphics/fb*; do
    [ -e "$sysdev/dev" ] || continue
    name="${sysdev##*/}"
    majmin="$(cat "$sysdev/dev" 2>/dev/null || true)"
    create_char_node "/dev/graphics/$name" "$majmin"
    create_char_node "/dev/$name" "$majmin"
  done
}

seed_dev_nodes
mount -t proc proc /proc 2>/dev/null || true
mount -t sysfs sysfs /sys 2>/dev/null || true
mkdir -p /sys/fs/cgroup
grep -q " /sys/fs/cgroup " /proc/mounts 2>/dev/null || mount -t cgroup2 -o nsdelegate cgroup2 /sys/fs/cgroup 2>/dev/null || mount -t cgroup2 cgroup2 /sys/fs/cgroup 2>/dev/null || true
mount -t tmpfs tmpfs /run 2>/dev/null || true
mkdir -p /dev/pts
mount -o remount,mode=0620,ptmxmode=0666,gid=5 /dev/pts 2>/dev/null || mount -t devpts -o mode=0620,ptmxmode=0666,gid=5 devpts /dev/pts 2>/dev/null || true
seed_dev_nodes
populate_char_nodes

if [ -f /etc/alioth-build-epoch ] && [ -x /bin/date ]; then
  now="$(/bin/date +%s 2>/dev/null || echo 0)"
  build_epoch="$(cat /etc/alioth-build-epoch 2>/dev/null || echo 0)"
  if [ "$now" -lt 1700000000 ] 2>/dev/null && [ "$build_epoch" -gt 1700000000 ] 2>/dev/null; then
    /bin/date -u -s "@$build_epoch" >>/run/alioth-ubuntu-init.log 2>&1 || true
  fi
fi

ip link set lo up 2>/dev/null || ifconfig lo up 2>/dev/null || true
ip addr add 127.0.0.1/8 dev lo 2>/dev/null || true
for i in $(seq 1 20); do
  ip link set usb0 up 2>/dev/null && break
  ifconfig usb0 up 2>/dev/null && break
  sleep 1
done
ip addr add 172.16.42.2/24 dev usb0 2>/dev/null || ifconfig usb0 172.16.42.2 netmask 255.255.255.0 up 2>/dev/null || true
ip route replace default via 172.16.42.1 dev usb0 2>/dev/null || true
"$helper/alioth-usb-dhcpd" usb0 172.16.42.2 172.16.42.1 >/run/alioth-usb-dhcpd.log 2>&1 &
MINITCPSH_SHELL=/bin/sh "$helper/minitcpsh" 2323 /bin/sh >/run/minitcpsh.log 2>&1 &

if [ -x "$helper/alioth-status-ui" ]; then
  (
    for i in $(seq 1 30); do
      populate_char_nodes
      [ -c /dev/dri/card0 ] && break
      sleep 1
    done
    exec "$helper/alioth-status-ui"
  ) >/run/alioth-status-ui.log 2>&1 &
fi

if [ -x /usr/bin/ssh-keygen ]; then
  /usr/bin/ssh-keygen -A >>/run/ssh-keygen.log 2>&1 || true
fi
if [ -x /usr/sbin/sshd ]; then
  mkdir -p /run/sshd
  /usr/sbin/sshd -D -e >/run/sshd.log 2>&1 &
elif [ -x /usr/bin/sshd ]; then
  mkdir -p /run/sshd
  /usr/bin/sshd -D -e >/run/sshd.log 2>&1 &
else
  log "sshd not found in Ubuntu rootfs"
fi

log "now running from Ubuntu rootfs"
{
  printf 'cmdline: '
  cat /proc/cmdline 2>/dev/null || true
  printf '\nos-release:\n'
  cat /etc/os-release 2>/dev/null || true
  printf '\nrootfs marker:\n'
  cat /etc/lele-rootfs-id 2>/dev/null || true
  printf '\nmounts:\n'
  mount 2>/dev/null || true
} >>/run/alioth-ubuntu-init.log 2>&1

while true; do
  sleep 3600
done
EOF
  chmod 0755 "$helper/alioth-ubuntu-init"

  log "switching root to /mnt/ubuntu with minimal Ubuntu PID1"
  exec switch_root /mnt/ubuntu /var/tmp/alioth-switchroot/alioth-ubuntu-init
}

start_local_shells() {
  while true; do
    setsid sh -l </dev/console >/dev/console 2>&1
    sleep 1
  done &
}

main() {
  seed_dev_nodes
  mount_once proc proc /proc || true
  mount_once sysfs sysfs /sys || true
  mount_once tmpfs tmpfs /run mode=0755 || true
  mount_once tmpfs tmpfs /tmp mode=1777 || true
  mount_once devpts devpts /dev/pts || true
  seed_dev_nodes
  setup_loopback
  populate_char_nodes

  log "pid1 started: $(uname -a)"
  log "cmdline: $(cat /proc/cmdline 2>/dev/null)"

  mode="$(cat /etc/alioth-init-mode 2>/dev/null || echo shell)"
  log "init mode: $mode"
  case "$mode" in
    switchroot-shell|switchroot-systemd|switchroot-ubuntu|switchroot-ubuntu-systemd)
      ;;
    *)
      start_status_ui
      ;;
  esac
  populate_block_nodes
  case "$mode" in
    switchroot-shell|switchroot-systemd|switchroot-ubuntu|switchroot-ubuntu-systemd)
      setup_usb_ncm no || true
      ;;
    *)
      setup_usb_ncm yes || true
      ;;
  esac
  write_enter_arch
  write_enter_ubuntu
  case "$mode" in
    switchroot-shell|switchroot-systemd)
      mount_arch_root || true
      switch_to_arch_root "$mode" || {
        log "switch_root failed; falling back to initramfs shell"
        start_status_ui
        start_initramfs_tcp_shell
      }
      ;;
    switchroot-ubuntu|switchroot-ubuntu-systemd)
      switch_to_ubuntu_root "$mode" || {
        log "Ubuntu switch_root failed; trying Arch fallback"
        mount_arch_root || true
        switch_to_arch_root switchroot-shell || {
          log "Arch fallback failed; staying in initramfs shell"
          start_status_ui
          start_initramfs_tcp_shell
        }
      }
      ;;
    *)
      mount_arch_root || true
      start_initramfs_tcp_shell
      ;;
  esac
  start_local_shells

  while true; do
    populate_char_nodes
    populate_block_nodes
    sleep 10
  done
}

main "$@"
INIT
  chmod 0755 "$path"
}

build_initramfs() {
  local root="$1"
  local ramdisk="$2"

  python3 - "$root" "$ramdisk" <<'PY'
from pathlib import Path
import gzip
import os
import stat
import struct
import sys
import time

root = Path(sys.argv[1])
out = Path(sys.argv[2])
now = int(time.time())
ino = 300000

entries = []

def add(path: str, mode: int, data: bytes = b"", target: bytes = b"", rdev=(0, 0)):
    global ino
    ino += 1
    entries.append((path, ino, mode, data, target, rdev))

def rel(p: Path) -> str:
    s = p.relative_to(root).as_posix()
    return "." if s == "." else s

for p in sorted(root.rglob("*")):
    st = os.lstat(p)
    path = rel(p)
    if stat.S_ISDIR(st.st_mode):
        add(path, stat.S_IFDIR | (st.st_mode & 0o777))
    elif stat.S_ISLNK(st.st_mode):
        add(path, stat.S_IFLNK | 0o777, target=os.readlink(p).encode())
    elif stat.S_ISREG(st.st_mode):
        add(path, stat.S_IFREG | (st.st_mode & 0o777), data=p.read_bytes())

for d in ["dev", "dev/block", "dev/block/by-name", "dev/pts", "proc", "sys", "run", "tmp", "mnt", "mnt/arch", "mnt/data", "mnt/ubuntu"]:
    if not any(e[0] == d for e in entries):
        add(d, stat.S_IFDIR | 0o755)

for name, perm, maj, minor in [
    ("dev/console", 0o600, 5, 1),
    ("dev/null", 0o666, 1, 3),
    ("dev/zero", 0o666, 1, 5),
    ("dev/full", 0o666, 1, 7),
    ("dev/random", 0o666, 1, 8),
    ("dev/urandom", 0o666, 1, 9),
    ("dev/kmsg", 0o600, 1, 11),
    ("dev/tty", 0o666, 5, 0),
    ("dev/ptmx", 0o666, 5, 2),
]:
    add(name, stat.S_IFCHR | perm, rdev=(maj, minor))

def pad4(buf: bytearray):
    while len(buf) % 4:
        buf.append(0)

def write_entry(buf: bytearray, path: str, ino: int, mode: int, data: bytes, target: bytes, rdev):
    body = target if stat.S_ISLNK(mode) else data
    namesize = len(path.encode()) + 1
    fields = [
        ino, mode, 0, 0, 1, now, len(body),
        0, 0, rdev[0], rdev[1], namesize, 0,
    ]
    buf.extend(b"070701")
    for f in fields:
        buf.extend(f"{f:08x}".encode())
    buf.extend(path.encode() + b"\0")
    pad4(buf)
    buf.extend(body)
    pad4(buf)

buf = bytearray()
for e in sorted(entries, key=lambda x: x[0]):
    write_entry(buf, *e)
write_entry(buf, "TRAILER!!!", ino + 1, 0, b"", b"", (0, 0))

out.parent.mkdir(parents=True, exist_ok=True)
with gzip.GzipFile(filename="", mode="wb", fileobj=out.open("wb"), mtime=0) as gz:
    gz.write(buf)
print(f"wrote {out} entries={len(entries)} bytes={out.stat().st_size}")
PY
}

build_minitcpsh() {
  local out="$1"
  require_cmd docker
  require_file "$MINITCPSH_SRC"
  log "building static aarch64 minitcpsh"
  docker run --rm \
    -v "$REPO_DIR:/work/repo:ro" \
    -v "$OUT_DIR:/work/out" \
    "$BUILDER_IMAGE" \
    bash -lc "aarch64-linux-gnu-gcc -Os -static -s -Wall -Wextra -o /work/out/minitcpsh-aarch64 /work/repo/src/minitcpsh/minitcpsh.c"
  cp "$OUT_DIR/minitcpsh-aarch64" "$out"
  chmod 0755 "$out"
}

build_usb_dhcpd() {
  local out="$1"
  if [ -n "$USB_DHCPD_BIN" ]; then
    require_file "$USB_DHCPD_BIN"
    cp "$USB_DHCPD_BIN" "$out"
    chmod 0755 "$out"
    return 0
  fi

  require_cmd docker
  require_file "$USB_DHCPD_SRC"
  log "building static aarch64 USB DHCP server"
  docker run --rm \
    -v "$USB_DHCPD_SRC:/work/alioth_usb_dhcpd.c:ro" \
    -v "$OUT_DIR:/work/out" \
    "$BUILDER_IMAGE" \
    bash -lc "aarch64-linux-gnu-gcc -Os -static -s -std=gnu11 -Wall -Wextra -o /work/out/alioth-usb-dhcpd /work/alioth_usb_dhcpd.c"
  cp "$OUT_DIR/alioth-usb-dhcpd" "$out"
  chmod 0755 "$out"
}

build_reboot_tool() {
  local out="$1"
  if [ -n "$REBOOT_TOOL_BIN" ]; then
    require_file "$REBOOT_TOOL_BIN"
    cp "$REBOOT_TOOL_BIN" "$out"
    chmod 0755 "$out"
    return 0
  fi

  require_cmd docker
  require_file "$REBOOT_TOOL_SRC"
  log "building static aarch64 reboot helper"
  docker run --rm \
    -v "$REBOOT_TOOL_SRC:/work/alioth_reboot.c:ro" \
    -v "$OUT_DIR:/work/out" \
    "$BUILDER_IMAGE" \
    bash -lc "aarch64-linux-gnu-gcc -Os -static -s -std=gnu11 -Wall -Wextra -o /work/out/alioth-reboot /work/alioth_reboot.c"
  cp "$OUT_DIR/alioth-reboot" "$out"
  chmod 0755 "$out"
}

build_status_ui() {
  local out="$1"
  if [ -n "$STATUS_UI_BIN" ]; then
    require_file "$STATUS_UI_BIN"
    cp "$STATUS_UI_BIN" "$out"
    chmod 0755 "$out"
    return 0
  fi

  require_cmd docker
  require_file "$STATUS_UI_SRC"
  log "building static aarch64 C status UI"
  docker run --rm \
    -v "$STATUS_UI_SRC:/work/alioth_status_ui.c:ro" \
    -v "$OUT_DIR:/work/out" \
    "$BUILDER_IMAGE" \
    bash -lc "aarch64-linux-gnu-gcc -Os -static -s -std=gnu11 -Wall -Wextra -Wno-format-truncation -o /work/out/alioth-status-ui /work/alioth_status_ui.c"
  cp "$OUT_DIR/alioth-status-ui" "$out"
  chmod 0755 "$out"
}

pack_boot_image() {
  local base="$1" kernel="$2" ramdisk="$3" out="$4"
  python3 - "$base" "$kernel" "$ramdisk" "$out" <<'PY'
from pathlib import Path
import struct
import sys

base_image, kernel_path, ramdisk_path, out_path = map(Path, sys.argv[1:])
base = base_image.read_bytes()
kernel = kernel_path.read_bytes()
ramdisk = ramdisk_path.read_bytes()

if base[:8] != b"ANDROID!":
    raise SystemExit(f"{base_image} is not an Android boot image")

_old_kernel_size, _old_ramdisk_size, os_version, header_size = struct.unpack_from("<4I", base, 8)
reserved = struct.unpack_from("<4I", base, 24)
header_version = struct.unpack_from("<I", base, 40)[0]
cmdline = base[44:44 + 1536]
if header_version != 3 or header_size != 1580:
    raise SystemExit(f"unsupported boot header: version={header_version} size={header_size}")

header = bytearray()
header += b"ANDROID!"
header += struct.pack("<4I", len(kernel), len(ramdisk), os_version, header_size)
header += struct.pack("<4I", *reserved)
header += struct.pack("<I", header_version)
header += cmdline
assert len(header) == header_size

page = 4096
def pad(blob):
    return blob + b"\0" * ((page - len(blob) % page) % page)

out = pad(bytes(header)) + pad(kernel) + pad(ramdisk)
out_path.write_bytes(out)
print(f"kernel_size={len(kernel)} ramdisk_size={len(ramdisk)} output={out_path} bytes={len(out)}")
PY
}

build() {
  require_cmd curl
  require_cmd gzip
  require_cmd python3
  require_cmd tar
  require_cmd sha256sum
  require_file "$BASE_BOOT_IMG"
  require_file "$KERNEL_IMG"

  mkdir -p "$OUT_DIR"
  local apk="$OUT_DIR/busybox-static-aarch64.apk"
  local root="$OUT_DIR/initramfs-root"
  local ramdisk="$OUT_DIR/initramfs.cpio.gz"
  local boot_img="$OUT_DIR/lineage-mininitramfs-boot.img"

  if [ ! -f "$apk" ]; then
    log "downloading aarch64 static busybox"
    curl -L --fail --retry 3 -o "$apk" "$BUSYBOX_APK_URL"
  fi

  rm -rf "$root"
  mkdir -p "$root/bin" "$root/sbin" "$root/proc" "$root/sys" "$root/run" "$root/tmp" \
    "$root/mnt/arch" "$root/mnt/data" "$root/mnt/ubuntu" "$root/etc"
  echo "$INIT_MODE" > "$root/etc/alioth-init-mode"
  echo "$UBUNTU_ROOTFS_PATH" > "$root/etc/alioth-ubuntu-rootfs"
  echo "$UBUNTU_SYSTEMD_WRAPPER_MODE" > "$root/etc/alioth-systemd-wrapper-mode"
  date +%s > "$root/etc/alioth-build-epoch"
  if [ -f "$SSH_PUBKEY_FILE" ]; then
    cp "$SSH_PUBKEY_FILE" "$root/etc/alioth-ssh-authorized-key"
    chmod 0644 "$root/etc/alioth-ssh-authorized-key"
  fi
  if [ -n "$DEFAULT_WIFI_CONFIG" ]; then
    printf '%s\n' "$DEFAULT_WIFI_CONFIG" > "$root/etc/alioth-wifi-default"
    chmod 0600 "$root/etc/alioth-wifi-default"
  elif [ -n "$DEFAULT_WIFI_SSID" ]; then
    printf '%s\n%s\n' "$DEFAULT_WIFI_SSID" "$DEFAULT_WIFI_PSK" > "$root/etc/alioth-wifi-default"
    chmod 0600 "$root/etc/alioth-wifi-default"
  fi
  tar -xOzf "$apk" bin/busybox.static > "$root/bin/busybox"
  chmod 0755 "$root/bin/busybox"
  build_minitcpsh "$root/bin/minitcpsh"
  build_usb_dhcpd "$root/bin/alioth-usb-dhcpd"
  build_reboot_tool "$root/bin/alioth-reboot"
  cat > "$root/bin/reboot-bootloader" <<'EOF'
#!/bin/sh
exec /bin/alioth-reboot bootloader
EOF
  chmod 0755 "$root/bin/reboot-bootloader"
  if [ -n "$STATUS_UI_WRAPPER_BIN" ]; then
    require_file "$STATUS_UI_WRAPPER_BIN"
    build_status_ui "$root/bin/alioth-status-ui-cpu"
    cp "$STATUS_UI_WRAPPER_BIN" "$root/bin/alioth-status-ui"
    chmod 0755 "$root/bin/alioth-status-ui"
  else
    build_status_ui "$root/bin/alioth-status-ui"
  fi
  for app in sh ash mount umount mkdir mknod sleep cat cp echo ls dmesg grep awk sed cut tr \
    ifconfig route ip udhcpd setsid chroot switch_root sync reboot poweroff \
    blkid uname ps kill ln chmod chown basename dirname true false seq find sort head tail date; do
    ln -sf busybox "$root/bin/$app"
  done
  ln -sf bin/busybox "$root/init"
  write_init_script "$root/init.real"
  mv "$root/init.real" "$root/init"

  log "building initramfs with explicit device nodes"
  build_initramfs "$root" "$ramdisk"

  log "packing Android boot v3 image with Lineage kernel"
  pack_boot_image "$BASE_BOOT_IMG" "$KERNEL_IMG" "$ramdisk" "$boot_img"
  sha256sum "$KERNEL_IMG" "$ramdisk" "$boot_img" > "$OUT_DIR/SHA256SUMS"
  ls -lh "$KERNEL_IMG" "$ramdisk" "$boot_img"
}

boot() {
  require_cmd "$ADB_BIN"
  require_cmd "$FASTBOOT_BIN"
  build
  ensure_fastboot
  local boot_img="$OUT_DIR/lineage-mininitramfs-boot.img"
  log "temporary booting mininitramfs image"
  "$FASTBOOT_BIN" boot "$boot_img"
  log "boot command sent; if successful, watch for a new USB NCM interface and telnet 172.16.42.2"
}

case "${1:-build}" in
  build)
    build
    ;;
  boot)
    boot
    ;;
  *)
    cat >&2 <<EOF
usage: $0 [build|boot]

Environment:
  BASE_BOOT_IMG=$BASE_BOOT_IMG
  KERNEL_IMG=$KERNEL_IMG
  OUT_DIR=$OUT_DIR
  INIT_MODE=$INIT_MODE
  UBUNTU_ROOTFS_PATH=$UBUNTU_ROOTFS_PATH
  UBUNTU_SYSTEMD_WRAPPER_MODE=$UBUNTU_SYSTEMD_WRAPPER_MODE
  USB_DHCPD_SRC=$USB_DHCPD_SRC
  USB_DHCPD_BIN=$USB_DHCPD_BIN
  REBOOT_TOOL_SRC=$REBOOT_TOOL_SRC
  REBOOT_TOOL_BIN=$REBOOT_TOOL_BIN
  STATUS_UI_SRC=$STATUS_UI_SRC
  STATUS_UI_BIN=$STATUS_UI_BIN
  STATUS_UI_WRAPPER_BIN=$STATUS_UI_WRAPPER_BIN
  NCM_DEVICE_IP=$NCM_DEVICE_IP
  NCM_HOST_IP=$NCM_HOST_IP
  NCM_PORT=$NCM_PORT
EOF
    exit 2
    ;;
esac
