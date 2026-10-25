/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Open World selectors and small processors.  Derived from (Author: Moppleton for all of them), translated to C 2026-10-18:
 *   processing/openworld/{OpenWorldCharacterSelection,OpenWorldGoalProcessor,OpenWorldClassSelection,OpenWorldSillyEnemyNamePicker,
 *     OpenWorldMtrSeedNumSelection,OpenWorldXmasHacks,OpenWorldDifficultyProcessor,PlandoUtils}.cs
 *   processing/hacks/openworld/StartingCharacterRandomizer.cs, processing/hacks/common/qol/{IceCountryLandableFixes,DontAutoResetAtFirstMenu}.cs
 */
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "sr_p3_ac.h"
#include "sr_p4_sel_int.h"

#define S(k) sr_settings_get(c->settings, (k))
#define P(off, v) sr_poke(c, (off), (v))

/* ------------------------------------------------------------------------------------------------ workingData helpers */
void sr_p4_sel_work_set_bool(struct sr_ctx *c, const char *key, int v) { sr_work_set(c, key, v ? "yes" : "no"); }

static int str_to_bool(const char *s) {
  char b[8];
  size_t n = 0;
  while (*s == ' ' || (*s >= 9 && *s <= 13)) s++;
  while (*s && n < sizeof b - 1) b[n++] = (char)((*s >= 'A' && *s <= 'Z') ? *s + 32 : *s), s++;
  while (*s == ' ' || (*s >= 9 && *s <= 13)) s++;
  if (*s) return 0;
  while (n && b[n - 1] == ' ') n--;
  b[n] = 0;
  return !strcmp(b, "yes") || !strcmp(b, "true") || !strcmp(b, "1");
}

int sr_p4_sel_work_get_bool(const struct sr_ctx *c, const char *key) {
  const char *v = sr_work_get(c, key);
  return v ? str_to_bool(v) : 0;
}

const char *sr_p4_sel_work_str(const struct sr_ctx *c, const char *key) {
  const char *v = sr_work_get(c, key);
  return v ? v : "";
}

void sr_p4_sel_work_set_double(struct sr_ctx *c, const char *key, double v) {
  char b[400];
  /* fixed notation with the fewest decimals that round-trips: what Double.ToString does for the magnitudes used here (1e-5 .. 1e15) */
  if (!(fabs(v) < 1e15 && (v == 0 || fabs(v) >= 1e-5))) sr_fail(c, SOMRANDO_ERR_FAILED, "double %g cannot be formatted", v);
  for (int dec = 0; dec <= 20; dec++) {
    snprintf(b, sizeof b, "%.*f", dec, v);
    if (strtod(b, NULL) == v) break;
  }
  sr_work_set(c, key, b);
}

double sr_p4_sel_work_get_double(const struct sr_ctx *c, const char *key) {
  const char *v = sr_work_get(c, key);
  char *end;
  if (!v || !v[0]) return NAN;
  double d = strtod(v, &end);
  return *end ? NAN : d;
}

int sr_p4_sel_work_get_int(struct sr_ctx *c, const char *key) {
  const char *v = sr_work_get(c, key);
  char *end;
  if (!v || !v[0]) sr_fail(c, SOMRANDO_ERR_FAILED, "Input string was not in a correct format (%s).", key);
  long n = strtol(v, &end, 10);
  if (*end) sr_fail(c, SOMRANDO_ERR_FAILED, "Input string was not in a correct format (%s).", key);
  return (int)n;
}

/* ------------------------------------------------------------------------------------------------------------- plando */
/* PlandoUtils.processPlandoSetting: an empty setting leaves plandoSettings empty; anything else is refused by sr_p4_sel_supports. */
void sr_p4_process_plando(struct sr_ctx *c) {
  if (S("plandoSettings")[0]) sr_fail(c, SOMRANDO_ERR_UNSUPPORTED, "unsupported option: plandoSettings is not supported");
}

