/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Chaos mode (group w9): floor layout and generated maps.  Derived from processing/chaos/{ChaosRandomizer,ChaosMapGenerator,
 * ChaosGenerationState,ChaosMapGenerationInfo}.cs and processing/common/mapgen/CreditsMap.cs (Author of every file: Moppleton).
 * Translated to C 2026-10-18.
 */
#include <stdio.h>
#include <string.h>

#include "sr_core.h"
#include "sr_p2_chaos_int.h"

#define E(v) sr_emit(c, (v))
#define SS(key) sr_settings_get(c->settings, (key))

#define MAX_FLOORS 128
#define MAX_LIST 256

/* ChaosMapGenerator constants */
static const uint8_t element_map_palettes[8] = {88, 89, 47, 91, 93, 92, 95, 97};
static const uint8_t element_sprites[8] = {0x91, 0x90, 0x93, 0x92, 0x94, 0x95, 0x96, 0x97};
static const uint8_t element_flags[8] = {0x11, 0x10, 0x13, 0x12, 0x14, 0x15, 0x16, 0x17};

#define MAPNUM_FIRESEED 349
#define MAPNUM_MANABEAST_ARENA 253
#define BOSSID_MANABEAST 127
#define BOSSID_DARKLICH 121

/* ChaosMapGenerationInfo; "has_x" is the C# nullable being non-null */
struct w9_info {
  const struct w9_vmap *map;
  int main_room_map_id;
  int next_floor_door_id;
  int has_walkon, walkon_event_id;
  int town_map;
  int neko_event_num;
  int has_next, door_index_to_next_floor;
  int has_last, door_index_to_last_floor;
  int has_seed_room, door_index_to_mana_seed_room;
  int has_element_room, door_index_to_element_room;
  int has_boss_id, boss_id;
  int has_boss_event, boss_event_id;
  int has_boss_death, boss_death_event_id;
  const struct w9_bmap *boss_map;
  int has_seed_index, seed_index, seed_event_index;
  int has_element_index, element_index, element_event_index;
};

struct w9_gen_state {
  int full_map_index;
  int door_index;
  struct w9_info *previous_floor_data;
};

/* ------------------------------------------------------------------------------------------------------------ helpers */
static uint8_t *door_slot(struct sr_ctx *c, int id) {
  return w8_door_entry(c, id)->d;
}

static struct sr_bytes *event_get(struct sr_ctx *c, int id) {
  for (int i = 0; i < c->n_events; i++)
    if (c->events[i].id == id) return c->events[i].data;
  sr_fail(c, SOMRANDO_ERR_FAILED, "The given key '%d' was not present in the dictionary.", id);
}

/* VanillaEventUtil.replaceEventData: first match only, scanning i < Count - pattern.Count */
static void replace_event_data(struct sr_bytes *e, const uint8_t *pat, int plen, const uint8_t *repl, int rlen) {
  int at = -1;
  for (int i = 0; i < (int)e->len - plen; i++) {
    int match = 1;
    for (int j = 0; j < plen; j++)
      if (e->p[i + j] != pat[j]) match = 0;
    if (match) {
      at = i;
      break;
    }
  }
  if (at < 0) return;
  /* same length in every call site: a plain overwrite is the RemoveRange + InsertRange */
  (void)rlen;
  for (int j = 0; j < plen; j++) e->p[at + j] = repl[j];
}

static void inject_door(struct sr_ctx *c, int event_id, const uint8_t *pat, int door) {
  uint8_t repl[3] = {0x18, (uint8_t)door, (uint8_t)((door >> 8) + 8)};
  replace_event_data(event_get(c, event_id), pat, 3, repl, 3);
}

static const uint8_t BOSS_DOOR_PATTERN[3] = {0xEE, 0xEE, 0xEE};
static const uint8_t NEXT_FLOOR_PATTERN[3] = {0xFF, 0xFF, 0xFF};

/* ChaosMapGenerator.getEnterExitByte */
static int enter_exit_byte(int dir) {
  int b = 0;
  switch (dir) {
    case 0: b |= 5 << 5; break;
    case 1: b |= 7 << 5; break;
    case 3: b |= 6 << 5; break;
    default: break;
  }
  return (uint8_t)b;
}

static void door_set_x(uint8_t *d, int x) { w8_vs_set(d, 1, 0xFE, (uint8_t)x); }
static void door_set_y(uint8_t *d, int y) { w8_vs_set(d, 2, 0xFE, (uint8_t)y); }
static void door_set_l2(uint8_t *d, int v) { w8_vs_set(d, 2, 0x01, v ? 1 : 0); }
static void door_set_transition(uint8_t *d, int t) { w8_vs_set(d, 3, 0xFF, (uint8_t)t); }

