# Hardware ABI Status

Last refresh: 2026-05-09 23:36 CST.

Current baseline:

```text
DISTRO: Ubuntu 24.04.4 LTS userspace
PID1:   systemd 255
KERNEL: Lineage/Android downstream 4.19.312-perf
ROOT:   /dev/block/by-name/userdata[/rootfs/ubuntu-24.04]
SLOT:   androidboot.slot_suffix=_b
REPORT: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-hw-abi-inventory/report.txt
INPUT:  /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-input-baseline/report.txt
TOUCH_HAPTIC: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-touch-haptic-smoke/report.txt
INPUT_TOUCH_LIVE: /vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/input-touch/report.txt
BRIGHTNESS: /vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/brightness/dim-restore-retry.txt
WIFI_SCAN_2026_05_09: /vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/wifi-scan/report.txt
AUDIO_INVENTORY_2026_05_09: /vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/audio-inventory/report.txt
AUDIO_ADSP_BOOT_TRIGGER_2026_05_09: /vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/audio-adsp-boot-trigger/report.txt
AUDIO_ALSA_UTILS_2026_05_09: /vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/audio-alsa-utils-enumeration/report.txt
AUDIO_ADSP_SERVICE_2026_05_09: /vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/audio-adsp-service/deploy-verify.txt
AUDIO_ADSP_SERVICE_RECREATE_2026_05_09: /vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/audio-adsp-service/recreate-devnodes.txt
AUDIO_SPEAKER_2026_05_09: /vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/audio-first-beep/speaker-tert-mi2s-max-5s-fixed.txt
AW8697_PATH:  /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-aw8697-path/report.txt
USB_ROLE:     /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-usb-role-hid-readiness/report.txt
DRM_MESA:     /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/report.txt
GFX_INSTALL:  /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/install-graphics-tools.txt
MODETEST:     /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/graphics-probe-modetest.txt
EGLINFO:      /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/graphics-probe-eglinfo.txt
KGSL_RETRY:   /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/kgsl-node-egl-retry.txt
WESTON_VT:    /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/graphics-probe-weston-pixman-openvt-switch.txt
WESTON_BUILTIN_NOVT: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/graphics-probe-weston-pixman-builtin-novt.txt
MESA_KGSL_STRACE:   /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-strace.txt
MESA_DRIVER_MATRIX: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-driver-selection-matrix.txt
MESA_TARGETS:       /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-gallium-targets.txt
LIBGALLIUM_INSPECT: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/libgallium-host-target-inspection.txt
ANDROID_GFX_INV:    /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/android-lineage-graphics-userspace-inventory.txt
KGSL_OPEN_PATH:     /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/kgsl-open-path-inventory.txt
KGSL_FW_GETPROP:    /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/kgsl-firmware-getproperty-smoke.txt
VULKAN_LOADER_INV:  /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-vulkan-loader-inventory.txt
VULKAN_TOOLS_SMOKE: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-vulkan-tools-smoke.txt
PULLED_FREEDRENO_ICD: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/libvulkan_freedreno-ubuntu-arm64.so
MESA_KGSL_BUILD_FEASIBILITY: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-build-feasibility.txt
MESA_KGSL_MESON_SETUP: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-meson-setup.txt
MESA_KGSL_PRIVATE_BUILD: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-build.txt
MESA_KGSL_GITSHA1_RETRY: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-build-gitsha1-retry.txt
MESA_KGSL_LIBDRM_INCLUDE_RETRY: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-build-libdrm-include-retry.txt
MESA_KGSL_DEFINE_USER_RETRY: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-build-define-user-retry.txt
PRIVATE_KGSL_FREEDRENO_ICD: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/libvulkan_freedreno-kgsl-mesa2034-arm64.so
PRIVATE_KGSL_VULKANINFO_SMOKE: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-vulkaninfo-smoke.txt
PRIVATE_KGSL_NOOP_SUBMIT: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-noop-submit.txt
PRIVATE_KGSL_COMPUTE_SMOKE: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-compute-smoke.txt
PRIVATE_KGSL_IMAGE_CLEAR_SMOKE: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-image-clear-smoke.txt
PRIVATE_KGSL_OFFSCREEN_TRIANGLE_SMOKE: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-offscreen-triangle-smoke.txt
PRIVATE_KGSL_VISIBLE_STAGED_PRESENT_FIRST: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-visible-staged-present-smoke.txt
PRIVATE_KGSL_VISIBLE_STAGED_PRESENT_RETRY1: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-visible-staged-present-smoke-retry1.txt
PRIVATE_KGSL_DMABUF_PROBE: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-dmabuf-probe.txt
PRIVATE_KGSL_DMABUF_PROBE_RETRY1: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-dmabuf-probe-retry1.txt
PRIVATE_KGSL_DMABUF_EXPORT_IMPORT: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-dmabuf-export-import.txt
DRM_PRIME_TO_KGSL_IMPORT: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-prime-to-kgsl-vulkan-import.txt
DRM_PRIME_KGSL_CLEAR_READBACK: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-prime-kgsl-clear-readback.txt
DRM_PRIME_KGSL_CLEAR_PRESENT: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-prime-kgsl-clear-present.txt
PRIVATE_KGSL_WSI_DISPLAY_PROBE: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/private-kgsl-vulkan-wsi-display-probe.txt
PRIVATE_KGSL_WSI_DISPLAY_PROBE_RETRY1: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/private-kgsl-vulkan-wsi-display-probe-retry1.txt
PRIVATE_KGSL_WSI_DISPLAY_PROBE_RETRY2: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/private-kgsl-vulkan-wsi-display-probe-retry2.txt
PRIVATE_EGL_GBM_GLONLY_RUNTIME: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-egl-gbm-glonly-runtime-inventory.txt
MSM_DRM_IOCTL_PROBE: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-msm-ioctl-probe.txt
DRM_PAGEFLIP_SMOKE: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-pageflip-smoke.txt
DRM_PRIME_KGSL_PAGEFLIP_CLEAR: /vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-prime-kgsl-pageflip-clear.txt
```

