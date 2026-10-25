/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Ancient Cave floor generator "forest".  Derived from processing/ancientcave/mapgen/forest/{ForestGenerator,ForestCaveRooms,ForestConstants,
 * ForestMountainLedges,ForestTileEdges}.cs (Author of every file: Moppleton).  Translated to C 2026-10-18.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sr_p3_ac.h"

#define LEDGE_SIZE 5
#define INSET 10
#define OUT_W 128
#define OUT_H 128
#define IN_W 128
#define IN_H 64
#define BOSS_W 32
#define BOSS_H 32

/* ForestConstants */
enum {
  L1_TALL_GRASS = 126, L1_TALL_GRASS_U = 110, L1_TALL_GRASS_L = 125, L1_TALL_GRASS_R = 127, L1_TALL_GRASS_B = 142,
  L1_TALL_GRASS_BL = 141, L1_TALL_GRASS_BR = 143, L1_TALL_GRASS_UL = 109, L1_TALL_GRASS_UR = 111,
  L1_EMPTY = 140, L1_EMPTY_BLOCK = 186, L1_EMPTY_DOOR = 185,
  L1_CAVE_TOP_LEFT = 145, L1_CAVE_TOP_RIGHT = 146, L1_CAVE_DOOR_LEFT = 161, L1_CAVE_DOOR_RIGHT = 162,
  L1_TREE_UPPERLEFT = 116, L1_TREE_UPPERRIGHT = 117, L1_TREE_MIDLEFT = 132, L1_TREE_MIDRIGHT = 133,
  L1_TREE_UPPERLEFT_EDGE = 112, L1_TREE_UPPERRIGHT_EDGE = 113, L1_TREE_MIDLEFT_EDGE = 128, L1_TREE_MIDRIGHT_EDGE = 129,
  L1_TREE_LOWLEFT_EDGE = 114, L1_TREE_LOWRIGHT_EDGE = 115,
  L1_BUSH = 5, L1_STEPS = 103,
  L1_LEDGE_LEFT_TOPMOST = 18, L1_LEDGE_LEFT_MIDTOP = 34, L1_LEDGE_LEFT_MIDBOTTOM = 50, L1_LEDGE_LEFT_BOTTOMMOST = 75,
  L1_LEDGE_RIGHT_TOPMOST = 19, L1_LEDGE_RIGHT_MIDTOP = 35, L1_LEDGE_RIGHT_MIDBOTTOM = 51, L1_LEDGE_RIGHT_BOTTOMMOST = 76,
  L2_GREEN = 187, L2_SHORT_GRASS = 97, L2_SHORT_GRASS_BR = 64, L2_SHORT_GRASS_BL = 65, L2_SHORT_GRASS_UR = 66, L2_SHORT_GRASS_UL = 67,
  L2_TALL_GRASS_DIRT = 180, L2_TALL_GRASS_DIRT_U = 177, L2_TALL_GRASS_DIRT_L = 179, L2_TALL_GRASS_DIRT_R = 181, L2_TALL_GRASS_DIRT_B = 183,
  L2_TALL_GRASS_DIRT_UL = 148, L2_TALL_GRASS_DIRT_UR = 149, L2_TALL_GRASS_DIRT_BL = 164, L2_TALL_GRASS_DIRT_BR = 165,
  L2_FLOWER_A = 80, L2_FLOWER_B = 81, L2_STUMP_LEFT = 70, L2_STUMP_RIGHT = 71, L2_WATERFALL = 124
};

static int is_tall_grass(int v) {
  return (v >= L1_TALL_GRASS_UL && v <= L1_TALL_GRASS_UR) || (v >= L1_TALL_GRASS_L && v <= L1_TALL_GRASS_R) ||
         (v >= L1_TALL_GRASS_BL && v <= L1_TALL_GRASS_BR);
}
static int is_grass(int v) { return v == 97 || v == 71 || v == 70; }
static int is_grass_or_tall(int l1, int l2) { return is_grass(l2) || is_tall_grass(l1); }
static int isnt_grass(int v) { return v == 187; }

/* (int) of a double as the oracle runtime does it: truncation, saturating, NaN gives 0 (0/0 when two nodes coincide) */
static int cs_d2i(double v) {
  if (v != v) return 0;
  if (v >= 2147483648.0) return INT32_MAX;
  if (v <= -2147483649.0) return INT32_MIN;
  return (int)v;
}

static const struct ac_palsrc outside_boss_pals[6] = {
    {NULL, 26}, {"acforest.fall.bin", 26}, {"acforest.winter.bin", 26}, {"acforest.spring.bin", 26}, {"acforest.night.bin", 26},
    {"acforest.sunset.bin", 26}};
static const struct ac_palsrc inside_pals[4] = {{NULL, 69}, {"accave.undinecave.bin", 69}, {"accave.fierycave.bin", 69}, {"accave.graycave.bin", 69}};

static const uint8_t npc_ids[33] = {0x99, 0x9C, 0x9E, 0x9F, 0xA0, 0xA1, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAC, 0xAD, 0xAE, 0xAF, 0xB1,
                                    0xB6, 0xB7, 0xBB, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB};

/* ---------------------------------------------------------------------------------------------------- tile edges */
static void copy_layer(struct ac_layer *dst, const struct ac_layer *src) { memcpy(dst->d, src->d, (size_t)src->w * (size_t)src->h); }

static void pat(struct sr_ctx *c, int width, int n, const char *const *search, const char *const *inject, const char *const *alt,
                const struct ac_layer *old_l, struct ac_layer *new_l, struct ac_layer *alt_l) {
  struct ac_pattern p;
  p.search = search;
  p.inject = inject;
  p.alt_inject = alt;
  p.width = width;
  p.n = n;
  ac_apply_pattern(c, &p, old_l, new_l, alt_l, NULL);
}
typedef const char *const sarr[];

static int is_ledge_tile(int v) {
  static const uint8_t ledge[] = {18, 19, 34, 35, 50, 51, 75, 76, 103, 1, 49, 33, 65, 4, 52, 36, 68, 145, 146, 161, 162, 6, 63, 79, 54, 9, 62, 78, 68};
  for (size_t i = 0; i < sizeof ledge; i++)
    if (v == ledge[i]) return 1;
  return 0;
}

