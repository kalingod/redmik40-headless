# Local Kernel Build with Docker

Local Docker builds are the default preparation path. A remote 36-core machine is only an acceleration fallback.

## Directory layout

```text
/Users/wuyuele/redmik40-headless          # control repo
/Users/wuyuele/android-kernel/alioth      # kernel source
/Users/wuyuele/android-kernel/out         # build output
/Users/wuyuele/android-kernel/cache       # ccache and download cache
```

Container mounts:

```text
/src     -> kernel source
/out     -> build output
/cache   -> ccache/cache
/project -> this repository, read-only
```

Edit source on macOS, compile in Docker.

## Build builder image

```bash
docker build -t redmik40-kernel-builder:bookworm docker/kernel-builder
```

## Clone kernel-only source

```bash
mkdir -p /Users/wuyuele/android-kernel
git clone --filter=blob:none --branch lineage-20 \
  https://github.com/xiaomi-sm8250-devs/android_kernel_xiaomi_sm8250 \
  /Users/wuyuele/android-kernel/alioth
```

Do not clone full LineageOS unless a later task needs full Android build/vendor/sepolicy context.

## Interactive shell

```bash
scripts/docker_kernel_shell.sh
```

Override paths:

```bash
KERNEL_SRC=/path/to/kernel \
KERNEL_OUT=/path/to/out \
KERNEL_CACHE=/path/to/cache \
scripts/docker_kernel_shell.sh
```

## Build

```bash
scripts/docker_kernel_build.sh
```

Common override:

```bash
DEFCONFIG=vendor/alioth_defconfig \
KERNEL_OUT=/Users/wuyuele/android-kernel/out/pstore-test \
JOBS=8 \
scripts/docker_kernel_build.sh
```

Expected outputs:

```text
/Users/wuyuele/android-kernel/out/pstore-test/arch/arm64/boot/Image
/Users/wuyuele/android-kernel/out/pstore-test/arch/arm64/boot/dts/**/*.dtb
/Users/wuyuele/android-kernel/out/pstore-test/.config
```

## Notes

- Default script uses Debian clang/LLVM with `LLVM=1 LLVM_IAS=1`.
- If the Lineage kernel requires a specific Android clang, add it later without changing the source layout.
- Keep separate `KERNEL_OUT` directories per experiment.
- Do not write build output into the kernel source tree.