static void obj_never_visible(uint8_t *o) {
  w8_obj_set_vis_flag(o, 0x00);
  w8_obj_set_vis_min(o, 0x0F);
  w8_obj_set_vis_max(o, 0x0F);
}
static void obj_always_visible(uint8_t *o) {
  w8_obj_set_vis_flag(o, 0x00);
  w8_obj_set_vis_min(o, 0x00);
  w8_obj_set_vis_max(o, 0x0F);
}

/* VanillaMapUtil.getObjects, appended to m */
static void load_objects(struct sr_ctx *c, int map_num, struct w8_map *m) {
  int start = w9_object_offset(c->orig, map_num) + 8;
  int end = w9_object_offset(c->orig, map_num + 1);
  for (int i = start; i < end; i += 8) memcpy(w9_map_add_object(c, m), c->orig + i, 8);
}

static int vanilla_trigger_count(struct sr_ctx *c, int map_num) {
  int start = w9_trigger_offset(c->orig, map_num), end = w9_trigger_offset(c->orig, map_num + 1);
  int n = 0;
  for (int i = start; i < end; i += 2) n++;
  return n;
}

static int list_contains(const int *l, int n, int v) {
  for (int i = 0; i < n; i++)
    if (l[i] == v) return 1;
  return 0;
}



/* List<int>.Remove(value): first occurrence */
static void remove_value(int *l, int *n, int v) {
  for (int k = 0; k < *n; k++)
    if (l[k] == v) {
      for (int m = k; m < *n - 1; m++) l[m] = l[m + 1];
      (*n)--;
      return;
    }
}

static void set_trigger(struct sr_ctx *c, struct w8_map *m, int id, int value) {
  if (id < 0 || id >= m->n_triggers) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range (map trigger %d).", id);
  m->triggers[id] = (uint16_t)value;
}

static void add_trigger(struct sr_ctx *c, struct w8_map *m, int value) {
  if (m->n_triggers >= W8_MAX_TRIGGERS) sr_fail(c, SOMRANDO_ERR_FAILED, "too many triggers");
  m->triggers[m->n_triggers++] = (uint16_t)value;
}

