#!/usr/bin/env python3
"""checksize.py -- per-region ROM budget table from the zmac listing.

gcal.asm brackets every module with an MB_* fence label. The banked image
has four code regions -- page 0 (the screens), the resident high half, and
the form/settings/splash pages -- and every page's labels are PHASEd to
0x2000, so a global address sort would interleave them; instead each region
declares its fence order and is budgeted on its own. Fails the build when
any region is over; grumbles when page 0 or the resident half run low.

Usage: checksize.py build/gcal.lst
"""

import re
import sys

WARN_SPARE = 100        # grumble (but pass) when a tight region drops below

# (name, budget, warn?, ordered fences). A fence may be absent (the DEMO
# build swaps include sets); spans pair consecutive PRESENT fences.
REGIONS = [
    ("page 0",   4096, True,  ["MB_MAIN", "MB_DATA", "MB_VIEWS", "MB_MONTH",
                               "MB_WEEK", "MB_DETAIL", "MB_PARSE", "MB_ALARM",
                               "MB_SOUND", "MB_P0END"]),
    ("resident", 2816, True,  ["MB_BANK", "MB_SHARED", "MB_UI", "MB_NET",
                               "MB_URL", "MB_DATE", "MB_CLOCK", "MB_INPUT",
                               "MB_STATE", "MB_GFX", "MB_FONT", "MB_FUJILIB",
                               "MB_RESEND"]),
    ("page 2",   4096, False, ["MB_PG2", "MB_FORM", "MB_EDIT", "MB_PG2END"]),
    ("page 3",   4096, False, ["MB_PG3", "MB_PICK", "MB_APPKEY",
                               "MB_PG3END"]),
    ("page 4",   4096, False, ["MB_PG4", "MB_SPLASH", "MB_DEMO",
                               "MB_PG4END"]),
]


def read_symbols(path: str) -> dict[str, int]:
    """MB_* fence labels from the zmac symbol table, name -> address."""
    syms = {}
    with open(path, encoding="utf-8", errors="replace") as f:
        text = f.read()
    # zmac's symbol table lowercases names and lists them as "name value",
    # several pairs per line, a trailing + marking multiple references.
    for name, val in re.findall(
            r"\b(mb_[a-z0-9_]+)\s+=?\s*([0-9a-f]{1,5})\+?", text):
        syms[name.upper()] = int(val, 16)
    return syms


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__.strip(), file=sys.stderr)
        return 2
    syms = read_symbols(sys.argv[1])
    if not syms:
        print("checksize: no MB_* fence labels found in the listing",
              file=sys.stderr)
        return 1

    bad = 0
    print("checksize: region budgets")
    for region, budget, warn, order in REGIONS:
        present = [(n, syms[n]) for n in order if n in syms]
        if len(present) < 2:
            continue
        for (name, addr), (_, nxt) in zip(present, present[1:]):
            size = nxt - addr
            if size:
                print(f"  {region:<9} {name[3:]:<8} {size:5} bytes")
        used = present[-1][1] - present[0][1]
        spare = budget - used
        print(f"  {region:<9} {'total':<8} {used:5} of {budget} "
              f"({spare} spare)")
        if spare < 0:
            print(f"checksize: {region} is {-spare} bytes over",
                  file=sys.stderr)
            bad = 1
        elif warn and spare < WARN_SPARE:
            print(f"checksize: {region} headroom under {WARN_SPARE} bytes",
                  file=sys.stderr)
    return bad


if __name__ == "__main__":
    sys.exit(main())
