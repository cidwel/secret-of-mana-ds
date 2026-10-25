/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * FallTheme (halloween) and WinterTheme (xmas) for the vanilla randomizer, with XmasDrops and the SnesColor helper:
 * processing/hacks/vanillarando/{FallTheme,WinterTheme}.cs, processing/hacks/common/other/XmasDrops.cs,
 * processing/common/structure/SnesColor.cs.  Author of every upstream file: Moppleton.  Translated to C 2026-10-18.
 */
#include <string.h>

#include "sr_core.h"
#include "sr_p2_system_holiday.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))

/* ------------------------------------------------------------------------------------------------------ SnesColor */
struct sr_w4_col {
  uint8_t b[2];
};

static struct sr_w4_col sr_w4_col_from(const uint8_t *rom, int addr) {
  struct sr_w4_col k;
  k.b[0] = rom[addr];
  k.b[1] = rom[addr + 1];
  return k;
}
static int sr_w4_red(const struct sr_w4_col *k) { return (uint8_t)((k->b[0] & 0x1F) * 8); }
static int sr_w4_green(const struct sr_w4_col *k) { return (uint8_t)((((k->b[0] & 0xE0) >> 5) + ((k->b[1] & 0x03) << 3)) * 8); }
static int sr_w4_blue(const struct sr_w4_col *k) { return (uint8_t)(((k->b[1] & 0x7C) >> 2) * 8); }
static void sr_w4_set_red(struct sr_w4_col *k, int v) { k->b[0] = (uint8_t)((k->b[0] & ~0x1F) | (uint8_t)(((uint8_t)v / 8) & 0xFF)); }
static void sr_w4_set_green(struct sr_w4_col *k, int v) {
  int g = (uint8_t)v / 8;
  k->b[0] = (uint8_t)((k->b[0] & ~0xE0) | (uint8_t)((g & 0x07) << 5));
  k->b[1] = (uint8_t)((k->b[1] & ~0x03) | (uint8_t)(g >> 3));
}
static void sr_w4_set_blue(struct sr_w4_col *k, int v) { k->b[1] = (uint8_t)((k->b[1] & ~0x7C) | (uint8_t)(((uint8_t)v / 8) << 2)); }
static struct sr_w4_col sr_w4_col_rgb(int r, int g, int b) {
  struct sr_w4_col k = {{0, 0}};
  sr_w4_set_red(&k, sr_clamp_to_endpoints(r, 0, 255));
  sr_w4_set_green(&k, sr_clamp_to_endpoints(g, 0, 255));
  sr_w4_set_blue(&k, sr_clamp_to_endpoints(b, 0, 255));
  return k;
}
static void sr_w4_add(struct sr_w4_col *k, int r, int g, int b) {
  int rn = sr_clamp_to_endpoints(sr_w4_red(k) + r, 0, 255);
  int gn = sr_clamp_to_endpoints(sr_w4_green(k) + g, 0, 255);
  int bn = sr_clamp_to_endpoints(sr_w4_blue(k) + b, 0, 255);
  sr_w4_set_red(k, rn);
  sr_w4_set_green(k, gn);
  sr_w4_set_blue(k, bn);
}
static void sr_w4_scale(struct sr_w4_col *k, double r, double g, double b) {
  int rn = sr_clamp_to_endpoints((int)(sr_w4_red(k) * r), 0, 255);
  int gn = sr_clamp_to_endpoints((int)(sr_w4_green(k) * g), 0, 255);
  int bn = sr_clamp_to_endpoints((int)(sr_w4_blue(k) * b), 0, 255);
  sr_w4_set_red(k, rn);
  sr_w4_set_green(k, gn);
  sr_w4_set_blue(k, bn);
}
static void sr_w4_put(struct sr_ctx *c, const struct sr_w4_col *k, int addr) {
  P(addr, k->b[0]);
  P(addr + 1, k->b[1]);
}
static int sr_w4_mostly_blue(const struct sr_w4_col *k) {
  return sr_w4_blue(k) > sr_w4_red(k) - 16 && sr_w4_blue(k) > sr_w4_green(k) - 16;
}
static int sr_w4_in(const int *list, int n, int v) {
  for (int i = 0; i < n; i++)
    if (list[i] == v) return 1;
  return 0;
}
#define IN(arr, v) sr_w4_in((arr), (int)(sizeof(arr) / sizeof((arr)[0])), (v))

