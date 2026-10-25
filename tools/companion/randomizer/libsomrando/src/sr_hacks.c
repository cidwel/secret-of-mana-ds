/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Ported hacks (all Author: Moppleton upstream), translated to C 2026-10-18.  Each function returns what RandoProcessor.process returns:
 * 1 when the hack was applied, 0 when it skipped itself.  The bytes written, their order and the working-offset layout are those of the
 * C# (the differential harness compares the whole ROM).  Upstream file of each hack:
 *
 *   TitleScreenAddition        processing/hacks/common/enhancement/TitleScreenAddition.cs
 *   NoSwordPullAnimation       processing/hacks/common/other/NoSwordPullAnimation.cs
 *   HexasFix                   processing/hacks/common/other/HexasFix.cs
 *   OrbRewardEventGenerator    processing/hacks/common/other/OrbRewardEventGenerator.cs
 *   HighLevelEnemyNameFix      processing/hacks/common/fix/HighLevelEnemyNameFix.cs
 *   NumbersOnScreen            processing/hacks/common/enhancement/NumbersOnScreen.cs
 *   DamageCancelFix            processing/hacks/common/fix/DamageCancelFix.cs
 *   Vineshroom                 processing/hacks/common/other/Vineshroom.cs
 *   PlayerAiDefaultChanges     processing/hacks/common/other/PlayerAiDefaultChanges.cs
 *   WeaponRandomizer (off)     processing/hacks/common/enhancement/WeaponRandomizer.cs   (only the "do not randomize" exit)
 *   InitialValues (empty)      processing/hacks/common/util/InitialValues.cs             (only the "nothing to set" exit)
 *   ElementSwaps               processing/common/ElementSwaps.cs
 *   FasterDialogueEvents       processing/vanillarando/FasterDialogueEvents.cs           (only the no-shortening branch)
 *   BossSwaps (bypass)         processing/common/BossSwaps.cs                            (only the bypass exit, with its name changes)
 *   EnemySwaps (off)           processing/common/EnemySwaps.cs                           (only the "no enemy swaps" exit)
 *   InvisClearsSlow            processing/common/InvisClearsSlow.cs
 *   BossZPosition              processing/hacks/common/fix/BossZPosition.cs
 *   ExperienceAdjust           processing/hacks/common/other/ExperienceAdjust.cs
 *   GoldAdjust                 processing/hacks/common/other/GoldAdjust.cs
 *   EnemyScaling (100%)        processing/hacks/vanillarando/EnemyScaling.cs             (only the "no scale changes" exit)
 *   FirePalaceDoorFix          processing/hacks/common/other/FirePalaceDoorFix.cs
 *
 * The hacks that are not ported are listed in sr_registry.c with the option values for which the C# skips them.
 */
#include <math.h>
#include <string.h>

#include "sr_core.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))
#define S(key) sr_settings_get(c->settings, (key))
#define SB(key) sr_settings_get_bool(c->settings, (key))

static void bad_mode(struct sr_ctx *c, const char *what) { sr_fail(c, SOMRANDO_ERR_UNSUPPORTED, "unsupported option: %s", what); }

