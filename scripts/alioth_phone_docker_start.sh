#!/bin/sh
set -eu

LOG_FILE="${LELE_DOCKER_LOG:-/run/dockerd.log}"
DATA_ROOT="${LELE_DOCKER_DATA_ROOT:-/var/lib/docker}"
IPTABLES_WRAPPER_DIR="${LELE_DOCKER_IPTABLES_DIR:-/run/lele-iptables-legacy}"

if [ -x /usr/local/sbin/lele-data-mount ]; then
  /usr/local/sbin/lele-data-mount >/run/lele-data-mount.last 2>&1 || {
    cat /run/lele-data-mount.last >&2
    exit 1
  }
fi

mkdir -p \
  "$DATA_ROOT" \
  /var/lib/containerd \
  /run/containerd \
  /run/docker \
  /sys/fs/cgroup \
  /dev/shm \
  /dev/mqueue \
  "$IPTABLES_WRAPPER_DIR"

mountpoint -q /sys/fs/cgroup || mount -t cgroup2 cgroup2 /sys/fs/cgroup
mountpoint -q /dev/shm || mount -t tmpfs -o mode=1777,nosuid,nodev shm /dev/shm
mountpoint -q /dev/mqueue || mount -t mqueue mqueue /dev/mqueue

sysctl -w net.ipv4.ip_forward=1 >/dev/null || true

if [ -x /usr/bin/iptables-legacy ]; then
  ln -sf /usr/bin/iptables-legacy "$IPTABLES_WRAPPER_DIR/iptables"
fi
if [ -x /usr/bin/ip6tables-legacy ]; then
  ln -sf /usr/bin/ip6tables-legacy "$IPTABLES_WRAPPER_DIR/ip6tables"
fi

pkill dockerd 2>/dev/null || true
pkill containerd 2>/dev/null || true
sleep 1

PATH="$IPTABLES_WRAPPER_DIR:/usr/local/sbin:/usr/local/bin:/usr/bin:/bin" \
nohup dockerd \
  --debug \
  --storage-driver=vfs \
  --exec-opt native.cgroupdriver=cgroupfs \
  --data-root "$DATA_ROOT" \
  >"$LOG_FILE" 2>&1 &

for _ in 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15; do
  if docker info >/run/docker-info.txt 2>/run/docker-info.err; then
    cat /run/docker-info.txt
    exit 0
  fi
  sleep 1
done

cat /run/docker-info.err 2>/dev/null || true
tail -120 "$LOG_FILE" 2>/dev/null || true
exit 1
