#!/usr/bin/env python3
import html
import os
import subprocess
import urllib.parse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer


CONFIG_PATH = os.environ.get("LELE_WIFI_CONFIG", "/etc/alioth-wifi-default")
AP_LOG = os.environ.get("LELE_AP_LOG", "/run/lele-ap/portal.log")


def read_known_ssids():
    try:
        with open(CONFIG_PATH, "r", encoding="utf-8") as f:
            lines = [line.rstrip("\n") for line in f]
    except FileNotFoundError:
        return []
    ssids = []
    for idx in range(0, len(lines), 2):
        ssid = lines[idx].strip()
        if ssid:
            ssids.append(ssid)
    return ssids


def write_config(ssid, psk):
    os.makedirs(os.path.dirname(CONFIG_PATH), exist_ok=True)
    tmp = CONFIG_PATH + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        f.write(ssid + "\n")
        f.write(psk + "\n")
    os.chmod(tmp, 0o600)
    os.replace(tmp, CONFIG_PATH)


def schedule_apply():
    cmd = (
        "sleep 2; "
        "/usr/local/sbin/lele-ap-stop >/run/lele-ap/ap-stop.log 2>&1 || true; "
        "if /usr/local/sbin/alioth-wifi-connect >/run/lele-ap/wifi-connect.log 2>&1; then exit 0; fi; "
        "/usr/local/sbin/lele-ap-start >/run/lele-ap/ap-recovery.log 2>&1 || true"
    )
    subprocess.Popen(
        ["setsid", "sh", "-c", cmd],
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        close_fds=True,
    )


def page(message=""):
    known = read_known_ssids()
    known_html = ""
    if known:
        known_html = "<p class='muted'>已保存: " + ", ".join(html.escape(s) for s in known) + "</p>"
    msg_html = f"<p class='ok'>{html.escape(message)}</p>" if message else ""
    return f"""<!doctype html>
<html lang="zh-CN">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>LELE OS Wi-Fi</title>
  <style>
    :root {{ color-scheme: light dark; }}
    body {{ margin: 0; font-family: system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif; background: #11181f; color: #eef3f8; }}
    main {{ max-width: 520px; margin: 0 auto; padding: 40px 20px; }}
    h1 {{ margin: 0 0 8px; font-size: 30px; letter-spacing: 0; }}
    p {{ line-height: 1.5; color: #b9c5cf; }}
    label {{ display: block; margin: 18px 0 8px; font-weight: 650; }}
    input {{ box-sizing: border-box; width: 100%; height: 46px; border: 1px solid #41505d; border-radius: 6px; padding: 0 12px; background: #17212a; color: #eef3f8; font-size: 16px; }}
    .actions {{ display: flex; gap: 10px; margin-top: 22px; flex-wrap: wrap; }}
    button {{ border: 0; border-radius: 6px; min-height: 44px; padding: 0 16px; font-size: 15px; font-weight: 700; }}
    .primary {{ background: #62c7ff; color: #06121b; }}
    .secondary {{ background: #25313c; color: #e5edf4; }}
    .ok {{ color: #84e1a6; }}
    .muted {{ color: #92a1ad; font-size: 14px; }}
  </style>
</head>
<body>
  <main>
    <h1>LELE OS Wi-Fi</h1>
    <p>配置外部 Wi-Fi。保存后可立即切回客户端模式；如果连接失败，设备会尝试恢复这个配置热点。</p>
    {msg_html}
    {known_html}
    <form method="post" action="/save">
      <label for="ssid">SSID</label>
      <input id="ssid" name="ssid" autocomplete="off" required>
      <label for="psk">密码</label>
      <input id="psk" name="psk" type="password" autocomplete="current-password">
      <div class="actions">
        <button class="primary" type="submit" name="apply" value="1">保存并连接</button>
        <button class="secondary" type="submit" name="apply" value="0">仅保存</button>
      </div>
    </form>
  </main>
</body>
</html>"""


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        os.makedirs(os.path.dirname(AP_LOG), exist_ok=True)
        with open(AP_LOG, "a", encoding="utf-8") as f:
            f.write("%s - %s\n" % (self.address_string(), fmt % args))

    def send_html(self, body):
        data = body.encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        self.send_html(page())

    def do_POST(self):
        length = int(self.headers.get("Content-Length", "0") or "0")
        raw = self.rfile.read(length).decode("utf-8", errors="replace")
        form = urllib.parse.parse_qs(raw, keep_blank_values=True)
        ssid = (form.get("ssid") or [""])[0].strip()
        psk = (form.get("psk") or [""])[0]
        apply = (form.get("apply") or ["0"])[0] == "1"
        if not ssid:
            self.send_html(page("SSID 不能为空"))
            return
        if psk and len(psk) < 8:
            self.send_html(page("WPA 密码至少 8 位"))
            return
        write_config(ssid, psk)
        if apply:
            schedule_apply()
            self.send_html(page("已保存，正在切回 Wi-Fi 客户端模式"))
        else:
            self.send_html(page("已保存"))


def main():
    host = os.environ.get("LELE_PORTAL_HOST", "10.42.0.1")
    port = int(os.environ.get("LELE_PORTAL_PORT", "80"))
    server = ThreadingHTTPServer((host, port), Handler)
    server.serve_forever()


if __name__ == "__main__":
    main()
