/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Ancient Cave floor generator "island".  Derived from processing/ancientcave/mapgen/island/{IslandGenerator,IslandHutRooms}.cs
 * (Author: Moppleton).  Translated to C 2026-10-18.
 */
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "sr_p3_ac.h"

#define OUT_W 128
#define OUT_H 128
#define IN_W 128
#define IN_H 64
#define BOSS_W 32
#define BOSS_H 32

static const struct ac_palsrc outside_and_boss_palettes[4] = {
    {NULL, 61},
    {"acisland.greenwater.bin", 61},
    {"acisland.night.bin", 61},
    {"acisland.alt.bin", 61},
};
static const struct ac_palsrc inside_palettes[1] = {{NULL, 30}};

static const uint8_t npc_ids[] = {0x99, 0x9C, 0x9E, 0x9F, 0xA0, 0xA1, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAC, 0xAD, 0xAE, 0xAF, 0xB1,
                                  0xB6, 0xB7, 0xBB, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB};

/* DataUtil.clampToPositiveRange (m is a constant > 0 here) */
static int clampr(int v, int m) {
  if (v >= m) return v % m;
  if (v < 0) return (v % m) + m;
  return v;
}

static int nget(struct sr_ctx *c, const struct ac_node *n, const char *k) { return (int)ac_dict_get(c, &n->values, AC_STRKEY(k)); }
static void nset(struct sr_ctx *c, struct ac_node *n, const char *k, int v) { ac_dict_set(c, &n->values, AC_STRKEY(k), v); }
static int nhas(const struct ac_node *n, const char *k) { return ac_dict_has(&n->values, AC_STRKEY(k)); }

static struct ac_gen_ctx *island_ctx(struct sr_ctx *c) {
  struct ac_gen_ctx *g = ac_gen_ctx_new(c);
  g->outdoor_w = OUT_W;
  g->outdoor_h = OUT_H;
  g->indoor_w = IN_W;
  g->indoor_h = IN_H;
  g->boss_w = BOSS_W;
  g->boss_h = BOSS_H;
  return g;
}

static struct ac_node *make_random_outdoor_node(struct sr_ctx *c) {
  struct ac_node *n = ac_node_new(c);
  n->x = AC_RND(c, OUT_W);
  n->y = AC_RND(c, OUT_H);
  nset(c, n, "width", AC_RND(c, 10) + 8);
  nset(c, n, "height", AC_RND(c, 10) + 8);
  n->center_x = clampr(n->x + nget(c, n, "width") / 2, OUT_W);
  n->center_y = clampr(n->y + nget(c, n, "height") / 2, OUT_H);
  nset(c, n, "cornersize", AC_RND(c, 3) + 2);
  if (nget(c, n, "cornersize") > nget(c, n, "width") / 2 - 3) nset(c, n, "cornersize", nget(c, n, "width") / 2 - 3);
  if (nget(c, n, "cornersize") > nget(c, n, "height") / 2 - 3) nset(c, n, "cornersize", nget(c, n, "height") / 2 - 3);
  return n;
}

/* The two edge/corner passes that are identical for the outdoor and the boss map. */
static void edge_pass1(struct sr_ctx *c, struct ac_layer *l1, struct ac_layer *l2, int w, int h) {
  struct ac_layer *c1 = ac_layer_new(c, w, h);
  memcpy(c1->d, l1->d, (size_t)w * (size_t)h);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int lx = clampr(x - 1, w), rx = clampr(x + 1, w), uy = clampr(y - 1, h), dy = clampr(y + 1, h);
      if (LAYER(c1, x, y) == 148 && LAYER(c1, lx, y) == 171 && LAYER(c1, x, uy) == 171) {
        LAYER(l1, x, y) = 85;
        LAYER(l2, x, y) = 136;
      } else if (LAYER(c1, x, y) == 148 && LAYER(c1, rx, y) == 171 && LAYER(c1, x, uy) == 171) {
        LAYER(l1, x, y) = 88;
        LAYER(l2, x, y) = 135;
      } else if (LAYER(c1, x, y) == 148 && LAYER(c1, rx, y) == 171 && LAYER(c1, x, dy) == 171) {
        LAYER(l1, x, y) = 100;
        LAYER(l2, x, y) = 122;
      } else if (LAYER(c1, x, y) == 148 && LAYER(c1, lx, y) == 171 && LAYER(c1, x, dy) == 171) {
        LAYER(l1, x, y) = 96;
        LAYER(l2, x, y) = 123;
      } else if (LAYER(c1, x, y) == 148 && LAYER(c1, lx, y) == 171) {
        LAYER(l1, x, y) = 89;
        LAYER(l2, x, y) = 141;
      } else if (LAYER(c1, x, y) == 148 && LAYER(c1, rx, y) == 171) {
        LAYER(l1, x, y) = 90;
        LAYER(l2, x, y) = 140;
      } else if (LAYER(c1, x, y) == 148 && LAYER(c1, x, uy) == 171) {
        LAYER(l1, x, y) = 82;
        LAYER(l2, x, y) = 143;
      } else if (LAYER(c1, x, y) == 148 && LAYER(c1, x, dy) == 171) {
        LAYER(l1, x, uy) = 98;   /* sic: upstream writes the tile above */
        LAYER(l2, x, y) = 142;
      }
    }
  }
}

