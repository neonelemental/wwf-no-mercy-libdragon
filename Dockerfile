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

# The emulator that tests/tools drives: mupen64plus with the fixes of
# tests/tools/patches, the cxd4 RSP and the angrylion RDP. The RDP is built
# without its OpenGL screen, which the harness supplies.
RUN apt-get update \
    && apt-get install -y --no-install-recommends cargo libpng-dev libsdl2-dev pkg-config \
    && rm -rf /var/lib/apt/lists/*

ARG M64P_CORE=cf00a1d1b4138a8e04cde47aabcd00ea9a047f57
ARG M64P_RSP=00906a92641c540a64e4e7505012157315314810
ARG ANGRYLION=9c8b9ed3e7d7f00dff8bc872ccdd3fba1a3673fc
COPY tests/tools/patches /tmp/patches
RUN cd /tmp \
    && curl -fsSL "https://github.com/mupen64plus/mupen64plus-core/archive/$M64P_CORE.tar.gz" | tar xz \
    && for p in /tmp/patches/*.patch; do patch -p1 -s -d "mupen64plus-core-$M64P_CORE" -i "$p" || exit 1; done \
    && make -C "mupen64plus-core-$M64P_CORE/projects/unix" -j"$(nproc)" all install NO_ASM=1 OSD=0 VULKAN=0 \
    && curl -fsSL "https://github.com/mupen64plus/mupen64plus-rsp-cxd4/archive/$M64P_RSP.tar.gz" | tar xz \
    && make -C "mupen64plus-rsp-cxd4-$M64P_RSP/projects/unix" -j"$(nproc)" all install \
        APIDIR=/usr/local/include/mupen64plus POSTFIX= \
    && curl -fsSL "https://github.com/ata4/angrylion-rdp-plus/archive/$ANGRYLION.tar.gz" | tar xz \
    && cd "angrylion-rdp-plus-$ANGRYLION" \
    && python3 make_version.py \
    && gcc -O3 -fPIC -Isrc -c src/core/n64video.c src/plugin/mupen64plus/gfx_m64p.c src/plugin/mupen64plus/msg.c \
    && g++ -O3 -fPIC -Isrc -c src/core/parallel.cpp \
    && g++ -shared -o /usr/local/lib/mupen64plus/mupen64plus-video-angrylion-plus.so *.o -lpthread \
    && cd /tmp \
    && rm -rf patches mupen64plus-* angrylion-* \
    && ln -s libmupen64plus.so.2 /usr/local/lib/libmupen64plus.so \
    && ldconfig

WORKDIR /src
