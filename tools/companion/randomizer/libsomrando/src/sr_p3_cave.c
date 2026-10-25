/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Ancient Cave floor generator "cave".  Derived from processing/ancientcave/mapgen/cave/CaveGenerator.cs and
 * processing/ancientcave/mapgen/forest/ForestCaveRooms.cs (Author of both: Moppleton).  Translated to C 2026-10-18.
 */
#include <math.h>
#include <string.h>

#include "sr_p3_cave.h"

#define OUT_W 128
#define OUT_H 128
#define IN_W 128
#define IN_H 64
#define BOSS_W 32
#define BOSS_H 32

#define NUM_SOLID_NODES 10
#define NUM_NORMAL_NODES 30
#define SOLID_NODE_SIZE 15
#define SOLID_NODE_SIZE_RANDOMNESS 6
#define NORMAL_NODE_SIZE 4
#define NORMAL_NODE_SIZE_RANDOMNESS 4
#define PATH_SIZE 3
#define PATH_SIZE_RANDOMNESS 4

#define DIR_LEFT 2
#define DIALOGUE_EVENT_START 0x280
#define PI 3.14159265358979323846

static const struct ac_palsrc palettes[4] = {
  {NULL, 69},
  {"accave.undinecave.bin", 69},
  {"accave.fierycave.bin", 69},
  {"accave.graycave.bin", 69},
};

static const uint8_t npc_ids[] = {0x99, 0x9C, 0x9E, 0x9F, 0xA0, 0xA1, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAC, 0xAD, 0xAE, 0xAF, 0xB1,
                                  0xB6, 0xB7, 0xBB, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB};

static int d2i(double v) {   /* (int) of a double: saturating, NaN is 0 (.NET 9+) */
  if (v != v) return 0;
  if (v >= 2147483648.0) return 2147483647;
  if (v <= -2147483648.0) return (-2147483647 - 1);
  return (int)v;
}

static struct ac_gen_ctx *get_ctx(struct sr_ctx *c) {
  struct ac_gen_ctx *g = ac_gen_ctx_new(c);
  g->outdoor_w = OUT_W;
  g->outdoor_h = OUT_H;
  g->indoor_w = IN_W;
  g->indoor_h = IN_H;
  g->boss_w = BOSS_W;
  g->boss_h = BOSS_H;
  return g;
}

static int node_radius(const struct ac_node *n) {
  int64_t v = 0;
  if (!ac_dict_try_get(&n->values, AC_STRKEY("radius"), &v)) return 0;
  return (int)v;
}

static struct ac_node *make_random_outdoor_node(struct sr_ctx *c, int solid) {
  struct ac_node *n = ac_node_new(c);
  if (solid) ac_dict_set(c, &n->values, AC_STRKEY("solid"), 1);
  int radius = NORMAL_NODE_SIZE + AC_RND(c, NORMAL_NODE_SIZE_RANDOMNESS);
  if (solid) radius = SOLID_NODE_SIZE + AC_RND(c, SOLID_NODE_SIZE_RANDOMNESS);
  int x_range = OUT_W - (20 + radius) * 2;
  int y_range = OUT_H - 25 - 5 - (radius * 2);
  int y_offset = 25;
  if (solid) {
    y_range -= 20;
    y_offset += 20;
  }
  n->x = 20 + radius + AC_RND(c, x_range);
  n->y = y_offset + radius + AC_RND(c, y_range);
  n->center_x = n->x;
  n->center_y = n->y;
  ac_dict_add(c, &n->values, AC_STRKEY("radius"), radius);
  return n;
}

static void clear_node_area(struct sr_ctx *c, const struct ac_node *node, struct ac_layer *layer1, struct ac_layer *layer2, int radial_noise) {
  double radius;
  double initial_radius = node_radius(node);
  int noise_seed1 = AC_RND(c, 5000);
  for (int d = 0; d < 360; d++) {
    double x_pos = cos(d * PI / 180.0);
    double y_pos = sin(d * PI / 180.0);
    radius = initial_radius + ac_perlin_noise(noise_seed1, x_pos * 10, y_pos * 10) * radial_noise;
    for (double r = 0; r < radius; r += 0.1) {
      int x_tile = d2i(node->x + x_pos * r);
      int y_tile = d2i(node->y + y_pos * r);
      if (x_tile >= 0 && x_tile < OUT_W && y_tile >= 0 && y_tile < OUT_H) {   /* sic: the outdoor size, also for the boss map */
        AC_AT(c, layer1, x_tile, y_tile) = 70;
        AC_AT(c, layer2, x_tile, y_tile) = 0;
      }
    }
  }
}