## Summary

| Area | Current ABI state | Verdict |
|---|---|---|
| Display | `/dev/dri/card0`, `/dev/dri/renderD128`, DSI connected, 1080x2400 modes at 60/90/120 Hz; `modetest` runtime OK; CPU-only page-flip 24/24 events OK; `/sys/class/backlight/panel0-backlight/brightness` writable with max 2047 and bounded 536 -> 178 -> 536 restore verified; EGL currently `llvmpipe`; Weston DRM backend blocked even with libseat builtin non-VT mode | Raw DRM/KMS, page-flip, and backlight writes usable; standard accelerated desktop compositor not ready |
| GPU userspace | Android/Lineage uses Qualcomm Adreno EGL/GLES/Vulkan blobs plus `libgsl.so` over KGSL and QCOM gralloc/HWComposer/mapper/composer HALs; Ubuntu can open `/dev/kgsl-3d0` after A650 firmware staging; stock Ubuntu Turnip rejects `/dev/dri/renderD128`, but private Lineage Mesa `20.3.4` KGSL Turnip enumerates `FD650`, submits through `IOCTL_KGSL_GPU_COMMAND`, passes no-surface compute/image/offscreen render-pass tests, can CPU-stage rendered pixels into a DRM dumb buffer, imports/writes/scans out DRM PRIME buffers, and now clears two DRM-owned buffers through KGSL while KMS page-flips 18/18 events; actual KGSL allocation export is stubbed; private `VK_KHR_display` WSI crashes; private EGL/GBM/Gallium builds but cannot create a hardware screen because Gallium KGSL is only an `msm` alias and current `msm_drm` nodes reject GPU params/IOVA | KGSL Vulkan graphics plus DRM-owned double-buffer direct scanout proven; standard EGL/GBM/WSI/compositor still not proven |
| Status UI | Direct KGSL Vulkan + DRM/KMS page-flip UI active by default; runtime-status helper shows systemd failure count, battery, USB, Wi-Fi, audio ADSP/SND state, launcher/menu hints and system details; CPU DRM/KMS UI remains fallback | Proven |
| Input keys | `/dev/input/event1` `qpnp_pon` has `KEY_VOLUMEDOWN`/`KEY_POWER`; `/dev/input/event5` `gpio-keys` has `KEY_VOLUMEUP`; `uinput-goodix` also exposes virtual nav/power/volume keys | Usable for power/volume |
| Touch | `/dev/input/event2` `fts_ts`, `ABS_MT_POSITION_X/Y`, `BTN_TOUCH`, `INPUT_PROP_DIRECT`; `libinput` lists it as `keyboard touch`; `libinput debug-events` captured live `TOUCH_DOWN`/`TOUCH_MOTION` frames; `evtest` captured live `ABS_MT_POSITION_X/Y` events | Evdev/libinput live touch path proven; compositor/app gesture polish remains |
| Haptics | `/dev/input/event4` `aw8697_haptic`; `FF_CONSTANT` upload/playback succeeds; `EVIOCRMFF` cleanup returns `EINVAL`; sysfs controls under `/sys/bus/i2c/devices/2-005a` | Basic playback path reached; physical feel and cleaner cleanup/sysfs path still need validation |
| Battery/charging | `power_supply`: `battery`, `bms`, `usb`, `main`, `bq2597x-standalone` | Usable for status; control policy not done |
| USB networking | ConfigFS gadget `alioth`, `ncm.usb0`, `usb0=172.16.42.2/24` | Proven |
| USB host/HID | Current port is in gadget/NCM device role; Type-C `data_role` shows `host [device]`; no `/sys/class/usb_role`; no `/sys/bus/usb/devices` entries | Host role looks exposed but untested; switching will likely drop SSH |
| Storage | userdata ext4 root, 189G, UFS partitions visible | Proven, but partition writes remain high-risk |
| CPU/thermal | 8 CPUs online, many thermal zones visible | Enumerated; power policy not tuned |
| Wi-Fi | QCA6390 works through boot-managed `alioth-wifi-*` services: modem firmware mount, Android `qrtr-ns`, Android `cnss-daemon`, cold-boot calibration, `/dev/wlan ON`; `wlan0`/`phy0` appear automatically, `wlan0` uses factory MAC `50:98:39:55:21:9c`, `iw dev wlan0 scan` succeeds | Standard Linux scan and nl80211 baseline proven; client association awaits credentials/config helper |
| Bluetooth | `rfkill0`/`btpower` power path works; temporary BT kernel candidates enable HCI UART/QCA handoff on `/dev/ttyHS0`; userspace patch/NVM download plus QCA line discipline can register `hci0`, power it through BlueZ, and override BD_ADDR through NVM tag 2. Android `bluetooth_b` partition firmware hashes match current `/lib/firmware/qca` aliases. | HCI baseline proven only on temporary kernel; scan/inquiry/radio-operation commands still time out, so vendor init/RF/coexistence sequencing is the next blocker |
| Audio | ADSP can be booted from Ubuntu by setting `firmware_class.path=/vendor/firmware_mnt/image` and writing `1` to `/sys/kernel/boot_adsp/boot`; `subsys2` reaches `ONLINE`; ASoC registers `kona-mtp-snd-card`; `/proc/asound/pcm` lists Qualcomm MultiMedia, AFE-PROXY, SLIMBUS, MI2S/TDM, USB hostless, and CDC DMA endpoints; `alioth-audio-adsp-boot.service` is enabled and recreates 120 `/dev/snd` char nodes from `/sys/class/sound/*/dev`; `aplay -l`, `arecord -l`, `amixer -c 0 controls`, and `alsactl info 0` work, with `controls_count: 5333`. A bounded TERT-MI2S / CS35L41 speaker route test returned `aplay_beep_rc=0`, generated CS35L41 amp unmute/mute events, was physically accepted by the user, and restored mixer controls. | Kernel/ADSP/ASoC/ALSA enumeration and first speaker playback proven; other routes and recording remain |
| Camera/V4L2 | many `cam-*` V4L2 subdevs and `video*` sysfs entries | Enumerated; likely Android camera stack dependent |
| Sensors/IIO | PMIC VADC and `distance` IIO devices present | Enumerated, needs userspace mapping |
| Modem | modem thermal zones; no usable QMI/QRTR path established | Not ready |

