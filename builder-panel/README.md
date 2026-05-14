# Redmi K40 Builder Panel

Minimal controlled build panel for the remote Redmi K40 kernel builder.

The app uses only Python standard-library modules. It exposes a fixed project
allowlist and runs only the expected git/build commands.

The UI is intentionally an operations console, not a general dashboard. It keeps
the first viewport focused on build control, active task state, repository
dirty state, load/disk signals, task logs, and downloadable artifacts.

## Artifact naming

Panel-triggered builds write a separate artifact directory per build task:

```text
panel-<branch-with-slashes-replaced>-<task-id>
```

Example:

```text
panel-lineage-20-headless-7
```

The directory name identifies the build record. The files inside that directory
keep stable names such as `Image`, `config`, and `SHA256SUMS`.

The panel also exposes a bundle URL per artifact directory:

```text
/download-bundle/panel-lineage-20-headless-7.tar.gz
```

Use the pipeline card download link when the build record list becomes long. A
pipeline is branch-scoped: `lineage-20` and `lineage-20-headless` are separate
pipelines with separate artifact directories and bundle URLs.

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

## Local preview

For a non-building local UI smoke test:

```bash
PANEL_HOST=127.0.0.1 \
PANEL_PORT=18081 \
PANEL_BASE=/tmp/redmik40-panel-dev \
PANEL_PROJECT_DIR=/Users/wuyuele/redmik40-headless \
PANEL_SOURCE_DIR=/tmp/redmik40-panel-dev/lineage-sm8250 \
PANEL_KERNEL_DIR=/Users/wuyuele/redmik40-headless \
PANEL_OUT_ROOT=/tmp/redmik40-panel-dev/out \
PANEL_ARTIFACT_ROOT=/tmp/redmik40-panel-dev/artifacts \
PANEL_LOG_DIR=/tmp/redmik40-panel-dev/logs \
PANEL_DB=/tmp/redmik40-panel-dev/panel.sqlite3 \
PANEL_BUILD_SCRIPT=/Users/wuyuele/redmik40-headless/scripts/build_lineage_sm8250_alioth_kernel.sh \
python3 app.py
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
