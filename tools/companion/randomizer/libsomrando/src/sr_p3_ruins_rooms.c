/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Ancient Cave "ruins" floors: the outdoor balconies and the indoor rooms.  Derived from
 * processing/ancientcave/mapgen/ruins/{OutsideRooms,InsideRooms}.cs (Author: Moppleton).  Translated to C 2026-10-18.
 */
#include <stdio.h>
#include <string.h>

#include "sr_p3_ruins_data.h"
#include "sr_p3_ruins_int.h"

#define L1(x, y) AC_AT(c, l1, (x), (y))
#define L2(x, y) AC_AT(c, l2, (x), (y))

/* nodes in the Dictionary enumeration order of getSharedNodesByName() */
static struct ac_node *dict_node(const struct ac_dict *d, int e) { return (struct ac_node *)AC_PTR(d->e[e].val); }

/* ------------------------------------------------------------------------------------------------------ OutsideRooms */
void sr_p3_ruins_outside_rooms(struct sr_ctx *c, struct ac_layer *l1, struct ac_layer *l2, struct ac_map *outside, const struct ac_gen_ctx *g) {
  (void)l2;
  struct ac_dict all;
  ac_gen_ctx_nodes_by_name(c, g, &all);
#define O(n) sr_p3_ruins_out_##n
  for (int e = 0; e < all.count; e++) {
    if (!all.e[e].used) continue;
    struct ac_node *n = dict_node(&all, e);
    if (n->name[0] != 'o') continue;
    int ypos = sr_p3_ruins_val(c, n, "ypos");
    int xpos = sr_p3_ruins_val(c, n, "xpos");
    int door_id = (int)(((uint32_t)ypos << 16) + (uint32_t)xpos);   /* same id as the door in InsideRooms */
    if (ypos <= 6) {
      for (int y = 0; y < 9; y++)
        for (int x = 0; x < 16; x++) {
          int px = n->x + x, py = n->y + y;
          L1(px, py) = sr_p3_ruins_out_layer1BaseRoom[y * 16 + x];
          if (L1(px, py) == 177 || L1(px, py) == 181) {
            ac_dict_set(c, &outside->alt_exit, AC_XYKEY(px, py), door_id);
            if (L1(px, py) == 177) ac_dict_set(c, &outside->alt_entry, door_id, AC_VAL(ac_xy_new(c, px, py + 1)));
          }
        }
    }
  }

  for (int e = 0; e < all.count; e++) {
    if (!all.e[e].used) continue;
    struct ac_node *n = dict_node(&all, e);
    int ypos = sr_p3_ruins_val(c, n, "ypos");
    if (n->name[0] == 'o' && ypos <= 6) {
      int xpos = sr_p3_ruins_val(c, n, "xpos");
      struct ac_node *left = sr_p3_ruins_node_find(c, &all, 'o', ypos, xpos - 1);
      if (left && ac_list_contains(&n->connections, AC_VAL(left))) ac_apply_segment(c, l1, n->x, n->y, &O(noWallLeft));
      struct ac_node *right = sr_p3_ruins_node_find(c, &all, 'o', ypos, xpos + 1);
      if (right && ac_list_contains(&n->connections, AC_VAL(right))) {
        ac_apply_segment(c, l1, n->x, n->y, &O(noWallRight));
        for (int i = 0; i < 8; i += 2) {
          ac_apply_segment(c, l1, n->x + i, n->y, &O(bridge));
          for (int y = 0; y < 6; y++) {
            int bx = n->x + i + O(bridge).x, by = n->y + y + O(bridge).h;
            if (L1(bx, by) == 122) L1(bx, by) = 125;
            if (L1(bx, by) == 124) L1(bx, by) = 127;
            if (L1(bx + 1, by) == 122) L1(bx + 1, by) = 125;
            if (L1(bx + 1, by) == 124) L1(bx + 1, by) = 127;
          }
        }
      }
    }
  }

  for (int e = 0; e < all.count; e++) {
    if (!all.e[e].used) continue;
    struct ac_node *n = dict_node(&all, e);
    if (n->name[0] != 'o') continue;
    int xpos = sr_p3_ruins_val(c, n, "xpos");
    int ypos = sr_p3_ruins_val(c, n, "ypos");
    int right_steps = ((xpos + ypos) % 2) == 0;
    struct ac_node *top = sr_p3_ruins_node_find(c, &all, 'o', ypos + 1, xpos);
    if (top && (ypos < 6 || sr_p3_ruins_val(c, top, "mainpath") == 1)) {
      if (ac_list_contains(&n->connections, AC_VAL(top))) {
        ac_apply_segment(c, l1, n->x, n->y, right_steps ? &O(stairsUpRight) : &O(stairsUpLeft));
        ac_apply_segment(c, l1, top->x, top->y, right_steps ? &O(stairsDownRight) : &O(stairsDownLeft));
      }
    }
  }
#undef O
}

