/*
 * Definitions of everything shared across the program.
 *
 * These live in their own file rather than in net.c so that the pure halves --
 * the listing parser, the colour matcher, the date arithmetic, the agenda
 * builder -- can be linked into tests/hosttest.c without dragging in
 * fujinet-lib and a 6502.
 */

#include "gcal.h"

struct event  gc_index[MAX_EVENTS];
unsigned char gc_count;
unsigned char gc_trunc;
char          gc_wtitle[41];

unsigned char gc_agd[AGD_MAX];
unsigned char gc_agd_count;

unsigned char gc_daycnt[32];
unsigned char gc_daychip[32];

struct cal    gc_cals[CAL_MAX];
unsigned char gc_cal_count;

unsigned int  cur_y;
unsigned char cur_mo;
unsigned char cur_d;

char          gc_cal[CAL_SEL_LEN];

unsigned char gc_ecode;
unsigned char gc_dev_ecode;
const char   *gc_stage;

/* The wall clock. The values live here, the fetch and tick live in clock.c,
   so a host test can stage a time and exercise the alarm scan. */
unsigned int  clk_y;
unsigned char clk_mo, clk_d, clk_h, clk_mi, clk_s;
unsigned char clk_ok;

unsigned char al_lead = AL_LEAD_DEFAULT;
unsigned char al_active;
unsigned char al_ev = AL_NONE;
