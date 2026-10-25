/* som_gameplay (C): the battle gameplay options of tools/gameplay/som_gameplay.py as an on-device patcher.
 *
 * Same records, same bytes: the Python tool is the reference and tools/companion/gameplay_c/test_gameplay_c.py compares the two on
 * many option combinations.  The input ROM buffer is patched in place by the caller's copy; nothing here touches a file.
 * Pure C99, no allocation, no OS dependency (3DS, Thor JNI and PC can all use it).
 */
#ifndef SOM_GAMEPLAY_C_H
#define SOM_GAMEPLAY_C_H

#include <stddef.h>
#include <stdint.h>

/* Option ids, in the order of gameplay_menu.json "options". */
enum sgp_opt {
  SGP_WEAPON_RECHARGE = 0,  /* 0 vanilla, 1 fast, 2 faster, 3 instant */
  SGP_RUN_RECHARGE,         /* 0 vanilla, 1 fast, 2 none */
  SGP_CHARGE_SPEED,         /* 0 vanilla, 1 fast, 2 faster, 3 instant */
  SGP_CHARGE_FRAMES,        /* -1 unset, 1..90 */
  SGP_CHARGE_SPEEDUP_BONUS, /* 0 / 1 */
  SGP_CHARGE_BONUS_ITEM,    /* -1 unset, 0..254 */
  SGP_WEAPON_LEVEL_RATE,    /* 1..3 */
  SGP_MONSTER_FLINCH,       /* 0 vanilla, 1 strong, 2 heavy, 3 never */
  SGP_HERO_FLINCH,          /* 0 vanilla, 1 light, 2 never */
  SGP_CRIT_GIRL,            /* -1 unset, 0..120 */
  SGP_CRIT_OTHERS,          /* -1 unset, 0..120 */
  SGP_CRIT_MULTIPLIER,      /* 2..4 */
  SGP_WALK_SPEED,           /* 2..4 */
  SGP_QOL_CHEST_FAST,       /* 0 / 1 */
  SGP_QOL_FAST_TRANSITIONS, /* 0 / 1 */
  SGP_QOL_FAST_FADES,       /* 0 / 1 */
  SGP_QOL_UNREADY_DAMAGE,   /* 0 / 1 */
  SGP_QOL_ITEM_LIMIT,       /* 4 or 7 */
  SGP_QOL_LEVELUP_MP,       /* 0 / 1 */
  SGP_QOL_DEAD_EXP,         /* 0 / 1 */
  SGP_QOL_MAGIC_RATE,       /* 1..3 */
  SGP_QOL_SCROLL_DEADZONE,  /* 0 vanilla, 1 tight, 2 tighter */
  SGP_QOL_RING_SPEED,       /* 1 vanilla, 2 */
  SGP_QOL_MESSAGE_SPEED,    /* 1 vanilla, 2, 4 */
  SGP_QOL_SHOP_FILL,        /* 0 / 1 */
  SGP_COUNT
};

struct sgp_opts { int32_t v[SGP_COUNT]; };

#define SGP_MAX_RECS 80

struct sgp_report {
  int nrecs;                /* records applied (or already present) */
  int nchanged_bytes;
  uint32_t skipped;         /* bit mask of options dropped because their bytes conflict (enum sgp_opt) */
  char why[96];             /* first conflict: option, offset and what was found */
  int checksum_updated;
  int mark_use_hits;        /* foreign uses of the monster byte $E16B (warning for monster_flinch strong/heavy) */
};

void sgp_defaults(struct sgp_opts *o);                    /* vanilla behaviour: applying them changes nothing */
int sgp_is_default(const struct sgp_opts *o);
const char *sgp_key(int opt);                             /* option key as in gameplay_menu.json */
int sgp_find_key(const char *key);                        /* -1 if unknown */
int sgp_valid(int opt, int32_t value);                    /* 1 when the value is allowed for the option */

/* Patches `rom` (len bytes, with or without the 512 byte copier header) in place.  Returns 0 and fills `rep`; options whose bytes do not match the
 * ROM (a randomizer hack on the same code, another region) are dropped whole and reported in rep->skipped.  -1: bad size or a bad value (rep->why). */
int sgp_apply(uint8_t *rom, size_t len, const struct sgp_opts *o, struct sgp_report *rep);

#endif
