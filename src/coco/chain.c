/*
 * The compose form as a separate program.
 *
 * The CoCo 3 client runs out of address space before it runs out of features:
 * the 80-column layout is the MS-DOS backend's, which costs about 1.7K more
 * code than the 32-column one, and the compose form upstream added costs 3.5K
 * on top. Everything that could be moved to the second bank has been, and the
 * arithmetic still does not close.
 *
 * So the form is GCALED3.BIN and this is the seam. compose_new() and
 * compose_edit() have the signatures main.c already calls; they write what the
 * editor needs into the second bank and hand over with the same RUNM poke the
 * disk's model-picker uses. The editor writes its result back and chains home,
 * and chain_resume() puts the screen where it was.
 *
 * That works because block $37 survives a LOADM: Super Extended BASIC declares
 * it unused and never references it, HDB-DOS never programs the MMU at all, so
 * nothing but this program can address it. Verified on hardware before this
 * file was written.
 *
 * None of it exists on the 1/2, which has the form in-process like every other
 * backend -- this whole file is behind GC_CHAIN_EDIT.
 */

#ifdef COCO3

#include <cmoc.h>
#include <coco.h>
#include <string.h>

#include "../gcal.h"
#include "platform.h"

/*
 * The same poke-and-jump the disk's loader uses: put M"<name>" in BASIC's
 * direct-mode buffer, point its input pointer at it, and enter RUNM. There is
 * no return -- the ROM loads over us and runs what it loaded.
 *
 * If the file is missing the ROM stops with its own error and BASIC is left at
 * a prompt, which is why the caller paints a line saying what it was doing
 * before calling this.
 */
void chain_run(const char *binary)
{
    *((unsigned short *) 0x02DD) = 0x4D22;      /* M"                    */
    strcpy((char *) 0x02DF, binary);
    *((unsigned short *) 0x00A6) = 0x02DD;

    asm
    {
        ldd     #$4D1C
        jmp     $AE75
    }
}

void chain_save(const struct chainstate *st)
{
    far_put(FAR_STATE, st, sizeof(struct chainstate));
}

void chain_load(struct chainstate *st)
{
    far_get(st, FAR_STATE, sizeof(struct chainstate));
}

void chain_clear(void)
{
    struct chainstate st;

    memset(&st, 0, sizeof(st));
    chain_save(&st);
}

#ifdef GC_CHAIN_EDIT

/* The line that stands on screen while the ROM loads the other binary, so a
   missing GCALED3.BIN reads as a failed step rather than a hang. */
static void say_loading(void)
{
    scr_field(FOOT_ROW, 0, "", SCR_COLS, A_FOOT);
    scr_text(FOOT_ROW, 2, "Loading the editor...", A_FOOT);
}

static unsigned char go(unsigned char mode, unsigned char vw, unsigned char ev,
                        unsigned int y, unsigned char mo, unsigned char d,
                        unsigned char sel, unsigned char first)
{
    struct chainstate st;

    memset(&st, 0, sizeof(st));
    st.magic0 = 'G';
    st.magic1 = 'C';
    st.mode   = mode;
    st.view   = vw;
    st.sel    = sel;
    st.first  = first;
    st.y      = y;
    st.mo     = mo;
    st.d      = d;
    st.ev     = ev;

    if (mode == GC_CHAIN_EDIT_M) {
        st.rec = gc_index[ev];
        strcpy(st.title, ev_title(ev));
    }

    chain_save(&st);
    say_loading();
    chain_run("GCALED3");

    return 0;                   /* not reached unless the load failed */
}

/*
 * main.c passes the anchor date for a new event and the view plus record for
 * an edit, exactly as the in-process versions take them. sel and first are not
 * in those signatures, so they are read back out of the resume path instead --
 * see chain_resume(), which the editor's reply carries them through.
 */
unsigned char compose_new(unsigned int y, unsigned char mo, unsigned char d)
{
    return go(GC_CHAIN_NEW, 0xFF, 0, y, mo, d, 0, 0);
}

unsigned char compose_edit(unsigned char view, unsigned char ev)
{
    return go(GC_CHAIN_EDIT_M, view, ev, cur_y, cur_mo, cur_d, 0, 0);
}

unsigned char chain_resume(unsigned char *vw, unsigned char *sel,
                           unsigned char *first)
{
    struct chainstate st;

    chain_load(&st);
    if (st.magic0 != 'G' || st.magic1 != 'C' || !st.returning)
        return 0;

    if (st.view != 0xFF)
        *vw = st.view;
    *sel   = st.sel;
    *first = st.first;
    cur_y  = st.y;
    cur_mo = st.mo;
    cur_d  = st.d;

    chain_clear();
    return st.changed ? 1 : 0;
}

#endif /* GC_CHAIN_EDIT */

#endif /* COCO3 */
