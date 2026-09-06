PRODUCT = gcal
PLATFORMS += adam
PLATFORMS += apple2enh
PLATFORMS += atari
PLATFORMS += coco
PLATFORMS += msdos

# This list is the backends that exist in src/, not the platforms mekkogx can
# target. SRC_DIRS globs src/%PLATFORM% with $(wildcard), so a platform with no
# directory does not fail the glob -- it compiles the portable core alone and
# dies at the link with every plat_*/ui_* symbol unresolved. Three entries
# inherited from the template did exactly that and have been dropped:
#
#   apple2   the unenhanced machine. src/apple2enh/ is not a backend it could
#            share: screen.c drives 80STORE/HISCR and the alternate character
#            set, and the chip column is MouseText. See the README.
#   c64      PLATFORM_COMBOS expands it to src/commodore/, which does not exist
#   msxrom   likewise src/msx/
#
# Add the entry back in the same commit that adds the directory, so that
# `make` and `make release`, which build every name here, stay green.

# adam_cpm is deliberately absent. PLATFORM_COMBOS below expands it to src/adam/
# as well as src/adam_cpm/, so listing it would compile this backend's EOS and
# SmartKeys calls into a CP/M binary that links neither library. The combo stays
# because it is also what points fnlib.py at the adam archive.

# You can run 'make <platform>' to build for a specific platform,
# or 'make <platform>/<target>' for a platform-specific target.
# Example shortcuts:
#   make coco        → build for coco
#   make apple2enh/disk → build the 'disk' target for apple2enh

# SRC_DIRS may use the literal %PLATFORM% token.
# It expands to the chosen PLATFORM plus any of its combos.
SRC_DIRS = src src/%PLATFORM%

# FUJINET_LIB can be
# - a version number such as 4.7.6
# - a directory which contains the libs for each platform
# - a zip file with an archived fujinet-lib
# - a URL to a git repo
# - empty which will use whatever is the latest
# - undefined, no fujinet-lib will be used
FUJINET_LIB =

# The Adam runs at 32x24 on the TMS9918A's GRAPHICS II page, and the SmartKeys
# band owns the bottom three rows -- so the screen is 32 wide like the CoCo's
# and 21 rows tall, which is five more than the CoCo has and the most this
# program has ever had.
#
# LIST_ROWS 14 is rows 4-17; 18-19 are the panel that spells the selection out
# and 20 is the status row. MAX_EVENTS is left at the portable default of 64,
# which no other 32-column build can afford: this target links at $0000 in
# all-RAM mode and the ceiling is the boot block at $C800, so 51K of address
# space against the CoCo's 27K. The compose form's ~4.5K of code spent the
# old 3.8K of headroom and then some, so the form and picker overlays ride
# the detail buffer here too (see gcal.h), DET_ROWS comes down to 40 -- at
# 32 columns still two and a half detail pages -- and GC_RXBUF and LINE_CAP
# take the CoCo's cheap trades. Check __BSS_END_tail in r2r/adam/gcal.map
# against $C800 if you raise anything here.
#
# TITLE_LEN is 40 rather than the 23 the list column shows, for the same reason
# as on the CoCo: the panel is two rows of 32, and "09:00-10:00  " leaves 51 of
# those for the title.
CFLAGS_EXTRA_ADAM  = -DBUILD_ADAM -Os
CFLAGS_EXTRA_ADAM += -DLIST_ROWS=14 -DPICK_ROWS=12 -DDET_WIN=16
CFLAGS_EXTRA_ADAM += -DDET_COLS=32 -DDET_ROWS=40 -DTITLE_LEN=40
CFLAGS_EXTRA_ADAM += -DGC_FORM_OVERLAY -DGC_CALS_OVERLAY
CFLAGS_EXTRA_ADAM += -DGC_RXBUF=256 -DLINE_CAP=100