/* NamesOfThings.originalNames[index] as the C# string (VanillaEventUtil.getChar of each byte up to the first zero) */
static const char *sr_w4_original_name(struct sr_ctx *c, int index) {
  int event_id = 0x800 + index;
  int off = sr_ushort_from_bytes(c->orig, 0xA0000 + (event_id - 0x400) * 2) + 0xA0000;
  char *out = (char *)sr_alloc(c, 256);
  int n = 0;
  for (uint8_t v = sr_peek_orig(c, off); v != 0 && n < 250; v = sr_peek_orig(c, ++off)) {
    char ch = ' ';
    if (v == 0x7F || (v >= 0xCF && v <= 0xD2)) sr_fail(c, SOMRANDO_ERR_UNSUPPORTED, "unsupported option: control or arrow character in an original name");
    if (v == 0x80) ch = ' ';
    else if (v >= 0x81 && v <= 0x9A) ch = (char)(v - 0x20);
    else if (v >= 0x9B && v <= 0xB4) ch = (char)(v - 0x5A);
    else if (v >= 0xB5 && v <= 0xBE) ch = (char)(v - 0x85);
    else if (v == 0xBF) ch = '.';
    else if (v == 0xC0) ch = ',';
    else if (v == 0xC1) ch = '/';
    else if (v == 0xC2) ch = '\'';
    else if (v == 0xC3) ch = '[';
    else if (v == 0xC4) ch = ']';
    else if (v == 0xC5) ch = ':';
    else if (v == 0xC6) ch = '-';
    else if (v == 0xC7) ch = '%';
    else if (v == 0xC8) ch = '!';
    else if (v == 0xC9) ch = '&';
    else if (v == 0xCA) ch = '?';
    else if (v == 0xCB) ch = '(';
    else if (v == 0xCC) ch = ')';
    else if (v == 0xCD) ch = '*';
    else if (v == 0xCE) ch = '^';
    else if (v == 0x57) ch = '>';
    out[n++] = ch;
  }
  out[n] = 0;
  return out;
}

/* ------------------------------------------------------------------------------------------------------- FallTheme */
/* the shared colour recipe of FallTheme for outdoor palettes and animated palettes */
static void sr_w4_fall_color(struct sr_w4_col *k, int pal_set, int palnum, const int *sets1, int n1, const int *sets2, int n2, const int *snowy, int ns) {
  int mostly_blue = sr_w4_mostly_blue(k);
  if (sr_w4_in(snowy, ns, pal_set)) {
    sr_w4_scale(k, 0.5, 0.4, 0.7);
  } else if (!mostly_blue || !(sr_w4_in(sets1, n1, pal_set) || sr_w4_in(sets2, n2, pal_set))) {
    sr_w4_set_green(k, (sr_w4_red(k) + sr_w4_green(k)) / 2);
    sr_w4_scale(k, 1.0, 0.65, 0.7);
    sr_w4_add(k, -16, -16, -16);
  } else {
    sr_w4_add(k, -64, -64, -64);
    if (palnum == 6 || palnum == 7) {
      sr_w4_set_red(k, sr_w4_blue(k));
      sr_w4_set_green(k, 0);
      sr_w4_set_blue(k, 0);
    }
  }
}

