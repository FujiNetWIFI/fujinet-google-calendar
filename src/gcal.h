/*
 * FujiNet Google Calendar client -- shared types and constants.
 *
 * Ported from the IntyBASIC original in intv/. Everything in src/ is meant to
 * stay portable across MekkoGX platforms; anything that touches a specific
 * machine lives in src/<platform>/ behind the ui_* / plat_* interface below.
 */

#ifndef GCAL_H
#define GCAL_H

/*
 * CMOC has no <stdint.h>. Nothing in this header needs one either: the only
 * uintN_t in the portable half is in settings.c, which includes
 * <fujinet-fuji.h> first -- and that header #defines uint8_t and uint16_t
 * itself under _CMOC_VERSION_.
 */
#ifndef _CMOC_VERSION_
#include <stdint.h>
#endif

/* ------------------------------------------------------------------ */
/* Sizing                                                              */
/* ------------------------------------------------------------------ */

/*
 * Events kept for one window, and how much title we keep for each.
 *
 * These are the RAM knobs, and they are why the numbers here are not the
 * Intellivision's. The link fails loudly when they are too large -- ld65
 * reports the BSS overflow in bytes -- so if you raise one, check the BSS line
 * in r2r/atari/gcal.map against the $B400 ceiling afterwards (MEMTOP minus
 * the P/M reserve and the C stack; see LDFLAGS_EXTRA_ATARI).
 *
 * Measure from clean when you change one. `make atari` keys off timestamps
 * rather than flags, so a build left over from tools/atari-shot.sh relinks its
 * own objects and reports a BSS figure that is not the shipping one -- the
 * GC_FAKE_DATA build compiles out the URL, receive and appkey buffers, which
 * is nearly a kilobyte of the difference.
 *
 * 64 is ample for a DAY, comfortable for a WEEK, and exactly what AGENDA asks
 * the adapter for.
 *
 * The adapter caps a window at 300 events regardless of what we ask for
 * (?count= only bounds AGENDA); past MAX_EVENTS we keep what arrived and set
 * gc_trunc, which the status line reports as "more".
 */
#ifndef MAX_EVENTS
#define MAX_EVENTS      64
#endif

/*
 * 39 characters plus NUL. At the width we ask for, the wire gives 41-49
 * characters of title and the list column shows 30 -- the extra is what the
 * two-row detail panel spells out.
 *
 * Overridable, because an 80-column backend can show most of what the wire
 * sends in the list column itself and has the RAM to keep it. Every override
 * costs MAX_EVENTS times the difference, so check the map file.
 */
#ifndef TITLE_LEN
#define TITLE_LEN       40
#endif


/* eventNum as ASCII decimal. It goes straight back out in a URL and is never
   used for arithmetic, so it is kept as the text we already had. The adapter
   caps a window at 300, so three digits and the NUL always suffice. */
#define EVNUM_LEN       4

/* Agenda display list: at most one separator per event, plus the events, so
   twice MAX_EVENTS is the exact bound. Overridable only to trade the tail of a
   very fragmented agenda for bytes -- agenda_build() stops at AGD_MAX. */
#ifndef AGD_MAX
#define AGD_MAX         (MAX_EVENTS * 2)
#endif

/* GCAL_MAX_CALENDARS is 8; plus "all shown calendars" and one spare. */
#define CAL_MAX         10
#define CAL_NAME_LEN    24      /* display name, upper-cased, for the picker */
#define CAL_SEL_LEN     48      /* the name verbatim, case intact */

/*
 * Wrapped event-detail rows kept before the truncation flag goes up, and the
 * width they are wrapped to. 48 x 40 is about 1900 characters of description,
 * which is two windows and then some.
 *
 * Both are overridable so a backend can match its own screen. Widen DET_COLS
 * and the same text needs fewer rows, so the two move in opposite directions
 * and the buffer need not grow: 32 x 78 holds more prose than 48 x 40 does.
 */
#ifndef DET_ROWS
#define DET_ROWS        48
#endif
#ifndef DET_COLS
#define DET_COLS        40
#endif
#define DET_STRIDE      (DET_COLS + 1)

/*
 * Every backend above knows its width at compile time; a PC does not -- it
 * inherits whatever text mode it was booted in, 40 or 80 columns, and finds
 * out at plat_init(). GC_RT_COLS turns the *wrap* width into a variable the
 * backend sets, while DET_COLS goes on sizing the storage for the widest
 * case. Wrapping narrower than the stride is safe; the reverse would be a
 * buffer overrun, which is why DET_STRIDE itself stays derived and
 * non-overridable.
 */