/* ------------------------------------------------------------------------------------------------ TitleScreenAddition */
int sr_hack_title_screen(struct sr_ctx *c) {
  const char *mode = S("mode");
  const char *title;
  if (!strcmp(mode, "open")) title = "open world";
  else if (!strcmp(mode, "rando")) title = "randomizer";
  else if (!strcmp(mode, "ancientcave")) title = "ancient cave";
  else if (!strcmp(mode, "bossrush")) title = "boss rush";
  else if (!strcmp(mode, "chaos")) title = "chaos mode";
  else { bad_mode(c, "title screen text for this mode"); return 0; }

  struct sr_bytes code = {0}, placements = {0};
  sr_lz77_decode(c, c->orig, 0x77C00, &code);
  sr_lz77_decode(c, c->orig, 0x7B480, &placements);

  int string_pos = 0x165D;
  for (int i = (int)strlen(title) - 1; i >= 0; i--) {
    uint8_t val = 0x60;
    char ch = title[i];
    if (ch >= 'A' && ch <= 'Z') val = (uint8_t)(0x61 + (ch - 'A'));
    if (ch >= 'a' && ch <= 'z') val = (uint8_t)(0x61 + (ch - 'a'));
    if (ch == '.') val = 0x7B;
    if (string_pos < 0 || (size_t)string_pos >= placements.len) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range (title screen).");
    placements.p[string_pos] = val;
    string_pos--;
  }
  if (placements.len <= 0x1B7F || code.len <= 11673) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range (title screen).");
  /* also moppleton */
  placements.p[0x1B5A] = 0x61; placements.p[0x1B5B] = 0x6C; placements.p[0x1B5C] = 0x73; placements.p[0x1B5D] = 0x6F;
  placements.p[0x1B77] = 0x6D; placements.p[0x1B78] = 0x6F; placements.p[0x1B79] = 0x70; placements.p[0x1B7A] = 0x70;
  placements.p[0x1B7B] = 0x6C; placements.p[0x1B7C] = 0x65; placements.p[0x1B7D] = 0x74; placements.p[0x1B7E] = 0x6F;
  placements.p[0x1B7F] = 0x6E;

  struct sr_bytes recomp_placements = {0};
  sr_lz77_encode(c, placements.p, placements.len, 4, &recomp_placements);
  sr_ensure_space_in_bank(&c->working_offset, (int)recomp_placements.len);
  code.p[11669] = (uint8_t)((c->working_offset >> 16) + 0xC0);
  code.p[11672] = (uint8_t)c->working_offset;
  code.p[11673] = (uint8_t)(c->working_offset >> 8);
  for (size_t i = 0; i < recomp_placements.len; i++) E(recomp_placements.p[i]);

  struct sr_bytes recomp_code = {0};
  sr_lz77_encode(c, code.p, code.len, 1, &recomp_code);
  sr_ensure_space_in_bank(&c->working_offset, (int)recomp_code.len);
  P(0x14D3C, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x14D3E, (uint8_t)c->working_offset);
  P(0x14D3F, (uint8_t)(c->working_offset >> 8));
  for (size_t i = 0; i < recomp_code.len; i++) E(recomp_code.p[i]);
  return 1;
}

/* ------------------------------------------------------------------------------------------------ small fixed patches */
int sr_hack_no_sword_pull(struct sr_ctx *c) {
  for (int i = 0; i < 4; i++) P(0x1EC78 + i, 0xEA);
  return 1;
}

int sr_hack_hexas_fix(struct sr_ctx *c) {
  P(0x20641, 0xEA);
  P(0x20642, 0xEA);
  return 1;
}

int sr_hack_player_ai_default(struct sr_ctx *c) {
  P(0x57B7, 0x00); P(0x57B8, 0x00); P(0x57B9, 0x00);   /* quadrants */
  P(0x57BA, 0x00); P(0x57BB, 0x00); P(0x57BC, 0x00);   /* ai position */
  P(0x75205, 0x00);                                    /* ai grid window position to match */
  return 1;
}

int sr_hack_vineshroom(struct sr_ctx *c) {
  const int offset = 0x190500;
  size_t len = 0;
  const uint8_t *tiles = sr_rom_res(c, "vineshroom_tiles.bin", &len);
  for (size_t i = 0; i < len; i++) P(offset + (int)i, tiles[i]);
  return 1;
}

int sr_hack_high_level_enemy_name_fix(struct sr_ctx *c) {
  P(0x10D43B, 0x22);
  P(0x10D43C, (uint8_t)c->working_offset);
  P(0x10D43D, (uint8_t)(c->working_offset >> 8));
  P(0x10D43E, (uint8_t)((c->working_offset >> 16) + 0xC0));
  /* replaced code */
  E(0xBF); E(0x80); E(0x01); E(0x7E);
  /* AND #00FF */
  E(0x29); E(0xFF); E(0x00);
  E(0x6B);   /* RTL */
  return 1;
}

