/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Phase 2: WeaponRandomizer, MagicRebalance, ElementalDamage.  Author of every upstream hack: Moppleton.  Translated to C 2026-10-18.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "sr_core.h"
#include "sr_p2_weapons_data.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))
#define S(key) sr_settings_get(c->settings, (key))
#define SB(key) sr_settings_get_bool(c->settings, (key))

static void bad_mode(struct sr_ctx *c, const char *what) { sr_fail(c, SOMRANDO_ERR_UNSUPPORTED, "unsupported option: %s", what); }


/* StatusCondition: palette sets are shared where the C# shares the array */
struct sr_w1_cond {
  uint16_t bitmask;
  const char *weapon_desc;
  int boost_stat;
  uint8_t enemy_effect, crit_rate, hit_rate, element;
  const struct sr_w1_pal *const *pals;
  int n_pals;
};

#define PALS(name, ...) static const struct sr_w1_pal *const name[] = {__VA_ARGS__}
#define PL(n) (&sr_w1_pal_##n)
PALS(sr_w1_ps_none, PL(dullGreen), PL(greenBrown), PL(manaSwordColorMaybe), PL(pink), PL(darkDullPurple), PL(plainPurple), PL(lavender), PL(dirtColor),
     PL(whiteish), PL(lightBlue), PL(pastelPink), PL(purpleGray), PL(gray));
PALS(sr_w1_ps_stun, PL(thunderSaberColor));
PALS(sr_w1_ps_freeze, PL(icyBlueAndPurple), PL(grayBlue), PL(brightLightBlue));
PALS(sr_w1_ps_engulf, PL(fiery), PL(orangeGoldish));
PALS(sr_w1_ps_poison, PL(neonGreen), PL(purpleRainbow), PL(niceGreen));
PALS(sr_w1_ps_stone, PL(grayish), PL(whiteish), PL(gray));
PALS(sr_w1_ps_golden, PL(glowyOrange), PL(brightYellow), PL(orangeGoldish));
PALS(sr_w1_ps_red, PL(rusty), PL(redBanana));
PALS(sr_w1_ps_holy, PL(lumina), PL(bluePinkRainbow), PL(brightYellow), PL(brightPink));
PALS(sr_w1_ps_black, PL(darkPurple), PL(redBanana));
PALS(sr_w1_ps_dragon, PL(blueAndGreen));
PALS(sr_w1_ps_crit, PL(purpleRainbow), PL(icyBlueAndPurple), PL(blueAndGreen));
#define NP(a) ((int)(sizeof(a) / sizeof((a)[0])))

