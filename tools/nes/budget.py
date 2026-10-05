#!/usr/bin/env python3
"""budget.py MAP -- how full each part of the NES image is.

ld65 fails a link that overflows a memory area, so this is not the guard
against overflow; it is the warning before it. Every bank and the WRAM are
printed with what is left, and anything with less than WARN bytes to spare
is flagged -- the moment to look for room is before the edit that needs it.
Exits non-zero only if something is actually over (which ld65 would also
have refused).
"""
import re
import sys

WARN = 256

AREAS = [
    # name, segments, start, end (exclusive)
    ("WRAM (data)", ["DATA", "BSS"], 0x6000, 0x8000),
    ("fixed $C000", ["STARTUP", "LOWCODE", "ONCE", "CODE", "RODATA", "+DATA"], 0xC000, 0xFFF0),
    ("R7 $A000", ["CODE2"], 0xA000, 0xC000),
    ("bank 0 net", ["OVL_NET"], 0x8000, 0xA000),
    ("bank 1 views", ["OVL_VIEWS"], 0x8000, 0xA000),
    ("bank 2 grid", ["OVL_GRID"], 0x8000, 0xA000),
    ("bank 3 detail", ["OVL_DETAIL"], 0x8000, 0xA000),
    ("bank 4 form", ["OVL_FORM"], 0x8000, 0xA000),
    ("bank 5 pickers", ["OVL_PICK"], 0x8000, 0xA000),
    ("bank 6 screens", ["OVL_BOOT"], 0x8000, 0xA000),
]


def main():
    if len(sys.argv) != 2:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    text = open(sys.argv[1]).read()
    segs = {}
    for name, start, end, size in re.findall(
            r"^(\w+)\s+([0-9A-F]{6})\s+([0-9A-F]{6})\s+([0-9A-F]{6})", text, re.M):
        segs[name] = (int(start, 16), int(size, 16))

    over = False
    for name, names, lo, hi in AREAS:
        used = 0
        for s in names:
            if s == "+DATA":
                used += segs.get("DATA", (0, 0))[1]     # its load image
            elif s in segs:
                used += segs[s][1]
        free = (hi - lo) - used
        flag = "OVER" if free < 0 else ("low" if free < WARN else "")
        over |= free < 0
        print("budget.py: %-15s %6d used %6d free  %s" % (name, used, free, flag))
    print("budget.py: C stack $0400-$07FF (1024), sprites $0200/$0300")
    return 1 if over else 0


if __name__ == "__main__":
    sys.exit(main())
