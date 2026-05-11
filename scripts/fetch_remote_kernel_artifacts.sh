#!/usr/bin/env bash
set -euo pipefail

REMOTE_HOST="${REMOTE_HOST:-47.98.228.151}"
REMOTE_PORT="${REMOTE_PORT:-60022}"
REMOTE_USER="${REMOTE_USER:-lele}"
REMOTE_KNOWN_HOSTS="${REMOTE_KNOWN_HOSTS:-/tmp/redmik40_remote_known_hosts}"
REMOTE_ARTIFACT_ROOT="${REMOTE_ARTIFACT_ROOT:-/home/lele/redmik40-build/lineage-sm8250/artifacts}"
LOCAL_ARTIFACT_ROOT="${LOCAL_ARTIFACT_ROOT:-artifacts/remote}"

artifact_name="${1:-kernel-lineage20-alioth-j36}"

case "$artifact_name" in
  *[!A-Za-z0-9._@+-]* | "" )
    echo "invalid artifact name: $artifact_name" >&2
    exit 2
    ;;
esac

local_dir="$LOCAL_ARTIFACT_ROOT/$artifact_name"
remote_dir="$REMOTE_ARTIFACT_ROOT/$artifact_name"

mkdir -p "$local_dir"

scp -P "$REMOTE_PORT" \
  -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile="$REMOTE_KNOWN_HOSTS" \
  "$REMOTE_USER@$REMOTE_HOST:$remote_dir/*" \
  "$local_dir/"

echo "fetched: $REMOTE_USER@$REMOTE_HOST:$remote_dir -> $local_dir"
