/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Open World level scaling: CustomTopOfScreenText, MinMaxLevel and the stat-table half of EnemiesAtYourLevel (processing/hacks/openworld/,
 * Author: Moppleton).  The code-emitting half of EnemiesAtYourLevel is generated (tools/gen_p4_hlevel.py -> sr_p4_hk_level_gen.c).
 * Translated to C 2026-10-18.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sr_p4.h"
#include "sr_p4_hk_level_gen.h"

#define E(v) sr_emit(c, (v))
#define STAT 0x101C00
#define WEAP 0x101000
#define NSTAT 19
#define NOFF 261

/* ---------------------------------------------------------------------------------------------- workingData helpers */
static int work_bool(const struct sr_ctx *c, const char *key) {
  const char *v = sr_work_get(c, key);
  if (!v) return 0;
  char b[16];
  size_t n = 0;
  while (*v == ' ' && *v) v++;
  while (v[n] && n < sizeof b - 1) { b[n] = (char)((v[n] >= 'A' && v[n] <= 'Z') ? v[n] + 32 : v[n]); n++; }
  b[n] = 0;
  while (n && b[n - 1] == ' ') b[--n] = 0;
  return !strcmp(b, "yes") || !strcmp(b, "true") || !strcmp(b, "1");
}

static int work_int(struct sr_ctx *c, const char *key) {
  const char *v = sr_work_get(c, key);
  char *end;
  if (!v || !*v) sr_fail(c, SOMRANDO_ERR_FAILED, "workingData key '%s' is not an integer", key);
  long x = strtol(v, &end, 10);
  if (*end) sr_fail(c, SOMRANDO_ERR_FAILED, "workingData key '%s' is not an integer", key);
  return (int)x;
}

static double work_double(struct sr_ctx *c, const char *key) {
  const char *v = sr_work_get(c, key);
  char *end;
  if (!v || !*v) sr_fail(c, SOMRANDO_ERR_FAILED, "workingData key '%s' is not a number", key);
  double x = strtod(v, &end);
  if (*end) sr_fail(c, SOMRANDO_ERR_FAILED, "workingData key '%s' is not a number", key);
  return x;
}

/* context.initialValues[addr] = value: the same ordered pair list as the common InitialValues hack reads */
static void initial_value_set(struct sr_ctx *c, int addr, int value) {
  int a[128];
  int n = sr_work_get_int_array(c, "w3.initialValues", a, 128);
  if (n < 0) n = 0;
  for (int i = 0; i + 1 < n; i += 2)
    if (a[i] == addr) {
      a[i + 1] = value & 0xFF;
      sr_work_set_int_array(c, "w3.initialValues", a, n);
      return;
    }
  if (n + 2 > 128) sr_fail(c, SOMRANDO_ERR_FAILED, "too many initial values");
  a[n++] = addr;
  a[n++] = value & 0xFF;
  sr_work_set_int_array(c, "w3.initialValues", a, n);
}

/* ------------------------------------------------------------------------------------------- CustomTopOfScreenText */
int sr_p4_hack_custom_top_of_screen_text(struct sr_ctx *c) { return sr_p4_hlevel_custom_top_text(c); }

