#!/usr/bin/env bash
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
KERNEL_ROOT="${KERNEL_ROOT:-/Users/wuyuele/android-kernel}"
KERNEL_SRC="${KERNEL_SRC:-$KERNEL_ROOT/alioth}"
KERNEL_OUT="${KERNEL_OUT:-$KERNEL_ROOT/out}"
KERNEL_CACHE="${KERNEL_CACHE:-$KERNEL_ROOT/cache}"
IMAGE="${KERNEL_BUILDER_IMAGE:-redmik40-kernel-builder:bookworm}"

mkdir -p "$KERNEL_SRC" "$KERNEL_OUT" "$KERNEL_CACHE"

if ! docker image inspect "$IMAGE" >/dev/null 2>&1; then
  echo "builder image '$IMAGE' not found. Build it with:" >&2
  echo "  docker build -t $IMAGE $REPO_DIR/docker/kernel-builder" >&2
  exit 1
fi

exec docker run --rm -it \
  --platform linux/amd64 \
  -v "$KERNEL_SRC:/src" \
  -v "$KERNEL_OUT:/out" \
  -v "$KERNEL_CACHE:/cache" \
  -v "$REPO_DIR:/project:ro" \
  -w /src \
  -e ARCH=arm64 \
  -e CCACHE_DIR=/cache/ccache \
  "$IMAGE" \
  bash
