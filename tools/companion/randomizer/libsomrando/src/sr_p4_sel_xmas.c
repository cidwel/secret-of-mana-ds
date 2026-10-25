/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Open World Christmas changes.  Derived from (Author: Moppleton for all of them), translated to C 2026-10-18:
 *   processing/openworld/OpenWorldXmasHacks.cs, processing/hacks/openworld/{OpenWorldWinterTheme,XmasMapPalettes}.cs,
 *   processing/hacks/common/other/XmasDrops.cs
 * The colour and palette tables come from tools/gen_p4_xmas.py (sr_p4_sel_xmas_data.h).
 */
#include <math.h>
#include <string.h>

#include "sr_p2_visual_color.h"
#include "sr_p4_sel_int.h"
#include "sr_p4_sel_xmas_data.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))
#define COUNT(a) ((int)(sizeof(a) / sizeof *(a)))

static void put_color(struct sr_ctx *c, struct sr_w2_color k, int off) {
  P(off, k.b[0]);
  P(off + 1, k.b[1]);
}

static struct sr_w2_color get_color(struct sr_ctx *c, const uint8_t *rom, int off) {
  (void)c;
  struct sr_w2_color k;
  k.b[0] = rom[off];
  k.b[1] = rom[off + 1];
  return k;
}

/* ColorUtil.rgbToHsv / hsvToRgb */
static void rgb_to_hsv(int r, int g, int b, double *hue, double *sat, double *val) {
  int max = r > g ? (r > b ? r : b) : (g > b ? g : b);
  int min = r < g ? (r < b ? r : b) : (g < b ? g : b);
  if (r == g && g == b) {
    *hue = 0;
  } else {
    double diff = max - min, h;
    if (r == max) h = (g - b) / diff;
    else if (g == max) h = 2 + (b - r) / diff;
    else h = 4 + (r - g) / diff;
    h *= 60;
    if (h < 0) h += 360;
    *hue = h;
  }
  *sat = (max == 0) ? 0 : 1.0 - (1.0 * min / max);
  *val = max / 255.0;
}

static int round_to_int(double v) { return (int)rint(v); }

static void hsv_to_rgb(double hue, double saturation, double value, int *r, int *g, int *b) {
  int hue_index = round_to_int(floor(hue / 60)) % 6;
  double f = hue / 60 - floor(hue / 60);
  if (saturation < 0) saturation = 0;
  if (saturation > 1) saturation = 1;
  value = value * 255;
  int v = sr_clamp_to_endpoints(round_to_int(value), 0, 255);
  int p = sr_clamp_to_endpoints(round_to_int(value * (1 - saturation)), 0, 255);
  int q = sr_clamp_to_endpoints(round_to_int(value * (1 - f * saturation)), 0, 255);
  int t = sr_clamp_to_endpoints(round_to_int(value * (1 - (1 - f) * saturation)), 0, 255);
  switch (hue_index) {
    case 0: *r = v; *g = t; *b = p; break;
    case 1: *r = q; *g = v; *b = p; break;
    case 2: *r = p; *g = v; *b = t; break;
    case 3: *r = p; *g = q; *b = v; break;
    case 4: *r = t; *g = p; *b = v; break;
    default: *r = v; *g = p; *b = q; break;
  }
}

