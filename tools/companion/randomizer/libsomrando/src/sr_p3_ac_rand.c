/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Ancient Cave: AncientCaveRandomizer.process() (floor generation, event and NPC wiring, door pairing, mana beast and credits maps).
 * Derived from processing/ancientcave/AncientCaveRandomizer.cs (Author: Moppleton).  Translated to C 2026-10-18.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sr_p3_ac.h"
#include "sr_p3_ac_floor.h"
#include "sr_p3_ac_mode.h"
#include "sr_p2_chaos_int.h"

/* an event pattern to be replaced by a 5-byte music play pattern; if not replaced, this just sets and unsets event flag FB */
static const uint8_t MUSIC_REPLACEMENT_PATTERN[5] = {0x29, 0xFB, 0x30, 0xFB, 0x00};
static const uint8_t UNKNOWN_DOOR_REPLACEMENT_PATTERN[3] = {0xFF, 0xFF, 0xFF};

/* ------------------------------------------------------------------------------------------------------------- Door */
struct door {
  int id;
  uint8_t d[4];
};

struct doors {
  struct door **p;
  int n, cap;
};

static struct door *door_new(struct sr_ctx *c, struct doors *ds, int id) {
  if (ds->n == ds->cap) {
    int nc = ds->cap ? ds->cap * 2 : 64;
    struct door **np = (struct door **)sr_alloc(c, (size_t)nc * sizeof *np);
    if (ds->n) memcpy(np, ds->p, (size_t)ds->n * sizeof *np);
    ds->p = np;
    ds->cap = nc;
  }
  struct door *d = (struct door *)sr_alloc(c, sizeof *d);
  d->id = id;
  ds->p[ds->n++] = d;   /* context.replacementDoors[id] = door (ids are published in this order, the last write to an id wins) */
  return d;
}

/* Door setters = VanillaStructure.set: the value is not masked to the field */
static void door_set_target_map(struct door *d, int map) {
  w8_vs_set(d->d, 0, 0xFF, (uint8_t)map);
  w8_vs_set(d->d, 1, 0x01, (uint8_t)(map >> 8));
}
static void door_set_xpos(struct door *d, int x) { w8_vs_set(d->d, 1, 0xFE, (uint8_t)x); }
static void door_set_ypos(struct door *d, int y) { w8_vs_set(d->d, 2, 0xFE, (uint8_t)y); }
static void door_set_layer2(struct door *d, int v) { w8_vs_set(d->d, 2, 0x01, v ? 1 : 0); }
static void door_set_transition(struct door *d, int t) { w8_vs_set(d->d, 3, 0xFF, (uint8_t)t); }

/* ------------------------------------------------------------------------------- SortedDictionary<XyPos, int> by (y, x) */
struct trig_entry {
  int x, y, val;
};
struct trig_map {
  struct trig_entry *e;
  int n, cap;
};

static void trig_set(struct sr_ctx *c, struct trig_map *m, int x, int y, int val) {
  int lo = 0;
  while (lo < m->n && (m->e[lo].y < y || (m->e[lo].y == y && m->e[lo].x < x))) lo++;
  if (lo < m->n && m->e[lo].y == y && m->e[lo].x == x) {
    m->e[lo].val = val;
    return;
  }
  if (m->n == m->cap) {
    int nc = m->cap ? m->cap * 2 : 32;
    struct trig_entry *ne = (struct trig_entry *)sr_alloc(c, (size_t)nc * sizeof *ne);
    if (m->n) memcpy(ne, m->e, (size_t)m->n * sizeof *ne);
    m->e = ne;
    m->cap = nc;
  }
  memmove(&m->e[lo + 1], &m->e[lo], (size_t)(m->n - lo) * sizeof *m->e);
  m->e[lo].x = x;
  m->e[lo].y = y;
  m->e[lo].val = val;
  m->n++;
}

/* ------------------------------------------------------------------------------------------------------------ events */
static struct sr_bytes *event_get(struct sr_ctx *c, int id) {
  for (int i = 0; i < c->n_events; i++)
    if (c->events[i].id == id) return c->events[i].data;
  sr_fail(c, SOMRANDO_ERR_FAILED, "The given key '%d' was not present in the dictionary.", id);
}

/* VanillaEventUtil.replaceEventData: first match only, scanning i < Count - pattern.Count; the replacement may have another length */
static void replace_event_data(struct sr_ctx *c, struct sr_bytes *e, const uint8_t *pat, int plen, const uint8_t *repl, int rlen) {
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
  if (rlen == plen) {
    memcpy(e->p + at, repl, (size_t)rlen);
    return;
  }
  size_t tail = e->len - (size_t)at - (size_t)plen;
  size_t new_len = e->len - (size_t)plen + (size_t)rlen;
  sr_bytes_reserve(c, e, (size_t)rlen);
  memmove(e->p + at + rlen, e->p + at + plen, tail);
  memcpy(e->p + at, repl, (size_t)rlen);
  e->len = new_len;
}