int sr_w4_hack_fall_theme(struct sr_ctx *c) {
  if (strcmp(sr_settings_get(c->settings, "rHoliday"), "halloween") != 0) return 0;
  const uint8_t *orig = c->orig;
  static const uint8_t enemies[] = {10, 10, 10, 10, 13, 13, 13, 13, 14, 18, 18, 18, 18, 21, 21, 21, 21, 23, 23, 23, 23, 30, 30, 30, 30, 31, 31, 31, 31,
                                    33, 35, 45, 50, 51, 54, 54, 54, 54, 55, 59, 66, 66, 66, 66, 70, 73, 80, 80, 80, 80, 81};
  static const int no_swap[] = {32, 46, 63, 50, 9, 15, 29, 48};
  const int n_enemies = (int)sizeof enemies;

  for (int i = 0; i < 84; i++) {
    int is_listed = 0;
    for (int k = 0; k < n_enemies; k++) is_listed |= (enemies[k] == i);
    if (IN(no_swap, i) || is_listed) continue;
    int new_enemy = enemies[sr_random_next(&c->rf) % n_enemies];
    for (int j = 0; j < 16; j++) P(0x100000 + i * 16 + j, orig[0x100000 + new_enemy * 16 + j]);

    int src_sr = orig[0x101C00 + new_enemy * 29 + 23], src_lr = orig[0x101C00 + new_enemy * 29 + 24];
    int dst_sr = orig[0x101C00 + i * 29 + 23], dst_lr = orig[0x101C00 + i * 29 + 24];
    for (int f = 0; f < 4; f++) {
      P(0x101000 + dst_sr * 12 + f, orig[0x101000 + src_sr * 12 + f]);
      P(0x101000 + dst_lr * 12 + f, orig[0x101000 + src_lr * 12 + f]);
    }
    for (int j = 0; j < 30; j++) P(0x80FFE + i * 30 + j, orig[0x80FFE + new_enemy * 30 + j]);
    sr_names_set(c, NAMES_INDEX_ENEMIES_START + i, sr_w4_original_name(c, NAMES_INDEX_ENEMIES_START + new_enemy));
  }

  for (int i = 0; i < 84; i++) {
    P(0x103A50 + i * 5 + 2, 0xF8);
    P(0x103A50 + i * 5 + 3, 0x40 + (sr_random_next(&c->rf) % 2));
  }
  P(0x428E, 0x0F);
  P(0x18FB9C, 0);
  P(0x18FB9D, 0);
  P(0x18FB9E, 0);
  P(0x18FB9F, 0);

  static const int sets1[] = {24, 25, 26, 29, 35, 57, 59, 61, 62, 78, 81, 112};
  static const int sets2[] = {98, 99, 55};
  const int pal_start = 0xC7FE0, src_pal = 65;
  static const uint8_t copy1[7][16] = {
    {1, 2, 3, 4, 5, 6, 0}, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0}, {11, 12, 13, 14, 15, 0}, {11, 12, 13, 14, 15, 0},
    {10, 11, 12, 13, 14, 15, 0}, {11, 12, 13, 14, 15, 0}, {6, 7, 0}};
  static const uint8_t copy2[7][16] = {
    {1, 2, 3, 4, 5, 6, 0}, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0}, {11, 12, 13, 14, 15, 0}, {11, 12, 13, 14, 15, 0},
    {10, 11, 12, 13, 14, 15, 0}, {0}, {0}};
  for (int pass = 0; pass < 2; pass++) {
    const int *sets = pass ? sets2 : sets1;
    int ns = pass ? 3 : 12;
    for (int s = 0; s < ns; s++)
      for (int palnum = 1; palnum < 8; palnum++)
        for (int col = 1; col < 16; col++) {
          const uint8_t *list = pass ? copy2[palnum - 1] : copy1[palnum - 1];
          int found = 0;
          for (int k = 0; list[k]; k++) found |= (list[k] == col);
          if (!found) continue;
          int from = pal_start + src_pal * 30 * 7 + palnum * 30 + col * 2;
          int to = pal_start + sets[s] * 30 * 7 + palnum * 30 + col * 2;
          P(to, orig[from]);
          P(to + 1, orig[from + 1]);
        }
  }

  static const int outdoor[] = {24, 25, 26, 27, 29, 32, 31, 35, 36, 37, 38, 40, 41, 48, 49, 54, 55, 57, 59, 61, 62, 63, 64, 65, 66, 68, 71, 72, 74, 77,
                                78, 81, 82, 83, 84, 94, 98, 99, 104, 107, 109, 110, 112, 101};
  const int n_outdoor = 43;
  static const int snowy[] = {37, 49, 77, 84, 107};
  static const int display_sets[] = {1, 4, 9, 19, 21};
  for (int s = 0; s < n_outdoor + 1; s++) {
    int pal_set = outdoor[s];
    if (s < n_outdoor) {
      for (int palnum = 1; palnum < 8; palnum++)
        for (int col = 1; col < 16; col++) {
          int addr = pal_start + pal_set * 30 * 7 + palnum * 30 + col * 2;
          struct sr_w4_col k = sr_w4_col_from(c->out, addr);
          sr_w4_fall_color(&k, pal_set, palnum, sets1, 12, sets2, 3, snowy, 5);
          sr_w4_put(c, &k, addr);
        }
    }
    int anim_index = orig[0x80802 + pal_set] + 1;
    int anim_pal_index = orig[0x8087B + anim_index];
    int anim_off = anim_pal_index * 0x1E + 0x808E4;
    for (int palnum = 0; palnum < 8; palnum++)
      for (int col = 0; col < 16; col++) {
        int off = anim_off + palnum * 30 + col * 2;
        if ((off < 0x80FFE || off > 0x819D6) && anim_pal_index > 0) {
          struct sr_w4_col k = sr_w4_col_from(orig, off);
          sr_w4_fall_color(&k, pal_set, palnum, sets1, 12, sets2, 3, snowy, 5);
          sr_w4_put(c, &k, off);
        }
      }
  }

  for (int s = 0; s < 5; s++) {
    int addr = 0x80100 + display_sets[s] * 9 + 7;
    struct sr_w4_col k = sr_w4_col_from(orig, addr);
    sr_w4_set_green(&k, (sr_w4_red(&k) + sr_w4_green(&k)) / 2);
    sr_w4_scale(&k, 1.0, 0.65, 0.7);
    sr_w4_add(&k, -16, -16, -16);
    sr_w4_put(c, &k, addr);
  }

  static const int world_offsets[] = {0x6483E, 0x67DFE};
  for (int w = 0; w < 2; w++)
    for (int palnum = 0; palnum < 13; palnum++)
      for (int col = 1; col < 16; col++) {
        int addr = world_offsets[w] + palnum * 30 + col * 2;
        struct sr_w4_col k = sr_w4_col_from(orig, addr);
        if (!sr_w4_mostly_blue(&k)) {
          sr_w4_set_green(&k, (sr_w4_red(&k) + sr_w4_green(&k)) / 2);
          sr_w4_scale(&k, 1.0, 0.5, 0.65);
        } else {
          sr_w4_add(&k, -64, -64, -64);
        }
        if (palnum == 1 || palnum == 2 || palnum == 3) {
          sr_w4_add(&k, -96, -96, -96);
          sr_w4_set_red(&k, 255 - sr_w4_red(&k));
          sr_w4_add(&k, -96, -96, -96);
        }
        if (w == 0) {
          sr_w4_put(c, &k, addr);
        } else {
          int from = 0x6483E + palnum * 30 + col * 2;
          P(addr, c->out[from]);
          P(addr + 1, c->out[from + 1]);
        }
      }
  return 1;
}

