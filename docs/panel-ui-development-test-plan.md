# Alioth GPU Panel Development And Test Plan

Status: draft v0.3

Date: 2026-05-14

Related documents:

- `docs/panel-ui-rebuild-prd.md`
- `docs/panel-ui-system-analysis.md`

## 1. Working Rule

Every implementation step should produce:

- a named binary
- local artifact copy
- phone deployment path
- SHA256
- systemd status proof
- at least one screenshot when UI changes
- rollback note

Do not combine risky platform work with UI work in the same step.

## 2. Phase 0 - Current Baseline

Baseline binary:

```text
/data/experiments/alioth-status-ui-gpu-ui-runtime-v2-cpu-tuning
```

Baseline local artifact:

```text
artifacts/remote/gpu-runtime-20260513/alioth-status-ui-gpu-ui-runtime-v2-cpu-tuning
```

Baseline proof:

```text
service active/running
NRestarts=0
sha256=615c3ea3dc1bfceef363796cfa03aed2b40ac7d5944db75be88cf25abf60a51e
MONITOR and POWER screenshots captured at 1080x2400
metrics: page=POWER panel_hz=120 fps=1 draw_ms=10 gpu_ms=6 present_ms=15
Monitor Health card still shows current systemd failed-unit count.
Power page now has real backlight status plus screen-mode and brightness controls.
Fifth LOGS tab now exposes service state and panel logs.
Monitor CPU section now shows governor, cluster frequencies, and hottest thread.
Power page now has Eco/Auto/Perf governor controls.
```

Rollback binary:

```text
/data/experiments/alioth-status-ui-gpu-ui-runtime-v2-wifi-save
sha256=9919eb459fd74911fb9bb5328f2a57ca184fc99bbbd6a64feca85cd4b75dcf1d
```

`v2-paths` is byte-identical to `v2-wifi-save`; it marks the first
behavior-preserving source organization step.

`v2-cpu-tuning` is the current visible baseline. It keeps the same GPU/Wi-Fi/Power
features, adds system health display on Monitor, updates Monitor touch hitboxes,
rebuilds Power as a touch-first control page, adds a fifth LOGS tab for service
details, and adds CPU hotspot/governor controls.

## 3. Phase 1 - Behavior-Preserving Module Split

Goal:

```text
Move from one monolithic C file toward a maintainable module structure without
changing visible behavior.
```

Tasks:

- Create `src/alioth-panel/`.
- Move shared runtime constants and file paths into `src/alioth-panel/panel_paths.h`.
- Move canvas/basic draw helpers.
- Move UI theme/primitives/nav.
- Move data helpers into provider files.
- Move page draw functions.
- Keep all public runtime paths unchanged.
- Keep the same service environment.

Artifact:

```text
alioth-status-ui-gpu-ui-runtime-v2-split
```

Tests:

- Cross build succeeds.
- Deploy and verify service.
- Screenshot Monitor page.
- Screenshot GPU page.
- Verify `/run/alioth-panel-page`.
- Verify `/run/alioth-panel-screenshot.request`.
- Verify `/run/alioth-panel-gpu-metrics.txt`.

Exit criteria:

```text
No visible regression against v1.
Service remains active/running with NRestarts=0.
```

## 4. Phase 2 - Network Page Rebuild

Goal:

```text
Make Wi-Fi/network page touch-first and visually consistent with the new runtime.
```

Tasks:

- Network status cards: USB, Wi-Fi, route, DNS.
- Scan results as list rows.
- SSID/password fields as runtime widgets.
- Password masking.
- Touch keyboard as runtime widget.
- Connect/scan progress display.
- Error/log tail display.

Artifact:

```text
alioth-status-ui-gpu-network-v1
```

Tests:

- Page screenshot before scan.
- Run scan from touch or request path.
- Screenshot scan results.
- Select an SSID by touch.
- Type into SSID/password fields.
- Trigger connect against known-safe config only when intended.
- Confirm USB SSH/2323 still visible and reachable.

Exit criteria:

```text
Network page is feature-equivalent or better than legacy Wi-Fi page.
No secret is exposed in screenshots by default.
```

## 5. Phase 3 - Power Page Rebuild

Goal:

