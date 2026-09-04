/*
 * Tandy Color Computer backend -- internal interface shared by the files in
 * this directory. The portable half of the program talks to us only through
 * the plat_* / ui_* declarations in ../gcal.h.
 *
 * Two machines are built from this directory: the CoCo 1/2, described below,
 * and the CoCo 3 under -DCOCO3, whose GIME text page has its own notes in the
 * COCO3 block further down. Only one of the two compiles into any binary.
 *
 * The 1/2's screen is the 6847's 32x16 alpha/semigraphics page at $0400, and
 * the whole design turns on one property of it: the VDG decides per byte
 * whether a cell is a character or a 2x2 block of color, with no mode switch
 * and no second display list.
 *
 *   $00-$3F   glyph (byte & $3F), INV asserted -- green on dark green
 *   $40-$7F   the same glyph, normal video -- dark green on green
 *   $80-$FF   SG4: bit 7 set, bits 6-4 colour, bits 3-0 quadrant mask
 *
 * So text and colour intermix freely, and this is the first backend besides
 * the Atari's that can show an event's real Google colour -- as ordinary bytes
 * in screen RAM rather than as four players steered by an interrupt.
 *
 * Three things about that byte map bite, and all three are load-bearing here:
 *
 *   - The blank byte is $60 (space $20 with bit 6 set), not $00. memset(scr, 0)
 *     paints a screen of inverse '@'.
 *
 *   - Inverse video is XOR $40 -- but only on a character. XOR $40 on an SG4
 *     byte changes its *colour* ($8F green becomes $CF buff), so every routine
 *     that flips a run leaves bytes >= $80 alone.
 *
 *   - An unlit SG4 quadrant is black, and the text background is green, so a
 *     solid green chip on a text row is invisible. Column 0 is therefore a
 *     black gutter: an empty row gets SG_BLACK and every colour reads against
 *     it, green included.
 *
 * A stock 6847 has no lowercase at all -- 64 glyphs, uppercase only -- so sc()
 * folds case and every string literal in this directory is written uppercase.
 */

#ifndef COCO_PLATFORM_H
#define COCO_PLATFORM_H

#include <coco.h>

#ifdef COCO3

/*
 * CoCo 3: the GIME's 80x24 text page, a different machine from the VDG page
 * above in every way that matters here.
 *
 * A cell is two bytes -- character then attribute -- and the character is
 * plain ASCII, so sc()'s $3F fold has no counterpart. The attribute is
 * (fg << 3) | bg with bit 6 underline and bit 7 blink; fg indexes palette
 * slots 8-15 and bg slots 0-7.
 *
 * The page is not in the CPU map. It lives in MMU block $36, banked into the
 * $C000 window to be written and unbanked afterwards with interrupts masked
 * across the pair -- see screen.c.
 *
 * There are no semigraphics, so an event's color is the background of a space
 * rather than an SG4 byte -- one cell per color instead of four quadrants. The
 * MONTH density bar loses its half-cell steps to that and gets eight whole
 * ones instead of the VDG's sixteen quadrants, across a cell three times wider.
 */

#define SCR_COLS    80
#define SCR_ROWS    24

/* The $C000 window the text page is banked into, and the block that holds it. */
#define SCR_WIN     ((unsigned char *) 0xC000)
#define SCR_BLOCK   0x36

/* Plain ASCII on this page. */
#define SCR_BLANK   0x20

/* ------------------------------------------------------------------ */
/* Palette                                                             */
/* ------------------------------------------------------------------ */

/*
 * Background slots 0-7, and all eight are spent.
 *
 * The chrome is the MS-DOS backend's -- a light page with black text, blue
 * bands, a red alarm -- because that is the other backend with real attribute
 * color and the client should read the same on both.
 *
 * The rest are the event inks. Three background bits is eight slots and two of
 * them are spent above, so Google's eleven color names quantize onto the six
 * left plus the chrome blue, which doubles as one: see inks[] in screen.c.
 * That is two more than the VDG build's five chips, and what buys Grape its
 * purple and Tangerine its orange.
 */
