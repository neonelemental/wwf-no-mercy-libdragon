#!/usr/bin/env python3
"""Copy samples of the first instrument bank out of the base ROM.

Writes OUT.bin, each sample as a 136-byte codebook and then its ADPCM frames,
and OUT.h, the offset and frame count of each one. Only samples with no loop.
"""
import struct
import sys

BANK = 0x16F32A0        # "N64 PtrTablesV2" of the first bank
WAVES = 0x16F6F10       # its sample data
PICK = (1, 3, 4, 5, 7, 8, 11, 17, 18, 20, 22, 23)


def main(rom_path, out):
    rom = open(rom_path, "rb").read()
    assert rom[BANK:BANK + 16] == b"N64 PtrTablesV2\0"
    count, _, _, ptrs = struct.unpack(">4I", rom[BANK + 0x20:BANK + 0x30])
    blob, rows = bytearray(), []
    for i in PICK:
        assert i < count
        d = BANK + struct.unpack(">I", rom[BANK + ptrs + 4 * i:BANK + ptrs + 4 * i + 4])[0]
        off, size, kind, _, _, loop, book = struct.unpack(">IIBBHII", rom[d:d + 0x14])
        order, npred = struct.unpack(">II", rom[BANK + book:BANK + book + 8])
        assert kind == 0 and loop == 0 and order == 2 and npred == 4, i
        frames = size // 9
        at = len(blob)
        blob += rom[BANK + book + 8:BANK + book + 8 + 128] + bytes(8)
        blob += rom[WAVES + off:WAVES + off + frames * 9]
        blob += bytes(-len(blob) % 16)
        rows.append("    { %d, %d, %d }," % (i, at, frames))
    open(out + ".bin", "wb").write(blob)
    open(out + ".h", "w").write("#define SAMPLES %d\nstatic const struct sample_row sample_rows[] = {\n%s\n};\n"
                                % (len(rows), "\n".join(rows)))


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
