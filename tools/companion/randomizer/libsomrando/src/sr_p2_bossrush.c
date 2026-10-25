/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Boss rush mode (group w8): registry of the mode hacks, shared state (generated maps, doors, map pieces, palettes, custom event
 * commands), the small hacks and the post-processing passes of RomGenerator.generate().  Derived from (Author of every file:
 * Moppleton): the processing/bossrush and processing/hacks/{bossrush,common/procgen} classes, and the common/util classes
 * CustomEventManager, DoorReplacer, FullMapReplacer, MapPaletteSetReplacer and MapPieceExpander.
 * Translated to C 2026-10-18.
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "sr_core.h"
#include "sr_p2_bossrush.h"
#include "sr_p2_bossrush_int.h"
#include "sr_p2_bossrush_data.h"
#include "sr_p2_chaos_int.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))
#define SB(key) sr_settings_get_bool(c->settings, (key))

/* ------------------------------------------------------------------------------------------------------ small helpers */
int w8_wget_int(struct sr_ctx *c, const char *key) {
  const char *s = sr_work_get(c, key);
  if (!s) sr_fail(c, SOMRANDO_ERR_FAILED, "Value cannot be null (workingData %s).", key);
  return (int)strtol(s, NULL, 10);
}

static int diff_column(const struct sr_ctx *c) {
  const char *d = c->req ? c->req->difficulty : NULL;
  if (!d || !d[0] || !strcmp(d, "casual")) return 0;
  if (!strcmp(d, "hard")) return 1;
  return 2;
}

double w8_diff(struct sr_ctx *c, const char *key) {
  if (sr_p3_is_ac(c)) {
    for (int i = 0; i < sr_p3_ac_diff_row_count; i++)
      if (!strcmp(sr_p3_ac_diff_rows[i].key, key)) return sr_p3_ac_diff_rows[i].v[diff_column(c)];
    sr_fail(c, SOMRANDO_ERR_FAILED, "unknown difficulty key %s", key);
  }
  if (w9_is_chaos(c)) {
    for (int i = 0; i < w9_chaos_diff_row_count; i++)
      if (!strcmp(w9_chaos_diff_rows[i].key, key)) return w9_chaos_diff_rows[i].v[diff_column(c)];
    sr_fail(c, SOMRANDO_ERR_FAILED, "unknown difficulty key %s", key);
  }
  for (int i = 0; i < sr_w8_diff_row_count; i++)
    if (!strcmp(sr_w8_diff_rows[i].key, key)) return sr_w8_diff_rows[i].v[diff_column(c)];
  sr_fail(c, SOMRANDO_ERR_FAILED, "unknown difficulty key %s", key);
}

int w8_diff_int(struct sr_ctx *c, const char *key) { return (int)w8_diff(c, key); }

int sr_w8_preflight(const char *mode, const char *difficulty, char *why, size_t why_len) {
  if (strcmp(mode, "bossrush")) return 1;
  if (!difficulty || !difficulty[0] || !strcmp(difficulty, "casual") || !strcmp(difficulty, "hard") || !strcmp(difficulty, "reallyhard")) return 1;
  snprintf(why, why_len, "unsupported option: difficulty '%s' for boss rush (supported: casual, hard, reallyhard)", difficulty);
  return 0;
}

/* ------------------------------------------------------------------------------------------------------------- state */
struct w8_state *w8_state_get(struct sr_ctx *c) {
  if (!c->w8) c->w8 = sr_alloc(c, sizeof(struct w8_state));
  return (struct w8_state *)c->w8;
}

/* replacementMapPieces[id] = data (a re-assignment keeps the original position) */
void w8_set_piece(struct sr_ctx *c, int id, const uint8_t *p, size_t n) {
  struct w8_state *st = w8_state_get(c);
  struct w8_piece_entry *e = NULL;
  for (int i = 0; i < st->n_pieces; i++)
    if (st->pieces[i].id == id) e = &st->pieces[i];
  if (!e) {
    if (st->n_pieces >= W8_MAX_PIECES) sr_fail(c, SOMRANDO_ERR_FAILED, "too many replacement map pieces");
    e = &st->pieces[st->n_pieces++];
    e->id = id;
  }
  memset(&e->data, 0, sizeof e->data);
  sr_bytes_push_n(c, &e->data, p, n);
}

struct w8_door_entry *w8_door_entry(struct sr_ctx *c, int id) {
  struct w8_state *st = w8_state_get(c);
  for (int i = 0; i < st->n_doors; i++)
    if (st->doors[i].id == id) return &st->doors[i];
  if (st->n_doors == st->cap_doors) {
    /* chaos holds pointers into this array: its sizes (< 1100) never grow it; only Ancient Cave does, and it keeps no pointers */
    int nc = st->cap_doors ? st->cap_doors * 2 : 1100;
    struct w8_door_entry *nd = (struct w8_door_entry *)sr_alloc(c, (size_t)nc * sizeof *nd);
    if (st->n_doors) memcpy(nd, st->doors, (size_t)st->n_doors * sizeof *nd);
    st->doors = nd;
    st->cap_doors = nc;
  }
  struct w8_door_entry *e = &st->doors[st->n_doors++];
  e->id = id;
  return e;
}