```text
Make power, charging, backlight, thermal, and dangerous actions touch-first.
```

Tasks:

- Battery and USB input cards.
- Charging detail rows.
- Thermal rows.
- Brightness controls with bounded values.
- Screen mode chips.
- Reboot/fastboot/poweroff action cards.
- Touch modal confirmation.

Artifact:

```text
alioth-status-ui-gpu-ui-runtime-v2-power-v1
```

Tests:

- Screenshot normal Power page.
- Touch screen mode chips.
- Test brightness small step and restore.
- Open confirmation modal and cancel.
- Do not execute destructive action unless explicitly requested in that run.

Exit criteria:

```text
Power page no longer depends on visible physical-key instructions.
Dangerous action confirmation is clear and touch-first.
```

Current status:

```text
Implemented in v2-power-v1:
  - battery / USB / backlight metric cards
  - live battery, charge, thermal, and screen mode rows
  - screen mode chips: Normal, Low, Night, Lamp, Off
  - brightness controls: - / 25% / 50% / 75% / +
  - existing confirmed actions: Back, Power Off, Reboot, Fastboot

Still needs human-on-device confirmation:
  - physical touch hitboxes for screen-mode chips
  - physical touch hitboxes for brightness controls
  - confirmation modal visual after tapping dangerous actions
```

## 6. Phase 4 - GPU Power Benchmark

Goal:

```text
Make GPU FPS/power experiments measurable rather than anecdotal.
```

Tasks:

- 10s and 30s rolling averages for BAT and USB.
- Rolling KGSL busy average.
- Benchmark start/stop.
- CSV sample file in `/run`.
- On-screen comparison table for 30/15/5 FPS.
- Small graph/sparkline.

Artifact:

```text
alioth-status-ui-gpu-benchmark-v1
```

Tests:

- Run 30 FPS for at least 30s.
- Run 15 FPS for at least 30s.
- Run 5 FPS for at least 30s.
- Verify sample file format.
- Confirm KGSL busy tracks FPS target.
- Report power numbers as trend/average, not lab precision.

Exit criteria:

```text
User can compare FPS, frame time, KGSL busy, BAT net power, and USB input power
from the panel itself.
```

## 7. Phase 5 - Polish And Interaction Pass

Goal:

```text
Make the panel feel coherent instead of a set of debug pages.
```

Tasks:

- Consistent spacing and typography.
- Better alert colors.
- Icon/indicator primitives.
- Optional haptic feedback for high-confidence touch actions.
- Loading/progress states.
- Empty/error states.
- Decide whether external libraries are needed.

Tests:

- Screenshot every page.
- Manual touch pass on every primary action.
- Service run for 10+ minutes on GPU page.
- Service run for 10+ minutes on Monitor page.

Exit criteria:

```text
All primary pages share the same visual system and interaction model.
```

## 8. Regression Checklist

Run after every deployed UI build:

```text
systemctl show lele-status-ui.service -p MainPID -p NRestarts -p ExecMainStatus -p ActiveState -p SubState
sha256sum /data/experiments/<binary>
cat /run/alioth-panel-gpu-demo.txt
printf monitor > /run/alioth-panel-page
touch /run/alioth-panel-screenshot.request
printf gpu > /run/alioth-panel-page
touch /run/alioth-panel-screenshot.request
ping -c 1 172.16.42.2
ssh root@172.16.42.2 true
printf 'true; exit\n' | nc -w 5 172.16.42.2 2323
```

Pass:

- Service active/running.
- No restarts.
- Correct binary SHA256.
- Screenshots match requested pages.
- SSH and 2323 remain usable.

## 9. Stop Conditions

Stop and diagnose before continuing if:

- Service restarts unexpectedly.
- Display goes blank.
- Touch stops responding.
- USB SSH and 2323 both fail.
- Screenshot request no longer writes metadata.
- GPU renderer falls back unexpectedly.
- dmesg/journal shows new DRM/KGSL faults.

## 10. Next Action

Start Phase 1:

```text
Implement behavior-preserving module split as
alioth-status-ui-gpu-ui-runtime-v2-split.
```

Do not rebuild Wi-Fi UI until the split build is deployed and screenshot-verified.
