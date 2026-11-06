"""Writes the linker script of one variant from the script of the split.

    python3 tools/link_script.py SPLIT OUT

It copies SPLIT/nomercy.ld to OUT/nomercy.ld with `. += PAD;` at the start
of main text, main data, main bss, and the text of ovl_a and ovl_e. ld takes
PAD by --defsym, so PAD=0 gives the retail layout. Code jumps into a slot
with no choice of the overlay in it. Thus each overlay must move a vram of a
slot by the same distance. Slot 2 starts at the end of ovl_a, and ovl_e
spans both slots, so ovl_b, ovl_c and ovl_d get no pad.

It also writes OUT/undefined_funcs.ld and OUT/undefined_syms.ld from the
lists of the split, with each line in PROVIDE. splat lists symbols that an
object also defines, and a plain assignment in a script wins over the object.
"""

import re
import sys
from pathlib import Path

SLOT1 = ["ovl_a", "ovl_e"]


def pads():
    names = ["main_TEXT_START", "main_DATA_START", "main_BSS_START"]
    return {name: "PAD" for name in names + [f"{o}_TEXT_START" for o in SLOT1]}


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
