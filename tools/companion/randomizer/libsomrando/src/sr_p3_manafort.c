/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Ancient Cave floor generator "Manafort Interior".  Derived from (Author of every file: Moppleton)
 * processing/ancientcave/mapgen/manafortint/{ManafortInteriorGenerator,ManafortIntNpcRooms}.cs.  The room makers are in sr_p3_manafort_rooms.c;
 * getMapLogString (the spoiler map drawing) is not part of the floor data and is not ported.  Translated to C 2026-10-18.
 */
#include <string.h>

#include "sr_p3_manafort.h"

#define OUT_W 128
#define OUT_H 128
#define IN_W 128
#define IN_H 64
#define BOSS_W 32
#define BOSS_H 32
#define ROOMS_X 4
#define ROOMS_Y 4
#define DIALOGUE_EVENT_START 0x280
#define DIR_LEFT 2

static const uint8_t enemy_valid_tiles[] = {48, 1, 2, 17, 18, 5, 6, 21, 22, 10, 11, 12, 26, 28, 42, 43, 44};

static const uint8_t npc_ids[] = {0x99, 0x9C, 0x9E, 0x9F, 0xA0, 0xA1, 0xA4, 0xA5, 0xA6, 0xA7, 0xA8, 0xA9, 0xAC, 0xAD, 0xAE, 0xAF, 0xB1,
                                  0xB6, 0xB7, 0xBB, 0xBF, 0xC0, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xCB};

static const uint8_t boss_types[] = {87, 90, 92, 94, 95, 98, 99, 101, 102, 105, 109, 111, 112, 113, 116, 120, 123, 125};

static const uint8_t song_data[] = {0x40, 0x01, 0x31, 0x16, 0x8F};

static const struct ac_palsrc palettes[] = {{NULL, 45}};   /* vanilla manafort palette */
#define NUM_PALETTES ((int)(sizeof palettes / sizeof palettes[0]))

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

/* the header every map of this tileset gets, up to (not including) the palette */
static void set_header_common(uint8_t *h) {
  ac_hdr_set_tileset16(h, 21);
  ac_hdr_set_tileset8(h, 21);
  ac_hdr_set_animated_tiles(h, 1);
  ac_hdr_set_display(h, 0);
  ac_hdr_set_layer1_fg(h, 0);
  ac_hdr_set_layer2_fg(h, 1);
  ac_hdr_set_npc_palette(h, 0xFF);
  ac_hdr_set_flammie(h, 1);
  ac_hdr_set_magic_rope(h, 1);
}

static void choose_palette(struct sr_ctx *c, struct ac_map *m) {
  ac_apply_palette_source(c, m, &palettes[AC_RND(c, NUM_PALETTES)]);
}

static int node_dist(int d) { return d > OUT_W / 2 ? OUT_W - d : d; }

static void make_outdoor_enemies(struct sr_ctx *c, struct ac_map *m, const struct ac_node *start, const struct ac_layer *l1) {
  static const uint8_t enemy_ids[] = {0x00, 0x00, 0x00, 0x00, 68, 69, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83};
  for (int i = 0; i < 40; i++) {
    int ex = AC_RND(c, 128), ey = AC_RND(c, 128);
    int dx = node_dist(ex > start->center_x ? ex - start->center_x : start->center_x - ex);
    int dy = node_dist(ey > start->center_y ? ey - start->center_y : start->center_y - ey);
    for (;;) {
      int valid = 0;
      uint8_t t = AC_AT(c, l1, ex, ey);
      for (size_t k = 0; k < sizeof enemy_valid_tiles; k++)
        if (enemy_valid_tiles[k] == t) valid = 1;
      if (valid && (dx > 16 || dy > 16)) break;
      ex = AC_RND(c, 128);
      ey = AC_RND(c, 128);
      dx = node_dist(ex > start->center_x ? ex - start->center_x : start->center_x - ex);
      dy = node_dist(ey > start->center_y ? ey - start->center_y : start->center_y - ey);
    }
    int type = enemy_ids[AC_RND(c, (int)sizeof enemy_ids)];
    uint8_t *o = ac_map_add_object(c, m->md);
    ac_obj_set_xpos(o, (uint8_t)ex);
    ac_obj_set_ypos(o, (uint8_t)ey);
    ac_obj_set_species(o, type);
    ac_obj_set_vis_flag(o, 0x00);
    ac_obj_set_vis_min(o, 0x00);
    ac_obj_set_vis_max(o, 0x0F);
    ac_obj_set_event(o, 0x01);
    ac_obj_set_direction(o, DIR_LEFT);
    ac_obj_set_unknown_b7(o, 0x18);
  }
}

