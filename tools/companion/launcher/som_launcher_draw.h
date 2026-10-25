/* Renderer of the start menu (see som_launcher.h).  Pixels are 0xAARRGGBB, stride in pixels; any size from 320x240 to 1920x1080. */
#ifndef SOM_LAUNCHER_DRAW_H
#define SOM_LAUNCHER_DRAW_H

#include <stddef.h>
#include <stdint.h>
#include "som_launcher.h"

/* Optional graphics pack (som_ui_pack.bin of tools/companion/ux; the memory must outlive the renderer).  Without it a built-in face is used. */
int sl_draw_load_pack(const uint8_t *blob, size_t len);   /* 0 ok, -1 not a pack */
void sl_draw_free_pack(void);
void sl_draw(const struct sl_launcher *L, uint32_t *px, int stride, int w, int h);
/* The top screen: wallpaper on the main menu, otherwise a summary of the game being set up (see som_launcher_draw.c). */
void sl_draw_top(const struct sl_launcher *L, uint32_t *px, int stride, int w, int h);
int sl_draw_audit(int reset);                           /* texts cut or wider than their box since the last reset (tests) */
const char *sl_draw_audit_note(void);
int sl_draw_ellipsized(int reset);                      /* rows whose label or value was shortened with ".." since the last reset */

#endif