/* ---------------------------------------------------------------------------------------------------- OpenWorldWinterTheme */
static void winter_theme(struct sr_ctx *c) {
  for (int i = 0; i < COUNT(sr_p4_xm_enemy_names); i++) sr_names_set(c, NAMES_INDEX_ENEMIES_START + sr_p4_xm_enemy_names[i].idx, sr_p4_xm_enemy_names[i].name);
  for (int i = 0; i < COUNT(sr_p4_xm_enemy_colors); i++) {
    const struct sr_p4_xm_col *k = &sr_p4_xm_enemy_colors[i];
    put_color(c, sr_w2_color_rgb(k->r, k->g, k->b), 0x80FFE + k->key * 0x1E + k->idx * 2);
  }
  for (int i = 0; i < (int)sizeof sr_p4_xm_giftbox; i++) P(i + 0x186540, sr_p4_xm_giftbox[i]);

  /* npc palette 6A (object EA = palette EB) */
  for (int i = 0; i < 15; i++) {
    int a = 0x80FFE + 0xEA * 0x1E + i * 2, b = 0x80FFE + 0xEB * 0x1E + i * 2;
    P(a, sr_peek(c, b));
    P(a + 1, sr_peek(c, b + 1));
  }

  /* world map x30 and xba/ca/da/ea landable in ice country region (or all) */
  for (int i = 0; i < 16; i++) {
    int base = 0x67600 + i * 0x18;
    P(base + 6, sr_peek(c, base + 6) & 0xFE);
    P(base + 23, sr_peek(c, base + 23) & 0xFB);
    P(base + 14, sr_peek(c, base + 14) & 0xFE);
    P(base + 13, sr_peek(c, base + 13) & 0x7F);
    P(base + 16, sr_peek(c, base + 16) & 0xFD);
    P(base + 14, sr_peek(c, base + 14) & 0xFB);
  }
  P(0x90A4, 0x08);
  P(0x90B5, 0x00);

  /* santa house */
  P(0x69784, 0x72);
  int outsideMapNum = 332, outsideX = 13, outsideY = 16;
  P(0x677F2, outsideMapNum);
  P(0x677F3, (outsideMapNum >> 8) | (outsideX << 1));
  P(0x677F4, outsideY << 1);
  P(0x677F5, outsideMapNum);
  P(0x677F6, (outsideMapNum >> 8) | (outsideX << 1));
  P(0x677F7, outsideY << 1);
  P(0x67b18, 0xA8);
  P(0x67b19, 0x1B);

  static const int worldMapPalOffsets[2] = {0x67DFE, 0x6483E};
  for (int w = 0; w < 2; w++) {
    int off0 = worldMapPalOffsets[w];
    for (int palnum = 0; palnum < 13; palnum++) {
      for (int col = 1; col < 16; col++) {
        int at = off0 + palnum * 30 + col * 2;
        struct sr_w2_color thisCol = get_color(c, c->orig, at);
        for (int k = COUNT(sr_p4_xm_world_colors) - 1; k >= 0; k--) {
          const struct sr_p4_xm_col *m = &sr_p4_xm_world_colors[k];
          if (m->key == palnum && m->idx == col) {
            thisCol = sr_w2_color_rgb(m->r, m->g, m->b);
            break;
          }
        }
        if (palnum == 1 || palnum == 2 || palnum == 3) sr_w2_color_add(&thisCol, -192, -192, -192);
        else sr_w2_color_add(&thisCol, -64, -64, -64);
        if (off0 == 0x67DFE) {
          put_color(c, thisCol, at);
        } else {
          int src = 0x67DFE + palnum * 30 + col * 2;
          P(at, sr_peek(c, src));
          P(at + 1, sr_peek(c, src + 1));
        }
      }
    }
  }
}

/* ------------------------------------------------------------------------------------------------------- XmasMapPalettes */
static int gray_hit(int pal, int palnum, int col) { return palnum < 8 && (sr_p4_xm_gray[pal][palnum] >> col & 1); }

static int gray_plus(int paletteSet, int palnum, int max, int anim) {
  int small;
  if (!anim) {
    small = paletteSet == 71 || paletteSet == 108 || paletteSet == 27 || paletteSet == 110 || paletteSet == 57 || paletteSet == 74 || paletteSet == 40 ||
            paletteSet == 49 || paletteSet == 37 || (paletteSet == 103 && (palnum == 4 || palnum == 5));
  } else {
    small = paletteSet == 71 || paletteSet == 27 || paletteSet == 74 || paletteSet == 40 || paletteSet == 49 || paletteSet == 37 || paletteSet == 57;
  }
  max += small ? 32 : 64;
  return sr_clamp_to_endpoints(max, 0, 248);
}

