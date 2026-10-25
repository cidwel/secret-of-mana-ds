/* libsomrando: Open World mode (phase 4) declarations shared by the pipeline and by every sr_p4_* module.  LGPL-2.1 or later.
 *
 * Derived from SoMRandomizer/processing/openworld/OpenWorldGenerator.cs (Author: Moppleton).  Translated to C 2026-10-18.
 * Every hack function has the C# process() contract: it returns 1 when the C# returns true, and it consumes the random streams, writes the ROM
 * and publishes workingData exactly like the C#.  Names (sr_hack.name) equal getName() of the C# class because the per-hack log used by the
 * tests (tests/hack_cmp.py) compares them. */
#ifndef SR_P4_H
#define SR_P4_H
#include <stddef.h>
#include "sr_core.h"

extern const struct sr_hack sr_p4_open_hacks[];
extern const int sr_p4_open_hack_count;

/* 1 when the request can be reproduced; otherwise writes the reason to why */
int sr_p4_preflight(const struct sr_settings *s, const char *mode, const char *difficulty, char *why, size_t why_len);

/* RandoProcessor.add() of a nested hack: runs fn, appends the per-hack log line (name must equal getName()).  Honours SOMRANDO_SKIP_NAMES in
 * debug builds.  Returns the hack's result. */
int sr_p4_nested(struct sr_ctx *c, const char *name, sr_hack_fn fn);
int sr_p4_name_skipped(const char *name);   /* debug builds: name listed in SOMRANDO_SKIP_NAMES ('|' separated) */

/* ---- selectors and small processors (sr_p4_sel_*.c) */
int sr_p4_hack_character_selection(struct sr_ctx *c);     /* OpenWorldCharacterSelection (also runs StartingCharacterRandomizer nested) */
int sr_p4_hack_goal_processor(struct sr_ctx *c);          /* OpenWorldGoalProcessor */
int sr_p4_hack_class_selection(struct sr_ctx *c);         /* OpenWorldClassSelection */
int sr_p4_hack_silly_enemy_names(struct sr_ctx *c);       /* OpenWorldSillyEnemyNamePicker */
int sr_p4_hack_mtr_seed_selection(struct sr_ctx *c);      /* OpenWorldMtrSeedNumSelection */
int sr_p4_hack_ice_country_fixes(struct sr_ctx *c);       /* IceCountryLandableFixes */
int sr_p4_hack_xmas_hacks(struct sr_ctx *c);              /* OpenWorldXmasHacks */
int sr_p4_hack_difficulty_processor(struct sr_ctx *c);    /* OpenWorldDifficultyProcessor */
int sr_p4_hack_dont_auto_reset(struct sr_ctx *c);         /* DontAutoResetAtFirstMenu */
/* PlandoUtils.processPlandoSetting(settings, context): called by OpenWorldGenerator.generate before the hacks (not a hack, no log line) */
void sr_p4_process_plando(struct sr_ctx *c);

/* ---- randomization core (sr_p4_ow_*.c) */
int sr_p4_hack_open_randomizer(struct sr_ctx *c);         /* OpenWorldRandomizer */

/* ---- events (sr_p4_ev_*.c) */
int sr_p4_hack_open_events(struct sr_ctx *c);             /* OpenWorldEvents: runs the nested event hacks through sr_p4_nested */
int sr_p4_ev_new_game(struct sr_ctx *c);                  /* NewGameEvent */
int sr_p4_ev_intro_gift(struct sr_ctx *c);                /* GiftDeliveryIntroEvent */
int sr_p4_ev_intro_reindeer(struct sr_ctx *c);            /* ReindeerIntroEvent */
int sr_p4_ev_intro_flammie(struct sr_ctx *c);             /* FlammieLogicIntroEvent */
int sr_p4_ev_intro_waterfall(struct sr_ctx *c);           /* WaterfallIntroEvent */
int sr_p4_ev_initial_flags(struct sr_ctx *c);             /* InitialEventFlagStates */
int sr_p4_ev_intro(struct sr_ctx *c);                     /* IntroEvent (runs the four intro events and InitialEventFlagStates nested) */
int sr_p4_ev_sword_grab(struct sr_ctx *c);                /* SwordGrabEvent */
int sr_p4_ev_prize_events(struct sr_ctx *c);              /* PrizeEvents */
int sr_p4_ev_hint_events(struct sr_ctx *c);               /* HintEvents */
int sr_p4_ev_pokedex(struct sr_ctx *c);                   /* PokedexEvent */
int sr_p4_ev_neko(struct sr_ctx *c);                      /* NekoEvents */
int sr_p4_ev_supporting_events(struct sr_ctx *c);         /* OpenWorldSupportingEvents */
int sr_p4_ev_supporting_map_changes(struct sr_ctx *c);    /* OpenWorldSupportingMapChanges */
int sr_p4_ev_misc_dialogue(struct sr_ctx *c);             /* MiscDialogue */

/* ---- hacks (sr_p4_hk_*.c) */
int sr_p4_hack_crystal_orb_colors(struct sr_ctx *c);
int sr_p4_hack_custom_top_of_screen_text(struct sr_ctx *c);
int sr_p4_hack_min_max_level(struct sr_ctx *c);
int sr_p4_hack_enemies_at_your_level(struct sr_ctx *c);
int sr_p4_hack_shop_randomizer(struct sr_ctx *c);         /* runs NoNekoOverflow nested ("Neko price overflow fix") */
int sr_p4_hack_watts_weapon_fix(struct sr_ctx *c);
int sr_p4_hack_lime_slime_flavors(struct sr_ctx *c);
int sr_p4_hack_drop_only_consumables(struct sr_ctx *c);
int sr_p4_hack_menu_icons_armor(struct sr_ctx *c);
int sr_p4_hack_magic_rope_refactor(struct sr_ctx *c);
int sr_p4_hack_character_join_changes(struct sr_ctx *c);
int sr_p4_hack_map_palette_randomizations(struct sr_ctx *c);
int sr_p4_hack_ai_swap(struct sr_ctx *c);
int sr_p4_hack_black_map_palettes(struct sr_ctx *c);
int sr_p4_hack_nesoberi(struct sr_ctx *c);
int sr_p4_hack_fast_whip_posts(struct sr_ctx *c);
int sr_p4_hack_invincible_rabite(struct sr_ctx *c);
int sr_p4_hack_character_classes(struct sr_ctx *c);
int sr_p4_hack_title_menu_seed_hash(struct sr_ctx *c);
/* Per-owner preflight: 1 when every option value that the owner's hacks read is reproduced byte for byte, else 0 with an "unsupported option: ..."
 * message in why.  Called by sr_p4_preflight for mode open (weak default in sr_p4_open.c returns 1: an owner overrides it in its own file). */
int sr_p4_sel_supports(const struct sr_settings *s, char *why, size_t why_len);
int sr_p4_shared_supports(const struct sr_settings *s, char *why, size_t why_len);
int sr_p4_core_supports(const struct sr_settings *s, char *why, size_t why_len);
int sr_p4_inject_supports(const struct sr_settings *s, char *why, size_t why_len);
int sr_p4_ev1_supports(const struct sr_settings *s, char *why, size_t why_len);
int sr_p4_ev2_supports(const struct sr_settings *s, char *why, size_t why_len);
int sr_p4_hlevel_supports(const struct sr_settings *s, char *why, size_t why_len);
int sr_p4_hchar_supports(const struct sr_settings *s, char *why, size_t why_len);
int sr_p4_hmisc_supports(const struct sr_settings *s, char *why, size_t why_len);
#endif
