/*
 * Apple //e (enhanced) backend -- internal interface shared by the files in
 * this directory. The portable half of the program talks to us only through
 * the plat_* / ui_* declarations in ../gcal.h.
 *
 * Target is cc65's apple2enh: 80-column text, the alternate character set, and
 * MouseText. There is no colour here at all -- 80-column text is one bit per
 * pixel -- so everything the Atari backend does with player/missile graphics
 * and display list interrupts is done with inverse video and glyphs instead.
 */

#ifndef APPLE2ENH_PLATFORM_H
#define APPLE2ENH_PLATFORM_H

#define SCR_COLS    80
#define SCR_ROWS    24

/* ------------------------------------------------------------------ */
/* Character set                                                       */
/* ------------------------------------------------------------------ */

/*
 * With ALTCHARSET on -- cc65's conio constructor does that for us -- the
 * enhanced //e character generator reads:
 *
 *   $00-$1F inverse uppercase    $20-$3F inverse symbols
 *   $40-$5F MouseText            $60-$7F inverse lowercase
 *   $80-$FF normal ASCII + $80
 *
 * so an inverse space is $20, a solid white cell. That is what the selection
 * bars, the chrome bands and the month density bars are all made of, exactly
 * as the Atari backend uses its own inverse spaces.
 *
 * MouseText has no ASCII to sit on, so the blitter steals the control range:
 * a byte $01-$1F in a string means MouseText glyph $40 + byte. Nothing from
 * the wire can collide with that, because copy_san() clamps every field it
 * copies to $20-$7E before it ever reaches a painter -- so only string
 * literals in this directory can produce one.
 *
 * Spelled as octal escapes on purpose: "\x1B" followed by a hex digit is one
 * escape, not two characters, and these end up inside longer hint strings.
 */
#define MT_HOURGLASS    "\003"
#define MT_CHECK        "\004"
#define MT_LEFT         "\010"
#define MT_DOTS         "\011"
#define MT_DOWN         "\012"
#define MT_UP           "\013"
#define MT_TOPRULE      "\014"
#define MT_RETURN       "\015"
#define MT_BLOCK        "\016"
#define MT_RULE         "\023"
#define MT_CORNER_BL    "\024"
#define MT_RIGHT        "\025"
#define MT_DITHER_A     "\026"
#define MT_DITHER_B     "\027"
#define MT_FOLDER_L     "\030"
#define MT_FOLDER_R     "\031"
#define MT_VRULE_R      "\032"
#define MT_DIAMOND      "\033"
#define MT_TWORULES     "\034"
#define MT_VRULE_L      "\037"

/*
 * The five colour chips, as glyphs.
 *
 * This is the one thing the Atari backend does that has no equivalent here:
 * it gives each event a coloured block in the gutter, and 80-column text has
 * no colour to give. Shapes stand in, chosen to differ at 7x8 rather than to
 * mean anything -- there is no monochrome glyph that reads as "Peacock". What
 * actually carries the information on this screen is the category column next
 * to them, which the Atari has no room for; the chips are the at-a-glance
 * grouping on top of it, and the settings screen legends them.
 */
#define CHIP_GLYPH_BLUE     MT_DIAMOND
#define CHIP_GLYPH_RED      MT_DITHER_B
#define CHIP_GLYPH_GREEN    MT_RULE
#define CHIP_GLYPH_YELLOW   MT_TWORULES
#define CHIP_GLYPH_GRAPHITE MT_BLOCK

/* One CHIP_* value in, one glyph string out. CHIP_NONE gives a space. */
#define CHIP_NONE   0xFF

const char *chip_glyph(unsigned char chip);

/* ------------------------------------------------------------------ */
/* Band geometry                                                       */
/* ------------------------------------------------------------------ */

/*
 * Rows 0-2 header, 3-22 content, 23 footer -- the same split the Atari uses,
 * so the shared screen logic in main.c does not know the difference. There the
 * three bands are three colours from a display list interrupt; here rows 0 and
 * 23 are painted inverse and the content between them is not.
 */
#define HDR_ROWS        3
#define FOOT_ROW        (SCR_ROWS - 1)
#define CONTENT_TOP     HDR_ROWS
#define CONTENT_ROWS    (FOOT_ROW - HDR_ROWS)

/* ------------------------------------------------------------------ */
/* screen.c / screen.s -- direct blitter over the 80-column text page  */
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

/* A run of one repeated glyph -- rules, density bars, chrome bands. */
void scr_fill(unsigned char row, unsigned char col, const char *glyph,
              unsigned char width, unsigned char inv);

/* ------------------------------------------------------------------ */
/* logo.c -- the Google Calendar mark                                  */
/* ------------------------------------------------------------------ */

/*
 * A white page with "31" on it, which on this machine means an inverse block
 * with inverse digits inside it. Two sizes, matching the Atari's two: the
 * small one sits in the header, the large one on the splash and busy screens.
 */
#define LOGO_SMALL_COLS 4
#define LOGO_SMALL_ROWS 3
#define LOGO_LARGE_COLS 14
#define LOGO_LARGE_ROWS 6

void logo_small(unsigned char row, unsigned char col);
void logo_large(unsigned char row, unsigned char col);

/* ------------------------------------------------------------------ */
/* ui.c -- chrome shared with views.c                                  */
/* ------------------------------------------------------------------ */

void ui_header(unsigned char view);     /* rows 0-2 plus the mark */
void ui_status(void);                   /* row 2 alone */
void ui_footer(const char *hints, const char *right);
void ui_hhmm(char *dst, unsigned char h, unsigned char m);

#endif /* APPLE2ENH_PLATFORM_H */
