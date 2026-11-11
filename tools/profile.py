"""Writes the table of the profile that the harness kept of a run.

    python3 tools/profile.py symbol_addrs.txt ELF PROFILE [IDLE]

PROFILE is what the `profile` line of a script leaves: each instruction that
ran, with its word, the times it ran and its misses on the emulator's model
of the caches, and each jump by its two ends. ELF is the one beside the ROM.

The table has a row for each routine that ran, the slowest alone first. Each
number is for one game frame, which is one new picture: two frames of the
harness in a match. A row gives the calls of the routine, and then its time
in ms, the lines of code and of data that the caches lacked, and the dirty
lines of data written back: first alone, then with what it calls.

A routine is a name of symbol_addrs.txt with a size, a function of our C or
of libdragon, or a thunk. Where overlays share addresses, it is one of the
overlay whose words ran. A call is a jump that links, or any jump to the
first instruction of another routine. What a routine calls is counted by
gprof's rule: the cost of a routine goes to its callers by their share of
its calls, which is right when each call costs the same.

The time is that of a VR4300 at 93.75 MHz: the cycles of each instruction
(`cycles`), 48 for a line of code that the cache lacks, and 40 for a line of
data and for a line written back.

IDLE names the routine that holds the loop of the game's idle thread. Its
time, with what it calls, is no work of a frame, and the total leaves it out.
"""

import bisect
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

HZ = 93_750_000
CODE_MISS, DATA_MISS, WRITE_BACK = 48, 40, 40
SINGLE, DOUBLE, WORD, LONG = 16, 17, 20, 21


def cycles(word):
    """The cycles of an instruction in the pipeline of the VR4300."""
    op, fmt, fn = word >> 26, (word >> 21) & 31, word & 63
    if op == 0:  # mult, div, dmult, ddiv, and each without a sign
        return {0x18: 5, 0x19: 5, 0x1A: 37, 0x1B: 37, 0x1C: 8, 0x1D: 8, 0x1E: 69, 0x1F: 69}.get(fn, 1)
    if op != 0x11 or fmt not in (SINGLE, DOUBLE, WORD, LONG):
        return 1
    if fn in (0, 1):  # add, sub
        return 3
    if fn == 2:  # mul
        return 8 if fmt == DOUBLE else 5
    if fn in (3, 4):  # div, sqrt
        return 58 if fmt == DOUBLE else 29
    if fn == 32:  # to a single
        return 2 if fmt == DOUBLE else 5
    if fn == 33:  # to a double
        return 1 if fmt == SINGLE else 5
    if 8 <= fn <= 15 or fn in (36, 37):  # to an integer
        return 5
    return 1


def read_elf(data):
    """Gives the code sections and their symbols of a 32-bit big-endian ELF."""
    at, = struct.unpack_from(">I", data, 0x20)
    size, count, names = struct.unpack_from(">3H", data, 0x2E)
    heads = [struct.unpack_from(">10I", data, at + i * size) for i in range(count)]

    def text(table, at):
        start = heads[table][4] + at
        return data[start:data.index(b"\0", start)].decode()

    code = {i: (text(names, h[0]), h[3], h[5], h[4]) for i, h in enumerate(heads) if h[2] & 4}
    symbols = []
    for head in heads:
        if head[1] == 2:
            for at in range(head[4], head[4] + head[5], 16):
                name, value, size, info, _, section = struct.unpack_from(">3I2BH", data, at)
                if section in code:
                    symbols.append((text(head[6], name), value, size, info & 15, info >> 4, section))
    return code, symbols


def with_calls(alone, calls):
    """Gives each routine's costs with those of what it calls."""
    into, out = defaultdict(int), defaultdict(list)
    for (caller, callee), count in calls.items():
        into[callee] += count
        out[caller].append((callee, count))
    done, path = {}, set()

    def cost(routine):
        if routine in done:
            return done[routine]
        if routine in path:
            return [0] * 4
        path.add(routine)
        total = list(alone[routine])
        for callee, count in out[routine]:
            total = [a + b * count / into[callee] for a, b in zip(total, cost(callee))]
        path.discard(routine)
        done[routine] = total
        return total

    return {routine: cost(routine) for routine in list(alone)}, into