static void new_enemy(struct sr_ctx *c, struct ac_map *m, int rx, int ry, int type) {
  uint8_t *e = ac_map_add_object(c, m->md);
  ac_obj_set_xpos(e, (uint8_t)rx);
  ac_obj_set_ypos(e, (uint8_t)ry);
  ac_obj_set_species(e, type);
  ac_obj_set_vis_flag(e, 0x00);
  ac_obj_set_vis_min(e, 0x00);
  ac_obj_set_vis_max(e, 0x0F);
  ac_obj_set_event(e, 0x01);
  ac_obj_set_direction(e, DIR_LEFT);
  ac_obj_set_unknown_b7(e, 0x18);
}

static void make_outdoor_enemies(struct sr_ctx *c, struct ac_map *m, const struct ac_node *start, const struct ac_layer *layer1) {
  static const uint8_t water_ids[] = {0x00, 0x00, 0x00, 0x00, 0x05, 0x0C, 0x11, 0x18};
  static const uint8_t land_ids[] = {0x00, 0x00, 0x00, 0x00, 0x06, 0x07, 0x08, 0x0A, 0x0B, 0x0E};
  int num_water = 0, num_land = 0;
  while (num_water < 16 || num_land < 16) {
    int rx = AC_RND(c, OUT_W);
    int ry = AC_RND(c, OUT_H);
    int xdiff = rx - start->x;
    int ydiff = ry - start->y;
    if (xdiff < 0) xdiff = -xdiff;
    if (ydiff < 0) ydiff = -ydiff;
    if (xdiff > 10 && ydiff > 10) {
      if (LAYER(layer1, rx, ry) == 43 && num_water < 16) {
        int type = water_ids[AC_RND(c, (int)sizeof water_ids)];
        new_enemy(c, m, rx, ry, type);
        num_water++;
      }
      if (LAYER(layer1, rx, ry) == 70 && num_land < 16) {
        int type = land_ids[AC_RND(c, (int)sizeof land_ids)];
        new_enemy(c, m, rx, ry, type);
        num_land++;
      }
    }
  }
}

static void set_cave_header(struct ac_map *m) {
  uint8_t *h = m->md->header;
  ac_hdr_set_tileset16(h, 10);
  ac_hdr_set_tileset8(h, 10);
  ac_hdr_set_display(h, 1);
  ac_hdr_set_layer1_fg(h, 0);
  ac_hdr_set_layer2_fg(h, 1);
  ac_hdr_set_npc_palette(h, 0xFF);
  ac_hdr_set_flammie(h, 1);
  ac_hdr_set_magic_rope(h, 1);
}

/* the waterfall ledge along the top of the outdoor map */
static void draw_top_border(struct sr_ctx *c, struct ac_layer *layer1, struct ac_layer *layer2, int *left_wall_start_y, int *right_wall_start_y) {
  int ledge_y = 15;
  int wall_direction = 0;
  for (int x = 0; x < OUT_W; x++) {
    if (x == 10) *left_wall_start_y = ledge_y + 1;
    if (x == OUT_W - 10) *right_wall_start_y = ledge_y + 1;

    int waterfall = AC_RND(c, 4) > 0;
    int next = AC_RND(c, 4);
    if (wall_direction == -1) {
      LAYER(layer1, x, ledge_y + 1) = 43;
      LAYER(layer1, x, ledge_y) = 19;
      LAYER(layer1, x, ledge_y - 1) = 16;
      LAYER(layer1, x, ledge_y - 2) = 13;
      LAYER(layer1, x, ledge_y - 3) = 10;
      LAYER(layer2, x, ledge_y) = 22;
      LAYER(layer2, x, ledge_y - 1) = (uint8_t)(waterfall ? 12 : 33);
      LAYER(layer2, x, ledge_y - 2) = (uint8_t)(waterfall ? 11 : 0);
      LAYER(layer2, x, ledge_y - 3) = (uint8_t)(waterfall ? 11 : 0);
      uint8_t val = 13;
      for (int y = ledge_y - 4; y >= 0; y--) {
        LAYER(layer1, x, y) = val;
        val = val == 13 ? 10 : 13;
        LAYER(layer2, x, y) = (uint8_t)(waterfall ? 11 : 0);
      }
    }
    if (wall_direction == 0) {
      LAYER(layer1, x, ledge_y + 1) = 43;
      LAYER(layer1, x, ledge_y) = 20;
      LAYER(layer1, x, ledge_y - 1) = 17;
      LAYER(layer1, x, ledge_y - 2) = 14;
      LAYER(layer1, x, ledge_y - 3) = 11;
      LAYER(layer2, x, ledge_y) = (uint8_t)(waterfall ? 43 : 50);
      LAYER(layer2, x, ledge_y - 1) = (uint8_t)(waterfall ? 27 : 0);
      LAYER(layer2, x, ledge_y - 2) = (uint8_t)(waterfall ? 11 : 0);
      LAYER(layer2, x, ledge_y - 3) = (uint8_t)(waterfall ? 11 : 0);
      uint8_t val = 14;
      for (int y = ledge_y - 4; y >= 0; y--) {
        LAYER(layer1, x, y) = val;
        val = val == 14 ? 11 : 14;
        LAYER(layer2, x, y) = (uint8_t)(waterfall ? 11 : 0);
      }
    }
    if (wall_direction == 1) {
      LAYER(layer1, x, ledge_y + 2) = 43;
      LAYER(layer1, x, ledge_y + 1) = 21;
      LAYER(layer1, x, ledge_y) = 18;
      LAYER(layer1, x, ledge_y - 1) = 15;
      LAYER(layer1, x, ledge_y - 2) = 12;
      LAYER(layer2, x, ledge_y + 1) = 22;
      LAYER(layer2, x, ledge_y) = (uint8_t)(waterfall ? 13 : 35);
      LAYER(layer2, x, ledge_y - 1) = (uint8_t)(waterfall ? 11 : 0);
      LAYER(layer2, x, ledge_y - 2) = (uint8_t)(waterfall ? 11 : 0);
      uint8_t val = 15;
      for (int y = ledge_y - 3; y >= 0; y--) {
        LAYER(layer1, x, y) = val;
        val = val == 15 ? 12 : 15;
        LAYER(layer2, x, y) = (uint8_t)(waterfall ? 11 : 0);
      }
    }

    wall_direction = 0;
    if (next == 0 && ledge_y > 10) {
      ledge_y--;
      wall_direction = 1;
    }
    if (next == 1 && ledge_y < 20) {
      ledge_y++;
      wall_direction = -1;
    }
  }
}