/* ---------------------------------------------------------------------------------------------------------- pairDoors */
/* pair one map's door exit locations to another's entry locations by ids specified in ancient cave floor processing, and create triggers
 * for each */
static void pair_doors(struct sr_ctx *c, struct doors *ds, struct trig_map *triggers, struct ac_map *src, struct ac_map *dst,
                       int *generated_door_id, int target_map_id) {
  const struct ac_dict *entries, *exits;
  if (src == dst) {
    entries = &src->same_entry;
    exits = &src->same_exit;
  } else {
    entries = &dst->alt_entry;
    exits = &src->alt_exit;
  }
  for (int i = 0; i < entries->count; i++) {
    if (!entries->e[i].used) continue;
    int dest_id = (int)entries->e[i].key;
    const struct ac_xy *dest_pos = (const struct ac_xy *)AC_PTR(ac_dict_get(c, entries, dest_id));
    int found = 0;
    for (int k = 0; k < exits->count; k++) {
      if (!exits->e[k].used) continue;
      if ((int)exits->e[k].val != dest_id) continue;
      struct door *door = door_new(c, ds, *generated_door_id);
      door_set_xpos(door, dest_pos->x);
      door_set_ypos(door, dest_pos->y);
      door_set_target_map(door, target_map_id);
      if (dst->layer2_collision) door_set_layer2(door, 1);
      trig_set(c, triggers, AC_KEY_X(exits->e[k].key), AC_KEY_Y(exits->e[k].key), 0x800 + *generated_door_id);
      (*generated_door_id)++;
      found = 1;
    }
    /* door that nothing pairs to on the other map; error out of everything */
    if (!found && !ac_list_contains(&src->return_doors, dest_id))
      sr_fail(c, SOMRANDO_ERR_FAILED, "Unable to find door %d in %s as requested by %s", dest_id, src->map_name, dst->map_name);
  }
}

/* -------------------------------------------------------------------------------------------------------- the hack */
static int biome_enabled(const struct sr_settings *s, const char *key) {
  const char *list = sr_settings_get(s, "acBiomeTypes");
  size_t n = strlen(key);
  while (*list) {
    const char *comma = strchr(list, ',');
    size_t len = comma ? (size_t)(comma - list) : strlen(list);
    if (len == n && !strncmp(list, key, n)) return 1;
    if (!comma) break;
    list = comma + 1;
  }
  return 0;
}

