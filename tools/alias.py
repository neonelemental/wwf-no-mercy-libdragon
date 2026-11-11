"""Prints the routines of a profile that share lines of the instruction cache.

    python3 tools/alias.py symbol_addrs.txt ELF PROFILE [ROWS]

The instruction cache of the console holds 16 KB in 512 lines of 32 bytes,
direct mapped: two lines of code that lie a multiple of 16 KB apart take the
same line of the cache, and each pushes the other out of it. The arguments
are those of tools/profile.py.

A row is a pair of routines with code on the same lines of the cache, where
both missed: the lines that the two share, and the misses of each routine on
them in one game frame. The pairs that lose most to each other come first:
on each shared line, the smaller of the two counts.
"""

import sys
from collections import defaultdict

sys.dont_write_bytecode = True
from profile import read  # noqa: E402

LINE, LINES = 32, 512


def main():
    args = sys.argv[1:]
    if len(args) not in (3, 4):
        sys.exit("usage: python3 tools/alias.py symbol_addrs.txt ELF PROFILE [ROWS]")
    frames, ran, _, routine, name = read(*args[:3])
    rows = int(args[3]) if len(args) == 4 else 40

    # The misses of each routine on each line of code, by the line of the cache.
    slots = defaultdict(lambda: defaultdict(int))
    for address, (_, _, misses, _, _) in ran.items():
        if misses:
            slots[address // LINE % LINES][address // LINE, routine(address)] += misses

    pairs = defaultdict(lambda: [0, 0, 0, 0])
    for found in slots.values():
        found = sorted(found.items(), key=lambda item: (name(item[0][1]), item[0][0]))
        for i, ((line, one), first) in enumerate(found):
            for (other, two), second in found[i + 1:]:
                if other != line:
                    pair = pairs[one, two]
                    for at, count in enumerate((1, first, second, min(first, second))):
                        pair[at] += count

    print(f"{'lines':>5} {'misses':>8} {'misses':>8}  routines")
    for (one, two), (lines, first, second, _) in sorted(pairs.items(), key=lambda item: -item[1][3])[:rows]:
        print(f"{lines:5} {first / frames:8.1f} {second / frames:8.1f}  {name(one)}  {name(two)}")


if __name__ == "__main__":
    main()