static void fill_left(struct ac_layer *layer1, struct ac_layer *layer2, int wall_x, int y) {
  for (int x = 0; x <= wall_x; x++) {
    LAYER(layer1, x, y) = 5;
    LAYER(layer2, x, y) = 0;
  }
}

static void draw_left_wall(struct sr_ctx *c, struct ac_layer *layer1, struct ac_layer *layer2, int left_wall_start_y) {
  int wall_x = 10;
  int wall_direction = 0;
  for (int x = 0; x < wall_x; x++) {
    AC_AT(c, layer1, x, left_wall_start_y) = 2;
    AC_AT(c, layer2, x, left_wall_start_y) = 0;
  }
  AC_AT(c, layer1, wall_x, left_wall_start_y) = 73;
  AC_AT(c, layer2, wall_x, left_wall_start_y) = 3;

  for (int y = left_wall_start_y + 1; y < OUT_H; y++) {
    int next = AC_RND(c, 4);
    fill_left(layer1, layer2, wall_x, y);
    if (wall_direction == -1) {
      LAYER(layer1, wall_x, y) = 73;
      LAYER(layer2, wall_x, y) = 3;
    } else if (wall_direction == 0) {
      LAYER(layer1, wall_x, y) = 6;
      LAYER(layer2, wall_x, y) = 0;
    } else if (wall_direction == 1) {
      LAYER(layer1, wall_x + 1, y) = 9;
      LAYER(layer1, wall_x, y) = 78;
      LAYER(layer2, wall_x + 1, y) = 0;
      LAYER(layer2, wall_x, y) = 0;
      if (y < OUT_H - 5) {
        static const uint8_t t1[4] = {12, 15, 18, 21};
        static const uint8_t t2[4] = {0, 0, 35, 22};
        for (int k = 0; k < 4; k++) {
          y++;
          fill_left(layer1, layer2, wall_x, y);
          LAYER(layer1, wall_x + 1, y) = t1[k];
          LAYER(layer1, wall_x, y) = 6;
          LAYER(layer2, wall_x + 1, y) = t2[k];
          LAYER(layer2, wall_x, y) = 0;
        }
      }
    }
    wall_direction = 0;
    if (next == 0 && wall_x > 5) {
      wall_x--;
      wall_direction = 1;
    }
    if (next == 1 && wall_x < 15) {
      wall_x++;
      wall_direction = -1;
    }
  }
}

static void fill_right(struct ac_layer *layer1, struct ac_layer *layer2, int wall_x, int y) {
  for (int x = wall_x; x < OUT_W; x++) {
    LAYER(layer1, x, y) = 5;
    LAYER(layer2, x, y) = 0;
  }
}