static void set_gray(struct sr_w2_color *k, int max) {
  sr_w2_set_red(k, (uint8_t)max);
  sr_w2_set_green(k, (uint8_t)max);
  sr_w2_set_blue(k, (uint8_t)max);
}

static int max3(int a, int b, int c) { return a > b ? (a > c ? a : c) : (b > c ? b : c); }

static void xmas_map_palettes(struct sr_ctx *c) {
  uint8_t *rom = c->out;
  struct sr_random *random = &c->rc;
  double prevH[128] = {0}, prevS[128] = {0}, prevV[128] = {0};
  const int palStartOffset = 0xC7FE0;
  const double blueHue = 230;
  const double satThreshold = 0.3;

  for (int paletteSet = 24; paletteSet < 120; paletteSet++) {
    double hueMax = 250;
    double hChange = (fmod((double)sr_random_next(random), hueMax * 2) / 1.0) - hueMax;
    double sChange = 0.0, vChange = 0.0;
    if (!sr_p4_xm_dontDarkenPalettes[paletteSet]) {
      sChange = +0.1;
      vChange = -0.4;
    }
    double hueChange = 0.9;
    double hueShift = 0.0;
    for (int d = 0; d < COUNT(sr_p4_xm_disp); d++) {
      if (sr_p4_xm_disp[d][0] != paletteSet) continue;
      int dispSetNum = sr_p4_xm_disp[d][1];
      int at = 0x80100 + dispSetNum * 9 + 7;
      struct sr_w2_color thisCol = get_color(c, rom, at);
      if (paletteSet == 54 || paletteSet == 104 || paletteSet == 108) {
        int max = max3(sr_w2_get_red(&thisCol), sr_w2_get_green(&thisCol), sr_w2_get_blue(&thisCol));
        if (paletteSet == 71 || paletteSet == 108 || paletteSet == 27 || paletteSet == 74 || paletteSet == 40 || paletteSet == 49 || paletteSet == 37 ||
            paletteSet == 110 || paletteSet == 57)
          max += 32;
        else
          max += 64;
        max = sr_clamp_to_endpoints(max, 0, 248);
        set_gray(&thisCol, max);
      }
      double h, s, v;
      int r, g, b;
      rgb_to_hsv(sr_w2_get_red(&thisCol), sr_w2_get_green(&thisCol), sr_w2_get_blue(&thisCol), &h, &s, &v);
      h += hueShift;
      double dist = h - blueHue;
      h -= dist * hueChange;
      s += sChange;
      if (s < satThreshold) {
        s = satThreshold;
        h = blueHue;
      }
      v += vChange;
      hsv_to_rgb(h, s, v, &r, &g, &b);
      sr_w2_set_red(&thisCol, (uint8_t)r);
      sr_w2_set_green(&thisCol, (uint8_t)g);
      sr_w2_set_blue(&thisCol, (uint8_t)b);
      if (!sr_p4_xm_dontChangePalettes[paletteSet]) put_color(c, thisCol, at);
      break;
    }

    int doAnimation = 0;
    for (int gi = 0; gi < COUNT(sr_p4_xm_group_len); gi++) {
      int member = -1;
      for (int k = 0; k < sr_p4_xm_group_len[gi]; k++)
        if (sr_p4_xm_group[gi][k] == paletteSet) member = k;
      if (member < 0) continue;
      if (member == 0) {
        doAnimation = 1;
        prevH[paletteSet] = hChange;
        prevS[paletteSet] = sChange;
        prevV[paletteSet] = vChange;
      } else {
        hChange = prevH[sr_p4_xm_group[gi][0]];
        sChange = prevS[sr_p4_xm_group[gi][0]];
        vChange = prevV[sr_p4_xm_group[gi][0]];
      }
    }
    (void)hChange;

    for (int palnum = 1; palnum < 8; palnum++) {
      for (int col = 1; col < 16; col++) {
        int readPalSet = paletteSet == 62 ? 66 : paletteSet;
        struct sr_w2_color thisCol = get_color(c, rom, palStartOffset + readPalSet * 30 * 7 + palnum * 30 + col * 2);
        if (gray_hit(paletteSet, palnum, col)) {
          int max = max3(sr_w2_get_red(&thisCol), sr_w2_get_green(&thisCol), sr_w2_get_blue(&thisCol));
          set_gray(&thisCol, gray_plus(paletteSet, palnum, max, 0));
        }
        double h, s, v;
        int r, g, b;
        rgb_to_hsv(sr_w2_get_red(&thisCol), sr_w2_get_green(&thisCol), sr_w2_get_blue(&thisCol), &h, &s, &v);
        if (!sr_p4_xm_darkenOnlyPalettes[paletteSet]) {
          h += hueShift;
          double dist = h - blueHue;
          h -= dist * hueChange;
        }
        s += sChange;
        if (s < satThreshold) {
          s = satThreshold;
          h = blueHue;
        }
        v += vChange;

        int changedSpecial = 0;
        if ((paletteSet == 27 || paletteSet == 110) && palnum == 3 && col >= 9) {
          h = 340; s -= sChange; v -= vChange;
          changedSpecial = 1;
        }
        if (paletteSet == 28 && palnum == 7 && col >= 1 && col <= 5) {
          h = 130; s -= sChange; v -= vChange; v -= 0.2;
          changedSpecial = 1;
        }
        if (paletteSet == 28 && palnum == 2 && col >= 1 && col <= 2) {
          h = 340; s -= sChange; v -= vChange; v -= 0.2; s += 0.3;
          changedSpecial = 1;
        }
        if (paletteSet == 28 && palnum == 2 && col >= 5 && col <= 6) {
          h = 130; s -= sChange; v -= vChange; v -= 0.2;
          changedSpecial = 1;
        }
        if ((paletteSet == 24 || paletteSet == 25 || paletteSet == 26 || paletteSet == 112) && palnum == 2 && col >= 8 && col <= 9) {
          h = 130; s -= sChange; v -= vChange; s -= 0.3; v -= 0.7;
          changedSpecial = 1;
        }
        if (((paletteSet == 24 || paletteSet == 112) && palnum == 3 && col >= 8 && col <= 9) || ((paletteSet == 25 || paletteSet == 26) && palnum == 5 && col >= 8 && col <= 9)) {
          h = 0; s -= sChange; v -= vChange; s -= 0.1; v -= 0.7;
          changedSpecial = 1;
        }
        if (((paletteSet == 24 || paletteSet == 112 || paletteSet == 61) && palnum == 3 && col >= 1 && col <= 6) || ((paletteSet == 25 || paletteSet == 26) && palnum == 5 && col >= 1 && col <= 6)) {
          s -= sChange; v -= vChange; v -= 0.5;
          changedSpecial = 1;
        }
        hsv_to_rgb(h, s, v, &r, &g, &b);
        sr_w2_set_red(&thisCol, (uint8_t)r);
        sr_w2_set_green(&thisCol, (uint8_t)g);
        sr_w2_set_blue(&thisCol, (uint8_t)b);

        int ok = !sr_p4_xm_dontChangePalettes[paletteSet];
        if (sr_p4_xm_exc[paletteSet][palnum] >> col & 1) ok = 0;
        if (ok || changedSpecial) put_color(c, thisCol, palStartOffset + paletteSet * 30 * 7 + palnum * 30 + col * 2);
      }
    }

    if (doAnimation) {
      int index = sr_peek(c, 0x80802 + paletteSet) + 1;
      int animPalIndex = sr_peek(c, 0x8087B + index);
      int sourcePalIndex = (animPalIndex & 0x07) + 1;
      if (paletteSet == 83) sourcePalIndex = 4;
      if (paletteSet == 57) sourcePalIndex = 1;
      if (animPalIndex >= 0x80) animPalIndex = sr_peek(c, 0x8087B + index + 1);
      int animPalOffset = animPalIndex * 0x1E + 0x808E4;
      for (int palnum = 0; palnum < 7; palnum++) {
        for (int col = 0; col < 15; col++) {
          int off = animPalOffset + palnum * 30 + col * 2;
          if (off < 0x80FFE || off > 0x819D6) {
            struct sr_w2_color thisCol = get_color(c, rom, off);
            if (gray_hit(paletteSet, sourcePalIndex, col)) {
              int max = max3(sr_w2_get_red(&thisCol), sr_w2_get_green(&thisCol), sr_w2_get_blue(&thisCol));
              set_gray(&thisCol, gray_plus(paletteSet, sourcePalIndex, max, 1));
            }
            double h, s, v;
            int r, g, b;
            rgb_to_hsv(sr_w2_get_red(&thisCol), sr_w2_get_green(&thisCol), sr_w2_get_blue(&thisCol), &h, &s, &v);
            h += hueShift;
            double dist = h - blueHue;
            h -= dist * hueChange;
            s += sChange;
            if (s < satThreshold) {
              s = satThreshold;
              h = blueHue;
            }
            v += vChange;
            hsv_to_rgb(h, s, v, &r, &g, &b);

            int ok = !sr_p4_xm_dontChangePalettes[paletteSet];
            if (sr_p4_xm_exc_first[paletteSet] >> (col + 1) & 1) ok = 0;
            if (ok) put_color(c, thisCol, off);
          }
        }
      }
    }
  }
  (void)prevS;
  (void)prevV;

  /* disable palette animation for ice country, undine cave and gaia's navel water */
  P(0x80886, 0x17);
  P(0x80887, 0x17);
  P(0x808D1, 0x36);
  P(0x808D2, 0x36);
  P(0x80884, 0x36);
  P(0x80885, 0x36);
}

