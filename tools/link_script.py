"""Writes the linker script of one variant from the script of the split.

    python3 tools/link_script.py SPLIT OUT [--source]

It copies SPLIT/nomercy.ld to OUT/nomercy.ld with `. += PAD;` at the start
of main text, main data, main bss, and the text of ovl_a and ovl_e. ld takes
PAD by --defsym, so PAD=0 gives the retail layout. Code jumps into a slot
with no choice of the overlay in it. Thus each overlay must move a vram of a
slot by the same distance. Slot 2 starts at the end of ovl_a, and ovl_e
spans both slots, so ovl_b, ovl_c and ovl_d get no pad.

With --source, the script is that of a variant that cuts labels:
- each object of the asm of the split comes from the cut split, OUT/split/obj/;
- boot.o follows what is left of the entry, and main follows the entry, as
  boot.o is larger than the retail entry;
- patch_space.o follows the last object of main text;
- each object of this script is in KEEP, as the link drops each section of
  libdragon that no code reaches (--gc-sections);
- dragon.ld, which the link reads after this script, discards what no
  section takes, so this script discards nothing.

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


def source(text, split, out):
    """Changes the script of the split into the script of a variant that cuts labels."""
    asm = f"{split}/obj/{split}/asm/"
    lines = []
    wanted = {"entry": f"{out}/obj/boot/boot.o(.text);", "main": f"{out}/obj/boot/patch_space.o(.text);"}
    discard = False
    moved = False
    for line in text.splitlines():
        stripped = line.strip()
        if stripped == "/DISCARD/ :":
            discard = True
        if discard:
            discard = stripped != "}"
            continue
        line = line.replace(asm, f"{out}/split/obj/asm/")
        line = re.sub(r"^(\s+)(\S+\.o\(\.\w+\));$", r"\1KEEP(\2);", line)
        if ".main 0x80000450 :" in line:
            line, moved = line.replace(".main 0x80000450 :", ".main :"), True
        lines.append(line)
        for name in list(wanted):
            if stripped == f"{asm}{name}.s.o(.text);":
                lines.append(f"{line[: len(line) - len(line.lstrip())]}KEEP({wanted.pop(name)[:-1]});")
    if wanted or not moved:
        sys.exit("link_script.py: the script of the split has no place for boot.o or patch_space.o")
    return "\n".join(lines) + "\n"


def provide(text):
    out = []
    for line in text.splitlines():
        m = re.fullmatch(r"\s*(\w+) = (0x[0-9A-Fa-f]+);\s*", line)
        if m:
            out.append(f"PROVIDE({m.group(1)} = {m.group(2)});")
    return "\n".join(out) + "\n"


def main():
    args = [a for a in sys.argv[1:] if a != "--source"]
    if len(args) != 2:
        sys.exit("usage: python3 tools/link_script.py SPLIT OUT [--source]")
    split, out = Path(args[0]), Path(args[1])
    out.mkdir(parents=True, exist_ok=True)
    text = script((split / "nomercy.ld").read_text())
    if "--source" in sys.argv:
        text = source(text, split, out)
    (out / "nomercy.ld").write_text(text)
    for kind in ("funcs", "syms"):
        text = (split / f"undefined_{kind}_auto.txt").read_text()
        (out / f"undefined_{kind}.ld").write_text(provide(text))


if __name__ == "__main__":
    main()