static void draw_right_wall(struct sr_ctx *c, struct ac_layer *layer1, struct ac_layer *layer2, int right_wall_start_y) {
  int wall_x = OUT_W - 10;
  int wall_direction = 0;
  for (int x = wall_x + 1; x < OUT_W; x++) {
    AC_AT(c, layer1, x, right_wall_start_y) = 2;
    AC_AT(c, layer2, x, right_wall_start_y) = 0;
  }
  AC_AT(c, layer1, wall_x, right_wall_start_y) = 71;
  AC_AT(c, layer2, wall_x, right_wall_start_y) = 1;

  for (int y = right_wall_start_y + 1; y < OUT_H; y++) {
    int next = AC_RND(c, 4);
    fill_right(layer1, layer2, wall_x, y);
    if (wall_direction == -1) {
      LAYER(layer1, wall_x, y) = 71;
      LAYER(layer2, wall_x, y) = 1;
    } else if (wall_direction == 0) {
      LAYER(layer1, wall_x, y) = 4;
      LAYER(layer2, wall_x, y) = 0;
    } else if (wall_direction == 1) {
      LAYER(layer1, wall_x - 1, y) = 7;
      LAYER(layer1, wall_x, y) = 79;
      LAYER(layer2, wall_x - 1, y) = 0;
      LAYER(layer2, wall_x, y) = 0;
      if (y < OUT_H - 5) {
        static const uint8_t t1[4] = {10, 13, 16, 19};
        static const uint8_t t2[4] = {0, 0, 33, 22};
        for (int k = 0; k < 4; k++) {
          y++;
          fill_right(layer1, layer2, wall_x, y);
          LAYER(layer1, wall_x - 1, y) = t1[k];
          LAYER(layer1, wall_x, y) = 4;
          LAYER(layer2, wall_x - 1, y) = t2[k];
          LAYER(layer2, wall_x, y) = 0;
        }
      }
    }
    wall_direction = 0;
    if (next == 0 && wall_x > OUT_W - 15) {
      wall_x--;
      wall_direction = -1;
    }
    if (next == 1 && wall_x < OUT_W - 5) {
      wall_x++;
      wall_direction = 1;
    }
  }
}

static struct ac_map *generate_outdoor(struct sr_ctx *c, struct ac_gen_ctx *g) {
  struct ac_layer *layer1 = ac_layer_new(c, OUT_W, OUT_H);
  struct ac_layer *layer2 = ac_layer_new(c, OUT_W, OUT_H);
  struct ac_map *out = ac_map_new(c);
  out->layer1 = layer1;
  out->layer2 = layer2;

  for (int y = 0; y < OUT_H; y++)
    for (int x = 0; x < OUT_W; x++) {
      LAYER(layer1, x, y) = 43;
      LAYER(layer2, x, y) = 22;
    }

  for (int i = 0; i < NUM_SOLID_NODES; i++) {
    struct ac_node *wall = make_random_outdoor_node(c, 1);
    double radius = node_radius(wall);
    double initial_radius = radius;
    for (int d = 0; d < 360; d++) {
      double x_pos = cos(d * PI / 180.0);
      double y_pos = sin(d * PI / 180.0);
      for (double r = 0; r < radius; r += 0.1) {
        int x_tile = d2i(wall->x + x_pos * r);
        int y_tile = d2i(wall->y + y_pos * r);
        if (x_tile >= 0 && x_tile < OUT_W && y_tile >= 0 && y_tile < OUT_H) {
          LAYER(layer1, x_tile, y_tile) = 5;
          LAYER(layer2, x_tile, y_tile) = 0;
        }
      }
      double radius_change = ((AC_RND(c, 10)) - 10) / 20.0;
      double new_radius = radius + radius_change;
      if (new_radius >= initial_radius - 3 && new_radius <= initial_radius + 3) radius = new_radius;
    }
  }

  struct ac_list all_nodes = {0};
  struct ac_node *start_node = make_random_outdoor_node(c, 0);
  start_node->name = "start";
  ac_dict_set(c, &g->special_nodes, AC_STRKEY("start"), AC_VAL(start_node));
  out->entry_pos = ac_xy_new(c, start_node->x, start_node->y);
  ac_list_add(c, &all_nodes, AC_VAL(start_node));
  for (int i = 0; i < NUM_NORMAL_NODES; i++) ac_list_add(c, &all_nodes, AC_VAL(make_random_outdoor_node(c, 0)));

  for (int i = 0; i < all_nodes.n; i++) clear_node_area(c, (struct ac_node *)AC_PTR(all_nodes.p[i]), layer1, layer2, 3);

  struct ac_list visited = {0}, unvisited = {0};
  for (int i = 0; i < all_nodes.n; i++) ac_list_add(c, &unvisited, all_nodes.p[i]);
  ac_list_add(c, &visited, AC_VAL(start_node));
  ac_list_remove(&unvisited, AC_VAL(start_node));
  while (unvisited.n > 0) {
    double min_dist = 999;
    struct ac_node *src = NULL, *dst = NULL;
    for (int i = 0; i < visited.n; i++) {
      struct ac_node *sn = (struct ac_node *)AC_PTR(visited.p[i]);
      for (int j = 0; j < unvisited.n; j++) {
        struct ac_node *dn = (struct ac_node *)AC_PTR(unvisited.p[j]);
        int xdiff = sn->x - dn->x;
        int ydiff = sn->y - dn->y;
        double diff = sqrt((double)(xdiff * xdiff + ydiff * ydiff));
        if (diff < min_dist) {
          min_dist = diff;
          src = sn;
          dst = dn;
        }
      }
    }
    int path_radius = (AC_RND(c, PATH_SIZE_RANDOMNESS)) + PATH_SIZE;
    if (!src) sr_fail(c, SOMRANDO_ERR_FAILED, "Object reference not set to an instance of an object.");
    int xd = dst->x - src->x;
    int yd = dst->y - src->y;
    double angle = atan2((double)yd, (double)xd);
    double dx = cos(angle);
    double dy = sin(angle);
    double x_pos = src->x;
    double y_pos = src->y;
    int iters;
    if (xd == 0) iters = d2i(fabs((src->y - dst->y) / dy));
    else iters = d2i(fabs((src->x - dst->x) / dx));
    for (int i = 0; i <= iters; i++) {
      for (int y = 0; y < OUT_H; y++)
        for (int x = 0; x < OUT_W; x++) {
          double xdiff = x - x_pos;
          double ydiff = y - y_pos;
          double diff = sqrt(xdiff * xdiff + ydiff * ydiff);
          if (diff < path_radius) {
            LAYER(layer1, x, y) = 70;
            LAYER(layer2, x, y) = 0;
          }
        }
      x_pos += dx;
      y_pos += dy;
    }
    ac_list_add(c, &visited, AC_VAL(dst));
    ac_list_remove(&unvisited, AC_VAL(dst));
    ac_list_add(c, &src->connections, AC_VAL(dst));
    ac_list_add(c, &dst->connections, AC_VAL(src));
  }