static const struct sr_w1_cond sr_w1_conds[SR_W1_C_COUNT] = {
  [SR_W1_C_NONE] = {0x0000, "", -1, 0, 0, 0, 0, sr_w1_ps_none, NP(sr_w1_ps_none)},
  [SR_W1_C_STUN] = {0x0010, "Causes stun/Thunder damage. ", -1, 0, 0, 0, 0x02, sr_w1_ps_stun, NP(sr_w1_ps_stun)},
  [SR_W1_C_FREEZE] = {0x0004, "Causes slow/Ice damage. ", -1, 0, 0, 0, 0x04, sr_w1_ps_freeze, NP(sr_w1_ps_freeze)},
  [SR_W1_C_ENGULF] = {0x4000, "Causes engulf/Fire damage. ", -1, 0x02, 0, 0, 0x08, sr_w1_ps_engulf, NP(sr_w1_ps_engulf)},
  [SR_W1_C_POISON] = {0x2000, "Causes poison/Dryad damage. ", -1, 0, 0, 0, 0x80, sr_w1_ps_poison, NP(sr_w1_ps_poison)},
  [SR_W1_C_CONFUSE] = {0x0080, "Chance to confuse. ", -1, 0, 0, 0, 0, NULL, 0},
  [SR_W1_C_STONE] = {0x0040, "Causes stone/Earth damage. ", -1, 0, 0, 0, 0x01, sr_w1_ps_stone, NP(sr_w1_ps_stone)},
  [SR_W1_C_NONE_GOLDEN] = {0, "Luna damage. ", -1, 0, 0, 0, 0x40, sr_w1_ps_golden, NP(sr_w1_ps_golden)},
  [SR_W1_C_NONE_RED] = {0, "Adds Beast damage. ", -1, 0x08, 0, 0, 0, sr_w1_ps_red, NP(sr_w1_ps_red)},
  [SR_W1_C_NONE_HOLY] = {0, "Adds light/undead damage. ", -1, 0x60, 0, 0, 0x20, sr_w1_ps_holy, NP(sr_w1_ps_holy)},
  [SR_W1_C_NONE_BLACK] = {0, "Adds dark/humanoid damage. ", -1, 0x01, 0, 0, 0x10, sr_w1_ps_black, NP(sr_w1_ps_black)},
  [SR_W1_C_NONE_DRAGON] = {0, "Adds Dragon damage. ", -1, 0x80, 0, 0, 0, sr_w1_ps_dragon, NP(sr_w1_ps_dragon)},
  [SR_W1_C_NONE_CRIT] = {0, "Increased crit. ", -1, 0, 30, 0, 0, sr_w1_ps_crit, NP(sr_w1_ps_crit)},
  [SR_W1_C_NONE_HIT] = {0, "Perfect hitrate. ", -1, 0, 0, 100, 0, sr_w1_ps_none, NP(sr_w1_ps_none)},
  [SR_W1_C_NONE_SUPERCRIT] = {0, "High damage range. ", -1, 0, 100, 50, 0, sr_w1_ps_none, NP(sr_w1_ps_none)},
  [SR_W1_C_NONE_AGIPLUS] = {0, "Adds agi. ", 0, 0, 0, 0, 0, NULL, 0},
  [SR_W1_C_NONE_CONPLUS] = {0, "Adds con. ", 1, 0, 0, 0, 0, NULL, 0},
  [SR_W1_C_NONE_INTPLUS] = {0, "Adds int/wis. ", 2, 0, 0, 0, 0, NULL, 0},
  [SR_W1_C_NONE_STRPLUS] = {0, "Adds str. ", 4, 0, 0, 0, 0, NULL, 0},
  [SR_W1_C_NONE_GARBAGE] = {0, "Trash. ", -1, 0, 0, 10, 0, NULL, 0},
  [SR_W1_C_MANASTR] = {0, "Adds str. ", 4, 0, 0, 0, 0, NULL, 0},
};

static int w1_ranged_next(struct sr_ctx *c, int n) { return (int)(sr_random_next(&c->rf) % n); }

/* WeaponCategory.getMatchingSpecs(conditions, sharedSpecialNames) is the matching specs repeated 16 times followed by the shared matches. */
struct w1_match {
  const struct sr_w1_spec *base;
  const struct sr_w1_spec **own;
  int n_own;
  const struct sr_w1_spec **shared;
  int n_shared;
  int total;
};

static int w1_cond_in(const int *conds, int n, int cond) {
  for (int i = 0; i < n; i++)
    if (conds[i] == cond) return 1;
  return 0;
}

static int w1_collect(const struct sr_w1_spec *specs, int n_specs, const int *conds, int n_conds, const struct sr_w1_spec **out) {
  int n = 0;
  for (int i = 0; i < n_specs; i++)
    if (w1_cond_in(conds, n_conds, specs[i].cond)) out[n++] = &specs[i];
  return n;
}

static int w1_str_in(char **list, int n, const char *s) {
  for (int i = 0; i < n; i++)
    if (!strcmp(list[i], s)) return 1;
  return 0;
}

