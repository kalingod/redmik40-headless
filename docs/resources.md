# Resources

> 当前文件保留历史 Linux 工作站和实验产物索引。当前 macOS 接手环境、实际可见路径、可用工具和缺失资源见 `docs/environment.md`。涉及 `/vmdata/...` 的路径在当前主机上未必存在，执行前先按 `environment.md` 复核。

## 工作区

| 资源 | 路径 |
|---|---|
| 项目根目录 | `/home/lele/桌面/project/redmik40` |
| 当前实验产物目录 | `/vmdata/android/redmik40` |
| 完整 LineageOS 工作区 | `/vmdata/android/redmik40-lineageos` |
| 完整 LineageOS 源码根 | `/vmdata/android/redmik40-lineageos/src` |
| 当前已验证 boot baseline 目录 | `/vmdata/android/redmik40/lineage-sm8250` |
| SSH key | `/vmdata/android/redmik40/keys/alioth_usb_ed25519` |
| known_hosts | `/vmdata/android/redmik40/keys/known_hosts` |
| ROM/OTA 日志目录 | `/home/lele/桌面/project/redmik40/ROM` |

项目根目录不是 Git 仓库。`/vmdata/android/redmik40/lineage-sm8250` 下的 kernel/device 源码目录是独立 Git 仓库。`/vmdata/android/redmik40-lineageos/src` 是 repo 管理的完整 LineageOS 工作区。

命名修正记录：

- `/vmdata/android/redmik40-leanos` 已重命名为 `/vmdata/android/redmik40-lineageos`。
- 项目根目录的 `ORM/` 已重命名为 `ROM/`。里面的 aria2 历史日志仍保留原始路径文本。

## 当前已验证 boot baseline 源码

| 仓库 | 本地路径 | 分支 | 当前 commit |
|---|---|---|---|
| `xiaomi-sm8250-devs/android_kernel_xiaomi_sm8250` | `/vmdata/android/redmik40/lineage-sm8250/kernel` | `lineage-20` | `f3d39dae3` |
| `xiaomi-sm8250-devs/android_device_xiaomi_alioth` | `/vmdata/android/redmik40/lineage-sm8250/device_xiaomi_alioth` | `lineage-20` | `77b3545` |
| `xiaomi-sm8250-devs/android_device_xiaomi_sm8250-common` | `/vmdata/android/redmik40/lineage-sm8250/device_xiaomi_sm8250-common` | `lineage-20` | `c61b05f` |

## 完整 LineageOS 工作区

完整工作区：

```text
/vmdata/android/redmik40-lineageos/src
```

K40 相关路径：

| 路径 | 作用 |
|---|---|
| `device/xiaomi/alioth` | alioth 设备树、BoardConfig、产品定义、overlay、camera/audio 配置 |
| `device/xiaomi/sm8250-common` | sm8250/kona 公共设备树、rootdir、sepolicy、Wi-Fi/audio/common HAL 配置 |
| `kernel/xiaomi/sm8250` | 完整 sm8250 kernel source，含 alioth DTS/DTBO |
| `vendor/xiaomi/alioth` | alioth vendor blobs |
| `vendor/xiaomi/sm8250-common` | sm8250 common vendor blobs |
| `hardware/xiaomi` | Xiaomi 硬件相关公共实现 |

local manifest：

```text
/vmdata/android/redmik40-lineageos/src/.repo/local_manifests/roomservice.xml
```

当前 manifest 关注 `lineage-20`，包括：

```text
LineageOS/android_device_xiaomi_alioth
LineageOS/android_device_xiaomi_sm8250-common
LineageOS/android_hardware_xiaomi
LineageOS/android_kernel_xiaomi_sm8250
TheMuppets/proprietary_vendor_xiaomi_alioth
TheMuppets/proprietary_vendor_xiaomi_sm8250-common
```

权限说明：该完整工作区目前多数字段是 `root:root`，部分 `.repo/projects/*.git` 目录为 `700 root:root`。普通用户可读源码文件，但不能直接用 `git -C` 查询所有 repo 元数据。后续如果要在该完整工作区内修改或提交代码，先统一权限策略。

## 当前产物