  for (int x = 15; x < OUT_W - 15; x++) {
    int y = OUT_H - 2;
    while (y > 10) {
      int num = 0;
      int start_y = y + 1;
      while (y > 10 && LAYER(layer1, x, y) == 5) {
        num++;
        y--;
      }
      if (num > 0 && num < 7) {
        for (int yy = 0; yy < num; yy++) {
          AC_AT(c, layer1, x, yy + y) = AC_AT(c, layer1, x, start_y);
          AC_AT(c, layer2, x, yy + y) = AC_AT(c, layer2, x, start_y);
        }
      }
      if (num == 0) y--;
    }
  }

  sr_p3_cave_add_tile_edges(c, layer1, layer2, start_node);

  int left_wall_start_y = 0, right_wall_start_y = 0;
  draw_top_border(c, layer1, layer2, &left_wall_start_y, &right_wall_start_y);
  draw_left_wall(c, layer1, layer2, left_wall_start_y);
  draw_right_wall(c, layer1, layer2, right_wall_start_y);

  for (int x = 0; x < OUT_W - 1; x++)
    if (LAYER(layer1, x, OUT_H - 1) == 43) LAYER(layer1, x, OUT_H - 1) = 68;

  /* -1 unreachable, 0 not checked, 1 reachable */
  int *water = (int *)sr_alloc(c, sizeof(int) * OUT_W * OUT_H);
  int *qx = (int *)sr_alloc(c, sizeof(int) * (OUT_W * OUT_H + 2));
  int *qy = (int *)sr_alloc(c, sizeof(int) * (OUT_W * OUT_H + 2));
#define WATER(x, y) water[(y) * OUT_W + (x)]
  int qhead = 0, qtail = 0;
  qx[qtail] = OUT_W / 2;
  int yc = 0;
  while (yc < 40) {
    if (LAYER(layer1, OUT_W / 2, yc) == 43) {
      WATER(OUT_W / 2, yc) = 1;
      break;
    }
    yc++;
  }
  if (yc < 40) {
    qy[qtail++] = yc;
    while (qhead < qtail) {
      int xche = qx[qhead];
      int yche = qy[qhead];
      qhead++;
      static const int ox[4] = {-1, 1, 0, 0}, oy[4] = {0, 0, -1, 1};
      for (int k = 0; k < 4; k++) {
        int nx = xche + ox[k], ny = yche + oy[k];
        if (nx < 0 || nx > OUT_W - 1 || ny < 0 || ny > OUT_H - 1) continue;
        if (LAYER(layer1, nx, ny) == 43) {
          if (WATER(nx, ny) == 0) {
            WATER(nx, ny) = 1;
            qx[qtail] = nx;
            qy[qtail] = ny;
            qtail++;
          }
        } else {
          WATER(nx, ny) = -1;
        }
      }
    }
  }

  struct ac_list step_x = {0}, step_y = {0};
  for (int y = 1; y < OUT_H - 1; y++)
    for (int x = 1; x < OUT_W - 1; x++)
      if (LAYER(layer1, x, y) == 68 && LAYER(layer1, x, y + 1) == 17 && AC_AT(c, layer1, x, y + 2) == 20 && AC_AT(c, layer1, x, y + 3) == 43)
        if (WATER(x, y + 3) == 1) {
          ac_list_add(c, &step_x, x);
          ac_list_add(c, &step_y, y);
        }

  if (step_x.n > 0) {
    for (int i = 0; i < 10 && i < step_x.n; i++) {
      int index = AC_RND(c, step_x.n);
      int x = (int)step_x.p[index];
      int y = (int)step_y.p[index];
      LAYER(layer1, x, y + 1) = 91;
      LAYER(layer1, x, y + 2) = 91;
    }
  }

