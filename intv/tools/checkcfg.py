#!/usr/bin/env python3
"""checkcfg.py -- fail the build if as1600 placed ROM anywhere it must not.

This replaces a check that compared each [mapping] line's target against a
list of four literal addresses. That worked while the program had one ORG per
segment, and stopped working the moment the dictionary introduced ORGs in the
middle of a segment ($B520, $D800): the targets are no longer segment bases,
so there is nothing to compare a name against. What matters was never the
address an ORG names -- it is where the bytes actually LAND.

So each region is expanded to its real [start, end] and checked three ways:

  1. It lies wholly inside a window this cartridge boots from. IntyBASIC
     SILENTLY auto-continues past the end of a segment, and the result links
     cleanly and fails EXEC's boot detection. This build has already produced
     a spill into $E000 once.

  2. $B800-$BFFF stays empty. GRAM is aliased there on the Intellivision bus,
     so ROM placed at $B800 is simply not what reads back -- there is no error,
     the data is just wrong.

  3. No two regions overlap.

Then the scratch RAM map in constants.bas, for the same class of mistake at
the other end of the bus: the STIC's control registers are mirrored over
$8000-$803F, so cart RAM declared there is written to the STIC as well as to
the RAM. That one cost an afternoon -- SC_EVT at $8000 put an event number
onto the colour stack and turned the header tan -- and it is invisible in the
cfg, so the constants are what get read.

And then, separately, that no code label sits inside either dictionary table.
That last check is the important one and the cfg alone CANNOT make it: when
code overruns its budget, the ORG in t9dict.bas rewinds the assembly pointer
and the table is written straight over the code, leaving ONE region in the
cfg and no error from as1600. It looks exactly like a clean build. This
happened during development -- st_form.bas grew past $AFFF and the letter pool
landed on top of frm_new -- and the only evidence was label_FRM_NEW sitting at
$B2D8, inside the pool. So the symbol table is what gets checked.

Prints the free space in each window on success, because the margins here are
thin and worth watching.
"""

import re
import sys

# Cartridge ROM windows this program is known to boot from. $7000 and $E000
# are deliberately absent -- both are known-bad -- and the pool window stops
# at $B7FF for the GRAM alias.
WINDOWS = [
    (0x5000, 0x6FFF, "$5000-$6FFF  main"),
    (0xA000, 0xB7FF, "$A000-$B7FF  text entry + dictionary index"),
    (0xD000, 0xDFFF, "$D000-$DFFF  cold views + dictionary pool"),
    (0xF000, 0xFFFF, "$F000-$FFFF  detail/picker + epilogue"),
]

LINE = re.compile(r"\$([0-9A-Fa-f]+)\s*-\s*\$([0-9A-Fa-f]+)\s*=\s*\$([0-9A-Fa-f]+)")
ORG = re.compile(r"^\s*ASM ORG \$([0-9A-Fa-f]+)", re.I)
DATA = re.compile(r"^\s*DATA\s+(.*)$", re.I)
SYM = re.compile(r"^0*([0-9A-Fa-f]{4,8})\s+(label_\S+)")
SCRATCH = re.compile(r"^\s*CONST\s+(SC_\w+)\s*=\s*\$([0-9A-Fa-f]+)", re.I)

# The STIC control registers, mirrored at $4000/$8000/$C000. Cart RAM starts
# at $8000, so the first 64 bytes of it are the register file -- see the map
# header in constants.bas.
STIC_ALIAS = (0x8000, 0x803F)


def dict_tables(path):
    """[(start, end, name)] for each ASM ORG'd table in the generated dict."""
    tables, org, count = [], None, 0
    for raw in open(path):
        m = ORG.match(raw)
        if m:
            if org is not None and count:
                tables.append((org, org + count - 1))
            org, count = int(m.group(1), 16), 0
            continue
        d = DATA.match(raw)
        if d and org is not None:
            count += len(d.group(1).split("'")[0].split(","))
    if org is not None and count:
        tables.append((org, org + count - 1))
    return tables


def check_labels(dictpath, sympath):
    """No code label may land inside a dictionary table."""
    try:
        tables = dict_tables(dictpath)
    except OSError:
        return []
    if not tables:
        return []
    # The dictionary's own labels are the tables.
    OWN = ("label_T9_CHARS", "label_T9_META", "label_T9_INDEX")
    bad = []
    try:
        syms = open(sympath)
    except OSError:
        return ["  (no .sym file -- run as1600 -s to enable the label check)"]
    for raw in syms:
        m = SYM.match(raw.strip())
        if not m:
            continue
        addr, name = int(m.group(1), 16), m.group(2)
        if name in OWN:
            continue
        for lo, hi in tables:
            if lo <= addr <= hi:
                bad.append(f"  {name} is at ${addr:04X}, inside the dictionary "
                           f"table at ${lo:04X}-${hi:04X} -- code overran its "
                           f"budget and the table was written over it")
    return bad


def check_scratch(path):
    """No SC_* may be declared inside the STIC's $8000 register mirror."""
    lo, hi = STIC_ALIAS
    bad = []
    try:
        src = open(path)
    except OSError:
        return [f"  (cannot read {path} -- the scratch RAM map is unchecked)"]
    for raw in src:
        m = SCRATCH.match(raw)
        if not m:
            continue
        addr = int(m.group(2), 16)
        if lo <= addr <= hi:
            bad.append(f"  {m.group(1)} is at ${addr:04X}, inside the STIC "
                       f"register mirror at ${lo:04X}-${hi:04X} -- writes "
                       f"there also land in the STIC")
    return bad


def main(path):
    regions, section = [], None
    for raw in open(path):
        line = raw.strip()
        if line.startswith("["):
            section = line
            continue
        # Only [mapping]. The [memattr] lines that follow describe the FujiNet
        # scratch RAM and legitimately name $8000/$9000.
        if section != "[mapping]":
            continue
        m = LINE.match(line)
        if m:
            lo, hi, tgt = (int(g, 16) for g in m.groups())
            regions.append((tgt, tgt + (hi - lo)))

    if not regions:
        sys.exit(f"checkcfg: no [mapping] regions in {path}")

    regions.sort()
    bad = []
    for start, end in regions:
        if not any(start >= lo and end <= hi for lo, hi, _ in WINDOWS):
            bad.append(f"  ${start:04X}-${end:04X} is not inside a bootable window")
        if start <= 0xBFFF and end >= 0xB800:
            bad.append(f"  ${start:04X}-${end:04X} reaches $B800+, which is the GRAM alias")
    for (s1, e1), (s2, e2) in zip(regions, regions[1:]):
        if e1 >= s2:
            bad.append(f"  ${s1:04X}-${e1:04X} overlaps ${s2:04X}-${e2:04X}")

    bad += check_scratch("constants.bas")
    bad += check_labels("t9dict.bas", "gcal.sym")

    if bad:
        print("SEGMENT CHECK FAILED:")
        print("\n".join(bad))
        return 1

    for lo, hi, name in WINDOWS:
        used = sum(e - s + 1 for s, e in regions if s >= lo and e <= hi)
        print(f"  {name:44s} {used:5d} used, {hi - lo + 1 - used:5d} free")
    print("segments OK")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1] if len(sys.argv) > 1 else "gcal.cfg"))