static void edge_pass2(struct sr_ctx *c, struct ac_layer *l1, struct ac_layer *l2, int w, int h) {
  struct ac_layer *c1 = ac_layer_new(c, w, h);
  memcpy(c1->d, l1->d, (size_t)w * (size_t)h);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      int lx = x == 0 ? w - 1 : x - 1, rx = x == w - 1 ? 0 : x + 1, uy = y == 0 ? h - 1 : y - 1, dy = y == h - 1 ? 0 : y + 1;
      if (LAYER(c1, x, y) == 98) LAYER(l1, x, dy) = 102;
      if (LAYER(c1, rx, y) == 100 && LAYER(c1, x, uy) != 98) {
        if (LAYER(c1, lx, y) != 98) LAYER(l1, lx, y) = 93;
        LAYER(l1, x, y) = 99;
        LAYER(l2, x, y) = 121;
      }
      if (LAYER(c1, lx, y) == 96 && LAYER(c1, x, uy) != 98) {
        if (LAYER(c1, rx, y) != 98) LAYER(l1, rx, y) = 92;
        LAYER(l1, x, y) = 97;
        LAYER(l2, x, y) = 124;
      }
      if (LAYER(c1, x, y) == 85 && LAYER(c1, rx, y) != 82) {
        LAYER(l1, rx, y) = 86;
        LAYER(l2, rx, y) = 137;
      }
      if (LAYER(c1, x, y) == 88 && LAYER(c1, lx, y) != 82) {
        LAYER(l1, lx, y) = 87;
        LAYER(l2, lx, y) = 127;
      }
      if (LAYER(c1, x, y) == 90 && LAYER(c1, x, dy) == 100) {
        LAYER(l1, x, y) = 94;
        LAYER(l2, x, y) = 190;
        LAYER(l1, lx, y) = 93;
      }
      if (LAYER(c1, x, y) == 89 && LAYER(c1, x, dy) == 96) {
        LAYER(l1, x, y) = 91;
        LAYER(l2, x, y) = 189;
        LAYER(l1, rx, y) = 92;
      }
    }
  }
}

static void fill_layers(struct ac_layer *l1, struct ac_layer *l2, int w, int h) {
  for (int y = 0; y < h; y++)
    for (int x = 0; x < w; x++) {
      LAYER(l1, x, y) = 171;
      LAYER(l2, x, y) = 139;
    }
}

static void make_outdoor_enemies(struct sr_ctx *c, struct ac_map *map, const struct ac_node *start) {
  static const uint8_t enemy_ids[] = {0x00, 0x00, 0x00, 0x00, 0x05, 0x0C, 0x11, 0x18, 0x2C, 0x34, 0x39, 0x3D, 0x43};
  for (int i = 0; i < 32; i++) {
    int ex = AC_RND(c, 128), ey = AC_RND(c, 128);
    int dx = abs(ex - start->center_x), dy = abs(ey - start->center_y);
    if (dx > OUT_W / 2) dx = OUT_W - dx;
    if (dy > OUT_H / 2) dy = OUT_H - dy;
    while (dx < 16 && dy < 16) {
      ex = AC_RND(c, 128);
      ey = AC_RND(c, 128);
      dx = abs(ex - start->center_x);
      dy = abs(ey - start->center_y);
      if (dx > OUT_W / 2) dx = OUT_W - dx;
      if (dy > OUT_H / 2) dy = OUT_H - dy;
    }
    int type = enemy_ids[AC_RND(c, (int)sizeof enemy_ids)];
    uint8_t *o = ac_map_add_object(c, map->md);
    ac_obj_set_xpos(o, (uint8_t)ex);
    ac_obj_set_ypos(o, (uint8_t)ey);
    ac_obj_set_species(o, type);
    ac_obj_set_vis_flag(o, 0x00);
    ac_obj_set_vis_min(o, 0x00);
    ac_obj_set_vis_max(o, 0x0F);
    ac_obj_set_event(o, 0x01);
    ac_obj_set_direction(o, 2);
    ac_obj_set_unknown_b7(o, 0x18);
  }
}

static struct ac_map *island_outdoor(struct sr_ctx *c, struct ac_gen_ctx *g) {
  struct ac_layer *l1 = ac_layer_new(c, OUT_W, OUT_H), *l2 = ac_layer_new(c, OUT_W, OUT_H);
  fill_layers(l1, l2, OUT_W, OUT_H);
  struct ac_map *map = ac_map_new(c);
  map->layer1 = l1;
  map->layer2 = l2;