static void add_tile_edges(struct sr_ctx *c, struct ac_layer *layer1, struct ac_layer *layer2) {
  int width = layer1->w, height = layer1->h;
  struct ac_layer *copy1 = ac_layer_new(c, width, height);
  copy_layer(copy1, layer1);
  static const uint8_t original[8] = {L1_TREE_UPPERRIGHT, L1_TREE_UPPERLEFT, L1_TREE_MIDRIGHT, L1_TREE_MIDLEFT,
                                      L1_TREE_UPPERLEFT, L1_TREE_UPPERRIGHT, L1_TREE_MIDLEFT, L1_TREE_MIDRIGHT};

  /* revert to blocks of the original pattern */
  for (int y = 0; y < height; y += 4) {
    for (int x = 0; x < width; x += 2) {
      int num_ledges = 0, tree_matches = 0;
      for (int dy = 0; dy < 4; dy++)
        for (int dx = 0; dx < 2; dx++) {
          int val = AC_AT(c, layer1, x + dx, y + dy);
          if (val == original[dy * 2 + dx]) tree_matches++;
          if (is_ledge_tile(val)) num_ledges++;
        }
      if (num_ledges > 0) {
        for (int dy = 0; dy < 4; dy++)
          for (int dx = 0; dx < 2; dx++) {
            int val = AC_AT(c, layer1, x + dx, y + dy);
            if (!is_ledge_tile(val)) AC_AT(c, layer1, x + dx, y + dy) = L1_EMPTY;
          }
      } else if (tree_matches < 8) {
        for (int dy = 0; dy < 4; dy++)
          for (int dx = 0; dx < 2; dx++) {
            int val = AC_AT(c, layer1, x + dx, y + dy);
            if (tree_matches < 4) {
              if (val == original[dy * 2 + dx]) AC_AT(c, layer1, x + dx, y + dy) = L1_EMPTY;
            } else if (val == L1_EMPTY) {
              AC_AT(c, layer1, x + dx, y + dy) = original[dy * 2 + dx];
            }
          }
      }
    }
  }
  copy_layer(copy1, layer1);

  /* fix bottoms of trees */
  pat(c, 1, 3, (sarr){"116", "132", "!117"}, (sarr){"115", "186", "-"}, (sarr){"-", "71", "-"}, copy1, layer1, layer2);
  pat(c, 1, 3, (sarr){"117", "133", "!116"}, (sarr){"114", "186", "-"}, (sarr){"-", "70", "-"}, copy1, layer1, layer2);
  /* fix right sides of trees */
  pat(c, 2, 2, (sarr){"116", "!117"}, (sarr){"115", "-"}, (sarr){"-", "-"}, copy1, layer1, layer2);
  pat(c, 2, 2, (sarr){"132", "!133"}, (sarr){"186", "-"}, (sarr){"71", "-"}, copy1, layer1, layer2);
  pat(c, 2, 2, (sarr){"117", "!116"}, (sarr){"113", "-"}, (sarr){"-", "-"}, copy1, layer1, layer2);
  pat(c, 2, 2, (sarr){"133", "!132"}, (sarr){"129", "-"}, (sarr){"-", "-"}, copy1, layer1, layer2);
  /* fix tops of trees */
  pat(c, 1, 3, (sarr){"!133", "116", "132"}, (sarr){"-", "112", "128"}, NULL, copy1, layer1, NULL);
  pat(c, 1, 3, (sarr){"!132", "117", "133"}, (sarr){"-", "113", "129"}, NULL, copy1, layer1, NULL);
  /* fix left sides of trees */
  pat(c, 2, 2, (sarr){"!116", "117"}, (sarr){"-", "114"}, (sarr){"-", "-"}, copy1, layer1, layer2);
  pat(c, 2, 2, (sarr){"!132", "133"}, (sarr){"-", "186"}, (sarr){"-", "70"}, copy1, layer1, layer2);
  pat(c, 2, 2, (sarr){"!117", "116"}, (sarr){"-", "112"}, (sarr){"-", "-"}, copy1, layer1, layer2);
  pat(c, 2, 2, (sarr){"!133", "132"}, (sarr){"-", "128"}, NULL, copy1, layer1, layer2);

  copy_layer(copy1, layer1);

  /* corners leftover after the above */
  {
    const char *ledge_plus_empty = "{18,19,34,35,50,51,75,76,103,1,49,33,65,4,52,36,68,145,146,161,162,140}";
    const char *const s1[] = {"113", "129", ledge_plus_empty};
    pat(c, 1, 3, s1, (sarr){"140", "140", "-"}, (sarr){"-", "97", "-"}, copy1, layer1, layer2);
    const char *const s2[] = {"112", ledge_plus_empty, "128", ledge_plus_empty};
    pat(c, 2, 4, s2, (sarr){"140", "-", "140", "-"}, (sarr){"-", "-", "97", "-"}, copy1, layer1, layer2);
    const char *const s3[] = {"112", "128", ledge_plus_empty};
    pat(c, 1, 3, s3, (sarr){"140", "140", "-"}, (sarr){"-", "97", "-"}, copy1, layer1, layer2);
    const char *const s4[] = {ledge_plus_empty, "114", "186"};
    pat(c, 1, 3, s4, (sarr){"-", "140", "140"}, (sarr){"-", "-", "97"}, copy1, layer1, layer2);
  }

  /* grass edges on layer 2 */
  for (int i = 0; i < 10; i++) {
    for (int y = 0; y < height; y++) {
      int top_most = y == 0, bottom_most = y == height - 1;
      for (int x = 0; x < width; x++) {
        int left_most = x == 0, right_most = x == width - 1;
        int middle = !left_most && !right_most && !top_most && !bottom_most;
        if (middle && (LAYER(layer2, x, y) == L2_GREEN || LAYER(layer2, x, y) == L2_SHORT_GRASS) && isnt_grass(LAYER(layer2, x - 1, y)) &&
            isnt_grass(LAYER(layer2, x, y - 1)) && is_grass(LAYER(layer2, x + 1, y)) && is_grass(LAYER(layer2, x, y + 1)))
          LAYER(layer2, x, y) = L2_SHORT_GRASS_BR;
        if (middle && (LAYER(layer2, x, y) == L2_GREEN || LAYER(layer2, x, y) == L2_SHORT_GRASS) && isnt_grass(LAYER(layer2, x - 1, y)) &&
            is_grass(LAYER(layer2, x, y - 1)) && is_grass(LAYER(layer2, x + 1, y)) && isnt_grass(LAYER(layer2, x, y + 1)))
          LAYER(layer2, x, y) = L2_SHORT_GRASS_UR;
        if (middle && (LAYER(layer2, x, y) == L2_GREEN || LAYER(layer2, x, y) == L2_SHORT_GRASS) && is_grass(LAYER(layer2, x - 1, y)) &&
            isnt_grass(LAYER(layer2, x, y - 1)) && isnt_grass(LAYER(layer2, x + 1, y)) && is_grass(LAYER(layer2, x, y + 1)))
          LAYER(layer2, x, y) = L2_SHORT_GRASS_BL;
        if (middle && (LAYER(layer2, x, y) == L2_GREEN || LAYER(layer2, x, y) == L2_SHORT_GRASS) && is_grass(LAYER(layer2, x - 1, y)) &&
            is_grass(LAYER(layer2, x, y - 1)) && isnt_grass(LAYER(layer2, x + 1, y)) && isnt_grass(LAYER(layer2, x, y + 1)))
          LAYER(layer2, x, y) = L2_SHORT_GRASS_UL;
      }
    }
  }

  /* perlin noise for grass and bush placement */
  int perlin_z = AC_RND(c, 2000);
  for (int y = 1; y < height - 1; y++) {
    for (int x = 1; x < width - 1; x++) {
      if (LAYER(layer1, x, y) == L1_EMPTY && LAYER(layer2, x, y) == L2_SHORT_GRASS &&
          ac_perlin_noise(x * 20 / (double)width, y * 20 / (double)height, perlin_z) > 0.2) {
        if (LAYER(layer1, x, y - 1) != L1_CAVE_DOOR_LEFT && LAYER(layer1, x, y - 1) != L1_CAVE_DOOR_RIGHT) LAYER(layer1, x, y) = 5;
      } else if (LAYER(layer1, x, y) == L1_EMPTY && LAYER(layer2, x, y) == L2_SHORT_GRASS &&
                 ac_perlin_noise(x * 20 / (double)width, y * 20 / (double)height, perlin_z) < -0.2) {
        int u = is_grass_or_tall(LAYER(layer1, x, y - 1), LAYER(layer2, x, y - 1));
        int l = is_grass_or_tall(LAYER(layer1, x - 1, y), LAYER(layer2, x - 1, y));
        int r = is_grass_or_tall(LAYER(layer1, x + 1, y), LAYER(layer2, x + 1, y));
        int b = is_grass_or_tall(LAYER(layer1, x, y + 1), LAYER(layer2, x, y + 1));
        if (u && l && r && b) {
          LAYER(layer1, x, y) = L1_TALL_GRASS;
          LAYER(layer2, x, y) = L2_TALL_GRASS_DIRT;
        }
      }
    }
  }

  /* edges on the tall grass */
  for (int i = 0; i < 10; i++) {
    for (int y = 1; y < height - 1; y++) {
      for (int x = 1; x < width - 1; x++) {
        if (!is_tall_grass(LAYER(layer1, x, y))) continue;
        int u = is_tall_grass(LAYER(layer1, x, y - 1));
        int l = is_tall_grass(LAYER(layer1, x - 1, y));
        int r = is_tall_grass(LAYER(layer1, x + 1, y));
        int b = is_tall_grass(LAYER(layer1, x, y + 1));
        if (u && l && r && b) {
          /* stay as is */
        } else if (u && r && l && !b) {
          LAYER(layer1, x, y) = L1_TALL_GRASS_B;
          LAYER(layer2, x, y) = L2_TALL_GRASS_DIRT_B;
        } else if (!u && r && l && b) {
          LAYER(layer1, x, y) = L1_TALL_GRASS_U;
          LAYER(layer2, x, y) = L2_TALL_GRASS_DIRT_U;
        } else if (u && !r && l && b) {
          LAYER(layer1, x, y) = L1_TALL_GRASS_R;
          LAYER(layer2, x, y) = L2_TALL_GRASS_DIRT_R;
        } else if (u && r && !l && b) {
          LAYER(layer1, x, y) = L1_TALL_GRASS_L;
          LAYER(layer2, x, y) = L2_TALL_GRASS_DIRT_L;
        } else if (!u && r && !l && b) {
          LAYER(layer1, x, y) = L1_TALL_GRASS_UL;
          LAYER(layer2, x, y) = L2_TALL_GRASS_DIRT_UL;
        } else if (!u && !r && l && b) {
          LAYER(layer1, x, y) = L1_TALL_GRASS_UR;
          LAYER(layer2, x, y) = L2_TALL_GRASS_DIRT_UR;
        } else if (u && r && !l && !b) {
          LAYER(layer1, x, y) = L1_TALL_GRASS_BL;
          LAYER(layer2, x, y) = L2_TALL_GRASS_DIRT_BL;
        } else if (u && !r && l && !b) {
          LAYER(layer1, x, y) = L1_TALL_GRASS_BR;
          LAYER(layer2, x, y) = L2_TALL_GRASS_DIRT_BR;
        } else {
          LAYER(layer1, x, y) = L1_EMPTY;
          LAYER(layer2, x, y) = 97;
        }
      }
    }
  }
}

