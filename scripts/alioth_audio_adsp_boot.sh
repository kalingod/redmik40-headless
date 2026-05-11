#!/bin/sh
set -u

PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

LOG="${ALIOTH_AUDIO_LOG:-/run/alioth-audio-adsp-boot.log}"
STATE="${ALIOTH_AUDIO_STATE:-/run/alioth-audio-state}"
CREATED_LIST="${ALIOTH_AUDIO_CREATED_LIST:-/run/alioth-audio-snd-nodes.created}"
FW_PATH_NODE="${ALIOTH_AUDIO_FW_PATH_NODE:-/sys/module/firmware_class/parameters/path}"
ADSP_BOOT_NODE="${ALIOTH_AUDIO_ADSP_BOOT_NODE:-/sys/kernel/boot_adsp/boot}"
ADSP_STATE_NODE="${ALIOTH_AUDIO_ADSP_STATE_NODE:-/sys/bus/msm_subsys/devices/subsys2/state}"
WAIT_SECS="${ALIOTH_AUDIO_WAIT_SECS:-30}"
FW_IMAGE="${ALIOTH_AUDIO_FW_IMAGE:-}"

mkdir -p /run "$(dirname "$LOG")" 2>/dev/null || true

log() {
  msg="$*"
  printf '[%s] alioth-audio-adsp-boot: %s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ' 2>/dev/null || echo unknown-time)" "$msg" | tee -a "$LOG"
  logger -t alioth-audio-adsp-boot -- "$msg" 2>/dev/null || true
}

write_state() {
  mode="$1"
  {
    printf 'mode=%s\n' "$mode"
    printf 'updated_utc=%s\n' "$(date -u '+%Y-%m-%dT%H:%M:%SZ' 2>/dev/null || true)"
    printf 'firmware_image=%s\n' "${FW_IMAGE:-}"
    printf 'firmware_class_path='
    cat "$FW_PATH_NODE" 2>/dev/null || true
    printf 'adsp_state='
    cat "$ADSP_STATE_NODE" 2>/dev/null || true
    printf 'sound_nodes=%s\n' "$(find /dev/snd -maxdepth 1 -type c 2>/dev/null | wc -l)"
    cat /proc/asound/cards 2>/dev/null | sed 's/^/card=/'
  } > "$STATE" 2>/dev/null || true
}

adsp_state() {
  cat "$ADSP_STATE_NODE" 2>/dev/null || echo unknown
}

choose_firmware_image() {
  if [ -n "$FW_IMAGE" ] && [ -r "$FW_IMAGE/adsp.mdt" ]; then
    return 0
  fi

  for candidate in \
    /vendor/firmware_mnt/image \
    /firmware/image \
    /run/alioth-firmware-mnt/image; do
    if [ -r "$candidate/adsp.mdt" ]; then
      FW_IMAGE="$candidate"
      return 0
    fi
  done

  return 1
}

ensure_firmware_mount() {
  if choose_firmware_image; then
    return 0
  fi

  if [ -x /usr/local/sbin/alioth-wifi-prepare ]; then
    log "ADSP firmware image not visible; running alioth-wifi-prepare"
    /usr/local/sbin/alioth-wifi-prepare >>"$LOG" 2>&1 || true
  fi

  choose_firmware_image
}

set_firmware_path() {
  [ -n "$FW_IMAGE" ] || return 1
  if [ ! -w "$FW_PATH_NODE" ]; then
    log "firmware class path node is not writable: $FW_PATH_NODE"
    return 1
  fi
  printf '%s\n' "$FW_IMAGE" > "$FW_PATH_NODE"
  log "set firmware_class.path to $FW_IMAGE"
}

boot_adsp() {
  state="$(adsp_state)"
  if [ "$state" = "ONLINE" ]; then
    log "ADSP already ONLINE"
    return 0
  fi

  if [ ! -w "$ADSP_BOOT_NODE" ]; then
    log "ADSP boot node is not writable: $ADSP_BOOT_NODE"
    return 1
  fi

  log "triggering ADSP boot from state=$state"
  printf '1\n' > "$ADSP_BOOT_NODE"
}

wait_for_audio_card() {
  i=1
  while [ "$i" -le "$WAIT_SECS" ]; do
    state="$(adsp_state)"
    if grep -q 'kona-mtp-snd-card' /proc/asound/cards 2>/dev/null; then
      log "audio card ready at poll=$i adsp_state=$state"
      return 0
    fi
    log "waiting for audio card poll=$i adsp_state=$state"
    sleep 1
    i=$((i + 1))
  done
  return 1
}

trigger_udev_sound() {
  if command -v udevadm >/dev/null 2>&1; then
    udevadm trigger --subsystem-match=sound --action=add >>"$LOG" 2>&1 || true
    udevadm settle --timeout=10 >>"$LOG" 2>&1 || true
  fi
}

valid_devno() {
  case "$1" in
    *[!0-9:]*|''|:*|*:|*:*:*) return 1 ;;
  esac
  return 0
}

ensure_sound_nodes() {
  mkdir -p /dev/snd
  : > "$CREATED_LIST"

  made=0
  skipped=0
  failed=0
  seen=0

  for devfile in /sys/class/sound/*/dev; do
    [ -r "$devfile" ] || continue
    seen=$((seen + 1))
    name="$(basename "$(dirname "$devfile")")"
    devno="$(cat "$devfile" 2>/dev/null || true)"
    if ! valid_devno "$devno"; then
      log "bad dev number for $name: $devno"
      failed=$((failed + 1))
      continue
    fi

    major="${devno%:*}"
    minor="${devno#*:}"
    target="/dev/snd/$name"

    if [ -c "$target" ]; then
      skipped=$((skipped + 1))
      continue
    fi
    if [ -e "$target" ]; then
      log "not replacing existing non-character path: $target"
      failed=$((failed + 1))
      continue
    fi

    if mknod -m 660 "$target" c "$major" "$minor"; then
      if getent group audio >/dev/null 2>&1; then
        chgrp audio "$target" 2>/dev/null || true
      fi
      chmod 0660 "$target" 2>/dev/null || true
      printf '%s\n' "$target" >> "$CREATED_LIST"
      made=$((made + 1))
    else
      log "failed to create $target c $major $minor"
      failed=$((failed + 1))
    fi
  done

  log "sound node pass complete: seen=$seen made=$made skipped=$skipped failed=$failed"
  [ "$seen" -gt 0 ] && [ "$failed" -eq 0 ] && [ -c /dev/snd/controlC0 ]
}

main() {
  : > "$LOG"
  log "starting"

  if ! ensure_firmware_mount; then
    log "ADSP firmware image not found"
    write_state failed
    exit 2
  fi

  if ! set_firmware_path; then
    write_state failed
    exit 3
  fi

  if ! boot_adsp; then
    write_state failed
    exit 4
  fi

  if ! wait_for_audio_card; then
    log "audio card did not appear"
    cat /proc/asound/cards >> "$LOG" 2>/dev/null || true
    dmesg | grep -iE 'adsp|apr|asoc|snd|wcd|bolero|audio|pdr|fastrpc' | tail -160 >> "$LOG" 2>/dev/null || true
    write_state failed
    exit 5
  fi

  trigger_udev_sound
  if ! ensure_sound_nodes; then
    log "sound node creation incomplete"
    write_state failed
    exit 6
  fi

  write_state ready
  log "ready"
}

main "$@"
