# Session Handoff 2026-05-14

## Current device state

- Device: Redmi K40 / POCO F3 `alioth`, Ubuntu 24.04.4 LTS arm64, kernel `4.19.312-perf`.
- USB SSH address: `root@172.16.42.2`.
- USB host address: `172.16.42.1`.
- Office Mac SSH public key added to `/root/.ssh/authorized_keys`:
  - `4096 SHA256:nStgLlvpWFHn7V2zwwjQtEzP5PlxBam8VkpXyTO9T1w wulele@didiglobal.com (RSA)`
- Existing USB SSH key remains:
  - `256 SHA256:tBW1zVMn/nAJp2YkCgqip4KcgwS6hRIk/IbKGQgO3fA alioth-usb-mac (ED25519)`
- Root filesystem is `/dev/block/by-name/userdata`, mounted as `/`, about 189G total.
- `/data` is the older 32G `arch` partition. Put large projects under `/root`, `/opt`, or `/var/lib/*` to use userdata.

## USB login from office Mac

Connect the phone by USB, then from the office Mac:

```sh
ssh -i ~/.ssh/id_rsa -o StrictHostKeyChecking=no root@172.16.42.2
```

If the RSA key is rejected by algorithm policy, retry with:

```sh
ssh -i ~/.ssh/id_rsa \
  -o PubkeyAcceptedAlgorithms=+rsa-sha2-512,rsa-sha2-256,ssh-rsa \
  -o HostkeyAlgorithms=+ssh-rsa \
  root@172.16.42.2
```

Basic USB checks on the Mac:

```sh
ping 172.16.42.2
route -n get 172.16.42.2
ssh -vvv -i ~/.ssh/id_rsa root@172.16.42.2 true
```

## Panel and services

Current panel service:

```sh
systemctl status lele-status-ui.service --no-pager -l
systemctl cat lele-status-ui.service
tail -80 /run/alioth-status-ui.log
```

Current GPU panel binary:

```text
/data/experiments/alioth-status-ui-gpu-ui-runtime-v2-wifi-ssid-fix
sha256 b5d895830c3c79415c9e24610cdd69c4543d3265648907b642fac961d8584aef
```

The panel service drop-in should keep:

```ini
KillMode=process
Environment=LD_LIBRARY_PATH=/data/experiments/gpu-runtime-20260513/lib:/data/experiments/mesa-kgsl-prefix/lib/aarch64-linux-gnu
Environment=VK_ICD_FILENAMES=/data/experiments/gpu-runtime-20260513/freedreno_icd-kgsl-mesa2034-aarch64.json
Environment=TU_DEBUG=startup
```

`KillMode=process` is intentional. Without it, restarting the panel can kill child-launched Wi-Fi helper processes.

## Wi-Fi notes

The Wi-Fi connection path has been simplified:

- `wpa_supplicant` handles association.
- `systemd-networkd` owns DHCP and routes.
- `dhcpcd` is no longer left running, because it was racing `systemd-networkd` and creating duplicate IPv4 addresses/routes.

Useful checks:

```sh
cat /run/alioth-wifi-state
wpa_cli -i wlan0 status
ip -o -4 addr show dev wlan0
ip route show default
ps -eo pid,ppid,stat,etime,cmd | grep -E 'wpa_supplicant|dhcpcd|systemd-networkd' | grep -v grep
journalctl --since '10 min ago' --no-pager | grep -Ei 'wlan0|wpa|dhcp|cfg80211|wifi|deauth|disconnect'
```

Expected clean state:

```text
wpa_state=COMPLETED
one global IPv4 on wlan0, usually 172.23.169.110/23
no long-running dhcpcd process
default via 172.23.169.254 dev wlan0, USB default route with higher metric
```

## Tooling state

Node was moved from Ubuntu's Node 18 package to NodeSource Node 24:

```text
node v24.15.0
npm  11.12.1
npm prefix /usr/local
npm registry https://registry.npmmirror.com/
```

Global npm tools verified:

```text
claude 2.1.141
codex-cli 0.130.0
ark-helper 1.2.17
```

