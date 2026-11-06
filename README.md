# WWF No Mercy on libdragon

WWF No Mercy for the Nintendo 64, built from its disassembly and moved from
libultra to [libdragon](https://github.com/n64brew/libdragon).

The build splits the main code and data of the game into assembly and links
it back into the retail image. The game files stay binary for now.

## Building

You need git, Docker and your own copy of WWF No Mercy (USA) (Rev 1):

    sha1 91cee3d096f4a76644d8b35b9aead6448909abd1

The build runs in a Docker image that holds libdragon's MIPS toolchain and
[splat](https://github.com/ethteck/splat). libdragon itself comes from the
`libdragon` submodule and is built by `make`.

    git clone --recurse-submodules https://github.com/neonelemental/wwf-no-mercy-libdragon
    cd wwf-no-mercy-libdragon
    cp /path/to/your/dump.z64 baserom.z64
    docker build -t wwf-no-mercy-libdragon .
    docker run --rm -v "$PWD:/src" -w /src --user "$(id -u):$(id -g)" -e HOME=/tmp wwf-no-mercy-libdragon make check

`make check` splits `baserom.z64` into `build/split/`, assembles and links
it into `build/match/nomercy.z64`, and checks that its SHA1 is the retail
one. splat refuses a dump with another SHA1. The dump must be big-endian
(`.z64`).

| target | does |
|---|---|
| `make` | builds the source variant, `build/source/nomercy.z64`, linked by symbols |
| `make source PAD=0x40` | builds `build/pad40/` with 0x40 bytes of pad in each section, so that the code moves |
| `make extract` | splits `baserom.z64` into `build/split/` |
| `make match` | builds `build/match/nomercy.z64` and `nomercy.elf` |
| `make check` | `make match`, then checks the SHA1 of the image |
| `make tests` | builds libdragon into `build/libdragon/` and each test ROM into `tests/NAME/build/NAME.z64` |
| `make clean` | removes everything the build wrote |

`symbol_addrs.txt` names each routine of the main code, and `extra_addrs.txt`
names each other address that a word points to. `reloc_addrs.txt` gives each
word that holds an address its symbol, and the word moves with the code. It
gives `MIPS_NONE` to each word that is a number. With no pad, the source
variant is the retail image.

`tests/hello` is a ROM that prints `hello from libdragon`. `tests/boot` starts
libdragon from the IPL3 of `baserom.z64`, with libdragon built at o32; it runs
nine threads and prints what they count. `tests/kernel` runs libultra's
thread, queue and event calls on libdragon's kernel and prints its counts.
`tests/mixer` plays the samples of `baserom.z64` on libdragon's mixer with up
to 24 channels. Run each on a console with a flashcart, or in an accurate
emulator such as [ares](https://ares-emu.net).

This repository contains no game data. Everything the split writes stays in
`build/`, which git ignores.