/* ---------------------------------------------------------------------------------------------------- NumbersOnScreen */
static void numbers_axis(struct sr_ctx *c, int patch_at, int adc_lo, int store_zp, int lower, int upper) {
  P(patch_at, 0x22);
  P(patch_at + 1, (uint8_t)c->working_offset);
  P(patch_at + 2, (uint8_t)(c->working_offset >> 8));
  P(patch_at + 3, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(patch_at + 4, 0xEA);
  /* (replaced code) ADC $E0xx,x */
  E(0x7D); E(adc_lo); E(0xE0);
  E(0x48);                        /* PHA */
  E(0xBD); E(0x75); E(0xE1);      /* LDA $E175,x */
  E(0x29); E(0xFF); E(0x00);      /* AND #00FF */
  E(0xC9); E(0xD8); E(0x00);      /* CMP #00D8 */
  E(0xF0); E(0x0F);               /* BEQ showingHp */
  E(0xBD); E(0x4F); E(0xE0);      /* LDA E04F,x */
  E(0x29); E(0xFF); E(0x00);      /* AND #00FF */
  E(0xC9); E(0xD0); E(0x00);      /* CMP #00D0 */
  E(0xF0); E(0x04);               /* BEQ showingHp */
  E(0x68);                        /* PLA */
  E(0x85); E(store_zp);           /* (removed code) STA $04 / $06 */
  E(0x6B);                        /* RTL */
  /* showingHp: */
  E(0x68);                        /* PLA */
  E(0xC9); E(0x00); E(0x80);      /* CMP #8000 - negative? */
  E(0x90); E(0x03);               /* BCC past loading the minimum */
  E(0xA9); E(lower); E(0x00);     /* LDA lower */
  E(0xC9); E(upper); E(0x00);     /* CMP upper */
  E(0x90); E(0x03);               /* BCC past loading the maximum */
  E(0xA9); E(upper); E(0x00);     /* LDA upper */
  E(0xC9); E(lower); E(0x00);     /* CMP lower */
  E(0xB0); E(0x03);               /* BCS past loading the minimum */
  E(0xA9); E(lower); E(0x00);     /* LDA lower */
  E(0x85); E(store_zp);           /* (replaced code) STA $04 / $06 */
  E(0x6B);                        /* RTL */
}

int sr_hack_numbers_on_screen(struct sr_ctx *c) {
  /* from the FreeWalking hack: edges of screen */
  const int x_lower = 0x08, x_upper = 0xE0, y_lower = 0x00, y_upper = 0xC8;
  numbers_axis(c, 0xE52D, 0x20, 0x04, x_lower, x_upper);
  numbers_axis(c, 0xE543, 0x22, 0x06, y_lower, y_upper);
  return 1;
}

/* ------------------------------------------------------------------------------------------------------ DamageCancelFix */
int sr_hack_damage_cancel_fix(struct sr_ctx *c) {
  const char *allow = S("dmgCancelAllow");
  if (!strcmp(allow, "all")) return 0;   /* vanilla behavior; change nothing */
  if (!strcmp(allow, "none")) {
    for (int i = 0; i < 6; i++) P(0x550c + i, 0xEA);
  } else {
    sr_ensure_space_in_bank(&c->working_offset, 20);
    P(0x550c, 0x22);
    P(0x550d, (uint8_t)c->working_offset);
    P(0x550e, (uint8_t)(c->working_offset >> 8));
    P(0x550f, (uint8_t)((c->working_offset >> 16) + 0xC0));
    P(0x5510, 0xEA);
    P(0x5511, 0xEA);
    E(0xC9); E(0x10);   /* CMP #10 */
    E(0xD0); E(0x01);   /* BNE over10 */
    E(0x6B);            /* RTL */
    E(0xC9); E(0x12);
    E(0xD0); E(0x01);
    E(0x6B);
    E(0x9E); E(0xF1); E(0xE1);   /* STZ $E1F1,x */
    E(0x9E); E(0xF2); E(0xE1);   /* STZ $E1F2,x */
    E(0x6B);
  }
  return 1;
}

/* ------------------------------------------------------------------------------------------------- OrbRewardEventGenerator */
static const char *weapon_name(int w) {
  static const char *n[] = {"Glove", "Sword", "Axe", "Spear", "Whip", "Bow", "Boomerang", "Javelin"};
  return (w >= 0 && w < 8) ? n[w] : "";
}

static void orb_process_swaps(struct sr_ctx *c, int swaps[8]) {
  int orb_nums[7] = {0, 2, 3, 4, 5, 6, 7};
  int count = 7;
  swaps[1] = 1;
  int old_id = 0;
  while (count > 0) {
    int new_id = sr_random_next(&c->rf) % count;
    swaps[old_id] = orb_nums[new_id];
    old_id++;
    if (old_id == 1) old_id++;
    for (int i = new_id; i < count - 1; i++) orb_nums[i] = orb_nums[i + 1];   /* RemoveAt */
    count--;
  }
}

static void orb_process_chest_swaps(struct sr_ctx *c, const int swaps[8]) {
  for (int map_num = 16; map_num < 500; map_num++) {
    int start = sr_vanilla_object_offset(c->orig, map_num) + 8;   /* skip header */
    int end = sr_vanilla_object_offset(c->orig, map_num + 1);
    int obj_num = 0;
    for (int i = start; i < end; i += 8, obj_num++) {
      struct sr_map_object obj;
      memcpy(obj.d, c->orig + i, 8);
      int vis = sr_obj_get_vis_flag(&obj);
      if (vis >= 0xB8 && vis <= 0xBF) {
        sr_obj_set_vis_flag(&obj, (uint8_t)(swaps[vis - 0xB8] + 0xB8));
        int dest = sr_vanilla_object_offset(c->out, map_num) + 8 + obj_num * 8;
        for (int k = 0; k < 8; k++) P(dest + k, obj.d[k]);
      }
    }
  }
}

static void orb_process_event_changes(struct sr_ctx *c, int orb_reward_fix, const int swaps[8]) {
  for (int i = 0; i < 8; i++) {
    uint8_t flag1 = (uint8_t)(0xb8 + swaps[i]);
    uint8_t flag2 = (uint8_t)(0xc0 + swaps[i]);
    struct sr_bytes *ev = sr_event_new(c, 0x500 + i);
    uint8_t cmd[2];
    if (orb_reward_fix) {
      sr_ev_incr_cmd(flag1, cmd);
      sr_ev_logic(c, ev, flag1, 0x0, 0x8, cmd);   /* just ignore getting more than 8 */
    } else {
      sr_ev_jump_cmd(0x508, cmd);
      sr_ev_logic(c, ev, flag1, 0x9, 0xF, cmd);   /* vanilla behavior */
    }
    sr_ev_incr_cmd(flag2, cmd);
    sr_ev_logic(c, ev, flag2, 0x0, 0x8, cmd);
    sr_bytes_push(c, ev, EV_OPEN_DIALOGUE);
    sr_bytes_push(c, ev, 0x7F);   /* enter */
    sr_bytes_push(c, ev, EV_MULTI_SPACE);
    sr_bytes_push(c, ev, (uint8_t)(8 - (int)strlen(weapon_name(swaps[i])) / 2));   /* text shift */
    char text[32];
    snprintf(text, sizeof text, "Got %s", weapon_name(swaps[i]));
    sr_ev_add_dialogue(c, ev, text);
    sr_bytes_push(c, ev, EV_JUMP_BASE + 0x05);
    sr_bytes_push(c, ev, 0x09);   /* 's orb */
    sr_ev_end(c, ev);
  }
}

int sr_hack_orb_reward_events(struct sr_ctx *c) {
  int orb_reward_fix = SB("orbFix");
  const char *mode = S("mode");
  int swaps[8];
  for (int i = 0; i < 8; i++) swaps[i] = i;
  if (!strcmp(mode, "open")) {
    /* upstream draws the swaps twice and keeps the second result */
    orb_process_swaps(c, swaps);
    orb_process_swaps(c, swaps);
    orb_process_event_changes(c, orb_reward_fix, swaps);
    return 1;
  }
  if (strcmp(mode, "rando")) {
    if (strcmp(mode, "bossrush") && strcmp(mode, "chaos") && strcmp(mode, "ancientcave")) { bad_mode(c, "orb reward events outside the vanilla randomizer, boss rush, chaos and ancient cave"); return 0; }
    if (!orb_reward_fix) return 0;   /* "No changes to be done for weapon orb reward events." */
    orb_process_event_changes(c, 1, swaps);
    return 1;
  }
  if (SB("rOrbs")) {
    orb_process_swaps(c, swaps);
    /* for vanilla rando, also swap which event flags control visibility of which chests with orbs in them */
    orb_process_chest_swaps(c, swaps);
  }
  orb_process_event_changes(c, orb_reward_fix, swaps);
  return 1;
}

/* ------------------------------------------------------------------------------------------------------- ElementSwaps */
int sr_p4_shared_element_open(struct sr_ctx *c);   /* sr_p4_shared_element.c */

int sr_hack_element_swaps(struct sr_ctx *c) {
  if (!strcmp(S("mode"), "open")) return sr_p4_shared_element_open(c);
  if (strcmp(S("mode"), "rando")) { bad_mode(c, "element randomizer outside the vanilla randomizer"); return 0; }
  const int modify_existing = !SB("rDialogue");
  uint8_t *rom = c->out;

  static const int map_nums[8][2] = {{314, 276}, {278, 0}, {194, 0}, {273, 0}, {120, 0}, {375, 0}, {351, 0}, {431, 0}};
  static const int obj_nums[8][2] = {{0, 2}, {0, 0}, {0, 0}, {2, 0}, {2, 0}, {2, 0}, {2, 0}, {2, 0}};
  static const int map_counts[8] = {2, 1, 1, 1, 1, 1, 1, 1};

  int original[8], n_orig = 8, new_eles[8], n_new = 0;
  for (int i = 0; i < 8; i++) original[i] = i;
  while (n_orig > 0) {
    int id = sr_random_next(&c->rf) % n_orig;
    new_eles[n_new++] = original[id];
    for (int i = id; i < n_orig - 1; i++) original[i] = original[i + 1];
    n_orig--;
  }

  /* change npcs */
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j < map_counts[i]; j++) {
      int objs_offset = 0x80000 + sr_peek(c, 0x87000 + map_nums[i][j] * 2) + (sr_peek(c, 0x87000 + map_nums[i][j] * 2 + 1) << 8);
      objs_offset += 8;                         /* skip header */
      objs_offset += obj_nums[i][j] * 8;        /* skip other objs */
      P(objs_offset + 5, (uint8_t)(new_eles[i] + 0x90));   /* set npc graphic */
    }
  }

  /* change orb spells */
  static const int event_conversions[8] = {0, 1, 3, 2, 4, 5, 6, 7};
  struct patch { int ev, ele, off_a, off_b; } patches[] = {
    {0x27e, 0, 8, 9},   /* gnome orb in matango cave */
    {0x239, 1, 8, 9},   /* undine orb in gaia's navel */
    {0x6ce, 1, 3, 4},   /* undine orb in fire palace */
    {0x25a, 3, 3, 4},   /* sylphid orb in upper land */
    {0x2c8, 2, 3, 4},   /* salamando orb in fire palace entrance */
    {0x6cb, 2, 3, 4},   /* salamando orb in fire palace */
    {0x2c4, 4, 3, 4},   /* lumina orb in luna palace */
  };
  for (size_t k = 0; k < sizeof patches / sizeof *patches; k++) {
    int base = sr_event_start_offset(c, rom, patches[k].ev);
    int conv = event_conversions[new_eles[patches[k].ele]];
    P(base + 1, (uint8_t)(0x50 + conv));
    if (patches[k].off_a == 8) {
      P(base + 8, (uint8_t)(0x98 + conv));
      P(base + 9, 0x13);
    } else {
      P(base + 3, (uint8_t)(0x98 + conv));
      P(base + 4, 0x13);
    }
  }

  /* if not generating new events for dialogue shortening, inject the randomized elements' spells into the existing events */
  if (modify_existing) {
    static const int attribute_values[8] = {0xC8, 0xC9, 0xCA, 0xCB, 0xCF, 0xCE, 0xCC, 0xCD};
    static const int girl_offsets[8] = {0xA6B90, 0xA6E35, 0xA6F60, 0xA6F6A, 0xA7218, 0xA708C, 0xA6F9D, 0xA7063};
    static const int sprite_offsets[8] = {0xA6B94, 0xA6E39, 0xA6F64, 0xA6F6E, 0xA721C, 0xA7090, 0xA6FA1, 0xA7067};
    for (int i = 0; i < 8; i++) {
      int new_ele = event_conversions[new_eles[i]];
      int attr = attribute_values[new_ele];
      P(girl_offsets[i], attr);
      P(sprite_offsets[i], attr);
      if (new_ele == 4) { P(girl_offsets[i] + 1, 0x38); P(sprite_offsets[i] + 1, 0); }          /* lumina */
      else if (new_ele == 5) { P(girl_offsets[i] + 1, 0); P(sprite_offsets[i] + 1, 0x07); }    /* shade */
      else if (new_ele == 7) { P(girl_offsets[i] + 1, 0x18); P(sprite_offsets[i] + 1, 0x03); } /* dryad */
      else { P(girl_offsets[i] + 1, 0x38); P(sprite_offsets[i] + 1, 0x07); }                   /* normal */
    }
  }

  sr_work_set_int_array(c, "vanillaRandoElementList", new_eles, 8);
  return 1;
}

