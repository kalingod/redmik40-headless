#!/bin/sh
# alioth-subsys-boot - Boot SLPI and CDSP subsystems
# Similar to alioth-audio-adsp-boot but for sensor and compute DSPs
#
# This creates firmware symlinks in /lib/firmware/ and triggers
# the subsystem boot via /sys/kernel/boot_*/boot nodes.

set -u

PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

LOG="/run/alioth-subsys-boot.log"
FW_BASE="/vendor/firmware_mnt/image"
LIB_FW="/lib/firmware"

mkdir -p /run /lib/firmware 2>/dev/null || true

log() {
  printf '[%s] alioth-subsys-boot: %s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ' 2>/dev/null || echo unknown)" "$*" | tee -a "$LOG"
  logger -t alioth-subsys-boot -- "$*" 2>/dev/null || true
}

ensure_firmware_symlinks() {
  name="$1"
  if [ ! -e "$LIB_FW/${name}.mdt" ]; then
    if [ -r "$FW_BASE/${name}.mdt" ]; then
      ln -sf "$FW_BASE/${name}.mdt" "$LIB_FW/${name}.mdt" 2>/dev/null
      for i in $(seq 0 25); do
        src="$FW_BASE/${name}.b$(printf '%02d' $i)"
        [ -r "$src" ] && ln -sf "$src" "$LIB_FW/${name}.b$(printf '%02d' $i)" 2>/dev/null || true
      done
      log "created $name firmware symlinks in $LIB_FW"
    else
      log "WARN: $name.mdt not found in $FW_BASE"
      return 1
    fi
  else
    log "$name firmware symlinks already exist"
  fi
  return 0
}

boot_subsys() {
  name="$1"
  state_node="$2"
  boot_node="$3"

  state=$(cat "$state_node" 2>/dev/null || echo unknown)
  if [ "$state" = "ONLINE" ]; then
    log "$name already ONLINE"
    return 0
  fi

  if [ ! -w "$boot_node" ]; then
    log "ERROR: $boot_node is not writable"
    return 1
  fi

  log "triggering $name boot from state=$state"
  printf '1\n' > "$boot_node"

  # Wait up to 30 seconds
  i=1
  while [ "$i" -le 30 ]; do
    state=$(cat "$state_node" 2>/dev/null || echo unknown)
    if [ "$state" = "ONLINE" ]; then
      log "$name is ONLINE (took ~${i}s)"
      return 0
    fi
    if [ "$state" = "CRASHED" ] || [ "$state" = "FAILED" ]; then
      log "ERROR: $name entered $state state"
      return 1
    fi
    sleep 1
    i=$((i + 1))
  done

  log "ERROR: $name did not come ONLINE within 30s (state=$state)"
  return 1
}

# --- Main ---
log "starting subsystem boot"

# Ensure firmware mount is available
if [ ! -r "$FW_BASE/adsp.mdt" ]; then
  if [ -x /usr/local/sbin/alioth-wifi-prepare ]; then
    log "running alioth-wifi-prepare for firmware mount"
    /usr/local/sbin/alioth-wifi-prepare >>"$LOG" 2>&1 || true
  fi
fi

# Create firmware symlinks for Venus (even if no boot node, needed for on-demand load)
ensure_firmware_symlinks venus || true

# Boot SLPI (Sensor Low Power Island)
slpi_rc=0
if ensure_firmware_symlinks slpi; then
  if ! boot_subsys slpi /sys/bus/msm_subsys/devices/subsys7/state /sys/kernel/boot_slpi/boot; then
    slpi_rc=1
  fi
else
  slpi_rc=1
fi

# Boot CDSP (Compute DSP)
cdsp_rc=0
if ensure_firmware_symlinks cdsp; then
  if ! boot_subsys cdsp /sys/bus/msm_subsys/devices/subsys3/state /sys/kernel/boot_cdsp/boot; then
    cdsp_rc=1
  fi
else
  cdsp_rc=1
fi

# Summary
log "done: slpi_rc=$slpi_rc cdsp_rc=$cdsp_rc"

if [ "$slpi_rc" -eq 0 ] && [ "$cdsp_rc" -eq 0 ]; then
  exit 0
elif [ "$slpi_rc" -eq 0 ] || [ "$cdsp_rc" -eq 0 ]; then
  exit 0  # partial success is OK
else
  exit 1
fi
