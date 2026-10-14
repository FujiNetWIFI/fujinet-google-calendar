/*
 * The name table shadow, the PPU queue's writers, the attributes, the
 * palette and the sprites. See nes.h for the model.
 *
 * Everything here is in the fixed bank: every painter in every overlay
 * calls it, and a string it is handed may live in whichever overlay bank is
 * mapped -- which stays mapped, because nothing here switches banks.
 */

#include <string.h>

#include "../gcal.h"
#include "nes.h"

uint8_t scr_buf[SCR_H * SCR_W];
uint8_t scr_attr[64];
uint8_t scr_full;
uint8_t scr_splash;             /* the screen being drawn shows the large mark */
uint8_t theme_dark;
uint8_t scr_gen;                /* counts full repaints: "is my screen up?" */

static uint8_t splash_on;       /* the splash CHR bank is in R1 */
static uint8_t *draw_oam;       /* the sprite page being written */

/* ------------------------------------------------------------------ */
/* The queue                                                           */
/* ------------------------------------------------------------------ */

/* Wait for room for n bytes. The drains free it in the blank -- or, while
   the display is off (the boot, before the first screen), this does. */
static void q_room(uint8_t n)
{
    while ((uint8_t) (qtail - qhead - 1) < n)
        if (!ppu_on)
            ppu_drain(255);
}

static uint8_t qh;

static void q_byte(uint8_t b)
{
    ppuq[qh++] = b;
}

/* A command is written past qhead and published by one store, so a drain
   never sees half of one. */
static void q_begin(uint8_t op, uint16_t addr, uint8_t n, uint8_t room)
{
    q_room(room);
    qh = qhead;
    q_byte(op);
    q_byte((uint8_t) (addr >> 8));
    q_byte((uint8_t) addr);
    q_byte(n);
}

static void q_end(void)
{
    qhead = qh;
}

static void q_copy(uint16_t addr, const uint8_t *src, uint8_t n)
{
    q_begin(Q_COPY, addr, n, (uint8_t) (n + 4));
    while (n--)
        q_byte(*src++);
    q_end();
}

static void q_fill(uint16_t addr, uint8_t v, uint8_t n)
{
    q_begin(Q_FILL, addr, n, 5);
    q_byte(v);
    q_end();
}

static void q_op(uint8_t op, uint8_t a, uint8_t b)
{
    q_room(3);
    qh = qhead;
    q_byte(op);
    if (op == Q_CHR) {
        q_byte(a);
        q_byte(b);
    }
    q_end();
}

void scr_sync(void)
{
    while (qtail != qhead)
        if (!ppu_on)
            ppu_drain(255);
}

/* The name table on show, and the one being drawn. */
static uint16_t nt_front(void)
{
    return (ppu_ctrl & 1) ? 0x2400 : 0x2000;
}

/* Send part of one row from the shadow to the shown name table. */
static void send_run(uint8_t row, uint8_t col, uint8_t n)
{
    uint16_t off = (uint16_t) row * SCR_W + col;

    if (scr_full || !n)
        return;
    q_copy(nt_front() + off, scr_buf + off, n);
}

void scr_send(uint8_t row, uint8_t col, uint8_t n)
{
    send_run(row, col, n);
}

void scr_hold(void)
{
    scr_full = 2;               /* not a full repaint: nothing to flip */
}

void scr_flush(uint8_t first, uint8_t last)
{
    uint8_t r;

    scr_full = 0;
    for (r = first; r <= last; r++)
        send_run(r, 0, SCR_W);
}

static void send_attrs(void)
{
    if (!scr_full)
        q_copy(nt_front() + 0x3C0, scr_attr, 64);
}

/* ------------------------------------------------------------------ */
/* Full repaints                                                       */
/* ------------------------------------------------------------------ */

void scr_init(void)
{
    memset(scr_buf, 0, sizeof(scr_buf));
    memset(scr_attr, 0, sizeof(scr_attr));
    draw_oam = oam0;
    scr_theme();
}