/* ------------------------------------------------------------------------------------------------------- XmasDrops */
static void sr_w4_xmas_drops(struct sr_ctx *c) {
  P(0x428E, 0x0F);
  uint8_t normal[64], gear[80];
  int n = 0, g = 0;
  /* the C# list repeats {40..45, 4A} 20 times and only the first 64 entries are used */
  for (int i = 0; i < 20 && n < 64; i++) {
    for (int b = 0x40; b <= 0x45 && n < 64; b++) normal[n++] = (uint8_t)b;
    if (n < 64) normal[n++] = 0x4A;
  }
  for (int b = 0x40; b <= 0x45; b++) gear[g++] = (uint8_t)b;
  gear[g++] = 0x4A;
  for (int b = 0x01; b <= 0x14; b++) gear[g++] = (uint8_t)b;
  for (int b = 0x16; b <= 0x29; b++)
    if (b != 22 && b != 23 && b != 24) gear[g++] = (uint8_t)b;
  for (int b = 0x2B; b <= 0x3D; b++) gear[g++] = (uint8_t)b;
  gear[g++] = 0x40;
  if (g != 64) sr_fail(c, SOMRANDO_ERR_FAILED, "xmas drop table size");

  int table_a = c->working_offset;
  for (int i = 0; i < 64; i++) E(normal[i]);
  int table_b = c->working_offset;
  for (int i = 0; i < 64; i++) E(gear[i]);
  for (int i = 0x8E12C; i < 0x8E17E; i++) P(i, 0xEA);
  P(0x8E120, 0x80);
  P(0x8E12C, 0x22);
  P(0x8E12D, c->working_offset);
  P(0x8E12E, c->working_offset >> 8);
  P(0x8E12F, (c->working_offset >> 16) + 0xC0);
  static const uint8_t pre[] = {0x22, 0x9C, 0x38, 0xC0, 0xC2, 0x20, 0x29, 0x3F, 0x00, 0xAA, 0xE2, 0x20, 0x22, 0x9C, 0x38, 0xC0, 0x29, 0x0F, 0xF0, 0x07};
  for (size_t i = 0; i < sizeof pre; i++) E(pre[i]);
  E(0xBF); E(table_a); E(table_a >> 8); E(0xC0 + (table_a >> 16));
  E(0x85); E(0xA4); E(0x6B);
  E(0xBF); E(table_b); E(table_b >> 8); E(0xC0 + (table_b >> 16));
  E(0x85); E(0xA4); E(0x6B);
}

