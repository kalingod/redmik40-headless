#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

BASE_IMG="${BASE_IMG:-$REPO_DIR/images/orangefox-arch-chroot.img}"
OUT_DIR="${OUT_DIR:-/vmdata/android/redmik40/kernel-candidate}"
ADB_BIN="${ADB_BIN:-adb}"
FASTBOOT_BIN="${FASTBOOT_BIN:-fastboot}"

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

usage() {
  cat <<EOF
Usage:
  $(basename "$0") --boot-img path/to/candidate_boot_or_recovery.img [--boot]
  $(basename "$0") --kernel path/to/Image_or_Image.gz [--boot]

What it does:
  1. Uses known-good OrangeFox boot header + ramdisk from:
     $BASE_IMG
  2. Replaces only the kernel with the candidate kernel.
  3. Builds:
     $OUT_DIR/candidate-with-orangefox-ramdisk.img
  4. With --boot, fastboot-boots it and runs diagnostics.

This does not flash anything.
EOF
}

extract_boot_parts() {
  local image="$1" kernel_out="$2" ramdisk_out="$3"
  python3 - "$image" "$kernel_out" "$ramdisk_out" <<'PY'
from pathlib import Path
import struct
import sys

image, kernel_out, ramdisk_out = map(Path, sys.argv[1:])
data = image.read_bytes()
if data[:8] != b"ANDROID!":
    raise SystemExit(f"{image} is not an Android boot image")

kernel_size, ramdisk_size, _os_version, header_size = struct.unpack_from("<4I", data, 8)
header_version = struct.unpack_from("<I", data, 40)[0]
if header_version != 3 or header_size != 1580:
    raise SystemExit(f"unsupported boot header: version={header_version} size={header_size}")

page = 4096
def align(n):
    return (n + page - 1) // page * page

kernel_off = align(header_size)
ramdisk_off = kernel_off + align(kernel_size)
kernel_out.write_bytes(data[kernel_off:kernel_off + kernel_size])
ramdisk_out.write_bytes(data[ramdisk_off:ramdisk_off + ramdisk_size])
print(f"kernel_size={kernel_size} ramdisk_size={ramdisk_size}")
PY
}

