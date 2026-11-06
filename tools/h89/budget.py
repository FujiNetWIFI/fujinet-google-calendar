#!/usr/bin/env python3
"""The H89 CP/M build's memory budget, read from the z88dk map.

z88dk's CP/M startup sets the stack pointer to the BDOS entry at (0006) and
lets the program run over the CCP, which the warm boot on exit reloads. So
everything from 0100H to the end of BSS, plus the stack, has to sit under
the BDOS of the smallest system the program runs on:

  CP/M on an SEBHC CF card                      BDOS entry CD06H
  62K CP/M (MOVCPM17 62 -- what FNDISK needs)   BDOS entry D806H
  64K CP/M (the emulator's boot disk)           BDOS entry E006H

The CF card's BIOS is the big one: DDT relocates to BD00H there, and DDT
sits 16 pages under the BDOS (D000H under the 64K system's E006H).

Past that, src/h89/tpa.asm stops the program at startup with the two
addresses -- on a CP/M with a bigger BIOS than Heath's, a CF card's, that
is all it can do -- so this fails the build before it gets that far on
the systems the program is meant for. (Without tpa.asm the startup's
clearing of BSS takes the BDOS and its own return address with it, and
the user is back at the CCP without a word.)

The .COM is the other limit. z88dk writes BSS into it as zeros, which
tpa.asm clears again anyway -- and the CCP will only load a file that ends
below itself, C500H on the CF card's system, so the zeros alone made it a
BAD LOAD (on the 62K system already, where the CCP is at D000H). --trim
cuts each .COM named off where BSS begins, which also takes a third off the
time an H17 spends loading it.

  budget.py MAP [--stack BYTES] [--trim COM...]

--stack is the Makefile's H89_STACK, the margin tpa.asm checks for too.
"""

import re
import sys

BDOS_CF = 0xCD06
BDOS_62K = 0xD806
BDOS_64K = 0xE006
CCP_CF = 0xC500


def main():
    args = sys.argv[1:]
    stack = 512
    trim = []
    if "--trim" in args:
        i = args.index("--trim")
        trim = args[i + 1:]
        del args[i:]
    if "--stack" in args:
        i = args.index("--stack")
        stack = int(args[i + 1], 0)
        del args[i:i + 2]
    if len(args) != 1:
        sys.exit(__doc__)

    syms = {}
    with open(args[0]) as f:
        for line in f:
            m = re.match(r"^(\w+)\s*=\s*\$([0-9A-Fa-f]+)\s*;", line)
            if m:
                syms[m.group(1)] = int(m.group(2), 16)

    end = max(syms.get("__tail", 0), syms.get("__BSS_END_tail", 0))
    if not end:
        sys.exit("budget.py: no __tail in %s" % args[0])

    code = syms.get("__code_compiler_size", 0)
    bss = syms.get("__bss_compiler_size", 0)
    top = end + stack
    print("h89 budget: end of program %04XH (%d bytes from 0100H), "
          "+%d stack = %04XH" % (end, end - 0x100, stack, top))
    print("h89 budget: compiled code %d, compiled BSS %d" % (code, bss))
    print("h89 budget: CF card CP/M ceiling %04XH: %+d spare; 62K %04XH: "
          "%+d; 64K %04XH: %+d" % (BDOS_CF, BDOS_CF - top,
                                   BDOS_62K, BDOS_62K - top,
                                   BDOS_64K, BDOS_64K - top))

    bss = syms.get("__BSS_head", 0)
    if trim and bss:
        if syms.get("__BSS_END_tail", end) != end:
            sys.exit("h89 budget: BSS is not last; cannot trim the .COM")
        for com in trim:
            with open(com, "r+b") as f:
                f.truncate(bss - 0x100)
        print("h89 budget: .COM cut to %d bytes, ending %04XH: %+d under the "
              "CF card's CCP" % (bss - 0x100, bss, CCP_CF - bss))
        if bss > CCP_CF:
            sys.exit("h89 budget: the .COM is past the CF card's CCP -- "
                     "BAD LOAD")

    if top > BDOS_CF:
        sys.exit("h89 budget: OVER the CF card's CP/M -- tpa.asm will refuse "
                 "to run there")


if __name__ == "__main__":
    main()