#ifdef GC_RT_COLS
extern unsigned char gc_wrap_cols;      /* set by the backend, <= DET_COLS */
#define GC_WRAP_COLS    gc_wrap_cols
#else
#define GC_WRAP_COLS    DET_COLS
#endif

/* Longest raw line accumulated before a hard flush. A width-80 listing row
   never exceeds 80, and the detail text arrives pre-wrapped at 38 or 80 --
   so the tight builds trim this toward 80-and-some without losing a byte
   that could actually arrive. */
#ifndef LINE_CAP
#define LINE_CAP        132
#endif

/*
 * The network receive buffer. Every reader drains through split_lines(), which
 * is indifferent to where a chunk boundary lands, so shrinking this costs only
 * round trips -- which is the trade a machine with 28K of program space wants.
 */
#ifndef GC_RXBUF
#define GC_RXBUF        512
#endif

/*
 * The detail ingest's own accumulator. It matches LINE_CAP unless DET_REFLOW is
 * on, in which case it holds a whole rejoined paragraph rather than one wire
 * line and wants to be several rows' worth -- a paragraph longer than this
 * still works, it just ends a row early where the carry happens.
 */
#ifndef DET_LINE_CAP
#define DET_LINE_CAP    LINE_CAP
#endif

/* ------------------------------------------------------------------ */
/* Views                                                               */
/* ------------------------------------------------------------------ */

#define VIEW_DAY        0
#define VIEW_WEEK       1
#define VIEW_MONTH      2
#define VIEW_AGENDA     3

/* ------------------------------------------------------------------ */
/* Wire format                                                         */
/* ------------------------------------------------------------------ */

/*
 * The listing is asked for as text, not as the packed CalEventItem structs
 * aux2 = 255 would give.
 *
 * CalEventItem carries start and end as uint64 epoch seconds UTC, so rendering
 * one means evaluating a POSIX TZ rule ("CST6CDT,M3.2.0/2,M11.1.0/2") and
 * converting a 64-bit epoch to a civil date, on a 6502 -- a second and
 * divergent copy of arithmetic NetworkProtocolCalendar has already done
 * correctly. The text form arrives resolved to the same local wall clock the
 * adapter used to compute the window, and is ~80 bytes per event against 277.
 *
 * The width goes in aux2 and the adapter takes it as (aux2 & 0x7F), so 80 is
 * legal and 128 would not be. It must be 80 rather than 40: format_index_human
 * only picks the single-line layout when (width - used) >= 40, and `used` runs
 * to 23, so a 40-column request silently falls back to the two-line Mailbox
 * layout this parser does not understand.
 */
#define GC_WIDTH        80
#define GC_TIMEW        11      /* "09:00-10:00" */
#define GC_CATW         14

/* Access modes (Protocol.h ACCESS_MODE). aux2 on a DIRECTORY open is the
   line width; on a READ open the Calendar protocol ignores it entirely,
   because every byte it emits already has lineEnding applied. */
#define GC_MODE_READ    4
#define GC_MODE_DIR     6

/* WRITE opens a draft channel: field lines go out via network_write() and the
   adapter commits the event on close. A selector-only spec composes a new
   event; the detail spec (view/date/N) edits that event. aux2 is ignored. */
#define GC_MODE_WRITE   8

/* NDEV_STATUS codes (status_error_codes.h) that this client maps.
   GC_NOREPLY is ours, not the device's: it means the status call itself
   failed, which is distinct from the device answering "0 bytes". */
#define GC_OK           1
#define GC_NOREPLY      0xFF
#define GC_EOF          136     /* buffer drained -- normal, not an error */
#define GC_BADSPEC      165
#define GC_DENIED       167
#define GC_NOTFOUND     170
#define GC_NOSERVICE    210
#define GC_NOAUTH       212

/* Codes a write channel can latch into the STATUS that follows the
   commit-on-close. Every draft rejection collapses to GC_BADDRAFT -- the
   specific reason only reaches the adapter's debug log, which is why
   form_validate() catches what it can before a byte goes out. GC_DENIED on a
   save usually means the OAuth grant predates the calendar.events scope and
   the user has to re-authorize Google in the web UI. */
