"""Writes the two CRC words of the header that the CIC 6102 IPL3 checks.

    python3 tools/n64crc.py ROM

The IPL3 sums the words of ROM 0x1000..0x101000 and hangs when the header
words at 0x10 and 0x14 differ from its sum.
"""

import sys
import zlib

IPL3_6102 = 0x90BB6CB5
SEED = 0xF8CA4DDC
START, LENGTH = 0x1000, 0x100000
MASK = 0xFFFFFFFF


def crc(rom):
    t1 = t2 = t3 = t4 = t5 = t6 = SEED
    for i in range(START, START + LENGTH, 4):
        d = int.from_bytes(rom[i:i + 4], "big")
        if (t6 + d) & MASK < t6:
            t4 = (t4 + 1) & MASK
        t6 = (t6 + d) & MASK
        t3 ^= d
        n = d & 0x1F
        r = ((d << n) | (d >> (32 - n))) & MASK
        t5 = (t5 + r) & MASK
        if t2 > d:
            t2 ^= r
        else:
            t2 ^= t6 ^ d
        t1 = (t1 + (t5 ^ d)) & MASK
    return t6 ^ t4 ^ t3, t5 ^ t2 ^ t1


def main():
    if len(sys.argv) != 2:
        sys.exit("usage: n64crc.py ROM")
    path = sys.argv[1]
    with open(path, "rb") as f:
        rom = bytearray(f.read())
    if len(rom) < START + LENGTH:
        sys.exit(f"{path}: {len(rom)} bytes, the IPL3 sums up to 0x{START + LENGTH:x}")
    if zlib.crc32(rom[0x40:0x1000]) != IPL3_6102:
        sys.exit(f"{path}: the IPL3 is not the one of CIC 6102")
    c1, c2 = crc(rom)
    rom[0x10:0x18] = c1.to_bytes(4, "big") + c2.to_bytes(4, "big")
    with open(path, "wb") as f:
        f.write(rom)
    print(f"{path}: {c1:08x} {c2:08x}")


if __name__ == "__main__":
    main()