def read(symbol_addrs, elf, profile):
    """Gives the frames, the instructions that ran, the jumps, and the routine and the name of an address."""
    sized = set(re.findall(r"^(\S+) = \S+ // type:func size:", Path(symbol_addrs).read_text(), re.M))
    data = Path(elf).read_bytes()
    code, symbols = read_elf(data)
    ran, jumps = {}, []
    for line in Path(profile).read_text().splitlines():
        kind, *words = line.split()
        if kind == "frames":
            frames = int(words[0])
        elif kind == "at":
            ran[int(words[0], 16)] = [int(words[1], 16)] + [int(word) for word in words[2:]]
        else:
            jumps.append((kind == "call", int(words[0], 16), int(words[1], 16), int(words[2])))

    # The overlay of a shared address is the one with more of the words that ran.
    def holders(address):
        return [i for i, (_, start, size, _) in code.items() if start <= address < start + size]

    same = defaultdict(int)
    for address, row in ran.items():
        for i in holders(address):
            at = code[i][3] + address - code[i][1]
            same[i] += data[at:at + 4] == row[0].to_bytes(4, "big")

    # The name of an address is the game's label, then a function, then a thunk.
    names = defaultdict(dict)
    for name, value, size, kind, bind, section in symbols:
        rank = 0 if name in sized else 1 if kind == 2 and size else 2
        if rank < 2 or (code[section][0] == ".dragon" and kind == 0 and bind == 1):
            names[section][value] = min(names[section].get(value, (rank, name)), (rank, name))
    starts = {section: sorted(found) for section, found in names.items()}

    def routine(address):
        section = max(holders(address), key=lambda i: same[i], default=None)
        found = starts.get(section, [])
        i = bisect.bisect_right(found, address) - 1
        return (section, found[i]) if i >= 0 else None

    def name(routine):
        return names[routine[0]][routine[1]][1] if routine else "?"

    return frames, ran, jumps, routine, name


def main():
    args = sys.argv[1:]
    if len(args) not in (3, 4):
        sys.exit("usage: python3 tools/profile.py symbol_addrs.txt ELF PROFILE [IDLE]")
    frames, ran, jumps, routine, name = read(*args[:3])

    alone = defaultdict(lambda: [0] * 4)
    for address, (word, runs, lines, loads, backs) in ran.items():
        costs = alone[routine(address)]
        costs[0] += runs * cycles(word) + CODE_MISS * lines + DATA_MISS * loads + WRITE_BACK * backs
        for i, count in enumerate((lines, loads, backs)):
            costs[i + 1] += count
    calls = defaultdict(int)
    for call, source, target, count in jumps:
        caller, callee = routine(source), routine(target)
        if caller != callee and (call or (callee and callee[1] == target)):
            calls[caller, callee] += count
            alone.setdefault(callee, [0] * 4)
    whole, into = with_calls(alone, calls)

    def ms(cycles):
        return cycles / frames / HZ * 1000

    idle = [whole[routine] for routine in alone if len(args) == 4 and name(routine) == args[3]]
    total = [sum(costs[i] for costs in alone.values()) - sum(costs[i] for costs in idle) for i in range(4)]
    misses = CODE_MISS * total[1] + DATA_MISS * total[2] + WRITE_BACK * total[3]
    print(f"{frames} game frames of {ms(total[0]):.2f} ms: {ms(total[0] - misses):.2f} ms of instructions, and "
          f"{total[1] / frames:.0f} lines of code missed, {total[2] / frames:.0f} of data, "
          f"{total[3] / frames:.0f} written back")
    for costs in idle:
        print(f"the idle loop of {args[3]} takes {ms(costs[0]):.2f} ms more")
    print(f"\n{'':9}  {'alone':>34}  {'with what it calls':>34}")
    print(f"{'calls':>9}  " + f"{'ms':>7} {'code':>8} {'data':>8} {'back':>8}  " * 2 + "routine")
    for routine in sorted(alone, key=lambda routine: -alone[routine][0]):
        row = f"{into[routine] / frames:9.1f}  "
        for costs in (alone[routine], whole[routine]):
            row += f"{ms(costs[0]):7.3f} " + " ".join(f"{count / frames:8.1f}" for count in costs[1:]) + "  "
        print(row + name(routine))


if __name__ == "__main__":
    main()