/* ------------------------------------------------------------------------------------------------------ MinMaxLevel */
int sr_p4_hack_min_max_level(struct sr_ctx *c) {
  int doMinLevel = sr_settings_get_bool(c->settings, "opMinLevel");
  int doMaxLevel = sr_settings_get_bool(c->settings, "opMaxLevel");
  sr_work_set_int(c, "enemyMinLevelOffset", 0);
  sr_work_set_int(c, "enemyMaxLevelOffset", 0);
  if (!doMinLevel && !doMaxLevel) return 0;
  uint8_t minLevels[512];
  memset(minLevels, 0, sizeof minLevels);
  for (int k = 0; k < sr_p4_hlevel_min_levels_n; k++) minLevels[sr_p4_hlevel_min_levels[k][0]] = (uint8_t)sr_p4_hlevel_min_levels[k][1];
  for (int i = 0; i < 512; i++)
    if (minLevels[i] != 0) minLevels[i]--;   /* off by one; 00 = level 01 */
  if (doMinLevel) {
    sr_ensure_space_in_bank(&c->working_offset, 512);
    sr_work_set_int(c, "enemyMinLevelOffset", c->working_offset + 0xC00000);
    for (int i = 0; i < 512; i++) E(minLevels[i]);
  }
  if (doMaxLevel) {
    sr_ensure_space_in_bank(&c->working_offset, 512);
    sr_work_set_int(c, "enemyMaxLevelOffset", c->working_offset + 0xC00000);
    for (int i = 0; i < 512; i++) E((uint8_t)(minLevels[i] + 5));
  }
  return 1;
}

/* ------------------------------------------------------------------------------------------------ EnemiesAtYourLevel */
struct dkey { int key; double v; };

static int dlook(const struct dkey *t, int n, int key, double def, double *out) {
  for (int i = 0; i < n; i++)
    if (t[i].key == key) { *out = t[i].v; return 1; }
  *out = def;
  return 0;
}

static const struct dkey HIGH_MUL[] = {{1, 1.6}, {4, 1.6}, {9, 3.5}, {18, 1.2}, {23, 1.2}, {24, 1.2}, {26, 1.3}, {260, 3.0}};
static const struct dkey LOW_MUL[] = {{1, 0.9}, {4, 0.8}, {23, 0.8}, {24, 0.8}, {26, 0.8}, {18, 0.8}};
static const struct dkey DIFF_THR[] = {{26, 0.05}, {260, 0.05}, {16, 10000}, {27, 10000}, {8, 0.5}, {9, 0.5}, {23, 0.3}};
static const struct dkey ZERO_VAL[] = {{1, 10}, {99, 1}, {4, 3}, {5, 1}, {18, 10}, {23, 7}};
static const struct dkey EXPONENT[] = {{1, 1.1}};
#define NT(t) ((int)(sizeof(t) / sizeof((t)[0])))

static const int SOURCE_OFFSETS[NSTAT] = {0, 1, 3, 4, 99, 5, 6, 7, 8, 9, 11, 12, 16, 18, 26, 260, 27, 23, 24};
static const int SIXTEEN_BIT[] = {1, 9, 12, 16, 27};
static const int ALT_LOADER[] = {4, 99, 5, 6, 7, 8, 9, 11, 12, 26, 260};

static const char *const DIFFS[6] = {"easy", "sortaeasy", "normal", "kindahard", "hard", "impossible"};
static const int DIFF_ADD[6] = {-2, -1, -1, -1, -1, 5};
static const int BOSS_ADD[6] = {3, 3, 3, 3, 3, 5};
static const int MIN_BOSS_LEVEL[6] = {3, 3, 4, 4, 5, 10};
static const int LEVEL_MAX[6] = {50, 60, 70, 80, 99, 99};
static const int EVADE_MAX[6] = {25, 40, 50, 75, 99, 99};
static const double EXTRA_MDEF[6] = {0.0, 1.0, 1.5, 2.0, 2.5, 4.0};
static const int WEAPMAG_MAX[6] = {4, 5, 6, 7, 8, 8};

static int in_ints(const int *l, int n, int v) {
  for (int i = 0; i < n; i++)
    if (l[i] == v) return 1;
  return 0;
}

static void set_ushorts(struct sr_p4_hlevel_state *st, int off, int n, int a, int b) {
  st->ramLocations[off].n = n;
  st->ramLocations[off].v[0] = (uint16_t)a;
  st->ramLocations[off].v[1] = (uint16_t)b;
}

