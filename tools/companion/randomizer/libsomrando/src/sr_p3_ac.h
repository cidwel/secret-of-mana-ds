/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Ancient Cave (phase 3): shared model of the floor generators.  Derived from (Author of every file: Moppleton):
 * processing/ancientcave/mapgen/{AncientCaveMapGenerator,ProcGenMapNode,TileReplacer,ReplacementMapSegment,*PaletteSetSource}.cs,
 * processing/common/{GeneratedMapUtil,structure/{MapHeader,MapObject,FullMap}}.cs and util/{XyPos,PerlinNoise,ErrorUtil}.cs.
 * Translated to C 2026-10-18.
 *
 * Fidelity contract for every file that ports a C# generator onto this header:
 *  - byte[x,y] is struct ac_layer; the element is LAYER(l,x,y) = d[y*w+x].  Use ac_at()/AC_AT() for any index that is computed: the C# throws
 *    IndexOutOfRangeException when one dimension is out of range (and the whole generation fails with a message), so ac_at() calls sr_fail.
 *  - integer / and % with a non-constant divisor: use ac_div()/ac_mod() (DivideByZeroException in C#, INT_MIN / -1 overflow).
 *  - List<T>/Dictionary: ac_list and ac_dict reproduce the .NET enumeration order, including slot reuse after Remove.
 *  - C# int is int32 with silent wrap-around: do not rely on signed overflow in C, wrap with (int32_t)(uint32_t) where it can happen.
 *  - Randomness: AC_RND(c, n) is `context.randomFunctional.Next() % n`; call sr_random_next_max / sr_random_next_range on &c->rf for the
 *    other overloads.  Keep the order and the number of draws exactly.
 */
#ifndef SR_P3_AC_H
#define SR_P3_AC_H

#include <stddef.h>
#include <stdint.h>

#include "sr_core.h"
#include "sr_p2_bossrush_int.h"

/* ------------------------------------------------------------------------------------------------ checked arithmetic */
int ac_div(struct sr_ctx *c, int a, int b);   /* a / b, C# semantics */
int ac_mod(struct sr_ctx *c, int a, int b);   /* a % b, C# semantics (sign of the dividend) */
#define AC_RND(c, n) ac_mod((c), sr_random_next(&(c)->rf), (n))

/* ------------------------------------------------------------------------------------------------------------ XyPos */
struct ac_xy {
  int16_t x, y;
};
struct ac_xy *ac_xy_new(struct sr_ctx *c, int x, int y);   /* new XyPos(int, int): (short) casts */
int ac_xy_equals(const struct ac_xy *a, const struct ac_xy *b);
/* Dictionary<XyPos,...> key: equality of x and y */
#define AC_XYKEY(x, y) ((int64_t)(((uint32_t)(uint16_t)(int16_t)(y) << 16) | (uint16_t)(int16_t)(x)))
#define AC_KEY_X(k) ((int16_t)((k) & 0xFFFF))
#define AC_KEY_Y(k) ((int16_t)(((k) >> 16) & 0xFFFF))

/* -------------------------------------------------------------------------------------------------------- byte[x,y] */
struct ac_layer {
  int w, h;       /* GetLength(0), GetLength(1) */
  uint8_t *d;     /* d[y * w + x] */
};
struct ac_layer *ac_layer_new(struct sr_ctx *c, int w, int h);   /* new byte[w,h], zeroed */
#define LAYER(l, x, y) ((l)->d[(size_t)(y) * (size_t)(l)->w + (size_t)(x)])
uint8_t *ac_at(struct sr_ctx *c, const struct ac_layer *l, int x, int y);   /* bounds-checked per dimension */
#define AC_AT(c, l, x, y) (*ac_at((c), (l), (x), (y)))

/* ------------------------------------------------------------------------------------------------------ List<T> */
/* items are int64_t: store ints directly and pointers through (int64_t)(intptr_t) */
struct ac_list {
  int64_t *p;
  int n, cap;
};
void ac_list_add(struct sr_ctx *c, struct ac_list *l, int64_t v);
void ac_list_insert(struct sr_ctx *c, struct ac_list *l, int idx, int64_t v);   /* Insert: throws when idx > Count */
int64_t ac_list_get(struct sr_ctx *c, const struct ac_list *l, int idx);        /* l[idx]: throws when out of range */
void ac_list_set(struct sr_ctx *c, struct ac_list *l, int idx, int64_t v);
void ac_list_remove_at(struct sr_ctx *c, struct ac_list *l, int idx);
int ac_list_index_of(const struct ac_list *l, int64_t v);                       /* -1 when absent; pointer or int equality */
int ac_list_contains(const struct ac_list *l, int64_t v);
int ac_list_remove(struct ac_list *l, int64_t v);                               /* List.Remove(item): first occurrence, returns bool */
void ac_list_clear(struct ac_list *l);
#define AC_PTR(v) ((void *)(intptr_t)(v))
#define AC_VAL(p) ((int64_t)(intptr_t)(p))
/* List<XyPos>.Contains / IndexOf use XyPos.Equals (value comparison) */
int ac_xylist_index_of(const struct ac_list *l, int x, int y);
#define ac_xylist_contains(l, x, y) (ac_xylist_index_of((l), (x), (y)) >= 0)

/* ---------------------------------------------------------------------------------------------------- Dictionary */
/* Insertion order with the .NET free-list: Remove frees a slot, the next Add reuses the most recently freed slot.  Enumerate with
 * for (i = 0; i < d->count; i++) if (d->e[i].used) ...  Keys are int64_t (ints, packed XyPos via AC_XYKEY, object pointers, which compare by
 * reference like the .NET default) or C strings (kind AC_DICT_STR, compared with strcmp, the string must outlive the dictionary). */
enum { AC_DICT_INT = 0, AC_DICT_STR = 1 };
struct ac_dent {
  int64_t key, val;
  int used, next_free;
};
struct ac_dict {
  int kind;
  struct ac_dent *e;
  int count, cap, free_list, free_count;
};
void ac_dict_init(struct ac_dict *d, int kind);
int ac_dict_find(const struct ac_dict *d, int64_t key);              /* slot index or -1 */
int ac_dict_has(const struct ac_dict *d, int64_t key);
void ac_dict_set(struct sr_ctx *c, struct ac_dict *d, int64_t key, int64_t val);   /* d[key] = val */
void ac_dict_add(struct sr_ctx *c, struct ac_dict *d, int64_t key, int64_t val);   /* Add: throws when the key exists */
int64_t ac_dict_get(struct sr_ctx *c, const struct ac_dict *d, int64_t key);       /* d[key]: throws KeyNotFound */
int ac_dict_try_get(const struct ac_dict *d, int64_t key, int64_t *val);
int ac_dict_remove(struct ac_dict *d, int64_t key);                  /* returns bool */
int ac_dict_size(const struct ac_dict *d);                           /* Count */
#define AC_STRKEY(s) ((int64_t)(intptr_t)(s))

/* -------------------------------------------------------------------------------------------------- ProcGenMapNode */
struct ac_node {
  int x, y, center_x, center_y;
  const char *name;           /* NULL when unset */
  struct ac_list connections; /* struct ac_node * */
  struct ac_dict values;      /* string -> int, kind AC_DICT_STR */
};
struct ac_node *ac_node_new(struct sr_ctx *c);

/* -------------------------------------------------------------------------------------------- MapHeader / MapObject */
/* Both are the raw 8 bytes of struct w8_map: map->header and map->objs[i].  The setters reproduce VanillaStructure.set: the value is NOT
 * masked to the bit field (a palette set above 0x7F spills into the palette-animated bit, exactly like the C#).  Getters of bool fields
 * return 0/1. */
uint8_t ac_hdr_get_tileset8(const uint8_t *h);
void ac_hdr_set_animated_tiles(uint8_t *h, int v);
void ac_hdr_set_unknown_tile_flag(uint8_t *h, int v);
void ac_hdr_set_tileset8(uint8_t *h, int v);
void ac_hdr_set_palette_animated(uint8_t *h, int v);
int ac_hdr_get_palette_set(const uint8_t *h);
void ac_hdr_set_palette_set(uint8_t *h, int v);
int ac_hdr_get_tileset16(const uint8_t *h);   /* sic: the C# getter reads the tileset8 field */
void ac_hdr_set_tileset16(uint8_t *h, int v);
void ac_hdr_set_layer1_fg(uint8_t *h, int v);
void ac_hdr_set_layer2_fg(uint8_t *h, int v);
void ac_hdr_set_mode7(uint8_t *h, int v);
int ac_hdr_get_walkon(const uint8_t *h);
void ac_hdr_set_walkon(uint8_t *h, int v);
void ac_hdr_set_special_event(uint8_t *h, int v);
void ac_hdr_set_flammie(uint8_t *h, int v);
void ac_hdr_set_magic_rope(uint8_t *h, int v);
void ac_hdr_set_bg_scrolling(uint8_t *h, int v);
void ac_hdr_set_display(uint8_t *h, int v);
int ac_hdr_get_display(const uint8_t *h);
void ac_hdr_set_npc_palette(uint8_t *h, int v);

int ac_obj_get_species(const uint8_t *o);
int ac_obj_get_xpos(const uint8_t *o);
int ac_obj_get_ypos(const uint8_t *o);
int ac_obj_get_event(const uint8_t *o);
int ac_obj_get_vis_flag(const uint8_t *o);
void ac_obj_set_vis_flag(uint8_t *o, int v);
void ac_obj_set_vis_min(uint8_t *o, int v);
void ac_obj_set_vis_max(uint8_t *o, int v);
void ac_obj_set_always_visible(uint8_t *o);
void ac_obj_set_never_visible(uint8_t *o);
void ac_obj_set_xpos(uint8_t *o, int v);
void ac_obj_set_ypos(uint8_t *o, int v);
void ac_obj_set_spawn_priority(uint8_t *o, int v);
void ac_obj_set_direction(uint8_t *o, int v);
void ac_obj_set_unknown4a(uint8_t *o, int v);
void ac_obj_set_layer2_collision(uint8_t *o, int v);
void ac_obj_set_frozen(uint8_t *o, int v);
void ac_obj_set_species(uint8_t *o, int v);
void ac_obj_set_event(uint8_t *o, int v);   /* (ushort) */
void ac_obj_set_unknown_b7(uint8_t *o, int v);

/* mapData.mapObjects.Add(new MapObject()): returns the zeroed 8 bytes of the new element (stable pointer, fixed-capacity array) */
uint8_t *ac_map_add_object(struct sr_ctx *c, struct w8_map *m);
/* mapData.mapTriggers.Add(v) */
void ac_map_add_trigger(struct sr_ctx *c, struct w8_map *m, int v);

/* ----------------------------------------------------------------------------------------- AncientCaveMap and context */
struct ac_map {
  struct w8_map *md;               /* FullMap: md->header (MapHeader), md->objs, md->triggers; md->id is set when the map is registered */
  int map_id;                      /* GeneratedMap.mapId */
  char map_name[64];
  struct ac_layer *layer1, *layer2;
  struct ac_xy *entry_pos;         /* NULL = null */
  struct ac_list exit_locations;   /* XyPos * */
  int layer2_collision;
  int is_ac;                       /* 1 for an AncientCaveMap (has the door dictionaries), 0 for a plain GeneratedMap such as a vanilla boss arena */
  /* AncientCaveMap only */
  struct ac_dict same_exit;        /* AC_XYKEY -> int */
  struct ac_dict same_entry;       /* int -> struct ac_xy * */
  struct ac_dict alt_exit;         /* AC_XYKEY -> int */
  struct ac_dict alt_entry;        /* int -> struct ac_xy * */
  struct ac_list return_doors;     /* int */
};
/* new AncientCaveMap() with mapData = new FullMap() and mapHeader = new MapHeader() (all zero) */
struct ac_map *ac_map_new(struct sr_ctx *c);

struct ac_gen_ctx {                /* AncientCaveGenerationContext */
  int outdoor_w, outdoor_h, indoor_w, indoor_h, boss_w, boss_h;
  struct ac_list shared_nodes;     /* struct ac_node * */
  struct ac_dict special_nodes;    /* string -> struct ac_node * */
  int num_shared_doors, has_num_shared_doors;   /* sharedSettings.setInt / getInt of the only property the generators use */
};
struct ac_gen_ctx *ac_gen_ctx_new(struct sr_ctx *c);
struct ac_node *ac_gen_ctx_shared_node(const struct ac_gen_ctx *g, const char *name);   /* sharedNodes.Find(name == n): NULL if none */
/* getSharedNodesByName(): new dictionary string -> node, later nodes with the same name overwrite (kept in first-insertion order) */
void ac_gen_ctx_nodes_by_name(struct sr_ctx *c, const struct ac_gen_ctx *g, struct ac_dict *out);

/* one floor generator (AncientCaveMapGenerator subclass); `key` is the acBiomeTypes value */
struct ac_biome {
  const char *key;
  const char *floor_type;   /* getFloorType(), e.g. "Manafort Interior" */
  struct ac_gen_ctx *(*get_ctx)(struct sr_ctx *c);
  struct ac_map *(*outdoor)(struct sr_ctx *c, struct ac_gen_ctx *g);
  struct ac_map *(*indoor)(struct sr_ctx *c, struct ac_gen_ctx *g);
  struct ac_map *(*boss)(struct sr_ctx *c, struct ac_gen_ctx *g);
  const uint8_t *(*song)(struct sr_ctx *c, int *len);   /* getSongEventData() */
};
/* the generators in Dictionary insertion order: forest, ruins, island, cave, manafortinterior, and "test" (debug builds only, NULL otherwise) */
extern const struct ac_biome *const sr_p3_biomes[6];

/* ------------------------------------------------------------------------------------------------------ palettes */
/* PaletteSetSource: either a resource (name) or a vanilla palette set index */
struct ac_palsrc {
  const char *resource;   /* "accave.fierycave.bin" or NULL for a vanilla set */
  int vanilla;            /* the vanilla palette set index: animation settings source (resource) or the set itself */
};
/* getPaletteData(): 210 bytes (7 palettes of 15 SNES colors) */
void ac_palsrc_get(struct sr_ctx *c, const struct ac_palsrc *s, uint8_t out[210]);
/* AncientCaveMapGenerator.applyPaletteSource(context, map, palSource) */
void ac_apply_palette_source(struct sr_ctx *c, struct ac_map *map, const struct ac_palsrc *s);
/* DataUtil.readResource for the custom maps: "acManafortBossArenaL1.bin" (1024 bytes), "acManafortNpcRoomL1.bin" (624) */
const uint8_t *ac_custom_map(struct sr_ctx *c, const char *name, size_t *len);

/* ------------------------------------------------------------------------------------------- vanilla boss arenas */
/* GeneratedMapUtil.importVanillaBossArena(context, bossId): returns a map whose md/layers/entry are filled (md->header is the original header
 * with the generated palette). AncientCaveMapGenerator.makeBossMap uses it. */
struct ac_map *ac_import_vanilla_boss_arena(struct sr_ctx *c, int boss_id);

/* --------------------------------------------------------------------------------------------------- PerlinNoise */
double ac_perlin_noise(double x, double y, double z);

/* ----------------------------------------------------------------------------------------- TileReplacer / segments */
struct ac_pattern {                /* TileReplacer.ReplacementPattern */
  const char *const *search;       /* numeric "12", "!12", "{1,2}", "!{1,2}", anything else matches */
  const char *const *inject;       /* numeric or "-" for no change */
  const char *const *alt_inject;   /* may be NULL */
  int width, n;                    /* pattern width; number of entries of search/inject */
};
struct ac_bounds {                 /* TileReplacer.BoundsRectangle */
  int x1, x2, y1, y2;
};
/* applyPattern(pattern, old, new, altLayer, rect): rect NULL = whole layer; alt may be NULL */
void ac_apply_pattern(struct sr_ctx *c, const struct ac_pattern *p, const struct ac_layer *old_l, struct ac_layer *new_l, struct ac_layer *alt,
                      const struct ac_bounds *rect);
struct ac_segment {                /* ReplacementMapSegment */
  int x, y, w, h;
  const uint8_t *data;
};
void ac_apply_segment(struct sr_ctx *c, struct ac_layer *l, int base_x, int base_y, const struct ac_segment *r);

/* ------------------------------------------------------------------------------------------ event maker interface */
/* NpcTaggedEvent and AncientCaveEventMaker.process() (sr_p3_events.c).  The event maker also publishes workingData entries with the same names
 * as the C# constants: "bossEventFloor<i>", "bossEntryEvent<i>", "bossDeathEvent<i>", "bossEventNum", "walkonEvent<i>", "walkonEventNum", ... */
struct ac_npc_event {
  int event_id;
  int npc_id;       /* byte */
  int flag_id;      /* eventFlagId, byte */
  int flag_min;     /* eventFlagMin, byte */
  int flag_max;     /* eventFlagMax, byte, default 0x0F */
  int mobile;
};
#define AC_MAX_FLOORS 24
struct ac_npc_events {
  struct ac_list floor[AC_MAX_FLOORS];   /* Dictionary<int, List<NpcTaggedEvent>> keyed by floor number; items are struct ac_npc_event * */
  int has[AC_MAX_FLOORS];                /* ContainsKey */
};
void sr_p3_event_maker(struct sr_ctx *c, struct ac_npc_events *out);

/* ------------------------------------------------------------------------------------------------------- debug dump */
/* One line per generated map in the format of the instrumented oracle (tests/ac_*.py compare them); a no-op unless the environment variable
 * SOM_AC_DUMP names a file (test builds only, -DSOMRANDO_DEBUG). */
void ac_dump_line(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void ac_dump_rng(struct sr_ctx *c, const char *tag);
void ac_dump_map(struct sr_ctx *c, const char *tag, const struct ac_map *m);

#endif