  int start_search_y = 40;
  if (step_x.n > 0) start_search_y = 2;

  struct ac_list door_x = {0}, door_y = {0};
  for (int y = start_search_y; y < OUT_H - 1; y++)
    for (int x = 16; x < OUT_W - 16; x++)
      if (LAYER(layer1, x, y) == 20 && LAYER(layer1, x + 1, y) == 20 && AC_AT(c, layer1, x, y - 1) == 17 && LAYER(layer1, x + 1, y - 1) == 17 &&
          AC_AT(c, layer1, x, y - 2) == 14 && LAYER(layer1, x + 1, y - 2) == 14) {
        int too_close = 0;
        for (int i = 0; i < door_x.n; i++) {
          int dd = x - (int)door_x.p[i];
          if (dd < 0) dd = -dd;
          if (dd <= 2 && y == (int)door_y.p[i]) too_close = 1;
        }
        if (!too_close) {
          ac_list_add(c, &door_x, x);
          ac_list_add(c, &door_y, y);
        }
      }

  int num_doors = 0;
  while (door_x.n > 0 && num_doors < 8) {
    int index = AC_RND(c, door_x.n);
    int dxp = (int)door_x.p[index];
    int dyp = (int)door_y.p[index];
    ac_list_remove_at(c, &door_x, index);
    ac_list_remove_at(c, &door_y, index);
    LAYER(layer1, dxp, dyp) = 116;
    LAYER(layer1, dxp + 1, dyp) = 100;
    LAYER(layer1, dxp, dyp - 1) = 99;
    LAYER(layer1, dxp + 1, dyp - 1) = 99;
    LAYER(layer1, dxp, dyp - 2) = 107;
    LAYER(layer1, dxp + 1, dyp - 2) = 107;
    ac_dict_set(c, &out->alt_entry, num_doors, AC_VAL(ac_xy_new(c, dxp, dyp + 1)));
    ac_dict_set(c, &out->alt_exit, AC_XYKEY(dxp, dyp), num_doors);
    ac_dict_set(c, &out->alt_exit, AC_XYKEY(dxp + 1, dyp), num_doors);
    num_doors++;
  }

  /* exit at the farthest node away from the start node */
  int max_dist = 0;
  struct ac_node *furthest = NULL;
  for (int i = 0; i < all_nodes.n; i++) {
    struct ac_node *n = (struct ac_node *)AC_PTR(all_nodes.p[i]);
    if (!ac_dict_has(&n->values, AC_STRKEY("solid"))) {
      int xd = n->x - start_node->x;
      int yd = n->y - start_node->y;
      if (xd < 0) xd = -xd;
      if (yd < 0) yd = -yd;
      int total = d2i(sqrt((double)(xd * xd + yd * yd)));
      if (total > max_dist) {
        max_dist = total;
        furthest = n;
      }
    }
  }
  ac_dict_set(c, &g->special_nodes, AC_STRKEY("end"), AC_VAL(furthest));
  if (!furthest) sr_fail(c, SOMRANDO_ERR_FAILED, "Object reference not set to an instance of an object.");

  AC_AT(c, layer1, furthest->x, furthest->y) = 97;
  AC_AT(c, layer2, furthest->x, furthest->y) = 62;
  AC_AT(c, layer1, furthest->x + 1, furthest->y) = 168;
  AC_AT(c, layer1, furthest->x - 1, furthest->y) = 168;
  AC_AT(c, layer1, furthest->x, furthest->y + 1) = 168;
  AC_AT(c, layer1, furthest->x, furthest->y - 1) = 168;

  ac_list_add(c, &out->exit_locations, AC_VAL(ac_xy_new(c, furthest->x, furthest->y)));

  uint8_t *h = out->md->header;
  ac_hdr_set_walkon(h, 1);
  set_cave_header(out);
  ac_hdr_set_palette_animated(h, 1);

  const struct ac_palsrc *pal = &palettes[AC_RND(c, 4)];
  ac_apply_palette_source(c, out, pal);
  ac_hdr_set_palette_animated(h, 1);

  make_outdoor_enemies(c, out, start_node, layer1);

  g->num_shared_doors = num_doors;
  g->has_num_shared_doors = 1;
  for (int i = 0; i < all_nodes.n; i++) ac_list_add(c, &g->shared_nodes, all_nodes.p[i]);
  return out;
#undef WATER
}

/* ---------------------------------------------------------------------------------- ForestCaveRooms (indoor layout) */
#define ROOM_BUFFER 4

