FROM debian:bookworm

ARG APT_MIRROR=http://deb.debian.org/debian
ARG APT_SECURITY_MIRROR=http://deb.debian.org/debian-security

ENV DEBIAN_FRONTEND=noninteractive \
    ARCH=arm64 \
    KBUILD_BUILD_USER=lele \
    KBUILD_BUILD_HOST=alioth-docker \
    CCACHE_DIR=/cache/ccache \
    PATH=/usr/lib/ccache:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin

RUN set -eux; \
  for f in /etc/apt/sources.list /etc/apt/sources.list.d/*.sources; do \
    [ -f "$f" ] || continue; \
    sed -i \
      -e "s|http://deb.debian.org/debian-security|${APT_SECURITY_MIRROR}|g" \
      -e "s|http://deb.debian.org/debian|${APT_MIRROR}|g" \
      "$f"; \
  done; \
  apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    git \
    make \
    bc \
    bison \
    flex \
    libssl-dev \
    libelf-dev \
    python3 \
    python-is-python3 \
    perl \
    rsync \
    cpio \
    kmod \
    file \
    xz-utils \
    zstd \
    unzip \
    wget \
    curl \
    device-tree-compiler \
    ccache \
    clang \
    lld \
    llvm \
    gcc-aarch64-linux-gnu \
    gcc-arm-linux-gnueabi \
    gcc-arm-linux-gnueabihf \
    libc6-dev-arm64-cross \
    libc6-dev-armel-cross \
    libc6-dev-armhf-cross \
  && rm -rf /var/lib/apt/lists/*

RUN mkdir -p /src /out /cache/ccache /work
WORKDIR /src

CMD ["bash"]
