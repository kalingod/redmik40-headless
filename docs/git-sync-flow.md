# Git sync flow

The project now uses two GitHub repositories under `kalingod`.

## Control repository

Repository:

```text
git@github-kalingod:kalingod/redmik40-headless.git
```

Purpose:

- documentation
- runbooks
- builder panel
- Docker builder definition
- build and deployment scripts

Large runtime artifacts are intentionally ignored by `.gitignore`.

## Kernel fork

Repository:

```text
git@github-kalingod:kalingod/android_kernel_xiaomi_sm8250.git
```

Upstream:

```text
https://github.com/xiaomi-sm8250-devs/android_kernel_xiaomi_sm8250
```

Development branch:

```text
lineage-20-headless
```

## Expected workflow

```text
local kernel edit
git commit
git push origin lineage-20-headless
builder panel Sync
builder panel Build
download artifacts from browser
```

The builder machine tracks:

```text
origin   https://github.com/kalingod/android_kernel_xiaomi_sm8250.git
upstream https://github.com/xiaomi-sm8250-devs/android_kernel_xiaomi_sm8250
branch   lineage-20-headless
```

The panel default branch is `lineage-20-headless`.
