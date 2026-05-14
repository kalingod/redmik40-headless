#!/bin/sh
case "$0" in
  */*) helper="${0%/*}" ;;
  *) helper=/var/tmp/alioth-switchroot ;;
esac

cpu="$helper/alioth-status-ui-cpu"
[ -x "$cpu" ] || cpu=/var/tmp/alioth-switchroot/alioth-status-ui-cpu
[ -x "$cpu" ] || cpu=/bin/alioth-status-ui-cpu

gpu=/data/experiments/alioth_gpu_monitor_service
[ -x "$gpu" ] || gpu=/data/experiments/gpu-runtime-20260513/alioth_gpu_monitor_service

icd=/data/experiments/freedreno_icd-kgsl-mesa2034-builddir.json
[ -r "$icd" ] || icd=/data/experiments/gpu-runtime-20260513/freedreno_icd-kgsl-mesa2034-aarch64.json

runtime_lib=/data/experiments/gpu-runtime-20260513/lib
turnip_lib=/data/experiments/mesa-kgsl-prefix/lib/aarch64-linux-gnu
vert=/data/experiments/alioth_vulkan_glyph_text.vert.spv
frag=/data/experiments/alioth_vulkan_glyph_text.frag.spv
log=/run/alioth-status-ui-wrapper.log

say() {
  mkdir -p /run 2>/dev/null || true
  printf '[%s] %s\n' "$(cut -d' ' -f1 /proc/uptime 2>/dev/null || echo 0)" "$*" >>"$log" 2>/dev/null || true
}

create_char_node() {
  target="$1"
  devfile="$2"
  group="$3"
  mode="$4"
  [ -r "$devfile" ] || return 1
  mm="$(cat "$devfile" 2>/dev/null || true)"
  maj="${mm%:*}"
  min="${mm#*:}"
  [ -n "$maj" ] && [ -n "$min" ] || return 1
  mkdir -p "${target%/*}" 2>/dev/null || true
  [ -c "$target" ] || rm -f "$target" 2>/dev/null || true
  [ -c "$target" ] || mknod "$target" c "$maj" "$min" 2>/dev/null || true
  chown "root:$group" "$target" 2>/dev/null || true
  chmod "$mode" "$target" 2>/dev/null || true
  [ -c "$target" ]
}

ensure_gpu_nodes() {
  for _ in $(seq 1 20); do
    create_char_node /dev/dri/card0 /sys/class/drm/card0/dev video 0660 || true
    create_char_node /dev/dri/renderD128 /sys/class/drm/renderD128/dev video 0660 || true
    create_char_node /dev/kgsl-3d0 /sys/class/kgsl/kgsl-3d0/dev video 0660 || true
    [ -c /dev/dri/card0 ] && [ -c /dev/kgsl-3d0 ] && return 0
    sleep 1
  done
  return 1
}

start_cpu() {
  say "starting CPU fallback: $cpu"
  exec "$cpu"
}

[ -x "$cpu" ] || {
  say "CPU fallback missing: $cpu"
  exit 127
}

if [ -x "$gpu" ] && [ -r "$icd" ] && [ -r "$vert" ] && [ -r "$frag" ] &&
   ensure_gpu_nodes; then
  say "starting GPU monitor service: $gpu"
  export LD_LIBRARY_PATH="$runtime_lib:$turnip_lib:${LD_LIBRARY_PATH:-}"
  export VK_ICD_FILENAMES="$icd"
  export TU_DEBUG=startup
  "$gpu" monitor "$vert" "$frag" &
  child="$!"
  trap 'kill "$child" 2>/dev/null; wait "$child" 2>/dev/null; exit 0' INT TERM
  wait "$child"
  rc="$?"
  trap - INT TERM
  say "GPU monitor exited rc=$rc; falling back to CPU UI"
fi

start_cpu
