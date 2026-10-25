/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Chaos mode (group w9): registry of the mode hacks, the small hacks, SellAllGear, FlammieDrum and the post-processing.  Derived from
 * (Author of every file: Moppleton): processing/chaos/ChaosGenerator.cs, processing/hacks/common/procgen/{SellAllGear,FlammieDrum,
 * StartingDoor,DeathEventDoorwayRemoval,MagDefAdjustment,EnemyBlackMagicPower}.cs and processing/hacks/common/music/CustomMusic.cs
 * (the flammie drum branch).  Translated to C 2026-10-18.
 *
 * The shared non-vanilla hacks (DoorExpansion, Layer3Changes, TimerDialogue, EnemyChanges, DropChanges, ShopChanges, MagicRope,
 * Statistics) are those of the boss rush port (sr_w8_*).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sr_core.h"
#include "sr_p2_bossrush.h"
#include "sr_p2_chaos.h"
#include "sr_p2_chaos_int.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))
#define SB(key) sr_settings_get_bool(c->settings, (key))

int w9_is_chaos(struct sr_ctx *c) { return !strcmp(sr_settings_get(c->settings, "mode"), "chaos"); }

int w9_num_floors(struct sr_ctx *c, int bossrush_value) {
  if (sr_p3_is_ac(c)) return sr_p3_ac_num_floors(c->settings);
  return w9_is_chaos(c) ? sr_w9_num_floors(c) : bossrush_value;
}

/* ----------------------------------------------------------------------------------------------------- small hacks */
static int hack_starting_door(struct sr_ctx *c) {
  P(0x751FD, 0x0A);   /* start at door 0x0A */
  return 1;
}

static int hack_death_event_doorway_removal(struct sr_ctx *c) {
  P(0xA98AF, 0x2F);
  P(0xA98B0, 0x00);
  return 1;
}

static int hack_mag_def_adjustment(struct sr_ctx *c) {
  double amount = 0.125;
  for (int i = 0; i < 62; i++) {
    int new_mgdef = (uint8_t)(amount * c->orig[0x103ed0 + 10 * i + 3]);
    if (new_mgdef > 255) new_mgdef = 255;
    P(0x103ed0 + 10 * i + 3, (uint8_t)new_mgdef);
  }
  return 1;
}

static int hack_enemy_black_magic_power(struct sr_ctx *c) {
  for (int i = 0; i < 128; i++) P(0x101C00 + 29 * i + 18, 0x38);
  return 1;
}

/* NamesOfThings.INDEX_CONSUMABLES_START = 0xBA */

/* ------------------------------------------------------------------------------------------------------- SellAllGear */
static void make_jump(struct sr_ctx *c, int source, int dest) { P(source + 1, (uint8_t)(dest - source - 2)); }

static void make_mul_two_thirds_subr(struct sr_ctx *c) {
  E(0x8F); E(0x04); E(0x42); E(0x00);   /* STA $4204 */
  E(0xE2); E(0x20);                     /* SEP 20 */
  E(0xA9); E(0x03);                     /* LDA #03 */
  E(0x8F); E(0x06); E(0x42); E(0x00);   /* STA $4206 */
  for (int i = 0; i < 8; i++) E(0xEA);  /* eight nops */
  E(0xC2); E(0x20);                     /* REP 20 */
  E(0xAF); E(0x14); E(0x42); E(0x00);   /* LDA $4214 */
  E(0x29); E(0xFF); E(0x00);            /* AND 00FF */
  E(0x0A);                              /* ASL */
  E(0x6B);                              /* RTL */
}

