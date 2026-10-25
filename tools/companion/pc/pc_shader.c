/* Retro display filters of the SoM PC frontend, see pc_shader.h. */
#include "pc_shader.h"

#include <stdlib.h>
#include <string.h>

static const char *const NAMES[PC_SH_COUNT] = {"PLAIN", "SHARP BILINEAR", "SCANLINES", "CRT LITE", "LCD GRID"};
static const char *const KEYS[PC_SH_COUNT] = {"plain", "sharp", "scanlines", "crt", "lcd"};

const char *pc_shader_name(int m) { return pc_shader_valid(m) ? NAMES[m] : "?"; }
const char *pc_shader_key(int m) { return pc_shader_valid(m) ? KEYS[m] : "?"; }
int pc_shader_valid(int m) { return m >= 0 && m < PC_SH_COUNT; }
int pc_shader_from_key(const char *k) {
  if (!k) return -1;
  for (int i = 0; i < PC_SH_COUNT; i++) if (!strcmp(k, KEYS[i])) return i;
  return -1;
}
int pc_shader_next(int m, int dir) { return ((pc_shader_valid(m) ? m : 0) + (dir < 0 ? PC_SH_COUNT - 1 : 1)) % PC_SH_COUNT; }
int pc_shader_has_overlay(int m) { return m == PC_SH_SCANLINES || m == PC_SH_CRT || m == PC_SH_LCD; }
int pc_shader_is_sharp(int m) { return pc_shader_valid(m) && m != PC_SH_PLAIN; }

int pc_shader_prescale(int src_w, int dst_w) {
  int k;
  if (src_w < 1 || dst_w < 1) return 1;
  k = (dst_w + src_w - 1) / src_w;
  return k < 1 ? 1 : k > 8 ? 8 : k;
}

/* Brightness profiles over one source pixel, t in [0,1), fixed point 0..256.  Both are 256 at the centre. */
static int scan_profile(int t256, int depth) {          /* depth 0..256: how dark the line edge gets */
  int d = t256 < 128 ? 128 - t256 : t256 - 128;          /* 0 at the centre, 128 at the edge */
  int d2 = d * d >> 6;                                  /* 0..256 */
  return 256 - (depth * d2 >> 8);
}

/* Average of f over the output pixel p, which spans source coordinates [p * src / dst, (p + 1) * src / dst).  Four sub-samples; when an
 * output pixel covers several source pixels the average flattens by itself instead of aliasing. */
static int avg_axis(int p, int dst, int src, int (*f)(int t256, int arg), int arg) {
  int sum = 0;
  for (int s = 0; s < 4; s++) {
    int64_t pos = ((int64_t)p * 8 + 2 * s + 1) * src * 32 / dst;   /* source coordinate in 1/256 pixel */
    sum += f((int)(pos & 255), arg);
  }
  return sum >> 2;
}

static int f_scan(int t, int depth) { return scan_profile(t, depth); }
/* LCD: a dark gap at the end of every source pixel.  gap is its width in 1/256 pixel, depth how dark. */
static int f_gap(int t, int gd) {
  int gap = gd >> 8, depth = gd & 255;
  return t >= 256 - gap ? 256 - depth : 256;
}

static uint32_t pack(int r, int g, int b) {
  if (r > 255) r = 255;
  if (g > 255) g = 255;
  if (b > 255) b = 255;
  return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

int pc_shader_overlay(int mode, int dw, int dh, int sw, int sh, uint32_t *out) {
  int *fy, *fx;
  if (!pc_shader_has_overlay(mode) || dw < 1 || dh < 1 || sw < 1 || sh < 1 || !out) return -1;
  fy = malloc(sizeof(int) * (size_t)dh);
  fx = malloc(sizeof(int) * (size_t)dw);
  if (!fy || !fx) { free(fy); free(fx); return -1; }
  if (mode == PC_SH_SCANLINES || mode == PC_SH_CRT) {
    int depth = mode == PC_SH_SCANLINES ? 230 : 110;       /* 0.90 and 0.43 of the brightness lost at the very edge of a line */
    for (int y = 0; y < dh; y++) fy[y] = avg_axis(y, dh, sh, f_scan, depth);
  } else {
    int cell_y = dh / sh, cell_x = dw / sw;
    int gy = cell_y >= 3 ? (256 / cell_y > 36 ? 256 / cell_y : 36) : 36, gx = cell_x >= 3 ? (256 / cell_x > 36 ? 256 / cell_x : 36) : 36;
    if (gy > 90) gy = 90;
    if (gx > 90) gx = 90;
    for (int y = 0; y < dh; y++) fy[y] = avg_axis(y, dh, sh, f_gap, (gy << 8) | 120);
    for (int x = 0; x < dw; x++) fx[x] = avg_axis(x, dw, sw, f_gap, (gx << 8) | 120);
  }
  for (int y = 0; y < dh; y++) {
    uint32_t *o = out + (size_t)y * dw;
    if (mode == PC_SH_SCANLINES) {
      uint32_t v = pack(fy[y], fy[y], fy[y]);
      for (int x = 0; x < dw; x++) o[x] = v;
    } else if (mode == PC_SH_LCD) {
      for (int x = 0; x < dw; x++) { int g = fx[x] * fy[y] >> 8; if (g > 255) g = 255; o[x] = pack(g, g, g); }
    } else {                                                   /* CRT: scanlines x aperture grille x vignette */
      int ry = dh > 1 ? (2 * y - (dh - 1)) * 256 / (dh - 1) : 0;   /* -256..256 */
      for (int x = 0; x < dw; x++) {
        int rx = dw > 1 ? (2 * x - (dw - 1)) * 256 / (dw - 1) : 0;
        int r2 = (rx * rx + ry * ry) >> 9;                     /* 0..256: 0 at the centre, 256 in the corners */
        int vig = 256 - (r2 * 46 >> 8);                        /* corners keep 0.82 */
        int base = fy[y] * vig >> 8;
        int ph = x % 3, m = 214;                               /* off-phase channels keep 0.84 */
        int r = base * (ph == 0 ? 256 : m) >> 8, g = base * (ph == 1 ? 256 : m) >> 8, b = base * (ph == 2 ? 256 : m) >> 8;
        o[x] = pack(r, g, b);
      }
    }
  }
  free(fy); free(fx);
  return 0;
}
