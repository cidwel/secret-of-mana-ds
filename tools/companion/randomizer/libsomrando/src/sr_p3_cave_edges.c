/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Ancient Cave "cave" floors: tile edge post-processing.  Derived from processing/ancientcave/mapgen/cave/CaveTileEdges.cs
 * (Author: Moppleton).  Translated to C 2026-10-18.
 */
#include <math.h>
#include <string.h>

#include "sr_p3_cave.h"
#include "sr_p3_cave_pat.h"

static int d2i(double v) {   /* (int) of a double: saturating, NaN is 0 (.NET 9+) */
  if (v != v) return 0;
  if (v >= 2147483648.0) return 2147483647;
  if (v <= -2147483648.0) return (-2147483647 - 1);
  return (int)v;
}

static void copy_layer(struct ac_layer *dst, const struct ac_layer *src) {
  memcpy(dst->d, src->d, (size_t)src->w * (size_t)src->h);
}

static void apply_group(struct sr_ctx *c, const struct cave_pat *pats, int n, const struct ac_layer *l1copy, struct ac_layer *layer1,
                        struct ac_layer *layer2) {
  for (int i = 0; i < n; i++) ac_apply_pattern(c, &pats[i].p, l1copy, layer1, pats[i].use_alt ? layer2 : NULL, NULL);
}

void sr_p3_cave_add_tile_edges(struct sr_ctx *c, struct ac_layer *layer1, struct ac_layer *layer2, const struct ac_node *start_node) {
  int map_w = layer1->w, map_h = layer1->h;
  struct ac_layer *l1c = ac_layer_new(c, map_w, map_h);
  struct ac_layer *l2c = ac_layer_new(c, map_w, map_h);
  copy_layer(l1c, layer1);
  copy_layer(l2c, layer2);

  /* remove artifacts of the radial node generation */
  static const uint8_t value_checks[3] = {70, 5, 43};
  for (int i = 0; i < 10; i++) {
    for (int y = 1; y < map_h - 1; y++) {
      for (int x = 1; x < map_w - 1; x++) {
        for (int vi = 0; vi < 3; vi++) {
          int value = value_checks[vi];
          if (LAYER(l1c, x, y) == value) {
            int num_matches = 0;
            uint8_t check1[4] = {LAYER(l1c, x + 1, y), LAYER(l1c, x - 1, y), LAYER(l1c, x, y + 1), LAYER(l1c, x, y - 1)};
            uint8_t check2[4] = {LAYER(l2c, x + 1, y), LAYER(l2c, x - 1, y), LAYER(l2c, x, y + 1), LAYER(l2c, x, y - 1)};
            uint8_t alt1[4], alt2[4];
            int n_alt = 0;
            int dirs[4] = {0, 0, 0, 0};
            for (int j = 0; j < 4; j++) {
              if (check1[j] == value) {
                num_matches++;
              } else {
                alt1[n_alt] = check1[j];
                alt2[n_alt] = check2[j];
                n_alt++;
                dirs[j] = 1;
              }
            }
            int flat = (dirs[0] && dirs[1]) || (dirs[2] && dirs[3]);
            if (num_matches <= 1 || flat) {
              int id = AC_RND(c, n_alt);
              LAYER(layer1, x, y) = alt1[id];
              LAYER(layer2, x, y) = alt2[id];
            }
          }
        }
      }
    }
    copy_layer(l1c, layer1);
    copy_layer(l2c, layer2);
  }

  for (int x = 15; x < map_w - 15; x++) {
    int y = map_h - 2;
    while (y > 10) {
      int num = 0;
      int start_y = y + 1;
      while (y > 10 && LAYER(layer1, x, y) == 5) {
        num++;
        y--;
      }
      if (num > 0 && num < 7) {
        for (int yy = 0; yy <= num; yy++) {
          AC_AT(c, layer1, x, yy + y) = AC_AT(c, layer1, x, start_y);
          AC_AT(c, layer2, x, yy + y) = AC_AT(c, layer2, x, start_y);
        }
      }
      if (num == 0) y--;
    }
  }

  /* edge/transition tiles */
  copy_layer(l1c, layer1);
  apply_group(c, cave_pats_a, CAVE_NPATS_A, l1c, layer1, layer2);
  copy_layer(l1c, layer1);   /* second pass */
  apply_group(c, cave_pats_b, CAVE_NPATS_B, l1c, layer1, layer2);

  /* perlin stuff */
  int perlin_z = AC_RND(c, 2000);
  for (int y = 1; y < map_h - 1; y++) {
    for (int x = 1; x < map_w - 1; x++) {
      int dist = 256;
      if (start_node) dist = d2i(sqrt((double)((x - start_node->x) * (x - start_node->x) + (y - start_node->y) * (y - start_node->y))));
      int here = LAYER(layer1, x, y);
      if (dist > 6 && (here == 43 || here == 70) && ac_perlin_noise(x * 40 / (double)map_w, y * 40 / (double)map_h, perlin_z) > 0.4) {
        if (LAYER(layer1, x - 1, y) == here && LAYER(layer1, x + 1, y) == here && LAYER(layer1, x, y + 1) == here &&
            LAYER(layer1, x, y - 1) == here)
          LAYER(layer1, x, y) = 168;
      } else if ((here == 43 || here == 70) && ac_perlin_noise(x * 20 / (double)map_w, y * 20 / (double)map_h, perlin_z) < -0.2 &&
                 (AC_RND(c, 4)) != 0) {
        if (LAYER(layer1, x - 1, y) == here && LAYER(layer1, x + 1, y) == here && LAYER(layer1, x, y + 1) == here &&
            LAYER(layer1, x, y - 1) == here)
          LAYER(layer1, x, y) = (uint8_t)((AC_RND(c, 2)) == 0 ? 29 : 45);
      }
    }
  }
}
