/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Ancient Cave floor generator "ruins".  Derived from processing/ancientcave/mapgen/ruins/RuinsGenerator.cs (Author: Moppleton).
 * Translated to C 2026-10-18.
 */
#include <stdlib.h>
#include <string.h>

#include "sr_p3_ruins_data.h"
#include "sr_p3_ruins_int.h"

#define MAP_W 128
#define MAP_H 128
#define BOSS_W 32
#define BOSS_H 32
#define DIR_LEFT 2
#define DIALOGUE_EVENT_START 0x280

#define L1(x, y) AC_AT(c, layer1, (x), (y))
#define L2(x, y) AC_AT(c, layer2, (x), (y))

static const struct ac_palsrc outside_palettes[4] = {{NULL, 78}, {"acruins.nightruins.bin", 78}, {NULL, 32}, {"acruins.darkruins.bin", 32}};
static const struct ac_palsrc inside_boss_palettes[1] = {{NULL, 86}};

static struct ac_gen_ctx *get_ctx(struct sr_ctx *c) {
  struct ac_gen_ctx *g = ac_gen_ctx_new(c);
  g->outdoor_w = MAP_W;
  g->outdoor_h = MAP_H;
  g->indoor_w = MAP_W;
  g->indoor_h = MAP_H;
  g->boss_w = BOSS_W;
  g->boss_h = BOSS_H;
  sr_p3_ruins_make_paths(c, g);
  return g;
}

struct start_node {
  int x, y, center_x, center_y;
};

static void make_outdoor_enemies(struct sr_ctx *c, struct ac_map *m, const struct start_node *start, const struct ac_layer *layer1) {
  const int max_enemies = 40;   /* the game breaks with more than 48 */
  for (int i = 0; i < max_enemies; i++) {
    int enemy_x = AC_RND(c, 128);
    int enemy_y = AC_RND(c, 128);
    int dist_x = abs(enemy_x - start->center_x);
    int dist_y = abs(enemy_y - start->center_y);
    if (dist_x > MAP_W / 2) dist_x = MAP_W - dist_x;
    if (dist_y > MAP_H / 2) dist_y = MAP_H - dist_y;
    for (;;) {
      uint8_t t = AC_AT(c, layer1, enemy_x, enemy_y);
      if ((t == 28 || t == 168 || t == 63 || t == 170 || t == 171) && (dist_x > 16 || dist_y > 16)) break;
      enemy_x = AC_RND(c, 128);
      enemy_y = AC_RND(c, 128);
      dist_x = abs(enemy_x - start->center_x);
      dist_y = abs(enemy_y - start->center_y);
      if (dist_x > MAP_W / 2) dist_x = MAP_W - dist_x;
      if (dist_y > MAP_H / 2) dist_y = MAP_H - dist_y;
    }
    int type = sr_p3_ruins_gen_enemyIds[AC_RND(c, (int)sizeof sr_p3_ruins_gen_enemyIds)];
    uint8_t *o = ac_map_add_object(c, m->md);
    ac_obj_set_xpos(o, (uint8_t)enemy_x);
    ac_obj_set_ypos(o, (uint8_t)enemy_y);
    ac_obj_set_species(o, type);
    ac_obj_set_vis_flag(o, 0x00);
    ac_obj_set_vis_min(o, 0x00);
    ac_obj_set_vis_max(o, 0x0F);
    ac_obj_set_event(o, 0x01);
    ac_obj_set_direction(o, DIR_LEFT);
    ac_obj_set_unknown_b7(o, 0x18);
  }
}

static void common_header(uint8_t *h, int tileset, int display) {
  ac_hdr_set_tileset16(h, tileset);
  ac_hdr_set_tileset8(h, tileset);
  ac_hdr_set_display(h, display);
  ac_hdr_set_layer1_fg(h, 0);
  ac_hdr_set_layer2_fg(h, 1);
  ac_hdr_set_npc_palette(h, 0xFF);
  ac_hdr_set_flammie(h, 1);
  ac_hdr_set_magic_rope(h, 1);
}

static struct ac_map *outdoor(struct sr_ctx *c, struct ac_gen_ctx *g) {
  struct ac_layer *layer1 = ac_layer_new(c, MAP_W, MAP_H);
  struct ac_layer *layer2 = ac_layer_new(c, MAP_W, MAP_H);
  struct ac_map *m = ac_map_new(c);
  m->layer1 = layer1;
  m->layer2 = layer2;