/* ----------------------------------------------------------------------------------------------- ChaosMapGenerator.generateFloor */
static void generate_floor(struct sr_ctx *c, struct w9_info *mi, struct w9_gen_state *st) {
  const struct w9_vmap *map = mi->map;
  struct sr_random *r = &c->rf;
  struct w8_map *gm = NULL;
  int main_map_index = st->full_map_index++;
  gm = w9_map_new(c, main_map_index);
  memcpy(gm->header, c->orig + w9_object_offset(c->orig, map->map_id), 8);
  uint8_t *hdr = gm->header;

  /* transfer triggers */
  int num_triggers = vanilla_trigger_count(c, map->map_id);
  int trigger_offset = 0;
  if (w8_vs_get(hdr, 3, 0x04) > 0) {
    /* first one is a walk-on; ignore */
    num_triggers--;
    trigger_offset++;
  }
  if (map->remove_walkon_event) {
    if (!mi->has_walkon) w8_hdr_set_walkon(hdr, 0);
  } else {
    if (mi->has_walkon) w8_hdr_set_walkon(hdr, 1);
  }
  /* fill in placeholder triggers that we will populate later */
  {
    int base = w9_trigger_offset(c->orig, map->map_id);
    for (int i = 0; i < num_triggers; i++) {
      int o = base + (i + trigger_offset) * 2;
      add_trigger(c, gm, c->orig[o] | (c->orig[o + 1] << 8));
    }
  }

  /* door to next floor */
  const struct w9_door *next_exit = &map->doors[mi->door_index_to_next_floor];
  for (int k = 0; k < next_exit->n_trigger_ids; k++) set_trigger(c, gm, next_exit->trigger_ids[k], st->door_index + 0x800);

  /* make room for other maps on this floor, if applicable */
  int element_map_num = 0, mana_seed_map_num = 0, boss_map_num = 0;
  if (mi->has_element_room) element_map_num = st->full_map_index++;
  if (mi->has_seed_room) mana_seed_map_num = st->full_map_index++;
  if (mi->has_boss_event) boss_map_num = st->full_map_index++;
  int next_floor_map_num = st->full_map_index;

  /* the door itself - x/y are set when the next map is picked */
  uint8_t *next_floor_door = door_slot(c, st->door_index);
  w8_door_init(next_floor_door, (uint8_t)next_floor_map_num, 0, 0, 0);
  int next_floor_door_id = st->door_index;
  mi->next_floor_door_id = next_floor_door_id;
  st->door_index++;

  if (mi->has_boss_event) {
    /* in the next event, replace placeholder FFFFFF with the door to the next actual floor */
    int skip_to_next_floor_event = mi->boss_event_id + 1;
    inject_door(c, skip_to_next_floor_event, NEXT_FLOOR_PATTERN, next_floor_door_id);
    for (int k = 0; k < next_exit->n_trigger_ids; k++) set_trigger(c, gm, next_exit->trigger_ids[k], mi->boss_event_id);
  }

  /* door back to prev floor */
  if (mi->has_last) {
    struct w9_info *pi = st->previous_floor_data;
    const struct w9_vmap *pmap = pi->map;
    int back_exit = mi->door_index_to_last_floor;
    const struct w9_door *back = &map->doors[back_exit];
    for (int k = 0; k < back->n_trigger_ids; k++) set_trigger(c, gm, back->trigger_ids[k], st->door_index + 0x800);

    const struct w9_door *pdoor = &pmap->doors[pi->door_index_to_next_floor];
    uint8_t *prev_door = door_slot(c, st->door_index);
    w8_door_init(prev_door, pi->main_room_map_id, pdoor->warp_back_x, pdoor->warp_back_y, enter_exit_byte(pdoor->dir));
    st->door_index++;

    /* modify the previous floor's "next map" door to point to the right spot, and have the right collision */
    uint8_t *to_this = door_slot(c, pi->next_floor_door_id);
    door_set_l2(to_this, map->layer2_collision);
    door_set_x(to_this, back->warp_back_x);
    door_set_y(to_this, back->warp_back_y);
  }

  /* copy objects from original map */
  load_objects(c, map->map_id, gm);

  /* enable flammie drum */
  w8_hdr_set_flammie(hdr, 1);

  if (!strcmp(SS("chMapColors"), "reasonable")) {
    if ((sr_random_next(r) % 2) == 0) {
      if (map->n_alt_palette_sets > 0) w8_hdr_set_palette_set(hdr, map->alt_palette_sets[sr_random_next(r) % map->n_alt_palette_sets]);
    }
  } else if (!strcmp(SS("chMapColors"), "ridiculous")) {
    static const uint8_t pals[32] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
                                     120, 121, 122, 123, 124, 125, 126, 127};
    w8_hdr_set_palette_set(hdr, pals[sr_random_next(r) % 32]);
  }

  int enemy_ids[W8_MAX_OBJS], n_enemy = 0;
  for (int i = 0; i < gm->n_objs; i++) {
    uint8_t *o = gm->objs[i];
    if (list_contains(map->objects_to_remove, map->n_objects_to_remove, i) && map->n_objects_to_remove > 0) {
      obj_never_visible(o);
    } else {
      int object_type = w8_vs_get(o, 5, 0xFF);
      if (object_type >= 0x80 || object_type == 0x55) {
        /* npcs and chests - always invisible; also, for now, orbs */
        obj_never_visible(o);
      } else {
        /* enemies - always visible */
        obj_always_visible(o);
        enemy_ids[n_enemy++] = i;
      }
    }
  }

  if (mi->town_map && n_enemy >= 2) {
    int watts_event_num = w8_wget_int(c, "wattsEventNum");
    int neko_event_num = mi->neko_event_num;
    int watts_index = sr_random_next(r) % n_enemy;
    int watts_id = enemy_ids[watts_index];
    for (int k = watts_index; k < n_enemy - 1; k++) enemy_ids[k] = enemy_ids[k + 1];
    n_enemy--;
    int neko_index = sr_random_next(r) % n_enemy;
    int neko_id = enemy_ids[neko_index];
    for (int i = 0; i < gm->n_objs; i++) {
      uint8_t *o = gm->objs[i];
      if (i == watts_id) {
        obj_always_visible(o);
        w8_obj_set_species(o, 0x9C);   /* watts */
        w8_obj_set_event(o, watts_event_num);
        w8_obj_set_unknown_b7(o, 0x08);
      } else if (i == neko_id) {
        obj_always_visible(o);
        w8_obj_set_species(o, 0x99);   /* neko */
        w8_obj_set_event(o, neko_event_num);
        w8_obj_set_unknown_b7(o, 0x08);
      } else {
        obj_never_visible(o);
      }
    }
  }

  /* transfer map pieces */
  w9_get_map_piece_reference(c, map->map_id, gm);

  /* mana seed map */
  if (mi->has_seed_room) {
    struct w8_map *sm = w9_map_new(c, mana_seed_map_num);
    int seed_og_map = MAPNUM_FIRESEED;
    /* no triggers on the original map; it uses a special event instead to provide a door back out */
    memcpy(sm->header, c->orig + w9_object_offset(c->orig, seed_og_map), 8);
    w8_hdr_set_walkon(sm->header, 0);
    w8_hdr_set_flammie(sm->header, 1);
    w8_hdr_set_palette_set(sm->header, element_map_palettes[mi->seed_index]);

    /* door to seedmap: ChaosDoor(dir 2, warp back 14,29) */
    uint8_t *to_seed = door_slot(c, st->door_index);
    w8_door_init(to_seed, mana_seed_map_num, 14, 29, enter_exit_byte(2));
    int seed_exit_num = mi->door_index_to_mana_seed_room;
    const struct w9_door *seed_exit = &map->doors[seed_exit_num];
    for (int k = 0; k < seed_exit->n_trigger_ids; k++) set_trigger(c, gm, seed_exit->trigger_ids[k], st->door_index + 0x800);
    st->door_index++;

    /* door back to floor main map */
    uint8_t *from_seed = door_slot(c, st->door_index);
    w8_door_init(from_seed, main_map_index, seed_exit->warp_back_x, seed_exit->warp_back_y, enter_exit_byte(seed_exit->dir));
    /* set the special event trigger to be the door back to the main map */
    add_trigger(c, sm, st->door_index + 0x800);
    st->door_index++;

    /* transfer map objects: two mana seeds, for some reason */
    load_objects(c, seed_og_map, sm);
    for (int i = 0; i < sm->n_objs; i++) w8_obj_set_event(sm->objs[i], mi->seed_event_index);
    /* transfer pieces - don't modify */
    w9_get_map_piece_reference(c, seed_og_map, sm);
  }

  /* element map */
  if (mi->has_element_room) {
    struct w8_map *em = w9_map_new(c, element_map_num);
    int element_og_map = 420;
    /* no triggers on the original map; it uses return tiles to provide a door back out */
    memcpy(em->header, c->orig + w9_object_offset(c->orig, element_og_map), 8);
    w8_hdr_set_walkon(em->header, 0);
    w8_hdr_set_flammie(em->header, 1);
    w8_hdr_set_palette_set(em->header, element_map_palettes[mi->element_index]);

    /* door to element map: ChaosDoor(dir 2, warp back 15,25) */
    uint8_t *to_el = door_slot(c, st->door_index);
    w8_door_init(to_el, element_map_num, 15, 25, enter_exit_byte(2));
    const struct w9_door *el_exit = &map->doors[mi->door_index_to_element_room];
    for (int k = 0; k < el_exit->n_trigger_ids; k++) set_trigger(c, gm, el_exit->trigger_ids[k], st->door_index + 0x800);
    st->door_index++;

    /* obj 0 is the crystal orb, 1 and 2 are enemies, obj 3 becomes the element npc */
    load_objects(c, element_og_map, em);
    int ei = mi->element_index;
    obj_never_visible(em->objs[0]);   /* spell orb */
    w8_obj_set_vis_flag(em->objs[3], element_flags[ei]);
    w8_obj_set_vis_min(em->objs[3], 0);
    w8_obj_set_vis_max(em->objs[3], 0);
    w8_obj_set_species(em->objs[3], element_sprites[ei]);
    w8_obj_set_event(em->objs[3], mi->element_event_index);
    w8_vs_set(em->objs[3], 3, 0x80, 1);   /* frozen */

    w9_get_map_piece_reference(c, element_og_map, em);
    /* center the piece with the door outlet */
    if (em->n_bg < 2) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range (element map pieces).");
    em->bg[1].x = 13;
  }

  /* make boss map */
  if (mi->has_boss_event) {
    const struct w9_bmap *bm = mi->boss_map;
    uint8_t *to_boss = door_slot(c, st->door_index);
    w8_door_init(to_boss, boss_map_num, bm->player_x, bm->player_y, enter_exit_byte(4));
    int boss_door_num = st->door_index;
    st->door_index++;
    int death_event = mi->boss_death_event_id;

    int boss_og_map_num;
    if (mi->boss_id == BOSSID_MANABEAST) {
      boss_og_map_num = MAPNUM_MANABEAST_ARENA;
      door_set_transition(to_boss, 0x81);
    } else {
      const struct sr_w8_boss_map *vm = NULL;
      for (int k = 0; k < W8_NUM_BOSS_ENTRIES; k++)
        if (sr_w8_boss_maps[k].id == mi->boss_id) vm = &sr_w8_boss_maps[k];
      if (!vm) sr_fail(c, SOMRANDO_ERR_FAILED, "The given key '%d' was not present in the dictionary.", mi->boss_id);
      boss_og_map_num = vm->original_map_num;
    }
    struct w8_map *bmap = w9_map_new(c, boss_map_num);
    memcpy(bmap->header, c->orig + w9_object_offset(c->orig, boss_og_map_num), 8);
    load_objects(c, boss_og_map_num, bmap);
    for (int i = 0; i < bmap->n_objs; i++) {
      int hide = 0, show = 0;
      for (int k = 0; k < bm->n_hide; k++)
        if (bm->objs_to_hide[k] == i) hide = 1;
      for (int k = 0; k < bm->n_show; k++)
        if (bm->objs_to_show[k] == i) show = 1;
      if (hide) {
        obj_never_visible(bmap->objs[i]);
      } else if (show) {
        obj_always_visible(bmap->objs[i]);
        w8_obj_set_event(bmap->objs[i], death_event);
      }
    }
    /* pieces */
    w9_get_map_piece_reference(c, boss_og_map_num, bmap);
    /* triggers - set all to 0 */
    int trigger_num = vanilla_trigger_count(c, boss_og_map_num);
    if (w8_vs_get(bmap->header, 3, 0x04) > 0) trigger_num--;   /* not enabled for chaos boss maps */
    for (int i = 0; i < trigger_num; i++) add_trigger(c, bmap, 0);
    w8_hdr_set_flammie(bmap->header, 1);
    w8_hdr_set_walkon(bmap->header, 0);

    /* in the boss entry event, replace placeholder EEEEEE with the door to the boss room */
    inject_door(c, mi->boss_event_id, BOSS_DOOR_PATTERN, boss_door_num);
    if (mi->boss_id == BOSSID_DARKLICH) {
      /* lich music instead of boss music */
      static const uint8_t from[5] = {0x40, 0x01, 0x04, 0x04, 0xFF}, to[5] = {0x40, 0x01, 0x0C, 0x06, 0xFF};
      replace_event_data(event_get(c, mi->boss_event_id), from, 5, to, 5);
    }
  }

  /* add walkon event once we've set up all the real triggers, since it goes first and throws off all the indexing */
  if (mi->has_walkon) {
    if (gm->n_triggers >= W8_MAX_TRIGGERS) sr_fail(c, SOMRANDO_ERR_FAILED, "too many triggers");
    memmove(&gm->triggers[1], &gm->triggers[0], (size_t)gm->n_triggers * sizeof gm->triggers[0]);
    gm->triggers[0] = (uint16_t)mi->walkon_event_id;
    gm->n_triggers++;
  }
}

