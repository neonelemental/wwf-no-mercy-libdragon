# WWF No Mercy on libdragon

WWF No Mercy for the Nintendo 64, built from its disassembly and moved from
libultra to [libdragon](https://github.com/n64brew/libdragon).

The project is at an early stage: it builds libdragon and a test ROM, but not
the game yet.

## Building

You need git and Docker. The build runs in a Docker image that holds
libdragon's MIPS toolchain and [splat](https://github.com/ethteck/splat).
libdragon itself comes from the `libdragon` submodule and is built by `make`.

    git clone --recurse-submodules https://github.com/neonelemental/wwf-no-mercy-libdragon
    cd wwf-no-mercy-libdragon
    docker build -t wwf-no-mercy-libdragon .
    docker run --rm -v "$PWD:/src" -w /src --user "$(id -u):$(id -g)" -e HOME=/tmp wwf-no-mercy-libdragon make tests

This builds libdragon into `build/libdragon/` and each test ROM into
`tests/NAME/build/NAME.z64`. `tests/hello` is a ROM that prints
`hello from libdragon`. Run it on a console with a flashcart, or in an
accurate emulator such as [ares](https://ares-emu.net).

`make clean` removes everything the build wrote.

## Base ROM

The build needs your own copy of WWF No Mercy (USA) (Rev 1):

    sha1 91cee3d096f4a76644d8b35b9aead6448909abd1

This repository contains no game data.
