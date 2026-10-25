/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Hack registry of the vanilla randomizer.  Order and names are those of RomGenerator.cs (65 common hacks) and
 * VanillaRandoGenerator.cs (15 mode hacks); Author: Moppleton.  Translated to C 2026-10-18.  Generated once from the upstream sources by a
 * throw-away script, then maintained by hand: when a hack is ported, set its fn and shorten or drop its "requires" list.
 *
 * A hack with fn == NULL is not ported.  Its "requires" pairs are the option values for which the C# hack returns immediately without
 * touching the ROM, the settings or the random streams (checked by reading each process() and by the differential harness, which flips
 * every option against the C# generator), so skipping it is exact.  Any other value is reported as an unsupported option.
 */
#include <string.h>

#include "sr_core.h"
#include "sr_p2_system_holiday.h"

static const char *const req_c_10[] = {"characterColors", "none|rando", NULL};

const struct sr_hack sr_common_hacks[] = {
  {"Weapon randomizer", sr_hack_weapon_randomizer, "WeaponRandomizer", NULL},
  {"Title screen changes", sr_hack_title_screen, "TitleScreenAddition", NULL},
  {"Faster magic levels", sr_hack_faster_magic_levels, "FasterMagicLevels", NULL},
  {"Faster weapon levels", sr_hack_faster_weapon_levels, "FasterWeaponLevels", NULL},
  {"Faster top of screen dialogue", sr_hack_faster_top_of_screen_dialogue, "FasterTopOfScreenDialogue", NULL},
  {"Levelups restore MP", sr_hack_levelup_mp_restore, "LevelupMpRestore", NULL},
  {"Chests open without shaking", sr_hack_chest_opening_speedup, "ChestOpeningSpeedup", NULL},
  {"Magic cost/power rebalance", sr_hack_magic_rebalance, "MagicRebalance", NULL},
  {"Extend status lengths", sr_hack_status_lengths, "StatusLengths", NULL},
  {"Rabite color randomizer", sr_hack_rabite_color, "RabiteColorRandomizer", NULL},
  {"Character palette randomizer", sr_hack_character_palette, "CharacterPaletteRandomizer", req_c_10},
  {"Allow characters to be off screen", sr_hack_free_walking, "FreeWalking", NULL},
  {"Overcharge fix", sr_hack_overcharge_fix, "OverchargeFix", NULL},
  {"Sound effects reduction", sr_hack_no_footstep_sound, "NoFootstepSound", NULL},
  {"Sound effects channel reduction", sr_hack_sound_effects_reduction, "SoundEffectsReduction", NULL},
  {"Remove sword-pull animation", sr_hack_no_sword_pull, "NoSwordPullAnimation", NULL},
  {"Increase max consumables to 7", sr_hack_max_items_increase, "MaxItemsIncrease", NULL},
  {"Hexas fix", sr_hack_hexas_fix, "HexasFix", NULL},
  {"Faster boss AI", sr_hack_fast_bosses, "FastBosses", NULL},
  {"Gnome speedup spell enhancements", sr_hack_speedup, "Speedup", NULL},
  {"Status condition palette glow", sr_hack_status_glow, "StatusGlow", NULL},
  {"Vampire hittable when cloaked", sr_hack_hittable_vampire, "HittableVampire", NULL},
  {"Starter gear in each slot", sr_hack_starter_gear, "StarterGear", NULL},
  {"Changes to boss/vanilla chest weapon orb reward events", sr_hack_orb_reward_events, "OrbRewardEventGenerator", NULL},
  {"Boss death softlock fix", sr_hack_boss_death_fix, "BossDeathFix", NULL},
  {"Enemy type/species damage", sr_hack_enemy_type_damage, "EnemyTypeDamage", NULL},
  {"Elemental damage", sr_hack_elemental_damage, "ElementalDamage", NULL},
  {"Faster door transitions", sr_hack_fast_transitions, "FastTransitions", NULL},
  {"Mode 7 minor graphical fix", sr_hack_mode7_edges_fix, "Mode7EdgesFix", NULL},
  {"Accurate damage percentages", sr_hack_damage_percent_fix, "DamagePercentFix", NULL},
  {"Remove energy cost for running", sr_hack_no_energy_to_run, "NoEnergyToRun", NULL},
  {"Gigases never split into those sparkly things", sr_hack_gigases_never_split, "GigasesNeverSplit", NULL},
  {"Whip coordinate fix", sr_hack_whip_fix, "WhipFix", NULL},
  {"Off-screen targetting", sr_hack_free_targetting, "FreeTargetting", NULL},
  {"Buy multiple consumables at once", sr_hack_buy_multiple, "BuyMultipleItems", NULL},
  {"Mana magic fixed charge level", sr_hack_mana_magic_fix, "ManaMagicFix", NULL},
  {"One-hit KO", sr_hack_one_hit_ko, "OneHitKo", NULL},
  {"Fix summoning enemies despawning certain NPCs", sr_hack_summon_despawn_fix, "SummonDespawnFix", NULL},
  {"Walk through walls", sr_hack_no_collision, "NoCollision", NULL},
  {"Confusion status inversion fix", sr_hack_confuse_fix, "ConfuseFix", NULL},
  {"Beak disable for Spring/Axe Beak", sr_hack_beak_shield_disable, "BeakShieldDisable", NULL},
  {"High-level enemy name fix", sr_hack_high_level_enemy_name_fix, "HighLevelEnemyNameFix", NULL},
  {"Mech rider death fix", sr_hack_mech_rider_death_fix, "MechRiderDeathFix", NULL},
  {"Position adjustments for vanilla enemies", sr_hack_enemy_position_adjust, "EnemyPositionAdjustments", NULL},
  {"Attacking doesn't cost stamina", sr_hack_no_energy_to_attack, "NoEnergyToAttack", NULL},
  {"Aggressive non-boss enemies", sr_hack_faster_enemies, "FasterEnemies", NULL},
  {"Players are always poisoned", sr_hack_always_poisoned, "AlwaysPoisoned", NULL},
  {"Poison deals percentage damage", sr_hack_percentage_poison, "PercentagePoison", NULL},
  {"Obscure damage numbers", sr_hack_obscure_damage, "ObscureDamage", NULL},
  {"Obscure player HP", sr_hack_obscure_own_hp, "ObscureOwnHp", NULL},
  {"Obscure own gold amount", sr_hack_obscure_gold_amount, "ObscureGoldAmount", NULL},
  {"Defense refactor", sr_hack_defense_refactor, "DefenseRefactor", NULL},
  {"Display zero damage for evades", sr_hack_display_evades_as_zero, "DisplayEvadesAsZero", NULL},
  {"Always show damage numbers on-screen", sr_hack_numbers_on_screen, "NumbersOnScreen", NULL},
  {"Boss element randomizer", sr_hack_boss_element_rando, "BossElementRando", NULL},
  {"12-letter name entry", sr_hack_name_entry_changes, "NameEntryChanges", NULL},
  {"Enemies have infinite MP", sr_hack_enemy_infinite_mp, "EnemyInfiniteMp", NULL},
  {"Fix damage canceling", sr_hack_damage_cancel_fix, "DamageCancelFix", NULL},
  {"Minimap when flying on flammie", sr_hack_minimap, "Ff6StyleMiniMap", NULL},
  {"Allow cup of wishes on any 0 HP characters", sr_hack_cup_wishes_on_zero_hp, "CupWishesOnZeroHp", NULL},
  {"Use candy and chocolate on 0 HP characters", sr_hack_candy_healouts, "CandyHealouts", NULL},
  {"Magic rope animation/death fix for girl/sprite", sr_hack_magic_rope_death_fix, "MagicRopeDeathFix", NULL},
  {"Initialization of values for start of run", sr_hack_initial_values, "InitialValues", NULL},
  {"Mushroom enemies become vineshrooms", sr_hack_vineshroom, "Vineshroom", NULL},
  {"Default player AI to aggressive", sr_hack_player_ai_default, "PlayerAiDefaultChanges", NULL},
};
const int sr_common_hack_count = 65;


const struct sr_hack sr_rando_hacks[] = {
  {"Spell orb element randomizer", sr_hack_element_swaps, "ElementSwaps", NULL},
  {"Dialogue shortening for vanilla rando", sr_hack_faster_dialogue_events, "FasterDialogueEvents", NULL},
  {"Boss randomizer", sr_hack_boss_swaps, "BossSwaps", NULL},
  {"Non-boss enemy randomizer", sr_hack_enemy_swaps, "EnemySwaps", NULL},
  {"Invisible passageways clear slow condition", sr_hack_invis_clears_slow, "InvisClearsSlow", NULL},
  {"Boss Z-position determination", sr_hack_boss_z_position, "BossZPosition", NULL},
  {"Fall theme for vanilla rando", sr_w4_hack_fall_theme, "FallTheme", NULL},
  {"Winter theme for vanilla rando", sr_w4_hack_winter_theme, "WinterTheme", NULL},
  {"Experience scale", sr_hack_experience_adjust, "ExperienceAdjust", NULL},
  {"Gold scale", sr_hack_gold_adjust, "GoldAdjust", NULL},
  {"Title screen changes", sr_hack_title_screen, "TitleScreenAddition", NULL},
  {"Music rando + imported music", sr_hack_custom_music, "CustomMusic", NULL},
  {"Vanilla rando enemy scaling", sr_hack_enemy_scaling, "EnemyScaling", NULL},
  {"Workaround for one issue with a fire palace door", sr_hack_fire_palace_door_fix, "FirePalaceDoorFix", NULL},
  {"Auto-save to slot 4 on door transitions", sr_hack_autosave, "Autosave", NULL},
};
const int sr_rando_hack_count = 15;

void sr_hacks_dump(FILE *f) {
  for (int pass = 0; pass < 2; pass++) {
    const struct sr_hack *list = pass ? sr_rando_hacks : sr_common_hacks;
    int n = pass ? sr_rando_hack_count : sr_common_hack_count;
    for (int i = 0; i < n; i++)
      fprintf(f, "%d %s %s|%s\n", i, pass ? "mode" : "common", list[i].fn ? (list[i].requires ? "partial" : "ported") : "stub", list[i].name);
  }
}