static const uint8_t sr_w4_giftbox[384] = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x03, 0x03, 0x04, 0x02, 0x05, 0x1F, 0x07, 0x22, 0x03,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x03, 0x00, 0x07, 0x00, 0x07, 0x00, 0x03, 0x00, 0x1D, 0x18,
  0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0xE7, 0xC3, 0xBD, 0x7E, 0x99, 0x7E, 0xE7, 0x99, 0x66, 0xFF,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xDB, 0x00, 0xE7, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
  0x20, 0x03, 0x3B, 0x1F, 0x3C, 0x1B, 0x27, 0x1A, 0x3D, 0x1A, 0x27, 0x1A, 0x3D, 0x1A, 0x27, 0x1A,
  0x1F, 0x04, 0x07, 0x00, 0x07, 0x18, 0x07, 0x00, 0x07, 0x18, 0x07, 0x00, 0x07, 0x18, 0x07, 0x00,
  0xDB, 0xE7, 0xE3, 0x24, 0x00, 0x00, 0x81, 0x81, 0x66, 0xE7, 0x99, 0xFF, 0x7E, 0xFF, 0x99, 0xFF,
  0xFF, 0x00, 0xDF, 0x18, 0xFF, 0x66, 0x7E, 0x18, 0x18, 0x00, 0x00, 0x81, 0x00, 0x66, 0x00, 0x81,
  0x3D, 0x1A, 0x27, 0x1A, 0x3D, 0x1A, 0x1F, 0x02, 0x05, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x07, 0x18, 0x07, 0x00, 0x07, 0x18, 0x07, 0x00, 0x07, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x7E, 0xFF, 0x99, 0xFF, 0x7E, 0xFF, 0x99, 0xFF, 0x7E, 0xFF, 0x99, 0x7E, 0x7E, 0x18, 0x18, 0x00,
  0x00, 0x66, 0x00, 0x81, 0x00, 0x66, 0x00, 0x81, 0x00, 0x66, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x07, 0x01, 0x1F, 0x07, 0x3E, 0x1F,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x07, 0x00, 0x1F, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x7E, 0x18, 0xFF, 0x7E, 0xE7, 0xFF, 0x81, 0xFF, 0x18, 0xE7,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x00, 0x7E, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00,
  0x3E, 0x01, 0x27, 0x18, 0x39, 0x1E, 0x26, 0x1D, 0x39, 0x1E, 0x27, 0x1A, 0x31, 0x1E, 0x25, 0x1A,
  0x07, 0x00, 0x01, 0x00, 0x00, 0x18, 0x02, 0x04, 0x01, 0x18, 0x05, 0x02, 0x01, 0x10, 0x07, 0x00,
  0x7E, 0x81, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x81, 0x7E, 0xE7, 0x99, 0xFF, 0x7E, 0xFF, 0x99, 0xFF,
  0xFF, 0x00, 0xFF, 0x00, 0x7E, 0x00, 0x18, 0x00, 0x00, 0x00, 0x00, 0x81, 0x00, 0x66, 0x00, 0x81,
};

/* ----------------------------------------------------------------------------------------------------- WinterTheme */
struct sr_w4_enemy_col {
  int pal;
  int r, g, b;
};

#define EC(p, r, g, b) {p, r, g, b}

