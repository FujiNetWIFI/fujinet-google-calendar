# Host-side tests for the portable core.
#
# Everything listed here depends on nothing but the C library, so it builds
# natively. The rest of src/ needs cc65 and a FujiNet, and is exercised through
# the emulator instead -- see tools/atari-shot.sh and tools/apple2-shot.sh.
#
# Two binaries, because the core's fixed-width constants are overridable and
# the backends do override them. hosttest is the Atari's shape and hosttest80
# the Apple II's, which is the only way the reflow path gets covered at all --
# it compiles out entirely without DET_REFLOW.

CC      ?= cc
CFLAGS  ?= -O1 -g -Wall -Wextra -Wno-unused-parameter

# Keep in step with CFLAGS_EXTRA_APPLE2ENH in the top-level Makefile.
CFLAGS80 = -DTITLE_LEN=50 -DDET_COLS=78 -DDET_ROWS=32 -DDET_LINE_CAP=512 \
           -DDET_REFLOW -DGC_KEEP_CAT

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

all: hosttest hosttest80
	./hosttest
	./hosttest80

hosttest: $(SRC) ../src/gcal.h
	$(CC) $(CFLAGS) -o $@ $(SRC)

hosttest80: $(SRC) ../src/gcal.h
	$(CC) $(CFLAGS) $(CFLAGS80) -o $@ $(SRC)

clean:
	rm -f hosttest hosttest80

.PHONY: all clean
