"""Copies the assembly of the split and cuts the labels of the replace lists.

    python3 tools/cut.py SPLIT OUT LIST... [--lift LIST...] [--check NAME...]

It copies each SPLIT/asm/**/*.s to OUT/asm/. For each label of each LIST, it
cuts the lines of that label from its file: from its glabel, alabel or dlabel
line to the next such line or the end of the file. A label that the list does not
give stays, as other code can jump to it. The `nonmatching` line, the
comments and the blank lines just above a label go with that label.

Each line of a list is `LABEL`, or `LABEL NAME` when our code gives NAME in its
place. For each `LABEL NAME`, OUT/../defsyms.txt gets `--defsym LABEL=NAME`, so
that each reference to LABEL goes to NAME.

A --lift list gives routines of the game that our C gives. Its line
`LABEL NAME` is cut as above, with the labels alone on the lines after it,
which are inside that routine. The words `int` or `float` after NAME say that
the routine gives a result, and `near` that our C may differ from the
assembly by 1 in a word or in each byte of it. `waits` says that the routine
waits, or calls one that does, so that no check can run it.

--check names routines of the lift lists by NAME, or each of them by `all`,
which leaves out those that wait. The assembly of such a routine stays, with
the name old_LABEL, and OUT/../checks.txt gets its line, from which
tools/thunks.py writes the LABEL that runs both.

The code writes some doubles by their halves, the high half through an odd
FPU register. That works only with the FR bit of the status register clear.
So it changes each such pair of `mtc1` to one write of the whole double,
which works with the bit clear or set, in the same two words. A pair of
`$zero` becomes `dmtc1 $zero` and a `nop`. A pair at an address of
double_addrs.txt, whose low half is always zero, becomes `dsll32 $at` of its
high half and `dmtc1 $at`. Any other access through an odd register stops it.

It writes a file only when its text changes, so make assembles only what the
cut changes.
"""

import re
import sys
from pathlib import Path

START = re.compile(r"\s*(?:glabel|alabel|dlabel)\s+([\w.$]+)")
NAMED = re.compile(r"(\s*(?:glabel|alabel|dlabel|endlabel|nonmatching)\s+)([\w.$]+)")
ABOVE = re.compile(r"\s*(?:$|/\*.*\*/\s*$|nonmatching\s)")
# An instruction of the split: its comment of rom, vram and word, then its text.
WORD = re.compile(r"(\s*/\* \w+ (\w{8}) \w{8} \*/\s+)(.*)")
ODD = re.compile(r"\$f[a-z]+\d+f\b")
HALF = re.compile(r"mtc1\s+(\$\w+), (\$f[a-z]+\d+)(f?)")


def lists(paths):
    """Gives each label of the lists, with its NAME or None."""
    labels = {}
    for path in paths:
        for n, line in enumerate(Path(path).read_text().splitlines(), 1):
            words = line.split("#")[0].split()
            if not words:
                continue
            if len(words) > 2 or words[0] in labels:
                sys.exit(f"cut.py: {path}:{n}: give LABEL or LABEL NAME, each label once")
            labels[words[0]] = words[1] if len(words) == 2 else None
    return labels


MARKS = {"int": 0x100, "float": 0x200, "near": 0x400, "waits": 0x800}
WAITS = MARKS["waits"]


def lifts(paths):
    """Gives (label, NAME, marks, inner labels) of each routine of the lift lists."""
    routines = []
    for path in paths:
        first = len(routines)
        for n, line in enumerate(Path(path).read_text().splitlines(), 1):
            words = line.split("#")[0].split()
            if len(words) == 1 and len(routines) > first:
                routines[-1][3].append(words[0])
            elif len(words) > 1 and all(w in MARKS for w in words[2:]):
                routines.append((words[0], words[1], sum(MARKS[w] for w in set(words[2:])), []))
            elif words:
                sys.exit(f"cut.py: {path}:{n}: give LABEL NAME [int|float] [near] [waits], then each label inside it")
    return routines


def options(args):
    """Gives the replace lists, the lift lists and the names to check."""
    words = {"": []}
    key = ""
    for arg in args:
        if arg in ("--lift", "--check"):
            key = arg
        words.setdefault(key, []).append(arg)
    return words[""], words.get("--lift", [])[1:], words.get("--check", [])[1:]