/* ------------------------------------------------------------------------------------------------ FasterDialogueEvents */

/* ------------------------------------------------------------------------------------------ BossSwaps / EnemySwaps exits */


/* ----------------------------------------------------------------------------------------------------- InvisClearsSlow */
int sr_hack_invis_clears_slow(struct sr_ctx *c) {
  P(0x1EA26, 0x22);
  P(0x1EA27, (uint8_t)c->working_offset);
  P(0x1EA28, (uint8_t)(c->working_offset >> 8));
  P(0x1EA29, (uint8_t)((c->working_offset >> 16) + 0xC0));
  for (int i = 0; i < 4; i++) P(0x1EA2A + i, 0xEA);
  /* original code first */
  E(0xAD); E(0x0E); E(0xE0); E(0x49); E(0x80); E(0x8D); E(0x0E); E(0xE0);
  /* clear the slow bit (0x04) of the status byte of each of the three characters */
  static const int status[3] = {0xE190, 0xE390, 0xE590};
  for (int i = 0; i < 3; i++) {
    E(0xAD); E(status[i] & 0xFF); E(status[i] >> 8);   /* LDA */
    E(0x29); E(0xFB);                                  /* AND FB */
    E(0x8D); E(status[i] & 0xFF); E(status[i] >> 8);   /* STA */
  }
  E(0x6B);   /* RTL */
  return 1;
}

