#!/usr/bin/env bash
set -u

INTERVAL_SEC="${ALIOTH_THERMAL_GUARD_INTERVAL_SEC:-5}"
STATUS_FILE="${ALIOTH_THERMAL_GUARD_STATUS_FILE:-/run/alioth-thermal-guard.status}"
STATE_FILE="${ALIOTH_THERMAL_GUARD_STATE_FILE:-/run/alioth-thermal-guard.state}"
LOG_FILE="${ALIOTH_THERMAL_GUARD_LOG_FILE:-/var/log/alioth-thermal-guard.log}"

WARM_BATT_MC="${ALIOTH_THERMAL_GUARD_WARM_BATT_MC:-40000}"
WARM_CPU_MC="${ALIOTH_THERMAL_GUARD_WARM_CPU_MC:-60000}"
WARM_GPU_MC="${ALIOTH_THERMAL_GUARD_WARM_GPU_MC:-60000}"
WARM_PMIC_MC="${ALIOTH_THERMAL_GUARD_WARM_PMIC_MC:-65000}"

HOT_BATT_MC="${ALIOTH_THERMAL_GUARD_HOT_BATT_MC:-42000}"
HOT_CPU_MC="${ALIOTH_THERMAL_GUARD_HOT_CPU_MC:-70000}"
HOT_GPU_MC="${ALIOTH_THERMAL_GUARD_HOT_GPU_MC:-70000}"
HOT_PMIC_MC="${ALIOTH_THERMAL_GUARD_HOT_PMIC_MC:-80000}"

CRIT_BATT_MC="${ALIOTH_THERMAL_GUARD_CRIT_BATT_MC:-48000}"
CRIT_CPU_MC="${ALIOTH_THERMAL_GUARD_CRIT_CPU_MC:-85000}"
CRIT_GPU_MC="${ALIOTH_THERMAL_GUARD_CRIT_GPU_MC:-85000}"
CRIT_PMIC_MC="${ALIOTH_THERMAL_GUARD_CRIT_PMIC_MC:-95000}"

SHUTDOWN_BATT_MC="${ALIOTH_THERMAL_GUARD_SHUTDOWN_BATT_MC:-55000}"
SHUTDOWN_CPU_MC="${ALIOTH_THERMAL_GUARD_SHUTDOWN_CPU_MC:-105000}"
SHUTDOWN_GPU_MC="${ALIOTH_THERMAL_GUARD_SHUTDOWN_GPU_MC:-105000}"
SHUTDOWN_PMIC_MC="${ALIOTH_THERMAL_GUARD_SHUTDOWN_PMIC_MC:-115000}"
POWEROFF_ON_SHUTDOWN="${ALIOTH_THERMAL_GUARD_POWEROFF:-1}"

log_msg() {
	local line
	line="$(date -Is) $*"
	printf '%s\n' "$line" >> "$LOG_FILE" 2>/dev/null || true
	command -v systemd-cat >/dev/null 2>&1 &&
		printf '%s\n' "$*" | systemd-cat -t alioth-thermal-guard -p warning || true
}

thermal_mC() {
	local wanted="$1" z type val
	for z in /sys/class/thermal/thermal_zone*; do
		[ -r "$z/type" ] || continue
		type="$(cat "$z/type" 2>/dev/null || true)"
		[ "$type" = "$wanted" ] || continue
		val="$(cat "$z/temp" 2>/dev/null || true)"
		case "$val" in
			''|*[!0-9-]*) return 1 ;;
		esac
		printf '%s\n' "$val"
		return 0
	done
	return 1
}

power_supply_temp_mC() {
	local supply="$1" val
	[ -r "/sys/class/power_supply/$supply/temp" ] || return 1
	val="$(cat "/sys/class/power_supply/$supply/temp" 2>/dev/null || true)"
	case "$val" in
		''|*[!0-9-]*) return 1 ;;
	esac
	# Android battery temp is normally deci-Celsius; thermal zones are mC.
	if [ "$val" -lt 1000 ] && [ "$val" -gt -1000 ]; then
		printf '%s\n' "$((val * 100))"
	else
		printf '%s\n' "$val"
	fi
}

max_of() {
	local max=-999999 val
	for val in "$@"; do
		[ -n "$val" ] || continue
		[ "$val" -gt "$max" ] && max="$val"
	done
	printf '%s\n' "$max"
}

fmt_c() {
	local val="$1"
	if [ -z "$val" ] || [ "$val" -le -999000 ]; then
		printf 'n/a'
	else
		awk -v v="$val" 'BEGIN { printf "%.1fC", v / 1000.0 }'
	fi
}

set_governor() {
	local governor="$1" g dir changed=0
	for g in /sys/devices/system/cpu/cpufreq/policy*/scaling_governor; do
		[ -w "$g" ] || continue
		dir="${g%/*}"
		grep -qw "$governor" "$dir/scaling_available_governors" 2>/dev/null || continue
		printf '%s\n' "$governor" > "$g" 2>/dev/null || continue
		changed=$((changed + 1))
	done
	printf '%s\n' "$changed"
}

read_prev_action() {
	[ -r "$STATE_FILE" ] && cat "$STATE_FILE" 2>/dev/null || printf 'unknown\n'
}

write_action() {
	printf '%s\n' "$1" > "$STATE_FILE" 2>/dev/null || true
}

