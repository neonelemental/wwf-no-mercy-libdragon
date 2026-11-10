# WWF No Mercy on libdragon

WWF No Mercy for the Nintendo 64, built from its disassembly and moved from
libultra to [libdragon](https://github.com/n64brew/libdragon).

The build splits the main code and data of the game into assembly and links
it back into the retail image. The game files stay binary for now.

## Status

In the source variant, libdragon is loaded into the Expansion Pak and starts
first. The game's threads, message queues and interrupts run on libdragon's
kernel. The rest of the game still runs on libultra, and each part moves to
libdragon in turn. The source variant needs the Expansion Pak: with no pak, it
stops before the game starts.

The source variant plays a match in the harness's emulator and boots in ares
147. No console has run it yet.

| part | on libdragon |
|---|---|
| threads, messages, interrupts | yes |
| video | no |
| cartridge reads | no |
| controllers | no |
| RSP tasks | no |
| audio out | no |
| flash saves | no |
| Controller Pak and rumble | no |
| audio library, maths, C helpers | decide when we get there |

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
| `make source PAD=0x40` | builds `build/pad40/` with 0x40 bytes of pad in each section of main and in `ovl_a` and `ovl_e`, so that the code moves |
| `make extract` | splits `baserom.z64` into `build/split/` |
| `make match` | builds `build/match/nomercy.z64` and `nomercy.elf` |
| `make check` | `make match`, then checks the SHA1 of the image |
| `make tests` | builds libdragon into `build/libdragon/` and each test ROM into `tests/NAME/build/NAME.z64` |
| `make harness` | builds the test harness of `tests/tools` into `build/harness/release/harness` |
| `make play` | plays `build/source/nomercy.z64` in the harness: a boot, a match and a boot on the save of the match. `make play VARIANT=match` plays the retail image |
| `make clean` | removes everything the build wrote |

`symbol_addrs.txt` names each routine of the main code, and `extra_addrs.txt`
names each other address that a word points to, and each variable that the
harness reads. `reloc_addrs.txt` gives each
word that holds an address its symbol, and the word moves with the code. It
gives `MIPS_NONE` to each word that is a number.

The source variant boots from `src/boot/boot.S`, which loads libdragon, built
at o32 and placed at rom 0x2000000, into the Expansion Pak. `src/boot/start.S`
there does the work of libdragon's IPL3 and goes on to libdragon's `_start`,
and `src/boot/main.c` then starts libdragon's kernel and calls the game's
entry.

Each `replace/PART.txt` lists labels of the split that the source variant
leaves out, and `src/dragon/PART/` holds our code in their place.
`replace/kernel.txt` takes out libultra's kernel: the exception handler, the
thread dispatch, the boot call, and the thread, queue, event, timer and
interrupt mask calls. `src/dragon/kernel/` gives each call on libdragon's
kernel and posts libultra's events from libdragon's interrupts. libultra's
managers of the VI, the PI and the SI still run, as threads on those calls.
The entry is longer than the retail one and main text is shorter, so nothing
after the entry sits at its retail address. `make play` passes on the variant
with no pad and with a pad of 0x40.

The harness runs a ROM in the image's emulator, as `make` runs there. It
presses the buttons of a script, saves the pictures that the script asks for,
and reads the game's memory by the symbols of the ELF beside the ROM:

    build/harness/release/harness build/source/nomercy.z64 tests/tools/scripts/match.txt build/play

Of `tests/tools/scripts/`, `menu.txt` boots to the main menu and `match.txt`
goes on into a single match. `tests/tools/src/main.rs` lists the commands of a
script. A run fails when libdragon stops the game for a fault or a failed
assert, when the game reads or writes outside RDRAM through the cache, which
stops the CPU of a console, when the game shows no new picture for 10 seconds
or plays no sound for 30, when a value that the script expects is another,
and when the script ends without a picture and a sound in its last second. It then prints where
the CPU is and what its registers hold, and saves the last picture as
`failed.png`. When libdragon stopped the game, that picture is libdragon's
report of the fault. The retail image has no such report: a fault there shows
as a picture that stops.

Each run starts on a blank cartridge save and throws its save away. A fourth
argument names a directory that keeps the save from one run to the next.
`make play` uses it: the first boot formats the save, the match must write to
it, and the last boot must leave the save of the match as it is, which a boot
that could not read it does not.

`tests/hello` is a ROM that prints `hello from libdragon`. `tests/boot` starts
libdragon from the IPL3 of `baserom.z64`, with libdragon built at o32; it runs
nine threads and prints what they count. `tests/kernel` runs libultra's
thread, queue and event calls on libdragon's kernel and prints its counts.
`tests/mixer` plays the samples of `baserom.z64` on libdragon's mixer with up
to 24 channels. Run each on a console with a flashcart, or in an accurate
emulator such as [ares](https://ares-emu.net).

This repository contains no game data. Everything the split writes stays in
`build/`, which git ignores.
