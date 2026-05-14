#ifndef ALIOTH_PANEL_APP_TYPES_H
#define ALIOTH_PANEL_APP_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#include "alioth-panel/panel_paths.h"

struct cpu_sample {
	bool valid;
	int cores;
	double total_pct;
	double core_pct[16];
	uint64_t prev_total[17];
	uint64_t prev_idle[17];
};

enum display_mode {
	DISPLAY_NORMAL = 0,
	DISPLAY_LOW,
	DISPLAY_NIGHT,
	DISPLAY_LAMP,
	DISPLAY_OFF,
};

enum ui_screen {
	UI_STATUS = 0,
	UI_POWER_MENU,
	UI_CONFIRM_ACTION,
};

enum app_page {
	PAGE_MONITOR = 0,
	PAGE_WIFI,
	PAGE_POWER,
	PAGE_GPU,
	PAGE_LOGS,
	PAGE_COUNT,
};

enum power_action {
	POWER_BACK = 0,
	POWER_OFF,
	POWER_REBOOT,
	POWER_FASTBOOT,
	POWER_ACTION_COUNT,
};

enum wifi_field {
	WIFI_FIELD_SSID = 0,
	WIFI_FIELD_PSK,
};

enum keyboard_layout {
	KB_ALPHA = 0,
	KB_SYMBOL,
};

struct input_state {
	int fds[16];
	char names[16][128];
	bool is_touch[16];
	int count;
	enum display_mode mode;
	enum display_mode previous_lit_mode;
	bool power_pending;
	int64_t power_deadline_ms;
	int64_t last_power_ms;
	bool power_pressed;
	int64_t power_pressed_ms;
	bool shutdown_confirm;
	int64_t shutdown_deadline_ms;
	bool touch_present;
	int touch_min_x;
	int touch_max_x;
	int touch_min_y;
	int touch_max_y;
	int touch_slot;
	bool touch_finger_down;
	bool touch_reported;
	bool touch_saw_x;
	bool touch_saw_y;
	int touch_x;
	int touch_y;
	int touch_start_x;
	int touch_start_y;
};

struct wifi_ui_state {
	bool initialized;
	char ssid[WIFI_SSID_MAX];
	char psk[WIFI_PSK_MAX];
	enum wifi_field field;
	enum keyboard_layout layout;
	bool keyboard_visible;
	bool shift;
	int scan_scroll;
	int scan_scroll_px;
	bool scan_drag_active;
	int scan_drag_last_y;
	int64_t scan_started_ms;
	int64_t connect_started_ms;
	char message[160];
};

struct wifi_scan_entry {
	char ssid[WIFI_SSID_MAX];
	char meta[64];
	char freq[16];
	double best_signal;
	int ap_count;
};

struct gpu_frame_metrics {
	int64_t last_frame_ms;
	int64_t window_ms;
	int frames;
	int fps;
	int frame_ms;
};

struct gpu_page_cache {
	int64_t last_update_ms;
	char nodes[128];
	char runtime[160];
	char kgsl[160];
	char drm[128];
	char probe[160];
	char device[160];
	char submit[160];
	char verify[160];
	char ext[160];
	char busy_clock[96];
};

struct panel_data_snapshot {
	char kernel[160];
	char os[160];
	char title[80];
	char mode[80];
	char uptime[80];
	char rootfs[160];
	char drm[128];
	char usb[128];
	char wifi[128];
	char wifi_mode[160];
	char ap_clients[128];
	char ssid[128];
	char route[128];
	char dns[128];
	char battery[160];
	char charge[128];
	char memory[128];
	char thermal[128];
	char cpus[64];
	char load[128];
	char procs[80];
	char disk[128];
	char swap[80];
	char docker[128];
	char services_value[32];
	char services_detail[160];
	char cpu_all[80];
	char cpu_top[96];
	char cpu_top_detail[160];
	char cpu_governor[96];
	char cpu_freq[128];
	char display[96];
	char battery_value[32];
	char battery_detail[80];
	char usb_value[32];
	char usb_detail[80];
	char renderer_detail[80];
	char kgsl[160];
	char gpu_probe[160];
	double memory_pct;
	double disk_pct;
	double battery_pct;
	int battery_capacity;
	int failed_units;
};

#endif
