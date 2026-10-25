/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * OpenWorldGenerator: the mode hack list in registration order (Author: Moppleton), the preflight for Open World requests and the helper that
 * logs nested hacks.  Translated to C 2026-10-18.
 *
 * Every module is a strong, directly referenced symbol: this file lists each hack and each supports() function, so a static library link pulls
 * all of them in without --whole-archive or any other linker option.  sr_p4_preflight answers "supported" only when SR_P4_READY is defined as 1. */
#include <stdlib.h>
#include <string.h>

#include "sr_p4.h"
#include "sr_p4_ow.h"

const struct sr_hack sr_p4_open_hacks[] = {
  {"Open world character selection", sr_p4_hack_character_selection, "OpenWorldCharacterSelection", NULL},
  {"Open world goal processing", sr_p4_hack_goal_processor, "OpenWorldGoalProcessor", NULL},
  {"Open world class selection", sr_p4_hack_class_selection, "OpenWorldClassSelection", NULL},
  {"Spell orb element randomizer", sr_hack_element_swaps, "ElementSwaps", NULL},
  {"Boss randomizer", sr_hack_boss_swaps, "BossSwaps", NULL},
  {"Pick silly names for enemies", sr_p4_hack_silly_enemy_names, "OpenWorldSillyEnemyNamePicker", NULL},
  {"MTR seed selection", sr_p4_hack_mtr_seed_selection, "OpenWorldMtrSeedNumSelection", NULL},
  {"Non-boss enemy randomizer", sr_hack_enemy_swaps, "EnemySwaps", NULL},
  {"Music rando + imported music", sr_hack_custom_music, "CustomMusic", NULL},
  {"Ice country landability changes", sr_p4_hack_ice_country_fixes, "IceCountryLandableFixes", NULL},
  {"Open world Christmas changes", sr_p4_hack_xmas_hacks, "OpenWorldXmasHacks", NULL},
  {"Invisible passageways clear slow condition", sr_hack_invis_clears_slow, "InvisClearsSlow", NULL},
  {"Boss Z-position determination", sr_hack_boss_z_position, "BossZPosition", NULL},
  {"Open world difficulty value processor", sr_p4_hack_difficulty_processor, "OpenWorldDifficultyProcessor", NULL},
  {"Set colors and names of spell orbs", sr_p4_hack_crystal_orb_colors, "CrystalOrbColors", NULL},
  {"Open world events", sr_p4_hack_open_events, "OpenWorldEvents", NULL},
  {"Custom top of screen text", sr_p4_hack_custom_top_of_screen_text, "CustomTopOfScreenText", NULL},
  {"Open world enemy min/max level", sr_p4_hack_min_max_level, "MinMaxLevel", NULL},
  {"Enemy stats scaling/adjustment", sr_p4_hack_enemies_at_your_level, "EnemiesAtYourLevel", NULL},
  {"Open world randomizations", sr_p4_hack_open_randomizer, "OpenWorldRandomizer", NULL},
  {"Workaround for one issue with a fire palace door", sr_hack_fire_palace_door_fix, "FirePalaceDoorFix", NULL},
  {"Auto-save to slot 4 on door transitions", sr_hack_autosave, "Autosave", NULL},
  {"Open world shop randomizer", sr_p4_hack_shop_randomizer, "ShopRandomizer", NULL},
  {"Watts weapon level fix", sr_p4_hack_watts_weapon_fix, "WattsWeaponLevelFix", NULL},
  {"Random flavor for lime slime", sr_p4_hack_lime_slime_flavors, "LimeSlimeFlavors", NULL},
  {"Open world enemies only drop consumables and money", sr_p4_hack_drop_only_consumables, "DropOnlyConsumables", NULL},
  {"Only show armor as wearable if it's an improvement", sr_p4_hack_menu_icons_armor, "MenuIconsIfArmorImprovement", NULL},
  {"Magic rope refactor for vanilla maps", sr_p4_hack_magic_rope_refactor, "MagicRopeRefactor", NULL},
  {"Changes for character join", sr_p4_hack_character_join_changes, "CharacterJoinChanges", NULL},
  {"Map palette randomization", sr_p4_hack_map_palette_randomizations, "MapPaletteRandomizations", NULL},
  {"Enemy AI swap", sr_p4_hack_ai_swap, "AISwap", NULL},
  {"Black-out all map palettes", sr_p4_hack_black_map_palettes, "BlackMapPalettes", NULL},
  {"Random nesoberi with hint", sr_p4_hack_nesoberi, "Nesoberi", NULL},
  {"Automatic whip-post jumps if you have the whip", sr_p4_hack_fast_whip_posts, "FastWhipPosts", NULL},
  {"Indestructible rabite in the wind palace", sr_p4_hack_invincible_rabite, "InvincibleRabite", NULL},
  {"Remove title-screen menu auto-reset", sr_p4_hack_dont_auto_reset, "DontAutoResetAtFirstMenu", NULL},
  {"Character roles/classes", sr_p4_hack_character_classes, "CharacterClasses", NULL},
  {"Seed/hash display", sr_p4_hack_title_menu_seed_hash, "TitleMenuSeedHashDisplay", NULL},
};
const int sr_p4_open_hack_count = (int)(sizeof sr_p4_open_hacks / sizeof *sr_p4_open_hacks);

int sr_p4_name_skipped(const char *name) {
#ifdef SOMRANDO_DEBUG
  const char *e = getenv("SOMRANDO_SKIP_NAMES");
  if (!e || !e[0]) return 0;
  size_t n = strlen(name);
  while (*e) {
    const char *q = strchr(e, '|');
    size_t len = q ? (size_t)(q - e) : strlen(e);
    if (len == n && !strncmp(e, name, n)) return 1;
    if (!q) break;
    e = q + 1;
  }
#else
  (void)name;
#endif
  return 0;
}

int sr_p4_nested(struct sr_ctx *c, const char *name, sr_hack_fn fn) {
  int r = 0;
  if (!sr_p4_name_skipped(name)) r = fn(c);
  sr_hacklog(c, name, r);
  return r;
}

/* The mode is not reproducible until every module above is merged and verified. */
#ifndef SR_P4_READY
#define SR_P4_READY 1
#endif
int sr_p4_preflight(const struct sr_settings *s, const char *mode, const char *difficulty, char *why, size_t why_len) {
  (void)s; (void)difficulty;
  if (strcmp(mode, "open")) return 1;
  int dev = 0;
#ifdef SOMRANDO_DEBUG
  dev = getenv("SOMRANDO_P4_DEV") != NULL;
#endif
  if (!SR_P4_READY && !dev) {
    snprintf(why, why_len, "unsupported option: mode 'open' is not ported to C yet");
    return 0;
  }
  return sr_p4_sel_supports(s, why, why_len) && sr_p4_shared_supports(s, why, why_len) && sr_p4_core_supports(s, why, why_len) && sr_p4_inject_supports(s, why, why_len) && sr_p4_ev1_supports(s, why, why_len) && sr_p4_ev2_supports(s, why, why_len) && sr_p4_hlevel_supports(s, why, why_len) && sr_p4_hchar_supports(s, why, why_len) && sr_p4_hmisc_supports(s, why, why_len);
}