/* -------------------------------------------------------------------------------------------------- mountain ledges */
static void make_ledges_with_caves(struct sr_ctx *c, struct ac_map *map, const int *ledge_ys, int n_ledges, const struct ac_list *all_nodes,
                                   int ledge_height) {
  struct ac_layer *layer1 = map->layer1;
  int map_w = layer1->w;
  int num_straight = 0;
  int num_indoor = AC_RND(c, 3) + 4;
  struct ac_list door_xs = {0}, door_ys = {0};

  for (int li = 0; li < n_ledges; li++) {
    int y_value = ledge_ys[li];
    struct ac_list steps_pos = {0};
    for (int a = 0; a < all_nodes->n; a++) {
      struct ac_node *n1 = (struct ac_node *)AC_PTR(all_nodes->p[a]);
      for (int b2 = 0; b2 < n1->connections.n; b2++) {
        struct ac_node *n2 = (struct ac_node *)AC_PTR(n1->connections.p[b2]);
        for (int x = 0; x < map_w - 1; x++) {
          if (n1->y != n2->y) {
            if (n1->x == n2->x) {
              if (x == n1->x && ((n1->y < y_value + 5 && n2->y >= y_value - 5) || (n2->y < y_value + 5 && n1->y >= y_value - 5)))
                ac_list_add(c, &steps_pos, x);
            } else if ((n1->y < y_value + 5 && n2->y >= y_value - 5) || (n2->y < y_value + 5 && n1->y >= y_value - 5)) {
              double m = (n2->y - n1->y) / (double)(n2->x - n1->x);
              double bb = n2->y - (m * n2->x);
              double x_at_y = (y_value - bb) / m;
              if (x <= x_at_y && x + 1 > x_at_y) ac_list_add(c, &steps_pos, x);
            }
          }
        }
      }
    }

    int y_pos = y_value, prev_y = y_value, steps = 0;
    for (int x = 8; x < map_w - 8; x++) {
      if (x == 8) {
        AC_AT(c, layer1, x, y_pos - 2) = 6;
        AC_AT(c, layer1, x, y_pos - 1) = 63;
        AC_AT(c, layer1, x, y_pos - 0) = 79;
        AC_AT(c, layer1, x, y_pos + 1) = 54;
      } else if (x == map_w - 9) {
        AC_AT(c, layer1, x, y_pos - 2) = 9;
        AC_AT(c, layer1, x, y_pos - 1) = 62;
        AC_AT(c, layer1, x, y_pos - 0) = 78;
        AC_AT(c, layer1, x, y_pos + 1) = 68;
      } else if (y_pos == prev_y) {
        if ((num_straight % 2) == 0) {
          steps = ac_list_contains(&steps_pos, x) || ac_list_contains(&steps_pos, x - 1);
          if (steps) {
            AC_AT(c, layer1, x, y_pos - 2) = L1_STEPS;
            AC_AT(c, layer1, x, y_pos - 1) = L1_STEPS;
            AC_AT(c, layer1, x, y_pos - 0) = L1_STEPS;
            AC_AT(c, layer1, x, y_pos + 1) = L1_STEPS;
          } else {
            AC_AT(c, layer1, x, y_pos - 2) = L1_LEDGE_LEFT_TOPMOST;
            AC_AT(c, layer1, x, y_pos - 1) = L1_LEDGE_LEFT_MIDTOP;
            AC_AT(c, layer1, x, y_pos - 0) = L1_LEDGE_LEFT_MIDBOTTOM;
            AC_AT(c, layer1, x, y_pos + 1) = L1_LEDGE_LEFT_BOTTOMMOST;
          }
        } else if (steps) {
          AC_AT(c, layer1, x, y_pos - 2) = L1_STEPS;
          AC_AT(c, layer1, x, y_pos - 1) = L1_STEPS;
          AC_AT(c, layer1, x, y_pos - 0) = L1_STEPS;
          AC_AT(c, layer1, x, y_pos + 1) = L1_STEPS;
        } else {
          AC_AT(c, layer1, x, y_pos - 2) = L1_LEDGE_RIGHT_TOPMOST;
          AC_AT(c, layer1, x, y_pos - 1) = L1_LEDGE_RIGHT_MIDTOP;
          AC_AT(c, layer1, x, y_pos - 0) = L1_LEDGE_RIGHT_MIDBOTTOM;
          AC_AT(c, layer1, x, y_pos + 1) = L1_LEDGE_RIGHT_BOTTOMMOST;
        }
        num_straight++;
        if ((num_straight % 2) == 1 && !steps) {
          if (AC_AT(c, layer1, x, y_pos + 2) == L1_EMPTY) {
            ac_list_add(c, &door_xs, x);
            ac_list_add(c, &door_ys, y_pos);
          }
        }
      } else if (y_pos > prev_y) {
        AC_AT(c, layer1, x, y_pos - 3) = 1;
        AC_AT(c, layer1, x, y_pos - 2) = 49;
        AC_AT(c, layer1, x, y_pos - 1) = 33;
        AC_AT(c, layer1, x, y_pos + 0) = 49;
        AC_AT(c, layer1, x, y_pos + 1) = 65;
        num_straight = 0;
        steps = 0;
      } else {
        AC_AT(c, layer1, x, y_pos - 2) = 4;
        AC_AT(c, layer1, x, y_pos - 1) = 52;
        AC_AT(c, layer1, x, y_pos - 0) = 36;
        AC_AT(c, layer1, x, y_pos + 1) = 52;
        AC_AT(c, layer1, x, y_pos + 2) = 68;
        num_straight = 0;
        steps = 0;
      }

      prev_y = y_pos;
      if (num_straight > 0 && (num_straight % 2) == 0 && (AC_RND(c, 2)) == 0 && !ac_list_contains(&steps_pos, x) &&
          !ac_list_contains(&steps_pos, x + 1) && x < map_w - 10) {
        if (y_pos <= y_value - ledge_height) y_pos++;
        else if (y_pos >= y_value + ledge_height) y_pos--;
        else y_pos += AC_RND(c, 2) == 0 ? -1 : 1;
      }
    }
  }

  for (int i = 0; i < num_indoor && door_xs.n > 0; i++) {
    int index = AC_RND(c, door_xs.n);
    int x = (int)ac_list_get(c, &door_xs, index);
    int y = (int)ac_list_get(c, &door_ys, index);
    ac_list_remove_at(c, &door_xs, index);
    ac_list_remove_at(c, &door_ys, index);
    AC_AT(c, layer1, x, y) = L1_CAVE_TOP_LEFT;
    AC_AT(c, layer1, x + 1, y) = L1_CAVE_TOP_RIGHT;
    AC_AT(c, layer1, x, y + 1) = L1_CAVE_DOOR_LEFT;
    AC_AT(c, layer1, x + 1, y + 1) = L1_CAVE_DOOR_RIGHT;
    ac_dict_set(c, &map->alt_exit, AC_XYKEY(x, y + 1), i);
    ac_dict_set(c, &map->alt_exit, AC_XYKEY(x + 1, y + 1), i);
    ac_dict_set(c, &map->alt_entry, i, AC_VAL(ac_xy_new(c, x, y + 2)));
  }
}