#define GC_WRONLY       131     /* read on a write channel */
#define GC_BADDRAFT     132     /* the adapter rejected the draft */
#define GC_RDONLY       135     /* provider or mode cannot write */
#define GC_FULL         162     /* draft exceeded the adapter's 16K cap */

/* fn_default_timeout is in 64-frame ticks. 15 (the library default) is ~16s;
   a window open is one upstream HTTPS round trip per calendar. */
#define TMO_NORM        15
#define TMO_LONG        90      /* ~96 seconds */

/* ------------------------------------------------------------------ */
/* Parsed model                                                        */
/* ------------------------------------------------------------------ */

#define EVF_ALLDAY      0x01
#define EVF_RECURRING   0x02
#define EVF_OPENEND     0x04    /* "HH:MM->" -- ends on a later day */
#define EVF_FIRED       0x08    /* the alarm has already sounded for this one */

/* Google's eleven colour names, in the order color.c matches them. */
#define GC_NCOLORS      11
#define COL_LAVENDER    0
#define COL_SAGE        1
#define COL_GRAPE       2
#define COL_FLAMINGO    3
#define COL_BANANA      4
#define COL_TANGERINE   5
#define COL_PEACOCK     6
#define COL_GRAPHITE    7
#define COL_BLUEBERRY   8
#define COL_BASIL       9
#define COL_TOMATO      10
#define COL_NONE        GC_NCOLORS      /* category was a calendar name */

/* The five chips those eleven quantise onto -- one per player, plus the
   missiles. Kept in the record so a painter never has to re-derive it. */
#define CHIP_BLUE       0
#define CHIP_RED        1
#define CHIP_GREEN      2
#define CHIP_YELLOW     3
#define CHIP_GRAPHITE   4
#define CHIP_COUNT      5

/*
 * The category column verbatim, plus NUL -- only when GC_KEEP_CAT is defined.
 *
 * It is not merely a spelling of `color` below: GCAL.cpp's category_for()
 * fills it from extendedProperties, else the Google colour name, else the
 * calendar's own name, so for an event with no colorId it says which calendar
 * the event came from, which `color` cannot.
 *
 * It costs MAX_EVENTS times CAT_LEN, which is 960 bytes -- more than the Atari
 * build has spare, and it has nowhere to put a fifteenth column anyway. So a
 * backend asks for it, and one that cannot show it does not pay for it.
 */
#ifdef GC_KEEP_CAT
#define CAT_LEN         (GC_CATW + 1)
#endif

struct event {
    char          num[EVNUM_LEN];
    char          title[TITLE_LEN];
#ifdef GC_KEEP_CAT
    char          cat[CAT_LEN];
#endif
    unsigned char flags;
    unsigned char color;        /* COL_*, 0..GC_NCOLORS */
    unsigned char chip;         /* CHIP_*, which player draws it */
    unsigned char sh, sm;       /* start hour, minute, 24-hour, local */
    unsigned char eh, em;       /* end hour, minute */
    /*
     * day is polymorphic, because each view's date column carries something
     * different and none of them is ever wanted in another view:
     *   DAY     always 0 -- every event is on the anchor day
     *   WEEK    0-6, day of week, from the "Sun".."Sat" column
     *   MONTH   events are not stored at all, only tallied
     *   AGENDA  1-31, day of month; mon carries the month so a separator can
     *           render across a month boundary
     */
    unsigned char day;
    unsigned char mon;          /* 1-12, AGENDA only */
};

/* Agenda display list, one byte per display row: AGD_SEP marks a date
   separator, the low seven bits are the event index. MAX_EVENTS is 64, so six
   bits always suffice -- the Intellivision needed two bytes per row. */
#define AGD_SEP     0x80
#define AGD_IDX     0x7F

struct cal {
    char name[CAL_NAME_LEN];    /* upper-cased, clipped, for the picker column */
    char sel[CAL_SEL_LEN];      /* the calendar name verbatim, case intact */
};

extern struct event  gc_index[MAX_EVENTS];
extern unsigned char gc_count;          /* events parsed, 0..MAX_EVENTS */
extern unsigned char gc_trunc;          /* the window had more than we kept */
extern char          gc_wtitle[41];     /* the adapter's own window title */

extern unsigned char gc_agd[AGD_MAX];
extern unsigned char gc_agd_count;

