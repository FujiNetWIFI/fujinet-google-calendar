# Makefile -- Google Calendar client for the Intellivision (IntyBASIC).
#
# Needs IntyBASIC v1.4.2 and jzintv's as1600. Only gcal.bas is handed to the
# compiler; every other .bas arrives through an INCLUDE and is listed here
# solely so a change to one retriggers the build.
#
# The .lst and .cfg are worth keeping an eye on. IntyBASIC will SILENTLY
# continue past the end of the $5000-$6FFF segment, and a cart that spills
# into $7000 (or auto-continues into $E000 past $D000) fails EXEC's boot
# detection -- the PC lands in unprogrammed GROM a couple of instructions in.
# `make check` diffs the segment list for exactly that; run it after any
# build that adds code.

INTYBASIC ?= intybasic
AS1600    ?= as1600
LIBDIR    ?= $(HOME)/Workspace/IntyBASIC/intybasic/

PRODUCT = gcal
SOURCES = gcal.bas constants.bas fujinet.bas clock.bas gcalnet.bas \
          screen.bas scroll.bas input.bas wrap.bas bar.bas gfx.bas \
          st_view.bas st_day.bas st_week.bas st_month.bas st_agenda.bas \
          st_event.bas st_pick.bas alarm.bas

all: $(PRODUCT).bin $(PRODUCT).rom

$(PRODUCT).asm: $(SOURCES)
	$(INTYBASIC) gcal.bas $(PRODUCT).asm $(LIBDIR)

# .bin + .cfg -- for loading from the cartridge SD.
$(PRODUCT).bin: $(PRODUCT).asm
	$(AS1600) -o $(PRODUCT).bin -l $(PRODUCT).lst $(PRODUCT).asm

# .rom -- Intellicart format, for jzIntv and network boot.
$(PRODUCT).rom: $(PRODUCT).asm
	$(AS1600) -o $(PRODUCT).rom $(PRODUCT).asm

# Fail the build if the linker placed code anywhere but the four segments this
# cart is known to boot from. Only the [mapping] section is checked -- the
# [memattr] lines that follow describe the FujiNet scratch RAM and legitimately
# name $8000/$9000.
#
# as1600 writes the .cfg with CRLF, so the target field carries a trailing \r
# and a naive string compare against "$5000" never matches. gsub strips it.
check: $(PRODUCT).bin
	@echo "--- $(PRODUCT).cfg ---"
	@cat $(PRODUCT).cfg
	@awk '/^\[mapping\]/ { m = 1; next } \
	      /^\[/          { m = 0 } \
	      m && NF >= 5   { seg = $$NF; gsub(/\r/, "", seg); \
	                       if (seg != "$$5000" && seg != "$$6000" && \
	                           seg != "$$D000" && seg != "$$F000") { \
	                         print "SEGMENT SPILL: " seg " is not a bootable area"; \
	                         bad = 1 } } \
	      END { if (bad) exit 1 }' $(PRODUCT).cfg \
	  && echo "segments OK: only \$$5000/\$$6000/\$$D000/\$$F000 in use"

run: $(PRODUCT).rom
	./run.sh

clean:
	rm -f $(PRODUCT).asm $(PRODUCT).bin $(PRODUCT).cfg $(PRODUCT).rom $(PRODUCT).lst

.PHONY: all check run clean
