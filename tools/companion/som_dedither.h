/* Horizontal de-dither of the field frame (Thor round 3c, item A).  The game draws its window background and some textures as a
 * pixel-by-pixel checkerboard that a CRT blends; unblended it looks noisy.  Where three luma steps in a row alternate in sign with an
 * amplitude of at least SOM_DD_T (48: the rock wall texture, 14-40 luma, is left alone; the window checker is 100+), the pixel gets the 1-2-1 horizontal mix of its neighbours.  Strokes narrower than 4 px (font stems,
 * outlines) never alternate three times, so text stays sharp.  Pure function over 0x00BBGGRR / 0x00RRGGBB pixels, any channel order. */
#ifndef SOM_DEDITHER_H
#define SOM_DEDITHER_H
#include <stdint.h>
#include <stdlib.h>

#ifndef SOM_DD_T
#define SOM_DD_T 48
#endif
#define SOM_DD_WHITE 224   /* a pixel next to near-white (text glyphs) is left alone */

static inline int som_dd_luma(uint32_t p) { return (int)(((p & 255) * 77 + ((p >> 8) & 255) * 150 + ((p >> 16) & 255) * 29) >> 8); }

/* Filters rows of w pixels in place; `row` has a stride of `stride` pixels; returns the number of pixels changed. */
static inline int som_dedither_rows(uint32_t *px, int w, int h, int stride)
{
  int changed = 0;
  int Lb[1024];
  uint32_t ob[1024];
  if (w < 6 || w > 1024) return 0;
  for (int y = 0; y < h; y++) {
    uint32_t *r = px + (size_t)y * stride;
    for (int x = 0; x < w; x++) { Lb[x] = som_dd_luma(r[x]); ob[x] = r[x]; }
    for (int x = 2; x < w - 2; x++) {
      int d0 = Lb[x] - Lb[x - 1], d1 = Lb[x + 1] - Lb[x], dm = Lb[x - 1] - Lb[x - 2], dp = Lb[x + 2] - Lb[x + 1];
      int a = (d0 > SOM_DD_T && d1 < -SOM_DD_T) || (d0 < -SOM_DD_T && d1 > SOM_DD_T);
      if (!a) continue;
      int ap = (d1 > SOM_DD_T && dp < -SOM_DD_T) || (d1 < -SOM_DD_T && dp > SOM_DD_T);
      int am = (dm > SOM_DD_T && d0 < -SOM_DD_T) || (dm < -SOM_DD_T && d0 > SOM_DD_T);
      if (!ap && !am) continue;
      if (Lb[x - 1] >= SOM_DD_WHITE || Lb[x] >= SOM_DD_WHITE || Lb[x + 1] >= SOM_DD_WHITE) continue;
      uint32_t o = 0;
      for (int s = 0; s < 24; s += 8) {
        uint32_t v = (((ob[x - 1] >> s) & 255) + 2 * ((ob[x] >> s) & 255) + ((ob[x + 1] >> s) & 255) + 2) >> 2;
        o |= v << s;
      }
      r[x] = (ob[x] & 0xFF000000u) | o;
      changed++;
    }
  }
  return changed;
}
#endif