# fujinet-lib has no fn_clock for this bus at all -- not clock_get_time, which
# the CoCo does have, and not clock_get_tz, which it does not. src/adam/
# clock_adam.c supplies the first by asking the Fuji device directly; there is
# no equivalent for the second, so the settings screen shows the clock's own
# reading instead. See src/clock.c and src/adam/ui.c.
CFLAGS_EXTRA_ADAM += -DGC_NO_CLOCK_TZ

CFLAGS_EXTRA_ADAM += $(ADAM_SHOT_FLAGS)

# -m keeps the map file, which is the only way to see how close the link is to
# the $C800 boot block. z88dk writes it next to the executable.
LDFLAGS_EXTRA_ADAM = -m

# The Atari build reserves the top of memory for a 1K-aligned player/missile
# graphics buffer (see src/atari/pmg.c). 1056 rather than a round 1024: the
# region runs up to MEMTOP ($BC20), so $420 of reserve puts its floor at
# exactly $B800 -- the 1K boundary the buffer needs, with the buffer ending
# $20 under MEMTOP. It used to be a lazy 2048, which guaranteed an aligned
# 1K block anywhere but wasted 992 bytes; the compose form is what made
# those bytes worth taking back. pmg.c reads APPMHI rather than assuming a
# size, so it lands on $B800 either way.
#
# __STACKSIZE__ trims cc65's default 2K C stack to 1K. The call graph here
# is shallow -- blocking screens on the call stack, small locals, statics
# for everything big -- and the deepest path (view loop -> compose ->
# save -> fujinet-lib) does not approach 1K of parameter stack.
#
# Both have to go through -Wl: cl65 forwards a bare -D to the *compiler*,
# which never runs on a link-only invocation, so the flag would be silently
# dropped and no memory would actually be reserved.
LDFLAGS_EXTRA_ATARI  = -Wl -D,__RESERVED_MEMORY__=1056
LDFLAGS_EXTRA_ATARI += -Wl -D,__STACKSIZE__=1024
LDFLAGS_EXTRA_ATARI += --mapfile r2r/atari/gcal.map

# This is the tightest build of the five -- CODE, RODATA, DATA and BSS run
# contiguously from $2000 and the whole lot has to stay under the ceiling
# the linker derives ($B400 with the reserve and stack above) -- and the
# compose form costs ~5K of 6502 code, so it pays its way from every purse
# at once:
#
#   GC_FORM_OVERLAY   the form buffer on top of gc_det -- never alive
#                     together; see gcal.h
#   GC_CALS_OVERLAY   the picker's list behind it in the same RAM -- only
#                     alive inside do_pick()
#   MAX_EVENTS=48     ~910 bytes; the agenda then asks ?count=48
#   DET_ROWS=40       ~330 bytes; very long descriptions truncate sooner
#   GC_RXBUF=256      the CoCo's receive-buffer trade: costs round trips
#
# Keep the shape knobs in step with the default hosttest shape in
# tests/Makefile, and check the BSS line in r2r/atari/gcal.map after
# moving any of them.
CFLAGS_EXTRA_ATARI  = -DGC_FORM_OVERLAY -DGC_CALS_OVERLAY
CFLAGS_EXTRA_ATARI += -DMAX_EVENTS=48 -DDET_ROWS=40 -DGC_RXBUF=256
CFLAGS_EXTRA_ATARI += -DLINE_CAP=100

# tools/atari-shot.sh appends -DGC_FAKE_DATA / -DGC_FAKE_KEYS through here --
# this variable now carries the knobs above and a command-line assignment
# would replace the lot.
CFLAGS_EXTRA_ATARI += $(ATARI_SHOT_FLAGS)

# The Apple II runs at 80 columns, so it overrides the core's fixed widths:
# more title in the list column, a wider and correspondingly shorter detail
# buffer, and paragraph reflow, without which a description the adapter wrapped
# at 38 arrives as a ragged column down the left half of the screen.
# DET_LINE_CAP is the reflow accumulator -- it holds a rejoined paragraph now,
# not one wire line. Keep these in step with CFLAGS80 in tests/Makefile.
CFLAGS_EXTRA_APPLE2ENH  = -DTITLE_LEN=50 -DDET_COLS=78 -DDET_ROWS=32
CFLAGS_EXTRA_APPLE2ENH += -DDET_LINE_CAP=512 -DDET_REFLOW -DLIST_ROWS=18