while :; do
	batt="$(max_of "$(power_supply_temp_mC battery || true)" "$(thermal_mC battery || true)" "$(thermal_mC bms || true)")"
	cpu="$(max_of "$(thermal_mC cpu_therm || true)" "$(thermal_mC cpu-1-0-step || true)" "$(thermal_mC cpu-1-7-step || true)")"
	gpu="$(max_of "$(thermal_mC gpuss-0-usr || true)" "$(thermal_mC gpuss-1-usr || true)" "$(thermal_mC gpuss-max-step || true)")"
	pmic="$(max_of "$(thermal_mC pm8150_tz || true)" "$(thermal_mC pm8150b_tz || true)" "$(thermal_mC pm8150l_tz || true)")"
	wifi="$(thermal_mC wifi_therm || true)"
	load="$(cut -d' ' -f1-3 /proc/loadavg 2>/dev/null || true)"

	action="normal"
	reason="within_limits"
	if [ "$batt" -ge "$SHUTDOWN_BATT_MC" ] || [ "$cpu" -ge "$SHUTDOWN_CPU_MC" ] ||
		[ "$gpu" -ge "$SHUTDOWN_GPU_MC" ] || [ "$pmic" -ge "$SHUTDOWN_PMIC_MC" ]; then
		action="shutdown"
		reason="shutdown_threshold"
	elif [ "$batt" -ge "$CRIT_BATT_MC" ] || [ "$cpu" -ge "$CRIT_CPU_MC" ] ||
		[ "$gpu" -ge "$CRIT_GPU_MC" ] || [ "$pmic" -ge "$CRIT_PMIC_MC" ]; then
		action="critical"
		reason="critical_threshold"
	elif [ "$batt" -ge "$HOT_BATT_MC" ] || [ "$cpu" -ge "$HOT_CPU_MC" ] ||
		[ "$gpu" -ge "$HOT_GPU_MC" ] || [ "$pmic" -ge "$HOT_PMIC_MC" ]; then
		action="hot"
		reason="hot_threshold"
	elif [ "$batt" -ge "$WARM_BATT_MC" ] || [ "$cpu" -ge "$WARM_CPU_MC" ] ||
		[ "$gpu" -ge "$WARM_GPU_MC" ] || [ "$pmic" -ge "$WARM_PMIC_MC" ]; then
		action="warm"
		reason="warm_threshold"
	fi

	prev="$(read_prev_action)"
	case "$action" in
		warm)
			[ "$prev" != "$action" ] && changed="$(set_governor schedutil)" &&
				log_msg "warm throttle: governor=schedutil changed=$changed"
			;;
		hot|critical|shutdown)
			[ "$prev" != "$action" ] && changed="$(set_governor powersave)" &&
				log_msg "$action throttle: governor=powersave changed=$changed"
			;;
		normal)
			case "$prev" in
				warm|hot|critical|shutdown)
					changed="$(set_governor schedutil)"
					log_msg "thermal recovered: governor=schedutil changed=$changed"
					;;
			esac
			;;
	esac
	write_action "$action"

	{
		printf 'ts=%s\n' "$(date -Is)"
		printf 'action=%s\nreason=%s\nload=%s\n' "$action" "$reason" "$load"
		printf 'battery=%s\ncpu=%s\ngpu=%s\npmic=%s\nwifi=%s\n' \
			"$(fmt_c "$batt")" "$(fmt_c "$cpu")" "$(fmt_c "$gpu")" \
			"$(fmt_c "$pmic")" "$(fmt_c "${wifi:--999999}")"
		printf 'thresholds=warm batt %.1f cpu %.1f gpu %.1f pmic %.1f; hot batt %.1f cpu %.1f gpu %.1f pmic %.1f; shutdown batt %.1f cpu %.1f gpu %.1f pmic %.1f\n' \
			"$(awk -v v="$WARM_BATT_MC" 'BEGIN{print v/1000}')" \
			"$(awk -v v="$WARM_CPU_MC" 'BEGIN{print v/1000}')" \
			"$(awk -v v="$WARM_GPU_MC" 'BEGIN{print v/1000}')" \
			"$(awk -v v="$WARM_PMIC_MC" 'BEGIN{print v/1000}')" \
			"$(awk -v v="$HOT_BATT_MC" 'BEGIN{print v/1000}')" \
			"$(awk -v v="$HOT_CPU_MC" 'BEGIN{print v/1000}')" \
			"$(awk -v v="$HOT_GPU_MC" 'BEGIN{print v/1000}')" \
			"$(awk -v v="$HOT_PMIC_MC" 'BEGIN{print v/1000}')" \
			"$(awk -v v="$SHUTDOWN_BATT_MC" 'BEGIN{print v/1000}')" \
			"$(awk -v v="$SHUTDOWN_CPU_MC" 'BEGIN{print v/1000}')" \
			"$(awk -v v="$SHUTDOWN_GPU_MC" 'BEGIN{print v/1000}')" \
			"$(awk -v v="$SHUTDOWN_PMIC_MC" 'BEGIN{print v/1000}')"
	} > "$STATUS_FILE".tmp 2>/dev/null && mv "$STATUS_FILE".tmp "$STATUS_FILE" 2>/dev/null || true

	if [ "$action" = "shutdown" ]; then
		log_msg "shutdown threshold reached: battery=$(fmt_c "$batt") cpu=$(fmt_c "$cpu") gpu=$(fmt_c "$gpu") pmic=$(fmt_c "$pmic") poweroff=$POWEROFF_ON_SHUTDOWN"
		if [ "$POWEROFF_ON_SHUTDOWN" = "1" ]; then
			systemctl poweroff
			sleep 60
		fi
	fi

	sleep "$INTERVAL_SEC"
done
