/* libsomrando: internal declarations of the Open World search (phase 4, owner "core").  LGPL-2.1 or later.
 *
 * Derived from processing/openworld/randomization (Author: Moppleton).  Translated to C 2026-10-18. */
#ifndef SR_P4_OW_CORE_INT_H
#define SR_P4_OW_CORE_INT_H
#include <stdint.h>

#include "sr_core.h"
#include "sr_p4_ow.h"

/* List<string>, List<int> */
struct ow_slist { const char **p; int n, cap; };
struct ow_ilist { int *p; int n, cap; };
/* List<PrizeLocation> / List<PrizeItem> */
struct ow_locs { struct ow_location **v; int n, cap; };
struct ow_prizes { struct ow_prize **v; int n, cap; };

#define ow_sa sr_p4_core_sa
#define ow_cat sr_p4_core_cat
#define ow_sl_add sr_p4_core_sl_add
#define ow_sl_arr sr_p4_core_sl_arr
#define ow_sl_contains sr_p4_core_sl_contains
#define ow_ilist_contains sr_p4_core_ilist_contains
#define ow_loc_add sr_p4_core_loc_add
#define ow_gift_text sr_p4_core_gift_text
#define ow_sg_set sr_p4_core_sg_set
#define ow_gold_flags_init sr_p4_core_gold_flags_init

/* NULL terminated argument lists (the C# `new string[] { ... }` / string concatenation) */
const char **sr_p4_core_sa(struct sr_ctx *c, ...) __attribute__((sentinel));
const char *sr_p4_core_cat(struct sr_ctx *c, ...) __attribute__((sentinel));
void sr_p4_core_sl_add(struct sr_ctx *c, struct ow_slist *l, const char *s);
const char **sr_p4_core_sl_arr(struct sr_ctx *c, const struct ow_slist *l);   /* ToArray(), NULL terminated */
int sr_p4_core_sl_contains(const struct ow_slist *l, const char *s);
void sr_p4_core_il_add(struct sr_ctx *c, struct ow_ilist *l, int v);
int sr_p4_core_ilist_contains(const struct ow_ilist *l, int v);
int sr_p4_core_il_remove(struct ow_ilist *l, int v);   /* List<int>.Remove(value) */
void sr_p4_core_loc_add(struct sr_ctx *c, struct ow_locs *L, const char *name, int map, int obj, int ev, int idx, const char **opts,
                        const char **hints, const char **locked, double reach);
struct ow_location *sr_p4_core_loc_new(struct sr_ctx *c, const char *name, int map, int obj, int ev, int idx, const char **opts,
                                       const char **hints, const char **locked, double reach);
int sr_p4_core_filtered_count(struct sr_ctx *c, const struct ow_locs *locs);   /* GiftModeProcessing.processLocations(...).Count */
void sr_p4_core_prize_add(struct sr_ctx *c, struct ow_prizes *P, const char *name, const char *type, const struct sr_bytes *data,
                          const char *hint, int flag, double value);

/* workingData typed access (StringValueSettings semantics) */
int sr_p4_core_w_bool(const struct sr_ctx *c, const char *key);       /* getBool: missing -> false */
const char *sr_p4_core_w_str(const struct sr_ctx *c, const char *key); /* get: missing -> "" */
int sr_p4_core_w_int(struct sr_ctx *c, const char *key);              /* getInt: fails like the FormatException */

/* SomVanillaValues */
const char *sr_p4_core_weapon_name(int weapon);                        /* weaponByteToName: "" when out of range */
const char *sr_p4_core_element_name(int element);                      /* elementOrbByteToName(b, false) (lower case) */
/* ElementSwaps.getCrystalOrbElementMap(context): value of the workingData entry "orbElement<mapNum>"; fails when the key is missing */
int sr_p4_core_orb_element(struct sr_ctx *c, int map_num);
int sr_p4_core_orb_has(struct sr_ctx *c, int map_num);

/* gift text: getRandomGiftText(r, prizePrefixes, name) appended to out (draws from rf) */
void sr_p4_core_gift_text(struct sr_ctx *c, struct sr_bytes *out, const char *prize_name);
void sr_p4_core_gift_text_fmt(struct sr_ctx *c, struct sr_bytes *out, const char *const *formats, int n_formats, const char *prize_name);
void sr_p4_core_sg_set(int *key, int *val, int *n, int k, int v);
void sr_p4_core_gold_flags_init(uint8_t *flags, int *n);

/* OpenWorldLocations / OpenWorldPrizes */
void sr_p4_core_locations(struct sr_ctx *c, struct ow_locs *L);
void sr_p4_core_prizes(struct sr_ctx *c, struct ow_prizes *P, const struct ow_locs *locs);

/* StartingWeaponRandomizer */
void sr_p4_core_set_starting_weapons(struct sr_ctx *c);

/* OpenWorldLocationPrizeMatcher + OpenWorldSimulator */
struct sr_p4_core_search;
struct sr_p4_core_search *sr_p4_core_search_new(struct sr_ctx *c, struct ow_locs *locs, struct ow_prizes *prizes);
/* placeItems: fills out (a fresh placement list); returns 0 when the C# returns false */
int sr_p4_core_place_items(struct sr_ctx *c, struct sr_p4_core_search *s, struct ow_placements *out);
/* runSimulation: allocates the result in the arena */
struct ow_sim_result *sr_p4_core_run_simulation(struct sr_ctx *c, struct sr_p4_core_search *s, const struct ow_placements *p);

/* debug dump (SOM_CORE_DUMP, debug builds only) */
void sr_p4_core_dump(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
int sr_p4_core_dump_enabled(void);
void sr_p4_core_dump_prizes(const struct ow_locs *locs, const struct ow_prizes *prizes);
void sr_p4_core_dump_rng(struct sr_ctx *c, const char *tag);
#endif