int sr_p3_hack_ancient_cave_randomizer(struct sr_ctx *c) {
  sr_work_set_int(c, W8_KEY_DOOR_LOCATION, 0x68000);
  const struct ac_biome *enabled[6];
  int n_enabled = 0;
  for (int i = 0; i < 6; i++)
    if (sr_p3_biomes[i] && biome_enabled(c->settings, sr_p3_biomes[i]->key)) enabled[n_enabled++] = sr_p3_biomes[i];

  const int num_floors = sr_p3_ac_num_floors(c->settings);

  /* generate events */
  struct ac_npc_events *npc_events = (struct ac_npc_events *)sr_alloc(c, sizeof *npc_events);
  sr_p3_event_maker(c, npc_events);

  char key[48];
  int has_boss[AC_MAX_FLOORS] = {0}, boss_entry[AC_MAX_FLOORS] = {0}, boss_death[AC_MAX_FLOORS] = {0};
  int num_boss_events = w8_wget_int(c, "bossEventNum");
  for (int i = 0; i < num_boss_events; i++) {
    snprintf(key, sizeof key, "bossEventFloor%d", i);
    int floor_num = w8_wget_int(c, key);
    snprintf(key, sizeof key, "bossEntryEvent%d", i);
    int entry = w8_wget_int(c, key);
    snprintf(key, sizeof key, "bossDeathEvent%d", i);
    int death = w8_wget_int(c, key);
    if (floor_num >= 0 && floor_num < AC_MAX_FLOORS) {
      has_boss[floor_num] = 1;
      boss_entry[floor_num] = entry;
      boss_death[floor_num] = death;
    }
  }

  int num_walkon = w8_wget_int(c, "walkonEventNum");
  int *walkon = (int *)sr_alloc(c, (size_t)(num_walkon > 0 ? num_walkon : 1) * sizeof *walkon);
  for (int i = 0; i < num_walkon; i++) {
    snprintf(key, sizeof key, "walkonEvent%d", i);
    walkon[i] = w8_wget_int(c, key);
  }

  /* replace vanilla doors starting here: ancient cave starting door is 0x0a (StartingDoor hack) */
  struct doors ds = {0};
  int generated_door_id = 10;
  struct door *starting_door = door_new(c, &ds, generated_door_id++);
  /* as we progress through floors, this is the transition to the start of each next floor */
  struct door **prev_exit = (struct door **)sr_alloc(c, 4 * sizeof *prev_exit);
  int n_prev = 1, cap_prev = 4;
  prev_exit[0] = starting_door;
  /* running map palette set index generated for floors */
  sr_work_set_int(c, "GeneratedPaletteIndex", 16);

  for (int i = 0; i < num_floors; i++) {
    /* the progress callback is honoured between floors */
    char *name = (char *)sr_alloc(c, 48);
    snprintf(name, 48, "Ancient Cave floor %d of %d", i + 1, num_floors);
    sr_next_step(c, name);

    /* write the "continue door" for DoorExpansion hack, which determines where we go when we die, and use magic rope */
    int start_door_offset = w8_wget_int(c, W8_KEY_START_DOOR_OFFSET);
    sr_poke(c, start_door_offset + i * 2, generated_door_id - 1);
    sr_poke(c, start_door_offset + i * 2 + 1, (generated_door_id - 1) >> 8);

    /* generate floor */
    const struct ac_biome *gen = enabled[AC_RND(c, n_enabled)];
    sr_work_set_int(c, "FloorNumber", i);
    struct ac_floor floor;
    sr_p3_generate_floor(c, gen, &floor);

    /* inject song play command into the dummy part of the event */
    if (i >= num_walkon) sr_fail(c, SOMRANDO_ERR_FAILED, "The given key '%d' was not present in the dictionary.", i);
    replace_event_data(c, event_get(c, walkon[i]), MUSIC_REPLACEMENT_PATTERN, 5, floor.song, floor.song_len);

    /* inject reward npcs etc for floor */
    if (i >= AC_MAX_FLOORS || !npc_events->has[i]) sr_fail(c, SOMRANDO_ERR_FAILED, "The given key '%d' was not present in the dictionary.", i);
    const struct ac_list *indoor_events = &npc_events->floor[i];
    struct w8_map *inside = floor.inside->md;
    int num_replacement_npcs = indoor_events->n < inside->n_objs ? indoor_events->n : inside->n_objs;
    int *replaced = (int *)sr_alloc(c, (size_t)(num_replacement_npcs > 0 ? num_replacement_npcs : 1) * sizeof *replaced);
    int n_replaced = 0;
    for (int n = 0; n < num_replacement_npcs; n++) {
      const struct ac_npc_event *ev = (const struct ac_npc_event *)AC_PTR(ac_list_get(c, indoor_events, n));
      int idx = AC_RND(c, inside->n_objs);
      for (;;) {
        int dup = 0;
        for (int k = 0; k < n_replaced; k++)
          if (replaced[k] == idx) dup = 1;
        if (!dup) break;
        idx = AC_RND(c, inside->n_objs);
      }
      replaced[n_replaced++] = idx;
      uint8_t *npc = inside->objs[idx];
      ac_obj_set_species(npc, ev->npc_id);
      ac_obj_set_vis_flag(npc, ev->flag_id);
      ac_obj_set_vis_min(npc, ev->flag_min);
      ac_obj_set_vis_max(npc, ev->flag_max);
      ac_obj_set_event(npc, ev->event_id);
      /* set frozen for element NPCs, otherwise their appearance glitches out */
      if (ev->npc_id >= 0x90 && ev->npc_id <= 0x97) ac_obj_set_frozen(npc, 1);
    }

    /* create all necessary doors: each outdoor -> indoor, then the outdoor map to itself where necessary */
    struct trig_map outdoor_triggers = {0}, outdoor_internal = {0}, indoor_triggers = {0}, indoor_internal = {0};
    pair_doors(c, &ds, &outdoor_triggers, floor.outside, floor.inside, &generated_door_id, floor.inside->map_id);
    pair_doors(c, &ds, &outdoor_internal, floor.outside, floor.outside, &generated_door_id, floor.outside->map_id);
    for (int k = 0; k < outdoor_internal.n; k++) trig_set(c, &outdoor_triggers, outdoor_internal.e[k].x, outdoor_internal.e[k].y, outdoor_internal.e[k].val);
    /* each indoor -> outdoor, then the indoor map to itself */
    pair_doors(c, &ds, &indoor_triggers, floor.inside, floor.outside, &generated_door_id, floor.outside->map_id);
    pair_doors(c, &ds, &indoor_internal, floor.inside, floor.inside, &generated_door_id, floor.inside->map_id);
    for (int k = 0; k < indoor_internal.n; k++) trig_set(c, &indoor_triggers, indoor_internal.e[k].x, indoor_internal.e[k].y, indoor_internal.e[k].val);
    /* finally, add the triggers, sorted by x/y, to the map data we'll generate */
    for (int k = 0; k < indoor_triggers.n; k++) ac_map_add_trigger(c, floor.inside->md, indoor_triggers.e[k].val);

    /* connect previous floor's exit door (or the start door, if this is floor 1) to this floor's entry pos */
    for (int k = 0; k < n_prev; k++) {
      door_set_target_map(prev_exit[k], floor.outside->map_id);
      door_set_xpos(prev_exit[k], floor.outside->entry_pos->x);
      door_set_ypos(prev_exit[k], floor.outside->entry_pos->y);
    }

    /* process exit to next floor / boss map and set the previous floor exit doors for the next floor to connect to */
    if (has_boss[i]) {
      /* boss on this floor - warp from outside map to boss map */
      int boss_entry_event = boss_entry[i];
      for (int k = 0; k < floor.outside->exit_locations.n; k++) {
        const struct ac_xy *p = (const struct ac_xy *)AC_PTR(ac_list_get(c, &floor.outside->exit_locations, k));
        trig_set(c, &outdoor_triggers, p->x, p->y, boss_entry_event);
      }
      struct door *boss_door = door_new(c, &ds, generated_door_id);
      door_set_target_map(boss_door, i * 2 + 256);
      door_set_xpos(boss_door, floor.boss->entry_pos->x);
      door_set_ypos(boss_door, floor.boss->entry_pos->y);
      door_set_layer2(boss_door, floor.boss->layer2_collision);
      int event_door_id = generated_door_id + 0x800;
      uint8_t repl[3] = {0x18, (uint8_t)event_door_id, (uint8_t)(event_door_id >> 8)};
      replace_event_data(c, event_get(c, boss_entry_event), UNKNOWN_DOOR_REPLACEMENT_PATTERN, 3, repl, 3);
      generated_door_id++;

      /* create door from boss to next floor; later processing fills the mapnum/x/y in */
      n_prev = 0;
      struct door *next_door = door_new(c, &ds, generated_door_id);
      prev_exit[n_prev++] = next_door;
      event_door_id = generated_door_id + 0x800;
      repl[1] = (uint8_t)event_door_id;
      repl[2] = (uint8_t)(event_door_id >> 8);
      replace_event_data(c, event_get(c, boss_death[i]), UNKNOWN_DOOR_REPLACEMENT_PATTERN, 3, repl, 3);
      generated_door_id++;

      /* attach boss death event to the boss object: species in boss range */
      for (int k = 0; k < floor.boss->md->n_objs; k++) {
        int sp = ac_obj_get_species(floor.boss->md->objs[k]);
        if (sp >= 0x57 && sp < 0x80) ac_obj_set_event(floor.boss->md->objs[k], boss_death[i]);
      }
    } else {
      n_prev = 0;
      /* no boss on this floor - warp from outside map right to the next floor */
      int n_exits = floor.outside->exit_locations.n;
      if (n_exits > cap_prev) {
        prev_exit = (struct door **)sr_alloc(c, (size_t)n_exits * sizeof *prev_exit);
        cap_prev = n_exits;
      }
      for (int k = 0; k < n_exits; k++) {
        const struct ac_xy *p = (const struct ac_xy *)AC_PTR(ac_list_get(c, &floor.outside->exit_locations, k));
        struct door *d = door_new(c, &ds, generated_door_id);
        trig_set(c, &outdoor_triggers, p->x, p->y, 0x800 + generated_door_id);
        generated_door_id++;
        prev_exit[n_prev++] = d;
      }
    }

    /* add the walkon trigger, as the first map trigger, then the others sorted by x/y */
    ac_map_add_trigger(c, floor.outside->md, walkon[i]);
    for (int k = 0; k < outdoor_triggers.n; k++) ac_map_add_trigger(c, floor.outside->md, outdoor_triggers.e[k].val);
  }

  /* last floor exit -> manabeast: a copy of vanilla door 0xBC, which leads to vanilla mana beast map 0xFD */
  for (int k = 0; k < n_prev; k++) {
    door_set_target_map(prev_exit[k], 0xFD);
    door_set_xpos(prev_exit[k], 18);
    door_set_ypos(prev_exit[k], 22);
    door_set_transition(prev_exit[k], 0x81);
  }

  /* re-create mana beast map - 0xFD - and credits map - 0xB4 since ancient cave effectively wipes all vanilla maps */
  w8_make_mana_beast_map(c);
  w8_make_credits_map(c);
  /* the door to the credits map, that the mana beast death event references */
  struct door *credits = door_new(c, &ds, 0x3FF);
  door_set_target_map(credits, 0xB4);
  door_set_transition(credits, 0xA0);
  door_set_xpos(credits, 8);
  door_set_ypos(credits, 6);

  for (int i = 0; i < ds.n; i++) w8_set_door(c, ds.p[i]->id, ds.p[i]->d);
  return 1;
}
