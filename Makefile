PRODUCT = gcal
PLATFORMS += adam
PLATFORMS += adam_cpm
PLATFORMS += apple2
PLATFORMS += apple2enh
PLATFORMS += atari
PLATFORMS += c64
PLATFORMS += coco
PLATFORMS += msdos
PLATFORMS += msxrom

# You can run 'make <platform>' to build for a specific platform,
# or 'make <platform>/<target>' for a platform-specific target.
# Example shortcuts:
#   make coco        → build for coco
#   make apple2/disk → build the 'disk' target for apple2

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

# The Atari build carves 2K off the top of memory so we can place a 1K-aligned
# player/missile graphics buffer above the C stack. See src/atari/pmg.c.
#
# It has to go through -Wl: cl65 forwards a bare -D to the *compiler*, which
# never runs on a link-only invocation, so the flag would be silently dropped
# and no memory would actually be reserved.
LDFLAGS_EXTRA_ATARI  = -Wl -D,__RESERVED_MEMORY__=2048
LDFLAGS_EXTRA_ATARI += --mapfile r2r/atari/gcal.map

# The Apple II runs at 80 columns, so it overrides the core's fixed widths:
# more title in the list column, a wider and correspondingly shorter detail
# buffer, and paragraph reflow, without which a description the adapter wrapped
# at 38 arrives as a ragged column down the left half of the screen.
# DET_LINE_CAP is the reflow accumulator -- it holds a rejoined paragraph now,
# not one wire line. Keep these in step with CFLAGS80 in tests/Makefile.
CFLAGS_EXTRA_APPLE2ENH  = -DTITLE_LEN=50 -DDET_COLS=78 -DDET_ROWS=32
CFLAGS_EXTRA_APPLE2ENH += -DDET_LINE_CAP=512 -DDET_REFLOW -DLIST_ROWS=18

# The category column off the wire, which needs somewhere to be shown and
# 960 bytes to be kept. The Atari has neither.
CFLAGS_EXTRA_APPLE2ENH += -DGC_KEEP_CAT

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
#   apple2/disk: apple2/custom-step1 apple2/custom-step2