# The picker window. It used to be 14 in src/apple2enh/ui.c and 12 in main.c,
# which is a scroll that advances a row early -- latent only because CAL_MAX is
# 10. PICK_ROWS now lives in gcal.h so there is one number, not two.
CFLAGS_EXTRA_APPLE2ENH += -DPICK_ROWS=14

# The category column off the wire, which needs somewhere to be shown and
# 960 bytes to be kept. The Atari has neither.
CFLAGS_EXTRA_APPLE2ENH += -DGC_KEEP_CAT

# The compose form's field capacities, widened because the 62-column field
# windows can show what they store and the map says the RAM is there.
CFLAGS_EXTRA_APPLE2ENH += -DFRM_LOC_MAX=50 -DFRM_DESC_MAX=160

# The enhanced //e is a 65C02 and cc65 generates smaller code for one, but
# cc65.mk appends its own --cpu 6502 *after* CFLAGS_EXTRA and the last --cpu
# wins, so asking for 65c02 here would be a flag that quietly does nothing.
# Making it work means a change to the shared mekkogx template, and the map
# file says there is no need: 6K spare with the ceiling below.

# apple2enh.cfg presumes RAM ends at $9600, which leaves room for ProDOS file
# buffers this client never opens -- fujinet-lib talks SmartPort directly, and
# nothing here touches the filesystem. $BF00 is the ProDOS global page, so
# that is the real ceiling, and the 10.5K between the two is what the wider
# screen's buffers are paid for out of.
LDFLAGS_EXTRA_APPLE2ENH  = -Wl -D,__HIMEM__=0xBF00
LDFLAGS_EXTRA_APPLE2ENH += --mapfile r2r/apple2enh/gcal.map

# The CoCo 1/2 runs at 32x16 on the VDG's semigraphics page, which is the
# narrowest and shortest screen this client has -- and one of the few that can
# show an event's real Google color. The CoCo 3 build is a second binary from
# the same source at 80x24 on the GIME, where the color is an attribute rather
# than a semigraphics cell; its knobs are in the MAKE_COCO3 branch below.
#
# LIST_ROWS 11 is rows 2-12; rows 13-14 are the panel that spells the selection
# out, which earns its keep harder here than on the Atari because the list's
# title column is 23 columns rather than 30.
#
# The RAM knobs below all came down a notch when the compose form arrived --
# it costs ~4K of 6809 code and this is a 28K machine that was down to its
# last kilobyte and a half. Check r2r/coco/gcal.map against the $7F00
# ceiling if you move any of them. MAX_EVENTS 24 is still two DAY screens
# and what ?count= asks the adapter for; DET_ROWS 22 is just under two
# detail pages, and cannot go below 22 -- the picker's list is overlaid on
# the buffer it sizes (see GC_CALS_OVERLAY in gcal.h) and needs 720 of its
# bytes. TITLE_LEN 36 shows 35 title characters in the two-row panel where
# 40 showed 39 ("09:00-10:00  " takes 13 of the panel's 64). GC_RXBUF 128
# only costs round trips -- every reader drains through split_lines(),
# which is indifferent to chunk boundaries.
#
# The CoCo 3 is a second binary rather than a runtime branch inside this one.
# The two machines never share a screen -- a 1/2 is always the VDG's 32x16 and a
# 3 is always the GIME's 80x24 -- so a combined image would carry two backends,
# two marks and two layouts through a ceiling the 1/2 has no room in.
#
# The 80-column layout is the MS-DOS backend's, not this one widened:
# src/coco/views3.c and ui3.c are that backend's painters, which name attribute
# roles rather than an inverse flag. The knobs below are its 80-column arm.
#
# They do not come down the way the 1/2's did for the compose form, because the
# objects that scale with the width are not in the 6809's 64K at all: the
# wrapped detail text and the event titles live in the second bank, see
# src/coco/far.c. The overlays below are still set, but they borrow gc_scratch
# rather than the gc_det this build does not have -- see the note in src/gcal.h.
#
ifeq ($(MAKE_COCO3),COCO3)