#define PAL_PAGE        0       /* the page: light gray                 */
#define PAL_CHROME      1       /* bands and the blue ink, sharing one  */
#define PAL_PURPLE      2       /* Grape, which the VDG build loses     */
#define PAL_GREEN       3
#define PAL_YELLOW      4
#define PAL_ORANGE      5       /* Tangerine and Flamingo               */
#define PAL_RED         6       /* Tomato, and the alarm banner         */
#define PAL_GRAPHITE    7

/* Kept under their old names so the VDG build's constants still resolve. */
#define PAL_BLUE        PAL_CHROME
#define PAL_ALARM       PAL_RED

/* Foreground indices. These are 0-7 in the attribute byte and land on palette
   slots 8-15, which is why they are not the same numbers as the backgrounds. */
#define FG_INK          0       /* black, on the page                   */
#define FG_PAPER        1       /* bright white, on the blue bands      */
#define FG_ACCENT       2       /* blue, the page's accent              */
#define FG_DIM          3       /* gray, secondary text                 */
#define FG_ALARM        4       /* red, on the page                     */

#define ATTR(f, b)      ((unsigned char) (((f) << 3) | (b)))
#define ATTR_UNDER      0x40

/* ------------------------------------------------------------------ */
/* Attribute roles                                                     */
/* ------------------------------------------------------------------ */

/*
 * Painters name a role, not a color -- the MS-DOS backend's arrangement, and
 * the same eleven roles, so the two read alike. A color picker would rewrite
 * this block and repaint; nothing that draws knows a palette slot.
 *
 * A_TODAY takes the GIME's underline bit, which is the one piece of emphasis
 * this hardware has that the VDG does not and the MDA also spends here.
 */
#define A_TEXT      ATTR(FG_INK,    PAL_PAGE)
#define A_DIM       ATTR(FG_DIM,    PAL_PAGE)
#define A_SEL       ATTR(FG_PAPER,  PAL_CHROME)
#define A_BAR       ATTR(FG_PAPER,  PAL_CHROME)
#define A_STAT      ATTR(FG_PAPER,  PAL_CHROME)
#define A_FOOT      ATTR(FG_PAPER,  PAL_CHROME)
#define A_TITLE     ATTR(FG_ACCENT, PAL_PAGE)
#define A_TODAY     (ATTR(FG_ACCENT, PAL_PAGE) | ATTR_UNDER)
#define A_ALARM_A   ATTR(FG_PAPER,  PAL_ALARM)
#define A_ALARM_B   ATTR(FG_ALARM,  PAL_PAGE)
#define A_EMPH      ATTR(FG_INK,    PAL_PAGE)

#define CHIP_NONE       0xFF

/* ------------------------------------------------------------------ */
/* What the ported painters expect                                     */
/* ------------------------------------------------------------------ */

/*
 * views3.c and ui3.c are the MS-DOS backend's painters. That backend cannot
 * know its width until it boots and carries the geometry in variables; this
 * one is always 80x24, so the same names are constants and the compiler folds
 * the scr_wide branches away.
 */
#define scr_cols    SCR_COLS
#define scr_wide    1
#define scr_color   1

/*
 * Glyphs. Code page 437's furniture is not on this machine, so the chrome
 * falls back to ASCII that the 6847 and the GIME both have. GL_BLOCK is a
 * space: a color cell here is its background, not a filled foreground, so
 * the chip is painted by ink_attr() putting the color behind a blank.
 */
#define GL_UPDOWN   "^v"
#define GL_LR       "<>"
#define GL_CHECK    "*"
#define GL_RULE     '-'
#define GL_BLOCK    ' '

/* Only the monochrome legend uses these and scr_color is always true here,
   so that arm is dead code the linker drops. They exist to compile. */