  struct ac_node *start = make_random_outdoor_node(c);
  start->name = "start";
  ac_dict_set(c, &g->special_nodes, AC_STRKEY("start"), AC_VAL(start));
  struct ac_list nodes = {0};
  ac_list_add(c, &nodes, AC_VAL(start));
  map->entry_pos = ac_xy_new(c, start->center_x, start->center_y);

  /* the end node is placed away from the start node (the map loops) */
  struct ac_node *end = make_random_outdoor_node(c);
  end->name = "end";
  ac_dict_set(c, &g->special_nodes, AC_STRKEY("end"), AC_VAL(end));
  end->x = clampr(start->x + OUT_W / 2 + AC_RND(c, 20) - 10, OUT_W);
  end->y = clampr(start->y + OUT_H / 2 + AC_RND(c, 20) - 10, OUT_H);
  end->center_x = clampr(end->x + nget(c, end, "width") / 2, OUT_W);
  end->center_y = clampr(end->y + nget(c, end, "height") / 2, OUT_H);
  ac_list_add(c, &nodes, AC_VAL(end));

  int done = 0, node_num = 0, num_arrows = 0;
  const int overlap_buffer = 1;
  struct ac_layer *overlap = ac_layer_new(c, OUT_W, OUT_H);
  while (!done) {
    int found_pos = 0, iters = 0;
    struct ac_node *this_node = NULL;
    while (!found_pos) {
      found_pos = 1;
      this_node = make_random_outdoor_node(c);
      ac_dict_remove(&this_node->values, AC_STRKEY("arrow"));
      int xdiff = abs(this_node->center_x - end->center_x);
      if (xdiff > OUT_W / 2) xdiff = OUT_W - xdiff;
      int ydiff = abs(this_node->center_y - end->center_y);
      if (ydiff > OUT_H / 2) ydiff = OUT_H - ydiff;

      if (xdiff < 5 && ydiff > 20) {
        if (num_arrows == 0 || AC_RND(c, num_arrows) == 0) {
          int down_arrow = 1;
          int yd = clampr(end->center_y - this_node->center_y, OUT_H);
          if (yd >= OUT_H / 2) down_arrow = 0;
          nset(c, this_node, "width", 7);
          nset(c, this_node, "height", 7);
          this_node->x = this_node->center_x - 3;
          this_node->y = this_node->center_y - 3;
          nset(c, this_node, "arrow", down_arrow ? 0 : 1);
        }
      } else if (ydiff < 5 && xdiff > 20) {
        if (num_arrows == 0 || AC_RND(c, num_arrows) == 0) {
          int right_arrow = 1;
          int xd = clampr(end->center_x - this_node->center_x, OUT_W);
          if (xd >= OUT_W / 2) right_arrow = 0;
          nset(c, this_node, "width", 7);
          nset(c, this_node, "height", 7);
          this_node->x = this_node->center_x - 3;
          this_node->y = this_node->center_y - 3;
          nset(c, this_node, "arrow", right_arrow ? 2 : 3);
        }
      }
      char *nm = (char *)sr_alloc(c, 16);
      snprintf(nm, 16, "n%d", node_num);
      this_node->name = nm;

      memset(overlap->d, 0, (size_t)OUT_W * OUT_H);
      for (int y = this_node->y - overlap_buffer; y < this_node->y + nget(c, this_node, "height") + overlap_buffer; y++) {
        int cy = clampr(y, OUT_W);
        for (int x = this_node->x - overlap_buffer; x < this_node->x + nget(c, this_node, "width") + overlap_buffer; x++) {
          int cx = clampr(x, OUT_W);
          AC_AT(c, overlap, cx, cy) = 1;
        }
      }

      for (int k = 0; k < nodes.n; k++) {
        struct ac_node *n = (struct ac_node *)AC_PTR(nodes.p[k]);
        int nh = nget(c, n, "height"), nw = nget(c, n, "width");
        for (int y = n->y - overlap_buffer; found_pos && y < n->y + nh + overlap_buffer; y++) {
          int cy = clampr(y, OUT_W);
          for (int x = n->x - overlap_buffer; found_pos && x < n->x + nw + overlap_buffer; x++) {
            int cx = clampr(x, OUT_W);
            if (AC_AT(c, overlap, cx, cy)) found_pos = 0;
          }
        }
        if (!found_pos) break;
      }
      iters++;
      if (iters > 1000) {
        done = 1;
        break;
      }
    }

    if (!done) {
      ac_list_add(c, &nodes, AC_VAL(this_node));
      if (nhas(this_node, "arrow")) num_arrows++;
      node_num++;
    }
  }