  const int effective_h = MAP_H - 10;
  for (int y = 0; y < MAP_H; y++)
    for (int x = 0; x < MAP_W; x++) {
      L1(x, y) = y < effective_h ? 27 : 0;
      L2(x, y) = 0;
    }

  sr_p3_ruins_make_courtyard(c, layer1, layer2, m);
  struct ac_node *roof = (struct ac_node *)AC_PTR(ac_dict_get(c, &g->special_nodes, AC_STRKEY(RUINS_ROOF_NODE_NAME)));

  const int room_min_y = MAP_H - (70 + 4 * RUINS_NODE_HEIGHT_OUTDOOR);
  const int room_max_y = MAP_H - (70 - RUINS_NODE_HEIGHT_OUTDOOR - 2);
  for (int x = 0; x < MAP_W; x++) {
    for (int y = room_min_y; y < room_max_y; y++) L1(x, y) = (x % 2) == 0 ? 124 : 122;
    L1(x, room_min_y - 1) = 102;
    L1(x, room_min_y - 2) = 87;
    L1(x, room_min_y - 3) = 162;
    L1(x, room_min_y - 4) = 162;
    L1(x, room_min_y - 5) = 162;
    L1(x, room_min_y - 6) = 162;
    L1(x, room_min_y - 7) = 132;
  }

  sr_p3_ruins_outside_rooms(c, layer1, layer2, m, g);

  for (int y = 0; y < roof->y + 2; y++) {
    int ty = y - 8;
    if (ty >= 3) ty = 3;
    for (int x = 0; x < MAP_W; x++) {
      if (ty < 0) {
        L1(x, y) = 0;
        L2(x, y) = 0;
      } else if (ty == 3) {
        L1(x, y) = 0;
        L2(x, y) = 65;
      } else {
        L1(x, y) = 0;
        L2(x, y) = (uint8_t)(16 + ty * 16 + x % 7);
      }
    }
  }

  for (int y = 0; y < 7; y++)
    for (int x = 0; x < 16; x++) {
      int v1;
      if (y == 0) v1 = x == 0 ? 131 : (x == 15 ? 133 : 31);
      else v1 = x == 0 ? 144 : (x == 15 ? 145 : 171);
      L1(roof->x + x, roof->y + y) = (uint8_t)v1;
      L2(roof->x + x, roof->y + y) = 0;
    }

  struct start_node start;
  start.x = MAP_W / 2;
  start.y = MAP_H - 15;
  start.center_x = start.x;
  start.center_y = start.y;

  if (AC_RND(c, 2) == 0) {
    L1(roof->x + 7, roof->y + 3) = 165;
    L1(roof->x + 6, roof->y + 4) = 165;
    L1(roof->x + 8, roof->y + 4) = 165;
    L1(roof->x + 7, roof->y + 5) = 165;
    L1(roof->x + 7, roof->y + 4) = 186;
    ac_list_add(c, &m->exit_locations, AC_VAL(ac_xy_new(c, roof->x + 7, roof->y + 4)));
  } else {
    L1(MAP_W / 2 - 1, MAP_H - 10) = 185;
    L1(MAP_W / 2, MAP_H - 10) = 185;
    ac_list_add(c, &m->exit_locations, AC_VAL(ac_xy_new(c, MAP_W / 2 - 1, MAP_H - 10)));
    ac_list_add(c, &m->exit_locations, AC_VAL(ac_xy_new(c, MAP_W / 2, MAP_H - 10)));
    start.x = roof->x + 7;
    start.y = roof->y + 4;
    start.center_x = start.x;
    start.center_y = start.y;
  }

  m->entry_pos = ac_xy_new(c, start.x, start.y);

  ac_hdr_set_walkon(m->md->header, 1);
  common_header(m->md->header, 8, 0);
  make_outdoor_enemies(c, m, &start, layer1);

  ac_apply_palette_source(c, m, &outside_palettes[AC_RND(c, 4)]);
  return m;
}

