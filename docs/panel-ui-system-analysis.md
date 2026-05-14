# Alioth GPU Panel System Analysis

Status: draft v0.1

Date: 2026-05-13

Related PRD: `docs/panel-ui-rebuild-prd.md`

## 1. Purpose

This document translates the PRD into an engineering plan for rebuilding the
alioth local panel.

The immediate engineering problem is not "can the GPU render?" anymore. That is
already proven. The problem is turning the current monolithic proof into a
maintainable UI application with predictable rendering, touch interaction,
telemetry, build/deploy, and validation boundaries.

## 2. Current Implementation Snapshot

Current main source:

```text
src/alioth-status-ui-c/alioth_status_ui.c
```

Current deployed binary:

```text
/data/experiments/alioth-status-ui-gpu-ui-runtime-v1
```

Current service override:

```text
/etc/systemd/system/lele-status-ui.service.d/10-gpu-first.conf
```

Current runtime artifacts:

```text
/data/experiments/gpu-runtime-20260513/
/data/experiments/alioth_panel_solid.vert.spv
/data/experiments/alioth_panel_solid.frag.spv
/data/experiments/alioth_panel_text.frag.spv
```

Important runtime request/status paths:

```text
/run/alioth-panel-page
/run/alioth-panel-screenshot.request
/run/alioth-panel-screenshot.bmp
/run/alioth-panel-screenshot.txt
/run/alioth-panel-gpu-demo.txt
/run/alioth-panel-gpu-fps
/run/alioth-panel-gpu-test.log
```

Current proven behaviors:

- DRM/KMS mode setup and page flip.
- Two scanout buffers.
- PRIME fd export from DRM dumb buffers.
- Vulkan import of DRM-owned buffers through KGSL Turnip.
- Solid rectangle pipeline.
- Text glyph atlas pipeline.
- Touch input handling.
- Page request and screenshot request handling.
- Monitor and GPU pages partially migrated to UI runtime primitives.

## 3. Main Problems To Solve

### 3.1 Source structure

The source is one large C file. It mixes:

- low-level DRM/KMS platform code
- Vulkan initialization and render pass code
- font loading and atlas baking
- telemetry collection
- Wi-Fi scan/connect UI state
- page drawing
- touch handling
- power actions
- screenshots
- service/probe helpers

This makes each visual change risky and makes page-by-page migration harder than
it needs to be.

### 3.2 Data collection cadence

Some data sources are cheap, some are expensive or noisy:

- Cheap: cached state variables, frame counters.
- Medium: sysfs one-line reads.
- Expensive/noisy: shell helpers, log scanning, Wi-Fi scan state, screenshot BMP
  copy, charger/power current.

The render loop must not block on all data sources every frame.

### 3.3 Interaction model

Touch is now the primary path, but some legacy text and logic still reflect a
physical button fallback model. Physical fallback is useful, but the product UI
should not be built around it.

### 3.4 Layout model

The current UI runtime has useful primitives, but page layout still uses many
hard-coded coordinates. This is acceptable for the first vertical slice, but
the next page migrations need stronger helpers to prevent repeated coordinate
math and text overlap.

### 3.5 Power measurement

Battery net power and USB input power are both useful but not equivalent.
Plugged/full operation makes battery-side readings especially misleading. The
GPU benchmark page needs rolling averages and clear labels.

## 4. Target Architecture

The target is still a single full-screen native process, but with internal
module boundaries.

Proposed source layout:

```text
src/alioth-panel/
  main.c
  app_state.h

  platform/
    drm_kms.c
    drm_kms.h
    input_evdev.c
    input_evdev.h
    screenshot.c
    screenshot.h

  renderer/
    gpu_vulkan.c
    gpu_vulkan.h
    cpu_fallback.c
    cpu_fallback.h
    font_atlas.c
    font_atlas.h
    canvas.h

  ui/
    ui_theme.c
    ui_theme.h
    ui_primitives.c
    ui_primitives.h
    ui_layout.c
    ui_layout.h
    ui_nav.c
    ui_nav.h

  data/
    data_system.c
    data_system.h
    data_power.c
    data_power.h
    data_network.c
    data_network.h
    data_gpu.c
    data_gpu.h
    data_services.c
    data_services.h

  pages/
    page_monitor.c
    page_network.c
    page_power.c
    page_gpu.c

  actions/
    action_wifi.c
    action_power.c
    action_gpu_probe.c
```

The first refactor can be less aggressive if needed, but the final structure
should have these boundaries.

## 5. Rendering Pipeline

Current pipeline:

```text
collect input/data
  -> draw page into canvas ops
  -> GPU renderer converts rect/glyph ops to Vulkan vertices
  -> render into back DRM scanout buffer
  -> KMS page flip
```

Keep this model.

Near-term improvements:

- Keep solid rect and text ops.
- Add runtime primitives instead of page-local drawing patterns.
- Add graph primitives using existing rect pipeline first.
- Add icon primitives either as small glyph/text symbols or a compact texture
  atlas later.

Do not introduce a generic scene graph yet. The UI is fixed-screen and the
current op list model is easier to reason about on this direct-scanout path.

