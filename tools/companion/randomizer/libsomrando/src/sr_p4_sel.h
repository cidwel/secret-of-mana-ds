/* libsomrando: Open World selectors (phase 4, owner "sel").  LGPL-2.1 or later.
 *
 * Derived from processing/openworld/{OpenWorldCharacterSelection,OpenWorldGoalProcessor,OpenWorldClassSelection,OpenWorldDifficultyProcessor,
 * OpenWorldMtrSeedNumSelection,OpenWorldXmasHacks,PlandoUtils}.cs (Author: Moppleton).  Translated to C 2026-10-18.
 *
 * Helpers for StringValueSettings-typed workingData access (setBool/getBool/setDouble/getDouble) that any Open World module may use.
 * The plando setting is only supported empty: PlandoUtils then leaves context.plandoSettings an empty dictionary (nothing reads defaults from
 * it), so there is no accessor; every ContainsKey/Count on it is false/0. */
#ifndef SR_P4_SEL_H
#define SR_P4_SEL_H
#include "sr_core.h"

/* workingData.setBool / getBool ("yes"/"no"; getBool of a missing key is false) */
void sr_p4_sel_work_set_bool(struct sr_ctx *c, const char *key, int v);
int sr_p4_sel_work_get_bool(const struct sr_ctx *c, const char *key);
/* workingData.setDouble (shortest round-trip text like .NET Core) / getDouble (NaN when missing or unparsable) */
void sr_p4_sel_work_set_double(struct sr_ctx *c, const char *key, double v);
double sr_p4_sel_work_get_double(const struct sr_ctx *c, const char *key);
/* workingData.get: "" when missing */
const char *sr_p4_sel_work_str(const struct sr_ctx *c, const char *key);
/* workingData.getInt: Int32.Parse, fails the generation (like the FormatException) when missing or not a number */
int sr_p4_sel_work_get_int(struct sr_ctx *c, const char *key);

/* workingData keys published by this module (same literals as the C# constants) */
#define SR_P4_STARTING_CHARACTER "startingCharacter"
#define SR_P4_BOY_IN_LOGIC "findBoy"
#define SR_P4_GIRL_IN_LOGIC "findGirl"
#define SR_P4_SPRITE_IN_LOGIC "findSprite"
#define SR_P4_START_WITH_BOY "startWithBoy"
#define SR_P4_START_WITH_GIRL "startWithGirl"
#define SR_P4_START_WITH_SPRITE "startWithSprite"
#define SR_P4_BOY_EXISTS "boyExists"
#define SR_P4_GIRL_EXISTS "girlExists"
#define SR_P4_SPRITE_EXISTS "spriteExists"
#define SR_P4_START_SOLO "startSolo"
#define SR_P4_FOUND_CHARS_GET_YOUR_LEVEL "foundCharsGetYourLevel"
#define SR_P4_MANA_FORT_ACCESSIBLE "fastManaFort"
#define SR_P4_GOAL_SHORT_NAME "openWorldGoalShortName"
#define SR_P4_GOAL_MANABEAST "manabeast"
#define SR_P4_GOAL_MTR "manatree"
#define SR_P4_GOAL_GIFTMODE "xmas2020"
#define SR_P4_GOAL_REINDEER "xmas2020b"
#define SR_P4_BOY_CLASS "boyClass"
#define SR_P4_GIRL_CLASS "girlClass"
#define SR_P4_SPRITE_CLASS "spriteClass"
#define SR_P4_GIRL_MAGIC_EXISTS "girlMagicExists"
#define SR_P4_SPRITE_MAGIC_EXISTS "spriteMagicExists"
#define SR_P4_ANY_MAGIC_EXISTS "anyMagicExists"
#define SR_P4_MANA_SEEDS_REQUIRED "manaSeedsRequired"
#define SR_P4_XMAS_PALETTES_INDICATOR "didXmasPalettes"
#define SR_P4_ENEMY_LEVEL_TYPE "enemyLevelType"
#define SR_P4_ENEMY_DIFFICULTY_LEVEL "enemyDifficultyLevel"
#define SR_P4_ENEMY_DIFFICULTY_STAT_SCALE "enemyDifficultyStatScale"
#define SR_P4_BOSS_DIFFICULTY_INCREMENT "enemyBossDifficultyIncValue"
#define SR_P4_ENEMY_DIFFICULTY_TIMER_INC "enemyDifficultyTimerIncPeriod"
#endif
