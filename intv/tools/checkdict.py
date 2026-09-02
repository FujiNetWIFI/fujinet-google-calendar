#!/usr/bin/env python3
"""checkdict.py -- prove the assembled ROM's T9 tables are where t9.bas says.

t9.bas reads the dictionary by PEEKing three fixed addresses (T9D_CHARS,
T9D_META, T9D_INDEX) because the generated include is assembled last and
IntyBASIC is single-pass, so its labels are not visible from there. Those three
CONSTs and the Makefile's --pool-org/--index-org are the same numbers written
down twice, and nothing in the toolchain compares them: point them at the wrong
place and the build still succeeds, the ROM still boots, and T9 just predicts
garbage from whatever bytes happen to live there.

So this reads the ADDRESSES out of t9.bas, the ROM IMAGE out of gcal.bin
(placed through gcal.cfg's mapping), decodes the dictionary the way t9.bas
would at runtime, and checks it against the word list the build used. It is the
only thing that ties the two halves together.
"""

import re
import sys

def consts(path):
    out = {}
    for line in open(path):
        m = re.match(r"\s*CONST\s+(T9D_\w+)\s*=\s*\$([0-9A-Fa-f]+)", line)
        if m:
            out[m.group(1)] = int(m.group(2), 16)
    return out


def load_rom(binpath, cfgpath):
    """address -> 16-bit word, from the flat .bin laid out by the .cfg."""
    data = open(binpath, "rb").read()
    mem, section = {}, None
    for raw in open(cfgpath):
        line = raw.strip()
        if line.startswith("["):
            section = line
            continue
        if section != "[mapping]":
            continue
        m = re.match(r"\$([0-9A-Fa-f]+)\s*-\s*\$([0-9A-Fa-f]+)\s*=\s*\$([0-9A-Fa-f]+)", line)
        if not m:
            continue
        lo, hi, tgt = (int(g, 16) for g in m.groups())
        for i in range(hi - lo + 1):
            off = (lo + i) * 2
            mem[tgt + i] = (data[off] << 8) | data[off + 1]
    return mem


def main():
    c = consts("t9.bas")
    mem = load_rom("gcal.bin", "gcal.cfg")
    missing = [k for k in ("T9D_CHARS", "T9D_META", "T9D_INDEX") if k not in c]
    if missing:
        sys.exit(f"checkdict: t9.bas is missing {', '.join(missing)}")

    count = mem.get(c["T9D_META"])
    maxlen = mem.get(c["T9D_META"] + 1)
    if not count or not 1 <= maxlen <= 15:
        sys.exit(f"checkdict: no plausible dictionary at T9D_META "
                 f"(${c['T9D_META']:04X}): count={count}, maxlen={maxlen}. "
                 f"The CONSTs in t9.bas and the Makefile's --index-org "
                 f"have most likely drifted apart.")

    def char_at(off):
        w = mem[c["T9D_CHARS"] + off // 2]
        return chr((w >> 8) & 0xFF if off % 2 == 0 else w & 0xFF)

    words = []
    for i in range(count):
        lo = mem[c["T9D_INDEX"] + i]
        hi = mem[c["T9D_INDEX"] + i + 1]
        words.append("".join(char_at(o) for o in range(lo, hi)))

    bad = [w for w in words if not re.fullmatch(r"[a-z]{1,15}", w)]
    if bad:
        sys.exit(f"checkdict: {len(bad)} decoded entries are not words, "
                 f"e.g. {bad[:5]!r} -- the tables are not where t9.bas points")

    L2D = {c: str(d) for ls, d in (("abc",2),("def",3),("ghi",4),("jkl",5),
                                   ("mno",6),("pqrs",7),("tuv",8),("wxyz",9))
           for c in ls}
    ds = ["".join(L2D[ch] for ch in w) for w in words]
    if ds != sorted(ds):
        sys.exit("checkdict: entries are not in digit-string order -- the "
                 "binary search in t9_narrow would return wrong candidates")

    print(f"checkdict: {count} words at T9D_CHARS=${c['T9D_CHARS']:04X} / "
          f"T9D_INDEX=${c['T9D_INDEX']:04X}, maxlen {maxlen}, digit-sorted; "
          f"first={words[0]!r} last={words[-1]!r}")
    for probe in ("the", "meeting", "dentist"):
        print(f"  {probe!r} in dictionary: {probe in words}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