CFLAGS_EXTRA_COCO  = -DCOCO3
CFLAGS_EXTRA_COCO += -DLIST_ROWS=18 -DPICK_ROWS=14 -DDET_WIN=20
CFLAGS_EXTRA_COCO += -DDET_COLS=78 -DDET_ROWS=32
CFLAGS_EXTRA_COCO += -DMAX_EVENTS=24 -DTITLE_LEN=40
CFLAGS_EXTRA_COCO += -DGC_RXBUF=96 -DLINE_CAP=96
CFLAGS_EXTRA_COCO += -DGC_FORM_OVERLAY -DGC_CALS_OVERLAY

# The compose form is GCALED3.BIN, a second program -- see src/coco/chain.c.
# GC_CHAIN_EDIT builds the client that hands over to it; MAKE_EDITOR builds the
# editor from the same tree, which is why the two defines are exclusive: the
# editor wants the real form and its own main(), the client wants neither.
ifeq ($(MAKE_EDITOR),1)
CFLAGS_EXTRA_COCO += -DGC_EDITOR
else
CFLAGS_EXTRA_COCO += -DGC_CHAIN_EDIT
endif

else

CFLAGS_EXTRA_COCO  = -DLIST_ROWS=11 -DPICK_ROWS=10 -DDET_WIN=12
CFLAGS_EXTRA_COCO += -DDET_COLS=32 -DDET_ROWS=22
CFLAGS_EXTRA_COCO += -DMAX_EVENTS=24 -DTITLE_LEN=36
CFLAGS_EXTRA_COCO += -DGC_RXBUF=96

# The compose form's buffer and the picker's list both ride on top of the
# detail rows (see gcal.h) -- the 27K ceiling had no room for the form's
# code, let alone more statics, so every borrowable buffer is borrowed and
# LINE_CAP comes down to what a width-80 line can actually need. Keep in
# step with CFLAGS32 in tests/Makefile.
CFLAGS_EXTRA_COCO += -DGC_FORM_OVERLAY -DGC_CALS_OVERLAY -DLINE_CAP=96

endif

# fujinet-lib declares clock_get_tz for every platform but only builds it for
# some: the CoCo archive carries fn_clock/clock_get_time.o and nothing else, so
# calling it is an undefined symbol at link. See src/clock.c and src/coco/ui.c.
CFLAGS_EXTRA_COCO += -DGC_NO_CLOCK_TZ

CFLAGS_EXTRA_COCO += -fomit-frame-pointer

# CMOC declares the string and memory functions in <cmoc.h> and ships no
# <string.h> at all. src/coco/include/ holds a shim so the portable half can go
# on including it the way ordinary C89 does; the directory has no .c files, so
# the source glob steps over it.
CFLAGS_EXTRA_COCO += -Isrc/coco/include

# tools/coco-shot.sh appends -DGC_FAKE_DATA / -DGC_FAKE_KEYS through here. It
# cannot set CFLAGS_EXTRA_COCO on the command line the way tools/atari-shot.sh
# does, because on this platform that variable carries every screen-shape knob
# above and a command-line assignment would replace the lot.
CFLAGS_EXTRA_COCO += $(COCO_SHOT_FLAGS)

