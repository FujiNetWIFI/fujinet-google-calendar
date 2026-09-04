/*
 * GCALED3 -- the CoCo 3 client's compose/edit form, as its own program.
 *
 * The main client has no room for the form (see src/coco/chain.c for the
 * arithmetic), so it writes what this needs into the second bank and hands
 * over. This restores that, runs the same form every other backend runs
 * in-process, writes the outcome back, and hands control home.
 *
 * Everything here is behind GC_EDITOR, which only the coco-edit build defines.
 * The main client compiles the same tree without it and gets main.c's main()
 * instead; the CoCo 1/2 and every other platform never see either.
 */

#ifdef GC_EDITOR

#include <cmoc.h>
#include <coco.h>
#include <string.h>

#include "../gcal.h"
#include "platform.h"

/*
 * Nothing was left in the far block, or someone typed LOADM"GCALED3" by hand.
 * There is nothing to edit and no state to honor, so go home rather than guess
 * at a date and calendar.
 */
static void go_home(void)
{
    chain_run("GCAL3");
}

int main(void)
{
    struct chainstate st;
    unsigned char     changed = 0;

    plat_init();
    chain_load(&st);

    if (st.magic0 != 'G' || st.magic1 != 'C' || st.returning) {
        go_home();
        return 0;
    }

    /*
     * The adapter is not re-tested here: the client checked it before chaining
     * and nothing has happened since but a disk load. If it has gone away, the
     * form's send reports it like any other device error.
     *
     * The clock is different -- every device spec names a date, and this
     * program did not inherit the one the client fetched.
     */
    ui_splash();
    ui_busy(BUSY_CLOCK);
    while (!clk_fetch()) {
        ui_noclock();
        plat_anykey();
        ui_busy(BUSY_CLOCK);
    }

    set_load();

    cur_y  = st.y;
    cur_mo = st.mo;
    cur_d  = st.d;

    if (st.mode == GC_CHAIN_NEW) {
        changed = compose_new(st.y, st.mo, st.d);
    } else {
        /*
         * compose_edit() reads the record out of gc_index, so put the one the
         * client sent back where it expects it. One entry is all it looks at,
         * and gc_count only has to be large enough that the index is valid.
         */
        gc_index[st.ev] = st.rec;
        ev_set_title(st.ev, st.title);
        if (gc_count <= st.ev)
            gc_count = (unsigned char) (st.ev + 1);
        changed = compose_edit(st.view, st.ev);
    }

    st.returning = 1;
    st.changed   = changed;
    chain_save(&st);

    scr_field(FOOT_ROW, 0, "", SCR_COLS, A_FOOT);
    scr_text(FOOT_ROW, 2, "Returning...", A_FOOT);
    go_home();
    return 0;
}

#endif /* GC_EDITOR */
