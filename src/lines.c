/*
 * Splitting a byte stream into lines.
 *
 * Shared by the listing parser and the calendar list, and kept out of net.c so
 * that tests/hosttest.c can feed it wire bytes without dragging in
 * fujinet-lib and a 6502. That is not incidental: the reason this file exists
 * is a bug the Atari could never have shown.
 *
 * Protocol.h's lineEnding is per bus. The Atari's sio/network.cpp leaves it at
 * the default "\x9B", but iwm/network.cpp -- the Apple II's -- sets "\x0D",
 * and drivewire and adamnet do the same. The first version of this treated CR
 * as the leading half of a CRLF and simply dropped it, which is correct for a
 * stream terminated by something else and catastrophic for one terminated by
 * CR: every line of a listing ran into the next, so the window title absorbed
 * the header row and not one event was parsed.
 *
 * So all three terminators end a line, and a CR only swallows the LF that
 * immediately follows it. detail.c has always done it this way.
 *
 * A line longer than the accumulator is clipped rather than split, because the
 * only thing past column 80 in a width-80 listing is title text the display
 * would truncate regardless.
 */

#include "gcal.h"

static char          linebuf[LINE_CAP + 1];
static unsigned char hold;
static unsigned char pending_lf;        /* saw CR, swallow a following LF */

void split_reset(void)
{
    hold = 0;
    pending_lf = 0;
}

void split_lines(const unsigned char *p, unsigned int n, line_fn emit)
{
    unsigned char c;

    while (n--) {
        c = *p++;

        if (pending_lf) {
            pending_lf = 0;
            if (c == 0x0A)
                continue;               /* the LF half of a CRLF */
        }

        if (c == 0x9B || c == 0x0D || c == 0x0A) {
            if (c == 0x0D)
                pending_lf = 1;
            linebuf[hold] = '\0';
            emit(linebuf, hold);
            hold = 0;
            continue;
        }

        if (hold < LINE_CAP)
            linebuf[hold++] = (char) c;
    }
}

/* Flush a reply whose last line had no terminator. */
void split_finish(line_fn emit)
{
    if (hold) {
        linebuf[hold] = '\0';
        emit(linebuf, hold);
        hold = 0;
    }
}