/* ----------------------------------------------------------------------------------------- StartingCharacterRandomizer */
static int starting_character_change(struct sr_ctx *c) {
  const char *startingChar = sr_p4_sel_work_str(c, SR_P4_STARTING_CHARACTER);
  int girl = !strcmp(startingChar, "girl");
  P(0x75f75, girl ? 1 : 2);
  if (girl) {
    P(0x751cf, 0xDF);
    P(0xFCD, 0x02);
    P(0x75762, 0x02);
    P(0x75ADD, 0x03 + 0x0C);
    P(0x75B5F, 0x85);
    P(0x75B7E, 0x87);
  } else {
    P(0x751d2, 0xDF);
    P(0xFCD, 0x04);
    P(0x75762, 0x04);
    P(0x75ADD, 0x03 + 0x0C + 0x0C);
    P(0x75B60, 0x62);
    P(0x75B7F, 0x62);
  }
  int wo = c->working_offset;
  P(0xFCE, 0x22);
  P(0xFCF, (uint8_t)wo);
  P(0xFD0, (uint8_t)(wo >> 8));
  P(0xFD1, (uint8_t)((wo >> 16) + 0xC0));
  P(0xFD2, 0xEA);
  sr_emit(c, 0x85);
  sr_emit(c, 0xD9);
  sr_emit(c, 0xA9);
  sr_emit(c, 0x01);
  sr_emit(c, 0x8D);
  sr_emit(c, 0x00);
  sr_emit(c, girl ? 0xE2 : 0xE4);
  sr_emit(c, 0x6B);
  /* the npc shown on the starting map (MAPNUM_INTRO_LOG = 256, object 0) */
  int start = sr_vanilla_object_offset(c->out, 256) + 8;
  int end = sr_vanilla_object_offset(c->out, 257);
  if (start + 8 > end) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range (map 256 objects).");
  P(start + 5, girl ? 0x8B : 0x8C);
  return 1;
}

/* ----------------------------------------------------------------------------------------------- character selection */
int sr_p4_hack_character_selection(struct sr_ctx *c) {
  char startingChar[8];
  snprintf(startingChar, sizeof startingChar, "%s", S("opStartChar"));
  if (!strcmp(startingChar, "random")) {
    switch (sr_random_next(&c->rf) % 3) {
      case 0: strcpy(startingChar, "boy"); break;
      case 1: strcpy(startingChar, "girl"); break;
      default: strcpy(startingChar, "sprite"); break;
    }
  }
  int boyInLogic = 0, girlInLogic = 1, spriteInLogic = 1, startSolo = 1;
  int startWithBoy = !strcmp(startingChar, "boy");
  int startWithGirl = !strcmp(startingChar, "girl");
  int startWithSprite = !strcmp(startingChar, "sprite");

  const char *others = S("opCharacters");
  int foundCharactersGetYourLevel = strstr(others, "CL") != NULL;
  if (strstr(others, "none")) {
    boyInLogic = girlInLogic = spriteInLogic = 0;
    startSolo = 1;
  } else if (strstr(others, "startboth")) {
    boyInLogic = girlInLogic = spriteInLogic = 0;
    startWithBoy = startWithGirl = startWithSprite = 1;
    startSolo = 0;
  } else if (strstr(others, "start1")) {
    boyInLogic = girlInLogic = spriteInLogic = 0;
    startSolo = 0;
    int which = (sr_random_next(&c->rf) & 2) == 0;
    int findOther = strstr(others, "find1") != NULL;
    if (startWithBoy) {
      startWithGirl = which;
      startWithSprite = !which;
      if (findOther) { girlInLogic = !which; spriteInLogic = which; }
    } else if (startWithGirl) {
      startWithBoy = which;
      startWithSprite = !which;
      if (findOther) { boyInLogic = !which; spriteInLogic = which; }
    } else if (startWithSprite) {
      startWithBoy = which;
      startWithGirl = !which;
      if (findOther) { boyInLogic = !which; girlInLogic = which; }
    }
  } else if (strstr(others, "find1")) {
    boyInLogic = girlInLogic = spriteInLogic = 0;
    startSolo = 1;
    int which = (sr_random_next(&c->rf) & 2) == 0;
    if (startWithBoy) {
      girlInLogic = which;
      spriteInLogic = !which;
    } else if (startWithGirl) {
      boyInLogic = which;
      spriteInLogic = !which;
    } else if (startWithSprite) {
      boyInLogic = which;
      girlInLogic = !which;
    }
  } else if (strstr(others, "findboth")) {
    boyInLogic = !startWithBoy;
    girlInLogic = !startWithGirl;
    spriteInLogic = !startWithSprite;
    startSolo = 1;
  }

  int boyExists = startWithBoy || boyInLogic;
  int girlExists = startWithGirl || girlInLogic;
  int spriteExists = startWithSprite || spriteInLogic;
  /* plando (NON-EXISTENT) is unsupported, so only the final sanity check of the C# remains */
  if (!boyExists && !girlExists && !spriteExists) sr_fail(c, SOMRANDO_ERR_FAILED, "Plando - no characters exist!");

  sr_work_set(c, SR_P4_STARTING_CHARACTER, startingChar);
  sr_p4_sel_work_set_bool(c, SR_P4_START_WITH_BOY, startWithBoy);
  sr_p4_sel_work_set_bool(c, SR_P4_START_WITH_GIRL, startWithGirl);
  sr_p4_sel_work_set_bool(c, SR_P4_START_WITH_SPRITE, startWithSprite);
  sr_p4_sel_work_set_bool(c, SR_P4_BOY_IN_LOGIC, boyInLogic);
  sr_p4_sel_work_set_bool(c, SR_P4_GIRL_IN_LOGIC, girlInLogic);
  sr_p4_sel_work_set_bool(c, SR_P4_SPRITE_IN_LOGIC, spriteInLogic);
  sr_p4_sel_work_set_bool(c, SR_P4_BOY_EXISTS, boyExists);
  sr_p4_sel_work_set_bool(c, SR_P4_GIRL_EXISTS, girlExists);
  sr_p4_sel_work_set_bool(c, SR_P4_SPRITE_EXISTS, spriteExists);
  sr_p4_sel_work_set_bool(c, SR_P4_START_SOLO, startSolo);
  sr_p4_sel_work_set_bool(c, SR_P4_FOUND_CHARS_GET_YOUR_LEVEL, foundCharactersGetYourLevel);

  if (strcmp(startingChar, "boy")) sr_p4_nested(c, "Starting character change hack", starting_character_change);
  return 1;
}