pack_boot_image() {
  local base_image="$1" kernel="$2" ramdisk="$3" out="$4"
  python3 - "$base_image" "$kernel" "$ramdisk" "$out" <<'PY'
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

adb_state() {
  "$ADB_BIN" devices 2>/dev/null | awk 'NR > 1 && NF >= 2 {print $2; exit}'
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

wait_adb() {
  local timeout="${1:-180}" deadline state
  deadline=$((SECONDS + timeout))
  while [ "$SECONDS" -lt "$deadline" ]; do
    state="$(adb_state || true)"
    if [ "$state" = "recovery" ] || [ "$state" = "device" ]; then
      "$ADB_BIN" devices -l
      return 0
    fi
    sleep 1
  done
  die "timed out waiting for adb"
}

boot_temp_image() {
  local image="$1"
  require_cmd "$ADB_BIN"
  require_cmd "$FASTBOOT_BIN"

  if "$FASTBOOT_BIN" devices 2>/dev/null | awk 'NF >= 2 {found=1} END {exit !found}'; then
    log "device already in fastboot"
  else
    local state
    state="$(adb_state || true)"
    [ -n "$state" ] || die "device is not visible in adb or fastboot"
    log "rebooting to bootloader from adb state: $state"
    "$ADB_BIN" reboot bootloader
    wait_fastboot 90
  fi

  log "temporary booting candidate image"
  "$FASTBOOT_BIN" boot "$image"
  wait_adb 180
}

run_diagnostics() {
  require_cmd "$ADB_BIN"
  log "running kernel capability diagnostics"
  "$ADB_BIN" shell 'echo "--- kernel ---"
uname -a
echo "--- boot state ---"
getprop ro.hardware 2>/dev/null
getprop ro.boot.slot_suffix 2>/dev/null
echo "--- drm/display ---"
for f in /sys/class/drm/card0-DSI-1/status /sys/class/drm/card0-DSI-1/enabled /sys/class/drm/card0-DSI-1/dpms /sys/class/drm/card0-DSI-1/modes; do
  [ -e "$f" ] && echo "$f=$(cat "$f" 2>/dev/null)"
done
echo "--- block devices ---"
ls -l /dev/block/by-name/arch /dev/block/by-name/esp 2>/dev/null || true
echo "--- docker-critical config ---"
zcat /proc/config.gz 2>/dev/null | grep -E "CONFIG_(PID_NS|USER_NS|NET_NS|IPC_NS|UTS_NS|CGROUPS|CGROUP_DEVICE|CGROUP_PIDS|MEMCG|OVERLAY_FS|VETH|BRIDGE|NETFILTER|NF_NAT|IP_NF_IPTABLES|SECCOMP|SECCOMP_FILTER)=" | sort || true
echo "--- kvm config ---"
zcat /proc/config.gz 2>/dev/null | grep -E "CONFIG_(VIRTUALIZATION|KVM|ARM64_VHE)=" | sort || true
ls -l /dev/kvm 2>/dev/null || true
echo "--- namespace smoke ---"
command -v unshare >/dev/null 2>&1 && {
  unshare -m sh -c true && echo mount_ns_ok || echo mount_ns_fail
  unshare -n sh -c true && echo net_ns_ok || echo net_ns_fail
  unshare -p -f sh -c true && echo pid_ns_ok || echo pid_ns_fail
} || echo no_unshare
echo "--- arch chroot smoke ---"
if [ -x /tmp/enter-arch ]; then
  /tmp/enter-arch "cat /etc/os-release | head -3; uname -r" 2>/dev/null || true
else
  echo no_enter_arch
fi'
}

main() {
  local candidate_boot="" candidate_kernel="" do_boot=0
  while [ "$#" -gt 0 ]; do
    case "$1" in
      --boot-img)
        candidate_boot="${2:-}"
        shift 2
        ;;
      --kernel)
        candidate_kernel="${2:-}"
        shift 2
        ;;
      --boot)
        do_boot=1
        shift
        ;;
      -h|--help)
        usage
        exit 0
        ;;
      *)
        usage
        die "unknown argument: $1"
        ;;
    esac
  done

  [ -n "$candidate_boot" ] || [ -n "$candidate_kernel" ] || {
    usage
    die "provide --boot-img or --kernel"
  }
  [ -z "$candidate_boot" ] || [ -z "$candidate_kernel" ] || die "use only one of --boot-img or --kernel"

  require_cmd python3
  require_file "$BASE_IMG"
  mkdir -p "$OUT_DIR"

  local base_kernel="$OUT_DIR/base-kernel"
  local base_ramdisk="$OUT_DIR/base-ramdisk"
  local candidate_kernel_out="$OUT_DIR/candidate-kernel"
  local out_img="$OUT_DIR/candidate-with-orangefox-ramdisk.img"

  log "extracting base OrangeFox kernel/ramdisk"
  extract_boot_parts "$BASE_IMG" "$base_kernel" "$base_ramdisk"

  if [ -n "$candidate_boot" ]; then
    require_file "$candidate_boot"
    log "extracting candidate kernel from Android boot image: $candidate_boot"
    extract_boot_parts "$candidate_boot" "$candidate_kernel_out" "$OUT_DIR/candidate-ramdisk-unused"
  else
    require_file "$candidate_kernel"
    log "using candidate kernel file: $candidate_kernel"
    cp "$candidate_kernel" "$candidate_kernel_out"
  fi

  log "packing candidate kernel with known-good OrangeFox ramdisk"
  pack_boot_image "$BASE_IMG" "$candidate_kernel_out" "$base_ramdisk" "$out_img"
  shasum -a 256 "$out_img" "$candidate_kernel_out" > "$OUT_DIR/SHA256SUMS"
  ls -lh "$out_img" "$candidate_kernel_out"

  if [ "$do_boot" = 1 ]; then
    boot_temp_image "$out_img"
    run_diagnostics
  else
    log "built only. To boot:"
    echo "  $FASTBOOT_BIN boot $out_img"
  fi
}

main "$@"