/* ------------------------------------------------------------------------------------------------------- BossZPosition */
int sr_hack_boss_z_position(struct sr_ctx *c) {
  if (SB("bossFixedZ")) {
    for (int i = 0; i < 3; i++) P(0x216B0 + i, 0xEA);
    for (int i = 0; i < 3; i++) P(0x2750E + i, 0xEA);
    return 1;
  }
  sr_ensure_space_in_bank(&c->working_offset, 500);
  /* hydras, snakes, tropicallos, spring beaks */
  static const uint8_t really_broken[8] = {92 /*jabber*/, 109 /*hydra*/, 105 /*great viper*/, 123 /*dragon worm*/,
                                           89 /*tropicallo*/, 104 /*boreal*/, 118 /*axe beak*/, 93 /*spring beak*/};
  int really_broken_subr = c->working_offset;
  for (int i = 0; i < 8; i++) {
    E(0xE2); E(0x20);                 /* SEP 20 */
    E(0xBD); E(0xE7); E(0x01);        /* LDA 01E7,x - load species */
    E(0xC9); E(really_broken[i]);     /* CMP #bossNum */
    E(0xD0); E(0x08);                 /* BNE next */
    E(0xA9); E(0x00);                 /* LDA #00 */
    E(0x9D); E(0x0B); E(0x00);        /* STA $000B,x */
    E(0xC2); E(0x20);                 /* REP 20 */
    E(0x6B);                          /* RTL */
  }
  E(0xC2); E(0x20);
  E(0x6B);

  P(0x216B0, 0x22);
  P(0x216B1, (uint8_t)c->working_offset);
  P(0x216B2, (uint8_t)(c->working_offset >> 8));
  P(0x216B3, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x216B4, 0xEA);

  /* (removed code) STA $000B,x ; REP #$20 */
  E(0x9D); E(0x0B); E(0x00);
  E(0xC2); E(0x20);
  E(0x48);   /* PHA */
  E(0x22); E((uint8_t)really_broken_subr); E((uint8_t)(really_broken_subr >> 8)); E((uint8_t)((really_broken_subr >> 16) + 0xC0));
  E(0x68);   /* PLA */
  E(0x6B);   /* RTL */

  /* the snake uses this one */
  P(0x2750e, 0x22);
  P(0x2750f, (uint8_t)c->working_offset);
  P(0x27510, (uint8_t)(c->working_offset >> 8));
  P(0x27511, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x27512, 0xEA);
  P(0x27513, 0xEA);

  E(0x9D); E(0x0B); E(0x00);   /* STA $000B,x */
  E(0x9E); E(0x66); E(0x00);   /* STZ $0066,x */
  E(0x48);
  E(0x22); E((uint8_t)really_broken_subr); E((uint8_t)(really_broken_subr >> 8)); E((uint8_t)((really_broken_subr >> 16) + 0xC0));
  E(0x68);
  E(0x6B);
  return 1;
}