  /* rounded corners on the islands */
  for (int k = 0; k < nodes.n; k++) {
    struct ac_node *n = (struct ac_node *)AC_PTR(nodes.p[k]);
    if (nhas(n, "arrow")) continue;
    struct ac_list corners = {0};
    for (int i = 0; i < 4; i++)
      if (AC_RND(c, 4) != 0) ac_list_add(c, &corners, i);
    int height = nget(c, n, "height"), width = nget(c, n, "width");
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        int xdist = x > width / 2 ? width - x - 1 : x;
        int ydist = y > height / 2 ? height - y - 1 : y;
        int corner_index = 0;
        if (x > width / 2) corner_index += 1;
        if (y > height / 2) corner_index += 2;
        int map_x = clampr(n->x + x, OUT_W), map_y = clampr(n->y + y, OUT_H);
        int corner_size = nget(c, n, "cornersize");
        if (!ac_list_contains(&corners, corner_index)) corner_size -= 2;
        if (xdist + ydist >= corner_size) {
          AC_AT(c, l1, map_x, map_y) = 148;
          AC_AT(c, l2, map_x, map_y) = 0;
        }
      }
    }
  }

  edge_pass1(c, l1, l2, OUT_W, OUT_H);
  edge_pass2(c, l1, l2, OUT_W, OUT_H);

  /* arrows pointing to the goal */
  static const int arrow_shape[49] = {
      0, 0, 0,  0, 0,  0, 0,   //
      0, 0, 0,  15, 0, 0, 0,   //
      0, 0, 10, 3, 6,  0, 0,   //
      0, 15, 12, 15, 12, 15, 0,   //
      0, 0, 9,  3, 5,  0, 0,   //
      0, 0, 0,  15, 0, 0, 0,   //
      0, 0, 0,  0, 0,  0, 0,   //
  };
  for (int k = 0; k < nodes.n; k++) {
    struct ac_node *n = (struct ac_node *)AC_PTR(nodes.p[k]);
    if (!nhas(n, "arrow")) continue;
    int arrow_dir = nget(c, n, "arrow");
    int bit_mask = 1 << arrow_dir;
    for (int y = 0; y < 7; y++) {
      int ty = clampr(y + n->center_y, OUT_H);
      for (int x = 0; x < 7; x++) {
        int tx = clampr(x + n->center_x, OUT_W);
        if ((arrow_shape[y * 7 + x] & bit_mask) > 0) AC_AT(c, l1, tx, ty) = 144;
      }
    }
  }

  /* up to 8 huts on islands that are large enough */
  static const uint8_t hut_layer1[25] = {0, 119, 120, 121, 0, 134, 135, 136, 137, 138, 118, 1, 2, 3, 122, 0, 17, 18, 19, 0, 0, 33, 34, 35, 0};
  const int max_doors = 8, hut_w = 5, hut_h = 5, hut_ox = -2, hut_oy = -2;
  int num_doors = 0;
  for (int k = 0; k < nodes.n; k++) {
    struct ac_node *n = (struct ac_node *)AC_PTR(nodes.p[k]);
    if (nhas(n, "arrow")) continue;
    if (n->name[0] != 'n') continue;
    if (AC_RND(c, 2) == 0 && num_doors < max_doors && nget(c, n, "height") - nget(c, n, "cornersize") >= 9 &&
        nget(c, n, "width") - nget(c, n, "cornersize") >= 9) {
      for (int y = 0; y < hut_h; y++) {
        for (int x = 0; x < hut_w; x++) {
          int tx = clampr(n->x + x + hut_ox + nget(c, n, "width") / 2, OUT_W);
          int ty = clampr(n->y + y + hut_oy + nget(c, n, "height") / 2, OUT_H);
          AC_AT(c, l1, tx, ty) = hut_layer1[y * hut_w + x];
          if (AC_AT(c, l2, tx, ty) == 0) AC_AT(c, l2, tx, ty) = 77;
          if (AC_AT(c, l1, tx, ty) == 34) {
            ac_dict_set(c, &map->alt_entry, num_doors, AC_VAL(ac_xy_new(c, tx, ty + 2)));
            ac_dict_set(c, &map->alt_exit, AC_XYKEY(tx, ty), num_doors);
          }
        }
      }
      /* little decorative jars to indicate which hut it is */
      int num_thingies = num_doors + 1, placed = 0;
      while (placed < num_thingies) {
        int x = clampr(n->x + AC_RND(c, nget(c, n, "width")), OUT_W);
        int y = clampr(n->y + AC_RND(c, nget(c, n, "height")), OUT_H);
        if (AC_AT(c, l1, x, y) == 148) {
          AC_AT(c, l1, x, y) = 39;
          placed++;
        }
      }
      num_doors++;
    }
  }

  /* flowers or water tiles, more often near a hut, from perlin noise */
  int perlin_z = AC_RND(c, 2000);
  int nhut = 0;
  int16_t *hut_x = (int16_t *)sr_alloc(c, sizeof(int16_t) * 2 * OUT_W * OUT_H);
  for (int y = 1; y < OUT_H - 1; y++)
    for (int x = 1; x < OUT_W - 1; x++)
      if (LAYER(l1, x, y) == 34) {
        hut_x[nhut * 2] = (int16_t)x;
        hut_x[nhut * 2 + 1] = (int16_t)y;
        nhut++;
      }
  for (int y = 1; y < OUT_H - 1; y++) {
    for (int x = 1; x < OUT_W - 1; x++) {
      int nearest = 500;
      for (int i = 0; i < nhut; i++) {
        int xd = abs(hut_x[i * 2] - x);
        if (xd > OUT_W / 2) xd = OUT_W - xd;
        int yd = abs(hut_x[i * 2 + 1] - y);
        if (yd > OUT_H / 2) yd = OUT_H - yd;
        int dist = (int)sqrt((double)(xd * xd + yd * yd));
        if (dist < nearest) nearest = dist;
      }
      double threshold = 0.0;
      threshold += nearest / 50.0;
      if (threshold > .5) threshold = .5;
      if (ac_perlin_noise(x * 20 / (double)OUT_W, y * 20 / (double)OUT_H, perlin_z) > threshold) {
        if (LAYER(l1, x, y) == 171) LAYER(l1, x, y) = 144;
        if (LAYER(l1, x, y) == 148) LAYER(l1, x, y) = 173;
      }
    }
  }

  /* steps so you can walk up on each island */
  for (int k = 0; k < nodes.n; k++) {
    struct ac_node *n = (struct ac_node *)AC_PTR(nodes.p[k]);
    if (nhas(n, "arrow")) continue;
    int w = nget(c, n, "width"), h = nget(c, n, "height");
    int num_steps = AC_RND(c, 3) + 1;
    struct ac_list steps = {0};
    for (int i = 0; i < num_steps; i++) {
      int ok = 0;
      while (!ok) {
        int dir = AC_RND(c, 4);
        if (!ac_list_contains(&steps, dir)) {
          ac_list_add(c, &steps, dir);
          ok = 1;
        }
      }
    }
    for (int s = 0; s < steps.n; s++) {
      switch ((int)steps.p[s]) {
        case 0:
          for (int x = n->x + w / 2 - 1; x <= n->x + w / 2 + 1; x++) AC_AT(c, l1, clampr(x, OUT_W), n->y) = 156;
          break;
        case 1:
          for (int y = n->y + h / 2 - 1; y <= n->y + h / 2 + 1; y++) {
            int my = clampr(y, OUT_H);
            if (y == n->y + h / 2 - 1) AC_AT(c, l1, n->x, my) = 13;
            else if (y == n->y + h / 2 + 1) AC_AT(c, l1, n->x, my) = 45;
            else AC_AT(c, l1, n->x, my) = 29;
          }
          break;
        case 2:
          for (int x = n->x + w / 2 - 1; x <= n->x + w / 2 + 1; x++) {
            int mx = clampr(x, OUT_W);
            int my1 = clampr(n->y + h - 1, OUT_H), my2 = clampr(n->y + h - 2, OUT_H);
            AC_AT(c, l1, mx, my1) = 153;
            if (x == n->x + w / 2 - 1) AC_AT(c, l1, mx, my2) = 92;
            else if (x == n->x + w / 2 + 1) AC_AT(c, l1, mx, my2) = 93;
            else AC_AT(c, l1, mx, my2) = 185;
          }
          break;
        case 3:
          for (int y = n->y + h / 2 - 1; y <= n->y + h / 2 + 1; y++) {
            int my = clampr(y, OUT_H);
            int mx = clampr(n->x + w - 1, OUT_W);
            if (y == n->y + h / 2 - 1) AC_AT(c, l1, mx, my) = 14;
            else if (y == n->y + h / 2 + 1) AC_AT(c, l1, mx, my) = 46;
            else AC_AT(c, l1, mx, my) = 30;
          }
          break;
        default: break;
      }
    }
  }

  /* exit: four fire tiles around a door tile */
  int ex0 = clampr(end->x + nget(c, end, "width") / 2 - 1, OUT_W);
  int ex1 = clampr(end->x + nget(c, end, "width") / 2, OUT_W);
  int ex2 = clampr(end->x + nget(c, end, "width") / 2 + 1, OUT_W);
  int ey0 = clampr(end->y + nget(c, end, "height") / 2 - 1, OUT_H);
  int ey1 = clampr(end->y + nget(c, end, "height") / 2, OUT_H);
  int ey2 = clampr(end->y + nget(c, end, "height") / 2 + 1, OUT_H);
  LAYER(l1, ex0, ey1) = 143;
  LAYER(l1, ex1, ey0) = 143;
  LAYER(l1, ex1, ey2) = 143;
  LAYER(l1, ex2, ey1) = 143;
  LAYER(l2, ex0, ey1) = 77;
  LAYER(l2, ex1, ey0) = 77;
  LAYER(l2, ex1, ey2) = 77;
  LAYER(l2, ex2, ey1) = 77;
  ac_list_add(c, &map->exit_locations, AC_VAL(ac_xy_new(c, ex1, ey1)));
  LAYER(l1, ex1, ey1) = 79;
  LAYER(l2, ex1, ey1) = 102;

  uint8_t *hd = map->md->header;
  ac_hdr_set_walkon(hd, 1);
  ac_hdr_set_tileset16(hd, 5);
  ac_hdr_set_tileset8(hd, 5);
  ac_hdr_set_animated_tiles(hd, 1);
  ac_hdr_set_display(hd, 1);
  ac_hdr_set_layer1_fg(hd, 0);
  ac_hdr_set_layer2_fg(hd, 1);
  ac_hdr_set_npc_palette(hd, 0xFF);
  ac_hdr_set_flammie(hd, 1);
  ac_hdr_set_magic_rope(hd, 1);

  ac_apply_palette_source(c, map, &outside_and_boss_palettes[AC_RND(c, 4)]);
  ac_hdr_set_palette_animated(hd, 1);

  make_outdoor_enemies(c, map, start);

  g->num_shared_doors = num_doors;
  g->has_num_shared_doors = 1;
  for (int k = 0; k < nodes.n; k++) ac_list_add(c, &g->shared_nodes, nodes.p[k]);
  return map;
}

