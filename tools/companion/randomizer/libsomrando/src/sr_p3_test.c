/* libsomrando: test-only Ancient Cave floor generator "test" (debug builds, acBiomeTypes=test): the C twin of TestGenerator.cs in the instrumented
 * oracle.  Tiny maps that exercise the door pairing and the event plumbing before the real generators exist.  Original work of this repository
 * (LGPL-2.1+).
 */
#include "sr_p3_ac.h"

#ifdef SOMRANDO_DEBUG
static const struct ac_palsrc pal = {NULL, 45};

static struct ac_gen_ctx *get_ctx(struct sr_ctx *c) {
  struct ac_gen_ctx *g = ac_gen_ctx_new(c);
  g->outdoor_w = 40; g->outdoor_h = 40; g->indoor_w = 40; g->indoor_h = 24; g->boss_w = 32; g->boss_h = 32;
  return g;
}

static struct ac_map *blank(struct sr_ctx *c, int w, int h, int tileset) {
  struct ac_map *m = ac_map_new(c);
  m->layer1 = ac_layer_new(c, w, h);
  m->layer2 = ac_layer_new(c, w, h);
  ac_hdr_set_tileset8(m->md->header, tileset);
  ac_hdr_set_tileset16(m->md->header, tileset + 2);
  ac_hdr_set_animated_tiles(m->md->header, 1);
  ac_hdr_set_display(m->md->header, 1);
  return m;
}

static struct ac_map *outdoor(struct sr_ctx *c, struct ac_gen_ctx *g) {
  (void)g;
  struct ac_map *m = blank(c, 40, 40, 5);
  for (int y = 0; y < 40; y++)
    for (int x = 0; x < 40; x++) LAYER(m->layer1, x, y) = (uint8_t)(1 + AC_RND(c, 3));
  ac_hdr_set_walkon(m->md->header, 1);
  m->entry_pos = ac_xy_new(c, 5, 5);
  ac_list_add(c, &m->exit_locations, AC_VAL(ac_xy_new(c, 30, 30)));
  ac_list_add(c, &m->exit_locations, AC_VAL(ac_xy_new(c, 32, 30)));
  ac_dict_set(c, &m->alt_exit, AC_XYKEY(10, 10), 0);
  ac_dict_set(c, &m->alt_exit, AC_XYKEY(12, 10), 1);
  ac_dict_set(c, &m->alt_entry, 0, AC_VAL(ac_xy_new(c, 11, 11)));
  ac_dict_set(c, &m->alt_entry, 1, AC_VAL(ac_xy_new(c, 13, 11)));
  ac_dict_set(c, &m->same_exit, AC_XYKEY(20, 20), 7);
  ac_dict_set(c, &m->same_entry, 7, AC_VAL(ac_xy_new(c, 22, 22)));
  ac_apply_palette_source(c, m, &pal);
  return m;
}

static struct ac_map *indoor(struct sr_ctx *c, struct ac_gen_ctx *g) {
  (void)g;
  struct ac_map *m = blank(c, 40, 24, 6);
  for (int y = 0; y < 24; y++)
    for (int x = 0; x < 40; x++) LAYER(m->layer2, x, y) = (uint8_t)AC_RND(c, 2);
  ac_dict_set(c, &m->alt_entry, 0, AC_VAL(ac_xy_new(c, 4, 4)));
  ac_dict_set(c, &m->alt_entry, 1, AC_VAL(ac_xy_new(c, 6, 4)));
  ac_dict_set(c, &m->alt_exit, AC_XYKEY(8, 8), 0);
  ac_dict_set(c, &m->alt_exit, AC_XYKEY(9, 8), 1);
  for (int i = 0; i < 12; i++) {
    uint8_t *o = ac_map_add_object(c, m->md);
    ac_obj_set_species(o, 0x99);
    ac_obj_set_xpos(o, 2 + i);
    ac_obj_set_ypos(o, 3);
    ac_obj_set_always_visible(o);
  }
  ac_apply_palette_source(c, m, &pal);
  return m;
}

static struct ac_map *boss(struct sr_ctx *c, struct ac_gen_ctx *g) {
  (void)g;
  struct ac_map *m = blank(c, 32, 32, 7);
  for (int y = 0; y < 32; y++)
    for (int x = 0; x < 32; x++) LAYER(m->layer1, x, y) = 5;
  int bx = AC_RND(c, 32);
  int by = AC_RND(c, 32);
  LAYER(m->layer1, bx, by) = 9;
  m->entry_pos = ac_xy_new(c, 16, 20);
  uint8_t *o = ac_map_add_object(c, m->md);
  ac_obj_set_species(o, (uint8_t)(0x57 + AC_RND(c, 4)));
  ac_obj_set_xpos(o, 16);
  ac_obj_set_ypos(o, 10);
  ac_obj_set_always_visible(o);
  ac_apply_palette_source(c, m, &pal);
  return m;
}

static const uint8_t song_data[5] = {0x40, 0x01, 0x31, 0x16, 0x8F};
static const uint8_t *song(struct sr_ctx *c, int *len) { (void)c; *len = 5; return song_data; }

const struct ac_biome sr_p3_test_biome = {"test", "Test", get_ctx, outdoor, indoor, boss, song};
#else
const struct ac_biome sr_p3_test_biome = {"test", "Test", 0, 0, 0, 0, 0};
#endif
