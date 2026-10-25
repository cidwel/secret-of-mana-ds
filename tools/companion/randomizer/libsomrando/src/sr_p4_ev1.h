/* libsomrando: Open World event modules (owner ev1), shared declarations.  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Derived from processing/openworld/events/ (all files) and processing/common/{structure/EventScript,VanillaEventUtil}.cs (Author: Moppleton).
 * Translated to C 2026-10-18. */
#ifndef SR_P4_EV1_H
#define SR_P4_EV1_H
#include <stdint.h>

#include "sr_core.h"
#include "sr_p4_ev1_consts.h"

/* EventScript / List<byte> builders (all bytes are truncated like (byte) casts) */
void sr_p4_ev1_add(struct sr_ctx *c, struct sr_bytes *e, int b);
void sr_p4_ev1_return(struct sr_ctx *c, struct sr_bytes *e);
void sr_p4_ev1_open_dialogue(struct sr_ctx *c, struct sr_bytes *e);
void sr_p4_ev1_close_dialogue(struct sr_ctx *c, struct sr_bytes *e);
void sr_p4_ev1_dialogue_wait(struct sr_ctx *c, struct sr_bytes *e);
/* VanillaEventUtil.getBytesDelay(text, delay, weaponDescripHandlingIndex) appended to e */
void sr_p4_ev1_text(struct sr_ctx *c, struct sr_bytes *e, const char *text, int delay, int weapon_index);
void sr_p4_ev1_dialogue(struct sr_ctx *c, struct sr_bytes *e, const char *text, int ticks);   /* AddDialogue(text, ticks) */
void sr_p4_ev1_dialogue_nowait(struct sr_ctx *c, struct sr_bytes *e, const char *text);      /* AddDialogue(text, null) */
void sr_p4_ev1_dialogue_box(struct sr_ctx *c, struct sr_bytes *e, const char *text);         /* AddDialogueBox */
void sr_p4_ev1_auto_dialogue_box(struct sr_ctx *c, struct sr_bytes *e, const char *text, int ticks);
/* AddDialogueWithChoices (n choices; commands are two bytes each) */
void sr_p4_ev1_dialogue_choices(struct sr_ctx *c, struct sr_bytes *e, const char *text, int n, const uint8_t *x_positions, const char *const *choice_text,
                                const uint8_t (*commands)[2], const uint8_t cancel[2]);
const char *sr_p4_ev1_word_wrap(struct sr_ctx *c, const char *text);                         /* VanillaEventUtil.wordWrapText */
const char *sr_p4_ev1_concat(struct sr_ctx *c, const char *a, const char *b);
struct sr_bytes *sr_p4_ev1_event_get(struct sr_ctx *c, int id);                              /* (EventScript)replacementEvents[id] */

/* prize / hint injection patterns (PrizeEvents, HintEvents); the injector replaces them later */
extern const uint8_t sr_p4_ev1_event_injection_pattern[4];   /* PrizeEvents.OPENWORLD_EVENT_INJECTION_PATTERN */
extern const uint8_t sr_p4_ev1_hint_injection_pattern[4];    /* HintEvents.OPENWORLD_HINT_INJECTION_PATTERN */
void sr_p4_ev1_inject_pattern(struct sr_ctx *c, struct sr_bytes *e, int index, int dialogue);                        /* injectReplacementPattern */
void sr_p4_ev1_inject_pattern_auto(struct sr_ctx *c, struct sr_bytes *e, int index, int dialogue, int wait);        /* ...AutoDialogue */
void sr_p4_ev1_inject_hint(struct sr_ctx *c, struct sr_bytes *e);

/* workingData as StringValueSettings: get() is "" when missing, getBool is yes/true/1, getInt fails like Int32.Parse */
const char *sr_p4_ev1_wstr(struct sr_ctx *c, const char *key);
int sr_p4_ev1_wbool(struct sr_ctx *c, const char *key);
int sr_p4_ev1_wint(struct sr_ctx *c, const char *key);

