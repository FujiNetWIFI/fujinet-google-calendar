/*
 * Event detail ingest.
 *
 * The raw reply is never stored. Each line coming off the wire is accumulated,
 * sanitized and folded into fixed-width display rows on arrival, and the
 * original is discarded -- which is what keeps the buffer to a size we can
 * actually budget for.
 *
 * NetworkProtocolCalendar composes the detail itself and has already applied
 * lineEnding and its own wrap, at 38 columns on real Atari firmware
 * (Calendar.cpp's BUILD_ATARI default) and 80 on a fujinet-pc RS232 build. We
 * re-wrap regardless: 38 passes through untouched, 80 splits cleanly, and
 * neither reflows across the adapter's own line breaks, so its paragraph
 * structure survives.
 *
 * The first two lines are dropped, because they are the summary and the when
 * line and the event screen already prints both from the index record.
 *
 * This file is deliberately free of any platform or network dependency so the
 * awkward parts (line-ending soup, the accumulator overflow, truncation) can
 * be exercised by tests/hosttest.c on a normal machine.
 */

#include <string.h>

#include "gcal.h"

char          gc_det[DET_ROWS][DET_STRIDE];
unsigned int  gc_det_rows;
unsigned char gc_det_trunc;

static char          linebuf[DET_LINE_CAP + 1];
static unsigned int  line_len;
static unsigned char pending_lf;        /* saw CR, swallow a following LF */
static unsigned char high_run;          /* inside a run of non-ASCII bytes */
static unsigned char skipped;           /* leading lines dropped so far */

#define DET_SKIP    2

#ifdef DET_REFLOW
/*
 * Paragraph reflow.
 *
 * Not re-flowing across the adapter's own line breaks is right at 40 columns,
 * where its 38 is near enough. It is wrong at 78: a description wrapped at 38
 * arrives as a ragged column down the left half of the screen.
 *
 * So the adapter's wrap is undone before ours is applied. Its breaks are
 * recoverable because format_detail() is the only thing that made them, and
 * append_wrapped() flushes the moment a line reaches `width` -- so every line
 * it emits is shorter than the wrap width, and a *noticeably* short one can
 * only be the last of something, never the middle. That inverts into the rule
 * below: a full-length line is continued, a short one ends the paragraph.
 *
 * `widest` is the wrap width estimated from the reply itself, which is what
 * lets one rule serve both the 38 of real Atari firmware and the 80 of a
 * fujinet-pc RS232 build. Until it has seen a line long enough to be worth
 * trusting, nothing is joined at all.
 */
#define REFLOW_MIN  24          /* no estimate worth having below this */

static unsigned int  seg_start;         /* line_len where this wire line began */
static unsigned int  widest;            /* longest kept wire line so far */
#endif

void detail_reset(void)
{
    gc_det_rows = 0;
    gc_det_trunc = 0;
    line_len = 0;
    pending_lf = 0;
    high_run = 0;
    skipped = 0;
#ifdef DET_REFLOW
    seg_start = 0;
    widest = 0;
#endif
}

/* Wrap whatever has accumulated into display rows and start a fresh line. */
static void flush_line(void)
{
    unsigned int avail;

    linebuf[line_len] = '\0';
    line_len = 0;
    high_run = 0;
#ifdef DET_REFLOW
    seg_start = 0;
#endif

    if (skipped < DET_SKIP) {
        skipped++;
        return;
    }

    if (gc_det_rows >= DET_ROWS) {
        gc_det_trunc = 1;
        return;
    }

    avail = DET_ROWS - gc_det_rows;
    gc_det_rows += wrap_text(linebuf, gc_det[gc_det_rows],
                             avail, DET_COLS, DET_STRIDE);

    if (gc_det_rows >= DET_ROWS)
        gc_det_trunc = 1;
}

/*
 * The accumulator filled up before the line ended. Flush what we have, but
 * break at the last space so a word is not sliced in half, and carry the tail
 * forward into the next chunk. Only a single word longer than the whole
 * accumulator falls back to a hard split, which the wrapper handles anyway.
 */