/* ----------------------------------------------------------------------------------------------------- cave rooms */
static void room_shapes(struct sr_ctx *c, struct ac_layer *layer1, struct ac_layer *layer2) {
  struct ac_layer *copy1 = ac_layer_new(c, 128, 64), *copy2 = ac_layer_new(c, 128, 64);
  copy_layer(copy1, layer1);
  copy_layer(copy2, layer2);
  for (int y = 2; y < 64 - 2; y += 2) {
    for (int x = 2; x < 126; x += 2) {
      if (LAYER(layer1, x, y) == 70) {
        if (LAYER(layer1, x, y - 2) == 5 || LAYER(layer1, x - 2, y) == 5 || LAYER(layer1, x + 2, y) == 5 || LAYER(layer1, x, y + 2) == 5) {
          int a = LAYER(layer1, x, y), b = LAYER(layer1, x + 1, y), d = LAYER(layer1, x + 1, y - 1), e = LAYER(layer1, x, y - 1);
          if (a != 173 && a != 174 && a != 180 && b != 173 && b != 174 && b != 180 && d != 173 && d != 174 && d != 180 && e != 173 && e != 174 &&
              e != 180) {
            if (AC_RND(c, 2) == 0) {
              LAYER(copy1, x, y) = 5;
              LAYER(copy1, x + 1, y) = 5;
              LAYER(copy1, x + 1, y - 1) = 5;
              LAYER(copy1, x, y - 1) = 5;
            }
          }
        }
      }
    }
  }
  copy_layer(layer1, copy1);
  copy_layer(layer2, copy2);
}

static void place_walls(struct sr_ctx *c, struct ac_layer *layer1, struct ac_layer *layer2) {
  static const uint8_t top_wall[5] = {8, 11, 14, 17, 20};
  static const uint8_t diag1[6] = {78, 9, 12, 15, 18, 21};
  static const uint8_t diag2[6] = {79, 7, 10, 13, 16, 19};
  struct ac_layer *copy1 = ac_layer_new(c, 128, 64), *copy2 = ac_layer_new(c, 128, 64);
  copy_layer(copy1, layer1);
  copy_layer(copy2, layer2);

  for (int y = 1; y < 64 - 1; y++) {
    for (int x = 1; x < 127; x++) {
      int corner = 0;
      if (LAYER(layer1, x, y) == 5 && LAYER(layer1, x + 1, y) == 70 && LAYER(layer1, x, y + 1) == 70) {
        for (int i = 0; i < 6; i++) {
          int yy = y - (6 - i) + 1;
          if (yy >= 0) LAYER(copy1, x, yy) = diag1[i];
        }
        corner = 1;
      }
      if (LAYER(layer1, x, y) == 5 && LAYER(layer1, x - 1, y) == 70 && LAYER(layer1, x, y + 1) == 70) {
        for (int i = 0; i < 6; i++) {
          int yy = y - (6 - i) + 1;
          if (yy >= 0) LAYER(copy1, x, yy) = diag2[i];
        }
        corner = 1;
      }
      if (LAYER(layer1, x, y) == 5 && LAYER(layer1, x, y + 1) == 70 && !corner) {
        for (int i = 0; i < 5; i++) {
          int yy = y - (5 - i) + 1;
          if (yy >= 0) LAYER(copy1, x, yy) = top_wall[i];
        }
      }
    }
  }
  copy_layer(layer1, copy1);
  copy_layer(layer2, copy2);

  for (int y = 1; y < 64 - 1; y++) {
    for (int x = 1; x < 127; x++) {
      int corner = 0, ledge_above = 0, ledge_right = 0, ledge_left = 0;
      for (int i = 0; i < 5; i++) {
        int v = top_wall[i];
        if (LAYER(layer1, x, y - 1) == v) ledge_above = 1;
        if (v != 8 && LAYER(layer1, x - 1, y) == v) ledge_left = 1;
        if (v != 8 && LAYER(layer1, x + 1, y) == v) ledge_right = 1;
      }
      for (int i = 0; i < 6; i++) {
        int v = diag1[i];
        if (LAYER(layer1, x, y - 1) == v) ledge_above = 1;
        if (v != 78 && v != 9 && LAYER(layer1, x - 1, y) == v) ledge_left = 1;
        if (v != 78 && v != 9 && LAYER(layer1, x + 1, y) == v) ledge_right = 1;
      }
      for (int i = 0; i < 6; i++) {
        int v = diag2[i];
        if (LAYER(layer1, x, y - 1) == v) ledge_above = 1;
        if (v != 79 && v != 7 && LAYER(layer1, x - 1, y) == v) ledge_left = 1;
        if (v != 79 && v != 7 && LAYER(layer1, x + 1, y) == v) ledge_right = 1;
      }
      if ((LAYER(layer1, x, y) == 5 || LAYER(layer1, x, y) == 79) && (LAYER(layer1, x - 1, y) == 70 || ledge_left) &&
          (LAYER(layer1, x, y - 1) == 70 || ledge_above)) {
        if (LAYER(layer1, x, y - 1) >= 19 && LAYER(layer1, x, y - 1) <= 21) LAYER(copy1, x, y) = 71;
        else if (ledge_above) LAYER(copy1, x, y) = 1;
        else LAYER(copy1, x, y) = 71;
        corner = 1;
      }
      if ((LAYER(layer1, x, y) == 5 || LAYER(layer1, x, y) == 78) && (LAYER(layer1, x + 1, y) == 70 || ledge_right) &&
          (LAYER(layer1, x, y - 1) == 70 || ledge_above)) {
        if (LAYER(layer1, x, y - 1) >= 19 && LAYER(layer1, x, y - 1) <= 21) LAYER(copy1, x, y) = 73;
        else if (ledge_above) LAYER(copy1, x, y) = 3;
        else LAYER(copy1, x, y) = 73;
        corner = 1;
      }
      if (LAYER(layer1, x, y + 1) == 5 && LAYER(layer1, x, y) == 70 && !corner) LAYER(copy1, x, y + 1) = 2;
      if (LAYER(layer1, x, y) == 5 && (LAYER(layer1, x - 1, y) == 70 || ledge_left) && !corner) LAYER(copy1, x, y) = 4;
      if (LAYER(layer1, x, y) == 5 && (LAYER(layer1, x + 1, y) == 70 || ledge_right) && !corner) LAYER(copy1, x, y) = 6;
    }
  }
  copy_layer(layer1, copy1);
  copy_layer(layer2, copy2);
}