int sr_w9_hack_sell_all_gear(struct sr_ctx *c) {
  /* in ram: 7ecc24 hats, two msbs indicate equipped char; 7ecc30 armors; 7ecc3c accessories.  This establishes a custom event
   * subroutine that can be called from nekos and such. */
  int mul_two_thirds_subr = c->working_offset;
  make_mul_two_thirds_subr(c);

  int subr_start = c->working_offset;
  w8_register_event(c, "sell all", subr_start);

  E(0x08);              /* PHP */
  E(0xC2); E(0x30);     /* REP 30 */
  E(0x48);              /* PHA */
  E(0xDA);              /* PHX */
  E(0x5A);              /* PHY */

  for (int i = 0; i < 3; i++) {
    int item_offset1 = 0x7ECC24 + i * 12;
    int item_offset2 = item_offset1 + 1;
    E(0xE2); E(0x20);                 /* SEP 20 */
    E(0xA2); E(0x00); E(0x00);        /* LDX #0000 */
    int jump_dest_loop1 = c->working_offset;
    E(0xBF); E((uint8_t)item_offset1); E((uint8_t)(item_offset1 >> 8)); E((uint8_t)(item_offset1 >> 16));   /* LDA item,x */
    E(0x89); E(0xC0);                 /* BIT #C0 */
    E(0xF0); E(0x02);                 /* BEQ sell */
    int jump_source_endloop = c->working_offset;
    E(0x80); E(0x00);                 /* BRA endloop1 */
    E(0xCA);                          /* DEX */
    E(0xDA);                          /* PHX */
    E(0xC2); E(0x20);                 /* REP 20 */
    E(0x0A);                          /* ASL */
    E(0x29); E(0xFF); E(0x00);        /* AND 00FF */
    E(0xAA);                          /* TAX */
    E(0xBF); E(0xB4); E(0xFB); E(0xD8);   /* LDA D8FBB4,x */
    E(0x22); E((uint8_t)mul_two_thirds_subr); E((uint8_t)(mul_two_thirds_subr >> 8)); E((uint8_t)((mul_two_thirds_subr >> 16) + 0xC0));
    E(0x18);                          /* CLC */
    E(0x6F); E(0x6A); E(0xCC); E(0x7E);   /* ADC $7ECC6A */
    E(0x8F); E(0x6A); E(0xCC); E(0x7E);   /* STA $7ECC6A */
    E(0xE2); E(0x20);                 /* SEP 20 */
    E(0xA9); E(0x00);                 /* LDA #00 */
    E(0x6F); E(0x6C); E(0xCC); E(0x7E);   /* ADC $7ECC6C */
    E(0x8F); E(0x6C); E(0xCC); E(0x7E);   /* STA $7ECC6C */
    E(0xFA);                          /* PLX */
    E(0xDA);                          /* PHX */
    int jump_dest_startloop = c->working_offset;
    E(0xE0); E(0x0A); E(0x00);        /* CPX #000A */
    E(0xD0); E(0x08);                 /* BNE over */
    E(0xA9); E(0xFF);                 /* LDA #FF */
    E(0x9F); E((uint8_t)item_offset1); E((uint8_t)(item_offset1 >> 8)); E((uint8_t)(item_offset1 >> 16));   /* STA item,x */
    int jump_source_out1 = c->working_offset;
    E(0x80); E(0x00);                 /* BRA out */
    E(0xE0); E(0x0B); E(0x00);        /* CPX #000B */
    int jump_source_out2 = c->working_offset;
    E(0xF0); E(0x00);                 /* BEQ out */
    E(0xE8);                          /* INX */
    E(0xBF); E((uint8_t)item_offset2); E((uint8_t)(item_offset2 >> 8)); E((uint8_t)(item_offset2 >> 16));   /* LDA item+1,x */
    E(0x9F); E((uint8_t)item_offset1); E((uint8_t)(item_offset1 >> 8)); E((uint8_t)(item_offset1 >> 16));   /* STA item,x */
    int jump_source_start_loop = c->working_offset;
    E(0x80); E(0x00);                 /* BRA startloop */
    int jump_dest_out = c->working_offset;
    E(0xFA);                          /* PLX */
    int jump_dest_endloop1 = c->working_offset;
    E(0xE8);                          /* INX */
    E(0xE0); E(0x0C); E(0x00);        /* CPX #000C */
    int jump_source_loop1 = c->working_offset;
    E(0xD0); E(0x00);                 /* BNE loop1 */

    make_jump(c, jump_source_endloop, jump_dest_endloop1);
    make_jump(c, jump_source_out1, jump_dest_out);
    make_jump(c, jump_source_out2, jump_dest_out);
    make_jump(c, jump_source_start_loop, jump_dest_startloop);
    make_jump(c, jump_source_loop1, jump_dest_loop1);
  }

  E(0xC2); E(0x30);   /* REP 30 */
  E(0x7A);            /* PLY */
  E(0xFA);            /* PLX */
  E(0x68);            /* PLA */
  E(0x28);            /* PLP */
  E(0x6B);            /* RTL */
  return 1;
}