Claude Code runs as `root` on this phone. Do not set
`permissions.defaultMode=bypassPermissions` in `/root/.claude/settings.json`;
Claude rejects that mode for root/sudo sessions with:

```text
--dangerously-skip-permissions cannot be used with root/sudo privileges for security reasons
```

Current root-safe settings keep the Bash allow list but use:

```json
{
  "permissions": {
    "defaultMode": "default"
  },
  "language": "中文",
  "autoUpdatesChannel": "stable"
}
```

Mihomo is installed as a system service:

```sh
systemctl status mihomo --no-pager -l
curl -x http://127.0.0.1:7890 -I https://www.google.com
```

Ports:

```text
127.0.0.1:7890 mixed proxy
127.0.0.1:7891 HTTP proxy
127.0.0.1:7892 SOCKS proxy
0.0.0.0:9090 controller/dashboard
```

Dashboard from Mac over USB:

```text
http://172.16.42.2:9090/ui/
```

Do not paste `/root/.config/mihomo/config.yaml` into logs or AI prompts; it contains proxy credentials.

## Thermal guard

Thermal guard is installed and running:

```sh
systemctl status alioth-thermal-guard.service --no-pager -l
cat /run/alioth-thermal-guard.status
```

Current policy uses conservative thresholds:

```text
warm: battery 40C / CPU 60C / GPU 60C / PMIC 65C -> schedutil
hot: battery 42C / CPU 70C / GPU 70C / PMIC 80C -> powersave
shutdown: battery 55C / CPU 105C / GPU 105C / PMIC 115C -> poweroff
```

## Screenshot

Panel screenshot trigger:

```sh
printf wifi > /run/alioth-panel-page
touch /run/alioth-panel-screenshot.request
sleep 1
cat /run/alioth-panel-screenshot.txt
```

Screenshot file:

```text
/run/alioth-panel-screenshot.bmp
```

## Prompt to paste to AI from office Mac

Use this when USB SSH or network behavior is confusing. Paste command outputs after the prompt.

```text
I am debugging a Redmi K40 / POCO F3 alioth running Ubuntu 24.04 arm64 headless over USB-NCM.

Important context:
- Phone USB SSH address should be root@172.16.42.2.
- Mac USB host address should be 172.16.42.1.
- The phone also has Wi-Fi on 172.23.169.0/23, but Wi-Fi may be unstable.
- Do not change host routes automatically. First explain the current route/interface state.
- On the phone, Wi-Fi should be managed by wpa_supplicant + systemd-networkd. dhcpcd should not be left running.
- Panel service is lele-status-ui.service. It should use KillMode=process so restarting the panel does not kill Wi-Fi helpers.
- Mihomo may be running on the phone: 127.0.0.1:7890/7891/7892 and controller 0.0.0.0:9090.
- Do not ask me to paste secrets such as /root/.config/mihomo/config.yaml or subscription URLs.

Please diagnose from evidence first. Start with:
1. Is USB SSH routing correct on the Mac?
2. Is ssh failing at route, TCP, host key, auth key, or server level?
3. Is phone Wi-Fi state stale or live?
4. Are there duplicate DHCP managers or duplicate wlan0 IPv4 addresses?

Mac outputs:
[paste: ping 172.16.42.2]
[paste: route -n get 172.16.42.2]
[paste: ifconfig or networksetup output for USB interface]
[paste: ssh -vvv -i ~/.ssh/id_rsa root@172.16.42.2 true]

Phone outputs if SSH works:
[paste: cat /run/alioth-wifi-state]
[paste: wpa_cli -i wlan0 status]
[paste: ip -o -4 addr show dev wlan0]
[paste: ip route show default]
[paste: systemctl status lele-status-ui.service --no-pager -l]
[paste: journalctl --since '10 min ago' --no-pager | grep -Ei 'wlan0|wpa|dhcp|wifi|deauth|disconnect']
```

## Current repo branch

```text
exp4-ubuntu-systemd-wifi
```

Commit this handoff together with the current panel, Wi-Fi, thermal guard, and builder-panel changes before stopping the session.