void scr_begin(void)
{
    scr_sync();
    scr_full = 1;
    scr_gen++;
    scr_splash = 0;
    spin_hide();
    memset(scr_buf, 0, sizeof(scr_buf));
    memset(scr_attr, 0, sizeof(scr_attr));
    draw_oam = (oam_page == (uint8_t) ((uint16_t) oam0 >> 8)) ? oam1 : oam0;
    memset(draw_oam, 0xF8, 256);
}

/*
 * Send the shadow to the hidden name table and flip. The first full screen
 * also turns the display on: until then the PPU is off (crt0.s), so the
 * boot's half-written name tables are never seen.
 */
void scr_end(void)
{
    uint16_t back = (ppu_ctrl & 1) ? 0x2000 : 0x2400;
    uint16_t off;

    for (off = 0; off < SCR_H * SCR_W; off += Q_MAX)
        q_copy(back + off, scr_buf + off, Q_MAX);
    q_copy(back + 0x3C0, scr_attr, 64);

    if (scr_splash != splash_on) {
        splash_on = scr_splash;
        q_op(Q_CHR, 1, (uint8_t) (splash_on ? 8 : 2));
    }
    q_op(Q_FLIP, 0, 0);

    if (!ppu_on) {
        ppu_drain(255);
        ppu_wait_on();
    }
    scr_sync();

    scr_full = 0;
    draw_oam = (oam_page == (uint8_t) ((uint16_t) oam0 >> 8)) ? oam0 : oam1;
}

/*
 * A whole screen kept aside and put back: the START menu's pages (help,
 * about) are full screens of their own, and coming back from one must not
 * cost the view a refetch -- or even a repaint from data the view's painter
 * would have to be asked for again. SCR_SAVE_SIZE bytes: the shadow, the
 * attributes and the shown sprite page.
 */
void scr_save(uint8_t *buf)
{
    scr_sync();
    memcpy(buf, scr_buf, sizeof(scr_buf));
    memcpy(buf + sizeof(scr_buf), scr_attr, sizeof(scr_attr));
    memcpy(buf + sizeof(scr_buf) + sizeof(scr_attr), draw_oam, 256);
}

void scr_restore(const uint8_t *buf)
{
    uint8_t gen = scr_gen;

    scr_begin();
    memcpy(scr_buf, buf, sizeof(scr_buf));
    memcpy(scr_attr, buf + sizeof(scr_buf), sizeof(scr_attr));
    memcpy(draw_oam, buf + sizeof(scr_buf) + sizeof(scr_attr), 256);
    scr_end();
    scr_gen = gen;              /* the screen is the one its painter left */
}

/* ------------------------------------------------------------------ */
/* Text                                                                */
/* ------------------------------------------------------------------ */

static uint8_t hl_on(uint8_t t);

/* A character's tile. Codes below $20 in a string are the icon tiles
   (T_* in nes.h) -- nothing off the wire reaches here unsanitised, and
   copy_san() turns its control bytes into spaces -- so a legend or a help
   line can carry the pad's buttons in-line. */
static uint8_t glyph(char c, uint8_t style)
{
    uint8_t g = (uint8_t) c;

    if (g < 0x20)
        return style ? hl_on(g) : g;
    if (g > 0x7E)
        g = ' ';
    return (uint8_t) (g | style);
}

void scr_field(uint8_t row, uint8_t col, const char *s, uint8_t w, uint8_t style)
{
    uint8_t *p = scr_buf + (uint16_t) row * SCR_W + col;
    uint8_t i;

    if (col + w > SCR_W)
        w = (uint8_t) (SCR_W - col);
    for (i = 0; i < w; i++)
        p[i] = glyph(*s ? *s++ : ' ', style);
    send_run(row, col, w);
}

void scr_text(uint8_t row, uint8_t col, const char *s, uint8_t style)
{
    uint8_t *p = scr_buf + (uint16_t) row * SCR_W + col;
    uint8_t n = 0;

    while (*s && col + n < SCR_W)
        p[n++] = glyph(*s++, style);
    send_run(row, col, n);
}