void w8_set_door(struct sr_ctx *c, int id, const uint8_t d[4]) { memcpy(w8_door_entry(c, id)->d, d, 4); }

void w8_set_pal(struct sr_ctx *c, int id, const uint8_t d[210]) {
  struct w8_state *st = w8_state_get(c);
  struct w8_pal_entry *e = NULL;
  for (int i = 0; i < st->n_pals; i++)
    if (st->pals[i].id == id) e = &st->pals[i];
  if (!e) {
    if (st->n_pals >= W8_MAX_PALS) sr_fail(c, SOMRANDO_ERR_FAILED, "too many replacement palettes");
    e = &st->pals[st->n_pals++];
    e->id = id;
  }
  memcpy(e->d, d, 210);
}

/* ------------------------------------------------------------------------------------------ CustomEventManager (3.. ids) */
int w8_register_event(struct sr_ctx *c, const char *name, int offset) {
  struct w8_state *st = w8_state_get(c);
  if (st->n_ev >= W8_MAX_CUSTOM_EVENTS) sr_fail(c, SOMRANDO_ERR_FAILED, "too many custom event commands");
  snprintf(st->ev_names[st->n_ev], sizeof st->ev_names[0], "%s", name);
  st->ev_offsets[st->n_ev] = offset;
  return 3 + st->n_ev++;
}

int w8_command_index(struct sr_ctx *c, const char *name) {
  struct w8_state *st = w8_state_get(c);
  for (int i = st->n_ev - 1; i >= 0; i--)
    if (!strcmp(st->ev_names[i], name)) return 3 + i;
  sr_fail(c, SOMRANDO_ERR_FAILED, "The given key '%s' was not present in the dictionary.", name);
}

void w8_custom_event_process(struct sr_ctx *c) {
  struct w8_state *st = w8_state_get(c);
  if (st->n_ev == 0) return;
  int new_code_offset = c->working_offset;
  P(0x185F, 0x22);
  P(0x1860, new_code_offset);
  P(0x1861, new_code_offset >> 8);
  P(0x1862, (new_code_offset >> 16) + 0xC0);
  for (int a = 0x1863; a <= 0x1875; a++) P(a, 0xEA);

  /* new subroutine: copy of 185F - 1876, but special-cased for X == 24 (param == 3) */
  E(0x5A);   /* phy */
  E(0xE0); E(0x24); E(0x00);   /* cpx #24 */
  E(0xB0); E(0x17);            /* bcs over this */
  E(0xA9); E(0x0C);
  E(0xA0); E(0x00); E(0x00);
  E(0xEB);
  E(0xBD); E(0x00); E(0xCC);
  E(0x99); E(0x2F); E(0xA2);
  E(0xE8);
  E(0xC8);
  E(0xEB);
  E(0x3A);
  E(0xD0); E(0xF3);
  E(0x99); E(0x2F); E(0xA2);
  E(0x7A);   /* ply */
  E(0x6B);   /* rtl */
  for (int i = 3; i < 3 + st->n_ev; i++) {
    int compare_value = i * 12;
    int jump_offset = st->ev_offsets[i - 3];
    E(0xE0); E(compare_value); E(compare_value >> 8);   /* CPX */
    E(0xD0); E(0x0A);                                    /* BNE over */
    E(0x22); E(jump_offset); E(jump_offset >> 8); E((jump_offset >> 16) + 0xC0);   /* JSL */
    E(0x7A);                                             /* PLY */
    E(0xA9); E(0x7E);                                    /* LDA #7E */
    E(0x48);                                             /* PHA */
    E(0xAB);                                             /* PLB */
    E(0x6B);                                             /* RTL */
  }
  /* default: just clean up, don't call anything */
  E(0x7A); E(0xA9); E(0x7E); E(0x48); E(0xAB); E(0x6B);
}

/* ----------------------------------------------------------------------------------------------------- small hacks */
/* common/other/StartingDoor.cs */
int sr_w8_hack_starting_door(struct sr_ctx *c) {
  P(0x751FD, 0x0A);   /* start at door 0x0A */
  return 1;
}

/* bossrush/ManaBeastDamageScaling.cs */
static int hack_mana_beast_damage_scaling(struct sr_ctx *c) {
  double ratio = W8_NUM_BOSS_ENTRIES / 64.0;
  P(0x10C0CD, (uint8_t)(0x59 * ratio));   /* flyby damage */
  P(0x10C0DB, (uint8_t)(0x5C * ratio));   /* zoom-in damage */
  P(0x10C0E2, (uint8_t)(0x78 * ratio));   /* fire attack damage */
  return 1;
}

/* common/other/DeathEventDoorwayRemoval.cs */
int sr_w8_hack_death_event_doorway_removal(struct sr_ctx *c) {
  P(0xA98AF, 0x2F);
  P(0xA98B0, 0x00);
  return 1;
}