# With Disk BASIC present a BASIC program lives at $0E00, so the one thing the
# org must not do is collide with the AUTOEXEC that is running LOADM. $0E80
# leaves that program 128 bytes -- it is one line, about 25 tokenised bytes,
# with no variables. The org was $1000 until the compose form needed the
# difference.
#
# Other CoCo clients in this family (fujinet-news, fujinet-config) org at $0E00
# and pay for it with a second-stage loader that pokes BASIC's direct-mode
# buffer and jumps into RUNM. That trick is ROM-version sensitive -- it gives
# ?UL ERROR on stock Disk BASIC 1.1 -- and 256 bytes is a cheaper price than a
# whole extra binary with its own file-type trap.
#
# The stack is placed explicitly rather than inherited from BASIC. LOADM
# leaves S wherever CLEAR put it, which moves if anyone edits the AUTOEXEC;
# ours does not. It lives at $0D00, growing down toward the text screen at
# $0400-$05FF -- 1.7K of headroom for a program whose call depth never
# nears it. That region is Disk BASIC's buffer/graphics-page space, which
# only matters while LOADM itself is running: by the time EXEC hands over
# and the start code loads S, the disk is done, FujiNet I/O is the
# bit-banger, and BASIC's own 60Hz IRQ pushes wherever S points. Moving
# the stack down is what let --limit rise to $7F00: the whole 768-byte gap
# the stack used to need at the top now holds program instead, and $7F00
# still leaves BASIC a page to come back to after EXEC returns.
#
# --limit is what turns "silently corrupts the top of memory" into a build
# failure, so it goes in from the first link rather than after the first
# mystery. -i keeps the .map, which tools/coco-shot.sh reads its
# breakpoint symbol out of.
LDFLAGS_EXTRA_COCO  = --org=0E80 --limit=7F00 --initial-s=0D00
LDFLAGS_EXTRA_COCO += --no-relocate -i

# `make coco` puts GCAL.BIN on a disk by itself, started by hand with
#
#   LOADM"GCAL":EXEC
#
# `make coco-dist` builds the combined CoCo 1/2 + CoCo 3 disk instead, which
# carries an AUTOEXEC.BAS, a loader as GCAL.BIN, and the two clients as
# GCAL1.BIN and GCAL3.BIN. See the recipe at the foot of this file.
#
# That AUTOEXEC is RUNM"GCAL" -- an HDB-DOS command -- and it has to be RUNM
# rather than LOADM. decb's -t tokenizer does not know LOADM: it matches LOAD
# greedily and leaves the M as text, so the line comes back as LOAD M"GCAL" and
# RUN answers ?SN ERROR. Storing it as ASCII with -a -l does not help either --
# BASIC tokenizes that correctly on the way in and then ?SN ERRORs anyway,
# because Disk BASIC runs an ASCII program out of the disk buffer that LOADM
# itself needs. Disk I/O from an ASCII-loaded program does not work.
#
# The loader itself is the second stage fujinet-news and fujinet-config use --
# poke BASIC's direct-mode buffer and jump into RUNM -- which is what lets one
# disk carry both models. support/coco/loader.c is it.