/*
 * MONTH tallies while streaming and stores no events: a 300-event month costs
 * these 64 bytes. Indexed by day of month, so [0] is never used.
 *
 * gc_daycol holds the leading event's COL_*, not its chip. It used to hold the
 * chip, which threw the distinction between Peacock and Blueberry away at
 * parse time -- fine for the three backends that quantise to five colours
 * anyway, and a real loss on the Adam, which has an ink for each of the eleven.
 * A backend that wants the chip calls color_chip() on it, which is where that
 * decision belongs.
 */
extern unsigned char gc_daycnt[32];
extern unsigned char gc_daycol[32];

/* Under GC_CALS_OVERLAY the picker's list lives in borrowed RAM; the macro
   that replaces this array is down in the compose-form section, after the
   struct whose size sets its offset. */
#ifndef GC_CALS_OVERLAY
extern struct cal    gc_cals[CAL_MAX];
#endif
extern unsigned char gc_cal_count;

extern char          gc_det[DET_ROWS][DET_STRIDE];
extern unsigned int  gc_det_rows;
extern unsigned char gc_det_trunc;

/* The view anchor. cur_d is also the MONTH grid's selection -- there is no
   second variable, so moving the cursor is ordinary date arithmetic and
   rolling into a neighbouring month comes free. */
extern unsigned int  cur_y;
extern unsigned char cur_mo, cur_d;

/* The selected calendar, as stored in the appkey. Empty means "every calendar
   Google is showing". It must never be "*": util_devicespec_fix_for_parsing()
   rewrites that to an embedded NUL on any non-DIRECTORY open, which would
   corrupt the detail fetch of the same event. */
extern char          gc_cal[CAL_SEL_LEN];

extern unsigned char gc_ecode;          /* last error, for the error screen */
extern unsigned char gc_dev_ecode;      /* raw device code behind it */
extern const char   *gc_stage;          /* open / status / read */

/* ------------------------------------------------------------------ */
/* Portable services                                                   */
/* ------------------------------------------------------------------ */

/* sanitize.c -- copy a NUL-terminated wire field into a fixed-width buffer,
   clamping control bytes to space and collapsing each run of bytes > 126 to a
   single '?'. Always NUL-terminates. */
void copy_san(char *dst, const char *src, unsigned char dstsize);

/* wrap.c -- greedy whole-word wrap into a row array of `stride` bytes each.
   Words longer than `cols` are hard-split. On row-budget overflow the last row
   is ellipsized with "...". Returns the number of rows produced. */
unsigned int wrap_text(const char *src, char *rows, unsigned int max_rows,
                       unsigned char cols, unsigned char stride);

/* detail.c -- folds the event-detail byte stream into wrapped display rows. */
void          detail_reset(void);
void          detail_ingest(const unsigned char *p, unsigned int n);
void          detail_finish(void);

/* index.c -- the listing parser. idx_reset() before a fetch, then one call per
   complete line; it sorts out the window title, the column layout and the
   events itself. */
void          idx_reset(unsigned char view);
void          idx_line(const char *p, unsigned char len);

/* lines.c -- cut a byte stream into NUL-terminated lines. All three
   terminators end a line: the bus decides which one arrives, and they do not
   agree ($9B over SIO, CR over IWM and DriveWire and AdamNet). */
typedef void (*line_fn)(const char *p, unsigned char len);

void          split_reset(void);
void          split_lines(const unsigned char *p, unsigned int n, line_fn emit);
void          split_finish(line_fn emit);

/* color.c -- match the category column against Google's colour names.
   Returns COL_* or COL_NONE. */
unsigned char color_match(const char *p, unsigned char len);
unsigned char color_chip(unsigned char color);

/* The colour's own name, upper-cased, or "" for COL_NONE -- which is not a
   colour but "the category was a calendar name". For a legend; a list column
   wants the event's own cat[], which says more. */
const char   *color_name(unsigned char color);

/* date.c -- civil arithmetic on (y, mo, d). No epoch anywhere in the program:
   the adapter resolves every timestamp to local wall clock before it reaches
   us, and these only ever move the anchor around. */
unsigned char date_leap(unsigned int y);
unsigned char date_dim(unsigned int y, unsigned char mo);
void          date_addday(unsigned int *y, unsigned char *mo, unsigned char *d);
void          date_subday(unsigned int *y, unsigned char *mo, unsigned char *d);
void          date_addmonth(unsigned int *y, unsigned char *mo, unsigned char *d);
void          date_submonth(unsigned int *y, unsigned char *mo, unsigned char *d);
unsigned char date_dow(unsigned int y, unsigned char mo, unsigned char d);
void          date_iso(char *dst, unsigned int y, unsigned char mo,
                       unsigned char d);