/* IslandHutRooms.populateIndoorMapData */
static void hut_rooms(struct sr_ctx *c, struct ac_map *inside, const struct ac_gen_ctx *g, struct ac_layer *l1, struct ac_layer *l2) {
  const int buffer = 6;
  int map_w = g->indoor_w, map_h = g->indoor_h;
  if (!g->has_num_shared_doors) sr_fail(c, SOMRANDO_ERR_FAILED, "Value cannot be null (SharedDoors).");
  int num_doors = g->num_shared_doors;

  for (int y = 0; y < map_h; y++)
    for (int x = 0; x < map_w; x++) {
      AC_AT(c, l1, x, y) = 0;
      AC_AT(c, l2, x, y) = 6;
    }

  for (int i = 0; i < num_doors; i++) {
    int xo = 32 * (i % 4), yo = 32 * (i / 4);
    int door_x = (buffer * 2 + (31 - buffer * 2)) / 2;
    for (int y = buffer; y < 32 - buffer; y++) {
      for (int x = buffer; x < 32 - buffer; x++) {
        if (x == buffer) {
          if (y > buffer * 2 && y < 31 - buffer * 2) {
            if (y == buffer * 2 + 1) AC_AT(c, l2, x + xo, y + yo - 1) = 1;
            AC_AT(c, l2, x + xo, y + yo) = 48;
            AC_AT(c, l2, x + xo, y + yo + 1) = 80;
            AC_AT(c, l1, x + xo, y + yo) = 0;
            AC_AT(c, l1, x + xo, y + yo + 1) = 1;
          }
        } else if (x > buffer && x <= buffer * 2) {
          if (x + y == buffer * 3) {
            AC_AT(c, l2, x + xo, y + yo) = 1;
            AC_AT(c, l2, x + xo, y + yo + 1) = 17;
            AC_AT(c, l2, x + xo, y + yo + 2) = 33;
            AC_AT(c, l2, x + xo, y + yo + 3) = 49;
          }
          if (x + (30 - y) == buffer * 3) {
            AC_AT(c, l2, x + xo, y + yo + 1) = 128;
            AC_AT(c, l1, x + xo, y + yo) = 2;
            AC_AT(c, l1, x + xo, y + yo + 1) = 1;
            AC_AT(c, l1, x + xo, y + yo + 2) = 37;
            AC_AT(c, l1, x + xo, y + yo + 3) = 34;
            AC_AT(c, l1, x + xo, y + yo + 4) = 50;
          }
        } else if (x > buffer * 2 && x < 31 - buffer * 2) {
          if (y == buffer) {
            AC_AT(c, l2, x + xo, y + yo) = 3;
            AC_AT(c, l2, x + xo, y + yo + 1) = 19;
            AC_AT(c, l2, x + xo, y + yo + 2) = 35;
          }
          if (y == 31 - buffer) {
            AC_AT(c, l2, x + xo, y + yo) = 130;
            AC_AT(c, l2, x + xo, y + yo + 1) = 130;
            AC_AT(c, l2, x + xo, y + yo + 2) = 176;
            if (x == door_x) {
              ac_dict_set(c, &inside->alt_entry, i, AC_VAL(ac_xy_new(c, x + xo, y + yo)));
              ac_list_add(c, &inside->return_doors, i);
            }
            AC_AT(c, l1, x + xo, y + yo) = 19;
            AC_AT(c, l1, x + xo, y + yo + 1) = 35;
            AC_AT(c, l1, x + xo, y + yo + 2) = 35;
            AC_AT(c, l1, x + xo, y + yo + 3) = 51;
          }
        } else if (x >= 31 - buffer * 2 && x < 31 - buffer) {
          if ((31 - x) + y == buffer * 3) {
            AC_AT(c, l2, x + xo, y + yo) = 5;
            AC_AT(c, l2, x + xo, y + yo + 1) = 21;
            AC_AT(c, l2, x + xo, y + yo + 2) = 37;
            AC_AT(c, l2, x + xo, y + yo + 3) = 53;
          }
          if ((31 - x) + (30 - y) == buffer * 3) {
            AC_AT(c, l2, x + xo, y + yo + 1) = 183;
            AC_AT(c, l1, x + xo, y + yo) = 6;
            AC_AT(c, l1, x + xo, y + yo + 1) = 22;
            AC_AT(c, l1, x + xo, y + yo + 2) = 37;
            AC_AT(c, l1, x + xo, y + yo + 3) = 38;
            AC_AT(c, l1, x + xo, y + yo + 4) = 54;
          }
        } else if (x == 31 - buffer) {
          if (y > buffer * 2 && y < 31 - buffer * 2) {
            if (y == buffer * 2 + 1) AC_AT(c, l2, x + xo, y + yo - 1) = 5;
            AC_AT(c, l2, x + xo, y + yo) = 54;
            AC_AT(c, l2, x + xo, y + yo + 1) = 80;
            AC_AT(c, l1, x + xo, y + yo) = 0;
            AC_AT(c, l1, x + xo, y + yo + 1) = 7;
          }
        }
      }
    }

    for (int y = buffer + 3; y < 32 - buffer - 1; y++) {
      int inside_room = 0, prev_wall = 0;
      for (int x = buffer; x < 32 - buffer; x++) {
        if (AC_AT(c, l2, x + xo, y + yo) != 6) {
          if (!prev_wall) inside_room = !inside_room;
          prev_wall = 1;
        } else {
          prev_wall = 0;
        }
        if (inside_room && AC_AT(c, l2, x + xo, y + yo) == 6) AC_AT(c, l2, x + xo, y + yo) = 184;
      }
    }
  }
}

