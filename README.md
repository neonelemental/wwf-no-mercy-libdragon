# WWF No Mercy on libdragon

WWF No Mercy for the Nintendo 64, built from its disassembly and moved from
libultra to [libdragon](https://github.com/n64brew/libdragon).

The build splits the main code and data of the game into assembly and links
it back into the retail image. The game files stay binary for now.

## Status

In the source variant, libdragon is loaded into the Expansion Pak and starts
first. The game's threads, message queues and interrupts run on libdragon's
kernel, and its video, cartridge reads, controllers, RSP tasks and audio
output go through our code on libdragon. The flash save, the Controller Pak
and rumble still run on libultra, on top of our calls. The source variant
needs the Expansion Pak: with no pak, it stops before the game starts.

The source variant plays a match in the harness's emulator and boots in ares
147. No console has run it yet.

Routines move from assembly to C one at a time, each checked call by call
against its assembly where the check can run it: the game's own in
`src/game/`, and libultra's maths and `bcopy` in `src/ultra/`. So far these
are the matrix and vector maths that a match calls most, the passes over the
nodes and objects of the scene, the model of a wrestler from its animation
data, the ring with its ropes, the passes over each wrestler's slot, the tick
of a match, and the draw side: the draw order, the display lists and the loop
of the graphics thread.

| part | on libdragon |
|---|---|
| threads, messages, interrupts | yes |
| video | yes |
| cartridge reads | yes |
| controllers | yes |
| RSP tasks | yes |
| audio out | yes |
| flash saves | no |
| Controller Pak and rumble | no |
| audio library, other C helpers | decide when we get there |

## Building

You need git, Docker and your own copy of WWF No Mercy (USA) (Rev 1):

    sha1 91cee3d096f4a76644d8b35b9aead6448909abd1

The build runs in a Docker image that holds libdragon's MIPS toolchain and
[splat](https://github.com/ethteck/splat). libdragon itself comes from the
`libdragon` submodule and is built by `make`, at its own flags, with the
patches of `patches/libdragon/`.

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
| `make profile` | plays 25 seconds of a match on `build/source/nomercy.z64` and writes what each routine costs in a game frame to `build/source/profile/table.txt`. `make profile VARIANT=match` profiles the retail image, and `SCRIPT=tag` a tag match |
| `make source CHECK=NAME` | builds `build/check/`, which runs the assembly of the lifted routine NAME beside our C; `make play CHECK=NAME` plays it |
| `make clean` | removes everything the build wrote |

`symbol_addrs.txt` names each routine of the main code, and `extra_addrs.txt`
names each other address that a word points to, and each variable that the
harness reads. `reloc_addrs.txt` gives each
word that holds an address its symbol, and the word moves with the code. It
gives `MIPS_NONE` to each word that is a number.

The source variant boots from `src/boot/boot.S`, which loads libdragon,
placed at rom 0x2000000, into the Expansion Pak. `src/boot/start.S`
there does the work of libdragon's IPL3 and goes on to libdragon's `_start`,
and `src/boot/main.c` then starts libdragon's kernel and calls the game's
entry.

Each `replace/PART.txt` lists labels of the split that the source variant
leaves out, and `src/dragon/PART/` holds our code in their place.
`replace/kernel.txt` takes out libultra's kernel: the exception handler, the
thread dispatch, the boot call, and the thread, queue, event, timer and
interrupt mask calls. `src/dragon/kernel/` gives each call on libdragon's
kernel and posts libultra's events from libdragon's interrupts. In the same
way, `ai`, `pi`, `si`, `sp` and `vi` take out libultra's audio output, PI
manager, controller calls, RSP task calls and VI manager. libultra's flash,
rumble and Controller Pak routines stay, and call ours.
The entry is longer than the retail one and main text is shorter, so nothing
after the entry sits at its retail address. `make play` passes on the variant
with no pad and with a pad of 0x40.

Each `lift/PART.txt` lists routines that `src/game/` or `src/ultra/` gives in C.
A line `LABEL NAME` takes LABEL out of the source variant and puts our NAME in
its place, as a replace list does. Each label inside that routine follows on
a line of its own. The words `int` or `float` after NAME say that the routine
gives a result, and `near` that our C is meant to differ from the assembly by
1 in a word, or in each byte of one. `waits` says that the routine waits, or
calls one that does.

`make source CHECK=NAME` builds `build/check/`, where the assembly of that
routine stays beside our C, and `make play CHECK=NAME` plays it. Each call
runs the assembly and then our C, with the same arguments and from the same
memory, and the harness's emulator compares what the two wrote to RDRAM and
what they gave. The run fails at the first call where they differ, with the
address, the two values and the arguments of that call. The game goes on with
what the assembly left. The emulator says when a routine first runs under the
check, and when 100, 10,000 and 1,000,000 of its calls were the same. `CHECK`
takes names with commas between them, or `all`.

A routine under the check must not wait, wake another thread or write to a
device, as the check cannot take those back. `CHECK=all` leaves out each
routine that its list marks `waits`, and `CHECK` refuses its name: the plays
alone test our C of it. A checked routine can take four arguments on the
stack, and the check does not compare the stack under its caller. A call
from inside a checked run goes to the assembly alone, so a routine that only
checked routines call needs a run with its own name. The check build runs
only in the harness's emulator.

libdragon and our C are at the o64 ABI, with 64-bit registers and the FR bit
of the status register set. The game's assembly keeps the o32 calls of the
retail game, so `tools/thunks.py` writes a thunk for each call between the
two, and `src/thunks/hand.S` holds the ones that pass a 64-bit value.
`src/dragon/kernel/calls.S` holds a shorter thunk for each of the three calls
that a frame makes most. The retail code also writes some doubles by their
halves, which works only with the FR bit clear: `tools/cut.py` makes each one
a single write of the whole double, at the places it finds and those of
`double_addrs.txt`.

The harness runs a ROM in the image's emulator, as `make` runs there. It
presses the buttons of a script, saves the pictures that the script asks for,
and reads the game's memory by the symbols of the ELF beside the ROM:

    build/harness/release/harness build/source/nomercy.z64 tests/tools/scripts/match.txt build/play

Of `tests/tools/scripts/`, `menu.txt` boots to the main menu, `match.txt`
goes on into a single match, and `long.txt` plays 25 seconds of that match
and profiles them. `tag.txt` does the same in a tag match, with four
wrestlers. `cage.txt` plays a cage match and climbs each wall, which runs
code of the ring that no other script reaches. `tests/tools/src/main.rs`
lists the commands of a script. A run fails when libdragon stops the game for a fault or a failed
assert, when the game reads or writes outside RDRAM through the cache, which
stops the CPU of a console, when the game shows no new picture for 10 seconds
or plays no sound for 30, when a value that the script expects is another,
when our C of a lifted routine differs from its assembly in a check build,
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

`make profile` plays `long.txt`, or `tag.txt` with `SCRIPT=tag`, and writes
the table to the same place. From the `profile` line of a script to its end,
the emulator counts each instruction that the CPU runs, each cache miss and
each jump, and `tools/profile.py` writes the table from the counts. The
table has a row for each routine that ran, the slowest first: its calls, its
time, the lines of code and of data that the caches lacked, and the dirty
lines of data written back, alone and with what it calls. Each number is for
one game frame, which is one new picture: two frames of the harness in a
match. The first line gives the sum of a game frame.

The time is a model of the VR4300 at 93.75 MHz. An instruction takes its
cycles in the pipeline: 1 for most, 5 for a float multiply, 29 for a float
divide or root. The caches are those of the console: 16 KB of instructions
in lines of 32 bytes and 8 KB of data in lines of 16 bytes, each direct
mapped. A line of instructions that the cache lacks costs 48 cycles, and a
line of data, or a dirty line written back, costs 40. For the retail game in
a single match the model gives 24.7 ms of CPU a game frame, where a console
measured 27.9 ms. In a tag match it gives 31.6 ms.

The model leaves out:

- the cycle that a load costs when the next instruction uses its value, and
  the cycles that an exception costs;
- the RSP and the RDP, and the time that the CPU waits while they hold the
  bus;
- the time of a read or a write of a device register, which counts as one
  cycle;
- the threads: a routine has one row, whichever threads run it;
- the timing of a console. The emulator gives each instruction the same
  time, so an interrupt does not come where the time of the model puts it.
  The loop of the game's idle thread runs for what the emulator leaves of a
  frame, so the sum leaves it out and names it on the second line.

A routine's cost with what it calls is a share: the cost of a routine goes
to its callers by their calls of it, which is right when each call costs the
same.

A run is one match, and no two builds play the same one. The emulator's clock
counts instructions, so a build with other code takes each interrupt at
another place, and the match goes another way: other moves, other sounds,
other reads of the cartridge. For that alone two runs differ by about 0.3 ms
a game frame. To compare two builds, take a routine's cost for each call, or
the mean of runs on scripts that wait a few frames more before the match.

`tests/hello` is a ROM that prints `hello from libdragon`. `tests/boot` starts
libdragon from the IPL3 of `baserom.z64`; it runs nine threads and prints
what they count. `tests/kernel` runs libultra's
thread, queue and event calls on libdragon's kernel and prints its counts.
`tests/mixer` plays the samples of `baserom.z64` on libdragon's mixer with up
to 24 channels. Run each on a console with a flashcart, or in an accurate
emulator such as [ares](https://ares-emu.net).

This repository contains no game data. Everything the split writes stays in
`build/`, which git ignores.