void scr_center(uint8_t row, const char *s, uint8_t style)
{
    uint8_t n = (uint8_t) strlen(s);

    if (n > SCR_W)
        n = SCR_W;
    scr_text(row, (uint8_t) ((SCR_W - n) / 2), s, style);
}

/* Right-aligned so the last character lands on rcol. */
void scr_right(uint8_t row, uint8_t rcol, const char *s, uint8_t style)
{
    uint8_t n = (uint8_t) strlen(s);

    if (n > rcol + 1)
        n = (uint8_t) (rcol + 1);
    scr_text(row, (uint8_t) (rcol + 1 - n), s, style);
}

void scr_tile(uint8_t row, uint8_t col, uint8_t t)
{
    scr_buf[(uint16_t) row * SCR_W + col] = t;
    send_run(row, col, 1);
}

void scr_fill(uint8_t row, uint8_t col, uint8_t t, uint8_t n)
{
    uint16_t off = (uint16_t) row * SCR_W + col;

    if (col + n > SCR_W)
        n = (uint8_t) (SCR_W - col);
    memset(scr_buf + off, t, n);
    if (!scr_full && n)
        q_fill(nt_front() + off, t, n);
}

void scr_rows_clear(uint8_t first, uint8_t last)
{
    uint8_t r;

    for (r = first; r <= last; r++)
        scr_fill(r, 0, T_BLANK, SCR_W);
}

void scr_rule(uint8_t row)
{
    scr_fill(row, LEFT_COL, T_RULE, TEXT_W);
}

/* The selection bar: the same characters on the other ground. */
static uint8_t hl_on(uint8_t t)
{
    if (t >= 0x20 && t < 0x7F)
        return (uint8_t) (t | HL);
    switch (t) {
    case T_BLANK:   return (uint8_t) (' ' | HL);
    case T_PREV:    return T_HL_PREV;
    case T_NEXT:    return T_HL_NEXT;
    case T_BULLET:  return T_HL_BULLET;
    }
    return t;
}

static uint8_t hl_off(uint8_t t)
{
    if (t >= 0xA0 && t < 0xFF)
        return (uint8_t) (t & 0x7F);
    switch (t) {
    case T_HL_PREV:   return T_PREV;
    case T_HL_NEXT:   return T_NEXT;
    case T_HL_BULLET: return T_BULLET;
    case T_HL_BLANK:  return T_BLANK;
    }
    return t;
}

void scr_hl(uint8_t row, uint8_t col, uint8_t n, uint8_t on)
{
    uint8_t *p = scr_buf + (uint16_t) row * SCR_W + col;
    uint8_t i;

    for (i = 0; i < n; i++)
        p[i] = on ? hl_on(p[i]) : hl_off(p[i]);
    send_run(row, col, n);
}

/* ------------------------------------------------------------------ */
/* Attributes and the palette                                          */
/* ------------------------------------------------------------------ */

void scr_pal(uint8_t row, uint8_t col, uint8_t w, uint8_t h, uint8_t pal)
{
    uint8_t r, c, shift, i;

    for (r = row; r < row + h; r += 2) {
        for (c = col; c < col + w; c += 2) {
            i = (uint8_t) ((r >> 2) * 8 + (c >> 2));
            shift = (uint8_t) (((r >> 1) & 1) * 4 + ((c >> 1) & 1) * 2);
            scr_attr[i] = (uint8_t) ((scr_attr[i] & ~(3 << shift)) | (pal << shift));
        }
    }
    send_attrs();
}

void scr_theme(void)
{
    memcpy(pal_buf, theme_dark ? pal_dark : pal_light, 32);
    pal_dirty = 1;
}

/* The alarm band (BP_ALERT, highlight characters: colour 1 on colour 2)
   flashes by trading its two colours. */
