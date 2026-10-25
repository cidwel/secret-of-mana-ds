/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Ancient Cave "ruins" floors: node graph and main path.  Derived from processing/ancientcave/mapgen/ruins/RuinsPathGenerator.cs
 * (Author: Moppleton).  Translated to C 2026-10-18.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sr_p3_ruins_int.h"

char *sr_p3_ruins_node_name(struct sr_ctx *c, char prefix, int a, int b) {
  char *s = (char *)sr_alloc(c, 32);
  snprintf(s, 32, "%c_%d_%d", prefix, a, b);
  return s;
}

struct ac_node *sr_p3_ruins_node_find(struct sr_ctx *c, const struct ac_dict *all, char prefix, int a, int b) {
  (void)c;
  char buf[32];
  snprintf(buf, sizeof buf, "%c_%d_%d", prefix, a, b);
  int64_t v;
  return ac_dict_try_get(all, AC_STRKEY(buf), &v) ? (struct ac_node *)AC_PTR(v) : NULL;
}

struct ac_node *sr_p3_ruins_node_get(struct sr_ctx *c, const struct ac_dict *all, char prefix, int a, int b) {
  char buf[32];
  snprintf(buf, sizeof buf, "%c_%d_%d", prefix, a, b);
  return (struct ac_node *)AC_PTR(ac_dict_get(c, all, AC_STRKEY(buf)));
}

int sr_p3_ruins_val(struct sr_ctx *c, const struct ac_node *n, const char *key) { return (int)ac_dict_get(c, &n->values, AC_STRKEY(key)); }

static void set_val(struct sr_ctx *c, struct ac_node *n, const char *key, int v) { ac_dict_set(c, &n->values, AC_STRKEY(key), v); }

static void connect(struct sr_ctx *c, struct ac_node *a, struct ac_node *b) {
  ac_list_add(c, &a->connections, AC_VAL(b));
  ac_list_add(c, &b->connections, AC_VAL(a));
}

static void visit(struct sr_ctx *c, struct ac_list *visited, struct ac_node *n) {
  if (!ac_list_contains(visited, AC_VAL(n))) ac_list_add(c, visited, AC_VAL(n));
}