static struct ac_map *indoor(struct sr_ctx *c, struct ac_gen_ctx *g) {
  struct ac_layer *layer1 = ac_layer_new(c, MAP_W, MAP_H);
  struct ac_layer *layer2 = ac_layer_new(c, MAP_W, MAP_H);
  struct ac_map *m = ac_map_new(c);
  m->layer1 = layer1;
  m->layer2 = layer2;
  for (int y = 0; y < MAP_H; y++)
    for (int x = 0; x < MAP_W; x++) {
      L1(x, y) = 18;
      L2(x, y) = 0;
    }

  sr_p3_ruins_inside_rooms(c, layer1, layer2, m, g);
  struct ac_dict all;
  ac_gen_ctx_nodes_by_name(c, g, &all);
  int steps_start_x = sr_p3_ruins_node_get(c, &all, 'i', 2, RUINS_NUM_NODES_X / 2)->x - 4;
  for (int x = steps_start_x; x < steps_start_x + 4; x++)
    for (int y = MAP_H - 18; y < MAP_H - 10; y++) {
      L1(x, y) = 108;
      if (y == MAP_H - 18) {
        if (x == steps_start_x) {
          L1(x, y) = 120;
          L2(x, y) = 154;
        } else if (x == steps_start_x + 3) {
          L1(x, y) = 120;
          L2(x, y) = 155;
        }
      } else {
        if (x == steps_start_x) {
          L1(x, y) = 120;
          L2(x, y) = 170;
        } else if (x == steps_start_x + 3) {
          L1(x, y) = 120;
          L2(x, y) = 171;
        }
      }
    }

  L1(steps_start_x + 1, MAP_H - 10) = 65;
  L1(steps_start_x + 2, MAP_H - 10) = 65;
  L2(steps_start_x + 1, MAP_H - 10) = 164;
  L2(steps_start_x + 2, MAP_H - 10) = 164;
  ac_dict_set(c, &m->alt_exit, AC_XYKEY(steps_start_x + 1, MAP_H - 10), -1);
  ac_dict_set(c, &m->alt_exit, AC_XYKEY(steps_start_x + 2, MAP_H - 10), -1);
  ac_dict_set(c, &m->alt_entry, -1, AC_VAL(ac_xy_new(c, steps_start_x + 1, MAP_H - 11)));

  int num_dialogue_events = w8_wget_int(c, "numDialogueEvents");
  for (int i = 0; i < 15; i++) {
    int npc_x = 0, npc_y = 0;
    int64_t entry;
    if (ac_dict_try_get(&m->same_entry, i, &entry)) {
      const struct ac_xy *p = (const struct ac_xy *)AC_PTR(entry);
      npc_x = p->x;
      npc_y = p->y - 2;
    } else {
      int floor_num = (i - 10) + 2;
      int found = 0, iters = 0;
      while (!found && iters < 100) {
        int x_room = AC_RND(c, 4);
        struct ac_node *n = sr_p3_ruins_node_get(c, &all, 'i', floor_num, x_room);
        int xpos = n->x + AC_RND(c, 16);
        int ypos = n->y + AC_RND(c, 16);
        if (xpos >= 0 && ypos >= 0 && xpos < MAP_W && ypos < MAP_H) {
          if (L1(xpos, ypos) == 103 || L1(xpos, ypos) == 100) {
            found = 1;
            npc_x = xpos;
            npc_y = ypos;
          }
        }
        iters++;
      }
    }
    int event_num = DIALOGUE_EVENT_START + AC_RND(c, num_dialogue_events);
    uint8_t *o = ac_map_add_object(c, m->md);
    ac_obj_set_species(o, sr_p3_ruins_base_npcIds[AC_RND(c, (int)sizeof sr_p3_ruins_base_npcIds)]);
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

  common_header(m->md->header, 15, 2);
  ac_apply_palette_source(c, m, &inside_boss_palettes[AC_RND(c, 1)]);
  return m;
}

static struct ac_map *boss(struct sr_ctx *c, struct ac_gen_ctx *g) {
  (void)g;
  struct ac_layer *layer1 = ac_layer_new(c, BOSS_W, BOSS_H);
  struct ac_layer *layer2 = ac_layer_new(c, BOSS_W, BOSS_H);
  for (int y = 0; y < BOSS_H; y++)
    for (int x = 0; x < BOSS_W; x++) {
      L1(x, y) = 18;
      L2(x, y) = 0;
    }

