# Alioth GPU Panel UI Rebuild PRD

Status: draft v0.1

Date: 2026-05-13

Scope: Redmi K40 / POCO F3 (`alioth`) headless Linux local status panel.

## 1. Background

The alioth project is no longer trying to turn the phone screen into a general
Linux desktop. The screen is a fixed local panel for a headless Linux device:
similar to a router LCD, NAS panel, lab instrument HMI, or server front panel.

The current graphics foundation is materially better than the earlier CPU panel:

- DRM/KMS scanout is usable on the built-in 1080x2400 panel.
- Touch input is proven through evdev.
- KGSL userspace Vulkan through the private Mesa Turnip/Freedreno path is usable.
- The full panel can be rendered by Vulkan into DRM scanout buffers.
- Android font files can be reused through a Vulkan sampled glyph atlas.
- Monitor and GPU pages already have a first-pass `GPU UI runtime v1`.
- Power and FPS telemetry are visible on the top status line and GPU page.

The current implementation is still a transitional prototype. Most logic lives
in one C source file, page structure is inherited from the original CPU panel,
Wi-Fi and Power are not yet migrated to the new UI primitives, and the rendering
layer has only the minimal components required to prove the GPU path.

This PRD defines the product target for a full panel rebuild before deeper
implementation continues.

## 2. Product Goal

Build a touch-first, GPU-rendered local control panel for alioth headless Linux.

The panel should be reliable enough to keep as the default visible UI, polished
enough to show why GPU rendering matters, and practical enough to operate the
device without a desktop environment.

The intended product is not "Android on Linux" and not "a Linux desktop". It is
a purpose-built, full-screen, single-application device panel.

## 3. Users

Primary user:

- The device owner standing in front of the phone, using touch to inspect status,
  connect Wi-Fi, change screen/power settings, and run GPU/power experiments.

Secondary user:

- A remote developer connected by USB SSH or 2323 rescue shell, using the panel
  as live visual feedback while deploying and validating system changes.

## 4. Product Principles

Reliability first:

- The panel must not break SSH, USB/NCM, 2323 rescue, systemd, or boot recovery.
- Physical keys can remain as fallback, but visible UI should be touch-first.
- Failed probes/actions must be visible and recoverable.

Headless-first:

- Keep DRM/KMS/Vulkan panel support.
- Do not introduce Wayland desktop sessions, labwc, cage, Weston, or a general
  desktop stack as the default route.

Self-contained:

- Source, shaders, build helpers, runtime configs, screenshots, and docs should
  live in this repo or its artifact folders when practical.
- Remote build machines can be used for build speed, but the local project must
  keep enough metadata to reproduce what was deployed.

Glanceable:

- Home page must answer "is the device healthy?" within a few seconds.
- State should be shown in cards, gauges, rows, and color-coded badges, not long
  text blocks.

Honest telemetry:

- Power and thermal numbers must explain their source and uncertainty.
- Battery net power and USB input power are different signals and must not be
  presented as exact total SoC/GPU consumption.

GPU-visible:

- The panel should visibly demonstrate animation, smooth updates, richer color,
  anti-aliased text, graphs, and live controls.
- The GPU page should make FPS, frame time, KGSL busy, and power trend obvious.

## 5. Non-Goals

This rebuild does not aim to:

- Build a general-purpose Android replacement UI.
- Bring up SurfaceFlinger, Android HWUI, or Android app compatibility.
- Bring up a full Linux compositor or desktop session.
- Replace the current kernel/initramfs/rootfs boot architecture.
- Replace the private KGSL-enabled Turnip path with official Qualcomm desktop
  drivers.
- Solve CJK shaping, rich text layout, or complete font fallback in the first
  rebuild pass.
- Make battery/USB power telemetry laboratory-grade.

## 6. Current Foundations

Verified device/runtime foundations:

- Display: DRM/KMS direct scanout, double-buffered page flips.
- GPU: private Mesa Turnip/Freedreno over `/dev/kgsl-3d0`, FD650, Vulkan 1.2.
- Buffers: DRM-owned dumb buffers exported as PRIME fd and imported into KGSL
  Vulkan images.
- Text: Android font file loaded from `/system/fonts/Roboto-Regular.ttf` with
  `stb_truetype`, rendered as anti-aliased Vulkan glyph quads.