/* ------------------------------------------------------------------------------------------------------- InsideRooms */
#define I(n) sr_p3_ruins_ins_##n

static int node_has_name(const struct ac_node *n, const char *sub) { return strstr(n->name, sub) != NULL; }

void sr_p3_ruins_inside_rooms(struct sr_ctx *c, struct ac_layer *l1, struct ac_layer *l2, struct ac_map *inside, const struct ac_gen_ctx *g) {
  struct ac_dict all;
  ac_gen_ctx_nodes_by_name(c, g, &all);
  const int room_x_off = 0, room_y_off = 0;

  int hallway_door_num = 0;
  for (int e = 0; e < all.count; e++) {
    if (!all.e[e].used) continue;
    struct ac_node *n = dict_node(&all, e);
    if (n->name[0] != 'i') continue;
    int floor = sr_p3_ruins_val(c, n, "ypos");
    int xpos = sr_p3_ruins_val(c, n, "xpos");
    int door_id = (int)(((uint32_t)floor << 16) + (uint32_t)xpos);   /* same id as the door in OutsideRooms */
    int indoor_room = xpos == 0 || xpos == 3;
    if (floor > 6) continue;
    int room1 = (floor % 2) == 0;
    const uint8_t *tpl = indoor_room ? (room1 ? I(layer1BaseRoom1Door) : I(layer1BaseRoom2Door))
                                     : (room1 ? I(layer1BaseRoom1NoDoor) : I(layer1BaseRoom2NoDoor));
    for (int y = 0; y < 15; y++)
      for (int x = 0; x < 16; x++) {
        int px = n->x + x + room_x_off, py = n->y + y + room_y_off;
        L1(px, py) = tpl[y * 16 + x];
        if (L1(px, py) == 65) {
          L2(px, py) = 164;
          ac_dict_set(c, &inside->alt_exit, AC_XYKEY(px, py), door_id);
          ac_dict_set(c, &inside->alt_entry, door_id, AC_VAL(ac_xy_new(c, px, py - 1)));
        }
        if (L1(px, py) == 181) ac_dict_set(c, &inside->same_exit, AC_XYKEY(px, py), hallway_door_num++);
      }
  }

  struct ac_segment table_seg;
  const struct ac_segment *decors[7] = {&I(l2PottedPlant), &I(l2Jar), &I(l2Clock), &I(l2Swords), &I(l2Bottle), &I(l2Ball), &I(l2Book)};
  const int table_resize_col = 1, table_resize_row = 1;

  for (int e = 0; e < all.count; e++) {
    if (!all.e[e].used) continue;
    struct ac_node *n = dict_node(&all, e);
    int ypos = sr_p3_ruins_val(c, n, "ypos");
    if (n->name[0] == 'i' && ypos <= 6) {
      int xpos = sr_p3_ruins_val(c, n, "xpos");
      int room1 = (ypos % 2) == 0;
      struct ac_node *left = sr_p3_ruins_node_find(c, &all, 'i', ypos, xpos - 1);
      if (left && ac_list_contains(&n->connections, AC_VAL(left)))
        ac_apply_segment(c, l1, n->x + room_x_off, n->y + room_y_off, room1 ? &I(noWallLeftRoom1) : &I(noWallLeftRoom2));
      struct ac_node *right = sr_p3_ruins_node_find(c, &all, 'i', ypos, xpos + 1);
      if (right && ac_list_contains(&n->connections, AC_VAL(right))) {
        ac_apply_segment(c, l1, n->x + room_x_off, n->y + room_y_off, room1 ? &I(noWallRightRoom1) : &I(noWallRightRoom2));
        for (int i = 0; i < 8; i++)
          ac_apply_segment(c, l1, n->x + room_x_off + i, n->y + room_y_off, room1 ? &I(noWallRightRoom1) : &I(noWallRightRoom2));
      }
    }
  }

  for (int e = 0; e < all.count; e++) {
    if (!all.e[e].used) continue;
    struct ac_node *n = dict_node(&all, e);
    int ypos = sr_p3_ruins_val(c, n, "ypos");
    if (!(n->name[0] == 'i' && ypos <= 6)) continue;
    int xpos = sr_p3_ruins_val(c, n, "xpos");

    int width = 16;
    int right_present = 0;
    struct ac_node *right = sr_p3_ruins_node_find(c, &all, 'i', ypos, xpos + 1);
    if (right && ac_list_contains(&n->connections, AC_VAL(right))) {
      width += 4;
      right_present = 1;
    }

    if (right_present) {
      if ((ypos % 2) == 0) {
        ac_apply_segment(c, l1, n->x + 16 + room_x_off + 1, n->y + room_y_off + 1, &I(l1Window1));
        ac_apply_segment(c, l2, n->x + 16 + room_x_off + 1, n->y + room_y_off + 1, &I(l2Window1));
      } else if (AC_RND(c, 2) == 0) {
        ac_apply_segment(c, l1, n->x + 16 + room_x_off + 2, n->y + room_y_off + 1, &I(l1Window2));
        ac_apply_segment(c, l2, n->x + 16 + room_x_off + 2, n->y + room_y_off + 1, &I(l2Window2));
      } else {
        ac_apply_segment(c, l1, n->x + 16 + room_x_off + 1, n->y + room_y_off + 1, &I(l1Window2));
        ac_apply_segment(c, l2, n->x + 16 + room_x_off + 1, n->y + room_y_off + 1, &I(l2Window2));
        ac_apply_segment(c, l1, n->x + 16 + room_x_off + 3, n->y + room_y_off + 1, &I(l1Window2));
        ac_apply_segment(c, l2, n->x + 16 + room_x_off + 3, n->y + room_y_off + 1, &I(l2Window2));
      }
    }

    int steps_up = 0, steps_down = 0;
    for (int k = 0; k < n->connections.n; k++) {
      struct ac_node *other = (struct ac_node *)AC_PTR(n->connections.p[k]);
      char key[32];
      if (ypos <= 5) {
        snprintf(key, sizeof key, "i_%d", ypos + 1);
        if (node_has_name(other, key)) steps_up = 1;
      }
      if (ypos >= 2) {
        snprintf(key, sizeof key, "i_%d", ypos - 1);
        if (node_has_name(other, key)) steps_down = 1;
      }
    }

    if (AC_RND(c, 4) != 0 && !(ypos == 2 && xpos == 1)) {
      int t_width = AC_RND(c, width - 12);
      int t_height = AC_RND(c, 3);
      int t_posx = 1 + AC_RND(c, width - 1 - (t_width + 2));
      int t_posy = 7;
      if (AC_RND(c, 2) > 0 && t_height == 0) t_posy++;
      if (steps_up && t_posy == 7) {
        t_posy++;
        if (t_height > 0) t_height--;
      }
      if (steps_down) {
        while (t_posy + t_height > 8) {
          if (t_height > 0) t_height--;
          else t_posy--;
        }
      }

      table_seg.x = t_posx;
      table_seg.y = t_posy;
      table_seg.w = t_width + I(l1Table).w - 1;
      table_seg.h = t_height + I(l1Table).h - 1;
      size_t cells = (size_t)table_seg.w * (size_t)table_seg.h;
      uint8_t *tdata = (uint8_t *)sr_alloc(c, cells ? cells : 1);
      uint8_t *occupied = (uint8_t *)sr_alloc(c, cells ? cells : 1);
      table_seg.data = tdata;
      int source_y = 0;
      for (int y = 0; y < table_seg.h; y++) {
        int source_x = 0;
        for (int x = 0; x < table_seg.w; x++) {
          int si = source_y * I(l1Table).w + source_x;
          if (source_y < 0 || source_x < 0 || si >= I(l1Table).w * I(l1Table).h)
            sr_fail(c, SOMRANDO_ERR_FAILED, "Index was outside the bounds of the array.");
          tdata[y * table_seg.w + x] = I(l1Table).data[si];
          occupied[y * table_seg.w + x] = (uint8_t)(y == table_seg.h - 1);
          if (x < table_resize_col) source_x++;
          if (x >= table_resize_col + t_width - 1) source_x++;
        }
        if (y < table_resize_row) source_y++;
        if (y >= table_resize_row + t_height - 1) source_y++;
      }
      ac_apply_segment(c, l1, n->x + room_x_off, n->y + room_y_off, &table_seg);

      for (int i = 0; i < 10; i++) {
        const struct ac_segment *decor = decors[AC_RND(c, 7)];
        int xp = AC_RND(c, table_seg.w - decor->w + 1);
        int yp = AC_RND(c, table_seg.h - decor->h);
        int ok = 1;
        for (int y = 0; y < decor->h; y++)
          for (int x = 0; x < decor->w; x++) {
            int index = (yp + y) * table_seg.w + (x + xp);
            if (index >= (int)cells || occupied[index]) ok = 0;
          }
        if (ok) {
          for (int y = 0; y < decor->h; y++)
            for (int x = 0; x < decor->w; x++) occupied[(yp + y) * table_seg.w + (x + xp)] = 1;
          ac_apply_segment(c, l2, n->x + room_x_off + table_seg.x + xp, n->y + room_y_off + table_seg.y + yp, decor);
        }
      }
    }
    if (AC_RND(c, 3) > 0 && !steps_up && right_present) {
      int t_posx = AC_RND(c, 6);
      ac_apply_segment(c, l1, n->x + 16 + room_x_off + t_posx, n->y + room_y_off + 4, &I(l1Bookshelf));
    }
  }

  for (int e = 0; e < all.count; e++) {
    if (!all.e[e].used) continue;
    struct ac_node *n = dict_node(&all, e);
    int ypos = sr_p3_ruins_val(c, n, "ypos");
    if (!(n->name[0] == 'i' && ypos <= 6)) continue;
    int xpos = sr_p3_ruins_val(c, n, "xpos");
    int right_steps = ((xpos + ypos) % 2) == 0;
    struct ac_node *top = ypos <= 5 ? sr_p3_ruins_node_find(c, &all, 'i', ypos + 1, xpos) : NULL;
    if (top && ac_list_contains(&n->connections, AC_VAL(top))) {
      ac_apply_segment(c, l1, n->x + room_x_off, n->y + room_y_off, right_steps ? &I(stairsUpRight) : &I(stairsUpLeft));
      ac_apply_segment(c, l1, top->x + room_x_off, top->y + room_y_off, right_steps ? &I(stairsDownRight) : &I(stairsDownLeft));
      ac_apply_segment(c, l2, n->x + room_x_off, n->y + room_y_off, right_steps ? &I(l2stairsUpRight) : &I(l2stairsUpLeft));
      ac_apply_segment(c, l2, top->x + room_x_off, top->y + room_y_off, right_steps ? &I(l2stairsDownRight) : &I(l2stairsDownLeft));
    }
  }

  const uint8_t *inner = I(layer1BaseInnerRoom);
  int room_num = 0;
  for (int ry = 0; ry < 2; ry++)
    for (int rx = 0; rx < 8; rx++) {
      int x_entry = 0, y_entry = 0;
      for (int y = 0; y < 18; y++)
        for (int x = 0; x < 12; x++) {
          L1(1 + rx * 16 + x, 1 + ry * 19 + y) = inner[y * 12 + x];
          if (inner[y * 12 + x] == 190) {
            L2(1 + rx * 16 + x, 1 + ry * 19 + y) = 164;
            if (x_entry == 0) {
              x_entry = x;
              y_entry = y - 1;
            }
          }
        }
      if (room_num < hallway_door_num) ac_dict_set(c, &inside->same_entry, room_num++, AC_VAL(ac_xy_new(c, x_entry, y_entry - 1)));
    }

  for (int ry = 2; ry < 6; ry++)
    for (int y = 0; y < 18; y++)
      for (int x = 0; x < 12; x++) {
        L1(1 + 7 * 16 + x, 1 + ry * 19 + y) = inner[y * 12 + x];
        if (inner[y * 12 + x] == 190) L2(1 + 7 * 16 + x, 1 + ry * 19 + y) = 164;
      }
}