/* ---------------------------------------------------------------------------------------------------------- XmasDrops */
static void remove_first(uint8_t *list, int *n, int v) {
  for (int i = 0; i < *n; i++)
    if (list[i] == v) {
      memmove(list + i, list + i + 1, (size_t)(*n - i - 1));
      (*n)--;
      return;
    }
}

static void xmas_drops(struct sr_ctx *c) {
  P(0x428E, 0x0F);
  uint8_t normal[20 * 7], gear[96];
  int nn = 0, ng = 0;
  for (int i = 0; i < 20; i++) {
    for (int b = 0x40; b <= 0x45; b++) normal[nn++] = (uint8_t)b;
    normal[nn++] = 0x4A;
  }
  for (int b = 0x40; b <= 0x45; b++) gear[ng++] = (uint8_t)b;
  gear[ng++] = 0x4A;
  for (int b = 0x01; b <= 0x14; b++) gear[ng++] = (uint8_t)b;
  for (int b = 0x16; b <= 0x29; b++) gear[ng++] = (uint8_t)b;
  for (int b = 0x2b; b <= 0x3d; b++) gear[ng++] = (uint8_t)b;
  remove_first(gear, &ng, 22);
  remove_first(gear, &ng, 23);
  remove_first(gear, &ng, 24);
  gear[ng++] = 0x40;
  if (ng < 64) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range.");
  int itemTableLocationA = c->working_offset;
  for (int i = 0; i < 64; i++) E(normal[i]);
  int itemTableLocationB = c->working_offset;
  for (int i = 0; i < 64; i++) E(gear[i]);
  for (int i = 0x8E12C; i < 0x8E17E; i++) P(i, 0xEA);
  P(0x8E120, 0x80);
  P(0x8E12C, 0x22);
  P(0x8E12D, (uint8_t)c->working_offset);
  P(0x8E12E, (uint8_t)(c->working_offset >> 8));
  P(0x8E12F, (uint8_t)((c->working_offset >> 16) + 0xC0));
  static const uint8_t pre[] = {0x22, 0x9C, 0x38, 0xC0, 0xC2, 0x20, 0x29, 0x3F, 0x00, 0xAA, 0xE2, 0x20, 0x22, 0x9C, 0x38, 0xC0, 0x29, 0x0F, 0xF0, 0x07};
  for (size_t i = 0; i < sizeof pre; i++) E(pre[i]);
  E(0xBF); E((uint8_t)itemTableLocationA); E((uint8_t)(itemTableLocationA >> 8)); E((uint8_t)(0xC0 + (itemTableLocationA >> 16)));
  E(0x85); E(0xA4); E(0x6B);
  E(0xBF); E((uint8_t)itemTableLocationB); E((uint8_t)(itemTableLocationB >> 8)); E((uint8_t)(0xC0 + (itemTableLocationB >> 16)));
  E(0x85); E(0xA4); E(0x6B);
}