static void populate_indoor(struct sr_ctx *c, struct ac_map *indoor, int num_rooms, struct ac_layer *layer1, struct ac_layer *layer2) {
  const int buffer = 4;
  memset(layer1->d, 5, (size_t)layer1->w * (size_t)layer1->h);
  for (int i = 0; i < num_rooms; i++) {
    int center1 = 15, center2 = 16;
    int xo = 32 * (i % 4), yo = 32 * (i / 4);
    int x_left = buffer, x_right = 31 - buffer, y_door = 31 - buffer;
    for (int y = buffer; y < 32 - buffer; y++) {
      for (int x = buffer; x < 32 - buffer; x++) {
        if (x != x_left && x != x_right) {
          if (y == y_door) {
            if (x == center1 || x == center2) {
              AC_AT(c, layer1, x + xo, y + yo) = 119;
              if (x == center1) ac_dict_set(c, &indoor->alt_entry, i, AC_VAL(ac_xy_new(c, x + xo, y + yo - 1)));
              ac_dict_set(c, &indoor->alt_exit, AC_XYKEY(x + xo, y + yo), i);
            }
          } else if (y == 31 - buffer - 1 && (x == center1 || x == center2)) {
            AC_AT(c, layer1, x + xo, y + yo) = x == center1 ? 173 : 174;
          } else {
            AC_AT(c, layer1, x + xo, y + yo) = 70;
          }
        }
      }
    }
  }
  room_shapes(c, layer1, layer2);
  place_walls(c, layer1, layer2);
}

/* ----------------------------------------------------------------------------------------------------- generator */
static struct ac_gen_ctx *forest_get_ctx(struct sr_ctx *c) {
  struct ac_gen_ctx *g = ac_gen_ctx_new(c);
  g->outdoor_w = OUT_W;
  g->outdoor_h = OUT_H;
  g->indoor_w = IN_W;
  g->indoor_h = IN_H;
  g->boss_w = BOSS_W;
  g->boss_h = BOSS_H;
  return g;
}

static int node_radius(struct sr_ctx *c, const struct ac_node *n) { return (int)ac_dict_get(c, &n->values, AC_STRKEY("radius")); }

static struct ac_node *make_random_outdoor_node(struct sr_ctx *c, const int *ledge_ys, int n_ledges) {
  int usable_x = OUT_W - INSET - INSET;
  int usable_y = OUT_H - INSET - INSET;
  int ledge_check = LEDGE_SIZE + 2;
  int actual_y = usable_y - n_ledges * ledge_check * 2;
  struct ac_node *n = ac_node_new(c);
  n->x = ac_mod(c, sr_random_next(&c->rf), usable_x) + INSET;
  n->y = ac_mod(c, sr_random_next(&c->rf), actual_y) + INSET;
  n->center_x = n->x;
  for (int i = 0; i < n_ledges; i++)
    if (n->y >= ledge_ys[i] - ledge_check) n->y += ledge_check * 2;
  n->center_y = n->y;
  ac_dict_set(c, &n->values, AC_STRKEY("radius"), AC_RND(c, 4) + 4);
  return n;
}

static void put_enemy(struct sr_ctx *c, struct ac_map *m, int x, int y, int type) {
  uint8_t *e = ac_map_add_object(c, m->md);
  ac_obj_set_xpos(e, (uint8_t)x);
  ac_obj_set_ypos(e, (uint8_t)y);
  ac_obj_set_species(e, type);
  ac_obj_set_vis_flag(e, 0x00);
  ac_obj_set_vis_min(e, 0x00);
  ac_obj_set_vis_max(e, 0x0F);
  ac_obj_set_event(e, 0x01);
  ac_obj_set_direction(e, 2);
  ac_obj_set_unknown_b7(e, 0x18);
}

static void make_outdoor_enemies(struct sr_ctx *c, struct ac_map *m, const struct ac_list *nodes, const struct ac_node *start,
                                 const struct ac_layer *layer1) {
  static const uint8_t enemy_ids[14] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x02, 0x04, 0x14, 0x29, 0x2A, 0x2B};
  int total = 0, max_enemies = 32;
  for (int k = 0; k < nodes->n; k++) {
    struct ac_node *n = (struct ac_node *)AC_PTR(nodes->p[k]);
    int dist_x = n->center_x - start->center_x, dist_y = n->center_y - start->center_y;
    if (dist_x < 0) dist_x = -dist_x;
    if (dist_y < 0) dist_y = -dist_y;
    if (dist_x > OUT_W / 2) dist_x = OUT_W - dist_x;
    if (dist_y > OUT_H / 2) dist_y = OUT_H - dist_y;
    if (n != start) {
      int num_enemies = AC_RND(c, 2);
      if (dist_x > 16 || dist_y > 16) {
        int rad = node_radius(c, n);
        for (int e = 0; e < num_enemies && total < max_enemies; e++) {
          int ex = n->x + AC_RND(c, rad * 2 - 2) - (rad - 1);
          int ey = n->y + AC_RND(c, rad * 2 - 2) - (rad - 1);
          int iters = 0;
          while (AC_AT(c, layer1, ex, ey) != L1_EMPTY && iters < 100) {
            ex = n->x + AC_RND(c, rad * 2 - 2) - (rad - 1);
            ey = n->y + AC_RND(c, rad * 2 - 2) - (rad - 1);
            iters++;
          }
          int type = enemy_ids[AC_RND(c, 14)];
          put_enemy(c, m, ex, ey, type);
          total++;
        }
        if (total >= max_enemies) break;
      }
    }
  }
}

static struct ac_map *forest_outdoor(struct sr_ctx *c, struct ac_gen_ctx *g) {
  struct ac_layer *layer1 = ac_layer_new(c, OUT_W, OUT_H);
  struct ac_layer *layer2 = ac_layer_new(c, OUT_W, OUT_H);
  struct ac_map *map = ac_map_new(c);
  map->layer1 = layer1;
  map->layer2 = layer2;