static void randomize_room_shapes(struct sr_ctx *c, struct ac_layer *layer1, struct ac_layer *layer2) {
  struct ac_layer *copy1 = ac_layer_new(c, 128, 64);
  struct ac_layer *copy2 = ac_layer_new(c, 128, 64);
  memcpy(copy1->d, layer1->d, 128 * 64);
  memcpy(copy2->d, layer2->d, 128 * 64);
#define DOOR(v) ((v) == 173 || (v) == 174 || (v) == 180)
  for (int y = 2; y < 64 - 2; y += 2)
    for (int x = 2; x < 126; x += 2)
      if (LAYER(layer1, x, y) == 70) {
        if (LAYER(layer1, x, y - 2) == 5 || LAYER(layer1, x - 2, y) == 5 || LAYER(layer1, x + 2, y) == 5 || LAYER(layer1, x, y + 2) == 5) {
          if (!DOOR(LAYER(layer1, x, y)) && !DOOR(LAYER(layer1, x + 1, y)) && !DOOR(LAYER(layer1, x + 1, y - 1)) && !DOOR(LAYER(layer1, x, y - 1))) {
            if ((AC_RND(c, 2)) == 0) {
              LAYER(copy1, x, y) = 5;
              LAYER(copy1, x + 1, y) = 5;
              LAYER(copy1, x + 1, y - 1) = 5;
              LAYER(copy1, x, y - 1) = 5;
            }
          }
        }
      }
#undef DOOR
  memcpy(layer1->d, copy1->d, 128 * 64);
  memcpy(layer2->d, copy2->d, 128 * 64);
}

static void place_walls(struct sr_ctx *c, struct ac_layer *layer1, struct ac_layer *layer2) {
  static const uint8_t top_wall[5] = {8, 11, 14, 17, 20};
  static const uint8_t diag1[6] = {78, 9, 12, 15, 18, 21};
  static const uint8_t diag2[6] = {79, 7, 10, 13, 16, 19};
  struct ac_layer *copy1 = ac_layer_new(c, 128, 64);
  struct ac_layer *copy2 = ac_layer_new(c, 128, 64);
  memcpy(copy1->d, layer1->d, 128 * 64);
  memcpy(copy2->d, layer2->d, 128 * 64);

  for (int y = 1; y < 64 - 1; y++)
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

  memcpy(layer1->d, copy1->d, 128 * 64);
  memcpy(layer2->d, copy2->d, 128 * 64);

  for (int y = 1; y < 64 - 1; y++)
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
      int here = LAYER(layer1, x, y);
      int above = LAYER(layer1, x, y - 1);
      if ((here == 5 || here == 79) && (LAYER(layer1, x - 1, y) == 70 || ledge_left) && (above == 70 || ledge_above)) {
        if (above >= 19 && above <= 21) LAYER(copy1, x, y) = 71;
        else if (ledge_above) LAYER(copy1, x, y) = 1;
        else LAYER(copy1, x, y) = 71;
        corner = 1;
      }
      if ((here == 5 || here == 78) && (LAYER(layer1, x + 1, y) == 70 || ledge_right) && (above == 70 || ledge_above)) {
        if (above >= 19 && above <= 21) LAYER(copy1, x, y) = 73;
        else if (ledge_above) LAYER(copy1, x, y) = 3;
        else LAYER(copy1, x, y) = 73;
        corner = 1;
      }
      if (LAYER(layer1, x, y + 1) == 5 && here == 70 && !corner) LAYER(copy1, x, y + 1) = 2;
      if (here == 5 && (LAYER(layer1, x - 1, y) == 70 || ledge_left) && !corner) LAYER(copy1, x, y) = 4;
      if (here == 5 && (LAYER(layer1, x + 1, y) == 70 || ledge_right) && !corner) LAYER(copy1, x, y) = 6;
    }

  memcpy(layer1->d, copy1->d, 128 * 64);
  memcpy(layer2->d, copy2->d, 128 * 64);
}

static void populate_indoor_map_data(struct sr_ctx *c, struct ac_map *indoor, int num_rooms, struct ac_layer *layer1, struct ac_layer *layer2) {
  memset(layer1->d, 5, (size_t)layer1->w * (size_t)layer1->h);
  for (int i = 0; i < num_rooms; i++) {
    int center_x1 = 15, center_x2 = 16;
    int x_offset = 32 * (i % 4);
    int y_offset = 32 * (i / 4);
    int x_at_left_wall = ROOM_BUFFER;
    int x_at_right_wall = 31 - ROOM_BUFFER;
    int y_at_entry_door = 31 - ROOM_BUFFER;
    for (int y = ROOM_BUFFER; y < 32 - ROOM_BUFFER; y++)
      for (int x = ROOM_BUFFER; x < 32 - ROOM_BUFFER; x++) {
        if (x != x_at_left_wall && x != x_at_right_wall) {
          if (y == y_at_entry_door) {
            if (x == center_x1 || x == center_x2) {
              AC_AT(c, layer1, x + x_offset, y + y_offset) = 119;
              if (x == center_x1) ac_dict_set(c, &indoor->alt_entry, i, AC_VAL(ac_xy_new(c, x + x_offset, y + y_offset - 1)));
              ac_dict_set(c, &indoor->alt_exit, AC_XYKEY(x + x_offset, y + y_offset), i);
            }
          } else {
            if (y == 31 - ROOM_BUFFER - 1 && (x == center_x1 || x == center_x2)) {
              AC_AT(c, layer1, x + x_offset, y + y_offset) = (uint8_t)(x == center_x1 ? 173 : 174);
            } else {
              AC_AT(c, layer1, x + x_offset, y + y_offset) = 70;
            }
          }
        }
      }
  }
  randomize_room_shapes(c, layer1, layer2);
  place_walls(c, layer1, layer2);
}

