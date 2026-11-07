"""Copies the assembly of the split and cuts the labels of the replace lists.

    python3 tools/cut.py SPLIT OUT LIST...

It copies each SPLIT/asm/**/*.s to OUT/asm/. For each label of each LIST, it
cuts the lines of that label from its file: from its glabel, alabel or dlabel
line to the next such line or the end of the file. A label that the list does not
give stays, as other code can jump to it. The `nonmatching` line, the
comments and the blank lines just above a label go with that label.

Each line of a list is `LABEL`, or `LABEL NAME` when our code gives NAME in its
place. For each `LABEL NAME`, OUT/../defsyms.txt gets `--defsym LABEL=NAME`, so
that each reference to LABEL goes to NAME.

It writes a file only when its text changes, so make assembles only what the
cut changes.
"""

import re
import sys
from pathlib import Path

START = re.compile(r"\s*(?:glabel|alabel|dlabel)\s+([\w.$]+)")
ABOVE = re.compile(r"\s*(?:$|/\*.*\*/\s*$|nonmatching\s)")


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


def cut(text, labels, found):
    lines = text.splitlines(keepends=True)
    keep = [True] * len(lines)
    for label, first, end in blocks(lines):
        if label in labels:
            found.add(label)
            for i in range(first, end):
                # A section directive stays, so the lines after the cut keep their section.
                keep[i] = lines[i].lstrip().startswith(".section")
    return "".join(line for line, k in zip(lines, keep) if k)


def write(path, text):
    if not path.is_file() or path.read_text() != text:
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(text)


def main():
    if len(sys.argv) < 3:
        sys.exit("usage: python3 tools/cut.py SPLIT OUT LIST...")
    split, out = Path(sys.argv[1]), Path(sys.argv[2])
    labels = lists(sys.argv[3:])
    found = set()
    sources = sorted((split / "asm").rglob("*.s"))
    for source in sources:
        write(out / source.relative_to(split), cut(source.read_text(), labels, found))
    # A file that the split no longer has would still be assembled and linked.
    wanted = {out / s.relative_to(split) for s in sources}
    for old in (out / "asm").rglob("*.s"):
        if old not in wanted:
            old.unlink()
    missing = sorted(set(labels) - found)
    if missing:
        sys.exit(f"cut.py: no label {', '.join(missing)} in {split}/asm")
    defsyms = "".join(f"--defsym {label}={name}\n" for label, name in labels.items() if name)
    write(out.parent / "defsyms.txt", defsyms)


if __name__ == "__main__":
    main()