/* ---------------------------------------------------------------------------------------------------- goal processor */
int sr_p4_hack_goal_processor(struct sr_ctx *c) {
  int fastManaFort = 1;
  const char *goal = "";
  const char *goalType = S("opGoal");
  if (strstr(goalType, "short")) { fastManaFort = 1; goal = SR_P4_GOAL_MANABEAST; }
  else if (strstr(goalType, "long")) { fastManaFort = 0; goal = SR_P4_GOAL_MANABEAST; }
  else if (strstr(goalType, "mtr")) { fastManaFort = 1; goal = SR_P4_GOAL_MTR; }
  else if (strstr(goalType, "gift")) { fastManaFort = 0; goal = SR_P4_GOAL_GIFTMODE; }
  else if (strstr(goalType, "reindeer")) { fastManaFort = 0; goal = SR_P4_GOAL_REINDEER; }
  sr_p4_sel_work_set_bool(c, SR_P4_MANA_FORT_ACCESSIBLE, fastManaFort);
  sr_work_set(c, SR_P4_GOAL_SHORT_NAME, goal);
  return 1;
}

/* ------------------------------------------------------------------------------------------------- class selection */
struct cls_list { const char *v[3]; int n; };
static const char *cls_get(struct sr_ctx *c, const struct cls_list *l, int i) {
  if (i < 0 || i >= l->n) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range.");
  return l->v[i];
}
static void cls_remove(struct cls_list *l, const char *s) {
  for (int i = 0; i < l->n; i++)
    if (!strcmp(l->v[i], s)) {
      for (int j = i; j < l->n - 1; j++) l->v[j] = l->v[j + 1];
      l->n--;
      return;
    }
}

int sr_p4_hack_class_selection(struct sr_ctx *c) {
  const char *cls[3];
  cls[0] = S("opBoyRole");
  cls[1] = S("opGirlRole");
  cls[2] = S("opSpriteRole");
  struct cls_list randomClass = {{"OGboy", "OGgirl", "OGsprite"}, 3};
  struct cls_list unique = {{"OGboy", "OGgirl", "OGsprite"}, 3};

  for (int i = 0; i < 3; i++) {
    if (!strcmp(cls[i], "random")) cls[i] = cls_get(c, &randomClass, AC_RND(c, randomClass.n));
    else cls_remove(&unique, cls[i]);
  }
  for (int i = 0; i < 3; i++) {
    if (!strcmp(cls[i], "randomunique")) {
      if (unique.n > 0) {
        cls[i] = cls_get(c, &unique, AC_RND(c, unique.n));
        cls_remove(&unique, cls[i]);
      } else {
        cls[i] = cls_get(c, &randomClass, AC_RND(c, randomClass.n));
      }
    }
  }
  int boyEx = sr_p4_sel_work_get_bool(c, SR_P4_BOY_EXISTS);
  int girlEx = sr_p4_sel_work_get_bool(c, SR_P4_GIRL_EXISTS);
  int spriteEx = sr_p4_sel_work_get_bool(c, SR_P4_SPRITE_EXISTS);
  int girlMagic = (boyEx && !strcmp(cls[0], "OGgirl")) || (girlEx && !strcmp(cls[1], "OGgirl")) || (spriteEx && !strcmp(cls[2], "OGgirl"));
  int spriteMagic = (boyEx && !strcmp(cls[0], "OGsprite")) || (girlEx && !strcmp(cls[1], "OGsprite")) || (spriteEx && !strcmp(cls[2], "OGsprite"));
  sr_work_set(c, SR_P4_BOY_CLASS, cls[0]);
  sr_work_set(c, SR_P4_GIRL_CLASS, cls[1]);
  sr_work_set(c, SR_P4_SPRITE_CLASS, cls[2]);
  sr_p4_sel_work_set_bool(c, SR_P4_GIRL_MAGIC_EXISTS, girlMagic);
  sr_p4_sel_work_set_bool(c, SR_P4_SPRITE_MAGIC_EXISTS, spriteMagic);
  sr_p4_sel_work_set_bool(c, SR_P4_ANY_MAGIC_EXISTS, girlMagic || spriteMagic);
  return 1;
}