## Tooling Present

Present on current rootfs:

```text
udevadm
lsusb
mmcli
evtest
evemu-tools
libinput-tools
kmscube
weston
modetest / libdrm-tests
eglinfo / glxinfo / mesa-utils
vulkaninfo / vulkan-tools
mesa-vulkan-drivers / freedreno_icd.json
wayland-info / wayland-utils
seatd / seatd-launch
iw
rfkill
wpa_supplicant / wpa_cli
bluetoothctl / bluez
btmgmt
alsa-utils / aplay / arecord / amixer / alsactl
```

Missing and likely useful for next hardware tests:

```text
v4l-utils
media-ctl
pipewire
```

## Notes

- Current display success is the low-level DRM/KMS path, not a full desktop compositor.
- Wi-Fi has moved from "not ready" to a boot-managed baseline. Required pieces are `/lib/firmware/wlan/qca_cld/WCNSS_qcom_cfg.ini`, `/lib/firmware/wlan/qca_cld/wlan_mac.bin` copied from `persist/wlan_mac.bin`, `/lib/firmware/qca6390` pointing at `modem_b:/image/qca6390`, Android `qrtr-ns`, Android `cnss-daemon`, cold-boot calibration through CNSS `fs_ready`, and `/dev/wlan ON`. Current managed interface is `wlan0`; P2P/NAN companion interfaces also appear.
- Bluetooth is no longer a generic missing-tools issue. The downstream `bt_power` path works through `rfkill0`, and a temporary non-serdev HCI UART/QCA kernel path now creates `hci0` after userspace firmware/NVM download. The best sequence is still BlueZ-first, no post-firmware HCI reset, and QCA attach with a longer idle window. Remaining failure is narrower: the controller answers init/read commands but scan, inquiry, and LE radio setup commands time out. The active Android `bluetooth_b` partition contains the same `htbtfw20.tlv`/`htnv20.bin` already staged in Ubuntu, so the next variable is QTI vendor initialization/RF/coexistence behavior rather than the basic firmware payload.
- DRM/Mesa readiness inventory found `card0` and `renderD128` openable by root, `card0-DSI-1` connected/enabled with 1080x2400 60/90/120 Hz modes, and `seat0` containing display plus built-in input devices.
- `modetest -c -p` successfully opened `MSM Snapdragon DRM` on driver `msm_drm` 1.3.0 after the custom status UI released DRM master, then the wrapper restored `lele-status-ui`.
- Mesa EGL initializes but currently renders with `llvmpipe`. `/sys/class/kgsl/kgsl-3d0` exposes `Adreno650v3`; temporary `/dev/kgsl-3d0` plus `libdrm-freedreno1` did not enable Adreno hardware rendering. The current `strace` and driver matrix show EGL/GBM does not open `/dev/kgsl-3d0` and does not load `kgsl_dri.so`/`msm_dri.so`; `MESA_LOADER_DRIVER_OVERRIDE=kgsl/msm/freedreno` stays on `llvmpipe`, while `GALLIUM_DRIVER=kgsl/msm/freedreno` fails EGL initialization. Treat stock Ubuntu Mesa environment selection as exhausted.
- Android/Lineage graphics inventory found the known-working vendor path: proprietary Adreno EGL/GLES/Vulkan blobs call `libgsl.so`, which contains KGSL ioctl entry points such as context creation, command submission, shared memory and GPU object operations. Those blobs depend on Android/Bionic/HIDL/ION/libsync/display HAL libraries, so they are not drop-in Ubuntu glibc libraries.
- KGSL open-path inventory found the char device and major/minor were correct, but the first `open(2)` failed with `ENOENT` because the driver tried to load missing `a650_sqe.fw`. After staging Lineage vendor A650 firmware files into `/lib/firmware`, the KGSL getproperty smoke test passed: `/dev/kgsl-3d0` opens, KGSL reports driver `3.14`, device `3.1`, derived GPU `650`, GMEM `1179648`, bitness `48`, and UBWC mode `4`. This is a kernel ABI success, not yet a Mesa/Vulkan renderer success.
- Firmware-present Vulkan testing installed `mesa-vulkan-drivers` and `vulkan-tools`. `freedreno_icd.json` and `libvulkan_freedreno.so` are present, and the KGSL helper still passes, but Turnip reports `device /dev/dri/renderD128 (msm_drm) is not compatible with turnip`. Default `vulkaninfo` falls back to CPU `llvmpipe`; forcing only the freedreno ICD returns `VK_ERROR_INCOMPATIBLE_DRIVER`/`ERROR_INITIALIZATION_FAILED`.
- Host inspection of the pulled Ubuntu `libvulkan_freedreno.so` found no `/dev/kgsl-3d0` or `tu_kgsl` strings. Local Lineage Mesa source shows KGSL support is gated by the `freedreno-kgsl` Meson option and `tu_kgsl.c`, so the next graphics work should evaluate a KGSL-enabled Mesa/Turnip build rather than repeating stock Ubuntu loader/environment tests.
- The KGSL-enabled Mesa build feasibility inventory found Lineage Mesa `20.3.4` can build `tu_kgsl.c` with `-Dfreedreno-kgsl=true`. Phone-native build is preferred over host cross-build for now: the phone has enough disk/RAM/CPU, apt candidates exist for build dependencies, and the host currently lacks the aarch64 cross toolchain plus Meson/Ninja.
- Phone-native Meson setup succeeded in `/data/experiments/mesa3d-20.3.4-kgsl-build` with `freedreno-kgsl=true`, `vulkan-drivers=freedreno`, private prefix `/data/experiments/mesa-kgsl-prefix`, `OpenGL/EGL/GBM/Gallium/LLVM` disabled, and 136 build targets. This setup is now the source of the tested private KGSL Turnip ICD.
- First bounded private Ninja build confirmed `tu_kgsl.c.o` is a real target, then failed before linking because `freedreno_uuid.c` could not include `git_sha1.h`. This is a Mesa source/build metadata issue caused by the staged source lacking generated Git version headers, not a hardware ABI failure.
- Private Lineage Mesa `20.3.4` KGSL Turnip now compiles after explicitly generating `src/git_sha1.h`, exposing `/usr/include/libdrm` to the compiler, and setting `-D__user=` for the userspace KGSL UAPI header. The resulting `libvulkan_freedreno.so` is an aarch64 ELF and statically contains `/dev/kgsl-3d0`, `tu_kgsl.c`, `tu_enumerate_devices`, `kgsl_device_getproperty`, and `kgsl_gpu_command`. Runtime Vulkan is proven through offscreen graphics pipeline tests, but display presentation is not.
- Forced private ICD `vulkaninfo --summary` now succeeds with `vulkaninfo_private_rc=0`, `deviceName=FD650`, `apiVersion=1.2.131`. `strace` confirms this path opens `/dev/kgsl-3d0` and issues KGSL getproperty/context/memory ioctls. Loader warnings remain because Mesa 20.3.4 reports Vulkan 1.2 with an older ICD loader interface version, but enumeration succeeds.
- The no-surface helper `scripts/alioth_vulkan_noop_submit.c` now passes through the private ICD: it creates instance/device/queue/command pool/command buffer, submits an empty command buffer, waits idle, and returns `noop_submit=PASS`. `strace` shows `IOCTL_KGSL_GPU_COMMAND` (`0x09/0x4a`) returns 0.
- The no-surface compute helper `scripts/alioth_vulkan_compute_smoke.c` plus `scripts/alioth_vulkan_compute_smoke.comp` now passes through the private ICD. It compiles GLSL to SPIR-V on the phone, creates a compute pipeline and host-visible storage buffer, dispatches one workgroup, waits idle, and reads back `0x5a17c0de`. `strace` again shows `/dev/kgsl-3d0` and successful `IOCTL_KGSL_GPU_COMMAND`; systemd remains `running` with 0 failed units.
- The no-surface image helper `scripts/alioth_vulkan_image_clear_smoke.c` now passes through the private ICD. It creates an optimal `VK_FORMAT_R8G8B8A8_UNORM` image, performs layout transitions, clears it to RGBA `00 ff 00 ff`, copies it to a host-visible buffer, and verifies 1024 bytes with zero mismatches. This proves image allocation, transfer clear/copy and readback, but not render passes or presentation.
- The offscreen triangle helper `scripts/alioth_vulkan_offscreen_triangle_smoke.c` plus minimal vertex/fragment shaders now passes through the private ICD. It creates a render pass, framebuffer and graphics pipeline, draws a triangle into an offscreen color attachment, copies the image to a host-visible buffer, and validates `center_pixel=ff 00 00 ff` plus `red=1352 black=2744 other=0`. This proves the KGSL Turnip graphics pipeline offscreen; presentation/display integration remains separate.
- The sampled-texture helper `scripts/alioth_vulkan_textured_quad_smoke.c` plus full-screen textured shaders now passes through the private ICD. It uploads a CPU-generated vertical-stripe RGBA texture through a staging buffer, samples it through a combined image sampler descriptor in a fragment shader, renders to a 64x64 offscreen target, copies back, and validates `red=2048 green=2048 other=0` plus `texture_sample_pass=8/8`. This proves descriptor sets, samplers and sampled image reads needed for glyph/icon/texture UI rendering.
- The CPU-staged visible presenter path now works. The modified offscreen triangle helper dumps a 64x64 XRGB file after GPU readback; `scripts/alioth_drm_present_raw_xrgb.c` opens `/dev/dri/card0`, creates an XRGB8888 dumb buffer, scales that raw image into the 1080x2400 mode, calls `drmModeSetCrtc`, and returns `present_raw_xrgb=PASS` for 5 seconds on connector 29 / CRTC 129. `lele-status-ui.service` restored to active and systemd stayed `running` with 0 failed units.
- The external-memory probe `scripts/alioth_vulkan_dmabuf_probe.c` reports `VK_EXT_external_memory_dma_buf`, `VK_EXT_image_drm_format_modifier`, `VK_KHR_external_memory_fd`, `VK_KHR_bind_memory2`, `VK_KHR_dedicated_allocation`, and `VK_KHR_swapchain`. RGBA optimal external image queries for dma-buf and opaque-fd both return success with `features=0x7` and `compatible=0x201`; interpret this as dedicated-only import/export capability for opaque-fd and dma-buf. `VK_EXT_queue_family_foreign` is absent. No fd export/import or DRM dmabuf framebuffer has been attempted yet.
- The minimal KGSL allocation export/import proof `scripts/alioth_vulkan_dmabuf_export_import.c` shows the real export path is blocked: `vkGetMemoryFdKHR` fails with Mesa logging `tu_kgsl.c:151 FINISHME: stub tu_bo_export_dmabuf`. Source inspection confirms `tu_bo_init_dmabuf()` is implemented through `IOCTL_KGSL_GPUOBJ_IMPORT`, while `tu_bo_export_dmabuf()` is stubbed. Treat DRM/GBM-allocated dmabuf import into KGSL as the next lower-copy direction.
- The DRM-owned import path now works end to end for custom KMS/Vulkan presentation. `scripts/alioth_drm_prime_to_vulkan_import.c` proves fd import/bind; `scripts/alioth_drm_prime_vulkan_clear_readback.c` proves KGSL write plus CPU readback with 4096/4096 red pixels; `scripts/alioth_drm_prime_vulkan_clear_present.c` proves a full-screen 1080x2400 DRM dumb buffer can be imported, cleared by KGSL, verified through CPU samples, added as FB2, and scanned out for 5 seconds. Vulkan rowPitch matched DRM pitch at `4352`, and final systemd/status UI state stayed healthy.
- Private EGL/GBM/Gallium GL-only Mesa now builds and installs into `/data/experiments/mesa-kgsl-egl-gbm-glonly-prefix`, including `libEGL`, `libgbm`, `kgsl_dri.so`, and `msm_dri.so`, but runtime inventory shows `eglinfo` cannot create a DRI screen/GBM device. Source/symbol inspection explains the result: `freedreno-kgsl` in this Mesa tree is for the Vulkan Turnip backend, while Gallium `kgsl` is only an alias to `msm`; the installed DRI library has `__driDriverGetExtensions_kgsl` but no `kgsl_device_new`.
- MSM DRM ioctl probing confirms `/dev/dri/renderD128` and `/dev/dri/card0` are display/dumb-GEM nodes, not useful freedreno GPU render nodes here. Both report `drm_version name=msm_drm version=1.3.0`; tiny `DRM_IOCTL_MSM_GEM_NEW` succeeds, but `MSM_PARAM_GPU_ID`, `GMEM_SIZE`, `CHIP_ID`, `MAX_FREQ`, `TIMESTAMP`, `GMEM_BASE`, `NR_RINGS`, and `GEM_INFO IOVA` all return `EINVAL`.
- KMS page-flip is proven independently by `scripts/alioth_drm_pageflip_smoke.c`: two full-screen CPU-filled dumb buffers alternated with `drmModePageFlip()` and 24/24 page-flip events were received, with the status UI restored afterward.
- The current best graphics foundation is `scripts/alioth_drm_prime_vulkan_pageflip_clear.c`: two full-screen DRM dumb buffers are exported as PRIME fds, imported into private KGSL Turnip as linear `B8G8R8A8_UNORM` images, cleared through KGSL, sampled through dma-buf sync, and page-flipped through KMS. The run passed 18/18 submitted flips and events, with red/green/blue/yellow center-pixel checks matching every frame.
- `scripts/alioth_drm_prime_vulkan_triangle_pageflip.c` extends that foundation from clear colors to real graphics pipeline output. It imports two full-screen DRM PRIME scanout buffers, creates image views/render pass/framebuffers/graphics pipeline over them, draws the existing red triangle shader directly into each imported linear `B8G8R8A8_UNORM` image, validates 5/5 CPU samples per frame, and receives 18/18 KMS page-flip events. The first attempt only failed because a black validation point lay on the triangle edge; retry1 passed after moving the sample points.
- `scripts/alioth_drm_prime_vulkan_textured_pageflip.c` extends the same direct-scanout route to texture-backed UI rendering. It uploads a 64x64 red/green stripe texture, binds it through a combined image sampler descriptor, samples it in the fragment shader, renders directly into two full-screen imported DRM PRIME scanout buffers, validates 8/8 BGRA stripe samples on every frame, and receives 18/18 KMS page-flip events. This is the current strongest proof for a custom KGSL Vulkan + DRM/KMS boot monitor renderer.
- `scripts/alioth_drm_prime_vulkan_glyph_text_pageflip.c` proves the first text-rendering version of that boot-monitor route. It generates a CPU 8x8 glyph atlas for `UBUNTU KGSL KMS`, uploads it as a sampled image, builds a vertex buffer containing 78 vertices for 13 glyph quads, uses atlas UVs plus fragment discard for transparent texels, renders directly into imported DRM PRIME scanout buffers, validates glyph-stroke/background samples on every frame, and receives 12/12 KMS page-flip events. The first attempt used the wrong Y NDC convention; retry1 passed after correcting the Vulkan viewport mapping.
- The same glyph helper now has a dynamic monitor-page mode and has been installed as the default status UI service path. The current runtime-status build reads systemd state and failed unit count, battery capacity/status, USB/Wi-Fi operstate, and `/run/alioth-audio-state`; it renders pages for runtime status, hardware stack, launcher hints, system details and the power menu. The bounded runtime-status smoke passed `drm_prime_kgsl_glyph_text_pageflip=PASS submitted=3 events=3`, then the helper was deployed over `/data/experiments/alioth_gpu_monitor_service` with the previous helper backed up at `/data/experiments/alioth_gpu_monitor_service.pre-runtime-status-20260509`. The systemd wrapper creates `/dev/kgsl-3d0`, starts the helper through the private KGSL Turnip ICD, and keeps the original CPU DRM/KMS status UI as fallback.
- WSI/direct-display inventory found advertised `VK_KHR_display`, `VK_KHR_surface`, `VK_EXT_direct_mode_display`, and device `VK_KHR_swapchain`, but no `VK_EXT_acquire_drm_display`. The minimal private-ICD helper crashes at `vkEnumeratePhysicalDevices()` when `VK_KHR_display` is enabled, before any surface/swapchain/modeset call. Do not run direct-display `vkcube` on this private ICD until Mesa WSI is fixed or rebuilt.
- Weston DRM backend with pixman renderer fails before compositor startup. Missing `/dev/tty0` was one issue, but even after temporary `/dev/tty0`/`tty1`, `openvt`/`openvt -s`, and `LIBSEAT_BACKEND=builtin SEATD_VTBOUND=0`, Weston cannot open `/dev/dri/card0`. Treat this as a compositor/session-control blocker rather than a raw KMS failure.
- Installing `kmscube`, `weston`, `libdrm-tests`, `wayland-utils`, `mesa-utils`, and `seatd` temporarily caused `fwupd-refresh.service` to fail. `fwupd-refresh.timer` was disabled, `fwupd.service` stopped, and failed state reset; systemd returned to `running` with 0 failed units.
- `libinput list-devices` sees `fts_ts` as a touch-capable device on `seat0`; the 2026-05-09 live report captured `TOUCH_DOWN`/`TOUCH_MOTION` through `libinput debug-events` and `ABS_MT_POSITION_X/Y` through `evtest`, so the standard evdev/libinput touch path is proven. Compositor/app-level gesture behavior still needs usability work.
- Display brightness control is available at `/sys/class/backlight/panel0-backlight/brightness`; the safe test wrote `536 -> 178 -> 536` and restored successfully. `actual_brightness` read `0` throughout, so use the writable `brightness` value as the practical control/readback for now.
- Audio changed materially on 2026-05-09. The earlier read-only inventory showed no ALSA card, but the actual blocker was the missing Android ADSP boot sequence and firmware lookup path, not a missing machine driver. Setting `firmware_class.path` to `/vendor/firmware_mnt/image` and writing `1` to `/sys/kernel/boot_adsp/boot` brings ADSP online and registers `kona-mtp-snd-card`; the persistent helper `/usr/local/sbin/alioth-audio-adsp-boot` now performs that sequence and recreates `/dev/snd` nodes at boot. Physical speaker output is now initially confirmed on the live TERT-MI2S / CS35L41 path: the fixed 5 second 1 kHz test enabled the TERT/DSP/MI2S route, `aplay` returned 0, dmesg showed CS35L41 amp unmute/mute, and all captured mixer values were restored. WSA speaker controls from base Android XML are not live on this profile; dmesg reports `tert_mi2s_rx_cs35l41_dai_links`.
- `evtest` confirms haptic force-feedback event codes. A direct Python/libc `EVIOCSFF` upload for `FF_RUMBLE` returned `EFAULT`; source inspection shows the downstream aw8697 driver expects `FF_CONSTANT` for simple duration playback or `FF_PERIODIC` with custom data. The `FF_CONSTANT` path uploads as effect id 0 and accepts play/stop writes; `EVIOCRMFF` returns `EINVAL`, so helper cleanup treats that as a warning after playback. Sysfs controls also exist at `/sys/bus/i2c/devices/2-005a/{duration,activate,effect_id,gain,vmax,...}`.
- Current USB connection is used as USB gadget/NCM for SSH. Testing a USB hub, keyboard, or mouse may switch the port role and can drop `usb0`; record a separate plan before doing that.
- USB readiness inventory found ConfigFS gadget `alioth` bound to UDC `a600000.dwc3`, function `ncm.usb0`, `usb0=172.16.42.2/24`, high-speed only. Type-C `port0` has writable root-owned `data_role`, `power_role`, `port_type`, and `preferred_role`; current data role is `[device]` with `host` available. A host/HID test should be treated as a controlled disconnect experiment with screen/button or manual reboot recovery.
- `scripts/alioth_usb_host_probe.sh` is staged on the phone at `/tmp/alioth_usb_host_probe.sh` and passed dry-run. It should be launched with `systemd-run ... --execute` only when a USB-C hub/HID device is connected and temporary loss of SSH is acceptable.
- v8e cleaned up USB DHCP ownership: the wrapper still starts an early helper for rescue access, then stops it before Ubuntu systemd takes over; persistent boot now has one systemd-owned `alioth-usb-dhcpd` listener on `usb0:67`.
- The current state should be described as `Ubuntu 24.04 userspace/systemd on Lineage/Android downstream kernel`, not as an Ubuntu generic-kernel bare-metal install.