# The MS-DOS build (Open Watcom wcc, 8086, small model) is the one target
# that does not know its own width until it is running: a PC inherits
# whatever text mode it booted in, 40 columns in modes 0/1, 80 in 2/3, and
# the MDA's mode 7. So the width knobs here size *storage* for the widest
# case and GC_RT_COLS makes the wrap width a runtime variable -- gc_det
# stays 48 rows of 79-byte stride while gc_wrap_cols wraps at 38 or 78.
# Wrapping narrower than the stride is safe; the reverse would be an
# overrun, which is why DET_STRIDE itself stays derived. Rows are still
# compile-time -- every mode this backend runs in is 25 of them, one more
# than the Apple, spent on a two-row detail panel (LIST_ROWS 18 + panel
# rows 22-23), a taller detail window and a month summary with a blank
# above it. Keep the shape knobs in step with CFLAGSDOS in tests/Makefile.
#
# DET_ROWS is 48 rather than the Apple's 32 because the same description
# needs roughly twice the rows when the runtime wrap is 38 -- 48x38 is the
# Atari's capacity, 48x78 is three-plus pages at 80. DET_REFLOW is on
# because it is required at 78 and harmless at 38. GC_KEEP_CAT is stored
# unconditionally (960 bytes, cheap here) and *rendered* only at 80
# columns, where ui_geom() gives it a column. GC_RXBUF is 1024 because
# every network_read is a whole INT F5 / RS-232 round trip.
#
# -os because wcc's default is NO optimisation at all. And wcc cannot carry
# a comma through -D -- everything after one parses as another source file
# (E1139) -- which is why tools/msdos-shot.sh sends scripted keys as
# GC_FAKE_KEYS_STR, a string with no commas in it.
#
# The budget is DGROUP -- one 64K group for all statics plus the stack --
# not address space. The arithmetic: gc_index 64 x 80 = 5,120 with the
# category kept, gc_det 48 x 79 = 3,792, rxbuf 1,024, gc_cals 720, agenda
# and assorted buffers ~1,000; call it 12K of ours plus the Watcom RTL and
# fujinet-lib statics and the 4K stack, comfortably under 20K of the 64K.
# Check DGROUP in r2r/msdos/gcal.map before raising MAX_EVENTS or DET_ROWS,
# and check the -DGC_FAKE_DATA build too -- it links the canned wire data
# alongside the real transport and runs out first.
CFLAGS_EXTRA_MSDOS  = -os -DGC_RT_COLS
CFLAGS_EXTRA_MSDOS += -DTITLE_LEN=50 -DDET_COLS=78 -DDET_ROWS=48
CFLAGS_EXTRA_MSDOS += -DDET_LINE_CAP=512 -DDET_REFLOW -DGC_KEEP_CAT
CFLAGS_EXTRA_MSDOS += -DLIST_ROWS=18 -DPICK_ROWS=14 -DDET_WIN=20
CFLAGS_EXTRA_MSDOS += -DGC_RXBUF=1024

# The compose form's field capacities, widened like the Apple's -- DGROUP
# has the room and the 80-column layout has the columns.
CFLAGS_EXTRA_MSDOS += -DFRM_LOC_MAX=50 -DFRM_DESC_MAX=160

# tools/msdos-shot.sh appends -DGC_FAKE_DATA / -DGC_SHOT / -DGC_FAKE_KEYS_STR
# through here -- this variable carries every screen-shape knob above and a
# command-line assignment would replace the lot.
CFLAGS_EXTRA_MSDOS += $(MSDOS_SHOT_FLAGS)

LDFLAGS_EXTRA_MSDOS  = OPTION map=r2r/msdos/gcal.map
LDFLAGS_EXTRA_MSDOS += OPTION stack=4096

# Build with `defoogi make msdos`: wcc lives only in the defoogi container.
# Leave FUJINET_LIB empty (the default above) rather than pointing it at a
# host checkout -- the container mounts the project directory and nothing
# else, so an absolute host path is invisible inside it, and the empty
# variable makes fnlib.py download the msdos release archive into the
# project's own _cache/ instead.

# HIRESTXT_LIB can be
# - a version number such as 0.5.0.2
# - a directory which contains the built library
# - a URL to a git repo
# - empty which will use whatever is the latest
# - undefined, no hirestxt-mod will be used
# Only used for coco/dragon builds.
#HIRESTXT_LIB =

# Define extra dirs ("combos") that expand with a platform.
# Format: platform+=combo1,combo2
PLATFORM_COMBOS = \
  c64+=commodore \
  atarixe+=atari \
  msxrom+=msx \
  msxdos+=msx \
  adam_cpm+=adam

include mekkogx/toplevel-rules.mk

# If you need to add extra platform-specific steps, do it below:
#   coco/r2r:: coco/custom-step1
#   coco/r2r:: coco/custom-step2
# or
#   apple2enh/disk: apple2enh/custom-step1 apple2enh/custom-step2

#################################################################
## COCO 1/2 + COCO 3 COMBINED DISK                             ##
#################################################################

COCO_R2R  = r2r/coco/$(PRODUCT)
COCO_DISK = $(COCO_R2R).dsk

