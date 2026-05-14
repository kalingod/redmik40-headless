#ifndef ALIOTH_PANEL_PATHS_H
#define ALIOTH_PANEL_PATHS_H

#define POWER_DOUBLE_MS 650
#define POWER_MENU_HOLD_MS 1200
#define SHUTDOWN_CONFIRM_MS 10000

#define WIFI_SSID_MAX 96
#define WIFI_PSK_MAX 128
#define WIFI_SCAN_MAX_UNIQUE 48
#define WIFI_SAVED_MAX 16

#define NAV_HEIGHT 150

#define WIFI_SCAN_PATH "/run/alioth-panel-wifi-scan"
#define WIFI_SCAN_LOG "/run/alioth-panel-wifi-scan.log"
#define WIFI_CONNECT_LOG "/run/alioth-panel-wifi-connect.log"
#define WIFI_SCROLL_REQUEST_PATH "/run/alioth-panel-wifi-scroll"
#define WIFI_CONFIG_PATH "/etc/alioth-wifi-default"
#define WIFI_CONFIG_TMP_PATH "/etc/alioth-wifi-default.tmp"

#define SCREENSHOT_REQUEST_PATH "/run/alioth-panel-screenshot.request"
#define SCREENSHOT_BMP_PATH "/run/alioth-panel-screenshot.bmp"
#define SCREENSHOT_INFO_PATH "/run/alioth-panel-screenshot.txt"
#define PAGE_REQUEST_PATH "/run/alioth-panel-page"

#define BACKLIGHT_DIR "/sys/class/backlight/panel0-backlight"
#define BACKLIGHT_BRIGHTNESS_PATH BACKLIGHT_DIR "/brightness"
#define BACKLIGHT_ACTUAL_PATH BACKLIGHT_DIR "/actual_brightness"
#define BACKLIGHT_MAX_PATH BACKLIGHT_DIR "/max_brightness"
#define BACKLIGHT_POWER_PATH BACKLIGHT_DIR "/bl_power"

#define GPU_TEST_LOG "/run/alioth-panel-gpu-test.log"
#define GPU_METRICS_PATH "/run/alioth-panel-gpu-metrics.txt"
#define GPU_TEST_HELPER "/data/experiments/gpu-runtime-20260513/alioth_gpu_monitor_service"
#define GPU_TEST_HELPER_ALT "/data/experiments/alioth_gpu_monitor_service"
#define GPU_TEST_ICD "/data/experiments/gpu-runtime-20260513/freedreno_icd-kgsl-mesa2034-aarch64.json"
#define GPU_TEST_ICD_ALT "/data/experiments/freedreno_icd-kgsl-mesa2034-builddir.json"
#define GPU_TEST_LD_LIBRARY_PATH "/data/experiments/gpu-runtime-20260513/lib:/data/experiments/mesa-kgsl-prefix/lib/aarch64-linux-gnu"
#define GPU_PANEL_VERT_SPV "/data/experiments/alioth_panel_solid.vert.spv"
#define GPU_PANEL_FRAG_SPV "/data/experiments/alioth_panel_solid.frag.spv"
#define GPU_PANEL_TEXT_FRAG_SPV "/data/experiments/alioth_panel_text.frag.spv"

#define GPU_FONT_FIRST 32
#define GPU_FONT_COUNT 95
#define GPU_FONT_MAX_SCALE 8

#endif