| 名称 | 路径 |
|---|---|
| 当前默认 Ubuntu 24.04 GPU monitor boot image | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/lineage-mininitramfs-boot.img` |
| 当前默认 Ubuntu 24.04 GPU monitor initramfs | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/initramfs.cpio.gz` |
| 2026-05-11 pmOS alioth 6.19.6 export directory | `/vmdata/android/redmik40/experiments/20260510-pmos-pmbootstrap-community/export` |
| 2026-05-11 pmOS alioth 6.19.6 boot image (not boot-validated) | `/vmdata/android/redmik40/experiments/20260510-pmos-pmbootstrap-community/export/boot.img` |
| 2026-05-11 pmOS alioth 6.19.6 combined sparse userdata image (flashed in E73) | `/vmdata/android/redmik40/experiments/20260510-pmos-pmbootstrap-community/export/xiaomi-alioth.img` |
| 2026-05-11 pmOS alioth 6.19.6 expanded raw verification image | `/vmdata/android/redmik40/experiments/20260510-pmos-pmbootstrap-community/work/chroot_native/home/pmos/rootfs/xiaomi-alioth.raw.img` |
| 2026-05-11 pmOS alioth 6.19.6 boot partition image (not flashed) | `/vmdata/android/redmik40/experiments/20260510-pmos-pmbootstrap-community/export/xiaomi-alioth-boot.img` |
| 2026-05-11 pmOS alioth 6.19.6 rootfs image (not flashed) | `/vmdata/android/redmik40/experiments/20260510-pmos-pmbootstrap-community/export/xiaomi-alioth-root.img` |
| 2026-05-11 pmOS alioth pmbootstrap workdir (use SSD for future rebuilds) | `/vmdata/android/redmik40/experiments/20260510-pmos-pmbootstrap-community/work` |
| 当前默认 Ubuntu 24.04 status UI wrapper | `/home/lele/桌面/project/redmik40/scripts/alioth_gpu_status_ui_wrapper.sh` |
| 当前 GPU monitor helper source | `/home/lele/桌面/project/redmik40/scripts/alioth_drm_prime_vulkan_glyph_text_pageflip.c` |
| 当前 runtime-status GPU monitor helper | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/gpu-monitor-runtime-status-wifi/alioth_gpu_monitor_service` and phone `/data/experiments/alioth_gpu_monitor_service` |
| latest wifi-status backup | phone `/data/experiments/alioth_gpu_monitor_service.pre-runtime-status-20260509-wifi2` |
| previous wifi-status backup | phone `/data/experiments/alioth_gpu_monitor_service.pre-runtime-status-20260509-wifi` |
| wifi-status scan-ready smoke | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/gpu-monitor-runtime-status-wifi-ready/wifi-status-ready-smoke.txt` |
| 当前 GPU monitor power menu report | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/gpu-monitor-power-menu-verify.txt` |
| 当前 GPU monitor reboot action report | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/gpu-monitor-reboot-action-verify.txt` |
| 当前 GPU monitor fastboot action report | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/gpu-monitor-fastboot-action-verify.txt` |
| runtime-status GPU monitor smoke | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/gpu-monitor-runtime-status/smoke.txt` |
| runtime-status GPU monitor default deploy | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/gpu-monitor-runtime-status/deploy-default.txt` |
| wlroots/cage Wayland introspection report | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/wlroots-cage-wayland-client-introspection.txt` |
| wlroots/cage screencopy report | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/wlroots-cage-screencopy-proof.txt` |
| wlroots/cage shm screenshot | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/cage-simple-shm.png` |
| wlroots/cage terminal screenshot report | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/wlroots-cage-terminal-screenshot.txt` |
| wlroots/cage terminal screenshot | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/cage-terminal.png` |
| Wayland cage session launcher script | `/home/lele/桌面/project/redmik40/scripts/alioth_wayland_cage_session.sh` |
| phone Wayland cage session launcher | `/usr/local/sbin/alioth-wayland-cage-session` |
| Wayland cage session systemd unit source | `/home/lele/桌面/project/redmik40/configs/alioth-wayland-cage-session.service` |
| phone Wayland cage session systemd unit | `/etc/systemd/system/alioth-wayland-cage-session.service` |
| Wayland launcher verification | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/alioth-wayland-cage-session-script-verify.txt` |
| Wayland launcher screenshot | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/launcher-terminal.png` |
| GPU monitor Wayland menu-action report | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/gpu-monitor-wayland-menu-action-verify.txt` |
| GPU monitor Wayland menu screenshot | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/menu-wayland-terminal.png` |
| Updated GPU menu reboot report | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/gpu-monitor-updated-menu-reboot-verify.txt` |
| Updated GPU menu fastboot report | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/gpu-monitor-updated-menu-fastboot-verify.txt` |
| labwc pixman terminal report | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/labwc-pixman-terminal-smoke.txt` |
| labwc terminal screenshot | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/labwc-terminal.png` |
| labwc session launcher script | `/home/lele/桌面/project/redmik40/scripts/alioth_wayland_labwc_session.sh` |
| phone labwc session launcher | `/usr/local/sbin/alioth-wayland-labwc-session` |
| labwc session systemd unit source | `/home/lele/桌面/project/redmik40/configs/alioth-wayland-labwc-session.service` |
| phone labwc session systemd unit | `/etc/systemd/system/alioth-wayland-labwc-session.service` |
| Brightness helper source | `/home/lele/桌面/project/redmik40/scripts/alioth_phone_brightness.sh` |
| phone brightness helper | `/usr/local/sbin/lele-brightness` |
| Audio ADSP boot helper source | `/home/lele/桌面/project/redmik40/scripts/alioth_audio_adsp_boot.sh` |
| Audio ADSP boot systemd unit source | `/home/lele/桌面/project/redmik40/configs/alioth-audio-adsp-boot.service` |
| phone audio ADSP boot helper | `/usr/local/sbin/alioth-audio-adsp-boot` |
| phone audio ADSP boot systemd unit | `/etc/systemd/system/alioth-audio-adsp-boot.service` |
| labwc systemd verification | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/alioth-wayland-labwc-session-systemd-verify.txt` |
| labwc systemd terminal screenshot | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/labwc-systemd-terminal.png` |
| minimal Wayland desktop package install | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/wayland-desktop-core-install.txt` |
| labwc desktop-smoke direct verification | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/labwc-desktop-smoke-verify.txt` |
| labwc desktop-smoke screenshot | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/labwc-desktop-smoke.png` |
| labwc touch-smoke helper | `/home/lele/桌面/project/redmik40/scripts/alioth_labwc_touch_smoke.sh` |
| labwc desktop-smoke systemd verification | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/labwc-desktop-smoke-systemd-verify.txt` |
| labwc desktop-smoke systemd screenshot | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/labwc-desktop-smoke-systemd.png` |
| 2026-05-10 AI2 labwc capture-wev artifacts | `/vmdata/android/redmik40/experiments/2026-05-10-ai2-ui-touch-smoke/alioth-ai2-labwc-touch-smoke` |
| 2026-05-10 AI2 labwc capture-wev screenshot | `/vmdata/android/redmik40/experiments/2026-05-10-ai2-ui-touch-smoke/alioth-ai2-labwc-touch-smoke.png` |
| GPU monitor labwc-menu deploy report | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/gpu-monitor-labwc-menu-deploy.txt` |
| GPU monitor labwc-menu action report | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/gpu-monitor-labwc-menu-action-verify.txt` |
| GPU monitor menu-triggered labwc screenshot | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/menu-labwc-desktop-smoke.png` |
| GPU monitor current-helper cancel report | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/gpu-monitor-labwc-helper-cancel-verify.txt` |
| GPU monitor current-helper reboot report | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/gpu-monitor-labwc-helper-reboot-verify.txt` |
| GPU monitor current-helper fastboot report | `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/gpu-monitor-labwc-helper-fastboot-verify.txt` |
| 2026-05-09 labwc health revalidation directory | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/labwc-health` |
| 2026-05-09 live touch/input report | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/input-touch/report.txt` |
| 2026-05-10 AI2 labwc touch-wev summary | `/vmdata/android/redmik40/experiments/2026-05-10-ai2-ui-touch-smoke/alioth-ai2-labwc-touch-smoke/summary.txt` |
| 2026-05-09 brightness dim/restore report | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/brightness/dim-restore-retry.txt` |
| 2026-05-09 brightness helper verification | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/brightness-helper/deploy-verify-v2.txt` |
| 2026-05-09 Wi-Fi scan revalidation | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/wifi-scan/report.txt` |
| 2026-05-09 audio ABI inventory | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/audio-inventory/report.txt` |
| 2026-05-09 audio kernel feasibility report | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/audio-kernel-feasibility/report.txt` |
| 2026-05-09 audio live ADSP probe | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/audio-live-adsp-probe/report.txt` |
| 2026-05-09 audio ADSP firmware/sysfs inventory | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/audio-adsp-firmware-sysfs-inventory/report.txt` |
| 2026-05-09 audio ADSP boot trigger | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/audio-adsp-boot-trigger/report.txt` |
| 2026-05-09 audio ALSA devnode udev probe | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/audio-alsa-devnodes/report.txt` |
| 2026-05-09 audio manual devnode creation | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/audio-manual-devnodes/report.txt` |
| 2026-05-09 audio alsa-utils enumeration | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/audio-alsa-utils-enumeration/report.txt` |
| 2026-05-09 audio ADSP service deployment | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/audio-adsp-service/deploy-verify.txt` |
| 2026-05-09 audio ADSP service devnode recreation | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/audio-adsp-service/recreate-devnodes.txt` |
| 2026-05-09 audio route inventory | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/audio-route-inventory/report.txt` |
| 2026-05-09 audio focused route summary | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/audio-route-inventory/focused-routes.txt` |
| 2026-05-09 time sync report | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/time-sync/report.txt` |
| 2026-05-09 HTTP Date time-sync service verification | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/http-time-sync-service/deploy-verify.txt` |
| 2026-05-09 HTTP Date time-sync staging check-only probe | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/http-time-sync-service/stage-check-only.txt` |
| 2026-05-09 rootfs policy live check | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/rootfs-policy-check/report.txt` |
| 2026-05-09 policy brightness helper check | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/policy-brightness-helper/live-check.txt` |
| 2026-05-09 staging check-only probe | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/policy-brightness-helper/stage-check-only.txt` |
| 2026-05-09 final health checkpoint | `/vmdata/android/redmik40/experiments/2026-05-09-mainline-resume/final-health.txt` |
| 2026-05-10 qmiwwan2 Ubuntu temporary boot image | `/vmdata/android/redmik40/experiments/20260510-20260510-114300-ai2-modemx-qmiwwan2-ubuntu-boot/lineage-mininitramfs-boot.img` |
| 2026-05-10 qmiwwan2 Ubuntu modem probe | `/vmdata/android/redmik40/experiments/20260510-20260510-114300-ai2-modemx-qmiwwan2-ubuntu-boot/e17-ubuntu-modem-probe.log` |
| 2026-05-10 QRTR/libqmi modem audit | `/vmdata/android/redmik40/experiments/20260510-20260510-114300-ai2-modemx-qmiwwan2-ubuntu-boot/e18-qrtr-libqmi-audit.log` |
| 2026-05-10 baseline restore health after qmiwwan2 test | `/vmdata/android/redmik40/experiments/20260510-20260510-114300-ai2-modemx-qmiwwan2-ubuntu-boot/e19-baseline-health.log` |
| 2026-05-10 Android modem service boundary audit | `/vmdata/android/redmik40/experiments/20260510-20260510-114300-ai2-modemx-qmiwwan2-ubuntu-boot/e20-modem-source-audit.log` |
| 2026-05-10 RMTS/UIO candidate kernel image | `/vmdata/android/redmik40/lineage-sm8250/artifacts/kernel-lineage20-alioth-rmtfs-uio/Image` |
| 2026-05-10 RMTS/UIO Ubuntu temporary boot image (E45) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e45-ubuntu-boot-rmtfs-uio/lineage-mininitramfs-boot.img` |
| 2026-05-10 live `dtbo_b` rollback backup (E43) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e43-dtbo-live-backup/dtbo_b-live.img` |
| 2026-05-10 current patched `dtbo_b` candidate (E46) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e46-live-entry17-minpatch/dtbo_b-live-entry17-minpatch.img` |
| 2026-05-10 RMTS/UIO boot validation report (E47) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e47-dtbo-rmtfs-uio-boot-validate/report.txt` |
| 2026-05-10 `rmt_storage` real UIO smoke report (E48) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e48-rmt-storage-real-uio-smoke/report.txt` |
| 2026-05-10 `rmt_storage` real UIO strace bundle (E48) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e48-rmt-storage-real-uio-smoke/e48-rmt-strace.tar.gz` |
| 2026-05-10 modem helper bundle smoke report (E49) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e49-helper-bundle-smoke/report.txt` |
| 2026-05-10 modem helper bundle logs (E49) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e49-helper-bundle-smoke/e49-helper-logs.tar.gz` |
| 2026-05-10 Android QRTR / esoc boundary audit report (E50) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e50-qrtr-esoc-boundary-audit/report.txt` |
| 2026-05-10 Android QRTR / esoc boundary source snippets (E50) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e50-qrtr-esoc-boundary-audit/host-source-snippets.txt` |
| 2026-05-10 `mdm_helper` stage/link smoke log (E51) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e51-mdm-helper-stage-link.log` |
| 2026-05-10 staged `mdm_helper` copy (E51) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e51-mdm-helper-stage-link/mdm_helper` |
| 2026-05-10 `mdm_helper` bounded execution smoke report (E52) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e52-mdm-helper-bounded-smoke/report.txt` |
| 2026-05-10 `mdm_helper` bounded execution strace bundle (E52) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e52-mdm-helper-bounded-smoke/e52-mdm-helper-strace.tar.gz` |
| 2026-05-10 `mdm_helper` bounded execution helper logs (E52) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e52-mdm-helper-bounded-smoke/e52-helper-logs.tar.gz` |
| 2026-05-10 ESOC source audit report (E53) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e53-esoc-source-audit/report.txt` |
| 2026-05-10 ESOC source audit log (E53) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e53-esoc-source-audit.log` |
| 2026-05-10 `/dev/subsys_esoc0` first trigger smoke report (E54) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e54-subsys-open-trigger-smoke/report.txt` |
| 2026-05-10 reboot-clean after first trigger (E55) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e55-reboot-clean-e45-baseline.log` |
| 2026-05-10 SDX55 firmware alias staging report (E56) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e56-sdx55-firmware-alias-stage/report.txt` |
| 2026-05-10 second trigger with firmware alias report (E57) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e57-subsys-open-trigger-with-firmware-alias/report.txt` |
| 2026-05-10 second trigger with firmware alias strace bundle (E57) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e57-subsys-open-trigger-with-firmware-alias/e57-mdm-helper-strace.tar.gz` |
| 2026-05-10 `ks` wrapper staging directory (E58) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e58-ks-wrapper-stage/` |
| 2026-05-10 `ks` wrapper source (E58) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e58-ks-wrapper-stage/alioth_ks_wrapper.c` |
| 2026-05-10 `ks` wrapper binary (E58) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e58-ks-wrapper-stage/ks` |
| 2026-05-10 reboot-clean after `ks` staging (E59) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e59-reboot-clean-after-ks-stage.log` |
| 2026-05-10 SDX55 `ONLINE` / MHI QMI/rmnet trigger report (E60) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e60-subsys-trigger-with-ks-wrapper/phone-output/report.txt` |
| 2026-05-10 SDX55 `ONLINE` / `ks` strace bundle (E60) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e60-subsys-trigger-with-ks-wrapper/phone-output/e60-mdm-helper-strace.tar.gz` |
| 2026-05-10 SDX55 `ONLINE` helper logs (E60) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e60-subsys-trigger-with-ks-wrapper/phone-output/e60-helper-logs.tar.gz` |
| 2026-05-10 SDX55 `ONLINE` host run log (E60) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e60-subsys-trigger-with-ks-wrapper.log` |
| 2026-05-10 E61 link-only `/system/lib64` ks symlinks artifacts | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e61-system-lib64-ks-symlinks` |
| 2026-05-10 E61 ks strace log (envp=NULL proof) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e61-system-lib64-ks-symlinks/e61-ks-strace.log` |
| 2026-05-10 E61 phone state snapshot | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e61-system-lib64-ks-symlinks/phone-state.txt` |
| 2026-05-10 E62 reboot-clean baseline snapshot | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e62-reboot-clean-after-e61/e62-baseline-snapshot.txt` |
| 2026-05-10 E63 trigger with ks fix script | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e63-trigger-with-ks-fix/e63-subsys-trigger-with-ks-wrapper.sh` |
| 2026-05-10 E63 trigger report | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e63-trigger-with-ks-fix/phone-output/report.txt` |
| 2026-05-10 E63 strace bundle | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e63-trigger-with-ks-fix/phone-output/e63-mdm-helper-strace.tar.gz` |
| 2026-05-10 E63 helper logs | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e63-trigger-with-ks-fix/phone-output/e63-helper-logs.tar.gz` |
| 2026-05-10 E66 QMI probe script (v2) | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e66-qmi-probe/e66-qmi-probe-v2.sh` |
| 2026-05-10 E66 QMI probe report | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e66-qmi-probe/phone-output/report-v2.txt` |
| 2026-05-10 E66 QMI probe phone artifacts | `/vmdata/android/redmik40/experiments/20260510-131830-ai2-modem-live-prereq-inventory/e66-qmi-probe/phone-output/` |
| LELE OS/Arch 回滚 boot image | `/vmdata/android/redmik40/lineage-sm8250/switchroot-shell-ui-docker-screen/lineage-mininitramfs-boot.img` |
| LELE OS/Arch 回滚 initramfs | `/vmdata/android/redmik40/lineage-sm8250/switchroot-shell-ui-docker-screen/initramfs.cpio.gz` |
| Docker-capable kernel | `/vmdata/android/redmik40/lineage-sm8250/artifacts/kernel-lineage20-alioth-docker/Image` |
| Docker-capable config | `/vmdata/android/redmik40/lineage-sm8250/artifacts/kernel-lineage20-alioth-docker/config` |
| Lineage base kernel | `/vmdata/android/redmik40/lineage-sm8250/artifacts/kernel-lineage20-alioth/Image` |
| systemd test kernel | `/vmdata/android/redmik40/lineage-sm8250/artifacts/kernel-lineage20-alioth-systemd/Image` |
| Ubuntu 26.04 rootfs cache | `/vmdata/android/redmik40/rootfs-cache/ubuntu-26.04/resolute-server-cloudimg-arm64-root.tar.xz` |
| Ubuntu 26.04 phone rootfs | `/data/rootfs/ubuntu-26.04` |
| Ubuntu 24.04 rootfs cache | `/vmdata/android/redmik40/rootfs-cache/ubuntu-24.04/noble-server-cloudimg-arm64-root.tar.xz` |
| Ubuntu 24.04 phone rootfs | `/data/rootfs/ubuntu-24.04` |
| Ubuntu temporary switchroot boot image | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu-switchroot/lineage-mininitramfs-boot.img` |
| Ubuntu temporary switchroot initramfs | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu-switchroot/initramfs.cpio.gz` |
| Ubuntu dev+cgroup+route boot image | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu-switchroot-dev-cgroup-route/lineage-mininitramfs-boot.img` |
| Ubuntu dev+cgroup+route initramfs | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu-switchroot-dev-cgroup-route/initramfs.cpio.gz` |
| Ubuntu 24.04 systemd journal boot image | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-systemd-v7-journal/lineage-mininitramfs-boot.img` |
| Ubuntu 24.04 systemd journal initramfs | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-systemd-v7-journal/initramfs.cpio.gz` |
| Ubuntu 24.04 hardware ABI inventory report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-hw-abi-inventory/report.txt` |
| Ubuntu 24.04 input baseline report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-input-baseline/report.txt` |
| Ubuntu 24.04 touch/haptic smoke report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-touch-haptic-smoke/report.txt` |
| Ubuntu 24.04 aw8697 control-path report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-aw8697-path/report.txt` |
| Ubuntu 24.04 USB role/HID readiness report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-usb-role-hid-readiness/report.txt` |
| Ubuntu 24.04 DRM/Mesa readiness report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/report.txt` |
| Ubuntu 24.04 graphics tools install report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/install-graphics-tools.txt` |
| Ubuntu 24.04 graphics probe dry-run report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/graphics-probe-dry-run.txt` |
| Ubuntu 24.04 modetest runtime report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/graphics-probe-modetest.txt` |
| Ubuntu 24.04 EGL/Mesa surfaceless report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/graphics-probe-eglinfo.txt` |
| Ubuntu 24.04 KGSL/Mesa gap report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/kgsl-mesa-gap.txt` |
| Ubuntu 24.04 KGSL node/EGL retry report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/kgsl-node-egl-retry.txt` |
| Ubuntu 24.04 Weston pixman report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/graphics-probe-weston-pixman.txt` |
| Ubuntu 24.04 Weston path retry report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/graphics-probe-weston-pixman-path.txt` |
| Ubuntu 24.04 Weston seat/VT inventory | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/weston-seat-vt-inventory.txt` |
| Ubuntu 24.04 Weston VT retry report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/graphics-probe-weston-pixman-vt.txt` |
| Ubuntu 24.04 Weston openvt report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/graphics-probe-weston-pixman-openvt.txt` |
| Ubuntu 24.04 Weston openvt switch report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/graphics-probe-weston-pixman-openvt-switch.txt` |
| Ubuntu 24.04 Weston builtin non-VT report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/graphics-probe-weston-pixman-builtin-novt.txt` |
| Ubuntu 24.04 Mesa/KGSL strace report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-strace.txt` |
| Ubuntu 24.04 Mesa driver matrix report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-driver-selection-matrix.txt` |
| Ubuntu 24.04 Mesa target report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-gallium-targets.txt` |
| Ubuntu 24.04 libgallium host inspection | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/libgallium-host-target-inspection.txt` |
| Android/Lineage alioth graphics userspace inventory | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/android-lineage-graphics-userspace-inventory.txt` |
| Ubuntu 24.04 KGSL open-path inventory | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/kgsl-open-path-inventory.txt` |
| Ubuntu 24.04 KGSL firmware getproperty smoke | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/kgsl-firmware-getproperty-smoke.txt` |
| Ubuntu 24.04 Mesa/Vulkan loader inventory | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-vulkan-loader-inventory.txt` |
| Ubuntu 24.04 Mesa/Vulkan tools smoke | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-vulkan-tools-smoke.txt` |
| Ubuntu 24.04 pulled freedreno Vulkan ICD | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/libvulkan_freedreno-ubuntu-arm64.so` |
| Ubuntu 24.04 Mesa KGSL build feasibility report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-build-feasibility.txt` |
| Ubuntu 24.04 Mesa KGSL Meson setup report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-meson-setup.txt` |
| Ubuntu 24.04 Mesa KGSL private build report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-build.txt` |
| Ubuntu 24.04 Mesa KGSL git_sha1 retry report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-build-gitsha1-retry.txt` |
| Ubuntu 24.04 Mesa KGSL libdrm include retry report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-build-libdrm-include-retry.txt` |
| Ubuntu 24.04 Mesa KGSL `__user` retry report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-build-define-user-retry.txt` |
| Ubuntu 24.04 private KGSL freedreno Vulkan ICD library | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/libvulkan_freedreno-kgsl-mesa2034-arm64.so` |
| Ubuntu 24.04 private KGSL freedreno ICD JSON | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/freedreno_icd-kgsl-mesa2034-aarch64.json` |
| Ubuntu 24.04 private KGSL Vulkaninfo smoke report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-vulkaninfo-smoke.txt` |
| Ubuntu 24.04 private KGSL no-op submit report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-noop-submit.txt` |
| Ubuntu 24.04 private KGSL compute smoke report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-compute-smoke.txt` |
| Ubuntu 24.04 private KGSL image clear smoke report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-image-clear-smoke.txt` |
| Ubuntu 24.04 private KGSL offscreen triangle smoke report | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-offscreen-triangle-smoke.txt` |
| Ubuntu 24.04 private KGSL visible staged present first attempt | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-visible-staged-present-smoke.txt` |
| Ubuntu 24.04 private KGSL visible staged present retry | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-visible-staged-present-smoke-retry1.txt` |
| Ubuntu 24.04 private KGSL dmabuf capability probe | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-dmabuf-probe.txt` |
| Ubuntu 24.04 private KGSL dmabuf capability probe retry | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-dmabuf-probe-retry1.txt` |
| Ubuntu 24.04 private KGSL dmabuf export/import proof | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-private-dmabuf-export-import.txt` |
| Ubuntu 24.04 DRM PRIME fd -> private KGSL import proof | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-prime-to-kgsl-vulkan-import.txt` |
| Ubuntu 24.04 DRM PRIME KGSL clear readback proof | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-prime-kgsl-clear-readback.txt` |
| Ubuntu 24.04 DRM PRIME KGSL clear present proof | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-prime-kgsl-clear-present.txt` |
| Ubuntu 24.04 private KGSL Vulkan WSI/display probe | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/private-kgsl-vulkan-wsi-display-probe.txt` |
| Ubuntu 24.04 private KGSL Vulkan WSI/display probe retry1 | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/private-kgsl-vulkan-wsi-display-probe-retry1.txt` |
| Ubuntu 24.04 private KGSL Vulkan WSI/display probe retry2 | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/private-kgsl-vulkan-wsi-display-probe-retry2.txt` |
| Ubuntu 24.04 private EGL/GBM/Gallium runtime inventory | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/mesa-kgsl-egl-gbm-glonly-runtime-inventory.txt` |
| Ubuntu 24.04 MSM DRM ioctl capability probe | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-msm-ioctl-probe.txt` |
| Ubuntu 24.04 CPU-only DRM page-flip smoke | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-pageflip-smoke.txt` |
| Ubuntu 24.04 DRM PRIME KGSL double-buffer page-flip clear | `/vmdata/android/redmik40/experiments/2026-05-04-ubuntu2404-drm-mesa-readiness/drm-prime-kgsl-pageflip-clear.txt` |

## 参考镜像

| 文件 | 作用 |
|---|---|
| `images/orangefox-arch-chroot.img` | 已知可启动 OrangeFox + Arch chroot 控制镜像 |
| `images/orangefox-recovery.img` | 原始 OrangeFox recovery 镜像 |
| `images/boot_b.img` | 当前 slot `_b` stock boot 备份 |
| `images/Mu-alioth-1.img` | Mu-Silicium UEFI 测试镜像 |

注意：2026-05-04 23:18 CST 已将 Ubuntu 24.04 v8e boot image 写入设备当前活动槽 `_b` 的 `boot_b`。当前正常开机默认进入 Ubuntu 24.04 LTS systemd。旧 LELE OS/Arch 监控镜像仍保留在 `/vmdata/android/redmik40/lineage-sm8250/switchroot-shell-ui-docker-screen/lineage-mininitramfs-boot.img`，作为回滚镜像。本地 `images/boot_b.img` 仍作为原 stock boot 备份保留。

Ubuntu 26.04 rootfs 来自官方 Ubuntu cloud image `resolute/current` arm64 root tarball，SHA256 为 `ef56b62a89f38909f60e181c9ee4494f42c2780f947b13bbb96f07e382e6f341`。2026-05-04 已通过临时 `fastboot boot` + `switch_root` smoke test；后续确认 systemd 259 可成为 PID1，但服务启动路径在当前 4.19 kernel 上失败：`Failed to spawn executor: Function not implemented`。因此 26.04 暂不作为 systemd 主线优先目标。

Ubuntu 24.04 rootfs 来自官方 Ubuntu cloud image `noble/current` arm64 root tarball，SHA256 为 `3c8f36e427583571cb5536b339355b9d9b60c233eb7aed5e51c41b3ceba5accf`，手机侧路径为 `/data/rootfs/ubuntu-24.04`。当前默认启动镜像是 24.04 GPU monitor service 镜像：boot image SHA256 为 `19e159007fb04e009f840b09dfb33b68808ac45618403c63e80f2851e7608a23`，initramfs SHA256 为 `4f399f55fcba7201a7d2384080f40f72f7d27d88550a38c5c4b0a86af9efdfa5`。该镜像已验证非 chroot Ubuntu 24.04、systemd 255 PID1、基础 service spawn、USB SSH、2323 fallback shell、GPU-first 状态 UI、输入按键节点、DNS、`apt-get update`、build-epoch 时间下限、HTTP Date 校时、128M capped persistent journal，以及 systemd-owned 单个 `alioth-usb-dhcpd` listener，且 `systemctl is-system-running` 为 `running`、0 failed units。状态 UI 默认优先使用 private KGSL Turnip + DRM/KMS page-flip 绘制 runtime-status 页面，包括 systemd failure count、电池、USB、Wi-Fi、audio ADSP/SND、launcher/menu hints 和系统详情，失败时回退到原静态 CPU DRM/KMS UI；这仍不是 Ubuntu generic kernel 裸机安装。

## 重要脚本

| 脚本 | 作用 |
|---|---|
| `scripts/build_lineage_sm8250_alioth_kernel.sh` | 构建当前主线 Lineage 20 kernel |
| `scripts/build_lineage_mininitramfs_boot.sh` | 构建自制 initramfs boot image，可选 shell/switchroot/systemd 模式 |
| `scripts/prep_ubuntu_rootfs_policy.sh` | 对已解压 Ubuntu rootfs 执行/检查 LELE rootfs 策略 |
| `scripts/stage_ubuntu_rootfs_to_phone.sh` | 从主机 rootfs tarball 缓存向手机 userdata staged Ubuntu rootfs |
| `scripts/alioth_ubuntu_hw_inventory.sh` | 通过 SSH 只读采集当前 Ubuntu 硬件 ABI 状态 |
| `scripts/validate_alioth_kernel_candidate.sh` | 用基准 ramdisk 打包候选 kernel 并可选 `fastboot boot` |
| `scripts/start_alioth_usb_socks.sh` | 主机侧建立手机反向 SOCKS 出网 |
| `scripts/alioth_phone_data_mount.sh` | 手机侧挂载 `/data` 并 bind 服务目录 |
| `scripts/alioth_phone_docker_start.sh` | 手机侧启动 Docker |
| `scripts/alioth_phone_services_start.sh` | 手机侧启动 nginx、Valkey、PostgreSQL |
| `scripts/alioth_phone_ap_start.sh` | 手机侧启动 Wi-Fi 配置热点 |
| `scripts/alioth_phone_ap_stop.sh` | 手机侧停止配置热点 |
| `scripts/alioth_phone_ap_status.sh` | 手机侧输出 AP 状态 |
| `scripts/alioth_wifi_portal.py` | Wi-Fi 配置 portal |
| `scripts/alioth_phone_torch.sh` | 安全手电 helper |
| `scripts/alioth_phone_brightness.sh` | 安全亮度 helper，部署为手机侧 `lele-brightness`；支持 status/get/set/percent/dim，dim 会自动恢复 |
| `scripts/alioth_http_time_sync.sh` | best-effort HTTP Date UTC 校时 helper，部署为手机侧 `alioth-http-time-sync` 并由 `alioth-http-time-sync.service` 开机 oneshot 运行 |
| `scripts/alioth_haptic_pulse.py` | 最小 Linux EV_FF haptic pulse helper；aw8697 需用 `--effect constant`，播放后 `EVIOCRMFF` cleanup 可能只给 warning |
| `scripts/alioth_usb_host_probe.sh` | USB host/HID 探针 helper；默认 dry-run，真实切换需用 `systemd-run ... --execute` 并预期 SSH 临时断开 |
| `scripts/alioth_graphics_probe.sh` | 图形 runtime 探针 helper；默认 dry-run，live test 会临时停止并恢复 `lele-status-ui`，当前 SHA256 `323d5a1c50c8f61297c54cf3693a177d20c8d083c25affc4d62bef646a502929` |
| `scripts/alioth_kgsl_getprop_probe.py` | KGSL `GETPROPERTY` smoke helper；只打开 `/dev/kgsl-3d0` 并读取只读属性，不运行 Mesa/Vulkan/GL |
| `scripts/alioth_vulkan_noop_submit.c` | 私有 KGSL Turnip no-surface Vulkan helper；创建 device/queue/command buffer 并提交空 command buffer，用于确认 `IOCTL_KGSL_GPU_COMMAND` |
| `scripts/alioth_vulkan_compute_smoke.c` | 私有 KGSL Turnip no-surface compute helper；创建 storage buffer 和 compute pipeline，dispatch 后回读 `0x5a17c0de` |
| `scripts/alioth_vulkan_compute_smoke.comp` | 上面 compute helper 使用的最小 GLSL compute shader |
| `scripts/alioth_vulkan_image_clear_smoke.c` | 私有 KGSL Turnip no-surface image transfer helper；清空小 RGBA image、copy 到 buffer 并校验像素 |
| `scripts/alioth_vulkan_offscreen_triangle_smoke.c` | 私有 KGSL Turnip offscreen graphics helper；render pass + framebuffer + graphics pipeline + draw + readback |
| `scripts/alioth_vulkan_offscreen_triangle.vert` | offscreen triangle helper 的最小 vertex shader |
| `scripts/alioth_vulkan_offscreen_triangle.frag` | offscreen triangle helper 的最小 fragment shader |
| `scripts/alioth_vulkan_textured_quad_smoke.c` | 私有 KGSL Turnip sampled texture helper；descriptor set + sampler + texture image + offscreen render/readback |
| `scripts/alioth_vulkan_textured_quad.vert` | textured quad helper 的 full-screen triangle vertex shader |
| `scripts/alioth_vulkan_textured_quad.frag` | textured quad helper 的 combined image sampler fragment shader |
| `scripts/alioth_drm_prime_vulkan_glyph_text_pageflip.c` | custom GPU monitor/glyph text direct-scanout proof：读取系统状态 + CPU glyph atlas + vertex-buffer glyph quads + KGSL Vulkan render pass + KMS page-flip |
| `scripts/alioth_gpu_status_ui_wrapper.sh` | 默认状态 UI wrapper：创建 DRM/KGSL 节点，优先启动 `/data/experiments/alioth_gpu_monitor_service`，失败时回退 CPU UI |
| `scripts/alioth_wayland_cage_session.sh` | 临时 cage/wlroots kiosk Wayland 会话 launcher；停止 GPU monitor、运行客户端/截图、退出后恢复监控 |
| `scripts/alioth_wayland_labwc_session.sh` | 临时 labwc/wlroots desktop-smoke launcher；运行 `foot+swaybg+fuzzel+wlr-randr`，截图后恢复监控 |
| `scripts/alioth_labwc_touch_smoke.sh` | 可复现实验：启动 labwc desktop-smoke，使用 evemu-event 注入触控并记录 libinput 事件窗口，用于后续触控可用性回归；当前 SHA256 `092d522f99354de26c3be84a8cd7f2582b9eb2372b283dfbe56507168c81edf7` |
| `scripts/sync_alioth_ubuntu2404_sysroot.sh` | 从当前手机 Ubuntu 24.04 同步 arm64 headers/libs 到主机 sysroot |
| `scripts/build_alioth_gpu_monitor_cross.sh` | 使用主机 Docker aarch64 工具链和手机 sysroot 交叉编译 GPU monitor |
| `docker/Dockerfile.cross-ubuntu2404` | 可选 Ubuntu 24.04 交叉工具链镜像；当前外网 apt 不稳定，默认仍用既有 `redmik40-kernel-builder:bullseye` |
| `scripts/alioth_vulkan_glyph_text.vert` | glyph text helper 的 vertex-buffer position/UV shader |
| `scripts/alioth_vulkan_glyph_text.frag` | glyph text helper 的 atlas sampler + transparent texel discard fragment shader |
| `scripts/alioth_drm_present_raw_xrgb.c` | DRM dumb-buffer presenter；读取 XRGB raw，缩放到当前 panel 模式并短时显示，用于 CPU-staged 可见 smoke |
| `scripts/alioth_vulkan_dmabuf_probe.c` | 私有 KGSL Turnip external-memory/dmabuf 能力探针；只查询扩展和 image format external memory properties |
| `scripts/alioth_vulkan_dmabuf_export_import.c` | 私有 KGSL Turnip dma-buf export/import proof；当前证明 KGSL allocation export 命中 Mesa stub |
| `scripts/alioth_drm_prime_to_vulkan_import.c` | DRM dumb PRIME fd 导出并导入私有 KGSL Vulkan image 的最小 proof |
| `scripts/alioth_drm_prime_vulkan_clear_readback.c` | KGSL 清色写入 imported DRM-owned buffer，并通过 DRM mmap/dma-buf sync 读回验证 |
| `scripts/alioth_drm_prime_vulkan_clear_present.c` | 全屏 DRM-owned PRIME buffer 由 KGSL 写入后直接 KMS scanout 的短时可见 proof |
| `scripts/alioth_vulkan_display_wsi_probe.c` | 私有 KGSL Turnip KHR_display/WSI 能力探针；当前证明启用 `VK_KHR_display` 后会在枚举 physical device 时崩溃 |
| `scripts/alioth_drm_msm_ioctl_probe.c` | MSM DRM GPU ioctl 能力探针；当前证明 `/dev/dri/renderD128`/`card0` 的 `msm_drm` 节点不提供 freedreno Gallium 所需 GPU params/IOVA |
| `scripts/alioth_drm_pageflip_smoke.c` | CPU-only DRM dumb-buffer page-flip smoke；验证 KMS page-flip event path |
| `scripts/alioth_drm_prime_vulkan_pageflip_clear.c` | 当前 custom graphics foundation：DRM PRIME 双缓冲 + KGSL Vulkan clear + CPU sample + KMS page-flip |
| `scripts/alioth_drm_prime_vulkan_triangle_pageflip.c` | custom graphics render-loop proof：DRM PRIME 双缓冲 + KGSL Vulkan render pass/triangle + CPU sample + KMS page-flip |
| `scripts/alioth_drm_prime_vulkan_textured_pageflip.c` | custom textured direct-scanout proof：DRM PRIME 双缓冲 + KGSL Vulkan sampled texture render pass + CPU sample + KMS page-flip |
| `scripts/package_redmik40_handoff.sh` | 打包交接资料 |

## initramfs / 手机侧源码

| 路径 | 作用 |
|---|---|
| `src/alioth-status-ui-c/alioth_status_ui.c` | 当前 C 版 DRM/KMS 状态 UI，包含 Ubuntu/systemd 信息、POWER 面板充电速率显示和电源键二次确认关机 |
| `src/alioth-status-ui/main.go` | 早期 Go 版状态 UI，legacy |
| `src/alioth-usb-dhcpd/alioth_usb_dhcpd.c` | 手机侧 USB DHCP server |
| `src/alioth-reboot/alioth_reboot.c` | `reboot-bootloader` helper |
| `src/minitcpsh/minitcpsh.c` | initramfs / switchroot 备用 TCP shell |

## 验证记录目录

```text
/vmdata/android/redmik40/boot-validation/
```

关键记录：

- `2026-04-26-lineage20-kernel`
- `2026-04-26-lineage-mininitramfs`
- `2026-04-26-lineage-mininitramfs-ui`
- `2026-04-26-switchroot-shell`
- `2026-04-26-usb-dhcp-ssh`
- `2026-04-26-stock-kernel`

汇总时间线见 `docs/boot-validation-log.md`。