/* --------------------------------------------------------------------------------------------- Experience / Gold scale */
static double multiplier_from(struct sr_ctx *c, const char *key) {
  const char *m = S(key);
  if (!strcmp(m, "half")) return 0.5;
  if (!strcmp(m, "double")) return 2.0;
  if (!strcmp(m, "triple")) return 3.0;
  return 1.0;
}

static int scale_enemy_stat(struct sr_ctx *c, const char *key, const char *open_key, int index) {
  const int is_open = !strcmp(S("mode"), "open");
  if (!is_open && strcmp(S("mode"), "rando")) { bad_mode(c, "enemy reward scale outside the vanilla randomizer"); return 0; }
  double mul = is_open ? sr_settings_get_double(c->settings, open_key) : multiplier_from(c, key);
  if (mul == 1.0) return 0;
  for (int i = 0; i <= 127; i++) {
    int at = 0x101C00 + i * 29 + index;
    int orig = sr_ushort_from_bytes(c->out, at);
    int nv = sr_clamp_to_endpoints((int)ceil(orig * mul), 0, 65535);
    sr_ushort_to_bytes(c->out, at, nv);
  }
  return 1;
}

int sr_hack_experience_adjust(struct sr_ctx *c) { return scale_enemy_stat(c, "rExp", "opExp", 16); }
int sr_hack_gold_adjust(struct sr_ctx *c) { return scale_enemy_stat(c, "rGold", "opGold", 0x1B); }


