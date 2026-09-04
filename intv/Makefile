# Makefile -- Google Calendar client for the Intellivision (IntyBASIC).
#
# Needs IntyBASIC v1.4.2, jzintv's as1600, and python3 (for the dictionary
# generator and the segment check). Only gcal.bas is handed to the compiler;
# every other .bas arrives through an INCLUDE and is listed here solely so a
# change to one retriggers the build.
#
# RUN `make check` AFTER ANY BUILD THAT ADDS CODE. Two different silent
# failures are waiting, and neither produces an error from the toolchain:
# IntyBASIC continues past the end of a full segment into whatever is next
# (producing a cart that links cleanly and fails EXEC's boot detection), and
# t9dict.bas's ASM ORGs rewind the assembly pointer, so code that outgrows its
# budget is simply overwritten by the dictionary. tools/checkcfg.py catches
# both -- see its header.

INTYBASIC ?= intybasic
AS1600    ?= as1600
LIBDIR    ?= $(HOME)/Workspace/IntyBASIC/intybasic/
PYTHON    ?= python3

PRODUCT = gcal
SOURCES = gcal.bas constants.bas fujinet.bas clock.bas gcalnet.bas \
          screen.bas scroll.bas input.bas wrap.bas bar.bas gfx.bas \
          st_view.bas st_day.bas st_week.bas st_month.bas st_agenda.bas \
          st_event.bas st_pick.bas alarm.bas t9.bas st_form.bas

# ---------------------------------------------------------------------------
# The T9 dictionary.
#
# DICT_COUNT is the tuning knob for the whole feature: entries cost about 2.5
# ROM words each in the pool plus 1 in the index, and what is left after the
# code is not generous. 650 is what fits with roughly 80 words of margin on
# each table; raise it and `make check` will tell you, immediately and
# specifically, which table ran into which.
#
# THE FOUR ADDRESSES BELOW ARE DUPLICATED AS CONSTs IN t9.bas (T9D_CHARS,
# T9D_META, T9D_INDEX) AND NOTHING VERIFIES THE AGREEMENT. t9.bas reads the
# tables by PEEKing fixed addresses -- it has to, because the generated file is
# included last and IntyBASIC is single-pass, so its labels are not visible
# from there. Change one, change the other; a mismatch does not fail the build,
# it just predicts garbage.
#
# The pool goes at $D800, above the cold views, and the index at $B520, above
# the text-entry code. They are split across two windows rather than kept
# together because the pool is ~2.5x the index and the two free holes are
# different sizes -- see the segment map printed by `make check`.
DICT_WORDS ?= wordlist/gcal-t9.txt
DICT_COUNT ?= 650
DICT_POOL_ORG    ?= $$D800
DICT_POOL_LIMIT  ?= 2048
DICT_INDEX_ORG   ?= $$B520
DICT_INDEX_LIMIT ?= 736

# Switching DICT_WORDS or DICT_COUNT has to force a regeneration even though
# neither file changed, so the stamp carries both in its name and its rule
# removes any other.
DICTSTAMP = .dictstamp-$(notdir $(DICT_WORDS))-$(DICT_COUNT)

all: $(PRODUCT).bin $(PRODUCT).rom

$(DICTSTAMP): $(DICT_WORDS) tools/mkdict.py
	@rm -f .dictstamp-*
	$(PYTHON) tools/mkdict.py --count $(DICT_COUNT) \
	    --pool-org  '$(DICT_POOL_ORG)'  --pool-limit  $(DICT_POOL_LIMIT) \
	    --index-org '$(DICT_INDEX_ORG)' --index-limit $(DICT_INDEX_LIMIT) \
	    $(DICT_WORDS) -o t9dict.bas
	@touch $@

t9dict.bas: $(DICTSTAMP)

$(PRODUCT).asm: $(SOURCES) t9dict.bas
	$(INTYBASIC) gcal.bas $(PRODUCT).asm $(LIBDIR)

# .bin + .cfg -- for loading from the cartridge SD. The .sym is not optional:
# `make check` needs it to prove no code landed inside a dictionary table.
$(PRODUCT).bin: $(PRODUCT).asm
	$(AS1600) -o $(PRODUCT).bin -l $(PRODUCT).lst -s $(PRODUCT).sym $(PRODUCT).asm

# .rom -- Intellicart format, for jzIntv and network boot.
$(PRODUCT).rom: $(PRODUCT).asm
	$(AS1600) -o $(PRODUCT).rom $(PRODUCT).asm

check: $(PRODUCT).bin
	@$(PYTHON) tools/checkcfg.py $(PRODUCT).cfg
	@$(PYTHON) tools/checkdict.py

run: $(PRODUCT).rom
	./run.sh

# kbdiag.rom -- what the console is actually reporting on the controller port.
# Standalone: no includes, no FujiNet, no clock. Reach for it when input misbehaves
# and the screen cannot tell you why -- a key that never lifts looks exactly like
# a key pressed over and over. See the header of kbdiag.bas.
kbdiag: kbdiag.rom
kbdiag.rom: kbdiag.bas
	$(INTYBASIC) kbdiag.bas kbdiag.asm $(LIBDIR)
	$(AS1600) -o kbdiag.rom kbdiag.asm
	@rm -f kbdiag.asm

# The scripted UI tests drive jzIntv's debugger headless -- no display, no
# hands -- through the injection hook in in_poll. See tests/README.md.
#
# Everything past boot.txt needs a FujiNet to talk to, because gcal halts at
# boot without a clock:
#
#     make test FUJINET=localhost:9995
#
# tests/live/ is deliberately NOT run here. Those scenarios COMMIT EVENTS to
# whichever Google account the FujiNet is authorised against, and this client
# cannot delete them again. Run one by name when you mean it.
FUJINET ?=
test: $(PRODUCT).rom $(PRODUCT).sym
	@for s in tests/*.txt; do \
	    echo "--- $$s"; \
	    JZINTV_ARGS="$(if $(FUJINET),--fujinet=$(FUJINET))" \
	        $(PYTHON) tools/intv_screen.py $(PRODUCT).rom $$s || exit 1; \
	done

# One scenario by name, live/ included: make test-one T=tests/live/compose.txt
test-one: $(PRODUCT).rom $(PRODUCT).sym
	@JZINTV_ARGS="$(if $(FUJINET),--fujinet=$(FUJINET))" \
	    $(PYTHON) tools/intv_screen.py $(PRODUCT).rom $(T)

clean:
	rm -f $(PRODUCT).asm $(PRODUCT).bin $(PRODUCT).cfg $(PRODUCT).rom \
	      $(PRODUCT).lst $(PRODUCT).sym t9dict.bas .dictstamp-* \
	      kbdiag.asm kbdiag.rom

.PHONY: all check run kbdiag test test-one clean

# The dictionary word list: a curated calendar vocabulary first, then the
# general frequency list behind it. Order is priority -- mkdict.py keeps the
# first DICT_COUNT survivors and breaks digit-string ties by position -- and at
# 650 entries that ordering is doing real work. A general top-650 has no
# "dentist", no "standup" and no "anniversary", which are exactly the words an
# event title is made of; it does have several hundred that never appear in
# one. Duplicates across the two files are harmless, mkdict.py drops them.
wordlist/gcal-t9.txt: wordlist/gcal-extra.txt wordlist/google-10000-english.txt
	cat $^ > $@