#define CHIP_GL_BLUE        ' '
#define CHIP_GL_RED         ' '
#define CHIP_GL_GREEN       ' '
#define CHIP_GL_YELLOW      ' '
#define CHIP_GL_GRAPHITE    ' '

unsigned char chip_glyph(unsigned char chip);

/*
 * One of Google's eleven color ids in, an attribute with that color behind
 * a blank out.
 *
 * The MS-DOS backend gives all eleven their own CGA ink. Three attribute bits
 * of background is eight slots, two of which are the page and the chrome, so
 * eleven do not fit: they quantize onto seven, which is still two more than
 * the VDG build's five and keeps Grape off Blueberry and Tangerine off Tomato.
 */
unsigned char ink_attr(unsigned char color);

/* ui_geom()'s runtime geometry, fixed here because the width is. The values
   are the MS-DOS backend's 80-column arm. */
#define right_col   (SCR_COLS - 2)

/*
 * No category column. GC_KEEP_CAT costs 960 bytes to hold the field and this
 * build has no such room -- see the ceiling arithmetic in the Makefile. w_cat
 * of zero is the flag views3.c already branches on to skip it, the Atari's
 * arrangement, so the title simply starts where the category would have.
 */
#define col_cat     0
#define w_cat       0
#define col_title   10
#define w_title     (SCR_COLS - col_title)
#define wk_ncol     12
#define wk_tcol     14
#define mo_cellw    11
#define mo_left     1
#define mo_bars     8

#else

#define SCR_COLS    32
#define SCR_ROWS    16
#define SCR_RAM     ((unsigned char *) 0x0400)

/* Space, normal video. Not zero -- see the header comment. */
#define SCR_BLANK   0x60

/* ------------------------------------------------------------------ */
/* Semigraphics-4                                                      */
/* ------------------------------------------------------------------ */

#define SG_GREEN        0
#define SG_YELLOW       1
#define SG_BLUE         2
#define SG_RED          3
#define SG_BUFF         4
#define SG_CYAN         5
#define SG_MAGENTA      6
#define SG_ORANGE       7

/* Quadrant mask bits, in the order the VDG reads them. */
#define Q_TL            0x08
#define Q_TR            0x04
#define Q_BL            0x02
#define Q_BR            0x01
#define Q_TOP           (Q_TL | Q_TR)
#define Q_BOT           (Q_BL | Q_BR)
#define Q_LEFT          (Q_TL | Q_BL)
#define Q_RIGHT         (Q_TR | Q_BR)
#define Q_ALL           0x0F

#define SG4(c, m)       ((unsigned char) (0x80 | ((c) << 4) | (m)))
#define SG_SOLID(c)     SG4(c, Q_ALL)

/* Every colour's empty cell looks the same, so the black gutter and the black
   rules are one constant. */
#define SG_BLACK        SG4(SG_GREEN, 0)

/*
 * Google's four brand colours land on four of the eight the VDG has, and
 * Graphite lands on buff. That is exactly the five chips color.c quantises its
 * eleven colour names onto, with nothing left over and nothing missing.
 */
#define CHIP_NONE       0xFF
unsigned char chip_sg(unsigned char chip);

#endif /* COCO3 */

/* ------------------------------------------------------------------ */
/* Band geometry                                                       */
/* ------------------------------------------------------------------ */

/*
 * Rows 0-1 header, 2-14 content, 15 footer.
 *
 * The Atari and the Apple both spend three rows on the header, the third being
 * a tab strip. Sixteen rows will not pay for one: the window title on row 0
 * already says which view is up, and the footer teaches the digit keys in the
 * space a static legend would have taken. The Intellivision made the same
 * trade at twelve rows and put its hints on a second page.
 */
#define HDR_ROWS        2
#define FOOT_ROW        (SCR_ROWS - 1)
#define CONTENT_TOP     HDR_ROWS
#define CONTENT_ROWS    (FOOT_ROW - HDR_ROWS)

/* Header columns. The mark occupies cols 0-3 on both header rows. */
#define LOGO_ROW        0
#ifdef COCO3
/* The GIME layout is the MS-DOS backend's: the mark at cols 1-4 over the
   three header rows, and the text clear of it. */