def blocks(lines):
    """Gives (label, first line, end line) of each label of a file."""
    starts = [(i, m.group(1)) for i, line in enumerate(lines) if (m := START.match(line))]
    firsts = []
    for i, _ in starts:
        first = i
        while first > 0 and ABOVE.match(lines[first - 1]):
            first -= 1
        firsts.append(first)
    # The run above a label never reaches back past the label before it.
    firsts = [max(f, starts[k - 1][0] + 1) if k else f for k, f in enumerate(firsts)]
    ends = firsts[1:] + [len(lines)]
    return [(label, first, end) for (_, label), first, end in zip(starts, firsts, ends)]


def cut(text, labels, kept, found):
    lines = text.splitlines(keepends=True)
    keep = [True] * len(lines)
    for i, line in enumerate(lines):
        if (m := NAMED.match(line)) and m.group(2) in kept:
            found.add(m.group(2))
            lines[i] = f"{m.group(1)}old_{m.group(2)}{line[m.end():]}"
    for label, first, end in blocks(lines):
        if label in labels:
            found.add(label)
            for i in range(first, end):
                # A section directive stays, so the lines after the cut keep their section.
                keep[i] = lines[i].lstrip().startswith(".section")
    return "".join(line for line, k in zip(lines, keep) if k)


def doubles(text, high):
    """Changes each write of a double by its halves to one write of all of it."""
    lines = text.splitlines(keepends=True)
    for i, line in enumerate(lines):
        if not ODD.search(line):
            continue
        low, top = WORD.match(lines[i - 1]), WORD.match(line)
        a = HALF.fullmatch(low.group(3).strip()) if low else None
        b = HALF.fullmatch(top.group(3).strip()) if top else None
        if not (a and b and not a.group(3) and b.group(3) and a.group(2) == b.group(2)):
            sys.exit(f"cut.py: {line.strip()}: an odd FPU register outside a pair of mtc1")
        reg = a.group(2)
        if a.group(1) == b.group(1) == "$zero":
            new = [f"dmtc1      $zero, {reg}", "nop"]
        elif int(low.group(2), 16) in high:
            new = [f"dsll32     $at, {b.group(1)}, 0", f"dmtc1      $at, {reg}"]
        else:
            sys.exit(f"cut.py: 0x{low.group(2)}: a double by its halves that double_addrs.txt does not give")
        lines[i - 1] = low.group(1) + new[0] + "\n"
        lines[i] = top.group(1) + new[1] + "\n"
    return "".join(lines)


def write(path, text):
    if not path.is_file() or path.read_text() != text:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)


def main():
    if len(sys.argv) < 3:
        sys.exit("usage: python3 tools/cut.py SPLIT OUT LIST... [--lift LIST...] [--check NAME...]")
    split, out = Path(sys.argv[1]), Path(sys.argv[2])
    replace, lift, check = options(sys.argv[3:])
    labels = lists(replace)
    routines = lifts(lift)
    unknown = sorted(set(check) - {"all"} - {name for _, name, _, _ in routines})
    if unknown:
        sys.exit(f"cut.py: no routine {', '.join(unknown)} in the lift lists")
    waiting = sorted(name for _, name, marks, _ in routines if marks & WAITS and name in check)
    if waiting:
        sys.exit(f"cut.py: {', '.join(waiting)} waits, so no check can run it")
    kept, checks = set(), []
    for label, name, marks, inner in routines:
        checked = not marks & WAITS and ("all" in check or name in check)
        new = {} if checked else {label: name, **dict.fromkeys(inner)}
        if label in kept or (set(new) | {label}) & set(labels):
            sys.exit(f"cut.py: {label} or a label inside it is in two lists")
        labels.update(new)
        if checked:
            kept.add(label)
            checks.append(f"{label} {name} {marks:#x}\n")
    high = {int(w[0], 16) for line in Path("double_addrs.txt").read_text().splitlines() if (w := line.split("#")[0].split())}
    found = set()
    sources = sorted((split / "asm").rglob("*.s"))
    for source in sources:
        write(out / source.relative_to(split), doubles(cut(source.read_text(), labels, kept, found), high))
    # A file that the split no longer has would still be assembled and linked.
    wanted = {out / s.relative_to(split) for s in sources}
    for old in (out / "asm").rglob("*.s"):
        if old not in wanted:
            old.unlink()
    missing = sorted((set(labels) | kept) - found)
    if missing:
        sys.exit(f"cut.py: no label {', '.join(missing)} in {split}/asm")
    defsyms = "".join(f"--defsym {label}={name}\n" for label, name in labels.items() if name)
    write(out.parent / "defsyms.txt", defsyms)
    write(out.parent / "checks.txt", "".join(checks))


if __name__ == "__main__":
    main()
