"""Writes the linker script of one variant from the script of the split.

    python3 tools/link_script.py SPLIT OUT

It copies SPLIT/nomercy.ld to OUT/nomercy.ld with `. += PAD;` at the start
of main text, main data and main bss, and a pad of PAD plus 0x10 times its
rom order at the start of each overlay's text. ld takes PAD by --defsym, so
PAD=0 gives the retail layout. Each overlay has its own pad, so a reference
that names the wrong overlay gives a wrong value in a padded build.

It also writes OUT/undefined_funcs.ld and OUT/undefined_syms.ld from the
lists of the split, with each line in PROVIDE. splat lists symbols that an
object also defines, and a plain assignment in a script wins over the object.
"""

import re
import sys
from pathlib import Path

OVERLAYS = ["ovl_a", "ovl_b", "ovl_c", "ovl_d", "ovl_e"]


def pads():
    out = {name: "PAD" for name in ("main_TEXT_START", "main_DATA_START", "main_BSS_START")}
    for order, name in enumerate(OVERLAYS):
        out[f"{name}_TEXT_START"] = f"PAD ? PAD + 0x{order * 0x10:X} : 0"
    return out


def script(text):
    wanted = pads()
    lines = []
    for line in text.splitlines():
        lines.append(line)
        m = re.fullmatch(r"(\s*)(\w+) = \.;", line)
        if m and m.group(2) in wanted:
            lines.append(f"{m.group(1)}. += {wanted.pop(m.group(2))};")
    if wanted:
        sys.exit(f"link_script.py: no place for the pad at {', '.join(sorted(wanted))}")
    return "\n".join(lines) + "\n"


def provide(text):
    out = []
    for line in text.splitlines():
        m = re.fullmatch(r"\s*(\w+) = (0x[0-9A-Fa-f]+);\s*", line)
        if m:
            out.append(f"PROVIDE({m.group(1)} = {m.group(2)});")
    return "\n".join(out) + "\n"


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: python3 tools/link_script.py SPLIT OUT")
    split, out = Path(sys.argv[1]), Path(sys.argv[2])
    out.mkdir(parents=True, exist_ok=True)
    (out / "nomercy.ld").write_text(script((split / "nomercy.ld").read_text()))
    for kind in ("funcs", "syms"):
        text = (split / f"undefined_{kind}_auto.txt").read_text()
        (out / f"undefined_{kind}.ld").write_text(provide(text))


if __name__ == "__main__":
    main()