int sr_w4_hack_winter_theme(struct sr_ctx *c) {
  if (strcmp(sr_settings_get(c->settings, "rHoliday"), "xmas") != 0) return 0;
  const uint8_t *orig = c->orig;

  if (!sr_settings_get_bool(c->settings, "statusGlow")) {
    int w = c->working_offset;
    P(0x8EB47, 0x22);
    P(0x8EB48, w);
    P(0x8EB49, w >> 8);
    P(0x8EB4A, (w >> 16) + 0xC0);
    P(0x8EB4B, 0xEA);
    P(0x8EB4C, 0xEA);
    P(0x8EB4D, 0xEA);
    static const uint8_t code[] = {0x29, 0xFC, 0x09, 0x02, 0x9D, 0xB0, 0xE1, 0xBD, 0x90, 0xE1, 0x09, 0x04, 0x9D, 0x90, 0xE1, 0x6B};
    for (size_t i = 0; i < sizeof code; i++) E(code[i]);
  }
  sr_names_set(c, 0 + 7, "FREEZING RAIN");
  for (int i = 0; i < 15; i++) {
    struct sr_w4_col k = sr_w4_col_from(c->out, 0x11FDB2 + i * 2);
    sr_w4_add(&k, -128, -128, 128);
    sr_w4_put(c, &k, 0x11FDB2 + i * 2);
  }
  for (int i = 0; i < 5; i++) {
    struct sr_w4_col k = sr_w4_col_from(c->out, 0x12CF72 + i * 2);
    sr_w4_add(&k, -128, -128, 128);
    sr_w4_put(c, &k, 0x12CF72 + i * 2 + 1);
  }
  for (int i = 0; i < 15; i++) {
    struct sr_w4_col k = sr_w4_col_from(c->out, 0x81FD2 + i * 2);
    sr_w4_add(&k, -128, -128, 128);
    sr_w4_put(c, &k, 0x81FD2 + i * 2);
  }

  /* enemy palettes in dictionary insertion order; name is NULL when the C# does not rename the enemy */
  struct {
    int enemy;
    const char *name;
    int n;
    struct sr_w4_enemy_col cols[16];
  } changes[] = {
    {1, "Jingle Bee", 5, {EC(6, 172, 32, 32), EC(7, 248, 64, 64), EC(8, 248, 128, 128), EC(9, 168, 32, 32), EC(10, 248, 64, 64)}},
    {2, NULL, 8, {EC(7, 128, 240, 128), EC(8, 160, 224, 160), EC(9, 200, 248, 200), EC(10, 224, 248, 224), EC(11, 160, 160, 248),
                  EC(12, 192, 192, 248), EC(13, 208, 208, 248), EC(14, 224, 224, 248)}},
    {3, NULL, 4, {EC(6, 128, 0, 0), EC(7, 200, 32, 32), EC(8, 248, 64, 64), EC(9, 248, 128, 128)}},
    {4, "Poinsettia", 5, {EC(7, 128, 8, 8), EC(8, 192, 48, 48), EC(9, 248, 96, 96), EC(10, 248, 128, 128), EC(11, 248, 160, 160)}},
    {5, "Frozen Fish", 9, {EC(2, 64, 64, 128), EC(4, 0, 0, 160), EC(5, 88, 88, 176), EC(6, 80, 80, 208), EC(7, 120, 120, 248), EC(8, 128, 128, 248),
                           EC(9, 160, 160, 248), EC(10, 192, 192, 248), EC(15, 208, 208, 248)}},
    {6, NULL, 6, {EC(2, 72, 72, 248), EC(3, 120, 120, 248), EC(4, 192, 192, 248), EC(5, 224, 224, 248), EC(10, 216, 40, 40), EC(11, 248, 96, 96)}},
    {7, "Festive Eye Spy", 5, {EC(4, 176, 0, 0), EC(5, 248, 0, 0), EC(6, 248, 32, 32), EC(7, 248, 96, 96), EC(8, 248, 160, 160)}},
    {0x12, "Zombie Caroler", 7, {EC(4, 0, 120, 40), EC(5, 0, 152, 40), EC(6, 56, 168, 72), EC(7, 160, 64, 64), EC(8, 208, 48, 48), EC(9, 224, 96, 96),
                                  EC(10, 232, 168, 168)}},
    {0x16, "Pebbler Grinch", 6, {EC(8, 248, 64, 64), EC(11, 0, 88, 0), EC(12, 96, 176, 32), EC(13, 136, 208, 24), EC(14, 160, 216, 32),
                                 EC(15, 184, 232, 160)}},
    {0x18, "Christmas Crab", 10, {EC(5, 160, 0, 0), EC(6, 224, 56, 56), EC(7, 248, 112, 112), EC(8, 152, 152, 248), EC(9, 176, 176, 248),
                                  EC(10, 200, 200, 248), EC(11, 232, 232, 248), EC(12, 96, 176, 96), EC(13, 128, 200, 128), EC(14, 192, 248, 192)}},
    {0x19, "Gingerhorse", 12, {EC(2, 96, 48, 8), EC(3, 32, 120, 32), EC(4, 112, 56, 0), EC(5, 128, 64, 0), EC(7, 176, 88, 16), EC(8, 176, 104, 48),
                               EC(9, 176, 120, 72), EC(10, 184, 112, 48), EC(11, 88, 176, 88), EC(12, 128, 176, 128), EC(13, 168, 248, 168),
                               EC(15, 208, 128, 80)}},
    {0x2D, "Jolly Mimic", 4, {EC(12, 200, 32, 32), EC(13, 248, 72, 72), EC(14, 248, 152, 152), EC(15, 248, 192, 192)}},
  };
  for (size_t e = 0; e < sizeof changes / sizeof changes[0]; e++) {
    if (changes[e].name) sr_names_set(c, NAMES_INDEX_ENEMIES_START + changes[e].enemy, changes[e].name);
    for (int k = 0; k < changes[e].n; k++) {
      struct sr_w4_col col = sr_w4_col_rgb(changes[e].cols[k].r, changes[e].cols[k].g, changes[e].cols[k].b);
      sr_w4_put(c, &col, 0x80FFE + changes[e].enemy * 0x1E + changes[e].cols[k].pal * 2);
    }
  }

  const int pal_start = 0xC7FE0;
  for (int palnum = 1; palnum < 8; palnum++)
    for (int col = 1; col < 16; col++) {
      struct sr_w4_col k = sr_w4_col_from(c->out, pal_start + 49 * 30 * 7 + palnum * 30 + col * 2);
      if (palnum != 4 || col < 6) sr_w4_put(c, &k, pal_start + 71 * 30 * 7 + palnum * 30 + col * 2);
    }

  /* icy palette sets for caves, castles and pure lands; the C# reads the tileset from the header's first byte */
  for (int i = 16; i < 500; i++) {
    int ho = sr_vanilla_object_offset(orig, i);
    uint8_t h[8];
    memcpy(h, orig + ho, 8);
    int tileset = h[0] & 0x3F, pal = -1;
    if (tileset == 10) pal = 101;
    else if (tileset == 14) pal = 84;
    else if (tileset == 15) pal = 85;
    else if (tileset == 30) pal = 54;
    if (pal < 0) continue;
    h[1] = (uint8_t)((h[1] & ~0x7F) | pal);
    int oo = sr_vanilla_object_offset(c->out, i);
    for (int j = 0; j < 8; j++) P(oo + j, h[j]);
  }

  static const int mod_pal_sets[] = {101, 54};
  for (int s = 0; s < 2; s++)
    for (int palnum = 1; palnum < 8; palnum++)
      for (int col = 1; col < 16; col++) {
        int addr = pal_start + mod_pal_sets[s] * 30 * 7 + palnum * 30 + col * 2;
        struct sr_w4_col k = sr_w4_col_from(c->out, addr);
        if (sr_w4_red(&k) != 0 || sr_w4_green(&k) != 0 || sr_w4_blue(&k) != 0) {
          sr_w4_scale(&k, 1.0, 1.0, 2.0);
          sr_w4_add(&k, 32, 32, 32);
        }
        sr_w4_put(c, &k, addr);
      }

  sr_w4_xmas_drops(c);

  for (int i = 0; i < 384; i++) P(i + 0x186540, sr_w4_giftbox[i]);

  static const int sets1[] = {24, 25, 26, 29, 35, 36, 57, 59, 61, 62, 78, 81, 112};
  static const int dark1[] = {1, 2, 3, 4, 5, 6};
  static const int dark3[] = {1, 2, 3, 7, 11, 12, 13, 14};
  static const int dark5[] = {10, 11, 12, 13, 14, 15};
  const int src_pal = 66;
  static const uint8_t copy1[7][16] = {
    {1, 2, 3, 4, 5, 6, 0}, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0}, {11, 12, 13, 14, 15, 0}, {11, 12, 13, 14, 15, 0},
    {7, 8, 9, 10, 11, 12, 13, 14, 15, 0}, {11, 12, 13, 14, 15, 0}, {6, 7, 0}};
  static const uint8_t copy2[7][16] = {
    {1, 2, 3, 4, 5, 6, 0}, {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0}, {11, 12, 13, 14, 15, 0}, {11, 12, 13, 14, 15, 0},
    {10, 11, 12, 13, 14, 15, 0}, {0}, {0}};
  for (size_t s = 0; s < sizeof sets1 / sizeof sets1[0]; s++)
    for (int palnum = 1; palnum < 8; palnum++)
      for (int col = 1; col < 16; col++) {
        int to = pal_start + sets1[s] * 30 * 7 + palnum * 30 + col * 2;
        if (palnum == 1 && IN(dark1, col)) {
          struct sr_w4_col k = sr_w4_col_from(c->out, to);
          sr_w4_add(&k, -32, -32, -32);
          sr_w4_put(c, &k, to);
        }
        int found = 0;
        for (int k = 0; copy1[palnum - 1][k]; k++) found |= (copy1[palnum - 1][k] == col);
        if (!found) continue;
        int from = pal_start + src_pal * 30 * 7 + palnum * 30 + col * 2;
        int p1 = c->out[from], p2 = c->out[from + 1];
        if ((palnum == 1 && IN(dark1, col)) || (palnum == 3 && IN(dark3, col))) {
          struct sr_w4_col k = {{(uint8_t)p1, (uint8_t)p2}};
          sr_w4_add(&k, -32, -32, -32);
          sr_w4_put(c, &k, to);
        } else if (palnum == 5 && IN(dark5, col)) {
          struct sr_w4_col k = {{(uint8_t)p1, (uint8_t)p2}};
          sr_w4_add(&k, -16, -16, -16);
          sr_w4_put(c, &k, to);
        }
        P(to, p1);
        P(to + 1, p2);
      }

  static const int sets2[] = {98, 99, 55};
  for (int s = 0; s < 3; s++)
    for (int palnum = 1; palnum < 8; palnum++)
      for (int col = 1; col < 16; col++) {
        int found = 0;
        for (int k = 0; copy2[palnum - 1][k]; k++) found |= (copy2[palnum - 1][k] == col);
        if (!found) continue;
        int from = pal_start + src_pal * 30 * 7 + palnum * 30 + col * 2;
        int to = pal_start + sets2[s] * 30 * 7 + palnum * 30 + col * 2;
        P(to, orig[from]);
        P(to + 1, orig[from + 1]);
      }

  static const int outdoor[] = {24, 25, 26, 27, 29, 32, 31, 35, 36, 37, 38, 40, 41, 48, 49, 54, 55, 57, 59, 61, 62, 63, 64, 65, 66, 68, 71, 72, 74, 77,
                                78, 81, 82, 83, 84, 94, 98, 99, 104, 107, 109, 110, 112, 101};
  const int n_outdoor = 43;
  static const int display_sets[] = {1, 4, 9, 19, 21};
  const double scale_r = 0.75, sg = 0.7, sb = 0.85;
  for (int s = 0; s < n_outdoor + 1; s++) {
    int pal_set = outdoor[s];
    int is_outdoor = s < n_outdoor;
    if (is_outdoor) {
      for (int palnum = 1; palnum < 8; palnum++)
        for (int col = 1; col < 16; col++) {
          int addr = pal_start + pal_set * 30 * 7 + palnum * 30 + col * 2;
          struct sr_w4_col k = sr_w4_col_from(c->out, addr);
          sr_w4_scale(&k, scale_r, sg, sb);
          sr_w4_put(c, &k, addr);
        }
    }
    int anim_index = orig[0x80802 + pal_set] + 1;
    int anim_pal_index = orig[0x8087B + anim_index];
    int anim_off = anim_pal_index * 0x1E + 0x808E4;
    for (int palnum = 0; palnum < 7; palnum++)
      for (int col = 0; col < 15; col++) {
        int off = anim_off + palnum * 30 + col * 2;
        if (off < 0x80FFE || off > 0x819D6) {
          struct sr_w4_col k = sr_w4_col_from(orig, off);
          if (is_outdoor) {
            sr_w4_scale(&k, scale_r, sg, sb);
          } else {
            sr_w4_scale(&k, 1.0, 1.0, 2.0);
            sr_w4_add(&k, 32, 32, 32);
          }
          sr_w4_put(c, &k, off);
        }
      }
  }

  for (int s = 0; s < 5; s++) {
    int set = display_sets[s], addr = 0x80100 + set * 9 + 7;
    struct sr_w4_col k = sr_w4_col_from(orig, addr);
    if (set == 9) {
      sr_w4_scale(&k, scale_r, sg, sb);
    } else if (set == 21) {
      sr_w4_scale(&k, 1.0, 1.0, 2.0);
      sr_w4_add(&k, 32, 32, 32);
    } else {
      sr_w4_set_red(&k, 112);
      sr_w4_set_green(&k, 112);
      sr_w4_set_blue(&k, 128);
      sr_w4_scale(&k, scale_r, sg, sb);
    }
    sr_w4_put(c, &k, addr);
  }

  P(0x90A4, 0x08);
  P(0x90B5, 0x00);

  /* world map: palette -> (colour index -> replacement) */
  static const struct {
    int pal, col, r, g, b;
  } wm[] = {
    {5, 1, 32, 88, 48}, {5, 2, 40, 96, 56}, {5, 3, 48, 104, 72}, {5, 4, 96, 128, 120}, {5, 5, 120, 136, 136}, {5, 6, 160, 160, 184},
    {5, 7, 176, 176, 200}, {5, 8, 192, 192, 216}, {5, 9, 248, 248, 248}, {5, 10, 208, 208, 232}, {5, 11, 216, 216, 240}, {5, 12, 224, 224, 248},
    {5, 13, 232, 232, 248}, {5, 14, 200, 200, 224}, {5, 15, 208, 208, 232},
    {6, 0, 208, 208, 232}, {6, 12, 192, 192, 216}, {6, 13, 216, 216, 240}, {6, 14, 168, 168, 192},
    {7, 10, 192, 192, 216}, {7, 11, 200, 200, 224}, {7, 12, 208, 208, 232}, {7, 13, 216, 216, 240}, {7, 14, 192, 192, 216}, {7, 15, 176, 176, 200},
    {12, 1, 176, 176, 200}, {12, 2, 192, 192, 216}, {12, 3, 216, 216, 240},
    {10, 6, 224, 224, 248}, {10, 7, 208, 208, 232}, {10, 8, 216, 216, 240},
  };
  static const int world_offsets[] = {0x67DFE, 0x6483E};
  for (int w = 0; w < 2; w++)
    for (int palnum = 0; palnum < 13; palnum++)
      for (int col = 1; col < 16; col++) {
        int addr = world_offsets[w] + palnum * 30 + col * 2;
        struct sr_w4_col k = sr_w4_col_from(orig, addr);
        for (size_t r = 0; r < sizeof wm / sizeof wm[0]; r++)
          if (wm[r].pal == palnum && wm[r].col == col) k = sr_w4_col_rgb(wm[r].r, wm[r].g, wm[r].b);
        if (w == 0) {
          sr_w4_put(c, &k, addr);
        } else {
          int from = 0x67DFE + palnum * 30 + col * 2;
          P(addr, c->out[from]);
          P(addr + 1, c->out[from + 1]);
        }
      }
  return 1;
}