static int has(const struct ac_node *n, const char *k) { return ac_dict_has(&n->values, AC_STRKEY(k)); }

static struct ac_map *outdoor(struct sr_ctx *c, struct ac_gen_ctx *g) {
  struct ac_map *out = ac_map_new(c);
  struct ac_layer *l1 = ac_layer_new(c, OUT_W, OUT_H), *l2 = ac_layer_new(c, OUT_W, OUT_H);
  out->layer1 = l1;
  out->layer2 = l2;
  memset(l1->d, 95, (size_t)OUT_W * OUT_H);

  /* roomMakerWeights in insertion order: crystal, bridge, diagonal, circle line, whip (0), multi diagonal; 10 each */
  static const int maker_order[5] = {SR_P3_MF_CRYSTAL, SR_P3_MF_BRIDGE, SR_P3_MF_DIAGONAL, SR_P3_MF_CIRCLELINE, SR_P3_MF_MULTIDIAGONAL};
  int all_makers[50], num_makers = 0;
  for (int k = 0; k < 5; k++)
    for (int i = 0; i < 10; i++) all_makers[num_makers++] = maker_order[k];

  struct ac_node *start = ac_node_new(c);
  int corner = AC_RND(c, 4);
  int src_x = (corner % 2) * (ROOMS_X - 1), src_y = (corner / 2) * (ROOMS_Y - 1);
  start->x = 4 + src_x * 20;
  start->y = 4 + src_y * 20;
  start->center_x = start->x + 10;
  start->center_y = start->y + 10;
  out->entry_pos = ac_xy_new(c, start->center_x, start->center_y);

  struct ac_node *grid[ROOMS_Y][ROOMS_X];   /* nodes["xy"] */
  struct ac_node *order[ROOMS_X * ROOMS_Y]; /* Dictionary insertion order */
  int norder = 0;
  int sx = (start->x - 4) / 20, sy = (start->y - 4) / 20;
  memset(grid, 0, sizeof grid);
  char *nm = (char *)sr_alloc(c, 3);
  nm[0] = (char)('0' + sx);
  nm[1] = (char)('0' + sy);
  start->name = nm;
  grid[sy][sx] = start;
  order[norder++] = start;

  int ex = sx == 0 ? ROOMS_X - 1 : 0, ey = sy == 0 ? ROOMS_Y - 1 : 0;
  for (int ny = 0; ny < ROOMS_Y; ny++)
    for (int nx = 0; nx < ROOMS_X; nx++)
      if (!grid[ny][nx]) {
        struct ac_node *n = ac_node_new(c);
        n->x = 4 + nx * 20;
        n->y = 4 + ny * 20;
        n->center_x = n->x + 10;
        n->center_y = n->y + 10;
        char *s = (char *)sr_alloc(c, 3);
        s[0] = (char)('0' + nx);
        s[1] = (char)('0' + ny);
        n->name = s;
        grid[ny][nx] = n;
        order[norder++] = n;
      }

  /* random spanning tree: random visited node, random unvisited neighbour */
  struct ac_list visited;
  memset(&visited, 0, sizeof visited);
  ac_list_add(c, &visited, AC_VAL(start));
  while (visited.n < ROOMS_X * ROOMS_Y) {
    struct ac_node *source = (struct ac_node *)AC_PTR(ac_list_get(c, &visited, AC_RND(c, visited.n)));
    struct ac_node *dest[4];
    int nd = 0;
    int sox = (source->x - 4) / 20, soy = (source->y - 4) / 20;
    if (sox != 0 && !ac_list_contains(&visited, AC_VAL(grid[soy][sox - 1]))) dest[nd++] = grid[soy][sox - 1];
    if (sox != ROOMS_X - 1 && !ac_list_contains(&visited, AC_VAL(grid[soy][sox + 1]))) dest[nd++] = grid[soy][sox + 1];
    if (soy != 0 && !ac_list_contains(&visited, AC_VAL(grid[soy - 1][sox]))) dest[nd++] = grid[soy - 1][sox];
    if (soy != ROOMS_Y - 1 && !ac_list_contains(&visited, AC_VAL(grid[soy + 1][sox]))) dest[nd++] = grid[soy + 1][sox];
    if (nd > 0) {
      struct ac_node *conn = dest[AC_RND(c, nd)];
      ac_list_add(c, &visited, AC_VAL(conn));
      int dx = (conn->x - 4) / 20, dy = (conn->y - 4) / 20;
      const char *dir = "", *other = "";
      if (dx == sox + 1) { dir = "right"; other = "left"; }
      if (dx == sox - 1) { dir = "left"; other = "right"; }
      if (dy == soy + 1) { dir = "bottom"; other = "top"; }
      if (dy == soy - 1) { dir = "top"; other = "bottom"; }
      ac_dict_set(c, &source->values, AC_STRKEY(dir), 1);
      ac_dict_set(c, &conn->values, AC_STRKEY(other), 1);
    }
  }

