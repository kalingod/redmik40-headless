#!/bin/sh
set -eu

DATA_DEV="${LELE_DATA_DEV:-/dev/block/by-name/userdata}"
DATA_MOUNT="${LELE_DATA_MOUNT:-/data}"

mount_data() {
  mkdir -p "$DATA_MOUNT"
  if ! mountpoint -q "$DATA_MOUNT"; then
    mount -o noatime "$DATA_DEV" "$DATA_MOUNT"
  fi
}

bind_dir() {
  src="$1"
  dst="$2"
  mkdir -p "$src" "$dst"
  if ! mountpoint -q "$dst"; then
    mount --bind "$src" "$dst"
  fi
}

mount_data
mkdir -p "$DATA_MOUNT/docker" "$DATA_MOUNT/srv" "$DATA_MOUNT/postgres" "$DATA_MOUNT/valkey"

bind_dir "$DATA_MOUNT/docker" /var/lib/docker
bind_dir "$DATA_MOUNT/srv" /srv
bind_dir "$DATA_MOUNT/postgres" /var/lib/postgres
bind_dir "$DATA_MOUNT/valkey" /var/lib/valkey

df -hT "$DATA_MOUNT" /var/lib/docker /srv /var/lib/postgres /var/lib/valkey
