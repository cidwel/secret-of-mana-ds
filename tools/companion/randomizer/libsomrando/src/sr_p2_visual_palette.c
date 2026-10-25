/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49.  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * CharacterPaletteRandomizer (processing/hacks/common/util/CharacterPaletteRandomizer.cs); author Moppleton.  Translated to C 2026-10-18.
 * The "custom" mode reads CustomCharColor* settings that the settings table never holds (the oracle fails on them), so it stays refused.
 */
#include <math.h>
#include <string.h>

#include "sr_core.h"
#include "sr_p2_visual_color.h"

struct sr_w2_pal_def {
  int n;
  uint8_t c[4][3];
};
#include "sr_p2_visual_paldata.h"

/* a mutable palette list (List<SnesColor>) */
struct sr_w2_pal {
  int n;
  struct sr_w2_color k[4];
};

static void pal_from_def(struct sr_w2_pal *p, const struct sr_w2_pal_def *d) {
  p->n = d->n;
  for (int i = 0; i < d->n; i++) p->k[i] = sr_w2_color_rgb(d->c[i][0], d->c[i][1], d->c[i][2]);
}

static double rgb_to_hue(int r, int g, int b) {
  if (r == g && g == b) return 0;
  int max = r > g ? (r > b ? r : b) : (g > b ? g : b);
  int min = r < g ? (r < b ? r : b) : (g < b ? g : b);
  double diff = max - min, h;
  if (r == max) h = (g - b) / diff;
  else if (g == max) h = 2 + (b - r) / diff;
  else h = 4 + (r - g) / diff;
  h *= 60;
  if (h < 0) h += 360;
  return h;
}

static int to_int32_round(double v) { return (int)rint(v); }

static void hsv_to_rgb(double hue, double saturation, double value, int *r, int *g, int *b) {
  int hue_index = to_int32_round(floor(hue / 60)) % 6;
  double f = hue / 60 - floor(hue / 60);
  if (saturation < 0) saturation = 0;
  if (saturation > 1) saturation = 1;
  value = value * 255;
  int v = sr_clamp_to_endpoints(to_int32_round(value), 0, 255);
  int p = sr_clamp_to_endpoints(to_int32_round(value * (1 - saturation)), 0, 255);
  int q = sr_clamp_to_endpoints(to_int32_round(value * (1 - f * saturation)), 0, 255);
  int t = sr_clamp_to_endpoints(to_int32_round(value * (1 - (1 - f) * saturation)), 0, 255);
  switch (hue_index) {
    case 0: *r = v; *g = t; *b = p; break;
    case 1: *r = q; *g = v; *b = p; break;
    case 2: *r = p; *g = v; *b = t; break;
    case 3: *r = p; *g = q; *b = v; break;
    case 4: *r = t; *g = p; *b = v; break;
    default: *r = v; *g = p; *b = q; break;
  }
}

static void random_hue_shift(struct sr_ctx *c, struct sr_w2_pal *p) {
  double hue_change = ((sr_random_next(&c->rc) % 300) - 150) / 10.0;
  for (int i = 0; i < p->n; i++) {
    struct sr_w2_color *k = &p->k[i];
    int r = sr_w2_get_red(k), g = sr_w2_get_green(k), b = sr_w2_get_blue(k);
    int max = r > g ? (r > b ? r : b) : (g > b ? g : b);
    int min = r < g ? (r < b ? r : b) : (g < b ? g : b);
    double h = rgb_to_hue(r, g, b);
    double s = (max == 0) ? 0 : 1.0 - (1.0 * min / max);
    double v = max / 255.0;
    h += hue_change;
    int nr, ng, nb;
    hsv_to_rgb(h, s, v, &nr, &ng, &nb);
    sr_w2_set_red(k, nr);
    sr_w2_set_green(k, ng);
    sr_w2_set_blue(k, nb);
  }
}

static void set_colors(struct sr_ctx *c, const struct sr_w2_pal *p, int palette_offset, int sprite_index) {
  for (int i = 0; i < p->n; i++) {
    int off = 0x80FFE + sprite_index * 0x1E + (palette_offset + i) * 2;
    sr_poke(c, off, p->k[i].b[0]);
    sr_poke(c, off + 1, p->k[i].b[1]);
  }
}

static struct sr_w2_pal add_brightness(const struct sr_w2_pal *p, int amount) {
  struct sr_w2_pal q;
  q.n = p->n;
  for (int i = 0; i < p->n; i++) {
    q.k[i] = sr_w2_color_rgb(sr_w2_get_red(&p->k[i]), sr_w2_get_green(&p->k[i]), sr_w2_get_blue(&p->k[i]));
    sr_w2_set_nonbit(&q.k[i], sr_w2_get_nonbit(&p->k[i]));
    sr_w2_color_add(&q.k[i], amount, amount, amount);
  }
  return q;
}

/* setBoyColors / setGirlColors / setSpriteColors: the three differ in the sprite ids, the extra copy of girl/sprite and the poison exception. */
static void set_char_colors(struct sr_ctx *c, struct sr_w2_pal *p, int offset, int normal, int shrunk, int extra, int dead, int poison, int skin_offset,
                            int flash1, int flash2, int flash3) {
  random_hue_shift(c, p);
  set_colors(c, p, offset, normal);
  set_colors(c, p, offset, shrunk);
  if (extra >= 0) set_colors(c, p, offset, extra);
  struct sr_w2_pal q = add_brightness(p, 64);
  set_colors(c, &q, offset, dead);
  if (offset != skin_offset) set_colors(c, p, offset, poison);
  q = add_brightness(p, 16);
  set_colors(c, &q, offset, flash1);
  q = add_brightness(p, 32);
  set_colors(c, &q, offset, flash2);
  q = add_brightness(p, 48);
  set_colors(c, &q, offset, flash3);
}