  int num_nodes = 20 + AC_RND(c, 100);

  int ledge_ys[4];
  int n_ledges = AC_RND(c, 4) + 1;
  for (int j = 0; j < n_ledges; j++) {
    int y_div = (OUT_H - INSET - INSET) / n_ledges;
    int y = INSET + (int)((j + 0.5) * y_div) + AC_RND(c, y_div / 2) - y_div / 4;
    ledge_ys[j] = y;
  }

  struct ac_node *start = make_random_outdoor_node(c, ledge_ys, n_ledges);
  start->name = "start";
  map->entry_pos = ac_xy_new(c, start->x, start->y);

  for (int y = 0; y < OUT_H / 4; y++)
    for (int x = 0; x < OUT_W / 2; x++) {
      LAYER(layer1, x * 2, y * 4) = L1_TREE_UPPERRIGHT;
      LAYER(layer1, x * 2 + 1, y * 4) = L1_TREE_UPPERLEFT;
      LAYER(layer1, x * 2, y * 4 + 1) = L1_TREE_MIDRIGHT;
      LAYER(layer1, x * 2 + 1, y * 4 + 1) = L1_TREE_MIDLEFT;
      LAYER(layer1, x * 2, y * 4 + 2) = L1_TREE_UPPERLEFT;
      LAYER(layer1, x * 2 + 1, y * 4 + 2) = L1_TREE_UPPERRIGHT;
      LAYER(layer1, x * 2, y * 4 + 3) = L1_TREE_MIDLEFT;
      LAYER(layer1, x * 2 + 1, y * 4 + 3) = L1_TREE_MIDRIGHT;
    }

  for (int y = 0; y < OUT_H; y++)
    for (int x = 0; x < OUT_W; x++) {
      int r = AC_RND(c, 100);
      if (r < 3) LAYER(layer2, x, y) = L2_FLOWER_A;
      else if (r < 6) LAYER(layer2, x, y) = L2_FLOWER_B;
      else LAYER(layer2, x, y) = L2_SHORT_GRASS;
    }

  struct ac_list map_nodes = {0};
  ac_list_add(c, &map_nodes, AC_VAL(start));
  ac_dict_set(c, &g->special_nodes, AC_STRKEY("start"), AC_VAL(start));
  for (int i = 0; i < num_nodes; i++) {
    struct ac_node *n = make_random_outdoor_node(c, ledge_ys, n_ledges);
    char *nm = (char *)sr_alloc(c, 16);
    snprintf(nm, 16, "n%d", i);
    n->name = nm;
    ac_list_add(c, &map_nodes, AC_VAL(n));
  }

  struct ac_node *end = ac_node_new(c);
  int north_quadrant = start->y < (OUT_H / 2);
  int west_quadrant = start->x < (OUT_W / 2);
  end->y = north_quadrant ? OUT_H - 1 - INSET : INSET;
  end->x = AC_RND(c, OUT_W / 2);
  if (west_quadrant) end->x += (OUT_W / 2) - INSET;
  else end->x += INSET;
  ac_dict_set(c, &end->values, AC_STRKEY("radius"), AC_RND(c, 4) + 4);
  end->name = "end";
  ac_dict_set(c, &g->special_nodes, AC_STRKEY("end"), AC_VAL(end));
  ac_list_add(c, &map_nodes, AC_VAL(end));

  /* clearings at every node */
  {
    int nn = map_nodes.n;
    int *rads = (int *)sr_alloc(c, (size_t)nn * sizeof(int) + 1);
    for (int k = 0; k < nn; k++) rads[k] = node_radius(c, (struct ac_node *)AC_PTR(map_nodes.p[k]));
    for (int y = 0; y < OUT_H; y++)
      for (int x = 0; x < OUT_W; x++)
        for (int k = 0; k < nn; k++) {
          struct ac_node *n = (struct ac_node *)AC_PTR(map_nodes.p[k]);
          int xdiff = x - n->x, ydiff = y - n->y;
          double diff = sqrt((double)(xdiff * xdiff + ydiff * ydiff));
          if (diff < rads[k]) LAYER(layer1, x, y) = L1_EMPTY;
          if (diff < rads[k] - 2) LAYER(layer2, x, y) = L2_GREEN;
        }
  }

  struct ac_list visited = {0}, unvisited = {0};
  for (int k = 0; k < map_nodes.n; k++) ac_list_add(c, &unvisited, map_nodes.p[k]);
  ac_list_add(c, &visited, AC_VAL(start));
  ac_list_remove(&unvisited, AC_VAL(start));
  while (unvisited.n > 0) {
    double min_dist = 999;
    struct ac_node *src = NULL, *dst = NULL;
    for (int a = 0; a < visited.n; a++) {
      struct ac_node *sn = (struct ac_node *)AC_PTR(visited.p[a]);
      for (int b = 0; b < unvisited.n; b++) {
        struct ac_node *dn = (struct ac_node *)AC_PTR(unvisited.p[b]);
        int xdiff = sn->x - dn->x, ydiff = sn->y - dn->y;
        double diff = sqrt((double)(xdiff * xdiff + ydiff * ydiff));
        if (diff < min_dist) {
          min_dist = diff;
          src = sn;
          dst = dn;
        }
      }
    }
    if (!src || !dst) sr_fail(c, SOMRANDO_ERR_FAILED, "Object reference not set to an instance of an object.");

    int path_radius = AC_RND(c, 3) + 3;
    for (int i = 0; i < n_ledges; i++) {
      int ly = ledge_ys[i];
      if ((src->y <= ly && dst->y > ly) || (src->y > ly && dst->y <= ly)) {
        path_radius += 4;
        break;
      }
    }

    int xd = dst->x - src->x, yd = dst->y - src->y;
    double angle = atan2((double)yd, (double)xd);
    double dx = cos(angle);
    double dy = sin(angle);
    double x_pos = src->x, y_pos = src->y;
    int iters;
    if (xd == 0) iters = cs_d2i(fabs((src->y - dst->y) / dy));
    else iters = cs_d2i(fabs((src->x - dst->x) / dx));
    for (int i = 0; i <= iters; i++) {
      /* only the box around the circle can match diff < pathRadius */
      int x0 = (int)floor(x_pos) - path_radius - 2, x1 = (int)floor(x_pos) + path_radius + 2;
      int y0 = (int)floor(y_pos) - path_radius - 2, y1 = (int)floor(y_pos) + path_radius + 2;
      if (x0 < 0) x0 = 0;
      if (y0 < 0) y0 = 0;
      if (x1 > OUT_W - 1) x1 = OUT_W - 1;
      if (y1 > OUT_H - 1) y1 = OUT_H - 1;
      for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) {
          double xdiff = x - x_pos;
          double ydiff = y - y_pos;
          double diff = sqrt(xdiff * xdiff + ydiff * ydiff);
          if (diff < path_radius) LAYER(layer1, x, y) = L1_EMPTY;
        }
      x_pos += dx;
      y_pos += dy;
    }

