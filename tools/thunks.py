"""Writes the thunks between the game's assembly and our C.

    python3 tools/thunks.py OUT NM OBJCOPY --split OBJ... --c OBJ... --hand OBJ...

The assembly of the split keeps the o32 calls of the retail game: 4-byte
argument slots on the stack, 16 bytes of home space, and saved registers of
32 bits. GCC builds our C at o64, as libdragon is: 8-byte slots, 32 bytes of
home space, and registers of 64 bits. So each call between the two goes
through a thunk.

- A routine NAME of our C that the split calls, by its name or through a line
  of OUT/defsyms.txt: our C keeps it as c_NAME, and the thunk NAME makes the
  o64 frame, copies four stack arguments to 8-byte slots and calls c_NAME.
- A routine LABEL of the split that our C calls: our C calls asm_LABEL, which
  keeps all 64 bits of each saved register around the call.

A --hand object is assembly of ours that gives such a NAME itself, for a call
that passes a 64-bit value, which no rule here can move. No thunk here moves a
double or a second float argument, and none goes into the split with more than
four arguments.

It writes OUT/thunk/thunks.S, and a copy of each --c object under OUT/thunk/
with each name changed.
"""

import subprocess
import sys
from pathlib import Path

TO_C = """\
	.globl	{name}
{name}:
	addiu	$sp, $sp, -80
	sd	$31, 72($sp)
	lw	$8, 96($sp)
	lw	$9, 100($sp)
	lw	$10, 104($sp)
	lw	$11, 108($sp)
	sd	$8, 32($sp)
	sd	$9, 40($sp)
	sd	$10, 48($sp)
	sd	$11, 56($sp)
	jal	c_{name}
	nop
	ld	$31, 72($sp)
	jr	$31
	addiu	$sp, $sp, 80

"""

SAVED = [16, 17, 18, 19, 20, 21, 22, 23, 30, 31]

TO_ASM = """\
	.globl	asm_{name}
asm_{name}:
	addiu	$sp, $sp, -112
{save}	jal	{name}
	nop
{load}	jr	$31
	addiu	$sp, $sp, 112

"""


def symbols(nm, objects):
    """Gives the routines that the objects define, and the names they lack."""
    defined, undefined = set(), set()
    for obj in objects:
        out = subprocess.run([nm, "-g", obj], capture_output=True, text=True, check=True).stdout
        for line in out.splitlines():
            words = line.split()
            if len(words) == 2 and words[0] == "U":
                undefined.add(words[1])
            elif len(words) == 3 and words[1] == "T":
                defined.add(words[2])
    return defined, undefined


def main():
    args = sys.argv[1:]
    if len(args) < 4 or "--split" not in args or "--c" not in args or "--hand" not in args:
        sys.exit("usage: python3 tools/thunks.py OUT NM OBJCOPY --split OBJ... --c OBJ... --hand OBJ...")
    out, nm, objcopy = Path(args[0]), args[1], args[2]
    split = args[args.index("--split") + 1 : args.index("--c")]
    ours = args[args.index("--c") + 1 : args.index("--hand")]
    hand = args[args.index("--hand") + 1 :]

    split_defined, split_undefined = symbols(nm, split)
    c_defined, c_undefined = symbols(nm, ours)
    hand_defined, _ = symbols(nm, hand)
    named = [line.split("=")[1].strip() for line in (out / "defsyms.txt").read_text().splitlines() if "=" in line]
    to_c = sorted(c_defined & (split_undefined | set(named)))
    to_asm = sorted(c_undefined & split_defined)

    text = "# Written by tools/thunks.py.\n\t.set\tnoreorder\n\t.set\tnoat\n\t.section .text\n\n"
    for name in to_c:
        if name not in hand_defined:
            text += TO_C.format(name=name)
    save = "".join(f"\tsd\t${reg}, {32 + 8 * i}($sp)\n" for i, reg in enumerate(SAVED))
    load = "".join(f"\tld\t${reg}, {32 + 8 * i}($sp)\n" for i, reg in enumerate(SAVED))
    for name in to_asm:
        text += TO_ASM.format(name=name, save=save, load=load)
    thunk = out / "thunk"
    thunk.mkdir(parents=True, exist_ok=True)
    (thunk / "thunks.S").write_text(text)
    names = thunk / "names.txt"
    names.write_text("".join(f"{n} c_{n}\n" for n in to_c) + "".join(f"{n} asm_{n}\n" for n in to_asm))
    for obj in ours:
        copy = thunk / Path(obj).relative_to(out / "obj")
        copy.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run([objcopy, f"--redefine-syms={names}", obj, str(copy)], check=True)


if __name__ == "__main__":
    main()