static void set_boy(struct sr_ctx *c, struct sr_w2_pal *p, int offset) {
  set_char_colors(c, p, offset, 0x80, 0xF6, -1, 0xE4, 0xE7, 6, 0x59, 0x58, 0x57);
}
static void set_girl(struct sr_ctx *c, struct sr_w2_pal *p, int offset) {
  set_char_colors(c, p, offset, 0x81, 0xF7, 0x8B, 0xE5, 0xE8, 6, 0x5C, 0x5B, 0x5A);
}
static void set_sprite(struct sr_ctx *c, struct sr_w2_pal *p, int offset) {
  set_char_colors(c, p, offset, 0x82, 0xF8, 0x8C, 0xE6, 0xE9, 5, 0x5F, 0x5E, 0x5D);
}

#define PICK(arr, n) ((arr)[sr_random_next(&c->rc) % (n)])

static void randomize_boy(struct sr_ctx *c) {
  int skin_index = sr_random_next(&c->rc) % BOY_SKINCOLORS_N;
  const struct sr_w2_pal_def *hair = PICK(boy_hairColors, BOY_HAIRCOLORS_N);
  const struct sr_w2_pal_def *skin = boy_skinColors[skin_index];
  const struct sr_w2_pal_def *bandana = PICK(boy_bandanaColors, BOY_BANDANACOLORS_N);
  const struct sr_w2_pal_def *pants = PICK(boy_pantsColors, BOY_PANTSCOLORS_N);
  const struct sr_w2_pal_def *shoes = PICK(boy_shoesColors, BOY_SHOESCOLORS_N);
  const struct sr_w2_pal_def *eye_white = boy_eyeWhiteColors[skin_index];
  struct sr_w2_pal p;
  pal_from_def(&p, hair); set_boy(c, &p, 2);
  pal_from_def(&p, skin); set_boy(c, &p, 6);
  pal_from_def(&p, bandana); set_boy(c, &p, 8);
  pal_from_def(&p, pants); set_boy(c, &p, 10);
  pal_from_def(&p, shoes); set_boy(c, &p, 13);
  pal_from_def(&p, eye_white); set_boy(c, &p, 15);
}

static void randomize_girl(struct sr_ctx *c) {
  int skin_index = sr_random_next(&c->rc) % GIRL_SKINCOLORS_N;
  const struct sr_w2_pal_def *hair = PICK(girl_hairColors, GIRL_HAIRCOLORS_N);
  const struct sr_w2_pal_def *skin = girl_skinColors[skin_index];
  const struct sr_w2_pal_def *shoes = PICK(girl_shoesColors, GIRL_SHOESCOLORS_N);
  const struct sr_w2_pal_def *pants = PICK(girl_pantsColors, GIRL_PANTSCOLORS_N);
  const struct sr_w2_pal_def *eye = PICK(girl_eyeColors, GIRL_EYECOLORS_N);
  const struct sr_w2_pal_def *eye_white = girl_eyeWhiteColors[skin_index];
  struct sr_w2_pal p;
  pal_from_def(&p, hair); set_girl(c, &p, 2);
  pal_from_def(&p, skin); set_girl(c, &p, 6);
  pal_from_def(&p, shoes); set_girl(c, &p, 8);
  pal_from_def(&p, pants); set_girl(c, &p, 10);
  pal_from_def(&p, &girl_graySpecular); set_girl(c, &p, 13);
  pal_from_def(&p, eye); set_girl(c, &p, 14);
  pal_from_def(&p, eye_white); set_girl(c, &p, 15);
}

static void randomize_sprite(struct sr_ctx *c) {
  int hair_index = sr_random_next(&c->rc) % SPRITE_HAIRCOLORS1_N;
  const struct sr_w2_pal_def *hair1 = sprite_hairColors1[hair_index];
  const struct sr_w2_pal_def *skin = PICK(sprite_skinColors, SPRITE_SKINCOLORS_N);
  const struct sr_w2_pal_def *hair2 = sprite_hairColors2[hair_index];
  const struct sr_w2_pal_def *shirt = PICK(sprite_shirtColors, SPRITE_SHIRTCOLORS_N);
  struct sr_w2_pal p;
  pal_from_def(&p, hair1); set_sprite(c, &p, 2);
  pal_from_def(&p, skin); set_sprite(c, &p, 5);
  pal_from_def(&p, hair2); set_sprite(c, &p, 8);
  pal_from_def(&p, shirt); set_sprite(c, &p, 10);
}

int sr_hack_character_palette(struct sr_ctx *c) {
  const char *mode = sr_settings_get(c->settings, "characterColors");
  if (!strcmp(mode, "rando")) {
    randomize_boy(c);
    randomize_girl(c);
    randomize_sprite(c);
    return 1;
  }
  if (!strcmp(mode, "custom")) sr_fail(c, SOMRANDO_ERR_UNSUPPORTED, "unsupported option: characterColors=custom");
  return 0;
}