void scr_alert_flash(uint8_t phase)
{
    const uint8_t *p = theme_dark ? pal_dark : pal_light;

    pal_buf[9] = p[phase ? 9 : 10];
    pal_buf[10] = p[phase ? 10 : 9];
    pal_dirty = 1;
}

/* ------------------------------------------------------------------ */
/* Sprites                                                             */
/* ------------------------------------------------------------------ */

void spr_put(uint8_t i, uint8_t x, uint8_t y, uint8_t tile, uint8_t attr)
{
    uint8_t *o = draw_oam + (i << 2);

    o[0] = (uint8_t) (y - 1);           /* the PPU shows a sprite a line late */
    o[1] = tile;
    o[2] = attr;
    o[3] = x;
}

void spr_hide(uint8_t first, uint8_t last)
{
    uint8_t i;

    for (i = first; i <= last; i++)
        draw_oam[i << 2] = 0xF8;
}

/* An event colour (COL_*, COL_NONE included) in one of the shapes. */
void spr_color(uint8_t i, uint8_t x, uint8_t y, uint8_t shape, uint8_t color)
{
    if (color > COL_NONE)
        color = COL_NONE;
    spr_put(i, x, y, (uint8_t) (shape + col_slot[color]), col_pal[color]);
}

void spr_logo_small(uint8_t row, uint8_t col)
{
    const uint8_t *s = logo_small_spr;
    uint8_t n = *s++, i;

    scr_tile(row, col, T_LOGO);
    scr_tile(row, (uint8_t) (col + 1), T_LOGO + 1);
    scr_tile((uint8_t) (row + 1), col, T_LOGO + 2);
    scr_tile((uint8_t) (row + 1), (uint8_t) (col + 1), T_LOGO + 3);
    scr_pal(row, col, 2, 2, BP_MARK);

    for (i = 0; i < n; i++, s += 3)
        spr_put((uint8_t) (SPR_LOGO + i), (uint8_t) (col * 8 + s[1]),
                (uint8_t) (row * 8 + s[0]), s[2], SP_BRAND);
}

/* The large mark: 6x6 tiles out of the splash bank, its coloured edges as
   sprites. Only inside a full repaint, which switches the bank in. */
void logo_big(uint8_t row, uint8_t col)
{
    const uint8_t *s = logo_big_spr;
    uint8_t n = *s++, r, c, i;

    scr_splash = 1;
    for (r = 0; r < 6; r++)
        for (c = 0; c < 6; c++)
            scr_buf[(uint16_t) (row + r) * SCR_W + col + c] = logo_big_map[r * 6 + c];
    /* The attribute blocks round it: from the even column at or before it to
       the one past its end, which is ground on both sides. */
    scr_pal(row, (uint8_t) (col & 0xFE), (uint8_t) (((col + 7) & 0xFE) - (col & 0xFE)),
            6, BP_MARK);

    for (i = 0; i < n; i++, s += 3)
        spr_put((uint8_t) (SPR_CONTENT + i), (uint8_t) (col * 8 + s[1]),
                (uint8_t) (row * 8 + s[0]), s[2], SP_BRAND);
}

/* The cursor bracket round a box of tiles. */
void spr_cursor(uint8_t row, uint8_t col, uint8_t w, uint8_t h)
{
    uint8_t x0 = (uint8_t) (col * 8 - 2), y0 = (uint8_t) (row * 8 - 2);
    uint8_t x1 = (uint8_t) ((col + w) * 8 - 6), y1 = (uint8_t) ((row + h) * 8 - 6);

    spr_put(SPR_CURSOR + 0, x0, y0, S_CUR_TL, SP_BLUE);
    spr_put(SPR_CURSOR + 1, x1, y0, S_CUR_TR, SP_BLUE);
    spr_put(SPR_CURSOR + 2, x0, y1, S_CUR_BL, SP_BLUE);
    spr_put(SPR_CURSOR + 3, x1, y1, S_CUR_BR, SP_BLUE);
}
