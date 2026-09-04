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
 * in r2r/atari/gcal.map against the $AC20 ceiling afterwards.
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
   caps a window at 300, so three digits always suffice. */
#define EVNUM_LEN       6

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
   never exceeds 80, and the detail text arrives pre-wrapped at 38 or 80. */
#define LINE_CAP        132

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
#ifndef COCO3
    char          title[TITLE_LEN];
#endif
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

extern struct cal    gc_cals[CAL_MAX];
extern unsigned char gc_cal_count;

#ifdef COCO3
/*
 * On the CoCo 3 the wrapped detail text lives in the second 64K, not here --
 * see src/coco/far.c. It is written a few rows at a time as the description
 * ingests and read one row at a time to draw, so it never needs to be
 * addressable, and keeping it out of the 6809's 64K is what pays for the
 * 80-column layout.
 */
#define FAR_DET     0
#define FAR_TITLE   ((unsigned int) DET_ROWS * DET_STRIDE)

void far_get(void *dst, unsigned int off, unsigned int len);
void far_put(unsigned int off, const void *src, unsigned int len);

/*
 * Titles go far too. They are the largest slice of an event -- forty bytes of
 * about sixty-five -- and every use is a whole string, so they cost one fetch
 * rather than a field access. ev_title() returns a shared buffer, so only one
 * title is live at a time; nothing here needs two.
 */
const char *ev_title(unsigned char ev);
void        ev_set_title(unsigned char ev, const char *src);
#else
extern char          gc_det[DET_ROWS][DET_STRIDE];
#endif
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

#endif /* GCAL_H */