# The CoCo 3 variant, through the same rules as the 1/2.
coco3:
	$(MAKE) coco MAKE_COCO3=COCO3

# The CoCo 3's compose form, built from the same tree with its own main().
coco-edit:
	$(MAKE) coco MAKE_COCO3=COCO3 MAKE_EDITOR=1

#
# One disk that runs on either machine: GCAL1.BIN for the VDG, GCAL3.BIN for
# the GIME, and GCAL.BIN a loader that reads the model and RUNMs the right one
# -- fujinet-fujirkle's pattern, and support/coco/loader.c is its loader.
# AUTOEXEC.BAS starts the loader, so the disk boots straight into the client.
#
# The object tree has to go between the two builds: make keys off timestamps
# rather than flags, so without this the second variant would silently relink
# the first one's objects.
#
coco-dist:
	$(MAKE) clean
	rm -rf build
	$(MAKE) coco
	mv $(COCO_R2R).bin $(COCO_R2R)1.bin

	rm -rf build
	$(MAKE) coco3
	mv $(COCO_R2R).bin $(COCO_R2R)3.bin

	rm -rf build
	$(MAKE) coco-edit
	mv $(COCO_R2R).bin $(COCO_R2R)e3.bin

	cmoc -o $(COCO_R2R).bin support/coco/loader.c

	$(RM) $(COCO_DISK)
	decb dskini $(COCO_DISK)
	mkdir -p build/coco
	echo RUNM\"GCAL\" > build/coco/autoexec.bas
	decb copy -t -0 build/coco/autoexec.bas $(COCO_DISK),AUTOEXEC.BAS
	decb copy -b -2 $(COCO_R2R).bin  $(COCO_DISK),GCAL.BIN
	decb copy -b -2 $(COCO_R2R)1.bin $(COCO_DISK),GCAL1.BIN
	decb copy -b -2 $(COCO_R2R)3.bin $(COCO_DISK),GCAL3.BIN
	decb copy -b -2 $(COCO_R2R)e3.bin $(COCO_DISK),GCALED3.BIN
	decb dir $(COCO_DISK)

# The MS-DOS disk is a bootless 360K FAT image (mformat lays no system
# tracks -- SYS A: it from a DOS disk) carrying GCAL.EXE and the FujiNet
# driver set. The drivers are built from the fujinet-msdos repo as named
# parts rather than through that repo's own `disk` target, because fmall
# and freset grew nasm dependencies and defoogi ships wasm, not nasm. The
# clone and sub-make run inside the same defoogi invocation as the build --
# the container has both the toolchain and the network.
#
# CONFIG.SYS is this project's own (src/msdos/CONFIG.SYS) rather than the
# clone's verbatim copy the gmail disk uses, for one line: FUJI_PORT=2. See
# the comment in that file. mcopy -t converts the two text files to CRLF;
# AUTOEXEC.BAT goes last so nothing shadows starting GCAL.
FUJINET_MSDOS_REPO = https://github.com/FujiNetWIFI/fujinet-msdos.git
FUJINET_MSDOS_CACHE = $(CACHE_DIR)/fujinet-msdos
FN_MSDOS_PARTS = sys/fujinet.sys printer/fujiprn.sys fconfig/fconfig.com

msdos/disk-post::
	@if [ ! -d $(FUJINET_MSDOS_CACHE) ]; then \
	  git clone $(FUJINET_MSDOS_REPO) $(FUJINET_MSDOS_CACHE); \
	fi
	$(MAKE) -C $(FUJINET_MSDOS_CACHE) $(FN_MSDOS_PARTS)
	mcopy -o -i $(DISK) $(addprefix $(FUJINET_MSDOS_CACHE)/,$(FN_MSDOS_PARTS)) '::/'
	mcopy -t -o -i $(DISK) src/msdos/CONFIG.SYS '::/CONFIG.SYS'
	mcopy -t -o -i $(DISK) src/msdos/AUTOEXEC.BAT '::/AUTOEXEC.BAT'
