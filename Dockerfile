# Build environment: libdragon's MIPS toolchain and splat.
# libdragon itself is built from the submodule by `make libdragon`.
FROM ubuntu:24.04@sha256:fe81560990fae86dc4743202b2cb4d3df98d0bcf949173cecef15ce1127ef852

ARG DEBIAN_FRONTEND=noninteractive

# The packages of libdragon's own Dockerfile, plus what this build needs.
RUN apt-get update \
    && apt-get install -y --no-install-recommends \
        build-essential \
        bzip2 \
        ca-certificates \
        curl \
        file \
        g++ \
        gcc \
        git \
        libmpc-dev \
        libmpfr-dev \
        make \
        patch \
        python3 \
        python3-venv \
        texinfo \
        wget \
        xxd \
        zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# libdragon's prebuilt toolchain (GCC, binutils, newlib) into /opt/libdragon.
# The release is in the archived DragonMinded repository, so its files no
# longer change and their checksums hold.
ARG TOOLCHAIN_URL=https://github.com/DragonMinded/libdragon/releases/download/toolchain-continuous-prerelease
RUN arch="$(dpkg --print-architecture)" \
    && case "$arch" in \
        arm64) sha256=271a6f10c891b316a4e47959698cc7d9a8da1261ca3aa13edea0c82de22bc4b4 ;; \
        amd64) sha256=fac8e6572493a66468b7d45df41b042f90b1a630ec96aa218bd5fb1f320a0a4d ;; \
        *) echo "no toolchain package for $arch" >&2; exit 1 ;; \
    esac \
    && curl -fsSL -o /tmp/toolchain.deb "$TOOLCHAIN_URL/gcc-toolchain-mips64-$arch.deb" \
    && echo "$sha256  /tmp/toolchain.deb" | sha256sum -c - \
    && dpkg -i /tmp/toolchain.deb \
    && rm /tmp/toolchain.deb \
    && test -x /opt/libdragon/bin/mips64-elf-gcc
ENV N64_GCCPREFIX=/opt/libdragon

COPY requirements.txt /tmp/requirements.txt
RUN python3 -m venv /opt/splat \
    && /opt/splat/bin/pip install --no-cache-dir --prefer-binary -r /tmp/requirements.txt \
    && rm /tmp/requirements.txt
ENV PATH=/opt/splat/bin:/opt/libdragon/bin:$PATH

WORKDIR /src
