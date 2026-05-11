#!/usr/bin/env bash
set -euo pipefail

REMOTE_HOST="${REMOTE_HOST:-47.98.228.151}"
REMOTE_PORT="${REMOTE_PORT:-60022}"
REMOTE_USER="${REMOTE_USER:-lele}"
REMOTE_BASE="${REMOTE_BASE:-/home/lele/redmik40-build}"
REMOTE_PANEL="${REMOTE_PANEL:-$REMOTE_BASE/panel}"
REMOTE_KNOWN_HOSTS="${REMOTE_KNOWN_HOSTS:-/tmp/redmik40_remote_known_hosts}"
PANEL_HOST="${PANEL_HOST:-127.0.0.1}"
PANEL_PORT="${PANEL_PORT:-18080}"
PANEL_BASIC_USER="${PANEL_BASIC_USER:-}"
PANEL_BASIC_PASSWORD_FILE="${PANEL_BASIC_PASSWORD_FILE:-}"
LOCAL_PANEL_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)/builder-panel"

SSH_OPTS=(-p "$REMOTE_PORT" -o StrictHostKeyChecking=no -o UserKnownHostsFile="$REMOTE_KNOWN_HOSTS")

ssh "${SSH_OPTS[@]}" "$REMOTE_USER@$REMOTE_HOST" "mkdir -p '$REMOTE_PANEL' '$REMOTE_BASE/logs'"
rsync -az --delete --exclude '.panel-password' -e "ssh -p $REMOTE_PORT -o StrictHostKeyChecking=no -o UserKnownHostsFile=$REMOTE_KNOWN_HOSTS" \
  "$LOCAL_PANEL_DIR/" "$REMOTE_USER@$REMOTE_HOST:$REMOTE_PANEL/"

ssh "${SSH_OPTS[@]}" "$REMOTE_USER@$REMOTE_HOST" "
  set -euo pipefail
  if [ -f '$REMOTE_BASE/logs/panel.pid' ]; then
    old_pid=\$(cat '$REMOTE_BASE/logs/panel.pid' || true)
    if [ -n \"\$old_pid\" ] && ps -p \"\$old_pid\" -o command= 2>/dev/null | grep -q 'builder-panel/app.py\\|panel/app.py'; then
      kill \"\$old_pid\"
      sleep 1
    fi
  fi
  for old_pid in \$(pgrep -f '$REMOTE_PANEL/app.py' || true); do
    if [ -n \"\$old_pid\" ] && [ \"\$old_pid\" != \"\$\$\" ]; then
      kill \"\$old_pid\" || true
    fi
  done
  for old_pid in \$(ss -ltnp 2>/dev/null | grep ':$PANEL_PORT ' | sed -n 's/.*pid=\\([0-9][0-9]*\\).*/\\1/p' | sort -u); do
    if [ -n \"\$old_pid\" ] && [ \"\$old_pid\" != \"\$\$\" ]; then
      kill \"\$old_pid\" || true
    fi
  done
  sleep 1
  cd '$REMOTE_PANEL'
  nohup env PANEL_HOST='$PANEL_HOST' PANEL_PORT='$PANEL_PORT' PANEL_BASE='$REMOTE_BASE' PANEL_DEFAULT_JOBS=36 \
    PANEL_BASIC_USER='$PANEL_BASIC_USER' PANEL_BASIC_PASSWORD_FILE='$PANEL_BASIC_PASSWORD_FILE' \
    python3 app.py >'$REMOTE_BASE/logs/panel.log' 2>&1 &
  echo \$! >'$REMOTE_BASE/logs/panel.pid'
  echo started-panel pid=\$(cat '$REMOTE_BASE/logs/panel.pid') url=http://$PANEL_HOST:$PANEL_PORT
"
