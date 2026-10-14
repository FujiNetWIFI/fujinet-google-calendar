/*
 * NES backend -- the interface the files in this directory share. The
 * portable half of the program talks to us only through the plat_* / ui_*
 * declarations in ../gcal.h.
 *
 * The screen is 32x30 tiles of 8x8 text on the NES's background, in CHR-RAM
 * (tools/nes/mkchr.py), with Google's colours in two places the background
 * cannot reach:
 *
 *   - Every event's own colour is a sprite. Four sprite palettes of three
 *     give twelve slots, and Google has eleven event colours, so each one is
 *     a (palette, tile) pair (col_pal[] / col_slot[], tools/nes/palette.py)
 *     and none of them is quantised -- the first backend since the Adam that
 *     shows all eleven.
 *   - The selection is not an attribute. The background's 16x16 attribute
 *     blocks are far too coarse for a list's row, so the font is in the
 *     pattern table twice: $20-$7E ink on the ground, $A0-$FE the same ink on
 *     the line grey. A selected row is the same characters with bit 7 set.
 *
 * The attributes carry what is coarse anyway: which of four background
 * palettes a 2x2-tile block uses (BP_*), and every screen is laid out so a
 * region of one palette starts on an even row and column.
 *
 * Drawing: everything goes through a shadow of the whole name table
 * (scr_buf) and a queue the interrupts drain in the blank (crt0.s). A full
 * repaint (scr_begin .. scr_end) builds the shadow from nothing, sends all of
 * it to the hidden name table and flips; between full repaints, every write
 * updates the shadow and goes straight to the shown one. Reading a row back
 * out of the shadow is how a selection bar moves without the caller knowing
 * what was under it.
 */

#ifndef NES_H
#define NES_H

#include <stdint.h>

#ifndef __CC65__
#define __fastcall__            /* for editors' C parsers */
#endif

/* ------------------------------------------------------------------ */
/* Geometry                                                            */
/* ------------------------------------------------------------------ */

#define SCR_W           32
#define SCR_H           30

/*
 * The rows every list screen shares. Rows 0-1 and 27-29 are outside what a
 * television shows reliably (and from line 215 down the scanline interrupt
 * blanks the display for the PPU queue's sake), and columns 0 and 31 are
 * the same, so nothing is written there but ground and the odd sprite.
 */
#define HDR_ROW         2       /* 2-3: the mark, the window, the clock */
#define TAB_ROW         4       /* the view tabs */
#define RULE_ROW        5
#define LIST_TOP        6       /* 6-19: LIST_ROWS (14) */
#define PANEL_RULE      20
#define PANEL_ROW       21      /* 21-22: the selection, spelled out */
#define STAT_ROW        24
#define LEGEND_ROW      25
#define LEFT_COL        1
#define RIGHT_COL       30      /* the last column written */
#define TEXT_W          (RIGHT_COL - LEFT_COL + 1)      /* 30 */

/* ------------------------------------------------------------------ */
/* Tiles (tools/nes/mkchr.py)                                          */
/* ------------------------------------------------------------------ */

#define T_BLANK         0x00
#define T_RULE          0x01
#define T_TRACK         0x02
#define T_THUMB         0x03
#define T_PREV          0x04
#define T_NEXT          0x05
#define T_UP            0x06
#define T_DOWN          0x07
#define T_REPEAT        0x08
#define T_ALLDAY        0x09
#define T_BELL          0x0A
#define T_CLOCK         0x0B
#define T_PIN           0x0C
#define T_TICK          0x0D
#define T_CAL           0x0E
#define T_BULLET        0x0F
#define T_BTN_A         0x10
#define T_BTN_B         0x11
#define T_PILL_L        0x12
#define T_PILL_R        0x13
#define T_PAD           0x14
#define T_K_DEL         0x15
#define T_K_SHIFT       0x16
#define T_K_LOCK        0x17
#define T_K_RET         0x18
#define T_K_SPACE       0x19
#define T_K_LEFT        0x1A
#define T_K_RIGHT       0x1B
#define T_ELLIPSIS      0x1C
#define T_KEY           0x1D
#define T_KEY_SHADOW    0x1E
#define T_ACCENT        0x7F
#define T_DISC_L        0x80    /* + tens digit 1-3 */
#define T_DISC_R2       0x84    /* + units digit: a two-digit day */
#define T_DISC_R1       0x8E    /* + digit: a one-digit day, centred */
#define T_HL_PREV       0x98
#define T_HL_NEXT       0x99
#define T_HL_BULLET     0x9A
#define T_HL_BLANK      0x9B
#define T_LOGO          0x9C    /* 2x2 */

#define HL              0x80    /* a character on the highlight ground */

