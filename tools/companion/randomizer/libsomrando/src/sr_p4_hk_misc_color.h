/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49.  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * ColorUtil.rgbToHsv / hsvToRgb (util/ColorUtil.cs, Author: Moppleton), shared by the hmisc hacks.  Translated to C 2026-10-18.
 */
#ifndef SR_P4_HK_MISC_COLOR_H
#define SR_P4_HK_MISC_COLOR_H
#include <math.h>

#include "sr_core.h"

static double sr_p4_hmisc_rgb_to_hue(int r, int g, int b) {
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

static void sr_p4_hmisc_rgb_to_hsv(int r, int g, int b, double *hue, double *sat, double *val) {
  int max = r > g ? (r > b ? r : b) : (g > b ? g : b);
  int min = r < g ? (r < b ? r : b) : (g < b ? g : b);
  *hue = sr_p4_hmisc_rgb_to_hue(r, g, b);
  *sat = (max == 0) ? 0 : 1.0 - (1.0 * min / max);
  *val = max / 255.0;
}

static int sr_p4_hmisc_to_int32(double v) { return (int)rint(v); }

static void sr_p4_hmisc_hsv_to_rgb(double hue, double saturation, double value, int *r, int *g, int *b) {
  int hue_index = sr_p4_hmisc_to_int32(floor(hue / 60)) % 6;
  double f = hue / 60 - floor(hue / 60);
  if (saturation < 0) saturation = 0;
  if (saturation > 1) saturation = 1;
  value = value * 255;
  int v = sr_clamp_to_endpoints(sr_p4_hmisc_to_int32(value), 0, 255);
  int p = sr_clamp_to_endpoints(sr_p4_hmisc_to_int32(value * (1 - saturation)), 0, 255);
  int q = sr_clamp_to_endpoints(sr_p4_hmisc_to_int32(value * (1 - f * saturation)), 0, 255);
  int t = sr_clamp_to_endpoints(sr_p4_hmisc_to_int32(value * (1 - (1 - f) * saturation)), 0, 255);
  switch (hue_index) {
    case 0: *r = v; *g = t; *b = p; break;
    case 1: *r = q; *g = v; *b = p; break;
    case 2: *r = p; *g = v; *b = t; break;
    case 3: *r = p; *g = q; *b = v; break;
    case 4: *r = t; *g = p; *b = v; break;
    default: *r = v; *g = p; *b = q; break;
  }
}
#endif