- Input: touch events from `/dev/input/event2` (`fts_ts`) are usable.
- Status paths: `/run/alioth-panel-page`, `/run/alioth-panel-screenshot.*`,
  `/run/alioth-panel-gpu-demo.txt`, `/run/alioth-panel-gpu-fps`.
- Rescue paths: USB/NCM, SSH, and 2323 shell remain important operational
  controls.

Current first UI runtime slice:

- Theme tokens.
- Panel/card primitives.
- Metric cards.
- Info rows.
- Touch chips/segmented controls.
- Modern tab bar.
- Rebuilt Monitor and GPU pages.

## 7. Information Architecture

The panel should converge on four primary tabs for the near-term product:

1. Monitor
2. Network
3. Power
4. GPU Lab

Optional later tabs:

- Services
- Logs
- Settings

The near-term four-tab layout keeps the interaction simple and fits the existing
navigation model.

### 7.1 Monitor

Purpose:

- Default home page.
- Show device health and primary entry points.

Required content:

- Battery capacity and battery net power.
- USB input power and charging status.
- Renderer status: Vulkan/CPU fallback, last FPS, KGSL summary.
- USB/NCM status and rescue address.
- Wi-Fi status, current SSID, route.
- System/kernel/rootfs summary.
- CPU, memory, disk, thermal summary.
- GPU/probe health.

Required interactions:

- Tap Network card -> Network page.
- Tap Power card or Battery card -> Power page.
- Tap Renderer/GPU card -> GPU Lab page.
- Bottom tab navigation.

### 7.2 Network

Purpose:

- Touch-first Wi-Fi and USB network panel.

Required content:

- USB/NCM status and IP.
- SSH and 2323 availability.
- Wi-Fi device state.
- Current SSID, signal, route, DNS.
- Scan results.
- Connection state and latest connect log tail.

Required interactions:

- Scan.
- Select SSID from result list.
- Enter SSID manually.
- Enter password using on-screen keyboard.
- Connect.
- Clear field/log.
- Preserve credentials handling without exposing secrets in screenshots/logs.

UX target:

- Should feel like a compact Android settings page, not a terminal form.

### 7.3 Power

Purpose:

- Battery, charging, backlight, thermal, and controlled power actions.

Required content:

- Battery capacity, status, temperature.
- Battery net power with sign.
- USB input power, voltage, current.
- Charging type and PD/fast-charge hints when available.
- Backlight state.
- Thermal summary.
- Safe action list.

Required interactions:

- Brightness presets or slider/steps.
- Screen mode: normal, low, night, lamp, off.
- Reboot.
- Reboot bootloader / fastboot.
- Power off.
- Dangerous actions require explicit touch confirmation.

UX target:

- No visible dependency on physical volume/power keys.
- Physical keys can remain as fallback outside the primary UI model.

### 7.4 GPU Lab

Purpose:

- Show GPU renderer status and measure FPS/power trends.

Required content:

- Current FPS and frame time.
- Target FPS.
- KGSL busy percentage and clock.
- USB input power and battery net power.
- Rolling power averages.
- Runtime status: KGSL node, DRM, ICD, helper, shaders.
- Last GPU probe result.
- Animated render demo.

Required interactions:

- 30/15/5 FPS presets.
- Probe.
- Clear probe log.
- Start/stop power benchmark.
- Export benchmark sample file.

UX target:

- This page should be the visual proof of GPU capability: animation, color,
  charts, and live response.

## 8. Functional Requirements

### Runtime and Rendering

- F-001: The panel must start as a systemd service and take over the display
  without requiring a desktop session.
- F-002: GPU rendering must remain the preferred path when Vulkan/KGSL init
  succeeds.
- F-003: CPU rendering or previous known-good binary must remain available as a
  rollback path.
- F-004: The panel must keep double-buffered KMS presentation to avoid visible
  touch-triggered flashing.
- F-005: Text rendering must use the Android font atlas path where available.
- F-006: UI primitives must support cards, rows, gauges, chips, nav tabs, modal
  confirmation, and simple animated graphs.
- F-007: Layout must be defined through reusable primitives, not page-specific
  ad hoc rectangle math everywhere.

### Input and Navigation

