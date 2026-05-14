#ifndef ALIOTH_PANEL_DATA_H
#define ALIOTH_PANEL_DATA_H

#include <stdio.h>
#include <string.h>

static void panel_data_refresh(struct panel_data_snapshot *s, const struct cpu_sample *cpu)
{
	double power_w = 0;
	double power_v = 0;
	double power_ma = 0;

	memset(s, 0, sizeof(*s));
	snprintf(s->battery_value, sizeof(s->battery_value), "?");
	snprintf(s->battery_detail, sizeof(s->battery_detail), "battery telemetry pending");
	snprintf(s->usb_value, sizeof(s->usb_value), "USB ?");
	snprintf(s->usb_detail, sizeof(s->usb_detail), "input telemetry pending");
	s->battery_capacity = -1;
	s->battery_pct = -1.0;

	os_line(s->os, sizeof(s->os));
	distro_title_line(s->title, sizeof(s->title));
	kernel_line(s->kernel, sizeof(s->kernel));
	boot_mode_line(s->mode, sizeof(s->mode));
	uptime_line(s->uptime, sizeof(s->uptime));
	rootfs_line(s->rootfs, sizeof(s->rootfs));
	drm_line(s->drm, sizeof(s->drm));
	iface_line("usb0", 2323, s->usb, sizeof(s->usb));
	iface_line("wlan0", 0, s->wifi, sizeof(s->wifi));
	wifi_mode_line(s->wifi_mode, sizeof(s->wifi_mode));
	ap_client_line(s->ap_clients, sizeof(s->ap_clients));
	ssid_line(s->ssid, sizeof(s->ssid));
	default_route_line(s->route, sizeof(s->route));
	dns_line(s->dns, sizeof(s->dns));
	battery_line(s->battery, sizeof(s->battery));
	charge_speed_line(s->charge, sizeof(s->charge));
	display_line(s->display, sizeof(s->display));
	mem_line(s->memory, sizeof(s->memory), &s->memory_pct);
	swap_line(s->swap, sizeof(s->swap));
	load_line(s->load, sizeof(s->load));
	process_line(s->procs, sizeof(s->procs));
	disk_line(s->disk, sizeof(s->disk), &s->disk_pct);
	docker_line(s->docker, sizeof(s->docker));
	service_health_line(s->services_value, sizeof(s->services_value),
		s->services_detail, sizeof(s->services_detail), &s->failed_units);
	thermal_line(s->thermal, sizeof(s->thermal));
	if (!read_file("/sys/devices/system/cpu/online", s->cpus, sizeof(s->cpus)))
		snprintf(s->cpus, sizeof(s->cpus), "unknown");
	snprintf(s->cpu_all, sizeof(s->cpu_all), "all %.0f%% online %s", cpu->total_pct, s->cpus);
	cpu_hot_thread_line(s->cpu_top, sizeof(s->cpu_top),
		s->cpu_top_detail, sizeof(s->cpu_top_detail));
	cpu_governor_line(s->cpu_governor, sizeof(s->cpu_governor));
	cpu_frequency_line(s->cpu_freq, sizeof(s->cpu_freq));

	s->battery_capacity = battery_capacity_value();
	if (s->battery_capacity >= 0) {
		snprintf(s->battery_value, sizeof(s->battery_value), "%d%%", s->battery_capacity);
		s->battery_pct = (double)s->battery_capacity / 100.0;
	}
	if (battery_net_power(&power_w, &power_v, &power_ma)) {
		char status[32] = "";
		battery_status_value(status, sizeof(status));
		snprintf(s->battery_detail, sizeof(s->battery_detail), "%s %+.2fW %.0fmA",
			status[0] ? status : "net", power_w, power_ma);
	}
	if (usb_input_power(&power_w, &power_v, &power_ma)) {
		snprintf(s->usb_value, sizeof(s->usb_value), "%.2fW", power_w);
		snprintf(s->usb_detail, sizeof(s->usb_detail), "%.2fV  %.0fmA input",
			power_v, power_ma);
	}

	update_gpu_page_cache();
	snprintf(s->renderer_detail, sizeof(s->renderer_detail), "FD650 / direct scanout");
	snprintf(s->kgsl, sizeof(s->kgsl), "%s", g_gpu_page_cache.kgsl);
	snprintf(s->gpu_probe, sizeof(s->gpu_probe), "%s", g_gpu_page_cache.probe);
}

#endif