/* ------------------------------------------------------------------------------------------- silly enemy names */
int sr_p4_hack_silly_enemy_names(struct sr_ctx *c) {
  struct sr_random *r = &c->rc;
#define NM(i, s) sr_names_set(c, NAMES_INDEX_ENEMIES_START + (i), (s))
#define RN(n) (sr_random_next(r) % (n))
  int rabiteName = RN(3);
  if (rabiteName == 0) { NM(0, "Pogopuschel"); NM(20, "Turbopuschel"); }
  else if (rabiteName == 1) { NM(20, "Rabilion"); }
  if (RN(2) == 0) NM(87, "Antis Mant");
  if (RN(2) == 0) NM(57, "Metroid");
  if (RN(2) == 0) { NM(105, "Snek"); NM(123, "Snek"); }
  else if (RN(2) == 0) { NM(105, "Danger Noodle"); NM(123, "Danger Noodle"); }
  else if (RN(2) == 0) { NM(105, "Nope Rope"); NM(123, "Nope Rope"); }
  if (RN(8) == 0) NM(102, "Gordon Bull");
  if (RN(2) == 0) NM(110, "Melon");
  else if (RN(2) == 0) NM(110, "Aegrfdiujeplf");
  if (RN(2) == 0) NM(2, "Myconid");
  if (RN(2) == 0) NM(3, "Ewok");
  if (RN(2) == 0) NM(28, "Ewok");
  if (RN(2) == 0) NM(13, "Sit on me");
  if (RN(2) == 0) NM(15, "Puking Cloud");
  if (RN(2) == 0) NM(29, "Puking Cloud");
  int mimicName = RN(4);
  if (mimicName == 1) NM(45, "Free Items");
  else if (mimicName == 2) NM(45, "Legit Chest");
  else if (mimicName == 3) NM(45, "Open me");
  if (RN(2) == 0) NM(21, "Flappy Death");
  if (RN(2) == 0) NM(22, "Molebear");
  else if (RN(2) == 0) NM(22, "Sonic");
  if (RN(2) == 0) { NM(122, "Pakkun Lizard"); NM(95, "Pakkun Dragon"); }
  if (RN(2) == 0) { NM(91, "Schmusekater"); NM(107, "Kuscheltiger"); }
  if (RN(2) == 0) { NM(96, "Alpha Cybermax"); NM(100, "Beta Cybermax"); NM(114, "Gamma Cybermax"); }
  int hydraName = RN(3);
  if (hydraName == 0) NM(92, "Gleeok");
  else if (hydraName == 1) NM(109, "Gleeok");
  if (RN(2) == 0) NM(127, "Flammie");
  else if (RN(2) == 0) NM(127, "Falkor");
#undef RN
#undef NM
  return 1;
}

/* --------------------------------------------------------------------------------------------- MTR seed number */
/* Int32.Parse for culture en-US with the default NumberStyles.Integer: optional white space and sign.  Returns 0 on FormatException. */
static int parse_int32(const char *s, int *out) {
  while (*s == ' ' || (*s >= 9 && *s <= 13)) s++;
  int neg = 0;
  if (*s == '+' || *s == '-') neg = *s++ == '-';
  if (*s < '0' || *s > '9') return 0;
  long long v = 0;
  while (*s >= '0' && *s <= '9') {
    v = v * 10 + (*s++ - '0');
    if (v > 2147483648LL) return 0;
  }
  while (*s == ' ' || (*s >= 9 && *s <= 13)) s++;
  if (*s) return 0;
  if (neg) v = -v;
  if (v > 2147483647LL || v < -2147483648LL) return 0;
  *out = (int)v;
  return 1;
}

