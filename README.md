# Redmi K40 Headless Linux 小服务器

> 目标：将 Redmi K40 (alioth) 打造为一台稳定运行的 ARM64 原生 Linux 随身服务器 + 本机状态面板。
> 仓库创建：2026-05-11

## 项目定位

不追求完整手机能力（基带、GPS、摄像头、指纹等），而是把 K40 做成一台：

```text
ARM64 原生 Linux 随身服务器
+ Wi-Fi / SSH / Tailscale / FRP 远程访问
+ 本机屏幕作为固定状态显示 Panel（GPU/Vulkan/DRM 直接渲染，不走桌面栈）
+ 可靠开机自启、断电恢复、无人值守运行
```

屏幕不是"Linux 桌面"，而是类似路由器/NAS/工业 HMI/服务器 LCD 的状态面板，只展示固定信息：IP、Wi-Fi、SSH、温度、电量、负载、服务状态。

**状态 Panel 渲染链路**：`LVGL/Vulkan glyph text → DRM/KMS pageflip → /dev/dri/card0 → 屏幕`

不经过 Wayland/X11/compositor，是嵌入式风格直接渲染。

## 工程结构

```text
redmik40-headless/
├── README.md                  # 本文件，项目入口
├── k40_headless_panel_conclusion.md  # headless 方向原始分析
├── scripts/                   # 手机端脚本 + 主机端构建/部署脚本
│   ├── alioth_wifi_*.sh       # Wi-Fi 启动/连接/扫描/状态
│   ├── alioth_audio_adsp_boot.sh  # ADSP 音频子系统启动
│   ├── alioth-subsys-boot.sh  # SLPI + CDSP 子系统启动
│   ├── alioth_bt_qca_init.sh  # 蓝牙固件加载
│   ├── alioth_http_time_sync.sh   # HTTP Date 校时
│   ├── alioth_gpu_status_ui_wrapper.sh  # GPU Monitor 状态面板启动 wrapper
│   ├── alioth_drm_prime_vulkan_*.c  # DRM/KMS + Vulkan 直接渲染（Panel 基础）
│   ├── alioth_vulkan_*.c/.vert/.frag/.comp  # Vulkan 渲染测试和 glyph text
│   ├── alioth_phone_*.sh      # 手机端辅助工具（亮度/手电/Docker/AP/数据挂载）
│   ├── alioth_wifi_portal.py  # AP 模式 Wi-Fi 配置 Portal
│   ├── build_*.sh             # 内核/boot 镜像/GPU monitor 构建
│   ├── prep_ubuntu_rootfs_policy.sh  # Ubuntu rootfs 策略配置
│   └── stage_ubuntu_rootfs_to_phone.sh  # rootfs 部署到手机
├── configs/                   # systemd 服务文件
│   ├── alioth-wifi-prepare.service
│   ├── alioth-wifi-bringup.service
│   ├── alioth-qrtr-ns.service
│   ├── alioth-cnss-daemon.service
│   ├── alioth-audio-adsp-boot.service
│   ├── alioth-http-time-sync.service
│   ├── alioth-bt-qca-init.service
│   └── stock-4.19.157-perf.config  # stock 内核配置参考
├── docs/                      # 项目文档
│   ├── runbook.md             # 操作手册
│   ├── current-state.md       # 当前接手状态和稳定基线
│   ├── environment.md         # 当前主机可用资源、工具、路径差异
│   ├── experiment-protocol.md # 实验前/后记录规范
│   ├── boot-validation-log.md # boot/刷写/验证日志入口
│   ├── k40_headless_panel_conclusion.md
│   ├── hardware-abi-status.md
│   ├── project-overview.md
│   └── resources.md
├── tools/                     # 主机端工具
│   ├── platform-tools/        # adb, fastboot (macOS)
│   ├── busybox-aarch64        # aarch64 静态 busybox
│   └── mkbootimg.py           # Android boot image 打包
└── artifacts/
    └── control/
        └── lineage-mininitramfs-boot.img  # 已验证的下游 boot 镜像
├── docker/
│   └── kernel-builder/        # 本机 Docker kernel 编译环境
```

## 当前可继承的基线

| 能力 | 状态 | 对 headless 的价值 |
|------|------|-------------------|
| Lineage 4.19.312 内核 | 稳定 | 基础运行保障 |
| Ubuntu 24.04 systemd | 稳定 | 服务管理基础 |
| USB NCM + SSH | 稳定 | 主要远程入口 |
| Wi-Fi (QCA6390) | 稳定 | 网络连通 |
| DRM/KMS 显示 | 稳定 | 状态屏基础 |
| GPU Monitor 状态 UI | 稳定 | 当前 Panel 方案 |
| KGSL Vulkan pageflip | 已验证 | Panel 直接渲染链路 |
| Glyph text pageflip | 已验证 | Panel 文字显示 |
| 电池 sysfs | 可读 | 电池监控 |
| 亮度控制 | 已验证 | 省电/熄屏 |
| ADSP/ALSA 枚举 | 已验证 | 音频子系统就绪 |
| 触摸 (fts_ts) | 已验证 | 可选 Panel 交互 |

