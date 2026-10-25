/* Start-menu pages of the Thor app that sit next to the shared launcher (tools/companion/launcher): the gameplay tweaks page (options of
 * tools/gameplay/gameplay_menu.json, applied to the ROM by som_gameplay_apply.c when a game starts) and the credits page (text of data/credits.json).
 * Both are pad-navigable and touch-operable, drawn full-window with the 5x7 face of pc_gameplay.c.  No Android dependency: the harness in
 * android/tests/test_start_pages.c renders them on the desktop.  Pure C99. */
#ifndef SOM_START_PAGES_H
#define SOM_START_PAGES_H

#include <stddef.h>
#include <stdint.h>

enum ssp_page { SSP_NONE = 0, SSP_GAMEPLAY, SSP_CREDITS };
/* Same numbering as enum sl_button of the launcher. */
enum ssp_button { SSP_UP = 0, SSP_DOWN, SSP_LEFT, SSP_RIGHT, SSP_A, SSP_B, SSP_X, SSP_Y, SSP_L, SSP_R, SSP_START, SSP_SELECT };
enum ssp_hit { SSP_H_NONE = 0, SSP_H_GAMEPLAY, SSP_H_CREDITS };

/* Loads the option list (the menu file) and the saved values (the file ssp_gameplay_json wrote; may be NULL).  Options the C applier does not know
 * are not listed.  Returns the number of options shown, -1 when the menu is not valid. */
int ssp_gameplay_load(const char *menu_json, const char *values_json);
/* {"options": {...}} with the values that differ from the defaults ("{\"options\":{}}" for none); returns the length or -1. */
int ssp_gameplay_json(char *out, size_t n);
int ssp_gameplay_default(void);                 /* 1 when every option is at its default */
/* Takes the credits as plain text: lines separated by \n, "# " starts a heading (built from data/credits.json by the host). */
void ssp_set_credits(const char *text);
/* Flattens data/credits.json (title, entries with who / author, what / patch, license, url, any other string field) into that text; length or -1. */
int ssp_credits_text(const char *json, char *out, size_t n);
void ssp_set_credits_json(const char *json);

void ssp_open(int page);
void ssp_close(void);
int ssp_page(void);
uint32_t ssp_rev(void);                         /* changes whenever the picture would */
int ssp_dirty(int clear);                       /* the gameplay values changed since the last call with clear = 1 */
/* Returns 1 when the page consumed the button.  B (or the BACK button) closes the page. */
int ssp_key(int button);
int ssp_tap(int x, int y, int w, int h);
void ssp_scroll(int rows);
void ssp_draw(uint32_t *px, int stride, int w, int h);

/* Footer of the main screen: Gameplay and Credits buttons over the bottom strip, opened by a tap, by X / Y, or by moving the focus down to them
 * (focus 1 or 2 outlines that button).  ssp_footer_hit tells what a tap opened. */
void ssp_draw_footer(uint32_t *px, int stride, int w, int h, int gameplay_active, int focus);
int ssp_footer_hit(int x, int y, int w, int h);

#endif