  const int b = 6;
  for (int y = b; y < BOSS_H - b; y++)
    for (int x = b; x < BOSS_W - b; x++) {
      if (y != b && y < BOSS_H - (b + 1)) {
        if (x == b) L1(x, y) = 17;
        else if (x == BOSS_W - (b + 1)) L1(x, y) = 19;
      }
      if (x != b && x != BOSS_W - (b + 1)) {
        if (y == b) {
          L1(x, y) = 2;
          L1(x, y + 1) = 139;
          L1(x, y + 2) = 56;
          L1(x, y + 3) = 56;
          L1(x, y + 4) = 56;
          L1(x, y + 5) = 56;
          L1(x, y + 6) = 136;
          L1(x, y + 7) = 103;
          if (x == b + 1) L1(x, y + 8) = 103;
          else if (x == b + 2) L1(x, y + 8) = 48;
          else if (x == BOSS_W - (b + 3)) L1(x, y + 8) = 50;
          else if (x == BOSS_W - (b + 2)) L1(x, y + 8) = 103;
          else L1(x, y + 8) = 49;

          for (int yy = y + 9; yy < BOSS_H - (b + 3); yy++) {
            if (x == b + 1) L1(x, yy) = 103;
            else if (x == b + 2) L1(x, yy) = 64;
            else if (x == BOSS_W - (b + 3)) L1(x, yy) = 66;
            else if (x == BOSS_W - (b + 2)) L1(x, yy) = 103;
            else L1(x, yy) = 184;
          }

          if (x == b + 1) L1(x, BOSS_H - (b + 3)) = 103;
          else if (x == b + 2) L1(x, BOSS_H - (b + 3)) = 80;
          else if (x == BOSS_W - (b + 3)) L1(x, BOSS_H - (b + 3)) = 82;
          else if (x == BOSS_W - (b + 2)) L1(x, BOSS_H - (b + 3)) = 103;
          else L1(x, BOSS_H - (b + 3)) = 81;

          L1(x, BOSS_H - (b + 2)) = 103;
        } else if (y == BOSS_H - (b + 1)) {
          L1(x, y) = 34;
        }
      }
    }

  L1(b + 5, b + 7) = 40;
  L1(b + 6, b + 7) = 41;
  L1(b + 5, b + 6) = 24;
  L1(b + 6, b + 6) = 25;
  L1(b + 5, b + 5) = 8;
  L1(b + 6, b + 5) = 9;

  L1(b + 10, b + 6) = 170;
  L1(b + 11, b + 6) = 171;
  L1(b + 12, b + 6) = 172;
  L1(b + 10, b + 5) = 154;
  L1(b + 11, b + 5) = 155;
  L1(b + 12, b + 5) = 156;
  L1(b + 10, b + 4) = 46;
  L1(b + 11, b + 4) = 92;
  L1(b + 12, b + 4) = 47;

  struct ac_map *m = ac_map_new(c);
  m->layer1 = layer1;
  m->layer2 = layer2;
  m->entry_pos = ac_xy_new(c, 16, 24);

  static const uint8_t supported_boss_types[] = {87, 90, 92, 94, 95, 98, 99, 101, 102, 105, 109, 111, 112, 113, 116, 120, 123, 125};
  int boss_id = supported_boss_types[AC_RND(c, (int)sizeof supported_boss_types)];
  uint8_t *o = ac_map_add_object(c, m->md);
  ac_obj_set_species(o, boss_id);
  ac_obj_set_xpos(o, 16);
  ac_obj_set_ypos(o, 16);
  ac_obj_set_vis_flag(o, 0x00);
  ac_obj_set_vis_min(o, 0x00);
  ac_obj_set_vis_max(o, 0x0F);

  ac_hdr_set_walkon(m->md->header, 0);
  common_header(m->md->header, 15, 2);
  ac_apply_palette_source(c, m, &inside_boss_palettes[AC_RND(c, 1)]);
  return m;
}

static const uint8_t song_data[5] = {0x40, 0x01, 0x0F, 0x18, 0x8F};
static const uint8_t *song(struct sr_ctx *c, int *len) {
  (void)c;
  *len = 5;
  return song_data;
}

const struct ac_biome sr_p3_ruins_biome = {"ruins", "Ruins", get_ctx, outdoor, indoor, boss, song};