## 不投入的能力

以下属于"完整手机/桌面"方向，本工程不追求：

- 完整 Wayland 桌面会话（cage/labwc/Phosh/KDE/GNOME）
- 摄像头、基带/移动数据、GPS、指纹
- 复杂音频链路（扬声器route调优等）
- 桌面 compositor 加速（EGL/GBM swapchain）

## 设备关键事实

- 设备：Redmi K40 / POCO F3, codename alioth
- SoC：Qualcomm Snapdragon 870 / SM8250-AC
- 分区模型：Virtual A/B + dynamic partitions
- 当前活动槽：`_b`

## 计划

### 阶段 1：最小可用服务器

| # | 任务 | 说明 |
|---|------|------|
| 1.1 | 冻结可用 boot 镜像 | 记录 SHA256，确保可复现 |
| 1.2 | SSH 开机自启 | sshd 在 systemd 中 enabled |
| 1.3 | Wi-Fi 自动连接 | wpa_supplicant + alioth-wifi-connect |
| 1.4 | 固定 IP 或 mDNS | 方便局域网发现 |
| 1.5 | 禁止自动睡眠/suspend | `systemctl mask sleep.target suspend.target` |
| 1.6 | 日志持久化 | `/var/log/journal` 持久化 |
| 1.7 | 电池/温度监控脚本 | 定期采集，超阈值告警 |
| 1.8 | GPU Monitor 状态面板 | DRM/KMS + Vulkan glyph text 直接渲染 |

### 阶段 2：远程访问增强

| # | 任务 | 说明 |
|---|------|------|
| 2.1 | Tailscale 安装配置 | 零配置 VPN |
| 2.2 | FRP 内网穿透 | 备选方案 |
| 2.3 | SSH key 认证 | 禁用密码登录 |
| 2.4 | Web 管理页（可选） | 轻量状态/控制 API |

### 阶段 3：服务器工作负载

| # | 任务 | 说明 |
|---|------|------|
| 3.1 | Docker/容器运行时 | 检查 4.19 内核兼容性 |
| 3.2 | nginx/Caddy 反代 | Web 服务 |
| 3.3 | 数据持久化方案 | /data 分区或独立挂载 |
| 3.4 | 自动备份脚本 | 关键数据定期 rsync |
| 3.5 | tmux/screen 持久会话 | 远程任务不因断连丢失 |

### 阶段 4：状态 Panel 升级

| # | 任务 | 说明 |
|---|------|------|
| 4.1 | 二维码显示 | SSH 地址、Web 管理页 URL |
| 4.2 | 可选触摸按钮 | 重启/关机/切 Wi-Fi |
| 4.3 | LVGL + DRM Panel | 更嵌入式风格的图形面板 |
| 4.4 | 自动熄屏/亮屏策略 | 省电，按键唤醒 |

### 阶段 5：可靠性加固

| # | 任务 | 说明 |
|---|------|------|
| 5.1 | 充电稳定性评估 | 长期无人值守是否安全 |
| 5.2 | 温度过热保护 | 自动降频/关机阈值 |
| 5.3 | 看门狗 | systemd watchdog 或硬件看门狗 |
| 5.4 | USB Gadget 网络救急 | Wi-Fi 失联时的备选入口 |
| 5.5 | 根文件系统只读化 | 降低意外断电损坏风险 |

## 永久性约束

1. **安全第一**：优先 `fastboot boot`，慎用 `fastboot flash`
2. **禁止无回滚刷写**：不刷 vendor_boot/dtbo/vbmeta/super/modem/bootloader/firmware 除非有回滚计划
3. **小步实验**：每次只改一个关键变量
4. **Headless 优先**：不投入桌面/手机专属能力，除非明确为 Panel 服务
5. **稳定性高于功能**：宁可少一个功能，也不要一个不稳定的功能
6. **不依赖桌面栈**：状态屏走 DRM/KMS + Vulkan 直接渲染，不走 Wayland/X11/compositor
7. **资源自理**：本工程自包含，不依赖 `~/redmik40-ai2` 或其他外部目录
8. **文档入口**：新对话先读本 README，再按需要读 docs/ 下具体文档
9. **充电风险**：长期无人值守插电需先评估安全性
