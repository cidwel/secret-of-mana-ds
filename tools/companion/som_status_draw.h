/* som_status_draw: platform-neutral bottom-screen (320x240) renderer and touch layout for the SoM companion.
 *
 * Draws into a 0x00RRGGBB buffer, so every frontend only converts pixels.  The touch layout lives here too:
 * som_ui_touch() turns a tap into a som_cmd for som_macro_start().  Depends only on som_companion.h.
 * Shared by the Android and 3DS frontends; the Python renderer (render_bottom.py) is the visual reference.
 */
#ifndef SOM_STATUS_DRAW_H
#define SOM_STATUS_DRAW_H

#include <stdint.h>
#include "som_companion.h"

#define SOM_UI_W 320
#define SOM_UI_H 240

enum som_ui_tab { SOM_TAB_WEAPON = 0, SOM_TAB_ITEM, SOM_TAB_MAGIC };

struct som_ui {
  uint8_t tab;      /* enum som_ui_tab */
  uint8_t hero;     /* selected caster / ring owner: hero slot 0..2 */
  uint8_t spirit;   /* magic tab: chosen spirit slot, 0xFF while choosing a spirit */
};

void som_ui_init(struct som_ui *ui);
/* Renders the whole screen.  px has SOM_UI_H rows of `stride` pixels (stride >= SOM_UI_W).  m may be NULL. */
void som_ui_draw(const struct som_state *s, const struct som_ui *ui, const struct som_macro *m,
                 uint32_t *px, int stride);
/* Handles a tap at (x, y) in 320x240 coordinates.  Returns 1 and fills *out when a macro should be started
 * (the caller checks som_macro_busy() first), else 0 after updating the UI selection. */
int som_ui_touch(struct som_ui *ui, const struct som_state *s, int x, int y, struct som_cmd *out);

/* Debug text (5x7 font, upper case) on top of a rendered screen, for frame-time read-outs. */
void som_ui_text(uint32_t *px, int stride, int x, int y, const char *s, uint32_t col);

#endif