/* ------------------------------------------------------------------------------------------------------- OpenWorldXmasHacks */
int sr_p4_hack_xmas_hacks(struct sr_ctx *c) {
  int didChristmasPalettes = 0, didChristmasDrops = 0;
  int xmasDeco = sr_settings_get_bool(c->settings, "opXmasMaps");
  int xmasDrops = sr_settings_get_bool(c->settings, "opXmasItems");
  const char *goal = sr_p4_sel_work_str(c, SR_P4_GOAL_SHORT_NAME);
  int xmasGoal = !strcmp(goal, SR_P4_GOAL_GIFTMODE) || !strcmp(goal, SR_P4_GOAL_REINDEER);

  if (xmasGoal || xmasDeco) {
    if (strstr(goal, "xmas")) sr_p4_sel_remove_item_command(c);
    winter_theme(c);
    xmas_map_palettes(c);
    didChristmasPalettes = 1;

    /* the return door out of santa's house: special case map 08 */
    int wo = c->working_offset;
    P(0x1CC83, 0x22);
    P(0x1CC84, (uint8_t)wo);
    P(0x1CC85, (uint8_t)(wo >> 8));
    P(0x1CC86, (uint8_t)((wo >> 16) + 0xC0));
    P(0x1CC87, 0xEA);
    static const uint8_t code[] = {0x8D, 0x08, 0xCD, 0xC9, 0x08, 0x00, 0xD0, 0x18, 0xA9, 0x4C, 0x01, 0x8D, 0x08, 0xCD, 0xA9, 0x50, 0x00, 0x8D, 0x06, 0xCD,
                                   0x8D, 0x02, 0xCD, 0xA9, 0x48, 0x00, 0x8D, 0x04, 0xCD, 0x8D, 0x00, 0xCD, 0xE2, 0x20, 0x6B};
    for (size_t i = 0; i < sizeof code; i++) E(code[i]);
  }

  if (xmasGoal || xmasDrops) {
    xmas_drops(c);
    didChristmasDrops = 1;
  }

  if (xmasGoal) {
    /* turtle island maps normally use event 0x6f, which is repurposed here, so make sure the gift guy is visible */
    int turtleMapNum2 = 449;
    int mapObjOffset = 0x80000 + sr_peek(c, 0x87000 + turtleMapNum2 * 2) + (sr_peek(c, 0x87000 + turtleMapNum2 * 2 + 1) << 8);
    mapObjOffset += 8;
    P(mapObjOffset + 8 * 0 + 1, 0x0F);
  }
  sr_p4_sel_work_set_bool(c, SR_P4_XMAS_PALETTES_INDICATOR, didChristmasPalettes);
  return didChristmasPalettes || didChristmasDrops;
}