/* ----------------------------------------------------------------------------------------------- FirePalaceDoorFix */
int sr_hack_fire_palace_door_fix(struct sr_ctx *c) {
  if (strcmp(S("mode"), "rando") && strcmp(S("mode"), "open")) { bad_mode(c, "fire palace door fix for this mode"); return 0; }
  /* door 151 to map 347 [x=16 y=29], from the original ROM */
  struct sr_door door;
  memcpy(door.d, c->orig + 0x83000 + 0x151 * 4, 4);
  sr_door_set_ypos(&door, (uint8_t)(sr_door_get_ypos(&door) - 6));
  for (int i = 0; i < 4; i++) P(0x83000 + 0x151 * 4 + i, door.d[i]);

  /* move the minotaur up too: object 0 of map 347 (read from the output ROM) */
  const int MAPNUM_MINOTAUR_ARENA = 347;
  int start = sr_vanilla_object_offset(c->out, MAPNUM_MINOTAUR_ARENA) + 8;
  int end = sr_vanilla_object_offset(c->out, MAPNUM_MINOTAUR_ARENA + 1);
  if (start >= end) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range (minotaur arena objects).");
  struct sr_map_object minotaur;
  memcpy(minotaur.d, c->out + start, 8);
  sr_obj_set_ypos(&minotaur, (uint8_t)(sr_obj_get_ypos(&minotaur) - 3));
  for (int i = 0; i < 8; i++) P(start + i, minotaur.d[i]);
  return 1;
}

/* ------------------------------------------------------------------------------- WeaponRandomizer / InitialValues exits */

int sr_hack_initial_values(struct sr_ctx *c) {
  return sr_w3_initial_values_hack(c);   /* BuyMultipleItems fills context.initialValues */
}