/* Sprite tiles: a shape's three colour variants are +1..+3. */
#define S_DOT           0x00
#define S_SQUARE        0x04
#define S_MDOT1         0x08
#define S_MDOT2         0x0C
#define S_MDOT3         0x10
#define S_BAR           0x14
#define S_SPIN          0x18
#define S_SWATCH        0x1C
#define S_BELL          0x20
#define S_POINTER       0x24
#define S_CUR_TL        0x28
#define S_CUR_TR        0x29
#define S_CUR_BL        0x2A
#define S_CUR_BR        0x2B
#define S_WORD          0x30    /* G o o g l e */

/* Sprite attribute bytes: palette in bits 0-1. */
#define SP_BRAND        0       /* Tomato, Banana, Basil: red yellow green */
#define SP_BLUE         1       /* Peacock, Blueberry, Lavender */
#define SP_WARM         2       /* Flamingo, Tangerine, Sage */
#define SP_GREY         3       /* Grape, Graphite, white */

/* Background palettes (attributes). */
#define BP_BODY         0
#define BP_CHROME       1
#define BP_ALERT        2
#define BP_MARK         3

/* gen_pal.s: per COL_* (COL_NONE last) the sprite palette and slot. */
extern const unsigned char col_pal[];
extern const unsigned char col_slot[];
extern const unsigned char pal_light[32];
extern const unsigned char pal_dark[32];

/* gen_logo.s */
extern const unsigned char logo_big_map[36];
extern const unsigned char logo_big_spr[];
extern const unsigned char logo_small_spr[];

/* ------------------------------------------------------------------ */
/* crt0.s                                                              */
/* ------------------------------------------------------------------ */

extern uint8_t qhead, qtail, mmc3_sel, bank_cur, ppu_on, ppu_ctrl, oam_page;
extern uint8_t pal_dirty, net_busy;
extern volatile uint8_t pad_now, pad_down, pad_ack, kb_busy;   /* the NMI's pad */
#pragma zpsym ("qhead")
#pragma zpsym ("qtail")
#pragma zpsym ("mmc3_sel")
#pragma zpsym ("bank_cur")
#pragma zpsym ("ppu_on")
#pragma zpsym ("ppu_ctrl")
#pragma zpsym ("oam_page")
#pragma zpsym ("pal_dirty")
#pragma zpsym ("net_busy")
#pragma zpsym ("pad_now")
#pragma zpsym ("pad_down")
#pragma zpsym ("pad_ack")
#pragma zpsym ("kb_busy")

extern volatile uint8_t frames[4];
extern uint8_t oam0[256], oam1[256];
extern uint8_t pal_buf[32];
extern uint8_t ppuq[256];

void __fastcall__ bank_set(uint8_t bank);
void __fastcall__ ppu_drain(uint8_t budget);
void ppu_wait_off(void);
void ppu_wait_on(void);

#define Q_COPY          1
#define Q_FILL          2
#define Q_FLIP          3
#define Q_CHR           4
#define Q_MAX           64      /* bytes in one command */

/* ------------------------------------------------------------------ */
/* Banks (gcal-nes.cfg) and the trampolines (tramp.c)                  */
/* ------------------------------------------------------------------ */

#define BANK_NET        0
#define BANK_VIEWS      1
#define BANK_GRID       2
#define BANK_DETAIL     3
#define BANK_FORM       4
#define BANK_PICK       5
#define BANK_BOOT       6

/* Enter a bank, returning the one to go back to. */
uint8_t bank_enter(uint8_t b);
#define bank_leave(b)   bank_set(b)

/* ------------------------------------------------------------------ */
/* screen.c                                                            */
/* ------------------------------------------------------------------ */

extern uint8_t scr_buf[SCR_H * SCR_W];
extern uint8_t scr_attr[64];
extern uint8_t scr_full;        /* inside scr_begin .. scr_end */
extern uint8_t theme_dark;
extern uint8_t scr_gen;         /* bumped by every scr_begin */

void scr_init(void);
void scr_begin(void);
void scr_end(void);
void scr_sync(void);                    /* wait for the queue to empty */

#define SCR_SAVE_SIZE   (SCR_H * SCR_W + 64 + 256)
void scr_save(uint8_t *buf);
void scr_restore(const uint8_t *buf);

/* Text: `style` is 0 or HL, OR'd into every character. */
void scr_text(uint8_t row, uint8_t col, const char *s, uint8_t style);
void scr_field(uint8_t row, uint8_t col, const char *s, uint8_t w, uint8_t style);
void scr_center(uint8_t row, const char *s, uint8_t style);
void scr_right(uint8_t row, uint8_t rcol, const char *s, uint8_t style);
void scr_tile(uint8_t row, uint8_t col, uint8_t t);
void scr_fill(uint8_t row, uint8_t col, uint8_t t, uint8_t n);
void scr_rows_clear(uint8_t first, uint8_t last);
void scr_hl(uint8_t row, uint8_t col, uint8_t n, uint8_t on);
void scr_rule(uint8_t row);
void scr_send(uint8_t row, uint8_t col, uint8_t n);    /* shadow -> screen */