## 6. Frame Scheduling

Current behavior:

- GPU page uses non-blocking input polling and page-flip pacing.
- Non-GPU pages update around once per second unless input wakes the loop.
- FPS target is 30/15/5 on GPU page.

Target behavior:

- GPU Lab: animated loop, target FPS selectable.
- Other pages: event-driven plus periodic telemetry refresh.
- Data refresh should be independent from render FPS.

Recommended cadence:

```text
Input events: every frame / poll wake
GPU frame metrics: every frame on GPU page
Clock/status line: 1s
CPU/load/memory/disk: 1s
Battery/USB power instantaneous: 1s
Power rolling averages: 1s sample, 10s/30s windows
KGSL busy/clock: 500ms on GPU page, 1s elsewhere
Wi-Fi scan results: on scan request or 2s while scanning
Logs/probe result: 500ms while probe running, 2s otherwise
```

## 7. Data Model

Introduce one top-level app state:

```text
struct app_state {
  enum app_page page;
  enum display_mode display_mode;
  struct input_state input;
  struct frame_metrics frame;
  struct system_snapshot system;
  struct power_snapshot power;
  struct network_snapshot network;
  struct gpu_snapshot gpu;
  struct wifi_ui_state wifi_ui;
  struct power_ui_state power_ui;
};
```

Each snapshot should carry:

- values
- source timestamp
- validity flag
- short error/status string when relevant

This allows pages to render stale-but-known data without blocking.

## 8. UI Runtime

Current primitives:

- theme
- outline
- page header
- panel
- metric card
- info row
- chip
- nav
- text width
- bar

Near-term additions:

- `ui_columns()` and `ui_stack()` layout helpers.
- `ui_status_badge()`.
- `ui_progress_card()`.
- `ui_list_row()`.
- `ui_text_field()`.
- `ui_keyboard()`.
- `ui_modal_confirm()`.
- `ui_sparkline()` or simple rolling graph.
- `ui_touch_target` registry.

The `ui_touch_target` registry is important. Instead of duplicating coordinates
in draw code and touch handlers, page draw functions should register targets:

```text
ui_hit_add(ctx, x, y, w, h, ACTION_NETWORK_SCAN, payload)
```

Then touch handling can dispatch actions through IDs.

This reduces a major source of future bugs: layout changes that forget to update
touch hit boxes.

## 9. Page Architecture

Each page should implement:

```text
void page_draw(struct ui_context *ui, struct app_state *app);
void page_handle_action(struct app_state *app, enum ui_action action, int payload);
void page_tick(struct app_state *app, int64_t now_ms);
```

Page modules should not open sysfs directly during draw. They should consume
snapshots from `data/*`.

### 9.1 Monitor

Already migrated in first pass.

Next work:

- Move draw function to `pages/page_monitor.c`.
- Replace direct coordinate touch rules with registered hit targets.
- Add health badges for Wi-Fi, USB, GPU, systemd, thermal.

### 9.2 Network

Most important next page.

Data sources:

- `wlan0` status
- `/run/alioth-net-status`
- `/run/alioth-wpa-status.log`
- `/run/alioth-wifi-state`
- scan result file
- scan/connect logs
- USB interface/SSH/2323 checks

Actions:

- scan
- select network
- focus SSID field
- focus password field
- keyboard input
- connect
- clear

Risks:

- Secret leakage in screenshot/logs.
- Keyboard layout consuming too much vertical space.
- Connect helper blocking UI if run synchronously.

Design decision:

- Keep helper execution asynchronous via request/log files.
- Render progress from logs.
- Mask password by default.

### 9.3 Power

Data sources:

- `/sys/class/power_supply/battery`
- `/sys/class/power_supply/bms`
- `/sys/class/power_supply/usb`
- `/sys/class/power_supply/main`
- `/sys/class/backlight/panel0-backlight`
- thermal zones

Actions:

- screen mode
- brightness preset/step
- reboot
- fastboot
- poweroff

Risks:

- Unsafe action triggering.
- Backlight set too low.
- Misleading power telemetry.

Design decision:

- Use modal confirmation for destructive actions.
- Clamp brightness changes.
- Keep clear labels for BAT net and USB input.

### 9.4 GPU Lab

Already migrated in first pass.

Next work:

- Rolling power averages.
- Benchmark start/stop.
- CSV-like sample output under `/run`.
- Visual graph for FPS/power/KGSL busy.
- Keep safe probe.

Proposed benchmark file:

```text
/run/alioth-panel-gpu-power-benchmark.csv
```

Columns:

```text
time_ms,target_fps,actual_fps,frame_ms,kgsl_busy,kgsl_clock_mhz,bat_w,usb_w
```

## 10. External Library Decision

Do not bring in a large UI library in the next immediate implementation step.

Reasoning:

- The current graphics path is custom DRM/KMS + KGSL/Vulkan direct scanout.
- Standard EGL/GBM/WSI/compositor paths are not proven.
- A large library can force assumptions about window systems, allocation, event
  loops, or text stacks that do not match this environment.

Near-term approach:

- Continue with a custom thin UI runtime.
- Use existing `stb_truetype` font rasterization.
- Keep shaders simple.
- Build enough components to migrate all pages.

Evaluate later:

- LVGL: good for embedded widgets, but backend/input integration must be proven.
- Nuklear: lightweight immediate-mode UI, possible source of ideas, but style
  and text quality may still need custom work.
- Skia: powerful text/vector stack, but build/runtime size and backend fit are
  high-risk.
- HarfBuzz/FreeType: useful if CJK/multilingual shaping becomes required.

## 11. Build And Deploy

Current build pattern:

```text
copy source/header/build script to remote temporary build dir
remote docker run redmik40-kernel-builder:bullseye
fetch binary to artifacts/remote/gpu-runtime-20260513/
scp binary to phone as .new
stop service
atomic replace
start service
verify systemd status and sha256
```

Keep this pattern until local Docker/Colima is known-good again.

Deployment rule:

- Never overwrite the running binary directly.
- Upload to `.new`.
- Stop service.
- Move `.new` into final path.
- Start service.
- Verify `MainPID`, `NRestarts`, `ExecMainStatus`, `ActiveState`, SHA256.

## 12. Testing Strategy

### 12.1 Build tests

- Cross build succeeds.
- Warnings reviewed.
- SHA256 recorded.

### 12.2 Service smoke tests

Commands:

```text
systemctl show lele-status-ui.service -p MainPID -p NRestarts -p ExecMainStatus -p ActiveState -p SubState
sha256sum /data/experiments/<binary>
```

Pass:

```text
ActiveState=active
SubState=running
ExecMainStatus=0
NRestarts=0
```

### 12.3 Screenshot tests

For every migrated page:

```text
printf <page> > /run/alioth-panel-page
sleep 2
touch /run/alioth-panel-screenshot.request
sleep 2
cat /run/alioth-panel-screenshot.txt
```

Then fetch BMP and convert to PNG locally.

Pass:

- Correct page metadata.
- Text legible.
- No obvious overlap.
- Touch targets match visual controls.
- Bottom nav state correct.

### 12.4 Touch tests

Manual or evdev-driven:

- Tap each tab.
- Tap cards that navigate to pages.
- Tap Wi-Fi scan.
- Tap scan result.
- Type SSID/password.
- Tap connect.
- Tap Power action and cancel.
- Tap Power action and confirm only for safe reboot path when explicitly chosen.
- Tap GPU FPS presets.

### 12.5 Telemetry tests

GPU:

- `30 FPS`: metrics show about 30 FPS / 33 ms.
- `15 FPS`: metrics show about 15 FPS.
- `5 FPS`: metrics show about 5 FPS.
- KGSL busy decreases with target FPS.

Power:

- BAT and USB values show separately.
- Rolling averages update.
- Benchmark file records samples.
- Documentation states that plugged/full readings are trend signals, not lab
  precision.

### 12.6 Regression tests

Must still work:

- USB SSH.
- 2323 fallback shell.
- page request file.
- screenshot request file.
- GPU probe.
- service restart.
- rollback binary.

## 13. Rollback

Every deployed panel binary must have:

- local artifact copy
- SHA256
- service override copy
- previous override backup on phone

Rollback options:

1. Restore previous service override backup and restart.
2. Point override to previous known-good binary.
3. Remove override to return to package/default service if appropriate.

Never remove rescue paths while testing panel UI.

## 14. Development Roadmap

Phase A: documentation baseline

- PRD.
- System analysis.
- Acceptance criteria.

Phase B: internal module split

- Move UI primitives out of the monolithic file.
- Move data providers out.
- Move page draw functions out.
- Keep behavior equivalent.

Phase C: Network page rebuild

- New touch list.
- New text fields.
- New keyboard.
- Scan/select/connect workflow.
- Screenshot and manual touch validation.

Phase D: Power page rebuild

- New power cards.
- Brightness/screen mode controls.
- Confirmation modal.
- Destructive action safeguards.

Phase E: GPU benchmark

- Rolling averages.
- Benchmark file.
- Power/FPS graph.
- Comparison table.

Phase F: polish and library decision

- Animation cleanup.
- Better icon/text assets.
- Haptic feedback if physically useful.
- Re-evaluate LVGL/HarfBuzz/Skia based on actual gaps.

## 15. Open Questions

- Should a fifth `Services` tab exist, or should service health stay inside
  Monitor?
- Should Wi-Fi password masking be always-on, or should a touch hold reveal it?
- Should GPU Lab support 60 FPS target before frame-in-flight optimization?
- Should benchmark data be persisted outside `/run`, or is runtime-only enough?
- How much CJK text support is required for the final panel?

## 16. Immediate Next Development Task

Implement Phase B in a small, behavior-preserving way:

1. Create `src/alioth-panel/` module structure.
2. Move UI primitives first.
3. Keep generated binary behavior equivalent to current v1.
4. Build/deploy as `alioth-status-ui-gpu-ui-runtime-v2-split`.
5. Validate Monitor/GPU screenshots and service stability.

Only after that should the Network page rebuild start.
