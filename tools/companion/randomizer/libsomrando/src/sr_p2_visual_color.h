/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49.  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * SnesColor (processing/common/structure/SnesColor.cs) and ColorUtil (util/ColorUtil.cs); author Moppleton.  Translated to C 2026-10-18.
 */
#ifndef SR_P2_VISUAL_COLOR_H
#define SR_P2_VISUAL_COLOR_H

#include <math.h>
#include <stdint.h>

#include "sr_core.h"

/* 16-bit snes color, b[0] = GGGRRRRR, b[1] = xBBBBBGG */
struct sr_w2_color {
  uint8_t b[2];
};

static inline int sr_w2_clamp255(int v) { return v > 255 ? 255 : (v < 0 ? 0 : v); }

static inline int sr_w2_get_red(const struct sr_w2_color *k) { return (uint8_t)((k->b[0] & 0x1F) * 8); }
static inline int sr_w2_get_green(const struct sr_w2_color *k) {
  int g1 = (k->b[0] & 0xE0) >> 5, g2 = k->b[1] & 0x03;
  return (uint8_t)((g1 + (g2 << 3)) * 8);
}
static inline int sr_w2_get_blue(const struct sr_w2_color *k) { return (uint8_t)(((k->b[1] & 0x7C) >> 2) * 8); }
static inline int sr_w2_get_nonbit(const struct sr_w2_color *k) { return (k->b[1] & 0x80) > 0; }

static inline void sr_w2_set_red(struct sr_w2_color *k, int red) {
  int v = (uint8_t)red / 8;
  k->b[0] = (uint8_t)((k->b[0] & ~0x1F) | (uint8_t)v);
}
static inline void sr_w2_set_green(struct sr_w2_color *k, int green) {
  int v = (uint8_t)green / 8;
  k->b[0] = (uint8_t)((k->b[0] & ~0xE0) | (uint8_t)((v & 0x07) << 5));
  k->b[1] = (uint8_t)((k->b[1] & ~0x03) | (uint8_t)(v >> 3));
}
static inline void sr_w2_set_blue(struct sr_w2_color *k, int blue) {
  int v = (uint8_t)blue / 8;
  k->b[1] = (uint8_t)((k->b[1] & ~0x7C) | (uint8_t)(v << 2));
}
static inline void sr_w2_set_nonbit(struct sr_w2_color *k, int on) { k->b[1] = (uint8_t)((k->b[1] & ~0x80) | (on ? 0x80 : 0)); }

static inline struct sr_w2_color sr_w2_color_rgb(int r, int g, int b) {
  struct sr_w2_color k = {{0, 0}};
  sr_w2_set_red(&k, sr_w2_clamp255(r));
  sr_w2_set_green(&k, sr_w2_clamp255(g));
  sr_w2_set_blue(&k, sr_w2_clamp255(b));
  return k;
}
static inline void sr_w2_color_add(struct sr_w2_color *k, int r, int g, int b) {
  int rn = sr_w2_clamp255(sr_w2_get_red(k) + r), gn = sr_w2_clamp255(sr_w2_get_green(k) + g), bn = sr_w2_clamp255(sr_w2_get_blue(k) + b);
  sr_w2_set_red(k, rn);
  sr_w2_set_green(k, gn);
  sr_w2_set_blue(k, bn);
}

#endif