static struct ac_map *island_indoor(struct sr_ctx *c, struct ac_gen_ctx *g) {
  struct ac_map *inside = ac_map_new(c);
  struct ac_layer *l1 = ac_layer_new(c, IN_W, IN_H), *l2 = ac_layer_new(c, IN_W, IN_H);
  hut_rooms(c, inside, g, l1, l2);

  int entry_x[8], entry_y[8];
  for (int i = 0; i < 8; i++) {
    entry_x[i] = 15 + (i % 4) * 32;
    entry_y[i] = 25 + (i / 4) * 32;
  }
  if (!g->has_num_shared_doors) sr_fail(c, SOMRANDO_ERR_FAILED, "Value cannot be null (SharedDoors).");
  int num_doors = g->num_shared_doors;
  int num_dialogue = w8_wget_int(c, "numDialogueEvents");

  for (int i = 0; i < num_doors; i++) {
    for (int n = 0; n < 3; n++) {
      int npc_x = entry_x[i];
      int npc_y = entry_y[i] - 3 - n;
      int event_num = 0x280 + AC_RND(c, num_dialogue);
      int species = npc_ids[AC_RND(c, (int)sizeof npc_ids)];
      uint8_t *o = ac_map_add_object(c, inside->md);
      ac_obj_set_species(o, species);
      ac_obj_set_vis_flag(o, 0x00);
      ac_obj_set_vis_min(o, 0x00);
      ac_obj_set_vis_max(o, 0x0F);
      ac_obj_set_xpos(o, (uint8_t)npc_x);
      ac_obj_set_ypos(o, (uint8_t)npc_y);
      ac_obj_set_event(o, (uint16_t)event_num);
      ac_obj_set_unknown_b7(o, 0x08);
      ac_obj_set_direction(o, 2);
      ac_obj_set_layer2_collision(o, 1);
    }
  }

