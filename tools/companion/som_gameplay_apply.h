/* som_gameplay_apply: C port of `tools/gameplay/som_gameplay.py apply` (board items T8 / T4).  The Thor app has no Python, so the gameplay
 * options chosen on the start menu are turned into the same byte patches here.  The output is byte-identical to the Python tool
 * (--on-conflict skip, --checksum auto); tools/companion/android/tests/gameplay_parity.py proves it against the tool for a fixed set of option
 * files and re-checks the option catalogue, so a new option added to som_gameplay.py shows up as a failing test until it is ported.
 * Pure C99, no allocation, no OS dependency. */
#ifndef SOM_GAMEPLAY_APPLY_H
#define SOM_GAMEPLAY_APPLY_H

#include <stddef.h>
#include <stdint.h>

struct sga_opts {
  char weapon_recharge[12], run_recharge[12], monster_flinch[12], hero_flinch[12], charge_speed[12], fix_checksum[12];
  int charge_frames_per_stage;      /* -1 = null */
  int charge_speed_up_bonus;        /* 0 / 1 */
  int hero_crit_bonus_girl, hero_crit_bonus_others;   /* -1 = null */
  int crit_multiplier, walk_speed, weapon_level_rate;
  int charge_bonus_item_id;         /* -1 = null */
  /* quality-of-life group (T11) */
  int qol_chest_fast, qol_fast_transitions, qol_fast_fades, qol_levelup_mp_restore, qol_dead_exp, qol_unready_damage;
  int qol_item_limit, qol_magic_level_rate;   /* 4 or 7; 1..3 */
  int qol_ring_speed, qol_message_speed, qol_shop_fill;   /* 1..2; 1, 2 or 4; 0 / 1 */
  char qol_scroll_deadzone[12];     /* vanilla / tight / tighter */
};

void sga_defaults(struct sga_opts *o);
/* Parses {"options": {...}} or a flat object (the file som_gameplay.py takes).  Unknown keys and bad values are errors (0 ok, -1 + err). */
int sga_parse(struct sga_opts *o, const char *json, char *err, size_t n);
/* 1 when the option key is implemented here (the menu page hides the others). */
int sga_supports(const char *key);
int sga_is_default(const struct sga_opts *o);
/* Applies the options to rom[0..len) in place (a 512 byte copier header is allowed).  Returns the number of bytes changed, or -1 with err
 * (the ROM is then untouched).  Options whose bytes conflict with the ROM (another hack) are skipped and named in `skipped` (comma separated). */
long sga_apply(uint8_t *rom, size_t len, const struct sga_opts *o, char *err, size_t en, char *skipped, size_t sn);

#endif
