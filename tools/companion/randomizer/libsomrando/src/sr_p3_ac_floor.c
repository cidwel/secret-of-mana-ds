/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Ancient Cave: AncientCaveMapGenerator.generate() and makeBossMap(), the part shared by the five floor generators.  Derived from
 * processing/ancientcave/mapgen/AncientCaveMapGenerator.cs (Author: Moppleton).  Translated to C 2026-10-18.
 */
#include <stdio.h>
#include <string.h>

#include "sr_p3_ac.h"
#include "sr_p3_ac_floor.h"
#include "sr_p2_bossrush_data.h"

static void check_not_null(struct sr_ctx *c, const void *p, const char *what, int floor, const char *type) {
  if (!p) sr_fail(c, SOMRANDO_ERR_FAILED, "%s for floor %d with floor type %s", what, floor, type);
}

static void check_map(struct sr_ctx *c, const struct ac_map *m, const char *name, int floor, const char *type, int need_entry, int need_exits) {
  char buf[96];
  check_not_null(c, m, (snprintf(buf, sizeof buf, "%s map not defined", name), buf), floor, type);
  if (need_entry) check_not_null(c, m->entry_pos, (snprintf(buf, sizeof buf, "%s map entry position not defined", name), buf), floor, type);
  check_not_null(c, m->layer1, (snprintf(buf, sizeof buf, "%s map layer 1 data not defined", name), buf), floor, type);
  check_not_null(c, m->layer2, (snprintf(buf, sizeof buf, "%s map layer 2 data not defined", name), buf), floor, type);
  if (need_exits && m->exit_locations.n == 0) sr_fail(c, SOMRANDO_ERR_FAILED, "%s map exit position not defined for floor %d with floor type %s", name, floor, type);
  check_not_null(c, m->md, (snprintf(buf, sizeof buf, "%s map misc data block not defined", name), buf), floor, type);
}

static struct ac_map *make_boss_map(struct sr_ctx *c, const struct ac_biome *b, struct ac_gen_ctx *g) {
  if ((sr_random_next(&c->rf) % 2) == 0) return b->boss(c, g);   /* floor-themed boss map */
  /* vanilla boss map; no triple tonpole (the dictionary keys in insertion order, minus 0x80) */
  int ids[W8_NUM_BOSS_ENTRIES], n = 0;
  for (int i = 0; i < W8_NUM_BOSS_ENTRIES; i++)
    if (sr_w8_boss_maps[i].id != 0x80) ids[n++] = sr_w8_boss_maps[i].id;
  int boss_id = ids[AC_RND(c, n)];
  struct ac_map *m = ac_import_vanilla_boss_arena(c, boss_id);
  ac_hdr_set_walkon(m->md->header, 0);   /* clear any walkon flag from vanilla */
  return m;
}

static void piece_ref(struct sr_ctx *c, struct w8_map *m, int piece_index, int is_fg) {
  struct w8_ref *r = (struct w8_ref *)0;
  if (is_fg) {
    if (m->n_fg >= W8_MAX_REFS) sr_fail(c, SOMRANDO_ERR_FAILED, "too many map pieces");
    r = &m->fg[m->n_fg++];
  } else {
    if (m->n_bg >= W8_MAX_REFS) sr_fail(c, SOMRANDO_ERR_FAILED, "too many map pieces");
    r = &m->bg[m->n_bg++];
  }
  memset(r, 0, sizeof *r);
  r->is_fg = is_fg;
  r->piece_index = piece_index;
  r->vis_high = 0x0F;
}

static void set_piece(struct sr_ctx *c, int index, const struct ac_layer *l) {
  struct sr_bytes enc = {0};
  w8_encode_layer(c, l->d, l->w, l->h, &enc);
  w8_set_piece(c, index, enc.p, enc.len);
}