#define LOGO_COL        1
#define HDR_TEXT_COL    6
#else
#define LOGO_COL        0
#define HDR_TEXT_COL    5
#endif
#define RIGHT_COL       (SCR_COLS - 1)

/* ------------------------------------------------------------------ */
/* screen.c -- the blitter                                             */
/* ------------------------------------------------------------------ */

void scr_clear(void);
void scr_row_clear(unsigned char row);
void scr_rows_clear(unsigned char first, unsigned char last);

/* Write s into [col, col+width), space padded and truncated to fit. */
void scr_field(unsigned char row, unsigned char col, const char *s,
               unsigned char width, unsigned char inv);
void scr_text(unsigned char row, unsigned char col, const char *s,
              unsigned char inv);
void scr_right(unsigned char row, unsigned char rcol, const char *s,
               unsigned char inv);
void scr_center(unsigned char row, const char *s, unsigned char inv);

#ifdef COCO3

/* A glyph and an explicit attribute. The GIME build carries the MS-DOS
   backend's shapes, because its painters are that backend's. */
void scr_cell(unsigned char row, unsigned char col, unsigned char glyph,
              unsigned char attr);
void scr_fill(unsigned char row, unsigned char col, unsigned char glyph,
              unsigned char width, unsigned char attr);

/* Resolving a role is the identity here -- see screen.c. */
unsigned char scr_attr_byte(unsigned char attr);

#else

/* Raw byte access, for SG4. scr_fill writes one byte across a run, which is
   what draws the chip gutter, the black rules and the density bars.
   (The obvious parameter name `byte` is a typedef in <coco.h>.) */
void scr_cell(unsigned char row, unsigned char col, unsigned char v);
void scr_fill(unsigned char row, unsigned char col, unsigned char v,
              unsigned char width);

#endif

/* Flip a run of characters between normal and inverse, leaving SG4 bytes
   alone -- on those, XOR $40 would change the color. On the GIME build there
   is no such hazard and this just restyles the run's attributes. */
void scr_run_inv(unsigned char row, unsigned char col, unsigned char width,
                 unsigned char inv);


/* ------------------------------------------------------------------ */
/* logo.c -- the Google Calendar mark                                  */
/* ------------------------------------------------------------------ */

/*
 * An SG4 quadrant is 4 pixels wide and 6 scanlines tall, so a cell is twice as
 * tall as it is wide in block terms and both marks are wider than they are
 * high in cells.
 *
 * The "31" cannot be printed inside the mark the way the Atari and the Apple
 * print it -- text on this machine is green, never white. It is punched out of
 * the buff page as unlit (black) quadrants instead, which works because within
 * one cell SG4 gives exactly one colour plus a 2x2 on/off mask.
 */
#define LOGO_SMALL_COLS 4
#define LOGO_LARGE_COLS 8
#define LOGO_LARGE_ROWS 6

#ifdef COCO3
/* Three rows, because the GIME build's header band is three rows and a
   two-row mark would leave the third of them empty beside it. */
#define LOGO_SMALL_ROWS 3
#else
#define LOGO_SMALL_ROWS 2
#endif

void logo_small(unsigned char row, unsigned char col);
void logo_large(unsigned char row, unsigned char col);

/* ------------------------------------------------------------------ */
/* ui.c -- chrome shared with views.c                                  */
/* ------------------------------------------------------------------ */

#ifdef COCO3
void ui_header(unsigned char view);     /* rows 0-2 plus the mark */
void ui_status(void);                   /* row 2 alone */
void ui_footer(const char *hints, const char *right);
#else
void ui_header(unsigned char view);     /* rows 0-1 plus the mark */
void ui_status(void);                   /* row 1 alone */
void ui_footer(const char *hints);
#endif
void ui_hhmm(char *dst, unsigned char h, unsigned char m);

#endif /* COCO_PLATFORM_H */
