# Agent capabilities

## Remote kernel build

The builder machine is reachable through the FRP SSH endpoint:

```text
ssh -p 60022 lele@47.98.228.151
```

Kernel source on the builder:

```text
/home/lele/redmik40-build/lineage-sm8250/kernel
```

Kernel fork tracked by the builder:

```text
origin   https://github.com/kalingod/android_kernel_xiaomi_sm8250.git
upstream https://github.com/xiaomi-sm8250-devs/android_kernel_xiaomi_sm8250
branch   lineage-20-headless
```

## Remote artifact retrieval

After a successful remote build, the agent should retrieve build artifacts by
direct SSH/SCP copy, not by browser download.

Default remote artifact root:

```text
/home/lele/redmik40-build/lineage-sm8250/artifacts
```

Default local destination:

```text
artifacts/remote/
```

Manual copy pattern:

```bash
mkdir -p artifacts/remote/<artifact-name>
scp -P 60022 \
  -o StrictHostKeyChecking=no \
  -o UserKnownHostsFile=/tmp/redmik40_remote_known_hosts \
  lele@47.98.228.151:/home/lele/redmik40-build/lineage-sm8250/artifacts/<artifact-name>/* \
  artifacts/remote/<artifact-name>/
```

Helper script:

```bash
scripts/fetch_remote_kernel_artifacts.sh <artifact-name>
```

Example:

```bash
scripts/fetch_remote_kernel_artifacts.sh kernel-lineage20-alioth-j36
```

For panel-triggered builds, artifact names are typically:

```text
panel-<branch>-<task-id>
```

Example:

```bash
scripts/fetch_remote_kernel_artifacts.sh panel-lineage-20-headless-7
```
