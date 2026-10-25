/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Ancient Cave mode (group p3): registry of the mode hacks, registry of the floor generators and the preflight.  Derived from
 * processing/ancientcave/{AncientCaveGenerator,AncientCaveRandomizer}.cs (Author: Moppleton).  Translated to C 2026-10-18.
 */
#include <stdio.h>
#include <string.h>

#include "sr_p3_ac.h"
#include "sr_p3_ac_mode.h"
#include "sr_p2_chaos_int.h"

extern const struct ac_biome sr_p3_forest_biome, sr_p3_ruins_biome, sr_p3_island_biome, sr_p3_cave_biome, sr_p3_manafort_biome, sr_p3_test_biome;

/* mapGenerators in Dictionary insertion order */
#ifdef SOMRANDO_DEBUG
const struct ac_biome *const sr_p3_biomes[6] = {&sr_p3_forest_biome, &sr_p3_ruins_biome, &sr_p3_island_biome, &sr_p3_cave_biome, &sr_p3_manafort_biome, &sr_p3_test_biome};
#else
const struct ac_biome *const sr_p3_biomes[6] = {&sr_p3_forest_biome, &sr_p3_ruins_biome, &sr_p3_island_biome, &sr_p3_cave_biome, &sr_p3_manafort_biome, NULL};
#endif

/* Which pieces are really ported (the stub files contain the text STUB and refuse the request half way, so a request that needs one is
 * refused up front instead).  Set an entry to 1 when the file of the biome stops being a stub.  Order of sr_p3_biomes: forest, ruins,
 * island, cave, manafortinterior, test (the test generator only exists in debug builds and is always ready). */
static const int biome_ready[6] = {1, 1, 1, 1, 1, 1};
/* sr_p3_events.c (AncientCaveEventMaker) */
static const int event_maker_ready = 1;

/* AncientCaveGenerator.LENGTH_CONVERSIONS */
int sr_p3_ac_num_floors(const struct sr_settings *s) {
  const char *l = sr_settings_get(s, "acLength");
  if (!strcmp(l, "short")) return 8;
  if (!strcmp(l, "long")) return 24;
  return 16;
}

int sr_p3_is_ac(struct sr_ctx *c) { return !strcmp(sr_settings_get(c->settings, "mode"), "ancientcave"); }

/* ------------------------------------------------------------------------------------------------------ hack registry */
const struct sr_hack sr_p3_ac_hacks[] = {
  {"Change starting door for non-vanilla mode", sr_w8_hack_starting_door, "StartingDoor", NULL},
  {"Door changes for non-vanilla modes", sr_w8_hack_door_expansion, "DoorExpansion", NULL},
  {"Simplify lookup for animated map palettes", sr_w8_hack_animated_palette, "AnimatedPaletteSimplification", NULL},
  {"Changes to island tileset for ancient cave", sr_p3_hack_island_tileset, "AcIslandTilesetChanges", NULL},
  {"Changes to cave tileset for ancient cave", sr_p3_hack_cave_tileset, "AcCaveTilesetChanges", NULL},
  {"Changes to manafort interior tileset for ancient cave", sr_p3_hack_manafort_tileset, "AcManafortTilesetChanges", NULL},
  {"Floor number and timer on layer 3", sr_w8_hack_layer3_changes, "Layer3Changes", NULL},
  {"Custom text printing events for non-vanilla modes", sr_w8_hack_timer_dialogue, "TimerDialogue", NULL},
  {"Custom event to sell all unused gear in non-vanilla modes", sr_w9_hack_sell_all_gear, "SellAllGear", NULL},
  {"Randomization for ancient cave mode", sr_p3_hack_ancient_cave_randomizer, "AncientCaveRandomizer", NULL},
  {"Stat generation for non-vanilla modes", sr_w8_hack_enemy_changes, "EnemyChanges", NULL},
  {"Enemy drop changes for non-vanilla modes", sr_w8_hack_drop_changes, "DropChanges", NULL},
  {"Shop changes for non-vanilla modes", sr_w8_hack_shop_changes, "ShopChanges", NULL},
  {"Death event change for non-vanilla modes", sr_w8_hack_death_event_doorway_removal, "DeathEventDoorwayRemoval", NULL},
  {"Magic rope as un-stuck for non-vanilla modes", sr_w8_hack_magic_rope, "MagicRope", NULL},
  {"Decreased armor magic defense for non-vanilla modes", sr_w8_hack_mag_def_adjustment, "MagDefAdjustment", NULL},
  {"Fixed magic power for enemies in non-vanilla modes", sr_w8_hack_enemy_black_magic_power, "EnemyBlackMagicPower", NULL},
  {"Statistics tracker for non-vanilla modes", sr_w8_hack_statistics, "Statistics", NULL},
  {"Title screen changes", sr_hack_title_screen, "TitleScreenAddition", NULL},
  {"Music rando + imported music", sr_hack_custom_music, "CustomMusic", NULL},
};
const int sr_p3_ac_hack_count = 20;

/* ---------------------------------------------------------------------------------------------------------- preflight */
int sr_p3_preflight(const struct sr_settings *s, const char *mode, const char *difficulty, char *why, size_t why_len) {
  if (strcmp(mode, "ancientcave")) return 1;
  /* the CLI reads Resources/difficulty_<name>.properties; any other name leaves every value at 1.0 (and acDifficulty is then illegal or
   * "custom"), which is not ported */
  if (difficulty && difficulty[0] && strcmp(difficulty, "casual") && strcmp(difficulty, "hard") && strcmp(difficulty, "reallyhard")) {
    snprintf(why, why_len, "unsupported option: difficulty '%s' for ancient cave (supported: casual, hard, reallyhard)", difficulty);
    return 0;
  }
  if (!event_maker_ready) {
    snprintf(why, why_len, "unsupported option: the Ancient Cave event maker is not ported to C yet");
    return 0;
  }
  const char *list = sr_settings_get(s, "acBiomeTypes");
  while (*list) {
    const char *comma = strchr(list, ',');
    size_t len = comma ? (size_t)(comma - list) : strlen(list);
    int known = 0;
    for (int i = 0; i < 6; i++) {
      const struct ac_biome *b = sr_p3_biomes[i];
      if (!b || strlen(b->key) != len || strncmp(b->key, list, len)) continue;
      known = 1;
      if (!biome_ready[i]) {
        snprintf(why, why_len, "unsupported option: acBiomeTypes=%s (%s floors are not ported to C yet)", b->key, b->key);
        return 0;
      }
    }
    if (!known) {
      snprintf(why, why_len, "unsupported option: acBiomeTypes has an unknown floor type");
      return 0;
    }
    if (!comma) break;
    list = comma + 1;
  }
  return 1;
}