  inside->layer1 = l1;
  inside->layer2 = l2;
  uint8_t *hd = inside->md->header;
  ac_hdr_set_tileset16(hd, 6);
  ac_hdr_set_tileset8(hd, 6);
  ac_hdr_set_display(hd, 0);
  ac_hdr_set_layer1_fg(hd, 0);
  ac_hdr_set_layer2_fg(hd, 1);
  ac_hdr_set_npc_palette(hd, 0xFF);
  ac_hdr_set_flammie(hd, 1);
  ac_hdr_set_magic_rope(hd, 1);
  ac_apply_palette_source(c, inside, &inside_palettes[AC_RND(c, 1)]);
  inside->layer2_collision = 1;
  return inside;
}

static struct ac_map *island_boss(struct sr_ctx *c, struct ac_gen_ctx *g) {
  (void)g;
  struct ac_layer *l1 = ac_layer_new(c, BOSS_W, BOSS_H), *l2 = ac_layer_new(c, BOSS_W, BOSS_H);
  fill_layers(l1, l2, BOSS_W, BOSS_H);

  /* 4 water on each side, corner size 4 */
  int nw = BOSS_W - 4 * 2, nh = BOSS_H - 4 * 2;
  for (int y = 0; y < nh; y++) {
    for (int x = 0; x < nw; x++) {
      int xdist = x > nw / 2 ? nw - x - 1 : x;
      int ydist = y > nh / 2 ? nh - y - 1 : y;
      int map_x = 4 + x, map_y = 4 + y;
      if (map_x >= BOSS_W) map_x -= BOSS_W;
      if (map_y >= BOSS_H) map_y -= BOSS_H;
      if (xdist + ydist >= 4) {
        LAYER(l1, map_x, map_y) = 148;
        LAYER(l2, map_x, map_y) = 0;
      }
    }
  }
  edge_pass1(c, l1, l2, BOSS_W, BOSS_H);
  edge_pass2(c, l1, l2, BOSS_W, BOSS_H);