/* ExperienceAdjust / GoldAdjust in Open World mode (the shared versions refuse every mode but the vanilla randomizer) */
static int open_scale_stat(struct sr_ctx *c, const char *key, int index) {
  double mul = sr_settings_get_double(c->settings, key);
  if (mul == 1.0) return 0;
  for (int i = 0; i <= 127; i++) {
    int at = STAT + i * 29 + index;
    int orig = sr_ushort_from_bytes(c->out, at);
    int nv = sr_clamp_to_endpoints((int)ceil(orig * mul), 0, 65535);
    sr_ushort_to_bytes(c->out, at, nv);
  }
  return 1;
}
static int open_exp_adjust(struct sr_ctx *c) { return open_scale_stat(c, "opExp", 16); }
static int open_gold_adjust(struct sr_ctx *c) { return open_scale_stat(c, "opGold", 0x1B); }

int sr_p4_hack_enemies_at_your_level(struct sr_ctx *c) {
  const char *difficulty = sr_settings_get(c->settings, "opDifficulty");
  double expMultiplier = sr_settings_get_double(c->settings, "opExp");
  double goldMultiplier = sr_settings_get_double(c->settings, "opGold");
  int randomizeBossElements = sr_settings_get_bool(c->settings, "bossElementRando");
  int vanillaManaBeast = !strcmp(sr_settings_get(c->settings, "opManaBeastScale"), "vanilla");
  int pauseTimerInMenu = sr_settings_get_bool(c->settings, "opPauseInMenu");
  uint8_t noFutureLevel = (uint8_t)sr_settings_get_int(c->settings, "opNoFutureLevel");

  double difficultyStatScale = work_double(c, "enemyDifficultyStatScale");
  const char *enemyLevelSource = sr_work_get(c, "enemyLevelType");
  if (!enemyLevelSource) enemyLevelSource = "";
  double bossIncrementValue = work_double(c, "enemyBossDifficultyIncValue") * 2.0;
  double timerIncrementValue = work_double(c, "enemyDifficultyTimerIncPeriod") / 2.0;
  int screenTextSubrLocation = work_int(c, "customTopOfScreenTextOffset");
  int minLevelTableLoc = work_int(c, "enemyMinLevelOffset");
  int maxLevelTableLoc = work_int(c, "enemyMaxLevelOffset");
  int startSolo = work_bool(c, "startSolo");

  initial_value_set(c, 0x7E0440, 0);   /* OPENWORLD_CURRENT_ENEMY_LEVEL */
  initial_value_set(c, 0x7E0438, 0);   /* TIMED_LEVEL_VALUE_8BIT */
  initial_value_set(c, 0x7ECF80, 1);   /* LEVELMATCH_BOSSES_KILLED */

  if (!strcmp(enemyLevelSource, "none")) {
    sr_p4_nested(c, "Experience scale", open_exp_adjust);
    sr_p4_nested(c, "Gold scale", open_gold_adjust);
    return 1;
  }

  int di = -1;
  for (int i = 0; i < 6; i++)
    if (!strcmp(difficulty, DIFFS[i])) di = i;
  if (di < 0) sr_fail(c, SOMRANDO_ERR_FAILED, "unknown difficulty '%s'", difficulty);

  struct sr_p4_hlevel_state *st = (struct sr_p4_hlevel_state *)sr_alloc(c, sizeof *st);
  memset(st, 0, sizeof *st);
  st->sourceOffsets.n = NSTAT;
  for (int i = 0; i < NSTAT; i++) st->sourceOffsets.v[i] = SOURCE_OFFSETS[i];
  st->sixteenBitOffsets.n = NT(SIXTEEN_BIT);
  for (int i = 0; i < NT(SIXTEEN_BIT); i++) st->sixteenBitOffsets.v[i] = SIXTEEN_BIT[i];
  st->altLoaderStats.n = NT(ALT_LOADER);
  for (int i = 0; i < NT(ALT_LOADER); i++) st->altLoaderStats.v[i] = ALT_LOADER[i];
  set_ushorts(st, 0, 1, 0xE181, 0);
  set_ushorts(st, 1, 2, 0xE182, 0xE184);
  set_ushorts(st, 3, 2, 0xE186, 0xE187);
  set_ushorts(st, 4, 1, 0xE188, 0);
  set_ushorts(st, 5, 1, 0xE189, 0);
  set_ushorts(st, 99, 1, 0xE18A, 0);
  set_ushorts(st, 6, 1, 0xE18B, 0);
  set_ushorts(st, 7, 1, 0xE18C, 0);
  set_ushorts(st, 8, 1, 0xE1A4, 0);
  set_ushorts(st, 9, 1, 0xE1A5, 0);
  set_ushorts(st, 11, 1, 0xE1A7, 0);
  set_ushorts(st, 12, 1, 0xE1A8, 0);
  set_ushorts(st, 16, 1, 0xE18D, 0);
  set_ushorts(st, 18, 2, 0xE1FD, 0xE1FE);
  set_ushorts(st, 26, 1, 0xE1C0, 0);
  set_ushorts(st, 260, 1, 0xE1C4, 0);
  set_ushorts(st, 27, 1, 0xE1C8, 0);

  /* enemy sets: normal (0..83 without 32/46/63) and bosses (87..127, then 32/46/63) */
  int normalEnemies[128], bosses[128];
  int nNormal = 0, nBoss = 0;
  for (int i = 0; i < 84; i++)
    if (i != 32 && i != 46 && i != 63) normalEnemies[nNormal++] = i;
  for (int i = 87; i < 128; i++) bosses[nBoss++] = i;
  bosses[nBoss++] = 32;
  bosses[nBoss++] = 46;
  bosses[nBoss++] = 63;
  const int *sets[2] = {normalEnemies, bosses};
  const int setN[2] = {nNormal, nBoss};

  for (int oi = 0; oi < NSTAT; oi++) {
    int offset = SOURCE_OFFSETS[oi];
    int srcOffset = offset == 260 ? 26 : offset;
    int sixteen = in_ints(SIXTEEN_BIT, NT(SIXTEEN_BIT), offset);
    double avgStatValuePerLevel[2];

    for (int es = 0; es < 2; es++) {
      double statValuePerLevel[128];
      int ns = 0;
      for (int k = 0; k < setN[es]; k++) {
        int i = sets[es][k];
        int val = 0;
        if (offset == 99) {
          val = 1;
        } else if (offset == 23 || offset == 24) {
          uint8_t weaponId = sr_peek_orig(c, STAT + i * 29 + srcOffset);
          val = sr_peek_orig(c, WEAP + weaponId * 12 + 8);
        } else if (offset == 26 || offset == 260) {
          uint8_t b = sr_peek_orig(c, STAT + i * 29 + srcOffset);
          val = offset == 26 ? (b & 0x0F) : ((b & 0xF0) >> 4);
        } else if (sixteen) {
          val = sr_ushort_from_bytes(c->orig, STAT + i * 29 + srcOffset);
        } else {
          val = sr_peek_orig(c, STAT + i * 29 + srcOffset);
        }
        int lev = sr_peek_orig(c, STAT + i * 29 + 0);
        if (lev == 0) lev = 50;
        statValuePerLevel[ns++] = val / (double)lev;
      }
      double avg = 0;
      for (int k = 0; k < ns; k++) avg += statValuePerLevel[k] / (double)ns;
      avgStatValuePerLevel[es] = avg;
    }

    if (offset == 0) continue;   /* no tables for the level itself */

    double highMultiplier, lowMultiplier, zeroValue, exponent;
    dlook(HIGH_MUL, NT(HIGH_MUL), offset, 2.0, &highMultiplier);
    dlook(LOW_MUL, NT(LOW_MUL), offset, 0.5, &lowMultiplier);
    dlook(ZERO_VAL, NT(ZERO_VAL), offset, 0.0, &zeroValue);
    dlook(EXPONENT, NT(EXPONENT), offset, 1.0, &exponent);
    double difficultyStatMultiplier = difficultyStatScale;
    int difficultyAddition = DIFF_ADD[di];
    int bossAddition = BOSS_ADD[di];

    int size = sixteen ? 6 : 3;
    sr_ensure_space_in_bank(&c->working_offset, 98 * size);
    st->valueTableLocations[offset] = c->working_offset;

    for (int i = 1; i <= 99; i++) {
      for (int e = 0; e < 2; e++) {
        int lev = i + difficultyAddition;
        if (e == 1) {
          lev += bossAddition;
          lev = lev > MIN_BOSS_LEVEL[di] ? lev : MIN_BOSS_LEVEL[di];
        }
        if (!startSolo) lev++;
        int levelMax = LEVEL_MAX[di];
        lev = lev > 1 ? lev : 1;
        lev = lev < levelMax ? lev : levelMax;
        double pw = pow((double)lev, exponent);
        int valueHigh = (int)(zeroValue + (avgStatValuePerLevel[e] * pw * difficultyStatMultiplier * highMultiplier));
        int valueAvg = (int)(zeroValue + (avgStatValuePerLevel[e] * pw * difficultyStatMultiplier));
        int valueLow = (int)(zeroValue + (avgStatValuePerLevel[e] * pw * difficultyStatMultiplier * lowMultiplier));
        if (offset == 16) {
          if (lev > 10) {
            valueAvg = (int)(7.806507 - 2.172297 * lev + 0.2366238 * lev * lev + 0.01093192 * lev * lev * lev);
          } else {
            static const int lowExpValues[] = {0, 1, 2, 3, 4, 7, 10, 12, 21, 30, 35};
            valueAvg = lowExpValues[lev];
          }
          if (valueAvg < 1) valueAvg = 1;
          if (e == 1) valueAvg *= 10;
          valueAvg = (int)(valueAvg * expMultiplier);
        }
        if (offset == 27) {
          if (lev > 10) {
            valueAvg = (int)(-40.17712 + 0.2992243 * lev + 0.6395421 * lev * lev);
          } else {
            static const int lowGoldValues[] = {0, 2, 5, 8, 11, 14, 17, 21, 25, 31, 36};
            valueAvg = lowGoldValues[lev];
          }
          if (valueAvg < 1) valueAvg = 1;
          if (e == 1) valueAvg *= 4;
          valueAvg = (int)(valueAvg * goldMultiplier);
        }
        if ((offset == 23 || offset == 24) && e == 1) {
          valueAvg = (int)(5 + lev * 1.4 * difficultyStatMultiplier);
          valueAvg = valueAvg > 8 ? valueAvg : 8;
        }
        if (offset == 8 || offset == 11) {
          valueLow = 0;
          valueAvg = 0;
          valueHigh = EVADE_MAX[di];
        }
        if (offset == 99) {
          valueLow = 1;
          valueAvg = 1;
          valueHigh = 1;
        }
        if (offset == 12) {
          valueLow += (int)(lev * EXTRA_MDEF[di]);
          valueAvg += (int)(lev * EXTRA_MDEF[di]);
          valueHigh += (int)(lev * EXTRA_MDEF[di]);
        }
        if (sixteen) {
          valueHigh = valueHigh < 65535 ? valueHigh : 65535;
          valueAvg = valueAvg < 65535 ? valueAvg : 65535;
          valueLow = valueLow < 65535 ? valueLow : 65535;
          E((uint8_t)valueLow);
          E((uint8_t)(valueLow >> 8));
          E((uint8_t)valueAvg);
          E((uint8_t)(valueAvg >> 8));
          E((uint8_t)valueHigh);
          E((uint8_t)(valueHigh >> 8));
        } else {
          int max = 255;
          if (offset == 26 || offset == 260) max = WEAPMAG_MAX[di];
          if (offset == 3) max = 40;
          valueHigh = valueHigh < max ? valueHigh : max;
          valueAvg = valueAvg < max ? valueAvg : max;
          valueLow = valueLow < max ? valueLow : max;
          if (offset == 3 && e == 1 && !randomizeBossElements) valueLow = 6;
          E((uint8_t)valueLow);
          E((uint8_t)valueAvg);
          E((uint8_t)valueHigh);
        }
      }
    }

    sr_ensure_space_in_bank(&c->working_offset, 128);
    st->classLocations[offset] = c->working_offset;

    for (int i = 0; i < 128; i++) {
      int typeIndex = 0;
      for (int e = 0; e < 2; e++)
        if (in_ints(sets[e], setN[e], i)) typeIndex = e;
      uint8_t level = sr_peek_orig(c, STAT + i * 29 + 0);
      int vanillaValue = 0;
      if (sixteen) {
        vanillaValue = sr_ushort_from_bytes(c->orig, STAT + i * 29 + srcOffset);
      } else if (offset == 99) {
        vanillaValue = 1;
      } else if (offset == 23 || offset == 24) {
        uint8_t weaponId = sr_peek_orig(c, STAT + i * 29 + srcOffset);
        vanillaValue = sr_peek_orig(c, WEAP + weaponId * 12 + 8);
        if (typeIndex == 1) {
          vanillaValue = (int)(5 + level * 1.4 * difficultyStatMultiplier);
          if (vanillaValue < 8) vanillaValue = 8;
        }
      } else if (offset == 26) {
        vanillaValue = sr_peek_orig(c, STAT + i * 29 + srcOffset) & 0x0F;
      } else if (offset == 260) {
        vanillaValue = (sr_peek_orig(c, STAT + i * 29 + srcOffset) & 0xF0) >> 4;
      } else {
        vanillaValue = sr_peek_orig(c, STAT + i * 29 + srcOffset);
      }

      uint8_t enemyClass;
      double diffThreshold;
      dlook(DIFF_THR, NT(DIFF_THR), offset, avgStatValuePerLevel[typeIndex] / 10.0, &diffThreshold);
      if ((vanillaValue - zeroValue) / (double)level < avgStatValuePerLevel[typeIndex] - diffThreshold)
        enemyClass = 0;
      else if ((vanillaValue - zeroValue) / (double)level > avgStatValuePerLevel[typeIndex] + diffThreshold)
        enemyClass = 2;
      else
        enemyClass = 1;

      if (typeIndex == 1 && offset == 3) enemyClass = vanillaValue == 99 ? 2 : 1;
      if (offset == 8 || offset == 11) enemyClass = vanillaValue == 0 ? 0 : 2;
      if (typeIndex == 1 && (offset == 23 || offset == 24)) enemyClass = 1;
      if (!randomizeBossElements && typeIndex == 1 && offset == 3 && (i == 95 || i == 113 || i == 122)) enemyClass = 0;
      if (offset == 260) enemyClass = 2;
      if ((offset == 9 || offset == 1) && !strcmp(difficulty, "impossible")) enemyClass = 0;
      enemyClass = (uint8_t)(enemyClass + typeIndex * 3);
      E(enemyClass);
    }
  }

  st->enemyLevelSource = enemyLevelSource;
  st->bossIncrementValue = bossIncrementValue;
  st->timerIncrementValue = timerIncrementValue;
  st->screenTextSubrLocation = screenTextSubrLocation;
  st->minLevelTableLoc = minLevelTableLoc;
  st->maxLevelTableLoc = maxLevelTableLoc;
  st->vanillaManaBeast = vanillaManaBeast;
  st->pauseTimerInMenu = pauseTimerInMenu;
  st->noFutureLevel = noFutureLevel;
  return sr_p4_hlevel_emit_tail(c, st);
}

/* ---------------------------------------------------------------------------------------------------------- supports */
int sr_p4_hlevel_supports(const struct sr_settings *s, char *why, size_t why_len) {
  (void)s; (void)why; (void)why_len;
  return 1;
}
