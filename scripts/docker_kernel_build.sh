#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KERNEL_ROOT="${KERNEL_ROOT:-/Users/wuyuele/android-kernel}"
KERNEL_SRC="${KERNEL_SRC:-$KERNEL_ROOT/alioth}"
KERNEL_OUT="${KERNEL_OUT:-$KERNEL_ROOT/out/lineage20-alioth}"
KERNEL_CACHE="${KERNEL_CACHE:-$KERNEL_ROOT/cache}"
IMAGE="${KERNEL_BUILDER_IMAGE:-redmik40-kernel-builder:bookworm}"
DEFCONFIG="${DEFCONFIG:-vendor/alioth_defconfig}"
JOBS="${JOBS:-$(sysctl -n hw.ncpu 2>/dev/null || echo 8)}"

mkdir -p "$KERNEL_SRC" "$KERNEL_OUT" "$KERNEL_CACHE"

if [ ! -f "$KERNEL_SRC/Makefile" ]; then
  echo "kernel source not found at $KERNEL_SRC" >&2
  echo "clone it first, for example:" >&2
  echo "  mkdir -p $KERNEL_ROOT" >&2
  echo "  git clone --filter=blob:none --branch lineage-20 https://github.com/xiaomi-sm8250-devs/android_kernel_xiaomi_sm8250 $KERNEL_SRC" >&2
  exit 1
fi

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
  echo "builder image '$IMAGE' not found. Build it with:" >&2
  echo "  docker build -t $IMAGE $REPO_DIR/docker/kernel-builder" >&2
  exit 1
fi

exec docker run --rm \
  --platform linux/amd64 \
  -v "$KERNEL_SRC:/src" \
  -v "$KERNEL_OUT:/out" \
  -v "$KERNEL_CACHE:/cache" \
  -v "$REPO_DIR:/project:ro" \
  -w /src \
  -e ARCH=arm64 \
  -e CCACHE_DIR=/cache/ccache \
  "$IMAGE" \
  bash -lc "set -euo pipefail
    echo 'source: /src'
    echo 'out: /out'
    echo 'defconfig: $DEFCONFIG'
    echo 'jobs: $JOBS'
    ccache -M 20G >/dev/null 2>&1 || true
    make O=/out ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CLANG_TRIPLE=aarch64-linux-gnu- LLVM=1 LLVM_IAS=1 $DEFCONFIG
    make O=/out ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- CLANG_TRIPLE=aarch64-linux-gnu- LLVM=1 LLVM_IAS=1 -j$JOBS
    echo 'artifacts:'
    find /out/arch/arm64/boot -maxdepth 3 -type f \( -name Image -o -name '*.dtb' -o -name '*.dtbo' \) -print 2>/dev/null | sort
  "
