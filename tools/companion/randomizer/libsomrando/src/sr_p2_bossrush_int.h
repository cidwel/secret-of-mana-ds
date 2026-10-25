/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Boss rush mode (group w8): declarations shared by the sr_p2_bossrush*.c files.
 */
#ifndef SR_P2_BOSSRUSH_INT_H
#define SR_P2_BOSSRUSH_INT_H
#include "sr_core.h"

/* workingData keys */
#define W8_KEY_DOOR_LOCATION "DoorLocation"
#define W8_KEY_START_DOOR_OFFSET "startDoorOffset"
#define W8_KEY_START_MUSIC_OFFSET "startMusicOffset"
#define W8_KEY_DIV10_SUBR_OFFSET "div10SubrOffset"
#define W8_KEY_ANIM_PAL_OFFSET "AnimatedPaletteOffset"

/* BossOrderRandomizer.allPossibleBosses.Count */
#define W8_NUM_BOSS_ENTRIES 37

/* full map model (FullMap, FullMapMapPieces) */
#define W8_MAX_OBJS 256
#define W8_MAX_REFS 96
#define W8_MAX_TRIGGERS 512

struct w8_ref {
  int index_on_map, is_fg, piece_index;
  int x, y;
  int vis_flag, vis_low, vis_high;
};

struct w8_map {
  int id;
  uint8_t header[8];
  uint8_t objs[W8_MAX_OBJS][8];
  int n_objs;
  struct w8_ref bg[W8_MAX_REFS], fg[W8_MAX_REFS];
  int n_bg, n_fg;
  uint16_t triggers[W8_MAX_TRIGGERS];
  int n_triggers;
};



#define W8_MAX_PIECES 320
#define W8_MAX_MAPS 192
#define W8_MAX_PALS 128
#define W8_MAX_CUSTOM_EVENTS 16

struct w8_piece_entry {
  int id;
  struct sr_bytes data;
};
struct w8_door_entry {
  int id;
  uint8_t d[4];
};
struct w8_pal_entry {
  int id;
  uint8_t d[210];
};

/* what RandoContext keeps for generated modes: replacementMapPieces, replacementDoors, generatedMaps, replacementMapPalettes and the
 * CustomEventManager; all in insertion order */
struct w8_state {
  struct w8_piece_entry pieces[W8_MAX_PIECES];
  int n_pieces;
  struct w8_door_entry *doors;   /* grows on demand: Ancient Cave on 24 floors of ruins makes ~1800 doors */
  int n_doors, cap_doors;
  struct w8_map *maps[W8_MAX_MAPS];
  int n_maps;
  struct w8_pal_entry pals[W8_MAX_PALS];
  int n_pals;
  char ev_names[W8_MAX_CUSTOM_EVENTS][32];
  int ev_offsets[W8_MAX_CUSTOM_EVENTS];
  int n_ev;
};

struct w8_state *w8_state_get(struct sr_ctx *c);
void w8_set_piece(struct sr_ctx *c, int id, const uint8_t *p, size_t n);
void w8_set_door(struct sr_ctx *c, int id, const uint8_t d[4]);
/* replacementDoors[id]: the entry of an existing id, or a new zeroed one at the end */
struct w8_door_entry *w8_door_entry(struct sr_ctx *c, int id);
void w8_set_pal(struct sr_ctx *c, int id, const uint8_t d[210]);
/* generatedMaps[id] = map (replaces in place when the id exists) */
void w8_register_map(struct sr_ctx *c, int id, struct w8_map *m);
void w8_encode_layer(struct sr_ctx *c, const uint8_t *d, int w, int h, struct sr_bytes *dest);
void w8_pieces_expand(struct sr_ctx *c);
void w8_doors_replace(struct sr_ctx *c);
void w8_maps_replace(struct sr_ctx *c);
void w8_pals_replace(struct sr_ctx *c);
int sr_w8_hack_bossrush_randomizer(struct sr_ctx *c);
int sr_w8_hack_boss_order(struct sr_ctx *c);
int sr_w8_hack_boss_rush_events(struct sr_ctx *c);
void w8_custom_event_process(struct sr_ctx *c);

/* workingData.getInt: Int32.Parse of a missing key is an error */
int w8_wget_int(struct sr_ctx *c, const char *key);

/* CustomEventManager.registerNewEvent / getCommandIndex */
int w8_register_event(struct sr_ctx *c, const char *name, int offset);
int w8_command_index(struct sr_ctx *c, const char *name);

/* difficulty table of the preset in the request, DifficultySettings keys */
double w8_diff(struct sr_ctx *c, const char *key);
int w8_diff_int(struct sr_ctx *c, const char *key);

/* chaos hooks (sr_p2_chaos.c): mode-dependent constants of the shared non-vanilla hacks */
int w9_is_chaos(struct sr_ctx *c);
/* Ancient Cave hooks (sr_p3_ac_mode.c) */
int sr_p3_is_ac(struct sr_ctx *c);
int sr_p3_ac_num_floors(const struct sr_settings *s);   /* AncientCaveGenerator.LENGTH_CONVERSIONS[acLength] */
/* the shared small hacks of the non-vanilla modes (sr_p2_bossrush.c) */
int sr_w8_hack_starting_door(struct sr_ctx *c);
int sr_w8_hack_death_event_doorway_removal(struct sr_ctx *c);
int sr_w8_hack_mag_def_adjustment(struct sr_ctx *c);
int sr_w8_hack_enemy_black_magic_power(struct sr_ctx *c);
/* ManaBeastMap.makeManaBeastMap / CreditsMap.makeCreditsMap (sr_p2_bossrush_map.c) */
void w8_make_mana_beast_map(struct sr_ctx *c);
void w8_make_credits_map(struct sr_ctx *c);
int w9_num_floors(struct sr_ctx *c, int bossrush_value);

/* gen file */
int sr_w8_hack_door_expansion(struct sr_ctx *c);
int sr_w8_hack_animated_palette(struct sr_ctx *c);
int sr_w8_hack_layer3_changes(struct sr_ctx *c);
int sr_w8_hack_timer_dialogue(struct sr_ctx *c);
int sr_w8_hack_statistics(struct sr_ctx *c);
int sr_w8_hack_instant_weapon_levels(struct sr_ctx *c);
int sr_w8_hack_mp_absorb_limiter(struct sr_ctx *c);
int sr_w8_hack_starting_level(struct sr_ctx *c);
int sr_w8_hack_magic_rope(struct sr_ctx *c);
void sr_w8_copy_palette_animation_settings(struct sr_ctx *c, int og_palette_set, int new_palette_set);
int sr_w8_hack_enemy_changes(struct sr_ctx *c);
int sr_w8_hack_drop_changes(struct sr_ctx *c);
int sr_w8_hack_shop_changes(struct sr_ctx *c);
#endif