const char   *date_dow3(unsigned char dow);
const char   *date_mon3(unsigned char mo);

/* agenda.c -- build the separator-interleaved display list from gc_index. */
void          agenda_build(void);

/* net.c -- all return 1 on success, 0 on failure with gc_ecode set. */
unsigned char gc_fetch_index(unsigned char view);
unsigned char gc_fetch_detail(unsigned char view, const char *evnum);

/* cals.c -- the calendar picker's list. */
unsigned char gc_fetch_cals(void);

/* clock.c -- the FujiNet clock, and a local tick between resyncs. */
extern unsigned int  clk_y;
extern unsigned char clk_mo, clk_d, clk_h, clk_mi, clk_s;
extern unsigned char clk_ok;

unsigned char clk_fetch(void);              /* 1 on success */
unsigned char clk_tick(void);               /* 1 when the minute changed */
unsigned char clk_due_resync(void);
void          clk_today(void);              /* anchor := the clock's today */
unsigned char clk_is_today(unsigned int y, unsigned char mo, unsigned char d);
unsigned char clk_get_tz(char *dst, unsigned char dstsize);

/* settings.c -- alarm lead and calendar selector, in a FujiNet appkey. */
#define AL_LEAD_DEFAULT 10
#define AL_LEAD_MAX     60

extern unsigned char al_lead;

void          set_load(void);
void          set_save(void);

/* alarm.c -- synthesised client side; the adapter's field mask never asks
   Google for reminders. */
void          alarm_reset(void);
unsigned char alarm_scan(unsigned char view);   /* event index, or AL_NONE */
unsigned char alarm_step(unsigned char view);   /* 1 while a banner is up */
void          alarm_dismiss(void);
extern unsigned char al_active;
extern unsigned char al_ev;
#define AL_NONE     0xFF

/* ------------------------------------------------------------------ */
/* Compose / edit form                                                 */
/* ------------------------------------------------------------------ */

/*
 * One row per field, in screen order. The DATE/START/END trio is what the
 * adapter's START:/END: keys are built from; a blank START time makes the
 * event all-day, exactly as a date-only START does on the wire.
 */
#define FRM_TITLE   0
#define FRM_DATE    1
#define FRM_START   2
#define FRM_END     3
#define FRM_LOC     4
#define FRM_DESC    5
#define FRM_CAT     6
#define FRM_NFIELDS 7

/*
 * Field capacities, excluding the NUL. These are RAM knobs like TITLE_LEN:
 * the defaults suit the 40-column targets, and the 80-column backends widen
 * LOC and DESC because they can show what they store. The wire imposes no
 * limit worth honouring here -- the adapter takes 16K a draft.
 */
#ifndef FRM_TITLE_MAX
#define FRM_TITLE_MAX   (TITLE_LEN - 1)
#endif
#ifndef FRM_LOC_MAX
#define FRM_LOC_MAX     32
#endif
#ifndef FRM_DESC_MAX
#define FRM_DESC_MAX    96
#endif
#define FRM_CAT_MAX     15      /* the wire's category column is 14 */
#define FRM_DATE_MAX    10      /* YYYY-MM-DD */
#define FRM_TIME_MAX    5       /* HH:MM */

/* The longest emitted line is "DESCRIPTION: " + desc + terminator + NUL;
   everything else, including the echo window, is shorter. */
#define FRM_LINE_MAX    (FRM_DESC_MAX + 16)

struct frmbuf {
    char title[FRM_TITLE_MAX + 1];
    char date[FRM_DATE_MAX + 1];
    char start[FRM_TIME_MAX + 1];
    char end[FRM_TIME_MAX + 1];
    char loc[FRM_LOC_MAX + 1];
    char desc[FRM_DESC_MAX + 1];
    char cat[FRM_CAT_MAX + 1];
    char line[FRM_LINE_MAX];    /* emit and echo scratch, shared */
};

/* sizeof(struct frmbuf), spelled for the preprocessor: the overlay guards
   below have to run in #if (CMOC cannot fold sizeof into an array bound),
   and a struct of nothing but chars has no padding to make them differ.
   tests/hosttest.c asserts the two stay equal. */