- F-101: Touch is the primary interaction path.
- F-102: Bottom tab navigation must be available on all primary pages.
- F-103: Page request file `/run/alioth-panel-page` must continue to work.
- F-104: Screenshot request files must continue to work for validation.
- F-105: Physical power/volume handling can remain for fallback, but should not
  be visible as the primary workflow.

### Telemetry

- F-201: Battery net power must be calculated from battery/bms
  `voltage_now * current_now`.
- F-202: USB input power must be calculated from USB/DC input voltage/current
  where available.
- F-203: Power display must label BAT and USB distinctly.
- F-204: GPU page must show instantaneous FPS/frame time and rolling average
  power readings.
- F-205: Telemetry should be cached by source-specific cadence to avoid reading
  sysfs/log files every animation frame.

### Wi-Fi

- F-301: Scan results must be readable and selectable by touch.
- F-302: The keyboard must be touch-first and sized for the phone screen.
- F-303: Password display must default to masked form.
- F-304: Connect progress and failure reason must be visible.
- F-305: Wi-Fi page must not remove USB/SSH/2323 rescue visibility.

### Power and Safety

- F-401: Reboot/poweroff/fastboot actions must require a confirmation screen.
- F-402: Confirmation must be touch-first.
- F-403: Backlight changes must be bounded and restorable.
- F-404: Any risky action must leave enough visual/log evidence for recovery.

## 9. Performance Requirements

Baseline:

- 30 FPS on GPU Lab page should remain stable.
- Frame time around 33 ms at 30 FPS should be visible and logged.
- Touch navigation should not cause panel flashing.
- Service restarts during normal page navigation should be zero.

Target:

- 60 FPS should be investigated after the UI split is stable.
- GPU page should support 30/15/5 FPS comparison with rolling power averages.
- UI should avoid per-frame blocking file I/O.

## 10. Visual Requirements

The panel should look like a modern device control surface:

- Large legible titles.
- Android-like font rendering.
- Touch targets large enough for fingers.
- Cards for high-level status.
- Rows for dense details.
- Color-coded health states.
- Charts and live animations where they add meaning.
- No visible instructional text explaining the UI itself unless there is a
  concrete operational warning.

The visual language should be quiet and technical, not a marketing landing page.

## 11. Observability Requirements

The panel must keep these validation surfaces:

- systemd service status.
- journal log messages for renderer init, page switches, touch events, probe
  actions, and errors.
- screenshot request output.
- GPU metrics file.
- power benchmark sample file once implemented.
- clear build artifact names and SHA256 in validation logs.

## 12. Acceptance Criteria

PRD-level rebuild completion means:

- All four primary pages use the UI runtime primitives.
- Monitor, Network, Power, and GPU Lab are touch-first.
- Wi-Fi scan/select/connect flow works at least as well as the legacy page.
- Power action confirmation is touch-first and safer than the legacy flow.
- GPU Lab can compare 30/15/5 FPS with FPS, KGSL busy, and rolling power data.
- Page request and screenshot workflows continue to pass.
- Service remains `active/running` with `NRestarts=0` after smoke validation.
- Rollback to the previous known-good binary is documented.

## 13. Risks

- The current code is monolithic; large edits risk regressions.
- USB SSH can become slow while the GPU page is busy or while screenshot BMPs
  are copied.
- Power readings are noisy when the phone is plugged and full.
- Full 60 FPS may require pipelined Vulkan synchronization and presentation
  changes, not just UI cleanup.
- Introducing a large UI library too early can add build/runtime risk before
  the custom direct-scanout path is stable.

## 14. Milestones

M1: Document-driven design

- PRD and system analysis written.
- Current baseline and acceptance criteria agreed.

M2: Runtime split without behavior change

- Extract UI, renderer, data, input, and page modules from the monolithic C file.
- Build and deploy a behavior-equivalent binary.

M3: Network page rebuild

- New Wi-Fi page built with runtime primitives.
- Touch scan/select/type/connect validated.

M4: Power page rebuild

- New Power page and confirmation flow.
- Backlight/screen mode controls validated.

M5: GPU power benchmark

- Rolling averages and sample export.
- 30/15/5 FPS benchmark workflow.

M6: Polish pass

- Visual consistency.
- Animation cleanup.
- Optional haptic feedback.
- Decide whether external UI/text/vector libraries are justified.
