/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Derived from processing/hacks/openworld/BossElementRando.cs (Author: Moppleton).  Translated to C 2026-10-18.
 * The emit blocks and the dictionaries come from tools/gen_p4_belem.py.  context.plandoSettings is empty in every supported setup; a
 * non-empty plando setting is refused.
 */
#include <math.h>
#include <string.h>

#include "sr_core.h"
#include "sr_p2_visual_color.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))

#include "sr_p4_belem_data.h"

/* Convert.ToInt32(double): half-even rounding, throws on NaN / overflow */
static int belem_to_int32(struct sr_ctx *c, double v) {
  if (!(v >= -2147483648.5 && v < 2147483647.5)) sr_fail(c, SOMRANDO_ERR_FAILED, "boss element: color value out of range");
  return (int)rint(v);
}

static double belem_rgb_to_hue(int r, int g, int b) {
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

static void belem_rgb_to_hsv(int r, int g, int b, double *hue, double *sat, double *val) {
  int max = r > g ? (r > b ? r : b) : (g > b ? g : b);
  int min = r < g ? (r < b ? r : b) : (g < b ? g : b);
  *hue = belem_rgb_to_hue(r, g, b);
  *sat = (max == 0) ? 0 : 1.0 - (1.0 * min / max);
  *val = max / 255.0;
}

static void belem_hsv_to_rgb(struct sr_ctx *c, double hue, double saturation, double value, int *r, int *g, int *b) {
  int hue_index = belem_to_int32(c, floor(hue / 60)) % 6;
  double f = hue / 60 - floor(hue / 60);
  if (saturation < 0) saturation = 0;
  if (saturation > 1) saturation = 1;
  value = value * 255;
  int v = sr_clamp_to_endpoints(belem_to_int32(c, value), 0, 255);
  int p = sr_clamp_to_endpoints(belem_to_int32(c, value * (1 - saturation)), 0, 255);
  int q = sr_clamp_to_endpoints(belem_to_int32(c, value * (1 - f * saturation)), 0, 255);
  int t = sr_clamp_to_endpoints(belem_to_int32(c, value * (1 - (1 - f) * saturation)), 0, 255);
  switch (hue_index) {
    case 0: *r = v; *g = t; *b = p; break;
    case 1: *r = q; *g = v; *b = p; break;
    case 2: *r = p; *g = v; *b = t; break;
    case 3: *r = p; *g = q; *b = v; break;
    case 4: *r = t; *g = p; *b = v; break;
    default: *r = v; *g = p; *b = q; break;
  }
}

int sr_hack_boss_element_rando(struct sr_ctx *c) {
  if (!sr_settings_get_bool(c->settings, "bossElementRando")) return 0;
  if (sr_settings_get(c->settings, "plandoSettings")[0])
    sr_fail(c, SOMRANDO_ERR_UNSUPPORTED, "unsupported option: plandoSettings (boss element plando)");

  struct sr_random *random = &c->rf;
  // make some space in-bank for spell id tables and code
  sr_ensure_space_in_bank(&c->working_offset, 500);
  int n_pal = (int)(sizeof belem_pal / sizeof belem_pal[0]);

  int bossReplacementTableOffset = c->working_offset;
  for (int i = 0x57; i <= 0x7f; i++) {
    int has = 0;
    for (int k = 0; k < n_pal; k++)
      if (belem_pal[k].boss == i) has = 1;
    E(has);
  }

#include "sr_p4_belem_code.h"

  // okay so now randomize stuff
  // 01=gnome, 02=sylphid, 04=undine, 08=salamando, 10=shade, 20=lumina, 40=luna, 80=dryad, 00=elementless
  int elements[] = {0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80, 0x00};
  const int n_elements = 9;

  double targetH[] = {30, 80, 220, 20, 280, 250, 60, 150, -1};
  double targetS[] = {150 / 255.0, 230 / 255.0, 150 / 255.0, 250 / 255.0, 200 / 255.0, 150 / 250.0, 250 / 255.0, 200 / 255.0, 50 / 255.0};
  double targetVFalloff[] = {0.1, 0.1, 0.0, 0.0, 0.2, 0.0, 0, 0.1, 0.3};
  double targetVAdd[] = {-0.3, -0.1, 0.0, 0.0, -0.3, 0.2, 0.1, -0.1, 0.1};

  for (int pk = 0; pk < n_pal; pk++) {
    int bossId = belem_pal[pk].boss;
    // No plando: the rando decides
    int ele = 0;
    int newEle = elements[ele];
    int origEle = sr_peek(c, 0x101c00 + bossId * 0x1D + 15);
    ele = sr_random_next(random) % n_elements;
    newEle = elements[ele];
    int iter = 0;
    while (iter < 100) {
      int dont = 0;
      for (size_t k = 0; k < sizeof belem_dont / sizeof belem_dont[0]; k++) {
        if (belem_dont[k].boss != bossId) continue;
        for (int j = 0; j < belem_dont[k].n; j++)
          if (belem_dont[k].ele[j] == newEle) dont = 1;
      }
      if (!(origEle == newEle || dont)) break;
      ele = sr_random_next(random) % n_elements;
      newEle = elements[ele];
      iter++;
    }

    for (int pi = 0; pi < belem_pal[pk].n; pi++) {
      int paletteId = belem_pal[pk].off[pi];
      int paletteOffset = 0x20000 + sr_ushort_from_bytes(c->out, 0x10bb24 + paletteId * 2);
      // fuck with the colors; leave index 0 and 14 out of it though

      double avgHue;
      int baseHue = -1;
      for (size_t k = 0; k < sizeof belem_hue / sizeof belem_hue[0]; k++)
        if (belem_hue[k].boss == bossId) baseHue = belem_hue[k].hue;

      if (baseHue < 0) {
        double totalWeight = 0;
        double totalHue = 0;
        for (int i = 1; i <= 14; i++) {
          struct sr_w2_color color = {{sr_peek(c, paletteOffset + i * 2), sr_peek(c, paletteOffset + i * 2 + 1)}};
          double h, s, v;
          belem_rgb_to_hsv(sr_w2_get_red(&color), sr_w2_get_green(&color), sr_w2_get_blue(&color), &h, &s, &v);
          // weigh colors by saturation & value
          double weight = s + (0.5 - fabs(0.5 - v)) * 2;
          totalHue += h * weight;
          totalWeight += weight;
        }
        avgHue = totalHue / totalWeight;
      } else {
        avgHue = baseHue;
      }

      double targetSValue = targetS[ele];
      for (size_t k = 0; k < sizeof belem_sat / sizeof belem_sat[0]; k++)
        if (belem_sat[k].boss == bossId) targetSValue = belem_sat[k].mul;
      double tH = targetH[ele] < 0 ? -1 : targetH[ele] + ((sr_random_next(random) % 100) / 100.0) * 8;
      double tS = targetSValue + ((sr_random_next(random) % 100) / 100.0) * 0.1;
      double tV = targetVFalloff[ele] + ((sr_random_next(random) % 100) / 100.0) * 0.02;
      double tA = targetVAdd[ele] + ((sr_random_next(random) % 100) / 100.0) * 0.05;

      double hueShift = tH < 0 ? -1 : avgHue - tH;  // subtract this later

      for (int i = 1; i <= 14; i++) {
        struct sr_w2_color color = {{sr_peek(c, paletteOffset + i * 2), sr_peek(c, paletteOffset + i * 2 + 1)}};
        double h, s, v;
        belem_rgb_to_hsv(sr_w2_get_red(&color), sr_w2_get_green(&color), sr_w2_get_blue(&color), &h, &s, &v);

        double vDiff = (1 - v) * tV;
        v -= vDiff;
        v += tA;
        if (v < 0.1) v = 0.1;
        if (v > 1) v = 1;

        if (hueShift != -1) {
          h -= hueShift;
          for (size_t k = 0; k < sizeof belem_flat / sizeof belem_flat[0]; k++) {
            if (belem_flat[k].boss != bossId) continue;
            double diffFromTh = h - tH;
            h -= diffFromTh * belem_flat[k].flat;
          }
        }

        s *= tS;

        int r, g, b;
        belem_hsv_to_rgb(c, h, s, v, &r, &g, &b);
        struct sr_w2_color colorNew = sr_w2_color_rgb(r, g, b);
        P(paletteOffset + i * 2, colorNew.b[0]);
        P(paletteOffset + i * 2 + 1, colorNew.b[1]);
      }
    }
    // set def ele on enemy data
    P(0x101c00 + bossId * 0x1D + 15, newEle);
  }
  return 1;
}