int sr_hack_weapon_randomizer(struct sr_ctx *c) {
  const int is_rando = !strcmp(S("mode"), "rando");
  const int is_open = !strcmp(S("mode"), "open");
  const int vanilla = is_rando || is_open;
  if (!vanilla && strcmp(S("mode"), "bossrush") && strcmp(S("mode"), "chaos") && strcmp(S("mode"), "ancientcave")) { bad_mode(c, "weapon randomizer in this mode"); return 0; }
  if ((is_rando && !SB("rWeapons")) || (is_open && !SB("opWeapons"))) {
    sr_work_set_int(c, "weaponElementsTableOffset", -1);
    return 0;
  }
  const int xmas = is_rando && !strcmp(S("rHoliday"), "xmas");
  const int include_trash = SB("trashWeapons");
  /* vanillaStats is true only for mode rando: other modes draw the enemy effect unconditionally and write the pow() based attack power */

  int status_conditions[8], n_status = 0;
  status_conditions[n_status++] = SR_W1_C_STUN;
  status_conditions[n_status++] = SR_W1_C_FREEZE;
  status_conditions[n_status++] = SR_W1_C_ENGULF;
  if (xmas) {
    status_conditions[n_status++] = SR_W1_C_ENGULF;
    status_conditions[n_status++] = SR_W1_C_ENGULF;
  }
  status_conditions[n_status++] = SR_W1_C_POISON;
  status_conditions[n_status++] = SR_W1_C_CONFUSE;
  status_conditions[n_status++] = SR_W1_C_STONE;
  static const int extra_none[] = {SR_W1_C_NONE_GOLDEN, SR_W1_C_NONE_RED, SR_W1_C_NONE_HOLY, SR_W1_C_NONE_BLACK,
                                   SR_W1_C_NONE_DRAGON, SR_W1_C_NONE_CRIT, SR_W1_C_NONE_HIT, SR_W1_C_NONE_SUPERCRIT};
  int stat_boost[13], n_stat_boost = 0;
  for (int i = 0; i < 3; i++) stat_boost[n_stat_boost++] = SR_W1_C_NONE_AGIPLUS;
  for (int i = 0; i < 3; i++) stat_boost[n_stat_boost++] = SR_W1_C_NONE_CONPLUS;
  for (int i = 0; i < 3; i++) stat_boost[n_stat_boost++] = SR_W1_C_NONE_INTPLUS;
  for (int i = 0; i < 3; i++) stat_boost[n_stat_boost++] = SR_W1_C_NONE_STRPLUS;

  const int elements_table = c->working_offset;
  struct sr_random *rnd = &c->rf;
  sr_work_set_int(c, "weaponElementsTableOffset", elements_table);
  c->working_offset += 72;

  int status_weapons[4], n_sw = 0;
  for (int i = 0; i < 2; i++) {
    int non_ranged = (int)(sr_random_next(rnd) % 4);
    while (w1_cond_in(status_weapons, n_sw, non_ranged)) non_ranged = (int)(sr_random_next(rnd) % 4);
    status_weapons[n_sw++] = non_ranged;
    int ranged = 4 + (int)(sr_random_next(rnd) % 4);
    while (w1_cond_in(status_weapons, n_sw, ranged)) ranged = 4 + (int)(sr_random_next(rnd) % 4);
    status_weapons[n_sw++] = ranged;
  }

  char **used_names = (char **)sr_alloc(c, 72 * sizeof(char *));
  int n_used = 0;
  char *names[8][9];
  char *descs[8][9];

  P(0x4B7B, 10);

  int trash_weapons[8], n_trash = 0;
  const struct sr_w1_spec **own = (const struct sr_w1_spec **)sr_alloc(c, 128 * sizeof(*own));
  const struct sr_w1_spec **shared = (const struct sr_w1_spec **)sr_alloc(c, 32 * sizeof(*shared));
  const struct sr_w1_spec **trash_list = (const struct sr_w1_spec **)sr_alloc(c, 128 * sizeof(*trash_list));

  for (int weapon_num = 0; weapon_num < 9; weapon_num++) {
    for (int weapon_type = 0; weapon_type < 8; weapon_type++) {
      const struct sr_w1_cat *weapon = &sr_w1_cats[weapon_type];
      int enemy_effect = SR_W1_C_NONE;
      if (!vanilla) enemy_effect = extra_none[w1_ranged_next(c, 8)];
      else if ((sr_random_next(rnd) % 2) == 0) enemy_effect = extra_none[w1_ranged_next(c, 8)];
      int stat_effect = stat_boost[w1_ranged_next(c, n_stat_boost)];

      int conditions[5], n_cond = 0;
      conditions[n_cond++] = SR_W1_C_NONE;
      if (enemy_effect != SR_W1_C_NONE) conditions[n_cond++] = enemy_effect;
      conditions[n_cond++] = stat_effect;

      const struct sr_w1_cond *ec = &sr_w1_conds[enemy_effect];
      int status_weapon = vanilla ? (enemy_effect == SR_W1_C_NONE || ec->element == 0) : w1_cond_in(status_weapons, n_sw, weapon_type);
      if (status_weapon) {
        int stat_cond = status_conditions[w1_ranged_next(c, n_status)];
        conditions[n_cond++] = stat_cond;
        conditions[n_cond++] = stat_cond;
      }

      int n_own = w1_collect(weapon->specs, weapon->n_specs, conditions, n_cond, own);
      int n_shared = w1_collect(sr_w1_shared_specs, SR_W1_N_SHARED, conditions, n_cond, shared);
      int total = n_own * 16 + n_shared;

      struct sr_w1_spec mana_spec;
      const struct sr_w1_spec *chosen = NULL;
      const char *chosen_name = "";
      if (vanilla && weapon_type == 1 && weapon_num == 8) {
        static const uint8_t icons88[] = {88};
        static const struct sr_w1_pal mana_pal = {0x04, 8};
        stat_boost[n_stat_boost++] = SR_W1_C_MANASTR;
        mana_spec.name = "Mana Sword";
        mana_spec.icons = icons88;
        mana_spec.n_icons = 1;
        mana_spec.cond = SR_W1_C_MANASTR;
        mana_spec.desc = "It's the legendary Sword of Mana.";
        mana_spec.pal = &mana_pal;
        chosen = &mana_spec;
        chosen_name = "Mana Sword";
      } else {
        int trash_weapon = include_trash && (weapon_num > 0 && weapon_num < 8 && (sr_random_next(rnd) % 50) == 0);
        int found_trash = 0;
        if (trash_weapon && !w1_cond_in(trash_weapons, n_trash, weapon_type)) {
          int garb = SR_W1_C_NONE_GARBAGE;
          int n_trash_list = w1_collect(weapon->specs, weapon->n_specs, &garb, 1, trash_list);
          if (n_trash_list > 0) {
            trash_weapons[n_trash++] = weapon_type;
            chosen = trash_list[w1_ranged_next(c, n_trash_list)];
            chosen_name = chosen->name;
            found_trash = 1;
            n_cond = 0;
            conditions[n_cond++] = SR_W1_C_NONE_GARBAGE;
          }
        }
        if (!found_trash) {
          int name_found = 0, iters = 0;
          while (!name_found && iters < 100) {
            int idx = w1_ranged_next(c, total);
            chosen = idx < n_own * 16 ? own[idx % n_own] : shared[idx - n_own * 16];
            chosen_name = chosen->name;
            if (!w1_str_in(used_names, n_used, chosen_name)) {
              name_found = 1;
              used_names[n_used++] = (char *)chosen_name;
            }
            iters++;
          }
        }
      }

      names[weapon_type][weapon_num] = (char *)chosen_name;

      size_t desc_cap = 62 + 512;
      char *desc = (char *)sr_alloc(c, desc_cap);
      size_t dlen = strlen(chosen->desc);
      memcpy(desc, chosen->desc, dlen);
      const int num_spaces = 62 - (int)dlen;
      if (num_spaces != 62)
        for (int i = 0; i < num_spaces; i++) desc[dlen++] = ' ';

      const struct sr_w1_pal *pals = NULL;
      const struct sr_w1_cond *cond = chosen->cond == SR_W1_C_MANASTR ? &sr_w1_conds[SR_W1_C_MANASTR] : &sr_w1_conds[chosen->cond];
      if (cond->pals) pals = cond->pals[sr_random_next(rnd) % cond->n_pals];
      if (chosen->pal) pals = chosen->pal;
      if (pals) {
        P(0x18FDE2 + weapon_type * 18 + weapon_num * 2 + 1, pals->menu_pal);
        P(0x101000 + weapon_type * 9 * 12 + weapon_num * 12 + 4, pals->weapon_pal);
      }

      int base = 0x101000 + weapon_type * 9 * 12 + weapon_num * 12;
      uint8_t icon = weapon->menu_icons[sr_random_next(rnd) % weapon->n_menu_icons];
      if (chosen->icons) icon = chosen->icons[sr_random_next(rnd) % chosen->n_icons];
      P(0x18FDE2 + weapon_type * 18 + weapon_num * 2, icon);

      for (int k = 0; k < weapon->n_proj; k++)
        if (weapon->proj_keys[k] == icon) P(base + 3, weapon->proj_vals[k]);

      unsigned full_bitmask = 0;
      unsigned enemy_bitmask = 0;
      unsigned element = 0;
      int stat_effects[8], n_se = 0;
      uint8_t max_crit = 0;
      uint8_t atk_power = 0;
      int atk_penalty = 0;
      int max_hit = 0;
      int desc_conds[5], n_dc = 0;
      for (int i = 0; i < n_cond; i++) {
        const struct sr_w1_cond *tc = &sr_w1_conds[conditions[i]];
        if (tc->bitmask != 0) {
          full_bitmask |= tc->bitmask;
          atk_penalty += 1 + weapon_num;
        }
        enemy_bitmask |= tc->enemy_effect;
        element |= tc->element;
        if (tc->boost_stat != -1 && !w1_cond_in(stat_effects, n_se, tc->boost_stat)) stat_effects[n_se++] = tc->boost_stat;
        if (tc->weapon_desc[0] != 0 && !w1_cond_in(desc_conds, n_dc, conditions[i])) {
          size_t l = strlen(tc->weapon_desc);
          memcpy(desc + dlen, tc->weapon_desc, l);
          dlen += l;
          desc_conds[n_dc++] = conditions[i];
        }
        if (tc->crit_rate > max_crit) max_crit = tc->crit_rate;
        if (tc->hit_rate > max_hit) max_hit = tc->hit_rate;
      }
      desc[dlen] = 0;
      if (max_hit == 0) max_hit = 75;
      if (!vanilla) {
        double atk = rint(5 + pow(weapon_num + 1, 1.25) * 8);
        if (weapon_type >= 4) atk *= 0.9;
        else if (weapon_type == 0) atk *= 1.1;
        atk_power = (uint8_t)(int)atk;
        atk_power = (uint8_t)(atk_power - atk_penalty);
        P(base + 8, atk_power);
      }
      P(base + 6, max_crit);
      P(base + 7, max_hit);

      int stat1 = 0, stat2 = 0;
      for (int i = 0; i < n_se; i++) {
        switch (stat_effects[i]) {
          case 0: stat1 |= 0x80; break;
          case 1: stat1 |= 0x20; break;
          case 2: stat1 |= 0x08; stat1 |= 0x02; break;
          case 4: stat2 |= 0x02; break;
        }
      }
      P(base + 1, stat1);
      P(base + 2, stat2);
      descs[weapon_type][weapon_num] = desc;

      P(base + 9, full_bitmask & 0xFF);
      P(base + 10, (full_bitmask >> 8) & 0xFF);
      int status_chance = (weapon_num + 4) * 10;
      if (status_chance > 100) status_chance = 100;
      P(base + 11, status_chance);
      P(base + 5, enemy_bitmask);
      P(elements_table + weapon_type * 9 + weapon_num, element);
    }
  }

  for (int i = 0; i < 8; i++)
    for (int j = 0; j < 9; j++) sr_names_set(c, 0x32 + i * 9 + j, names[i][j]);

  int total_desc = 50;
  for (int i = 0; i < 8; i++)
    for (int j = 0; j < 9; j++) total_desc += (int)strlen(descs[i][j]) + 1;
  sr_ensure_space_in_bank(&c->working_offset, total_desc);
  const int desc_bank = (c->working_offset >> 16) + 0xC0;

  P(0x76495, 0x22);
  P(0x76496, c->working_offset);
  P(0x76497, c->working_offset >> 8);
  P(0x76498, (c->working_offset >> 16) + 0xC0);
  P(0x76499, 0xEA);

  E(0xE2); E(0x20);
  E(0x8B);
  E(0xA9); E(desc_bank);
  E(0x48);
  E(0xAB);
  E(0xB9); E(0x00); E(0x00);
  E(0xF0); E(0x08);
  E(0x9F); E(0x00); E(0x9C); E(0x7E);
  E(0xE8);
  E(0xC8);
  E(0x80); E(0xF3);
  E(0xAB);
  E(0x6B);

  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < 9; j++) {
      struct sr_bytes b = {0};
      sr_event_text_bytes(c, &b, descs[i][j]);
      int rom_offset = c->working_offset & 0xFFFF;
      int event_num = 0x94F + i * 9 + j;
      int eoo = 0xA0000 + (event_num - 0x400) * 2;
      P(eoo, rom_offset);
      P(eoo + 1, rom_offset >> 8);
      for (size_t k = 0; k < b.len; k++) E(b.p[k]);
      E(0x00);
    }
  }
  return 0;
}

