# Host-side tests for the portable core.
#
# Everything listed here depends on nothing but the C library, so it builds
# natively. The rest of src/ needs cc65 and a FujiNet, and is exercised through
# the emulator instead -- see tools/atari-shot.sh and tools/apple2-shot.sh.
#
# Four binaries, because the core's fixed-width constants are overridable and
# every backend overrides them differently. hosttest is the Atari's shape,
# hosttest80 the Apple II's -- which is the only way the reflow path gets
# covered at all, since it compiles out entirely without DET_REFLOW --
# hosttest32 the CoCo's, and hosttestadam the Adam's.

CC      ?= cc
CFLAGS  ?= -O1 -g -Wall -Wextra -Wno-unused-parameter

# Keep in step with CFLAGS_EXTRA_APPLE2ENH in the top-level Makefile.
CFLAGS80 = -DTITLE_LEN=50 -DDET_COLS=78 -DDET_ROWS=32 -DDET_LINE_CAP=512 \
           -DDET_REFLOW -DGC_KEEP_CAT

# Keep in step with CFLAGS_EXTRA_COCO. This is the only shape that exercises
# MAX_EVENTS at anything but 64, which is what the truncation and agenda
# overflow assertions are actually about -- and the only one where AGD_MAX is
# derived rather than the literal 128 both the others happen to get.
CFLAGS32 = -DMAX_EVENTS=32 -DTITLE_LEN=40 -DDET_COLS=32 -DDET_ROWS=24

# Keep in step with CFLAGS_EXTRA_ADAM. The Adam is the only backend that pairs
# a 32-column detail with the default 64 events and a 48-row buffer: the CoCo
# is 32 columns but pays for it with half the events and half the rows, so this
# is the one shape where the widest index and the narrowest wrap meet.
CFLAGSADAM = -DTITLE_LEN=40 -DDET_COLS=32 -DDET_ROWS=48

SRC = hosttest.c \
      ../src/wrap.c \
      ../src/sanitize.c \
      ../src/detail.c \
      ../src/date.c \
      ../src/color.c \
      ../src/index.c \
      ../src/lines.c \
      ../src/agenda.c \
      ../src/alarm.c \
      ../src/model.c

all: hosttest hosttest80 hosttest32 hosttestadam
	./hosttest
	./hosttest80
	./hosttest32
	./hosttestadam

hosttest: $(SRC) ../src/gcal.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

hosttest80: $(SRC) ../src/gcal.h
	$(CC) $(CFLAGS) $(CFLAGS80) -o $@ $(SRC)

hosttest32: $(SRC) ../src/gcal.h
	$(CC) $(CFLAGS) $(CFLAGS32) -o $@ $(SRC)

hosttestadam: $(SRC) ../src/gcal.h
	$(CC) $(CFLAGS) $(CFLAGSADAM) -o $@ $(SRC)

clean:
	rm -f hosttest hosttest80 hosttest32 hosttestadam

.PHONY: all clean
