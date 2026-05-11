# Environment

Last refreshed: 2026-05-11 16:25 CST.

This file records what is actually usable from the current host checkout. It intentionally separates local facts from historical paths referenced by older documents.

## Host

| Item | Value |
|---|---|
| Host OS | macOS 26.3.1 |
| Kernel | Darwin 25.3.0 x86_64 |
| User | `wuyuele` |
| Shell | `/bin/zsh` |
| Repository | `/Users/wuyuele/redmik40-headless` |
| Timezone observed | CST, host date command at `2026-05-11 16:25:55 CST` |

Disk observation for the current checkout:

```text
/System/Volumes/Data: 466 GiB total, 233 GiB used, 214 GiB available
```

## Available host tools

| Tool | Path |
|---|---|
| adb | `/Users/wuyuele/soft/platform-tools/adb` in `PATH`; repo copy also exists |
| fastboot | `/Users/wuyuele/soft/platform-tools/fastboot` in `PATH`; repo copy also exists |
| ssh | `/usr/bin/ssh` |
| scp | `/usr/bin/scp` |
| nc | `/usr/bin/nc` |
| ping | `/sbin/ping` |
| shasum | `/usr/bin/shasum` |
| file | `/usr/bin/file` |
| make | `/usr/bin/make` |
| clang | `/usr/bin/clang` |
| gcc | `/usr/bin/gcc` |
| python3 | `/usr/bin/python3` |
| git | `/usr/bin/git` |
| rsync | `/usr/bin/rsync` |

Repo-local Android platform tools:

```text
tools/platform-tools/adb      Android Debug Bridge 37.0.0-14910828
tools/platform-tools/fastboot fastboot 37.0.0-14910828
```

Prefer repo-local platform tools in project commands to avoid PATH ambiguity:

```bash
./tools/platform-tools/adb version
./tools/platform-tools/fastboot --version
```

## Local repository resources

| Resource | State |
|---|---|
| Project checkout | present at `/Users/wuyuele/redmik40-headless` |
| Platform tools | present under `tools/platform-tools` |
| Static busybox | present at `tools/busybox-aarch64` |
| mkbootimg | present at `tools/mkbootimg.py` |
| Control boot image | present at `artifacts/control/lineage-mininitramfs-boot.img` |

Control boot image details:

```text
path:   artifacts/control/lineage-mininitramfs-boot.img
size:   46 MiB
type:   Android bootimg, kernel (0x1a78da)
sha256: 19e159007fb04e009f840b09dfb33b68808ac45618403c63e80f2851e7608a23
```

No SSH private key or known_hosts file was found inside this repository during the local scan.

## Historical or currently unavailable resources

The following paths are heavily referenced by older docs, but were missing on the current macOS host at refresh time:

| Path | Current host state |
|---|---|
| `/vmdata` | missing |
| `/vmdata/android/redmik40` | missing |
| `/vmdata/android/redmik40/experiments/2026-05-05-ubuntu2404-gpu-monitor-service/lineage-mininitramfs-boot.img` | missing |
| `/vmdata/android/redmik40/keys/alioth_usb_ed25519` | missing |
| `/vmdata/android/redmik40-lineageos/src` | missing |
| `/data/rootfs/ubuntu-24.04` | missing on host; this is a phone-side path in the documented baseline |

Do not paste `/vmdata/...` commands directly on this host unless those resources are mounted or recreated.

## Source and remote build policy

Do not treat a full local Android/LineageOS source checkout as a requirement for the current macOS workflow. Large source trees from previous Linux hosts can be treated as historical references unless they are mounted and intentionally selected for a specific build.

Current practical constraint:

```text
Public network bandwidth is about 5 MB/s.
```

Because of that, remote build machines should keep source trees and build caches in place. The local host should exchange only small inputs and final artifacts:

```text
send:    manifest, patches, configs, helper scripts
receive: boot.img, Image, dtb/dtbo, modules, config, SHA256, compressed logs
avoid:   full repo sync locally, uploading full source trees, pulling full out/ directories
```

## Device reachability snapshot

At refresh time:

| Probe | Result |
|---|---|
| `ping -c 1 172.16.42.2` | success, 0.795 ms |
| `nc -z -G 1 172.16.42.2 22` | TCP 22 open |

This proves network reachability and an open SSH port only. It does not prove SSH authentication, current slot, rootfs health, systemd state, or the active boot image.

Follow-up read-only probe result:

| Probe | Result |
|---|---|
| SSH batch login | failed: `Permission denied (publickey,password)` |
| TCP shell `2323/tcp` | open and accepted root commands |
| Live slot from `/proc/cmdline` | `_a` |
| Live root | Arch Linux ARM on `/dev/block/by-name/arch` |
| Live PID1 | BusyBox `alioth-switch-init switchroot-shell` |
| Ubuntu rootfs | present at `/data/rootfs/ubuntu-24.04`, not PID1 |

Treat `2323/tcp` as the current working rescue channel. Because it is a root shell, avoid exposing this network path beyond the trusted USB/NCM link.

## Practical command prefixes

Use these from the repository root:

```bash
./tools/platform-tools/fastboot devices
./tools/platform-tools/fastboot getvar current-slot
./tools/platform-tools/fastboot boot artifacts/control/lineage-mininitramfs-boot.img
```

Only use `fastboot flash` after writing a rollback plan into `boot-validation-log.md`.

If an SSH key is provided later, prefer a repository-local or documented absolute key path and record it here before using it in runbook commands.
