/* Host test of source/sui.c: navigation rules and a rendering to PPM files (argv[1] = output prefix, optional). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../source/sui.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void ppm(const char *path, const uint32_t *px, int w, int h) {
  FILE *f = fopen(path, "wb");
  if (!f) return;
  fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (int i = 0; i < w * h; i++) { fputc(px[i] >> 16, f); fputc(px[i] >> 8, f); fputc(px[i], f); }
  fclose(f);
}

int main(int argc, char **argv) {
  static struct sui_list l;
  static struct sui_text t;
  static uint32_t top[400 * 240], bot[320 * 240], thumb[SUI_THUMB_W * SUI_THUMB_H];
  sui_list_clear(&l, "Pause");
  sui_list_add(&l, SUI_ROW_HEADER, "STATE", "", "");
  sui_list_add(&l, SUI_ROW_ITEM, "Save state", "slot 1", "Writes the whole game to the SD card.");
  sui_list_add(&l, SUI_ROW_DISABLED, "Load state", "empty", "");
  sui_list_add(&l, SUI_ROW_ITEM, "Video filter", "scanlines", "Scanlines darken every other output line. Smooth blends neighbouring pixels (slower on the Old 3DS).");
  for (int i = 0; i < 20; i++) { char b[64]; snprintf(b, sizeof b, "Row %d with a rather long label to cut", i); sui_list_add(&l, SUI_ROW_ITEM, b, "value", "x"); }
  l.sel = 1;
  sui_list_move(&l, 1);
  CHECK(l.sel == 3);                      /* skips the disabled row */
  sui_list_move(&l, -1);
  CHECK(l.sel == 1);                      /* skips the header going up and wraps to the end */
  sui_list_move(&l, -1);
  CHECK(l.sel == l.n - 1);
  CHECK(l.scroll + sui_list_visible_rows() > l.sel);
  sui_list_move(&l, 1);
  CHECK(l.sel == 1 && l.scroll == 0);
  sui_list_page(&l, 1);
  CHECK(l.sel > 1 && l.rows[l.sel].kind == SUI_ROW_ITEM);
  for (int i = 0; i < SUI_THUMB_W * SUI_THUMB_H; i++) thumb[i] = (uint32_t)((i % SUI_THUMB_W) * 2) << 16 | (uint32_t)((i / SUI_THUMB_W) * 2) << 8 | 0x40;
  l.sel = 3; l.scroll = 0; l.thumb = thumb; snprintf(l.thumb_caption, sizeof l.thumb_caption, "2026-10-20 18:22");
  snprintf(l.hint, sizeof l.hint, "D-PAD move  LEFT RIGHT change  A select  B back");
  sui_draw_list(top, 400, &l);
  sui_draw_grid(bot, 320, 320, 240);
  if (argc > 1) { char p[256]; snprintf(p, sizeof p, "%s_list.ppm", argv[1]); ppm(p, top, 400, 240); snprintf(p, sizeof p, "%s_grid.ppm", argv[1]); ppm(p, bot, 320, 240); }

  sui_text_clear(&t, "Special thanks");
  sui_text_add(&t, "Moppleton and contributors", 33, 1, 0);
  sui_text_add(&t, "Secret of Mana Randomizer and its Ancient Cave mode (libsomrando is a C port of their generator)", 31, 0, 0);
  sui_text_add(&t, "https://github.com/Moppu/SecretOfManaRandomizer", 33, 0, 1);
  CHECK(t.n >= 6);
  for (int i = 0; i < t.n; i++) CHECK(strlen(t.line[i]) <= 33);
  sui_text_scroll(&t, 100);
  CHECK(t.scroll == 0);                   /* shorter than a page: no scrolling */
  for (int i = 0; i < 30; i++) sui_text_add(&t, "More text to make the page scroll.", 33, 0, 0);
  sui_text_scroll(&t, 5);
  CHECK(t.scroll == 5);
  sui_text_scroll(&t, 1000);
  CHECK(t.scroll == t.n - sui_text_visible_lines());
  t.scroll = 0;
  sui_draw_text(top, 400, &t);
  if (argc > 1) { char p[256]; snprintf(p, sizeof p, "%s_text.ppm", argv[1]); ppm(p, top, 400, 240); }
  printf(fails ? "test_sui: %d FAILED\n" : "test_sui: all passed\n", fails);
  return fails != 0;
}