    ac_list_add(c, &visited, AC_VAL(dst));
    ac_list_remove(&unvisited, AC_VAL(dst));
    ac_list_add(c, &src->connections, AC_VAL(dst));
    ac_list_add(c, &dst->connections, AC_VAL(src));
  }

  make_ledges_with_caves(c, map, ledge_ys, n_ledges, &map_nodes, LEDGE_SIZE);
  add_tile_edges(c, layer1, layer2);

  AC_AT(c, layer1, end->x, end->y - 1) = L1_BUSH;
  AC_AT(c, layer1, end->x, end->y + 1) = L1_BUSH;
  AC_AT(c, layer1, end->x - 1, end->y) = L1_BUSH;
  AC_AT(c, layer1, end->x + 1, end->y) = L1_BUSH;
  AC_AT(c, layer1, end->x, end->y) = L1_EMPTY_DOOR;
  AC_AT(c, layer2, end->x, end->y) = L2_WATERFALL;
  ac_list_add(c, &map->exit_locations, AC_VAL(ac_xy_new(c, (int16_t)end->x, (int16_t)end->y)));

  uint8_t *h = map->md->header;
  ac_hdr_set_walkon(h, 1);
  ac_hdr_set_tileset16(h, 2);
  ac_hdr_set_tileset8(h, 2);
  ac_hdr_set_animated_tiles(h, 1);
  ac_hdr_set_display(h, 1);
  ac_hdr_set_layer1_fg(h, 0);
  ac_hdr_set_layer2_fg(h, 1);
  ac_hdr_set_npc_palette(h, 0xFF);
  ac_hdr_set_flammie(h, 1);
  ac_hdr_set_magic_rope(h, 1);

  ac_apply_palette_source(c, map, &outside_boss_pals[AC_RND(c, 6)]);
  ac_hdr_set_palette_animated(h, 1);

  make_outdoor_enemies(c, map, &map_nodes, start, layer1);

  g->num_shared_doors = ac_dict_size(&map->alt_entry);
  g->has_num_shared_doors = 1;
  for (int k = 0; k < map_nodes.n; k++) ac_list_add(c, &g->shared_nodes, map_nodes.p[k]);
  return map;
}

static struct ac_map *forest_indoor(struct sr_ctx *c, struct ac_gen_ctx *g) {
  struct ac_map *map = ac_map_new(c);
  struct ac_layer *l1 = ac_layer_new(c, IN_W, IN_H);
  struct ac_layer *l2 = ac_layer_new(c, IN_W, IN_H);
  if (!g->has_num_shared_doors) sr_fail(c, SOMRANDO_ERR_FAILED, "The given key was not present in the dictionary.");
  int num_shared = g->num_shared_doors;
  populate_indoor(c, map, num_shared, l1, l2);

  int num_dialogue = w8_wget_int(c, "numDialogueEvents");
  for (int i = 0; i < num_shared; i++) {
    if (i >= 8) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range. Must be non-negative and less than the size of the collection.");
    int entry_x = 15 + (i % 4) * 32, entry_y = 25 + (i / 4) * 32;
    for (int n = 0; n < 3; n++) {
      int npc_x = entry_x, npc_y = entry_y - 3 - n;
      int event_num = 0x280 + AC_RND(c, num_dialogue);
      uint8_t *o = ac_map_add_object(c, map->md);
      ac_obj_set_species(o, npc_ids[AC_RND(c, 33)]);
      ac_obj_set_vis_flag(o, 0x00);
      ac_obj_set_vis_min(o, 0x00);
      ac_obj_set_vis_max(o, 0x0F);
      ac_obj_set_xpos(o, (uint8_t)npc_x);
      ac_obj_set_ypos(o, (uint8_t)npc_y);
      ac_obj_set_event(o, (uint16_t)event_num);
      ac_obj_set_unknown_b7(o, 0x08);
      ac_obj_set_direction(o, 2);
      ac_obj_set_unknown4a(o, 1);
    }
  }

  map->layer1 = l1;
  map->layer2 = l2;

  uint8_t *h = map->md->header;
  ac_hdr_set_tileset16(h, 10);
  ac_hdr_set_tileset8(h, 10);
  ac_hdr_set_display(h, 1);
  ac_hdr_set_layer1_fg(h, 0);
  ac_hdr_set_layer2_fg(h, 1);
  ac_hdr_set_npc_palette(h, 0xFF);
  ac_hdr_set_flammie(h, 1);
  ac_hdr_set_magic_rope(h, 1);
  ac_apply_palette_source(c, map, &inside_pals[AC_RND(c, 4)]);
  return map;
}

static void boss_tree_full(struct ac_layer *l, int x, int y) {
  LAYER(l, x * 2, y * 4) = L1_TREE_UPPERRIGHT;
  LAYER(l, x * 2 + 1, y * 4) = L1_TREE_UPPERLEFT;
  LAYER(l, x * 2, y * 4 + 1) = L1_TREE_MIDRIGHT;
  LAYER(l, x * 2 + 1, y * 4 + 1) = L1_TREE_MIDLEFT;
  LAYER(l, x * 2, y * 4 + 2) = L1_TREE_UPPERLEFT;
  LAYER(l, x * 2 + 1, y * 4 + 2) = L1_TREE_UPPERRIGHT;
  LAYER(l, x * 2, y * 4 + 3) = L1_TREE_MIDLEFT;
  LAYER(l, x * 2 + 1, y * 4 + 3) = L1_TREE_MIDRIGHT;
}

static struct ac_map *forest_boss(struct sr_ctx *c, struct ac_gen_ctx *g) {
  (void)g;
  struct ac_layer *b1 = ac_layer_new(c, BOSS_W, BOSS_H);
  struct ac_layer *b2 = ac_layer_new(c, BOSS_W, BOSS_H);

  for (int y = 0; y < BOSS_H; y++)
    for (int x = 0; x < BOSS_W; x++) {
      LAYER(b1, x, y) = L1_EMPTY;
      LAYER(b2, x, y) = L2_SHORT_GRASS;
    }

  for (int y = 0; y < BOSS_H / 4; y++) {
    for (int x = 0; x < BOSS_W / 2; x++) {
      if ((x * 2 < 4 || x * 2 >= BOSS_W - 4) || (y * 4 < 4 || y * 4 >= BOSS_H - 4)) {
        boss_tree_full(b1, x, y);
      } else if (x * 2 == 4) {
        LAYER(b1, x * 2, y * 4) = L1_TREE_UPPERRIGHT_EDGE;
        LAYER(b1, x * 2, y * 4 + 1) = L1_TREE_MIDRIGHT_EDGE;
        LAYER(b1, x * 2, y * 4 + 2) = L1_TREE_LOWRIGHT_EDGE;
        LAYER(b1, x * 2, y * 4 + 3) = L1_EMPTY_BLOCK;
        LAYER(b2, x * 2, y * 4 + 3) = L2_STUMP_RIGHT;
      } else if (x * 2 == BOSS_W - 6) {
        LAYER(b1, x * 2 + 1, y * 4) = L1_TREE_UPPERLEFT_EDGE;
        LAYER(b1, x * 2 + 1, y * 4 + 1) = L1_TREE_MIDLEFT_EDGE;
        LAYER(b1, x * 2 + 1, y * 4 + 2) = L1_TREE_LOWLEFT_EDGE;
        LAYER(b1, x * 2 + 1, y * 4 + 3) = L1_EMPTY_BLOCK;
        LAYER(b2, x * 2 + 1, y * 4 + 3) = L2_STUMP_LEFT;
      } else if (y * 4 == 4) {
        LAYER(b1, x * 2, y * 4) = L1_TREE_LOWLEFT_EDGE;
        LAYER(b1, x * 2 + 1, y * 4) = L1_TREE_LOWRIGHT_EDGE;
        LAYER(b1, x * 2, y * 4 + 1) = L1_EMPTY_BLOCK;
        LAYER(b1, x * 2 + 1, y * 4 + 1) = L1_EMPTY_BLOCK;
        LAYER(b2, x * 2, y * 4 + 1) = L2_STUMP_LEFT;
        LAYER(b2, x * 2 + 1, y * 4 + 1) = L2_STUMP_RIGHT;
      } else if (y * 4 == BOSS_H - 8) {
        LAYER(b1, x * 2, y * 4 + 2) = L1_TREE_UPPERLEFT_EDGE;
        LAYER(b1, x * 2 + 1, y * 4 + 2) = L1_TREE_UPPERRIGHT_EDGE;
        LAYER(b1, x * 2, y * 4 + 3) = L1_TREE_MIDLEFT_EDGE;
        LAYER(b1, x * 2 + 1, y * 4 + 3) = L1_TREE_MIDRIGHT_EDGE;
      }
    }
  }

