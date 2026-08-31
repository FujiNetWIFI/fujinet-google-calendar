/*
 * MS-DOS backend -- internal interface shared by the files in this directory.
 * The portable half of the program talks to us only through the plat_* / ui_*
 * declarations in ../gcal.h.
 *
 * Target is Open Watcom's 8086 small model, BIOS text modes only, which is
 * what lets the same GCAL.EXE run on everything from a PCjr to a 486: no
 * instruction newer than the 8088 has, no video access fancier than the text
 * page every adapter in the family exposes.
 *
 * The one thing no other backend has to deal with: the screen width is not
 * known until the program is running. A PC inherits whatever video mode it
 * was started in -- 40x25 in modes 0/1, 80x25 in modes 2/3, and the MDA's
 * mode 7 -- so SCR_COLS cannot be a macro here. screen.c probes the mode in
 * plat_init() and exports the geometry as variables; ui_geom() picks the
 * column layout off scr_wide, and detail.c wraps to the runtime width
 * through the GC_RT_COLS hook in gcal.h. At 80 columns the layout is the
 * Apple //e's, at 40 it is the Atari's, both stretched one row for the 25th
 * line -- so the same machine shows one client or the other depending on
 * how it booted.
 */

#ifndef MSDOS_PLATFORM_H
#define MSDOS_PLATFORM_H

#ifndef GC_RT_COLS
#error "the MS-DOS backend requires -DGC_RT_COLS (see CFLAGS_EXTRA_MSDOS)"
#endif

/* Every mode this backend runs in is 25 rows. EGA and VGA can be talked into
   43 or 50, and a program started there is put back into mode 3 by the probe
   rather than taught a third geometry -- see plat_init(). */
#define SCR_ROWS    25

extern unsigned char scr_cols;      /* 40 or 80, probed at plat_init()      */
extern unsigned char scr_wide;      /* scr_cols >= 80: the layout switch    */

/* ------------------------------------------------------------------ */
/* Attributes                                                          */
/* ------------------------------------------------------------------ */

/*
 * Painters name a role, not a byte. screen.c resolves the role through one
 * of three tables picked at init -- colour (modes 1/3), black-and-white
 * (modes 0/2, or /MONO), and MDA (mode 7) -- so a painter never knows
 * whether the heading it just drew is blue-on-grey or the MDA's underline.
 * This is the job the Adam's attribute roles do, grown to fit hardware that
 * has more than one bit of emphasis: the MDA's underline and intensity are
 * real attributes here, not glyph tricks. Eleven roles where the gmail
 * client needs six, because a calendar has more kinds of chrome -- a status
 * band, a today mark, a two-phase alarm flash.
 */
#define A_TEXT      0   /* the page: list and body text                  */
#define A_DIM       1   /* secondary: agenda rules, "(nothing)"          */
#define A_SEL       2   /* the selection bar                             */
#define A_BAR       3   /* the app bar, row 0                            */
#define A_STAT      4   /* header rows 1-2: window title, status, clock  */
#define A_FOOT      5   /* the hint bar, row 24                          */
#define A_TITLE     6   /* detail title, panel headings; MDA underline   */
#define A_TODAY     7   /* today's number and column head; MDA bright    */
                        /* underline                                     */
#define A_ALARM_A   8   /* banner flash phase 1                          */
#define A_ALARM_B   9   /* banner flash phase 0                          */
#define A_EMPH      10  /* the active view tab                           */
#define N_ATTRS     11

/* ------------------------------------------------------------------ */
/* Character set                                                       */
/* ------------------------------------------------------------------ */

/*
 * Code page 437. Unlike every other backend there is no sc() mapping at
 * all: the byte in the string is the glyph in the cell. copy_san() clamps
 * every wire field to $20-$7E before it reaches a painter, so the control
 * range and the high range are ours for chrome -- and CP437 fills both with
 * exactly the furniture a calendar wants. Spelled as separate string
 * literals so an escape is never followed by a hex digit inside one.
 */
#define GL_UPDOWN   "\x18\x19"  /* up and down arrows, for the hints     */
#define GL_LR       "\x1B\x1A"  /* left and right                        */
#define GL_CHECK    "\xFB"      /* the picker's "this one is saved" tick */
#define GL_RULE     0xC4        /* single horizontal line, for scr_fill  */
#define GL_BLOCK    0xDB        /* full block: colour chips, logo, bars  */

/*
 * The five chips as glyphs, for the two tables with no colour to spend. A
 * density ramp rather than five arbitrary shapes: the hotter the chip, the
 * heavier the cell, and the settings screen legends them. In colour mode
 * these are never used -- each of Google's eleven colours has its own ink
 * (ink_attr below), the Adam's arrangement.
 */
#define CHIP_GL_BLUE        0xDB    /* full block                        */
#define CHIP_GL_RED         0xB2    /* dark shade                        */
#define CHIP_GL_GREEN       0xB1    /* medium shade                      */
#define CHIP_GL_YELLOW      0xB0    /* light shade                       */
#define CHIP_GL_GRAPHITE    0xFE    /* centred square                    */

#define CHIP_NONE   0xFF

/* One CHIP_* value in, one glyph byte out. CHIP_NONE gives a space. */
unsigned char chip_glyph(unsigned char chip);