int sr_p4_hack_mtr_seed_selection(struct sr_ctx *c) {
  int req;
  if (!parse_int32(S("opNumSeeds"), &req)) {
    int mn, mx;
    if (!parse_int32(S("opMinSeeds"), &mn) || !parse_int32(S("opMaxSeeds"), &mx))
      sr_fail(c, SOMRANDO_ERR_FAILED, "Input string was not in a correct format.");
    if (mn > mx) mn = mx;
    int diff = (int)((uint32_t)mx - (uint32_t)mn);
    if (diff == 0) {
      req = mn;
    } else {
      int hi = (int)((uint32_t)mx + 1u);
      if (mn > hi) sr_fail(c, SOMRANDO_ERR_FAILED, "'minValue' cannot be greater than maxValue.");
      req = sr_random_next_range(&c->rf, mn, hi);
    }
    if (req < 1) req = 1;
    if (req > 8) req = 8;
  }
  sr_work_set_int(c, SR_P4_MANA_SEEDS_REQUIRED, req);
  return 1;
}

/* --------------------------------------------------------------------------------------- Ice country landability */
int sr_p4_hack_ice_country_fixes(struct sr_ctx *c) {
  for (int i = 0; i < 0x18; i++) P(0x67630 + i, sr_peek(c, 0x67600 + i));
  static const uint8_t tileIndexes[3] = {0x30, 0xBB, 0xBA};
  for (int k = 0; k < 3; k++) {
    int tile = tileIndexes[k];
    int off = 0x67630 + tile / 8;
    P(off, sr_peek(c, off) & (uint8_t)~(1 << (tile % 8)));
  }
  return 1;
}

/* ------------------------------------------------------------------------------------------- difficulty processor */
static const char *const diff_names[6] = {"easy", "sortaeasy", "normal", "kindahard", "hard", "impossible"};
static const double player_values[6] = {0.6, 0.7, 0.8, 1.0, 1.2, 2.0};
static const double timed_values[6] = {320, 256, 200, 160, 128, 64};
static const double bosses_values[6] = {0.5, 1, 1.5, 2, 3, 4};

int sr_p4_hack_difficulty_processor(struct sr_ctx *c) {
  const char *enemyLevelType = "none";
  double difficultyLevel = 0.0;
  const char *difficulty = S("opDifficulty");
  int di = -1;
  for (int i = 0; i < 6; i++)
    if (!strcmp(difficulty, diff_names[i])) di = i;
  static const char *const growthTypes[4] = {"player", "timed", "bosses", "future"};
  const char *statGrowth = S("opStatGrowth");
  for (int k = 0; k < 4; k++) {
    if (!strstr(statGrowth, growthTypes[k])) continue;
    enemyLevelType = growthTypes[k];
    if (k < 3) {
      if (di < 0) sr_fail(c, SOMRANDO_ERR_FAILED, "The given key was not present in the dictionary.");
      difficultyLevel = k == 0 ? player_values[di] : k == 1 ? timed_values[di] : bosses_values[di];
    } else {
      difficultyLevel = 1.0;
    }
  }
  if (di < 0) sr_fail(c, SOMRANDO_ERR_FAILED, "The given key was not present in the dictionary.");
  sr_work_set(c, SR_P4_ENEMY_LEVEL_TYPE, enemyLevelType);
  sr_p4_sel_work_set_double(c, SR_P4_ENEMY_DIFFICULTY_LEVEL, difficultyLevel);
  sr_p4_sel_work_set_double(c, SR_P4_ENEMY_DIFFICULTY_STAT_SCALE, player_values[di]);
  sr_p4_sel_work_set_double(c, SR_P4_BOSS_DIFFICULTY_INCREMENT, bosses_values[di]);
  sr_p4_sel_work_set_double(c, SR_P4_ENEMY_DIFFICULTY_TIMER_INC, timed_values[di]);
  return 1;
}

/* --------------------------------------------------------------------------------------- DontAutoResetAtFirstMenu */
int sr_p4_hack_dont_auto_reset(struct sr_ctx *c) {
  P(0x28EF, 0x80);
  return 1;
}

/* ------------------------------------------------------------------------------------------------------ preflight */
int sr_p4_sel_supports(const struct sr_settings *s, char *why, size_t why_len) {
  if (sr_settings_get(s, "plandoSettings")[0]) {
    snprintf(why, why_len, "unsupported option: plandoSettings is not supported");
    return 0;
  }
  return 1;
}
