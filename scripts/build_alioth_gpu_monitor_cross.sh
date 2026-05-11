#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

SYSROOT="${SYSROOT:-/vmdata/android/redmik40/sysroots/ubuntu2404-arm64-gpu-monitor}"
OUT_DIR="${OUT_DIR:-/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/cross}"
BUILDER_IMAGE="${BUILDER_IMAGE:-redmik40-kernel-builder:bullseye}"
SRC="${SRC:-$REPO_DIR/scripts/alioth_drm_prime_vulkan_glyph_text_pageflip.c}"
OUT="$OUT_DIR/alioth_gpu_monitor_service"

[ -d "$SYSROOT/usr/include" ] || {
  echo "missing sysroot: $SYSROOT" >&2
  echo "run scripts/sync_alioth_ubuntu2404_sysroot.sh first" >&2
  exit 1
}

mkdir -p "$OUT_DIR"

if ! docker image inspect "$BUILDER_IMAGE" >/dev/null 2>&1 &&
   [ -f "$REPO_DIR/docker/Dockerfile.cross-ubuntu2404" ]; then
  docker build -f "$REPO_DIR/docker/Dockerfile.cross-ubuntu2404" \
    -t "$BUILDER_IMAGE" "$REPO_DIR"
fi

docker run --rm \
  -v "$REPO_DIR:/work/repo:ro" \
  -v "$SYSROOT:/work/sysroot:ro" \
  -v "$OUT_DIR:/work/out" \
  "$BUILDER_IMAGE" \
  bash -lc '
set -euo pipefail
export PKG_CONFIG_SYSROOT_DIR=/work/sysroot
export PKG_CONFIG_LIBDIR=/work/sysroot/usr/lib/aarch64-linux-gnu/pkgconfig:/work/sysroot/usr/lib/pkgconfig:/work/sysroot/usr/share/pkgconfig
cflags="-I/work/sysroot/usr/include/libdrm -I/work/sysroot/usr/include/drm"
libs="-ldrm -lvulkan"
if command -v pkg-config >/dev/null 2>&1; then
  cflags="$(pkg-config --cflags libdrm vulkan 2>/dev/null || printf "%s" "$cflags") $cflags"
  libs="$(pkg-config --libs libdrm vulkan 2>/dev/null || printf "%s" "$libs")"
fi
aarch64-linux-gnu-gcc --sysroot=/work/sysroot \
  -B/work/sysroot/usr/lib/aarch64-linux-gnu \
  -B/work/sysroot/lib/aarch64-linux-gnu \
  -O2 -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation \
  $cflags \
  -L/work/sysroot/lib/aarch64-linux-gnu \
  -L/work/sysroot/usr/lib/aarch64-linux-gnu \
  -Wl,-rpath-link,/work/sysroot/lib/aarch64-linux-gnu \
  -Wl,-rpath-link,/work/sysroot/usr/lib/aarch64-linux-gnu \
  -o /work/out/alioth_gpu_monitor_service \
  /work/repo/scripts/alioth_drm_prime_vulkan_glyph_text_pageflip.c \
  $libs
readelf -d /work/out/alioth_gpu_monitor_service | grep -E "NEEDED|interpreter" || true
'

file "$OUT"
sha256sum "$OUT"