/* ----------------------------------------------------------------------------------------------------- CreditsMap */
static void make_credits_map(struct sr_ctx *c, int map_num) {
  const int vanilla = 0xB4;
  struct w8_map *m = w9_map_new(c, map_num);
  memcpy(m->header, c->orig + w9_object_offset(c->orig, vanilla), 8);
  for (int layer = 0; layer < 2; layer++) {
    int piece = layer ? 898 : 897;
    w9_copy_piece_encoded(c, piece);
    struct w8_ref *rf = w9_map_add_ref(c, m, layer);
    rf->vis_flag = 0x00;
    rf->vis_low = 0x00;
    rf->vis_high = 0x0F;
    rf->piece_index = piece;
  }
}

/* ----------------------------------------------------------------------------------------------------- ChaosRandomizer */
static int get_setting_value(struct sr_ctx *c, const char *key, const char *const *names, const int *vals) {
  const char *v = SS(key);
  for (int i = 0; i < 5; i++)
    if (!strcmp(v, names[i])) return vals[i];
  sr_fail(c, SOMRANDO_ERR_FAILED, "The given key '%s' was not present in the dictionary.", v);
}

int sr_w9_num_floors(struct sr_ctx *c) {
  static const char *const names[5] = {"vfew", "few", "avg", "many", "vmany"};
  static const int vals[5] = {20, 40, 60, 80, 100};
  return get_setting_value(c, "chFloors", names, vals);
}

