#!/bin/sh
set -eu

if [ "$#" -lt 1 ]; then
  echo "usage: alioth-android-daemon /vendor/bin/name [args...]" >&2
  exit 2
fi

binary="$1"
shift

linker="${ALIOTH_ANDROID_LINKER:-/system/bin/linker64}"
if [ ! -x "$linker" ]; then
  linker=/apex/com.android.runtime/bin/linker64
fi

if [ ! -x "$linker" ]; then
  echo "Android linker64 not found" >&2
  exit 1
fi
if [ ! -x "$binary" ]; then
  echo "Android daemon not executable: $binary" >&2
  exit 1
fi

export ANDROID_ROOT="${ANDROID_ROOT:-/system}"
export ANDROID_DATA="${ANDROID_DATA:-/data}"
export APEX_ROOT="${APEX_ROOT:-/apex}"
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-/vendor/lib64:/odm/lib64:/system_ext/lib64:/product/lib64:/system/lib64:/apex/com.android.runtime/lib64/bionic}"

exec "$linker" "$binary" "$@"