static int sr_w1_get_elements_table(const struct sr_ctx *c) {
  const char *v = sr_work_get(c, "weaponElementsTableOffset");
  if (!v) return 0;
  return atoi(v);
}

/* MagicRebalance.cs (Moppleton) */
int sr_hack_magic_rebalance(struct sr_ctx *c) {
  if (!SB("magicRebalance")) return 0;

  P(0x102CD8 + 7, 0x03);

  P(0x102E18 + 7, 0x03);

  P(0x102E58 + 7, 0x02);

  P(0x102F98 + 7, 0x03);

  P(0x102FD8 + 7, 0x01);

  P(0x103058 + 7, 0x03);

  P(0x1030D8 + 7, 0x04);

  P(0x103158 + 7, 0x04);

  P(0x103358 + 7, 0x04);

  P(0x1033D8 + 2, 0x64);
  P(0x1033D8 + 7, 0x05);

  P(0x8EDBE, 0x4A);
  P(0x8EDBF, 0xEA);
  P(0x8EDC0, 0xEA);

  P(0x103418 + 7, 0x03);

  P(0x103458 + 7, 0x01);

  P(0x1034D8 + 7, 0x04);

  return 0;
}

/* ElementalDamage.cs (Moppleton): hooks the damage routine (C0/51FC, C0/5200 for weapon elements, C0/4A6F for enemy defenses). */
int sr_hack_elemental_damage(struct sr_ctx *c) {
  if (!SB("elemDamage")) return 0;
  int weapon_elements_location = sr_w1_get_elements_table(c);
  P(0x51FC, 0x22);
  P(0x51FD, c->working_offset);
  P(0x51FE, c->working_offset >> 8);
  P(0x51FF, (c->working_offset >> 16) + 0xC0);

  E(0xB9);
  E(0x95);
  E(0xE1);
  E(0xD0);
  E(0x05);
  E(0x64);
  E(0xD5);
  E(0xE2);
  E(0x20);
  E(0x6B);
  E(0xBD);
  E(0xA1);
  E(0xE1);
  E(0xD0);
  E(0x05);
  E(0x64);
  E(0xD5);
  E(0xE2);
  E(0x20);
  E(0x6B);
  E(0xB9);
  E(0x95);
  E(0xE1);
  E(0x3D);
  E(0xA1);
  E(0xE1);
  E(0xF0);
  E(0x13);
  E(0xA5);
  E(0xD4);
  E(0x4A);
  E(0x18);
  E(0x65);
  E(0xD4);
  E(0xC5);
  E(0xD4);
  E(0xB0);
  E(0x02);
  E(0xA9);
  E(0xFF);
  E(0x85);
  E(0xD4);
  E(0x64);
  E(0xD5);
  E(0xE2);
  E(0x20);
  E(0x6B);
  E(0xBD);
  E(0xA1);
  E(0xE1);
  E(0x29);
  E(0xAA);
  E(0xF0);
  E(0x06);
  E(0xBD);
  E(0xA1);
  E(0xE1);
  E(0x4A);
  E(0x80);
  E(0x04);
  E(0xBD);
  E(0xA1);
  E(0xE1);
  E(0x0A);
  E(0x39);
  E(0x95);
  E(0xE1);
  E(0xF0);
  E(0x05);
  E(0xA5);
  E(0xD4);
  E(0x4A);
  E(0x85);
  E(0xD4);
  E(0x64);
  E(0xD5);
  E(0xE2);
  E(0x20);
  E(0x6B);

  if (weapon_elements_location != -1) {

    P(0x5200, 0x22);
    P(0x5201, c->working_offset);
    P(0x5202, c->working_offset >> 8);
    P(0x5203, (c->working_offset >> 16) + 0xC0);

    E(0xC0);
    E(0x00);
    E(0x06);
    E(0x90);
    E(0x05);
    E(0xB9);
    E(0x9B);
    E(0xE1);
    E(0x0A);
    E(0x6B);

    E(0xB9);
    E(0x95);
    E(0xE1);
    E(0xF0);
    E(0x05);
    E(0xB9);
    E(0x9B);
    E(0xE1);
    E(0x0A);
    E(0x6B);

    E(0xDA);
    E(0xC2);
    E(0x20);
    E(0x48);
    E(0xB9);
    E(0xE3);
    E(0xE1);
    E(0x29);
    E(0xFF);
    E(0x00);
    E(0xAA);
    E(0x68);
    E(0xE2);
    E(0x20);
    E(0xBF);
    E(weapon_elements_location);
    E(weapon_elements_location >> 8);
    E((weapon_elements_location >> 16) + 0xC0);

    E(0xD0);
    E(0x06);
    E(0xFA);
    E(0xB9);
    E(0x9B);
    E(0xE1);
    E(0x0A);
    E(0x6B);
    E(0xFA);

    E(0x48);
    E(0x3D);
    E(0xA1);
    E(0xE1);
    E(0xF0);
    E(0x14);
    E(0xA5);
    E(0xD4);
    E(0x4A);
    E(0x18);
    E(0x65);
    E(0xD4);
    E(0xC5);
    E(0xD4);
    E(0xB0);
    E(0x02);
    E(0xA9);
    E(0xFF);
    E(0x85);
    E(0xD4);
    E(0x68);

    E(0xB9);
    E(0x9B);
    E(0xE1);
    E(0x0A);

    E(0x6B);
    E(0xBD);
    E(0xA1);
    E(0xE1);
    E(0x29);
    E(0xAA);
    E(0xF0);
    E(0x06);
    E(0xBD);
    E(0xA1);
    E(0xE1);
    E(0x4A);
    E(0x80);
    E(0x04);
    E(0xBD);
    E(0xA1);
    E(0xE1);
    E(0x0A);
    E(0x23);
    E(0x01);

    E(0xF0);
    E(0x05);
    E(0xA5);
    E(0xD4);
    E(0x4A);
    E(0x85);
    E(0xD4);
    E(0x68);

    E(0xB9);
    E(0x9B);
    E(0xE1);
    E(0x0A);

    E(0x6B);

  }

  P(0x4A6F, 0x22);
  P(0x4A70, c->working_offset);
  P(0x4A71, c->working_offset >> 8);
  P(0x4A72, (c->working_offset >> 16) + 0xC0);
  P(0x4A73, 0xEA);

  E(0x29);
  E(0x3F);
  E(0x9D);
  E(0xA1);
  E(0xE1);
  E(0xBD);
  E(0x93);
  E(0xE1);
  E(0x29);
  E(0x40);
  E(0xF0);
  E(0x08);
  E(0xBD);
  E(0xA1);
  E(0xE1);
  E(0x09);
  E(0x80);
  E(0x9D);
  E(0xA1);
  E(0xE1);
  E(0xBD);
  E(0x93);
  E(0xE1);
  E(0x29);
  E(0x80);
  E(0xF0);
  E(0x08);
  E(0xBD);
  E(0xA1);
  E(0xE1);
  E(0x09);
  E(0x40);
  E(0x9D);
  E(0xA1);
  E(0xE1);
  E(0x6B);

  return 0;
}