/* common/enhancement/MagDefAdjustment.cs */
int sr_w8_hack_mag_def_adjustment(struct sr_ctx *c) {
  double amount = 0.125;
  for (int i = 0; i < 62; i++) {
    int new_mgdef = (uint8_t)(amount * c->orig[0x103ed0 + 10 * i + 3]);
    if (new_mgdef > 255) new_mgdef = 255;
    P(0x103ed0 + 10 * i + 3, (uint8_t)new_mgdef);
  }
  return 1;
}

/* common/procgen/EnemyBlackMagicPower.cs */
int sr_w8_hack_enemy_black_magic_power(struct sr_ctx *c) {
  for (int i = 0; i < 128; i++) P(0x101C00 + 29 * i + 18, 0x38);
  return 1;
}

/* bossrush/LimitMode7BossMp.cs; the C# writes 0x28458 three times, so only that address changes */
static int hack_limit_mode7_boss_mp(struct sr_ctx *c) {
  P(0x28F36, 0xEA);
  P(0x28F37, 0xEA);
  P(0x28F38, 0xEA);
  P(0x28458, 0xEA);
  return 1;
}

/* ---------------------------------------------------------------------------------------------------------- registry */
const struct sr_hack sr_w8_bossrush_hacks[] = {
  {"Change starting door for non-vanilla mode", sr_w8_hack_starting_door, "StartingDoor", NULL},
  {"Door changes for non-vanilla modes", sr_w8_hack_door_expansion, "DoorExpansion", NULL},
  {"Simplify lookup for animated map palettes", sr_w8_hack_animated_palette, "AnimatedPaletteSimplification", NULL},
  {"Floor number and timer on layer 3", sr_w8_hack_layer3_changes, "Layer3Changes", NULL},
  {"Custom text printing events for non-vanilla modes", sr_w8_hack_timer_dialogue, "TimerDialogue", NULL},
  {"Randomize boss order for boss rush", sr_w8_hack_boss_order, "BossOrderRandomizer", NULL},
  {"Event generator for boss rush mode", sr_w8_hack_boss_rush_events, "BossRushEventGenerator", NULL},
  {"Randomizer for boss rush mode", sr_w8_hack_bossrush_randomizer, "BossRushRandomizer", NULL},
  {"Reduction of mana beast power for boss rush", hack_mana_beast_damage_scaling, "ManaBeastDamageScaling", NULL},
  {"Stat generation for non-vanilla modes", sr_w8_hack_enemy_changes, "EnemyChanges", NULL},
  {"Enemy drop changes for non-vanilla modes", sr_w8_hack_drop_changes, "DropChanges", NULL},
  {"Shop changes for non-vanilla modes", sr_w8_hack_shop_changes, "ShopChanges", NULL},
  {"Starting level changes for boss rush mode", sr_w8_hack_starting_level, "StartingLevel", NULL},
  {"Death event change for non-vanilla modes", sr_w8_hack_death_event_doorway_removal, "DeathEventDoorwayRemoval", NULL},
  {"Magic rope as un-stuck for non-vanilla modes", sr_w8_hack_magic_rope, "MagicRope", NULL},
  {"Decreased armor magic defense for non-vanilla modes", sr_w8_hack_mag_def_adjustment, "MagDefAdjustment", NULL},
  {"Fixed magic power for enemies in non-vanilla modes", sr_w8_hack_enemy_black_magic_power, "EnemyBlackMagicPower", NULL},
  {"Statistics tracker for non-vanilla modes", sr_w8_hack_statistics, "Statistics", NULL},
  {"Limit MP absorb to 3 mp", sr_w8_hack_mp_absorb_limiter, "MpAbsorbLimiter", NULL},
  {"Remove unlimited MP for Mode7 bosses", hack_limit_mode7_boss_mp, "LimitMode7BossMp", NULL},
  {"Level weapons for everyone as soon as they are upgraded", sr_w8_hack_instant_weapon_levels, "InstantWeaponLevels", NULL},
  {"Title screen changes", sr_hack_title_screen, "TitleScreenAddition", NULL},
  {"Music rando + imported music", sr_hack_custom_music, "CustomMusic", NULL},
};
const int sr_w8_bossrush_hack_count = 23;

/* ---------------------------------------------------------------------------------------------------- post-processing */
void sr_w8_post_process(struct sr_ctx *c) {
  if (strcmp(sr_settings_get(c->settings, "mode"), "bossrush")) {
    if (c->custom_events > 0 || c->unsupported_containers > 0)
      sr_fail(c, SOMRANDO_ERR_UNSUPPORTED, "unsupported option: custom event commands, generated maps or doors");
    return;
  }
  w8_pieces_expand(c);
  sr_debug_dump_rom(c, "p102_pieces");
  w8_doors_replace(c);
  sr_debug_dump_rom(c, "p103_doors");
  w8_maps_replace(c);
  sr_debug_dump_rom(c, "p104_maps");
  w8_pals_replace(c);
  sr_debug_dump_rom(c, "p105_palettes");
  w8_custom_event_process(c);
}