/* ------------------------------------------------------------------ */
/* Band geometry                                                       */
/* ------------------------------------------------------------------ */

/* Rows 0-2 header, 3-23 content, 24 footer -- the same three-region split
   every backend makes, with the extra row a 25-line screen has over the
   Apple's 24 spent on the content band. */
#define HDR_ROWS        3
#define FOOT_ROW        (SCR_ROWS - 1)
#define CONTENT_TOP     HDR_ROWS
#define CONTENT_ROWS    (FOOT_ROW - HDR_ROWS)

/* ------------------------------------------------------------------ */
/* screen.c -- direct writes into the B000/B800 text page              */
/* ------------------------------------------------------------------ */

void scr_clear(void);
void scr_row_clear(unsigned char row);
void scr_rows_clear(unsigned char first, unsigned char last);

/* Write s into [col, col+width), space padded and truncated to fit. */
void scr_field(unsigned char row, unsigned char col, const char *s,
               unsigned char width, unsigned char attr);
void scr_text(unsigned char row, unsigned char col, const char *s,
              unsigned char attr);
void scr_right(unsigned char row, unsigned char rcol, const char *s,
               unsigned char attr);
void scr_center(unsigned char row, const char *s, unsigned char attr);

/* A run of one repeated glyph -- rules and chrome bands. */
void scr_fill(unsigned char row, unsigned char col, unsigned char glyph,
              unsigned char width, unsigned char attr);

/* One cell with an explicit attribute byte, sidestepping the role tables --
   the logo's strokes and the colour chips are painted with this. */
void scr_cell(unsigned char row, unsigned char col, unsigned char glyph,
              unsigned char rawattr);

/* The byte a role resolves to, and whether the table in force is the colour
   one -- what logo.c and the chip painters need to build raw attributes
   that sit on the right background. */
unsigned char scr_attr_byte(unsigned char attr);
extern unsigned char scr_color;

/*
 * The raw attribute for one of Google's eleven colours: the page's
 * background under the colour's own CGA ink, for a GL_BLOCK cell. Callers
 * in colour mode only -- the two monochrome tables go through chip_glyph()
 * instead. COL_NONE gets dark grey, "a calendar name, not a colour".
 */
unsigned char ink_attr(unsigned char color);

#ifdef GC_SHOT
/* tools/msdos-shot.sh's capture: the text page, verbatim, into SCREEN.BIN
   in the current directory -- two geometry bytes and the BIOS mode, then
   cols x rows char/attr pairs. input.c calls it where the program would
   otherwise block. */
void scr_snapshot(void);
#endif

/* ------------------------------------------------------------------ */
/* ui.c -- chrome shared with views.c                                  */
/* ------------------------------------------------------------------ */

/* The layout ui_geom() chooses once screen.c has probed the width. Views
   paint through these rather than macros; everything not listed here is the
   same at both widths. */
extern unsigned char right_col;             /* last text column           */
extern unsigned char col_cat, w_cat;        /* 80 columns only, else 0    */
extern unsigned char col_title, w_title;
extern unsigned char wk_ncol, wk_tcol;      /* WEEK count / lead columns  */
extern unsigned char mo_cellw, mo_left;     /* MONTH cell pitch and edge  */
extern unsigned char mo_bars;               /* densest bar we draw        */

void ui_geom(void);

void ui_header(unsigned char view);     /* rows 0-2 plus the mark */
void ui_status(void);                   /* row 2 alone */
void ui_footer(const char *hints, const char *right);
void ui_hhmm(char *dst, unsigned char h, unsigned char m);

/* ------------------------------------------------------------------ */
/* logo.c -- the Google Calendar mark                                  */
/* ------------------------------------------------------------------ */

/*
 * A white page with "31" on it, ringed in the four brand colours. Two
 * sizes, the Apple's dimensions exactly, so its centring math ports: the
 * small one sits in the header, the large one on the splash and busy
 * screens.
 */
#define LOGO_SMALL_COLS 4
#define LOGO_SMALL_ROWS 3
#define LOGO_LARGE_COLS 14
#define LOGO_LARGE_ROWS 6

void logo_small(unsigned char row, unsigned char col);
void logo_large(unsigned char row, unsigned char col);

/* ------------------------------------------------------------------ */
/* The bus, for the shims                                              */
/* ------------------------------------------------------------------ */

/*
 * fujinet-lib's INT F5 entry points: DL=0x40 read / 0x80 write, AL=device,
 * AH=command, CL/CH=aux1/2, ES:BX=buffer, DI=length; AL returns 'C' for
 * complete, 'E' error, 'N' NAK. Both are public members of the shipped
 * archive, but their declarations live in the library's internal
 * fujinet-fuji-msdos.h, which the release zip does not carry -- so the
 * shims declare them here instead of growing a bus layer of their own.
 */
unsigned char int_f5_read(unsigned char dev, unsigned char command,
                          unsigned char aux1, unsigned char aux2,
                          void *buf, unsigned short len);
unsigned char int_f5_write(unsigned char dev, unsigned char command,
                           unsigned char aux1, unsigned char aux2,
                           void *buf, unsigned short len);

#endif /* MSDOS_PLATFORM_H */
