#!/bin/sh
set -eu

PATH=/usr/local/sbin:/usr/sbin:/usr/bin:/sbin:/bin

DEV="${ALIOTH_BT_DEV:-/dev/ttyHS0}"
PROBE="${ALIOTH_BT_PROBE:-/usr/local/sbin/alioth_qca_uart_probe}"
QCA_ATTACH="${ALIOTH_BT_QCA_ATTACH:-/usr/local/sbin/alioth_hciattach_qca}"
ATTACH_MODE="${ALIOTH_BT_ATTACH_MODE:-h4}"
PATCH="${ALIOTH_BT_PATCH:-/lib/firmware/qca/crbtfw20.tlv}"
NVM="${ALIOTH_BT_NVM:-/lib/firmware/qca/crnv20.bin}"
BDADDR="${ALIOTH_BT_BDADDR:-}"

log() {
	echo "alioth-bt-qca-init: $*"
}

find_bt_rfkill_state() {
	for rfk in /sys/class/rfkill/rfkill*; do
		[ -e "$rfk/name" ] || continue
		if [ "$(cat "$rfk/name" 2>/dev/null || true)" = "bt_power" ]; then
			echo "$rfk/state"
			return 0
		fi
	done
	return 1
}

ensure_nodes() {
	if [ ! -c /dev/rfkill ]; then
		mknod /dev/rfkill c 10 242
	fi

	bt_major="$(awk '$2=="bt"{print $1}' /proc/devices | head -n 1)"
	if [ -n "$bt_major" ] && [ ! -c /dev/btpower ]; then
		mknod /dev/btpower c "$bt_major" 0
	fi

	ttyhs_major="$(awk '$2=="ttyHS"{print $1}' /proc/devices | head -n 1)"
	if [ -n "$ttyhs_major" ] && [ ! -c "$DEV" ]; then
		mknod "$DEV" c "$ttyhs_major" 0
	fi
}

require_inputs() {
	[ -x "$PROBE" ] || {
		log "missing executable probe: $PROBE"
		exit 1
	}
	[ -c "$DEV" ] || {
		log "missing UART device: $DEV"
		exit 1
	}
	[ -r "$PATCH" ] || {
		log "missing patch firmware: $PATCH"
		exit 1
	}
	[ -r "$NVM" ] || {
		log "missing NVM firmware: $NVM"
		exit 1
	}
}

run_probe() {
	if [ -n "$BDADDR" ]; then
		log "using temporary NVM BD_ADDR override: $BDADDR"
		"$PROBE" \
			--dev "$DEV" \
			--quiet-tlv \
			--pulse \
			--flow \
			--switch-oper 3000000 \
			--nvm-mode h4 \
			--bdaddr "$BDADDR" \
			--patch "$PATCH" \
			--nvm "$NVM" \
			--no-reset \
			--no-post-fw-probe
	else
		"$PROBE" \
			--dev "$DEV" \
			--quiet-tlv \
			--pulse \
			--flow \
			--switch-oper 3000000 \
			--nvm-mode h4 \
			--patch "$PATCH" \
			--nvm "$NVM" \
			--no-reset \
			--no-post-fw-probe
	fi
}

ensure_nodes
require_inputs

rfkill_state="$(find_bt_rfkill_state || true)"
[ -n "$rfkill_state" ] || {
	log "could not find bt_power rfkill"
	exit 1
}

for p in $(pgrep -f '^/usr/local/sbin/alioth_hciattach_qca ' 2>/dev/null || true); do
	kill -9 "$p" 2>/dev/null || true
done
for p in $(pgrep -f '^hciattach ' 2>/dev/null || true); do
	kill -9 "$p" 2>/dev/null || true
done

log "power cycling bluetooth"
echo 0 > "$rfkill_state" 2>/dev/null || true
sleep 1
timeout 2 stty -F "$DEV" 115200 raw -echo -crtscts 2>/dev/null || true
echo 1 > "$rfkill_state"
sleep 1

log "loading QCA firmware and NVM"
run_probe

case "$ATTACH_MODE" in
qca)
	[ -x "$QCA_ATTACH" ] || {
		log "missing executable QCA attach helper: $QCA_ATTACH"
		exit 1
	}
	log "attaching QCA HCI UART line discipline"
	exec "$QCA_ATTACH" --dev "$DEV" --speed 3000000 --flow
	;;
h4)
	log "attaching generic H4 HCI UART line discipline"
	timeout 2 stty -F "$DEV" 3000000 raw -echo crtscts 2>/dev/null || true
	exec hciattach -n "$DEV" any 3000000 flow
	;;
*)
	log "unsupported attach mode: $ATTACH_MODE"
	exit 2
	;;
esac