  /* some extra left/right doors to make it less tedious */
  int extra = 5, attempts = 0;
  while (extra > 0 && attempts < 100) {
    int x = AC_RND(c, ROOMS_X);
    int y = AC_RND(c, ROOMS_Y);
    struct ac_node *n = grid[y][x];
    int64_t v;
    if ((!ac_dict_try_get(&n->values, AC_STRKEY("left"), &v) || v == 0) && x != 0) {
      ac_dict_set(c, &n->values, AC_STRKEY("left"), 1);
      ac_dict_set(c, &grid[y][x - 1]->values, AC_STRKEY("right"), 1);
      extra--;
    }
    if ((!ac_dict_try_get(&n->values, AC_STRKEY("right"), &v) || v == 0) && x != ROOMS_X - 1) {
      ac_dict_set(c, &n->values, AC_STRKEY("right"), 1);
      ac_dict_set(c, &grid[y][x + 1]->values, AC_STRKEY("left"), 1);
      extra--;
    }
    attempts++;
  }

  /* spots where a door could lay: every room without a top connection */
  struct ac_list possible;
  memset(&possible, 0, sizeof possible);
  for (int i = 0; i < norder; i++)
    if (!has(order[i], "top")) ac_list_add(c, &possible, AC_VAL(order[i]));
  int num_doors = 0;
  while (num_doors < 8 && possible.n > 0) {
    int idx = AC_RND(c, possible.n);
    struct ac_node *n = (struct ac_node *)AC_PTR(ac_list_get(c, &possible, idx));
    ac_list_remove_at(c, &possible, idx);
    ac_dict_add(c, &n->values, AC_STRKEY("door"), 1);
    num_doors++;
  }

  int door_num = 0;
  for (int ny = 0; ny < ROOMS_Y; ny++)
    for (int nx = 0; nx < ROOMS_X; nx++) {
      struct ac_node *n = grid[ny][nx];
      int kind;
      if (ny == sy && nx == sx) {
        kind = SR_P3_MF_START;
        ac_dict_set(c, &g->special_nodes, AC_STRKEY("start"), AC_VAL(n));
      } else if (ny == ey && nx == ex) {
        kind = SR_P3_MF_END;
        ac_dict_set(c, &g->special_nodes, AC_STRKEY("end"), AC_VAL(n));
      } else {
        kind = all_makers[AC_RND(c, num_makers)];
      }
      uint8_t room[400];
      sr_p3_manafort_room(c, kind, room, has(n, "right"), has(n, "left"), has(n, "top"), has(n, "bottom"), has(n, "door"));
      int rx = 4 + nx * 20, ry = 4 + ny * 20;
      for (int y = 0; y < 20; y++)
        for (int x = 0; x < 20; x++) {
          uint8_t t = room[y * 20 + x];
          AC_AT(c, l1, rx + x, ry + y) = t;
          /* 177 and 181 are the door tiles of the rooms */
          if (t == 177 || t == 181) {
            if (t == 177) ac_dict_set(c, &out->alt_entry, door_num, AC_VAL(ac_xy_new(c, rx + x, ry + y + 1)));
            ac_dict_set(c, &out->alt_exit, AC_XYKEY(rx + x, ry + y), door_num);
          }
        }
      if (has(n, "door")) door_num++;
    }

  /* 54 is the warper tile of the end room; the last one found wins */
  struct ac_xy *exit_pos = NULL;
  for (int y = 0; y < OUT_H; y++)
    for (int x = 0; x < OUT_W; x++)
      if (LAYER(l1, x, y) == 54) exit_pos = ac_xy_new(c, x, y);
  int floor_number = w8_wget_int(c, "FloorNumber");
  if (!exit_pos) sr_fail(c, SOMRANDO_ERR_FAILED, "Couldn't find exit room for manafort map on floor %d", floor_number);
  ac_list_add(c, &out->exit_locations, AC_VAL(exit_pos));

  uint8_t *h = out->md->header;
  ac_hdr_set_walkon(h, 1);
  set_header_common(h);
  choose_palette(c, out);
  ac_hdr_set_palette_animated(h, 1);
  make_outdoor_enemies(c, out, start, l1);

