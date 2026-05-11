# Redmi K40 Builder Panel

Minimal controlled build panel for the remote Redmi K40 kernel builder.

The app uses only Python standard-library modules. It exposes a fixed project
allowlist and runs only the expected git/build commands.

## Default remote layout

- App: `/home/lele/redmik40-build/panel`
- Build base: `/home/lele/redmik40-build`
- Kernel repo: `/home/lele/redmik40-build/lineage-sm8250/kernel`
- Build script: `/home/lele/redmik40-build/project/scripts/build_lineage_sm8250_alioth_kernel.sh`
- Logs: `/home/lele/redmik40-build/logs`
- Artifacts: `/home/lele/redmik40-build/lineage-sm8250/artifacts`

## Start manually on the remote host

```bash
cd /home/lele/redmik40-build/panel
PANEL_HOST=127.0.0.1 PANEL_PORT=18080 PANEL_BASE=/home/lele/redmik40-build \
  nohup python3 app.py >/home/lele/redmik40-build/logs/panel.log 2>&1 &
echo $! >/home/lele/redmik40-build/logs/panel.pid
```

## Access without opening an Aliyun public port

```bash
ssh -p 60022 -L 18080:127.0.0.1:18080 lele@47.98.228.151
```

Then open:

```text
http://127.0.0.1:18080
```

## Public access notes

If binding to `0.0.0.0`, set basic auth first:

```bash
export PANEL_BASIC_USER=builder
export PANEL_BASIC_PASSWORD_FILE=/home/lele/redmik40-build/panel/.panel-password
export PANEL_HOST=0.0.0.0
```

Then open only the selected TCP port in the Aliyun security group, preferably
with source restricted to your current public IP.
