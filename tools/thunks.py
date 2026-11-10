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

A second float or double argument is in $f14 for the assembly and in $f13 for
our C, and each thunk copies it. A --hand object is assembly of ours that
gives such a NAME itself, for a call that passes a 64-bit integer, or an
integer after a double, which no rule here can move. No thunk goes into the
split with more than four arguments.

A line `LABEL NAME MARKS` of OUT/checks.txt is a routine of the game that the
build checks: the assembly is old_LABEL, and our C is NAME. Its thunk LABEL
runs the assembly and then our C, with the same arguments and with interrupts
off, and tells the harness's emulator of each step by a write to the
cartridge at 0xB3FE0000. The emulator gives our C the memory that the
assembly had, compares what the two left, and leaves what the assembly left,
which is also what the caller gets. A call from inside such a run goes to the
assembly alone.

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
	dmfc1	$1, $f14
	dmtc1	$1, $f13
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
{save}	dmfc1	$1, $f13
	dmtc1	$1, $f14
	jal	{name}
	nop
{load}	jr	$31
	addiu	$sp, $sp, 112

"""


# The arguments on the stack, for a callee that takes them from this frame.
STACK = """\
	lw	$8, 144($sp)
	lw	$9, 148($sp)
	lw	$10, 152($sp)
	lw	$11, 156($sp)
	sw	$8, 16($sp)
	sw	$9, 20($sp)
	sw	$10, 24($sp)
	sw	$11, 28($sp)
"""

# The emulator takes the name, the end of the routine's stack, the result of
# each run in $2 and $f0, and the steps 1, 2 and 3 with the marks.
CHECK = """\
	.globl	{label}
{label}:
	lui	$1, %hi(lift_checking)
	lw	$1, %lo(lift_checking)($1)
	beqz	$1, 1f
	nop
	j	old_{label}
	nop
1:	addiu	$sp, $sp, -128
	sd	$31, 32($sp)
	sd	$4, 40($sp)
	sd	$5, 48($sp)
	sd	$6, 56($sp)
	sd	$7, 64($sp)
	sdc1	$f12, 72($sp)
	sdc1	$f14, 80($sp)
	mfc0	$8, $12
	sd	$8, 88($sp)
	addiu	$9, $0, -2
	and	$9, $8, $9
	mtc0	$9, $12
	nop
	nop
{stack}	lui	$1, %hi(lift_checking)
	addiu	$8, $0, 1
	sw	$8, %lo(lift_checking)($1)
	lui	$10, 0xb3fe
	lui	$9, %hi(2f)
	addiu	$9, $9, %lo(2f)
	sw	$9, 4($10)
	addiu	$9, $sp, 144
	sw	$9, 8($10)
	sw	$8, 0($10)
	jal	old_{label}
	nop
	sd	$2, 96($sp)
	sd	$3, 104($sp)
	sdc1	$f0, 112($sp)
	lui	$10, 0xb3fe
	sw	$2, 16($10)
	mfc1	$9, $f0
	sw	$9, 20($10)
	addiu	$9, $0, 2
	sw	$9, 0($10)
	ld	$4, 40($sp)
	ld	$5, 48($sp)
	ld	$6, 56($sp)
	ld	$7, 64($sp)
	ldc1	$f12, 72($sp)
	ldc1	$f14, 80($sp)
{stack}	jal	{name}
	nop
	lui	$10, 0xb3fe
	sw	$2, 24($10)
	mfc1	$9, $f0
	sw	$9, 28($10)
	addiu	$9, $0, {marks} + 3
	sw	$9, 0($10)
	lui	$1, %hi(lift_checking)
	sw	$0, %lo(lift_checking)($1)
	ld	$2, 96($sp)
	ld	$3, 104($sp)
	ldc1	$f0, 112($sp)
	ld	$8, 88($sp)
	mtc0	$8, $12
	ld	$31, 32($sp)
	jr	$31
	addiu	$sp, $sp, 128
	.section .rodata
2:	.asciz	"{name} ({label})"
	.section .text

"""

CHECKING = """\
	.section .bss
	.align	2
lift_checking:
	.space	4
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
    checks = [line.split() for line in (out / "checks.txt").read_text().splitlines()]
    missing = sorted({name for _, name, _ in checks} - c_defined - hand_defined)
    if missing:
        sys.exit(f"thunks.py: our C has no routine {', '.join(missing)}")
    to_c = sorted(c_defined & (split_undefined | set(named) | {name for _, name, _ in checks}))
    to_asm = sorted(c_undefined & split_defined)

    text = "# Written by tools/thunks.py.\n\t.set\tnoreorder\n\t.set\tnoat\n\t.section .text\n\n"
    for name in to_c:
        if name not in hand_defined:
            text += TO_C.format(name=name)
    save = "".join(f"\tsd\t${reg}, {32 + 8 * i}($sp)\n" for i, reg in enumerate(SAVED))
    load = "".join(f"\tld\t${reg}, {32 + 8 * i}($sp)\n" for i, reg in enumerate(SAVED))
    for name in to_asm:
        text += TO_ASM.format(name=name, save=save, load=load)
    for label, name, marks in checks:
        text += CHECK.format(label=label, name=name, marks=marks, stack=STACK)
    if checks:
        text += CHECKING
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