/* ---------------------------------------------------------------------------------------------------------- registry */
const struct sr_hack sr_w9_chaos_hacks[] = {
  {"Change starting door for non-vanilla mode", hack_starting_door, "StartingDoor", NULL},
  {"Door changes for non-vanilla modes", sr_w8_hack_door_expansion, "DoorExpansion", NULL},
  {"Custom event to sell all unused gear in non-vanilla modes", sr_w9_hack_sell_all_gear, "SellAllGear", NULL},
  {"Floor number and timer on layer 3", sr_w8_hack_layer3_changes, "Layer3Changes", NULL},
  {"Custom text printing events for non-vanilla modes", sr_w8_hack_timer_dialogue, "TimerDialogue", NULL},
  {"Chaos mode event generator", sr_w9_hack_chaos_events, "ChaosEventMaker", NULL},
  {"Chaos mode map generation & randomization", sr_w9_hack_chaos_randomizer, "ChaosRandomizer", NULL},
  {"Stat generation for non-vanilla modes", sr_w8_hack_enemy_changes, "EnemyChanges", NULL},
  {"Enemy drop changes for non-vanilla modes", sr_w8_hack_drop_changes, "DropChanges", NULL},
  {"Shop changes for non-vanilla modes", sr_w8_hack_shop_changes, "ShopChanges", NULL},
  {"Death event change for non-vanilla modes", hack_death_event_doorway_removal, "DeathEventDoorwayRemoval", NULL},
  {"Magic rope as un-stuck for non-vanilla modes", sr_w8_hack_magic_rope, "MagicRope", NULL},
  {"Decreased armor magic defense for non-vanilla modes", hack_mag_def_adjustment, "MagDefAdjustment", NULL},
  {"Fixed magic power for enemies in non-vanilla modes", hack_enemy_black_magic_power, "EnemyBlackMagicPower", NULL},
  {"Statistics tracker for non-vanilla modes", sr_w8_hack_statistics, "Statistics", NULL},
  {"Title screen changes", sr_hack_title_screen, "TitleScreenAddition", NULL},
  {"Music rando + imported music", sr_hack_custom_music, "CustomMusic", NULL},
};
const int sr_w9_chaos_hack_count = 17;

/* EnemyPositionAdjustments.process: "if (randoMode != rando && randoMode != open) return false" */
int sr_w9_noop_in_mode(const char *upstream, const char *mode) {
  if (!strcmp(upstream, "EnemyPositionAdjustments") && (!strcmp(mode, "chaos") || !strcmp(mode, "bossrush") || !strcmp(mode, "ancientcave"))) return 1;
  return 0;
}

/* ---------------------------------------------------------------------------------------------------- preflight */
int sr_w9_preflight(const struct sr_settings *s, const char *mode, const char *difficulty, char *why, size_t why_len) {
  (void)s;
  if (strcmp(mode, "chaos")) return 1;
  if (difficulty && difficulty[0] && strcmp(difficulty, "casual") && strcmp(difficulty, "hard") && strcmp(difficulty, "reallyhard")) {
    snprintf(why, why_len, "unsupported option: difficulty '%s' for chaos (supported: casual, hard, reallyhard)", difficulty);
    return 0;
  }
  return 1;
}

/* ---------------------------------------------------------------------------------------------------- post-processing */
void sr_w9_post_process(struct sr_ctx *c) {
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
