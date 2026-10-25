/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Chaos mode (group w9): declarations shared by the sr_p2_chaos*.c files.
 */
#ifndef SR_P2_CHAOS_INT_H
#define SR_P2_CHAOS_INT_H
#include "sr_core.h"
#include "sr_p2_bossrush_int.h"
#include "sr_p2_bossrush_data.h"

/* ChaosDoor */
struct w9_door {
  const int *trigger_ids;
  int n_trigger_ids;
  int dir;   /* 0 up 1 right 2 down 3 left 4 any */
  int warp_back_x, warp_back_y;
};

/* ChaosVanillaMap */
struct w9_vmap {
  int map_id;
  const struct w9_door *doors;
  int n_doors;
  int layer2_collision;
  const uint8_t *alt_palette_sets;
  int n_alt_palette_sets;
  int remove_walkon_event;
  const int *objects_to_remove;
  int n_objects_to_remove;
  int town_map;
  const int *triggers_to_remove;
  int n_triggers_to_remove;
};

/* ChaosBossMap, with the boss id it is registered under */
struct w9_bmap {
  int boss_id;
  int map_id, player_x, player_y, layer2_collision;
  const uint8_t *objs_to_show;
  int n_show;
  const uint8_t *objs_to_hide;
  int n_hide;
};

extern const struct w9_vmap w9_chaos_maps[];
extern const int w9_chaos_map_count;
extern const struct w9_vmap w9_chaos_start_map, w9_chaos_credits_map;
extern const struct w9_bmap w9_chaos_boss_maps[];
extern const int w9_chaos_boss_map_count;
extern const struct w9_bmap w9_chaos_final_boss_map;
extern const struct sr_w8_diff_row w9_chaos_diff_rows[];
extern const int w9_chaos_diff_row_count;

/* exports of sr_p2_bossrush_map.c (w9 block at the end of that file) */
struct w8_map *w9_map_new(struct sr_ctx *c, int id);
uint8_t *w9_map_add_object(struct sr_ctx *c, struct w8_map *m);
struct w8_ref *w9_map_add_ref(struct sr_ctx *c, struct w8_map *m, int fg);
int w9_object_offset(const uint8_t *rom, int map_num);
int w9_trigger_offset(const uint8_t *rom, int map_num);
void w9_get_map_piece_reference(struct sr_ctx *c, int map_num, struct w8_map *out);
void w9_copy_piece_encoded(struct sr_ctx *c, int piece_index);

/* w8 helpers that are not declared in sr_p2_bossrush_int.h */
int w8_vs_get(const uint8_t *v, int byte_index, int bits);
void w8_vs_set(uint8_t *v, int byte_index, int bits, int value);
void w8_hdr_set_palette_set(uint8_t *h, int v);
int w8_hdr_get_palette_set(const uint8_t *h);
void w8_hdr_set_walkon(uint8_t *h, int v);
void w8_hdr_set_flammie(uint8_t *h, int v);
void w8_obj_set_species(uint8_t *o, int v);
void w8_obj_set_event(uint8_t *o, int v);
void w8_obj_set_vis_flag(uint8_t *o, int v);
void w8_obj_set_vis_min(uint8_t *o, int v);
void w8_obj_set_vis_max(uint8_t *o, int v);
void w8_obj_set_unknown_b7(uint8_t *o, int v);
void w8_door_init(uint8_t d[4], int map, int x, int y, int transition);

int sr_w9_num_floors(struct sr_ctx *c);

/* sr_p2_chaos_events.c */
int sr_w9_hack_chaos_events(struct sr_ctx *c);
/* sr_p2_chaos_map.c */
int sr_w9_hack_chaos_randomizer(struct sr_ctx *c);
/* sr_p2_chaos.c */
int sr_w9_hack_sell_all_gear(struct sr_ctx *c);
#endif