  g->num_shared_doors = ac_dict_size(&out->alt_entry);
  g->has_num_shared_doors = 1;
  for (int i = 0; i < norder; i++) ac_list_add(c, &g->shared_nodes, AC_VAL(order[i]));
  return out;
}

/* ManafortIntNpcRooms.populateIndoorMapData */
static void populate_indoor(struct sr_ctx *c, struct ac_map *m, int num_doors, struct ac_layer *l1) {
  size_t len = 0;
  const uint8_t *data = ac_custom_map(c, "acManafortNpcRoomL1.bin", &len);
  if (!data) sr_fail(c, SOMRANDO_ERR_FAILED, "resource acManafortNpcRoomL1.bin is missing");
  int door_num = 0;
  for (int y = 0; y < l1->h; y += 32)
    for (int x = 0; x < l1->w; x += 32)
      if (door_num < num_doors) {
        for (int yy = 0; yy < 32; yy++)
          for (int xx = 0; xx < 32; xx++) AC_AT(c, l1, x + xx, y + yy) = 0x0F;
        for (int yy = 0; yy < 26; yy++)
          for (int xx = 0; xx < 24; xx++) {
            if ((size_t)(yy * 24 + xx) >= len) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was outside the bounds of the array.");
            uint8_t t = data[yy * 24 + xx];
            AC_AT(c, l1, x + xx + 4, y + yy + 3) = t;
            /* 0x3C is a return door */
            if (t == 0x3C) {
              ac_dict_set(c, &m->alt_entry, door_num, AC_VAL(ac_xy_new(c, x + xx + 4, y + yy + 2)));
              ac_list_add(c, &m->return_doors, door_num);
            }
          }
        door_num++;
      }
}

static struct ac_map *indoor(struct sr_ctx *c, struct ac_gen_ctx *g) {
  struct ac_map *in = ac_map_new(c);
  struct ac_layer *l1 = ac_layer_new(c, IN_W, IN_H), *l2 = ac_layer_new(c, IN_W, IN_H);
  in->layer1 = l1;
  in->layer2 = l2;
  if (!g->has_num_shared_doors) sr_fail(c, SOMRANDO_ERR_FAILED, "The given key was not present in the dictionary.");
  int num_doors = g->num_shared_doors;
  populate_indoor(c, in, num_doors, l1);

  int num_dialogue = w8_wget_int(c, "numDialogueEvents");
  for (int i = 0; i < num_doors; i++)
    for (int n = 0; n < 3; n++) {
      int npc_x = (i % 4) * 32 + 16;
      int npc_y = (i / 4) * 32 + 24 - 3 - n;
      int event_num = DIALOGUE_EVENT_START + AC_RND(c, num_dialogue);
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

  uint8_t *h = in->md->header;
  set_header_common(h);
  ac_hdr_set_palette_animated(h, 1);
  choose_palette(c, in);
  return in;
}

static struct ac_map *boss(struct sr_ctx *c, struct ac_gen_ctx *g) {
  (void)g;
  struct ac_layer *l1 = ac_layer_new(c, BOSS_W, BOSS_H), *l2 = ac_layer_new(c, BOSS_W, BOSS_H);
  size_t len = 0;
  const uint8_t *data = ac_custom_map(c, "acManafortBossArenaL1.bin", &len);
  if (!data || len < (size_t)(BOSS_W * BOSS_H)) sr_fail(c, SOMRANDO_ERR_FAILED, "resource acManafortBossArenaL1.bin is missing");
  memcpy(l1->d, data, (size_t)BOSS_W * BOSS_H);

  struct ac_map *m = ac_map_new(c);
  m->layer1 = l1;
  m->layer2 = l2;
  m->entry_pos = ac_xy_new(c, 16, 20);

  int boss_id = boss_types[AC_RND(c, (int)sizeof boss_types)];
  uint8_t *o = ac_map_add_object(c, m->md);
  ac_obj_set_species(o, boss_id);
  ac_obj_set_xpos(o, 16);
  ac_obj_set_ypos(o, 14);
  ac_obj_set_vis_flag(o, 0x00);
  ac_obj_set_vis_min(o, 0x00);
  ac_obj_set_vis_max(o, 0x0F);

  uint8_t *h = m->md->header;
  ac_hdr_set_walkon(h, 0);
  set_header_common(h);
  ac_hdr_set_palette_animated(h, 1);
  choose_palette(c, m);
  return m;
}

static const uint8_t *song(struct sr_ctx *c, int *len) {
  (void)c;
  *len = (int)sizeof song_data;
  return song_data;
}

const struct ac_biome sr_p3_manafort_biome = {"manafortinterior", "Manafort Interior", get_ctx, outdoor, indoor, boss, song};
