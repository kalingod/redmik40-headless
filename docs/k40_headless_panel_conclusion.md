# Redmi K40 原生 Linux 后续方向结论：Headless 小服务器 + 固定状态 Panel

> 生成日期：2026-05-11  
> 设备：Redmi K40 / alioth  
> 当前状态：原生 boot.img 魔改路线，内核可启动，rootfs 已存在，Wi-Fi 已可用，基带/音频/桌面等深度硬件能力暂不可用。

---

## 1. 总结结论

当前阶段最有价值的方向不是继续强行移植完整 Linux 桌面，而是将 Redmi K40 定位为：

```text
ARM64 原生 Linux 随身小服务器
+ Wi-Fi / SSH / Tailscale / FRP 远程访问
+ 本机屏幕仅作为固定状态显示 Panel
```

也就是说，屏幕仍然存在，但不作为“发行版 Linux 桌面”使用，而是作为类似路由器、NAS、工业设备 HMI、服务器 LCD 的状态面板，只展示固定信息：IP、Wi-Fi、SSH、温度、电量、负载、服务状态等。

这个方向比完整桌面简单得多，也更符合当前已经打通 Wi-Fi 和 rootfs 的项目进度。

---

## 2. 为什么不建议优先做完整桌面

完整 Linux 桌面需要打通一整条复杂图形链路：

```text
屏幕 panel 驱动
→ DRM/KMS
→ Adreno GPU
→ Mesa / Freedreno
→ Wayland / X11
→ 输入设备 / 触摸
→ 屏幕旋转
→ 窗口管理器 / 合成器
→ Phosh / Sway / KDE / GNOME
```

这条链路每一层都可能失败，尤其是手机平台上的显示、GPU、触摸、旋转、合成器、电源管理和权限管理。

如果你的目标只是让 K40 成为一台可用的 Linux 节点，那么完整桌面投入产出比并不高。

---

## 3. 推荐的新定位

建议将项目目标从：

```text
红米 K40 = Linux 桌面手机
```

改为：

```text
红米 K40 = Headless Linux 小服务器 + 本机状态屏
```

核心使用方式仍然是远程访问：

```text
SSH
Tailscale
FRP
Web 管理页
rsync / git / tmux / systemd services
```

本机屏幕只负责告诉你：

```text
设备是否活着
当前 IP 是多少
Wi-Fi 是否在线
SSH / Tailscale / FRP 是否运行
电量和充电状态
温度是否异常
CPU / 内存 / 存储负载
```

---

## 4. 推荐系统架构

```text
boot.img / vendor_boot / dtbo
        ↓
Linux kernel + DTB + firmware
        ↓
Arch Linux ARM / Debian ARM64 rootfs
        ↓
systemd multi-user.target
        ↓
NetworkManager / sshd / tailscaled / frpc
        ↓
状态采集脚本
        ↓
固定全屏 panel 程序
        ↓
屏幕显示状态信息
```

这不是 desktop-first，而是：

```text
headless-first + local status panel
```

---

## 5. 优先级排序

建议按以下顺序推进：

```text
1. 冻结当前 Wi-Fi 可用 boot.img / vendor_boot / dtbo
2. SSH 开机自动启动
3. Wi-Fi 自动连接
4. 固定 IP 或 Tailscale / FRP 远程访问
5. 日志持久化
6. 禁止自动睡眠 / suspend
7. 电池、充电、温度监控
8. TTY 文本状态屏
9. USB Gadget 网络救援通道
10. LVGL / framebuffer / DRM 状态 Panel
11. 可选触摸按钮
12. 最后再考虑完整桌面
```

---

## 6. 当前最小可用目标

第一阶段只追求：

```text
开机后自动进入 Linux
自动连接 Wi-Fi
自动启动 sshd
局域网内可 SSH 登录
可通过 Tailscale / FRP 远程登录
屏幕显示 IP、负载、电量、温度、服务状态
断电或重启后能自动恢复
```

达到这个程度，K40 就已经不是“半成品 Linux 手机”，而是一台真正可用的 ARM64 Linux 小服务器。

---

## 7. Panel 显示内容建议

固定状态面板可以显示：

```text
Redmi K40 Linux Node

Hostname : k40-node
IP       : 192.168.1.240
Wi-Fi    : connected / signal xx%
SSH      : active
Tailscale: active
FRP      : active
Uptime   : 3h 21m
Load     : 0.32 0.40 0.38
Memory   : 612M / 6G
Rootfs   : 8.4G / 64G
Battery  : 82%, charging
Temp     : 38.5°C
Kernel   : 5.x / 6.x
Rootfs   : Arch Linux ARM
```

可以扩展显示二维码：

```text
ssh://lele@192.168.1.240
http://192.168.1.240:8080
Tailscale 访问地址
```

---

## 8. Panel 技术路线

### 8.1 第一版：TTY 文本状态屏

最简单，最推荐先做。

依赖最少：

```text
不需要桌面
不需要 Wayland
不需要 X11
不需要 Mesa
不需要 GPU 加速
```

只要屏幕能作为 console 输出，就可以使用 systemd 在 `/dev/tty1` 上循环显示状态。

---

### 8.2 第二版：Framebuffer 状态屏

如果系统存在：

```bash
/dev/fb0
```

可以通过 framebuffer 直接画图形界面。

可选方案：

```text
LVGL + fbdev
SDL + fbdev
自写 framebuffer 程序
```

优点是比桌面轻很多，缺点是 framebuffer 较老，旋转、颜色格式、刷新可能需要调。

---

### 8.3 第三版：DRM/KMS + LVGL

如果系统存在：