/* A batch: everything between scr_hold() and scr_flush() goes to the shadow
   only, and the flush sends rows first..last whole -- one queued copy a row,
   where a picker's month of single tiles would be a hundred commands. */
void scr_hold(void);
void scr_flush(uint8_t first, uint8_t last);

/* A rectangle of attribute blocks, in tiles (row, col, w, h all even). */
void scr_pal(uint8_t row, uint8_t col, uint8_t w, uint8_t h, uint8_t pal);

/* The palette: the theme into pal_buf, sent in the next blank. */
void scr_theme(void);
void scr_alert_flash(uint8_t phase);

/* Sprites. Indices: */
#define SPR_LOGO        0       /* 0-3: the header's mark */
#define SPR_CURSOR      4       /* 4-7: the bracket */
#define SPR_MISC        8       /* 8-11: bell, pointer */
#define SPR_SPIN        12      /* 12-15: the busy spinner (the NMI's) */
#define SPR_CONTENT     16      /* 16-63: chips, dots */
#define SPR_NONE        0xFF

void spr_put(uint8_t i, uint8_t x, uint8_t y, uint8_t tile, uint8_t attr);
void spr_hide(uint8_t first, uint8_t last);
void spr_color(uint8_t i, uint8_t x, uint8_t y, uint8_t shape, uint8_t color);
void spr_logo_small(uint8_t row, uint8_t col);
void spr_cursor(uint8_t row, uint8_t col, uint8_t w, uint8_t h);

/* The large mark on the splash and busy screens (its tiles live in the
   splash CHR bank, which scr_end switches in or out with the flip). */
extern uint8_t scr_splash;
void logo_big(uint8_t row, uint8_t col);

/* The busy spinner, animated by the NMI while a bus call blocks. */
void spin_show(uint8_t x, uint8_t y);
void spin_hide(void);

/* ------------------------------------------------------------------ */
/* input.c                                                             */
/* ------------------------------------------------------------------ */

/* What the buttons mean on the screen being shown (in_mode). */
#define M_VIEW          0
#define M_DETAIL        1
#define M_PICK          2
#define M_SETUP         3
#define M_OK            4       /* any button: plat_anykey's screens */
#define M_FORM          5       /* plat_getch (osk.c) */
#define M_MENU          6

extern uint8_t in_mode;
extern uint8_t cur_view;        /* tramp.c, for SELECT's cycle */
extern char    pick_ref[];      /* tramp.c: the START time END's picker uses */
extern char    stage_name[];    /* tramp.c: gc_stage, copied out of bank 0 */

/* The raw pad, read once a frame: */
#define J_A             0x80
#define J_B             0x40
#define J_SELECT        0x20
#define J_START         0x10
#define J_UP            0x08
#define J_DOWN          0x04
#define J_LEFT          0x02
#define J_RIGHT         0x01

extern uint8_t pad, pad_new;    /* held, and newly pressed with repeats */
extern uint8_t kb_type;         /* FUJI_NES_KBD_* */
extern char    kb_ch;           /* this frame's character, 0 if none */
extern uint8_t kb_fn;           /* this frame's F-key 1-8, 9 STOP, 0 none */

void in_init(void);
void in_poll(void);             /* once a frame, from plat_vsync */
void in_flush(void);
void in_push(uint8_t k);        /* a K_* / E_* code ahead of real input */
uint8_t in_pop(void);

/* ------------------------------------------------------------------ */
/* sound.c / sfx.s                                                     */
/* ------------------------------------------------------------------ */

#define SFX_CLICK       0
#define SFX_TICK        1
#define SFX_ERROR       2
#define SFX_CHIME0      3
#define SFX_CHIME1      4
#define SFX_CHIME2      5
#define SFX_JINGLE      6
#define SFX_SAVED       7
#define SFX_OPEN        8

void snd_init(void);
void snd_play(uint8_t id);

/* ------------------------------------------------------------------ */
/* prefs.c -- this backend's own settings, in appkey 1                 */
/* ------------------------------------------------------------------ */

extern uint8_t pref_dark, pref_24h, pref_clicks;
void prefs_load(void);
void prefs_save(void);

/* ------------------------------------------------------------------ */
/* chrome.c (resident): what every screen shares                       */
/* ------------------------------------------------------------------ */

extern char sbuf[64];

void ui_header(const char *title, const char *sub);
void ui_tabs(uint8_t view);
void ui_legend(const char *s);  /* the button legend row: see chrome.c */
void ui_status_text(const char *s);
const char *ui_err_text(uint8_t code);
void ui_hhmm(char *dst, uint8_t h, uint8_t m);
void ui_time_range(char *dst, const struct event *e);
void num(char *dst, unsigned int v);
void date_line(char *dst, unsigned int y, uint8_t mo, uint8_t d);
void clock_paint(void);

#endif /* NES_H */
