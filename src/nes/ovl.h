/*
 * The overlays' entry points, for tramp.c and nothing else: calling one of
 * these from anywhere but a trampoline runs whatever bank happens to be in
 * the window (see tramp.c).
 */

#ifndef OVL_H
#define OVL_H

#include <stdint.h>

/* The portable core, renamed into its overlays by the Makefile. */
unsigned char bk_gc_fetch_index(unsigned char view);
unsigned char bk_gc_fetch_detail(unsigned char view, const char *evnum);
unsigned char bk_gc_fetch_cals(void);
unsigned char bk_gc_save_begin(unsigned char editing, unsigned char view,
                               const char *evnum);
unsigned char bk_gc_save_end(void);
void          bk_form_put(const char *line);
unsigned char bk_compose_new(unsigned int y, unsigned char mo, unsigned char d);
unsigned char bk_compose_edit(unsigned char view, unsigned char ev);

/* views.c, bank 1 */
void v_view(unsigned char view, unsigned char sel, unsigned char first);
void v_view_sel(unsigned char view, unsigned char from, unsigned char to,
                unsigned char first);
void v_hints(unsigned char view);
void v_alarm(unsigned char phase);

/* grid.c, bank 2 */
void g_view(unsigned char view, unsigned char sel, unsigned char first);
void g_view_sel(unsigned char view, unsigned char from, unsigned char to,
                unsigned char first);

/* detail_ui.c, bank 3 */
void d_detail(unsigned char ev, unsigned int top);
void d_pick(unsigned char sel, unsigned char first);
void d_setup(void);
void d_setup_lead(void);
uint8_t d_setup_key(uint8_t k);

/* screens.c, bank 6 */
void b_splash(void);
void b_notfound(void);
void b_noclock(void);
void b_busy(unsigned char reason);
void b_error(unsigned char code);
uint8_t b_menu(void);

/* pickers.c, bank 5: run the picker for a form field, starting from
   value[] (the field's text, which it rewrites on PK_SET). */
#define PK_SET      1           /* value[] holds the new value */
#define PK_NEXT     2           /* on to the next field, unchanged */
#define PK_DONE     3           /* leave the form */
#define PK_TYPE     4           /* type this one on the keyboard instead */
#define PK_KEY      5           /* a Famicom key is waiting */
#define PK_PREV     6           /* back to the field before */
uint8_t k_pick(uint8_t field, char *value);

/* tramp.c's doors to the backend's own overlays */
uint8_t setup_key(uint8_t k);
uint8_t menu_run(void);
uint8_t pick_run(uint8_t field, char *value);

#endif /* OVL_H */