```bash
/dev/dri/card0
```

可以走更现代的 DRM/KMS 方式。

推荐架构：

```text
k40-panel
↓
LVGL
↓
libdrm / DRM/KMS
↓
/dev/dri/card0
↓
屏幕
```

这是中期最推荐路线：轻量、嵌入式风格强、不需要完整桌面。

---

### 8.4 第四版：Qt/QML EGLFS

如果想做得更漂亮，可以用 Qt/QML 写单应用全屏 Panel：

```bash
QT_QPA_PLATFORM=eglfs /usr/local/bin/k40-panel
```

或者：

```bash
QT_QPA_PLATFORM=linuxfb /usr/local/bin/k40-panel
```

优点是 UI 开发效率高，缺点是依赖比 LVGL 重，调试复杂度也更高。

---

## 9. 第一版 TTY 状态屏示例

`/usr/local/bin/k40-status-panel.sh`：

```bash
#!/bin/bash

while true; do
  clear

  echo "================================"
  echo "      Redmi K40 Linux Node"
  echo "================================"
  echo
  echo "Hostname : $(hostname)"
  echo "IP       : $(hostname -I | awk '{print $1}')"
  echo "Uptime   : $(uptime -p)"
  echo "Load     : $(cat /proc/loadavg | awk '{print $1, $2, $3}')"
  echo

  if [ -f /sys/class/power_supply/battery/capacity ]; then
    echo "Battery  : $(cat /sys/class/power_supply/battery/capacity)%"
  else
    echo "Battery  : N/A"
  fi

  if [ -f /sys/class/power_supply/battery/status ]; then
    echo "Charging : $(cat /sys/class/power_supply/battery/status)"
  else
    echo "Charging : N/A"
  fi

  echo
  echo "Services:"
  systemctl is-active --quiet sshd && echo "  SSH       : active" || echo "  SSH       : inactive"
  systemctl is-active --quiet NetworkManager && echo "  Network   : active" || echo "  Network   : inactive"
  systemctl is-active --quiet tailscaled && echo "  Tailscale : active" || echo "  Tailscale : inactive"
  systemctl is-active --quiet frpc && echo "  FRP       : active" || echo "  FRP       : inactive"

  echo
  echo "Updated: $(date '+%F %T')"
  sleep 5
done
```

systemd 服务：

```ini
[Unit]
Description=K40 Status Panel
After=multi-user.target network-online.target
Wants=network-online.target

[Service]
Type=simple
ExecStart=/usr/local/bin/k40-status-panel.sh
StandardInput=tty
StandardOutput=tty
TTYPath=/dev/tty1
Restart=always
RestartSec=2

[Install]
WantedBy=multi-user.target
```

保存为：

```bash
/etc/systemd/system/k40-status-panel.service
```

启用：

```bash
chmod +x /usr/local/bin/k40-status-panel.sh
systemctl enable k40-status-panel.service
```

---

## 10. Headless 必做配置

### 10.1 SSH

```bash
pacman -S openssh sudo
systemctl enable sshd
```

### 10.2 NetworkManager

```bash
pacman -S networkmanager
systemctl enable NetworkManager
nmcli dev wifi connect "你的WiFi名" password "你的WiFi密码"
nmcli connection modify "你的WiFi名" connection.autoconnect yes
```

### 10.3 禁止自动睡眠

```bash
systemctl mask sleep.target suspend.target hibernate.target hybrid-sleep.target
```

可选修改 `/etc/systemd/logind.conf`：

```ini
HandlePowerKey=ignore
HandleSuspendKey=ignore
HandleHibernateKey=ignore
IdleAction=ignore
```

### 10.4 日志持久化

```bash
mkdir -p /var/log/journal
systemctl restart systemd-journald
```

---

## 11. 需要重点关注的风险

### 11.1 充电与电池管理

检查：

```bash
ls /sys/class/power_supply/
cat /sys/class/power_supply/battery/capacity
cat /sys/class/power_supply/battery/status
cat /sys/class/power_supply/battery/temp
```

如果充电状态不稳定，不建议长期无人值守插电运行。

### 11.2 温度

检查：

```bash
find /sys/class/thermal/thermal_zone*/temp -type f -exec sh -c 'echo "$1: $(cat $1)"' _ {} \;
```

手机散热有限，不建议一开始就长期跑高负载编译。

### 11.3 Wi-Fi 救援

如果 Wi-Fi 配坏，headless 设备可能失联。建议保留至少一种救援方式：

```text
TWRP / recovery
fastboot
USB Gadget 网络
备用 boot 槽
已备份的 boot.img / vendor_boot / dtbo
```

---

## 12. 不建议优先做的事情

当前阶段不建议优先投入：

```text
完整 GNOME / KDE 桌面
Phosh / Plasma Mobile 完整手机桌面
摄像头
基带 / 移动数据
GPS
指纹
复杂音频链路
传感器全集
```

这些属于后期长尾硬件适配，难度大，投入产出比低。

---

## 13. 最终判断

当前最优路线是：

```text
先把 K40 做成稳定的 headless Linux 节点
再加一个极简状态屏
最后再决定是否继续挑战完整桌面
```

推荐最终形态：

```text
Redmi K40 原生 Linux 随身服务器
- ARM64 rootfs
- Wi-Fi 自动联网
- SSH / Tailscale / FRP 远程控制
- 本机屏幕显示状态
- 可选触摸按钮
- 可复现 boot/rootfs 构建流程
```

这条路线相比“强行移植桌面 Linux 手机”更容易成功，也更像一个完整、可交付、可沉淀的嵌入式 Linux 项目。