static void flush_overflow(void)
{
    unsigned int brk = line_len;
    unsigned int tail;
    char save;

    while (brk > 0 && linebuf[brk - 1] != ' ')
        brk--;

    if (brk == 0) {
        flush_line();
        return;
    }

    tail = line_len - brk;
    save = linebuf[brk];        /* flush_line is about to NUL this */
    line_len = brk;
    flush_line();
    linebuf[brk] = save;

    memmove(linebuf, linebuf + brk, tail);
    line_len = tail;
#ifdef DET_REFLOW
    /* flush_line() zeroed seg_start, but the tail it left behind is still the
       middle of a wire line, so put the mark back where that line now starts.
       Getting this wrong would make the next segment measure as the tail's
       length and reflow would break the paragraph at an arbitrary point. */
    seg_start = (seg_start > brk) ? seg_start - brk : 0;
#endif
}

#ifdef DET_REFLOW

static unsigned char starts_with(const char *pfx)
{
    unsigned int i;

    for (i = 0; pfx[i]; i++)
        if (i >= line_len || linebuf[i] != pfx[i])
            return 0;

    return 1;
}

/*
 * The three items format_detail() emits between the when line and the
 * description. Each is a field in its own right, so however long one runs it
 * must never swallow the one after it -- which is the single case the
 * length rule below cannot see coming.
 */
static unsigned char starts_header(void)
{
    return (unsigned char) (starts_with("Category: ") ||
                            starts_with("Where: ") ||
                            starts_with("Repeats"));
}

/*
 * End of a wire line. Returns 1 if it was absorbed into the paragraph being
 * built, 0 if the caller should flush as usual.
 */
static unsigned char reflow_eol(void)
{
    unsigned int seg_len = line_len - seg_start;

    /* A blank line ends the paragraph *and* is one itself. Emit the pending
       text here, then let the caller flush the now-empty buffer, which is what
       puts the blank row between two paragraphs. */
    if (seg_len == 0) {
        if (line_len)
            flush_line();
        seg_start = 0;
        return 0;
    }

    /* The summary and the when line are about to be dropped and must be
       counted separately. Joining them would drop one line instead of two and
       take the Category line down with it -- and the summary is the one line
       format_detail() emits unwrapped, so it would poison the estimate too. */
    if (skipped < DET_SKIP) {
        seg_start = 0;
        return 0;
    }

    if (seg_len > widest)
        widest = seg_len;

    if (widest < REFLOW_MIN || starts_header() ||
        seg_len < widest - (widest >> 2)) {
        seg_start = 0;
        return 0;
    }

    /* Continue: the adapter's break becomes the space it broke on. */
    if (linebuf[line_len - 1] != ' ') {
        if (line_len >= DET_LINE_CAP)
            flush_overflow();
        linebuf[line_len++] = ' ';
    }
    seg_start = line_len;

    return 1;
}

#endif /* DET_REFLOW */

/*
 * Protocol.h's lineEnding is "\x9B", so ATASCII EOL is what actually arrives.
 * Accepting CR and LF as well costs nothing and makes the ingest correct
 * whichever the firmware sends -- a Google description can carry bare LFs of
 * its own regardless.
 *
 * The end-of-line test has to come before the charset rules below, or $9B
 * would be collapsed to '?' as a high byte and the whole reply would arrive as
 * one enormous line.
 */
void detail_ingest(const unsigned char *p, unsigned int n)
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
#ifdef DET_REFLOW
            if (reflow_eol()) {
                if (gc_det_trunc)
                    return;
                continue;
            }
#endif
            flush_line();
            if (gc_det_trunc)
                return;
            continue;
        }

        /* Same charset policy as copy_san(). */
        if (c > 126) {
            if (high_run)
                continue;
            high_run = 1;
            c = '?';
        } else {
            high_run = 0;
            if (c < 32)
                c = ' ';
        }

        if (line_len >= DET_LINE_CAP) {
            flush_overflow();
            if (gc_det_trunc)
                return;
        }
        linebuf[line_len++] = (char) c;
    }
}

/* Flush a reply that did not end with a line terminator. */
void detail_finish(void)
{
    if (line_len)
        flush_line();
}