void sr_p3_ruins_make_paths(struct sr_ctx *c, struct ac_gen_ctx *g) {
  const int map_height = g->outdoor_h;
  struct ac_dict all;
  ac_dict_init(&all, AC_DICT_STR);
  struct ac_list visited = {0};
  struct ac_node *roof = NULL;
  for (int floor = 2; floor <= RUINS_NUM_NODES_Y; floor++) {
    for (int x = 0; x < RUINS_NUM_NODES_X; x++) {
      struct ac_node *in = ac_node_new(c);
      in->name = sr_p3_ruins_node_name(c, 'i', floor, x);
      in->x = RUINS_NODE_WIDTH / 2 + x * RUINS_NODE_WIDTH;
      in->center_x = in->x;
      in->y = map_height - (floor * RUINS_NODE_HEIGHT_INDOOR);
      in->center_y = in->y;
      set_val(c, in, "xpos", x);
      set_val(c, in, "ypos", floor);
      set_val(c, in, "mainpath", 0);
      ac_dict_add(c, &all, AC_STRKEY(in->name), AC_VAL(in));

      struct ac_node *out = ac_node_new(c);
      out->name = sr_p3_ruins_node_name(c, 'o', floor, x);
      out->x = RUINS_NODE_WIDTH / 2 + x * RUINS_NODE_WIDTH;
      out->center_x = out->x;
      out->y = map_height - (70 + (floor - 2) * RUINS_NODE_HEIGHT_OUTDOOR);
      out->center_y = out->y;
      set_val(c, out, "xpos", x);
      set_val(c, out, "ypos", floor);
      set_val(c, out, "mainpath", 0);
      ac_dict_add(c, &all, AC_STRKEY(out->name), AC_VAL(out));

      if (floor == 2) {
        ac_list_add(c, &visited, AC_VAL(in));
        ac_list_add(c, &visited, AC_VAL(out));
      }
    }
  }

  int lower = AC_RND(c, RUINS_NUM_NODES_X);
  int lower_outer = 1;
  for (int floor = 2; floor < RUINS_NUM_NODES_Y; floor++) {
    if (floor == 2)
      for (int x = 0; x < RUINS_NUM_NODES_X - 1; x++)
        connect(c, sr_p3_ruins_node_get(c, &all, 'i', 2, x), sr_p3_ruins_node_get(c, &all, 'i', 2, x + 1));

    int upper = AC_RND(c, RUINS_NUM_NODES_X);
    int h_dist = abs(lower - upper);
    char side = lower_outer ? 'o' : 'i';
    char opp = lower_outer ? 'i' : 'o';
    if (h_dist == 0) {
      struct ac_node *n1 = sr_p3_ruins_node_get(c, &all, side, floor, lower);
      struct ac_node *n2 = sr_p3_ruins_node_get(c, &all, side, floor + 1, upper);
      struct ac_node *n2_opp = sr_p3_ruins_node_get(c, &all, opp, floor + 1, upper);
      connect(c, n1, n2);
      set_val(c, n1, "mainpath", 1);
      set_val(c, n2, "mainpath", 1);
      set_val(c, n2_opp, "mainpath", 1);
      if (floor == 6) roof = n2;
      visit(c, &visited, n2);
      visit(c, &visited, n2_opp);
    } else {
      int v_change = AC_RND(c, h_dist);
      int xpos = lower;
      int x_change = upper < lower ? -1 : 1;
      int up = 0;
      for (int x = 0; x < h_dist; x++) {
        if (x == v_change) {
          struct ac_node *n1 = sr_p3_ruins_node_get(c, &all, side, floor, xpos);
          struct ac_node *n2 = sr_p3_ruins_node_get(c, &all, side, floor + 1, xpos);
          connect(c, n1, n2);
          set_val(c, n1, "mainpath", 1);
          set_val(c, n2, "mainpath", 1);
          if (floor == 6) roof = n2;
          visit(c, &visited, n2);
          up = 1;
        }
        {
          int f = up ? floor + 1 : floor;
          struct ac_node *n1 = sr_p3_ruins_node_get(c, &all, side, f, xpos);
          struct ac_node *n2 = sr_p3_ruins_node_get(c, &all, side, f, xpos + x_change);
          connect(c, n1, n2);
          set_val(c, n1, "mainpath", 1);
          set_val(c, n2, "mainpath", 1);
          visit(c, &visited, n2);
        }
        xpos += x_change;
      }
      struct ac_node *final_opp = sr_p3_ruins_node_get(c, &all, opp, floor + 1, upper);
      ac_list_add(c, &visited, AC_VAL(final_opp));
      set_val(c, final_opp, "mainpath", 1);
    }
    lower = upper;
    lower_outer = !lower_outer;
  }

  int passes = 0;
  while (all.count - all.free_count != visited.n) {
    if (++passes > 100000) sr_fail(c, SOMRANDO_ERR_FAILED, "ruins path generation did not converge");
    for (int e = 0; e < all.count; e++) {
      if (!all.e[e].used) continue;
      struct ac_node *n = (struct ac_node *)AC_PTR(all.e[e].val);
      char side = n->name[0] == 'o' ? 'o' : 'i';
      if (!ac_list_contains(&visited, AC_VAL(n)) || sr_p3_ruins_val(c, n, "mainpath") == 0) {
        struct ac_list poss = {0};
        int xp = sr_p3_ruins_val(c, n, "xpos"), yp = sr_p3_ruins_val(c, n, "ypos");
        if (xp != 0) {
          struct ac_node *o = sr_p3_ruins_node_get(c, &all, side, yp, xp - 1);
          if (ac_list_contains(&visited, AC_VAL(o)) || sr_p3_ruins_val(c, o, "mainpath") == 0) ac_list_add(c, &poss, AC_VAL(o));
        }
        if (xp != RUINS_NUM_NODES_X - 1) {
          struct ac_node *o = sr_p3_ruins_node_get(c, &all, side, yp, xp + 1);
          if (ac_list_contains(&visited, AC_VAL(o)) || sr_p3_ruins_val(c, o, "mainpath") == 0) ac_list_add(c, &poss, AC_VAL(o));
        }
        if (yp < RUINS_NUM_NODES_Y - 1) {
          struct ac_node *o = sr_p3_ruins_node_get(c, &all, side, yp + 1, xp);
          if (ac_list_contains(&visited, AC_VAL(o)) || sr_p3_ruins_val(c, o, "mainpath") == 0) ac_list_add(c, &poss, AC_VAL(o));
        }
        if (yp != 2) {
          struct ac_node *o = sr_p3_ruins_node_get(c, &all, side, yp - 1, xp);
          if (ac_list_contains(&visited, AC_VAL(o)) || sr_p3_ruins_val(c, o, "mainpath") == 0) ac_list_add(c, &poss, AC_VAL(o));
        }
        if (poss.n > 0) {
          int index = AC_RND(c, poss.n);
          struct ac_node *other = (struct ac_node *)AC_PTR(ac_list_get(c, &poss, index));
          connect(c, n, other);
          visit(c, &visited, n);
          struct ac_node *opposite = sr_p3_ruins_node_get(c, &all, side == 'o' ? 'i' : 'o', yp, xp);
          visit(c, &visited, opposite);
        }
      }
    }
  }

  for (int e = 0; e < all.count; e++)
    if (all.e[e].used) ac_list_add(c, &g->shared_nodes, all.e[e].val);
  ac_dict_set(c, &g->special_nodes, AC_STRKEY(RUINS_ROOF_NODE_NAME), AC_VAL(roof));
}