#define FRMBUF_SIZE ((FRM_TITLE_MAX + 1) + (FRM_DATE_MAX + 1) + \
                     2 * (FRM_TIME_MAX + 1) + (FRM_LOC_MAX + 1) + \
                     (FRM_DESC_MAX + 1) + (FRM_CAT_MAX + 1) + FRM_LINE_MAX)

/*
 * GC_FORM_OVERLAY parks the form on top of the detail buffer instead of
 * paying for it. Safe because the two are never alive together: entering the
 * form abandons any detail screen, and the refetch on the way back rebuilds
 * gc_det from scratch. form.c carries the compile-time size guard.
 */
#ifdef GC_FORM_OVERLAY
#define frm (*(struct frmbuf *) gc_det)
#else
extern struct frmbuf frm;
#endif

/*
 * GC_CALS_OVERLAY parks the calendar picker's list in the same borrowed
 * RAM. Also sound, for the same shape of reason: gc_cals only lives inside
 * do_pick() -- fetched, painted, and copied out of before it returns --
 * and neither the detail screen nor the form can be up at the same time as
 * the picker. It sits behind the form when the buffer has room for both,
 * else on top of it -- the form and the picker are themselves never alive
 * together, so sharing the base is as sound as sharing the buffer.
 * form.c guards whichever placement this resolves to.
 */
#ifdef GC_CALS_OVERLAY
#define GC_CALS_SIZE (CAL_MAX * (CAL_NAME_LEN + CAL_SEL_LEN))
#if FRMBUF_SIZE + GC_CALS_SIZE <= DET_ROWS * DET_STRIDE
#define GC_CALS_OFF FRMBUF_SIZE
#else
#define GC_CALS_OFF 0
#endif
#define gc_cals ((struct cal *) ((char *) gc_det + GC_CALS_OFF))
#endif

/* Which fields the user has touched. Compose emits every non-empty field;
   edit emits only the dirty ones, so an untouched field can never clobber
   the server's copy with this client's truncated one. */
extern unsigned char frm_dirty[FRM_NFIELDS];

/* Form messages, drawn by ui_form_msg(). FM_NONE restores the normal hints. */
#define FM_NONE      0
#define FM_ASK       1          /* save? yes / no */
#define FM_NEEDTITLE 2
#define FM_BADDATE   3
#define FM_BADTIME   4
#define FM_ENDALONE  5          /* an END time needs a START time */

/* form.c -- the form model. Pure; tests/hosttest.c exercises all of it.
   form_init with e == 0 starts a blank compose on (y, mo, d); with an
   event it prefills for an edit. form_emit sends the draft one line at a
   time through form_put() -- net.c's in the real program, the capture in
   the tests -- and returns how many lines went out; write failures are
   form_put's own to latch, and gc_save_end() is where they report. */
void          form_init(const struct event *e, unsigned int y,
                        unsigned char mo, unsigned char d);
char         *form_field_ptr(unsigned char f);
unsigned char form_field_max(unsigned char f);
unsigned char form_date_ok(const char *s);
unsigned char form_time_ok(const char *s);
unsigned char form_any_dirty(void);
unsigned char form_validate(unsigned char editing, unsigned char *bad);
unsigned char form_emit(unsigned char editing);
void          form_put(const char *line);

/* compose.c -- the form screen. Both return 1 when an event was saved and
   the listing is stale, 0 on cancel or a failure already reported. */
unsigned char compose_new(unsigned int y, unsigned char mo, unsigned char d);
unsigned char compose_edit(unsigned char view, unsigned char ev);

/* net.c -- the draft channel. begin opens it, form_put() above writes the
   field lines into it, end closes -- which is what commits -- and reads
   the verdict, a latched write failure included. */
unsigned char gc_save_begin(unsigned char editing, unsigned char view,
                            const char *evnum);
unsigned char gc_save_end(void);

/* ------------------------------------------------------------------ */
/* Platform backend -- implemented per target under src/<platform>/     */
/* ------------------------------------------------------------------ */

#ifndef LIST_ROWS
#define LIST_ROWS   16
#endif
#ifndef DET_WIN
#define DET_WIN     18
#endif