static struct ac_map *generate_indoor(struct sr_ctx *c, struct ac_gen_ctx *g) {
  struct ac_map *in = ac_map_new(c);
  struct ac_layer *l1 = ac_layer_new(c, IN_W, IN_H);
  struct ac_layer *l2 = ac_layer_new(c, IN_W, IN_H);
  if (!g->has_num_shared_doors) sr_fail(c, SOMRANDO_ERR_FAILED, "The given key was not present in the dictionary.");
  int num_shared_doors = g->num_shared_doors;
  populate_indoor_map_data(c, in, num_shared_doors, l1, l2);
  int entry_x[8], entry_y[8];
  for (int i = 0; i < 8; i++) {
    entry_x[i] = 15 + (i % 4) * 32;
    entry_y[i] = 25 + (i / 4) * 32;
  }

  int num_dialogue_events = w8_wget_int(c, "numDialogueEvents");
  for (int i = 0; i < num_shared_doors; i++) {
    if (i >= 8) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range. Must be non-negative and less than the size of the collection.");
    for (int n = 0; n < 3; n++) {
      int npc_x = entry_x[i];
      int npc_y = entry_y[i] - 3 - n;
      int event_num = DIALOGUE_EVENT_START + (AC_RND(c, num_dialogue_events));
      uint8_t *o = ac_map_add_object(c, in->md);
      ac_obj_set_species(o, npc_ids[AC_RND(c, (int)sizeof npc_ids)]);
      ac_obj_set_vis_flag(o, 0x00);
      ac_obj_set_vis_min(o, 0x00);
      ac_obj_set_vis_max(o, 0x0F);
      ac_obj_set_xpos(o, (uint8_t)npc_x);
      ac_obj_set_ypos(o, (uint8_t)npc_y);
      ac_obj_set_event(o, (uint16_t)event_num);
      ac_obj_set_unknown_b7(o, 0x08);
      ac_obj_set_direction(o, DIR_LEFT);
      ac_obj_set_unknown4a(o, 1);
    }
  }
  in->layer1 = l1;
  in->layer2 = l2;
  set_cave_header(in);
  ac_apply_palette_source(c, in, &palettes[AC_RND(c, 4)]);
  return in;
}

static struct ac_map *generate_boss(struct sr_ctx *c, struct ac_gen_ctx *g) {
  (void)g;
  struct ac_layer *l1 = ac_layer_new(c, BOSS_W, BOSS_H);
  struct ac_layer *l2 = ac_layer_new(c, BOSS_W, BOSS_H);
  for (int y = 0; y < BOSS_H; y++)
    for (int x = 0; x < BOSS_W; x++) {
      LAYER(l1, x, y) = 43;
      LAYER(l2, x, y) = 22;
    }
  struct ac_node *boss_node = ac_node_new(c);
  boss_node->center_x = 16;
  boss_node->x = 16;
  boss_node->center_y = 16;
  boss_node->y = 16;
  ac_dict_set(c, &boss_node->values, AC_STRKEY("radius"), 10);
  clear_node_area(c, boss_node, l1, l2, 6);
  sr_p3_cave_add_tile_edges(c, l1, l2, boss_node);

  struct ac_map *m = ac_map_new(c);
  m->layer1 = l1;
  m->layer2 = l2;
  m->entry_pos = ac_xy_new(c, 16, 20);

  static const uint8_t boss_types[18] = {87, 90, 92, 94, 95, 98, 99, 101, 102, 105, 109, 111, 112, 113, 116, 120, 123, 125};
  int boss_id = boss_types[AC_RND(c, 18)];
  uint8_t *o = ac_map_add_object(c, m->md);
  ac_obj_set_species(o, boss_id);
  ac_obj_set_xpos(o, 16);
  ac_obj_set_ypos(o, 14);
  ac_obj_set_vis_flag(o, 0x00);
  ac_obj_set_vis_min(o, 0x00);
  ac_obj_set_vis_max(o, 0x0F);

  ac_hdr_set_walkon(m->md->header, 0);
  set_cave_header(m);
  ac_apply_palette_source(c, m, &palettes[AC_RND(c, 4)]);
  return m;
}

static const uint8_t song_data[5] = {0x40, 0x01, 0x0E, 0x1B, 0x8F};

static const uint8_t *song(struct sr_ctx *c, int *len) {
  (void)c;
  *len = (int)sizeof song_data;
  return song_data;
}

const struct ac_biome sr_p3_cave_biome = {"cave", "Cave", get_ctx, generate_outdoor, generate_indoor, generate_boss, song};