void sr_p3_generate_floor(struct sr_ctx *c, const struct ac_biome *b, struct ac_floor *out) {
  int floor = w8_wget_int(c, "FloorNumber");
  int pal_start = w8_wget_int(c, "GeneratedPaletteIndex");
  (void)pal_start;   /* only the debug dump uses it */
  memset(out, 0, sizeof *out);
  ac_dump_line("floor %d type=%s", floor, b->floor_type);
  ac_dump_rng(c, "floor-start");
  struct ac_gen_ctx *g = b->get_ctx(c);
  ac_dump_rng(c, "after-genctx");
  out->outside = b->outdoor(c, g);
  /* the map ids are chosen so that the 8-bit map id divided by two is the floor number (Layer3Changes, EnemyChanges) */
  if (out->outside) {
    out->outside->map_id = (uint16_t)(floor * 2);
    snprintf(out->outside->map_name, sizeof out->outside->map_name, "Floor %d outside map", floor);
    ac_dump_map(c, "outside", out->outside);
  }
  out->inside = b->indoor(c, g);
  if (out->inside) {
    out->inside->map_id = (uint16_t)(floor * 2 + 1);
    snprintf(out->inside->map_name, sizeof out->inside->map_name, "Floor %d inside map", floor);
    ac_dump_map(c, "inside", out->inside);
  }
  out->boss = make_boss_map(c, b, g);
  out->boss->map_id = (uint16_t)(floor * 2 + 256);
  snprintf(out->boss->map_name, sizeof out->boss->map_name, "Floor %d boss map", floor);
  ac_dump_map(c, "boss", out->boss);
  out->song = b->song(c, &out->song_len);
  {
    char hex[64] = "";
    for (int i = 0; i < out->song_len && i < 20; i++) snprintf(hex + strlen(hex), sizeof hex - strlen(hex), "%s%02X", i ? "-" : "", out->song[i]);
    ac_dump_line("song %s", hex);
    ac_dump_rng(c, "song-rng");
  }

  check_map(c, out->outside, "Outside", floor, b->floor_type, 1, 1);
  check_map(c, out->inside, "Inside", floor, b->floor_type, 0, 0);
  check_map(c, out->boss, "Boss", floor, b->floor_type, 1, 0);

  w8_register_map(c, out->outside->map_id, out->outside->md);
  w8_register_map(c, out->inside->map_id, out->inside->md);
  w8_register_map(c, out->boss->map_id, out->boss->md);

  /* map pieces (floor) * 6: 2 for outdoor, 2 for indoor, 2 for boss */
  struct ac_map *maps[3] = {out->outside, out->inside, out->boss};
  for (int k = 0; k < 3; k++) {
    piece_ref(c, maps[k]->md, floor * 6 + k * 2, 0);
    piece_ref(c, maps[k]->md, floor * 6 + k * 2 + 1, 1);
    set_piece(c, floor * 6 + k * 2, maps[k]->layer1);
    set_piece(c, floor * 6 + k * 2 + 1, maps[k]->layer2);
  }
#ifdef SOMRANDO_DEBUG
  {
    struct w8_state *st = w8_state_get(c);
    char line[256] = "";
    for (int q = 0; q < 6; q++) {
      uint32_t h = 2166136261u;
      for (int i = 0; i < st->n_pieces; i++)
        if (st->pieces[i].id == floor * 6 + q)
          for (size_t j = 0; j < st->pieces[i].data.len; j++) { h ^= st->pieces[i].data.p[j]; h *= 16777619u; }
      snprintf(line + strlen(line), sizeof line - strlen(line), "%08x;", h);
    }
    ac_dump_line("pieces %s pal=%d", line, w8_wget_int(c, "GeneratedPaletteIndex"));
    ac_dump_line("palhash");
    for (int i = 0; i < st->n_pals; i++) {
      if (st->pals[i].id < pal_start) continue;
      uint32_t h = 2166136261u;
      for (int j = 0; j < 210; j++) { h ^= st->pals[i].d[j]; h *= 16777619u; }
      ac_dump_line("palset %d %08x", st->pals[i].id, h);
    }
  }
#endif
}