/* ElementSwaps.getCrystalOrbElementMap */
struct sr_p4_ev1_orbmap {
  int n;
  int map[64];
  int val[64];
};
void sr_p4_ev1_orbmap_load(struct sr_ctx *c, struct sr_p4_ev1_orbmap *m);
int sr_p4_ev1_orbmap_has(const struct sr_p4_ev1_orbmap *m, int map_num);
int sr_p4_ev1_orbmap_get(struct sr_ctx *c, const struct sr_p4_ev1_orbmap *m, int map_num);
extern const uint8_t sr_p4_ev1_lost_continent_orb_flags[7];
extern const uint8_t sr_p4_ev1_xmas_extra_flags[8];   /* EventFlags.XMAS_EXTRA_FLAGS */

#define EV1_JUMP_CMD(t) ((const uint8_t[2]){(uint8_t)(EV_JUMP_BASE + ((t) >> 8)), (uint8_t)(t)})
#define EV1_JSR_CMD(t) ((const uint8_t[2]){(uint8_t)(EV_JUMP_SUBR_BASE + ((t) >> 8)), (uint8_t)(t)})
#define EV1_INCR_CMD(f) ((const uint8_t[2]){EV_INCREMENT_FLAG, (uint8_t)(f)})
#define EV1_DECR_CMD(f) ((const uint8_t[2]){EV_DECREMENT_FLAG, (uint8_t)(f)})

const char *sr_p4_ev1_cat(struct sr_ctx *c, int n, ...);                                       /* string + string + ... */

/* GiftDeliveryIntroEvent.NpcLocationData (the per-run event ids newEventBeforeGift / newEventGiveGift live in workingData, see
 * sr_p4_ev1_gift_location) */
struct sr_p4_ev1_npc {
  const char *key, *description;
  int event_id, female;
  int n_item_dependencies;
  const char *const *item_dependencies;
};
extern const struct sr_p4_ev1_npc sr_p4_ev1_gift_locations[];   /* GiftDeliveryIntroEvent.DELIVERY_LOCATIONS */
extern const int sr_p4_ev1_gift_location_count;
extern const int sr_p4_ev1_santa_dialogue_events[16];           /* GiftDeliveryIntroEvent.SANTA_DIALOGUE_EVENTS */
extern const int sr_p4_ev1_gift_npc_dialogue_events[16];
extern const char *const sr_p4_ev1_gift_no_restrictive[];
extern const int sr_p4_ev1_gift_no_restrictive_count;
/* DELIVERY_LOCATIONS[index] with newEventBeforeGift / newEventGiveGift as set by the gift intro of this run (0 for locations not chosen) */
void sr_p4_ev1_gift_location(struct sr_ctx *c, int index, struct sr_p4_ev1_npc *out, int *new_event_before_gift, int *new_event_give_gift);
void sr_p4_ev1_gift_strings(struct sr_ctx *c, int i, int num_gifts, const struct sr_p4_ev1_npc *loc, const char *characterName, const char **delivery,
                            const char **prize);
const char *sr_p4_ev1_gift_intro_text(struct sr_ctx *c, const char *characterName, const char *finalString);

/* ReindeerIntroEvent: XmasRandoData.ReplacementObject table (index: easy, then medium, then hard, = allReplacementLocations) */
struct sr_p4_ev1_reindeer_loc {
  int map_num, index;                 /* index -1: any enemy object, picked at random by the intro event */
  int n_hint_locations;
  const char *hint_locations[1];
  int n_index_exceptions;
  const int *index_exceptions;
};
extern const struct sr_p4_ev1_reindeer_loc sr_p4_ev1_reindeer_locations[];
extern const int sr_p4_ev1_reindeer_location_count, sr_p4_ev1_reindeer_count_easy, sr_p4_ev1_reindeer_count_medium;
extern const char *const sr_p4_ev1_reindeer_names[8];
const char *sr_p4_ev1_reindeer_intro_text(struct sr_ctx *c, const char *characterName);
/* ReindeerIntroEvent.getByGlobalId(workingData[reindeerLocation<i>]) with the object index the intro event settled on */
void sr_p4_ev1_reindeer_location(struct sr_ctx *c, int global_id, int *map_num, int *object_index);

/* PokedexEvent data, generated at build time from the upstream pokedata3.xml (tools/gen_p4_ev1_resources.py) */
struct sr_p4_ev1_pokemon {
  const char *id, *name, *description;
};
extern const struct sr_p4_ev1_pokemon sr_res_pokedex[];
extern const int sr_res_pokedex_count;
#endif
