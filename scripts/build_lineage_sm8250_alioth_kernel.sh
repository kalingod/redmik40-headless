#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

WORK_DIR="${WORK_DIR:-/vmdata/android/redmik40/lineage-sm8250}"
KERNEL_DIR="${KERNEL_DIR:-$WORK_DIR/kernel}"
OUT_DIR="${OUT_DIR:-$WORK_DIR/out/kernel-lineage20-alioth}"
ARTIFACT_DIR="${ARTIFACT_DIR:-$WORK_DIR/artifacts/kernel-lineage20-alioth}"
BUILDER_IMAGE="${BUILDER_IMAGE:-redmik40-kernel-builder:bullseye}"
JOBS="${JOBS:-36}"
SYSTEMD_FRIENDLY_CONFIG="${SYSTEMD_FRIENDLY_CONFIG:-yes}"
EXTRA_CONFIG_FRAGMENT="${EXTRA_CONFIG_FRAGMENT:-}"

die() {
  echo "error: $*" >&2
  exit 1
}

log() {
  printf '[%s] %s\n' "$(date '+%F %T')" "$*"
}

require_dir() {
  [ -d "$1" ] || die "missing directory: $1"
}

work_path_for_container() {
  case "$1" in
    "$WORK_DIR")
      printf '/work\n'
      ;;
    "$WORK_DIR"/*)
      printf '/work/%s\n' "${1#"$WORK_DIR"/}"
      ;;
    *)
      die "path must live under WORK_DIR ($WORK_DIR): $1"
      ;;
  esac
}

main() {
  require_dir "$KERNEL_DIR"
  command -v docker >/dev/null 2>&1 || die "missing command: docker"
  docker image inspect "$BUILDER_IMAGE" >/dev/null 2>&1 || die "missing Docker image: $BUILDER_IMAGE"

  mkdir -p "$ARTIFACT_DIR"
  extra_config_in_work=""
  if [ -n "$EXTRA_CONFIG_FRAGMENT" ]; then
    [ -f "$EXTRA_CONFIG_FRAGMENT" ] || die "missing EXTRA_CONFIG_FRAGMENT: $EXTRA_CONFIG_FRAGMENT"
    mkdir -p "$WORK_DIR/.kernel-extra-config"
    cp "$EXTRA_CONFIG_FRAGMENT" "$WORK_DIR/.kernel-extra-config/extra.config"
    extra_config_in_work="/work/.kernel-extra-config/extra.config"
  fi
  out_in_work="$(work_path_for_container "$OUT_DIR")"
  art_in_work="$(work_path_for_container "$ARTIFACT_DIR")"

  log "building Lineage 20 alioth kernel with LLVM"
  docker run --rm \
    -v "$WORK_DIR:/work" \
    -e OUT_IN_WORK="$out_in_work" \
    -e ART_IN_WORK="$art_in_work" \
    -e SYSTEMD_FRIENDLY_CONFIG="$SYSTEMD_FRIENDLY_CONFIG" \
    -e EXTRA_CONFIG_IN_WORK="$extra_config_in_work" \
    -w /work \
    "$BUILDER_IMAGE" \
    bash -lc "
set -euo pipefail
KERNEL=/work/kernel
OUT=\"\$OUT_IN_WORK\"
ART=\"\$ART_IN_WORK\"
rm -rf \"\$OUT\"
mkdir -p \"\$OUT\" \"\$ART\"

make -C \"\$KERNEL\" O=\"\$OUT\" ARCH=arm64 LLVM=1 CROSS_COMPILE=aarch64-linux-gnu- vendor/kona-perf_defconfig
fragments=(
  \"\$KERNEL\"/arch/arm64/configs/vendor/xiaomi/sm8250-common.config
  \"\$KERNEL\"/arch/arm64/configs/vendor/xiaomi/alioth.config
)
if [ \"\$SYSTEMD_FRIENDLY_CONFIG\" = yes ]; then
  cat > \"\$OUT\"/systemd-friendly.config <<'CFG'
CONFIG_FHANDLE=y
CONFIG_SYSVIPC=y
CONFIG_POSIX_MQUEUE=y
CONFIG_DEVTMPFS=y
CONFIG_DEVTMPFS_MOUNT=y
CONFIG_CGROUP_PIDS=y
CONFIG_CGROUP_DEVICE=y
CONFIG_CGROUP_PERF=y
CONFIG_CGROUP_HUGETLB=y
CONFIG_CFS_BANDWIDTH=y
CONFIG_PID_NS=y
CONFIG_IPC_NS=y
CONFIG_USER_NS=y
CONFIG_CHECKPOINT_RESTORE=y
CONFIG_FANOTIFY=y
CONFIG_AUTOFS4_FS=y
CONFIG_TMPFS_POSIX_ACL=y
CONFIG_TMPFS_XATTR=y
CONFIG_BRIDGE_NETFILTER=y
CONFIG_NETFILTER_XT_MATCH_ADDRTYPE=y
CONFIG_NETFILTER_XT_MATCH_CONNTRACK=y
CONFIG_NETFILTER_XT_MATCH_COMMENT=y
CONFIG_NETFILTER_XT_TARGET_REDIRECT=y
CONFIG_IP_NF_TARGET_MASQUERADE=y
CONFIG_IP_NF_NAT=y
CONFIG_NF_NAT_IPV4=y
CONFIG_NF_NAT_IPV6=y
CONFIG_IP_VS=y
CONFIG_IP_VS_NFCT=y
CONFIG_IP_VS_RR=y
CONFIG_IPVLAN=y
CONFIG_MACVLAN=y
CFG
  fragments+=(\"\$OUT\"/systemd-friendly.config)
fi
if [ -n \"\${EXTRA_CONFIG_IN_WORK:-}\" ]; then
  fragments+=(\"\$EXTRA_CONFIG_IN_WORK\")
fi
\"\$KERNEL\"/scripts/kconfig/merge_config.sh -m -O \"\$OUT\" \
  \"\$OUT\"/.config \
  \"\${fragments[@]}\"
make -C \"\$KERNEL\" O=\"\$OUT\" ARCH=arm64 LLVM=1 CROSS_COMPILE=aarch64-linux-gnu- olddefconfig
make -C \"\$KERNEL\" O=\"\$OUT\" ARCH=arm64 LLVM=1 CROSS_COMPILE=aarch64-linux-gnu- -j$JOBS Image dtbs

cp \"\$OUT\"/arch/arm64/boot/Image \"\$ART\"/Image
[ -f \"\$OUT\"/arch/arm64/boot/Image.gz ] && cp \"\$OUT\"/arch/arm64/boot/Image.gz \"\$ART\"/Image.gz
find \"\$OUT\"/arch/arm64/boot/dts/vendor/qcom -maxdepth 1 -type f \( -name 'alioth*.dtb' -o -name 'alioth*.dtbo' \) -print -exec cp {} \"\$ART\"/ \;
cp \"\$OUT\"/.config \"\$ART\"/config
sha256sum \"\$ART\"/* > \"\$ART\"/SHA256SUMS
"

  ls -lh "$ARTIFACT_DIR"
  log "done"
}

main "$@"