  /* upper left corner */
  LAYER(b1, 4, 4) = L1_TREE_UPPERRIGHT;
  LAYER(b1, 5, 4) = L1_TREE_LOWRIGHT_EDGE;
  LAYER(b1, 4, 5) = L1_TREE_MIDRIGHT;
  LAYER(b1, 5, 5) = L1_EMPTY_BLOCK;
  LAYER(b2, 5, 5) = L2_STUMP_RIGHT;
  /* upper right corner */
  LAYER(b1, BOSS_W - 6, 4) = L1_TREE_LOWLEFT_EDGE;
  LAYER(b1, BOSS_W - 5, 4) = L1_TREE_UPPERLEFT;
  LAYER(b1, BOSS_W - 6, 5) = L1_EMPTY_BLOCK;
  LAYER(b2, BOSS_W - 6, 5) = L2_STUMP_LEFT;
  LAYER(b1, BOSS_W - 5, 5) = L1_TREE_MIDLEFT;
  /* lower left corner */
  LAYER(b1, 4, BOSS_H - 6) = L1_TREE_UPPERLEFT;
  LAYER(b1, 5, BOSS_H - 6) = L1_TREE_UPPERRIGHT_EDGE;
  LAYER(b1, 4, BOSS_H - 5) = L1_TREE_MIDLEFT;
  LAYER(b1, 5, BOSS_H - 5) = L1_TREE_MIDRIGHT_EDGE;
  /* lower right corner */
  LAYER(b1, BOSS_W - 6, BOSS_H - 6) = L1_TREE_UPPERLEFT_EDGE;
  LAYER(b1, BOSS_W - 5, BOSS_H - 6) = L1_TREE_UPPERRIGHT;
  LAYER(b1, BOSS_W - 6, BOSS_H - 5) = L1_TREE_MIDLEFT_EDGE;
  LAYER(b1, BOSS_W - 5, BOSS_H - 5) = L1_TREE_MIDRIGHT;

  const int player_x = 16, player_y = 20, boss_x = 16, boss_y = 14;
  int num_extra = 8 + AC_RND(c, 8);
  int tree_xs[16], tree_ys[16], n_trees = 0;
  for (int i = 0; i < num_extra; i++) {
    int tx = 6 + AC_RND(c, BOSS_W - 12);
    int ty = 8 + AC_RND(c, BOSS_H - 16);
    int overlap = 0;
    for (int t = 0; t < n_trees; t++) {
      if (abs(tx - tree_xs[t]) < 2 && abs(ty - tree_ys[t]) < 4) overlap = 1;
    }
    if (!overlap && (abs(tx - boss_x) > 6 || abs(ty - boss_y) > 6) && (abs(tx - player_x) > 6 || abs(ty - player_y) > 6)) {
      LAYER(b1, tx, ty) = L1_TREE_UPPERLEFT_EDGE;
      LAYER(b1, tx + 1, ty) = L1_TREE_UPPERRIGHT_EDGE;
      LAYER(b1, tx, ty + 1) = L1_TREE_MIDLEFT_EDGE;
      LAYER(b1, tx + 1, ty + 1) = L1_TREE_MIDRIGHT_EDGE;
      LAYER(b1, tx, ty + 2) = L1_TREE_LOWLEFT_EDGE;
      LAYER(b1, tx + 1, ty + 2) = L1_TREE_LOWRIGHT_EDGE;
      LAYER(b1, tx, ty + 3) = L1_EMPTY_BLOCK;
      LAYER(b1, tx + 1, ty + 3) = L1_EMPTY_BLOCK;
      LAYER(b2, tx, ty + 3) = L2_STUMP_LEFT;
      LAYER(b2, tx + 1, ty + 3) = L2_STUMP_RIGHT;
      tree_xs[n_trees] = tx;
      tree_ys[n_trees] = ty;
      n_trees++;
    }
  }

  int perlin_z = AC_RND(c, 2000);
  for (int y = 1; y < BOSS_H; y++)
    for (int x = 1; x < BOSS_W; x++) {
      double noise = ac_perlin_noise(100 + x * 7 / (double)BOSS_W, 100 + y * 7 / (double)BOSS_H, perlin_z);
      if (LAYER(b1, x, y) == L1_EMPTY && LAYER(b2, x, y) == L2_SHORT_GRASS && noise > 0.3) LAYER(b2, x, y) = L2_FLOWER_A;
      else if (LAYER(b1, x, y) == L1_EMPTY && LAYER(b2, x, y) == L2_SHORT_GRASS && noise > 0) LAYER(b2, x, y) = L2_FLOWER_B;
    }

  struct ac_map *map = ac_map_new(c);
  map->layer1 = b1;
  map->layer2 = b2;
  map->entry_pos = ac_xy_new(c, player_x, player_y);

  static const uint8_t boss_types[18] = {87, 90, 92, 94, 95, 98, 99, 101, 102, 105, 109, 111, 112, 113, 116, 120, 123, 125};
  int boss_id = boss_types[AC_RND(c, 18)];
  uint8_t *bo = ac_map_add_object(c, map->md);
  ac_obj_set_species(bo, boss_id);
  ac_obj_set_xpos(bo, boss_x);
  ac_obj_set_ypos(bo, boss_y);
  ac_obj_set_vis_flag(bo, 0x00);
  ac_obj_set_vis_min(bo, 0x00);
  ac_obj_set_vis_max(bo, 0x0F);

  uint8_t *h = map->md->header;
  ac_hdr_set_walkon(h, 0);
  ac_hdr_set_tileset16(h, 2);
  ac_hdr_set_tileset8(h, 2);
  ac_hdr_set_display(h, 1);
  ac_hdr_set_layer1_fg(h, 0);
  ac_hdr_set_layer2_fg(h, 1);
  ac_hdr_set_npc_palette(h, 0xFF);
  ac_hdr_set_flammie(h, 1);
  ac_hdr_set_magic_rope(h, 1);
  ac_apply_palette_source(c, map, &outside_boss_pals[AC_RND(c, 6)]);
  return map;
}

static const uint8_t *forest_song(struct sr_ctx *c, int *len) {
  static const uint8_t song[5] = {0x40, 0x01, 0x0E, 0x1B, 0x8F};
  (void)c;
  *len = 5;
  return song;
}

const struct ac_biome sr_p3_forest_biome = {"forest", "Forest", forest_get_ctx, forest_outdoor, forest_indoor, forest_boss, forest_song};