/*
 * The calendar picker's window. main.c bounds its own scrolling by this and
 * every backend paints exactly this many rows: the two have to agree, or the
 * window advances a row early and the rows past it are painted stale. It lives
 * here rather than in each backend because that is exactly the disagreement
 * that went unnoticed while there were only two of them.
 */
#ifndef PICK_ROWS
#define PICK_ROWS   12
#endif

/* Portable key codes returned by plat_getkey(). */
#define K_NONE      0
#define K_UP        1
#define K_DOWN      2
#define K_LEFT      3
#define K_RIGHT     4
#define K_ENTER     5
#define K_BACK      6
#define K_REFRESH   7
#define K_QUIT      8
#define K_TODAY     9
#define K_VIEW1     10          /* day */
#define K_VIEW2     11          /* week */
#define K_VIEW3     12          /* month */
#define K_VIEW4     13          /* agenda */
#define K_NEW       14          /* compose an event on the anchor date */
#define K_EDIT      15          /* edit the selected event */

void          plat_init(void);
void          plat_shutdown(void);

/* Bracket every network, clock and fuji device call. On the Atari this
   suppresses display list interrupts, which would otherwise steal cycles from
   a timing-critical SIO transfer. */
void          plat_net_begin(void);
void          plat_net_end(void);

unsigned char plat_getkey(void);        /* blocks, returns a K_* code */
unsigned char plat_getkey_poll(void);   /* K_NONE at once when nothing waits */
void          plat_anykey(void);

/*
 * The form's key read. Printable ASCII $20-$7E passes through verbatim;
 * everything with a meaning maps to an E_* code below $20, so the two can
 * never collide. Blocks like plat_getkey() -- and with the same obligation
 * to keep plat_vsync() turning where the frame counter needs a hand.
 *
 * Not every backend can produce every code: a keyboard without cursor keys
 * simply never sends E_LEFT, and the editor edits append-and-backspace there.
 */
#define E_ENTER     1           /* next field */
#define E_UP        2
#define E_DOWN      3
#define E_LEFT      4
#define E_RIGHT     5
#define E_BS        6           /* delete before the cursor */
#define E_DONE      7           /* leave the form (ESC / BREAK / SmartKey) */
#define E_SAVE      8           /* save now, skipping the ask (Adam SmartKey) */

unsigned char plat_getch(void);

/* Frame timing. plat_ticks() is a free-running frame counter that keeps
   running while a blocking screen sits inside the keyboard handler, which is
   what lets the clock stay honest across one. */
void          plat_vsync(void);
unsigned long plat_ticks(void);
unsigned char plat_fps(void);

/* sound.c -- the alarm chime, three rising notes. */
void          plat_tone(unsigned char note);
void          plat_silence(void);

/* Busy overlay reasons. */
#define BUSY_CLOCK  1
#define BUSY_INDEX  2
#define BUSY_DETAIL 3
#define BUSY_CALS   4
#define BUSY_SAVE   5

void          ui_splash(void);
void          ui_notfound(void);
void          ui_noclock(void);
void          ui_busy(unsigned char reason);
void          ui_error(unsigned char code);

void          ui_view(unsigned char view, unsigned char sel,
                      unsigned char first);
void          ui_view_sel(unsigned char view, unsigned char from,
                          unsigned char to, unsigned char first);
void          ui_detail(unsigned char ev, unsigned int top);
void          ui_pick(unsigned char sel, unsigned char first);
void          ui_setup(void);
void          ui_setup_lead(void);      /* the lead line alone */
void          ui_clock(void);
void          ui_alarm(unsigned char phase);
void          ui_hints(unsigned char view);

/*
 * The form screen. ui_form() paints the chrome -- title, field labels, the
 * footer hints -- and nothing inside the fields; compose.c then draws every
 * row through ui_form_row(), which is also how each keystroke is echoed.
 *
 * ui_form_row() gets the visible slice of the field already windowed --
 * compose.c owns the horizontal scroll -- with curx the cursor's column
 * within it, only meaningful while `active`. This is deliberately the one
 * hook where the backends' inv-flag / attribute-role split lives.
 *
 * ui_form_width() reports how many text columns field f's window has, so the
 * engine and the painter cannot disagree about where the scroll lands.
 */
void          ui_form(unsigned char editing);
unsigned char ui_form_width(unsigned char f);
void          ui_form_row(unsigned char f, const char *win,
                          unsigned char curx, unsigned char active);
void          ui_form_msg(unsigned char msg);

#endif /* GCAL_H */