  const int boss_x = 16, boss_y = 14, player_x = 16, player_y = 20;
  int perlin_z = AC_RND(c, 2000);
  for (int y = 1; y < BOSS_H - 1; y++) {
    for (int x = 1; x < BOSS_W - 1; x++) {
      double noise = ac_perlin_noise(x * 5 / (double)BOSS_W + 100, y * 5 / (double)BOSS_H + 100, perlin_z);
      if (noise > -0.2) {
        if (noise > 0.1 && LAYER(l1, x, y) == 171) LAYER(l1, x, y) = 144;
        if (LAYER(l1, x, y) == 148) {
          if (noise > .5) {
            if ((abs(x - boss_x) > 5 || abs(y - boss_y) > 5) && (abs(x - player_x) > 5 || abs(y - player_y) > 5)) LAYER(l1, x, y) = 127;
            else LAYER(l1, x, y) = 173;
          } else if (noise > .3) {
            if (AC_RND(c, 2) == 0) LAYER(l1, x, y) = 173;
            else LAYER(l1, x, y) = 174;
          } else if (noise > .1) {
            if (AC_RND(c, 4) == 0) {
              if (AC_RND(c, 2) == 0) LAYER(l1, x, y) = 173;
              else LAYER(l1, x, y) = 174;
            }
          } else {
            if (AC_RND(c, 16) == 0) {
              if (AC_RND(c, 2) == 0) LAYER(l1, x, y) = 173;
              else LAYER(l1, x, y) = 174;
            }
          }
        }
      }
      if (noise < -0.4 && LAYER(l1, x, y) == 148 && LAYER(l1, x + 1, y) == 148 && AC_RND(c, 4) == 0) LAYER(l1, x, y) = 60;
    }
  }

  struct ac_map *boss = ac_map_new(c);
  boss->layer1 = l1;
  boss->layer2 = l2;
  boss->entry_pos = ac_xy_new(c, player_x, player_y);

  static const uint8_t boss_types[18] = {87, 90, 92, 94, 95, 98, 99, 101, 102, 105, 109, 111, 112, 113, 116, 120, 123, 125};
  int boss_id = boss_types[AC_RND(c, 18)];
  uint8_t *o = ac_map_add_object(c, boss->md);
  ac_obj_set_species(o, boss_id);
  ac_obj_set_xpos(o, boss_x);
  ac_obj_set_ypos(o, boss_y);
  ac_obj_set_vis_flag(o, 0x00);
  ac_obj_set_vis_min(o, 0x00);
  ac_obj_set_vis_max(o, 0x0F);

  uint8_t *hd = boss->md->header;
  ac_hdr_set_walkon(hd, 0);
  ac_hdr_set_tileset16(hd, 5);
  ac_hdr_set_tileset8(hd, 5);
  ac_hdr_set_display(hd, 0x41);
  ac_hdr_set_layer1_fg(hd, 0);
  ac_hdr_set_layer2_fg(hd, 1);
  ac_hdr_set_npc_palette(hd, 0xFF);
  ac_hdr_set_flammie(hd, 1);
  ac_hdr_set_magic_rope(hd, 1);
  ac_apply_palette_source(c, boss, &outside_and_boss_palettes[AC_RND(c, 4)]);
  return boss;
}

static const uint8_t *island_song(struct sr_ctx *c, int *len) {
  static const uint8_t song[5] = {0x40, 0x01, 0x05, 0x10, 0x8F};   /* gaia's navel overworld music */
  (void)c;
  *len = 5;
  return song;
}

const struct ac_biome sr_p3_island_biome = {"island", "Island", island_ctx, island_outdoor, island_indoor, island_boss, island_song};