int sr_w9_hack_chaos_randomizer(struct sr_ctx *c) {
  static const char *const names[5] = {"vfew", "few", "avg", "many", "vmany"};
  static const int boss_vals[5] = {1, 2, 3, 4, 5};
  struct sr_random *r = &c->rf;
  int door_num = 10;
  int new_map_num = 16;
  int floor_num = 2;
  int boss_floors[MAX_LIST], n_boss_floors = 0;
  char key[64];

  int shop_event_ids[16];
  for (int i = 0; i < 16; i++) {
    snprintf(key, sizeof key, "shopEventId%d", i);
    shop_event_ids[i] = w8_wget_int(c, key);
  }
  int boss_floor_events[32], n_boss_floor_events = 0;
  int num_floor_events = w8_wget_int(c, "bossEntryEventNum");
  for (int i = 0; i < num_floor_events; i++) {
    snprintf(key, sizeof key, "bossEntryEventId%d", i);
    boss_floor_events[n_boss_floor_events++] = w8_wget_int(c, key);
  }
  int boss_death_events[32], n_boss_death_events = 0;
  int num_death_events = w8_wget_int(c, "bossDeathEventNum");
  for (int i = 0; i < num_death_events; i++) {
    snprintf(key, sizeof key, "bossDeathEventId%d", i);
    boss_death_events[n_boss_death_events++] = w8_wget_int(c, key);
  }
  /* elementEvents[elementSprites[i]] = event of element i */
  int element_events[8];   /* index: sprite - 0x90 */
  for (int i = 0; i < 8; i++) {
    snprintf(key, sizeof key, "elementEventId%d", i);
    element_events[element_sprites[i] - 0x90] = w8_wget_int(c, key);
  }
  int initial_walkon_event = w8_wget_int(c, "initialWalkonEventNum");

  int boss_amount = get_setting_value(c, "chBosses", names, boss_vals);
  int num_floors = sr_w9_num_floors(c);

  int mana_beast_floor_event = w8_wget_int(c, "manaBeastWalkonEventNum");
  int mana_beast_death_event = w8_wget_int(c, "manaBeastDeathEventNum");
  boss_floor_events[n_boss_floor_events++] = mana_beast_floor_event;
  boss_death_events[n_boss_death_events++] = mana_beast_death_event;
  int seed_events[8];
  for (int i = 0; i < 8; i++) {
    snprintf(key, sizeof key, "seedEventId%d", i);
    seed_events[i] = w8_wget_int(c, key);
  }

  /* first door: 31,58 on map 16 */
  uint8_t *sd = door_slot(c, door_num);
  w8_door_init(sd, new_map_num, 31, 58, 0);
  door_num++;

  int floor_mapping[512];
  int floor_mapping_set[512];
  memset(floor_mapping_set, 0, sizeof floor_mapping_set);
  memset(floor_mapping, 0, sizeof floor_mapping);
  floor_mapping[new_map_num] = 0;
  floor_mapping_set[new_map_num] = 1;

  struct w9_gen_state state;
  state.full_map_index = new_map_num;
  state.door_index = door_num;
  state.previous_floor_data = NULL;

  struct w9_info *initial = (struct w9_info *)sr_alloc(c, sizeof *initial);
  initial->has_walkon = 1;
  initial->walkon_event_id = initial_walkon_event;
  initial->has_next = 1;
  initial->door_index_to_next_floor = 0;   /* only one way out of this map */
  initial->map = &w9_chaos_start_map;
  initial->main_room_map_id = state.full_map_index;
  initial->next_floor_door_id = state.door_index;

  generate_floor(c, initial, &state);
  state.previous_floor_data = initial;
  const struct w9_vmap *prev_map = &w9_chaos_start_map;

  int town_floors[MAX_LIST], n_town = 0;
  int element_floors[16], n_element_floors = 0;
  int elements[16], n_elements = 0;
  int elements_left[8], n_elements_left = 8;

  /* und gno syl sal lum sha lun dry */
  int element_weights[8] = {10, 1, 1, 1, 1, 1, 1, 2};
  if (!sr_settings_get_bool(c->settings, "chHealSpellsFirst")) {
    for (int i = 0; i < 8; i++) element_weights[i] = 1;
  }
  for (int i = 0; i < 8; i++) elements_left[i] = i;

  int mana_seed_floors[16], n_seed_floors = 0;
  int seeds[16], n_seeds = 0;
  int seeds_left[8], n_seeds_left = 8;
  for (int i = 0; i < 8; i++) seeds_left[i] = i;

  int num_bosses = (int)(boss_amount * 5 * num_floors / 100.0);
  if (num_bosses > 25) num_bosses = 25;
  int attempts = 0;
  int boss_frequency = 7 - boss_amount;
  int boss_start_multiplier = 1;
  if (sr_settings_get_bool(c->settings, "chEasyEarlyFloors")) boss_start_multiplier = 2;

  /* determine which floors have bosses */
  if (boss_amount > 1) {
    for (int i = 0; i < num_bosses; i++) {
      int floor = boss_frequency * boss_start_multiplier + (sr_random_next(r) % (num_floors - 2));
      int floor_avail = 1;
      for (int k = 0; k < n_boss_floors; k++) {
        int d = boss_floors[k] - floor;
        if (d < 0) d = -d;
        if (d < boss_frequency) {
          floor_avail = 0;
          break;
        }
      }
      attempts++;
      if (floor_avail) boss_floors[n_boss_floors++] = floor;
      else i--;
      if (attempts >= 1000) break;
    }
  }

  /* determine which floors are towns */
  {
    int town_map_num = floor_num;
    while (town_map_num < num_floors) {
      town_map_num += (sr_random_next(r) % 4) + 4;
      if (n_town >= MAX_LIST) sr_fail(c, SOMRANDO_ERR_FAILED, "too many town floors");
      town_floors[n_town++] = town_map_num;
    }
  }

  /* determine which floors have elements */
  attempts = 0;
  while (n_element_floors < 8 && attempts < 100) {
    /* don't stick em on first or last floor */
    int floor = 2 + sr_random_next(r) % (num_floors - 3);
    if (!list_contains(element_floors, n_element_floors, floor)) {
      element_floors[n_element_floors++] = floor;
      int weighted[64], n_weighted = 0;
      for (int k = 0; k < n_elements_left; k++)
        for (int i = 0; i < element_weights[elements_left[k]]; i++) weighted[n_weighted++] = elements_left[k];
      int id = weighted[sr_random_next(r) % n_weighted];
      elements[n_elements++] = id;
      for (int k = 0; k < n_elements_left; k++)
        if (elements_left[k] == id) {
          for (int m = k; m < n_elements_left - 1; m++) elements_left[m] = elements_left[m + 1];
          n_elements_left--;
          break;
        }
    }
    attempts++;
  }

  /* determine which floors have mana seeds */
  int seed_events_in_order[16], n_seed_events_in_order = 0;
  attempts = 0;
  while (n_seed_floors < 8 && attempts < 100) {
    int floor = 2 + sr_random_next(r) % (num_floors - 3);
    if (!list_contains(mana_seed_floors, n_seed_floors, floor)) {
      mana_seed_floors[n_seed_floors++] = floor;
      int id = sr_random_next(r) % n_seeds_left;
      seeds[n_seeds++] = seeds_left[id];
      seed_events_in_order[n_seed_events_in_order++] = seed_events[seeds_left[id]];
      for (int m = id; m < n_seeds_left - 1; m++) seeds_left[m] = seeds_left[m + 1];
      n_seeds_left--;
    }
    attempts++;
  }

  const struct w9_vmap *used_maps[MAX_FLOORS + 8];
  int n_used = 0;
  int element_id = 0, seed_id = 0, boss_id = 0;

  const struct w9_bmap *possible_bosses[64];
  int n_possible = 0;
  for (int i = 0; i < w9_chaos_boss_map_count; i++) possible_bosses[n_possible++] = &w9_chaos_boss_maps[i];

  /* generate all floors after the start floor */
  int n_infos = 0;
  while (floor_num < num_floors) {
    if (n_infos >= MAX_FLOORS) sr_fail(c, SOMRANDO_ERR_FAILED, "too many floors");
    struct w9_info *mi = (struct w9_info *)sr_alloc(c, sizeof *mi);
    n_infos++;

    /* number of exits needed on the chosen vanilla map */
    int exit_num = 2;
    int is_el_floor = list_contains(element_floors, n_element_floors, floor_num);
    int is_seed_floor = list_contains(mana_seed_floors, n_seed_floors, floor_num);
    if (is_el_floor) exit_num++;
    if (is_seed_floor) exit_num++;

    if (list_contains(boss_floors, n_boss_floors, floor_num) || floor_num == num_floors - 1) {
      if (floor_num == num_floors - 1) {
        mi->has_boss_event = 1;
        mi->boss_event_id = mana_beast_floor_event;
        mi->has_boss_death = 1;
        mi->boss_death_event_id = mana_beast_death_event;
        mi->boss_map = &w9_chaos_final_boss_map;
        mi->has_boss_id = 1;
        mi->boss_id = 0x7F;
      } else {
        mi->has_boss_event = 1;
        mi->boss_event_id = boss_floor_events[boss_id];
        mi->has_boss_death = 1;
        mi->boss_death_event_id = boss_death_events[boss_id];
        int boss_index = sr_random_next(r) % n_possible;
        const struct w9_bmap *bm = possible_bosses[boss_index];
        for (int k = boss_index; k < n_possible - 1; k++) possible_bosses[k] = possible_bosses[k + 1];
        n_possible--;
        if (n_possible == 0) {
          /* reset boss possibilities */
          for (int i = 0; i < w9_chaos_boss_map_count; i++) possible_bosses[n_possible++] = &w9_chaos_boss_maps[i];
        }
        mi->boss_map = bm;
        mi->has_boss_id = 1;
        mi->boss_id = bm->boss_id;
        boss_id++;
      }
    }

    /* pick a base map for this floor, based on the number of exits */
    const struct w9_vmap *eligible[160];
    int n_eligible = 0;
    int town_floor = list_contains(town_floors, n_town, floor_num);
    for (int i = 0; i < w9_chaos_map_count; i++) {
      if (w9_chaos_maps[i].n_doors != exit_num) continue;
      if (town_floor && !w9_chaos_maps[i].town_map) continue;
      eligible[n_eligible++] = &w9_chaos_maps[i];
    }
    if (n_eligible == 0) sr_fail(c, SOMRANDO_ERR_FAILED, "The given key '%d' was not present in the dictionary.", exit_num);
    const struct w9_vmap *map = eligible[sr_random_next(r) % n_eligible];
    {
      int used = 0;
      for (int k = 0; k < n_used; k++)
        if (used_maps[k] == map) used = 1;
      if (used) map = eligible[sr_random_next(r) % n_eligible];   /* reroll once */
    }
    /* reroll forever to prevent the same one twice in a row (prevMap is never updated upstream) */
    while (map == prev_map) map = eligible[sr_random_next(r) % n_eligible];
    used_maps[n_used++] = map;

    int door_indexes[4], n_door_indexes = 0;
    for (int i = 0; i < exit_num; i++) door_indexes[n_door_indexes++] = i;

    /* pointing back to prev floor */
    int back_floor_exit_num = door_indexes[sr_random_next(r) % n_door_indexes];
    remove_value(door_indexes, &n_door_indexes, back_floor_exit_num);
    /* next floor (or boss) */
    int next_floor_exit_num = door_indexes[sr_random_next(r) % n_door_indexes];
    remove_value(door_indexes, &n_door_indexes, next_floor_exit_num);

    mi->has_next = 1;
    mi->door_index_to_next_floor = next_floor_exit_num;
    mi->has_last = 1;
    mi->door_index_to_last_floor = back_floor_exit_num;

    if (is_el_floor) {
      mi->has_element_room = 1;
      mi->door_index_to_element_room = door_indexes[sr_random_next(r) % n_door_indexes];
      remove_value(door_indexes, &n_door_indexes, mi->door_index_to_element_room);
      mi->has_element_index = 1;
      mi->element_index = elements[element_id];
      mi->element_event_index = element_events[element_id];   /* elementEvents[elementId + 0x90] */
      element_id++;
    }
    if (is_seed_floor) {
      mi->has_seed_room = 1;
      mi->door_index_to_mana_seed_room = door_indexes[sr_random_next(r) % n_door_indexes];
      remove_value(door_indexes, &n_door_indexes, mi->door_index_to_mana_seed_room);
      mi->has_seed_index = 1;
      mi->seed_index = seeds[seed_id];
      mi->seed_event_index = seed_events_in_order[seed_id];
      seed_id++;
    }

    /* walkon event: walkonEvents only has floor 1 */
    mi->town_map = town_floor;
    int shop_id = (floor_num * 16) / num_floors;
    if (shop_id >= 16) shop_id = 15;
    mi->neko_event_num = shop_event_ids[shop_id];

    mi->map = map;
    mi->main_room_map_id = state.full_map_index;
    mi->next_floor_door_id = state.door_index;
    int floor_first_map_index = state.full_map_index;

    generate_floor(c, mi, &state);

    int floor_last_map_index = state.full_map_index - 1;
    for (int i = floor_first_map_index; i <= floor_last_map_index; i++) {
      if (i < 0 || i >= 512) sr_fail(c, SOMRANDO_ERR_FAILED, "floor map index out of range");
      floor_mapping[i] = floor_num;
      floor_mapping_set[i] = 1;
    }
    state.previous_floor_data = mi;
    floor_num++;
  }

  /* make the credits map */
  int credits_new_map_num = state.full_map_index++;
  make_credits_map(c, credits_new_map_num);
  {
    const struct w9_door *cd = &w9_chaos_credits_map.doors[0];
    int credits_door_id = state.door_index;
    uint8_t *d = door_slot(c, state.door_index);
    w8_door_init(d, credits_new_map_num, cd->warp_back_x, cd->warp_back_y, enter_exit_byte(cd->dir));
    state.door_index++;
    /* inject credits door into mana beast death event */
    inject_door(c, mana_beast_death_event, NEXT_FLOOR_PATTERN, credits_door_id);
  }

  int floor_table_location = c->working_offset;
  sr_work_set_int(c, "floorTableLocation", floor_table_location);
  for (int i = 0; i < 512; i++) E(floor_mapping_set[i] ? (uint8_t)floor_mapping[i] : 0);

  int drop_table_location = c->working_offset;
  sr_work_set_int(c, "dropTableLocation", drop_table_location);
  for (int i = 0; i < 512; i++) {
    if (floor_mapping_set[i]) {
      int num = (int)((floor_mapping[i] * 32.0) / num_floors);
      if (num >= 32) num = 31;
      E((uint8_t)num);
    } else {
      E(0);
    }
  }
  return 1;
}
