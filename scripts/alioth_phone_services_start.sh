#!/bin/sh
set -eu

log() {
  printf '[lele-services] %s\n' "$*"
}

ensure_group() {
  name="$1"
  if ! getent group "$name" >/dev/null 2>&1; then
    groupadd --system "$name"
  fi
}

ensure_user() {
  name="$1"
  home="$2"
  shell="$3"
  ensure_group "$name"
  if ! getent passwd "$name" >/dev/null 2>&1; then
    useradd --system --gid "$name" --home-dir "$home" --shell "$shell" "$name"
  fi
}

run_as() {
  user="$1"
  shift
  runuser -u "$user" -- "$@"
}

ensure_data_mount() {
  if [ -x /usr/local/sbin/lele-data-mount ]; then
    /usr/local/sbin/lele-data-mount >/run/lele-data-mount.last 2>&1 || {
      cat /run/lele-data-mount.last >&2
      exit 1
    }
  fi
}

ensure_users_and_dirs() {
  getent group 3003 >/dev/null 2>&1 || groupadd -g 3003 inet
  getent group 3004 >/dev/null 2>&1 || groupadd -g 3004 net_raw

  getent passwd http >/dev/null 2>&1 || {
    ensure_group http
    useradd --system --gid http --home-dir /srv/http --shell /usr/bin/nologin http
  }
  ensure_user valkey /var/lib/valkey /usr/bin/nologin
  ensure_user postgres /var/lib/postgres /usr/bin/bash
  usermod -aG inet valkey
  usermod -aG inet postgres

  mkdir -p /srv/http /var/lib/valkey /etc/valkey /var/lib/postgres/data /run /run/postgresql
  chown http:http /srv/http
  chown valkey:valkey /var/lib/valkey
  chown postgres:postgres /var/lib/postgres /var/lib/postgres/data /run/postgresql
  chmod 700 /var/lib/valkey /var/lib/postgres /var/lib/postgres/data
  chmod 775 /run/postgresql
  touch /var/log/valkey.log
  chown valkey:valkey /var/log/valkey.log

  if [ ! -s /srv/http/index.html ]; then
    cat >/srv/http/index.html <<'HTML'
<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>LELE OS</title>
  <style>
    body { margin: 0; font-family: system-ui, sans-serif; background: #101418; color: #eef3f8; }
    main { max-width: 760px; margin: 0 auto; padding: 56px 24px; }
    h1 { margin: 0 0 12px; font-size: 42px; letter-spacing: 0; }
    p { color: #b8c4ce; line-height: 1.6; }
    code { color: #8bd8ff; }
  </style>
</head>
<body>
  <main>
    <h1>LELE OS</h1>
    <p>Redmi K40 bare-metal Linux server is online.</p>
    <p>USB SSH: <code>172.16.42.2</code> · Docker, nginx, Valkey and PostgreSQL are installed.</p>
  </main>
</body>
</html>
HTML
    chown http:http /srv/http/index.html
  fi

  if [ -f /etc/nginx/nginx.conf ] && grep -q '/usr/share/nginx/html' /etc/nginx/nginx.conf; then
    cp -n /etc/nginx/nginx.conf /etc/nginx/nginx.conf.pre-lele 2>/dev/null || true
    sed -i 's#/usr/share/nginx/html#/srv/http#g' /etc/nginx/nginx.conf
  fi
}

start_docker() {
  if docker info >/dev/null 2>&1; then
    log "docker already running"
    return
  fi
  if [ -x /usr/local/sbin/lele-docker-start ]; then
    log "starting docker"
    /usr/local/sbin/lele-docker-start >/run/lele-docker-start.last 2>&1
  else
    log "docker helper missing: /usr/local/sbin/lele-docker-start"
  fi
}

start_nginx() {
  if pgrep -x nginx >/dev/null 2>&1; then
    log "nginx already running, reloading config"
    nginx -t
    nginx -s reload
    return
  fi
  log "starting nginx"
  nginx -t
  nginx >/run/nginx-start.log 2>&1
}

start_valkey() {
  if pgrep -x valkey-server >/dev/null 2>&1; then
    log "valkey already running"
    return
  fi
  log "starting valkey"
  sysctl -w vm.overcommit_memory=1 >/dev/null || true
  run_as valkey valkey-server /etc/valkey/valkey.conf \
    --daemonize yes \
    --dir /var/lib/valkey \
    --pidfile /run/valkey.pid \
    --logfile /var/log/valkey.log \
    >/run/valkey-start.log 2>&1
}

start_postgres() {
  if [ ! -s /var/lib/postgres/data/PG_VERSION ]; then
    log "initializing postgresql data directory"
    run_as postgres initdb -D /var/lib/postgres/data --locale=C --encoding=UTF8
  fi
  if pg_isready -q >/dev/null 2>&1; then
    log "postgresql already running"
    return
  fi
  log "starting postgresql"
  run_as postgres pg_ctl -D /var/lib/postgres/data -l /var/lib/postgres/logfile -w start
}

print_status() {
  log "status"
  uname -a
  df -h /
  docker info --format 'docker={{.ServerVersion}} driver={{.Driver}} cgroup={{.CgroupVersion}} containers={{.Containers}}' 2>/dev/null || true
  docker compose version 2>/dev/null || true
  nginx -v 2>&1 || true
  valkey-cli ping 2>/dev/null || true
  pg_isready 2>/dev/null || true
  curl -fsS http://127.0.0.1/ >/dev/null && printf 'nginx=http-ok\n' || true
}

ensure_data_mount
ensure_users_and_dirs
start_docker
start_nginx
start_valkey
start_postgres
print_status
