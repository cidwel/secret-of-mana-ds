/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Derived from SoMRandomizer/config/settings/StringValueSettings.cs, CommonSettings.cs, RandoSettings.cs and VanillaRandoSettings.cs
 * (Author: Moppleton).  Translated to C 2026-10-18.
 *
 * Every option is a string, like in StringValueSettings; a mode sees its own options and the common ones in one namespace
 * (RandoSettings.get falls through to the common settings).  The tables below are checked against `somrando --list-options`
 * by tests/diff_harness.py, so a default or a key that drifts from the C# is a test failure.
 */
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include "sr_core.h"

enum { OPT_BOOL, OPT_ENUM, OPT_INT, OPT_LIST, OPT_NUM, OPT_TEXT };

struct sr_opt {
  const char *key;
  int type;
  const char *def;
  const char *legal;   /* '|' separated, enums only */
  int internal;        /* not settable by the caller */
};

#define B(k, d) {k, OPT_BOOL, d, "yes|no", 0}
static const struct sr_opt common_opts[] = {
  B("debugLoggingEnabled", "no"), B("ovrchrgFix", "yes"), B("leavePlayersBehind", "yes"), B("seReduction", "yes"), B("aggBosses", "no"),
  B("funSpeedup", "yes"), B("rainbowRabites", "yes"), B("longStatus", "yes"), B("fasterChests", "yes"), B("magicRebalance", "yes"),
  B("mpAtLevel", "yes"), B("fasterMagicLevel", "yes"), B("fasterWeaponLevel", "yes"), B("fasterMessages", "yes"), B("moreItems", "yes"),
  B("starterGear", "no"), B("noBsVampires", "yes"), B("orbFix", "yes"), B("bossDeathFix", "yes"), B("speciesDamage", "yes"),
  B("elemDamage", "yes"), B("fastTransition", "no"), B("accuratePercent", "no"), B("noStaminaRun", "no"), B("statusGlow", "yes"),
  B("mode7Fix", "yes"), B("noGigasSplit", "yes"), B("whipFix", "yes"), B("offscreenTarget", "yes"), B("buyMultiple", "yes"),
  B("manaMagicFix", "yes"), B("ohko", "no"), B("noSummonDespawn", "yes"), B("noclip", "no"), B("confusionFix", "yes"),
  B("hittableBeaks", "no"), B("mechRiderDeathFix", "yes"), B("adjustEnemyPos", "yes"), B("attacksForever", "no"), B("deathByPoison", "no"),
  B("percentPoison", "no"), B("aggEnemies", "no"), B("obscureDamage", "no"), B("obscureHp", "no"), B("obscureGold", "no"),
  B("defRefactor", "no"), B("muteMusic", "no"), B("showEvades", "yes"), B("bossFixedZ", "no"), B("bossElementRando", "no"),
  B("nameEntryFix", "yes"), B("enemyInfiniteMp", "no"), B("trashWeapons", "yes"), B("showMinimap", "yes"), B("cupAtZeroHp", "yes"),
  B("candyHealouts", "yes"), B("magicRopeDeathFix", "yes"), B("spoilerLog", "yes"), B("raceMode", "no"), B("testOnly", "no"),
  {"version", OPT_ENUM, "1.49", "", 1}, {"buildDate", OPT_ENUM, "unknown", "", 1},
  {"mode", OPT_ENUM, "open", "rando|open|ancientcave|bossrush|chaos", 1},
  {"characterColors", OPT_ENUM, "rando", "none|rando|custom", 0},
  {"dmgCancelAllow", OPT_ENUM, "all", "all|consumables|none", 0},
  {"currentGenerateProgress", OPT_INT, "0", "", 1},
};

static const struct sr_opt rando_opts[] = {
  B("rEnemies", "yes"), B("rBosses", "yes"), B("rOrbs", "yes"), B("rElements", "yes"), B("rWeapons", "yes"), B("rMusic", "yes"),
  B("rAutosave", "yes"), B("rDialogue", "yes"), B("rPreserve", "no"),
  {"rExp", OPT_ENUM, "double", "half|normal|double|triple", 0},
  {"rGold", OPT_ENUM, "double", "half|normal|double|triple", 0},
  {"rHoliday", OPT_ENUM, "none", "none|halloween|xmas", 0},
  {"rDifficulty", OPT_ENUM, "100", "20|40|60|80|100|125|150|175|200", 0},
  {"rStatusAilments", OPT_ENUM, "location", "location|type|easy|annoying|awful", 0},
};
static const struct sr_opt bossrush_opts[] = {
  B("brBoy", "yes"), B("brGirl", "yes"), B("brSprite", "yes"), B("brLimitMpAbsorb", "yes"),
  {"brDifficulty", OPT_ENUM, "casual", "casual|hard|reallyhard|custom", 0},
};
static const struct sr_opt chaos_opts[] = {
  B("chBoy", "yes"), B("chGirl", "yes"), B("chSprite", "yes"), B("chHealSpellsFirst", "yes"), B("chEasyEarlyFloors", "yes"),
  {"chMapColors", OPT_ENUM, "reasonable", "none|reasonable|ridiculous", 0},
  {"chFloors", OPT_ENUM, "avg", "vfew|few|avg|many|vmany", 0},
  {"chDifficulty", OPT_ENUM, "casual", "casual|hard|reallyhard|custom", 0},
  {"chBosses", OPT_ENUM, "avg", "vfew|few|avg|many|vmany", 0},
};
#include "sr_p4_opts.h"
static const struct sr_opt ancientcave_opts[] = {
  B("acBoy", "yes"), B("acGirl", "yes"), B("acSprite", "yes"), B("acFilter", "no"), B("acMusic", "yes"),
  {"acLength", OPT_ENUM, "medium", "short|medium|long", 0},
  {"BossFrequency", OPT_ENUM, "everyfew", "every|everyfew|final", 0},
  {"acDialogue", OPT_ENUM, "mitch", "mitch|demetri", 0},
  {"acDifficulty", OPT_ENUM, "casual", "casual|hard|reallyhard|custom", 0},
  #ifdef SOMRANDO_DEBUG
  {"acBiomeTypes", OPT_LIST, "forest,island,ruins,cave,manafortinterior", "forest|island|ruins|cave|manafortinterior|test", 0},
#else
  {"acBiomeTypes", OPT_LIST, "forest,island,ruins,cave,manafortinterior", "forest|island|ruins|cave|manafortinterior", 0},
#endif
};
#undef B

static const struct sr_opt *mode_opts(const char *mode, int *n) {
  if (mode && !strcmp(mode, "bossrush")) { *n = (int)(sizeof bossrush_opts / sizeof *bossrush_opts); return bossrush_opts; }
  if (mode && !strcmp(mode, "chaos")) { *n = (int)(sizeof chaos_opts / sizeof *chaos_opts); return chaos_opts; }
  if (mode && !strcmp(mode, "open")) { *n = (int)(sizeof open_opts / sizeof *open_opts); return open_opts; }
  if (mode && !strcmp(mode, "ancientcave")) { *n = (int)(sizeof ancientcave_opts / sizeof *ancientcave_opts); return ancientcave_opts; }
  *n = (int)(sizeof rando_opts / sizeof *rando_opts);
  return rando_opts;
}

struct sr_setting {
  const struct sr_opt *opt;
  char *val;
};

struct sr_settings {
  struct sr_arena *arena;
  struct sr_setting *items;
  int n;
};

int sr_settings_known_mode(const char *mode) { return mode && (!strcmp(mode, "rando") || !strcmp(mode, "bossrush") || !strcmp(mode, "chaos") || !strcmp(mode, "ancientcave") || !strcmp(mode, "open")); }

struct sr_settings *sr_settings_new(struct sr_arena *a, const char *mode) {
  if (!sr_settings_known_mode(mode)) return NULL;
  int nc = (int)(sizeof common_opts / sizeof *common_opts), nr;
  const struct sr_opt *mopts = mode_opts(mode, &nr);
  struct sr_settings *s = (struct sr_settings *)sr_arena_alloc(a, sizeof *s);
  if (!s) return NULL;
  s->arena = a;
  s->items = (struct sr_setting *)sr_arena_alloc(a, (size_t)(nc + nr) * sizeof *s->items);
  if (!s->items) return NULL;
  for (int i = 0; i < nc + nr; i++) {
    const struct sr_opt *o = i < nc ? &common_opts[i] : &mopts[i - nc];
    s->items[i].opt = o;
    s->items[i].val = (char *)o->def;   /* defaults are literals; replaced (never written through) on set */
  }
  s->n = nc + nr;
  sr_settings_set(s, "mode", mode);
  return s;
}

static struct sr_setting *find(const struct sr_settings *s, const char *key) {
  for (int i = 0; i < s->n; i++)
    if (!strcmp(s->items[i].opt->key, key)) return &s->items[i];
  return NULL;
}

void sr_settings_set(struct sr_settings *s, const char *key, const char *val) {
  struct sr_setting *it = find(s, key);
  if (!it) return;
  size_t n = strlen(val) + 1;
  char *copy = (char *)sr_arena_alloc(s->arena, n);
  if (!copy) return;
  memcpy(copy, val, n);
  it->val = copy;
}

const char *sr_settings_get(const struct sr_settings *s, const char *key) {
  struct sr_setting *it = find(s, key);
  return it ? it->val : "";
}

static int ci_equal(const char *a, size_t n, const char *lit) {
  if (strlen(lit) != n) return 0;
  for (size_t i = 0; i < n; i++)
    if (tolower((unsigned char)a[i]) != lit[i]) return 0;
  return 1;
}

/* StringValueSettings.stringToBool: BOOLEAN_TRUE_VALUES.Contains(str.ToLower().Trim()) */
static int string_to_bool(const char *v) {
  while (*v && isspace((unsigned char)*v)) v++;
  size_t n = strlen(v);
  while (n && isspace((unsigned char)v[n - 1])) n--;
  return ci_equal(v, n, "yes") || ci_equal(v, n, "true") || ci_equal(v, n, "1");
}

int sr_settings_get_bool(const struct sr_settings *s, const char *key) { return string_to_bool(sr_settings_get(s, key)); }

double sr_settings_get_double(const struct sr_settings *s, const char *key) {
  const char *v = sr_settings_get(s, key);
  char *end;
  double x = strtod(v, &end);
  return (end == v || *end) ? (double)NAN : x;
}

int sr_settings_get_int(const struct sr_settings *s, const char *key) {
  const char *v = sr_settings_get(s, key);
  char *end;
  long x = strtol(v, &end, 10);
  if (end == v) return 0;
  return (int)x;
}

static int in_list(const char *list, const char *v, size_t n) {
  const char *p = list;
  while (*p) {
    const char *q = strchr(p, '|');
    size_t len = q ? (size_t)(q - p) : strlen(p);
    if (len == n && !strncmp(p, v, n)) return 1;
    if (!q) break;
    p = q + 1;
  }
  return 0;
}

int sr_settings_apply_lines(struct sr_settings *s, const char *lines, char *err, size_t err_len) {
  if (!lines) return 0;
  const char *p = lines;
  while (*p) {
    const char *nl = strchr(p, '\n');
    size_t len = nl ? (size_t)(nl - p) : strlen(p);
    const char *eq = memchr(p, '=', len);
    if (eq && eq > p) {
      const char *k = p, *ke = eq, *v = eq + 1, *ve = p + len;
      while (k < ke && isspace((unsigned char)*k)) k++;
      while (ke > k && isspace((unsigned char)ke[-1])) ke--;
      while (v < ve && isspace((unsigned char)*v)) v++;
      while (ve > v && isspace((unsigned char)ve[-1])) ve--;
      if (ke > k) {
        char key[64], val[64];
        if ((size_t)(ke - k) >= sizeof key || (size_t)(ve - v) >= sizeof val) {
          snprintf(err, err_len, "unsupported option: line too long");
          return SOMRANDO_ERR_UNSUPPORTED;
        }
        memcpy(key, k, (size_t)(ke - k)); key[ke - k] = 0;
        memcpy(val, v, (size_t)(ve - v)); val[ve - v] = 0;
        struct sr_setting *it = find(s, key);
        if (!it || it->opt->internal) {
          snprintf(err, err_len, "unsupported option: unknown option '%s' for mode %s", key, sr_settings_get(s, "mode"));
          return SOMRANDO_ERR_UNSUPPORTED;
        }
        if (it->opt->type == OPT_BOOL) {
          int ok = ci_equal(val, strlen(val), "yes") || ci_equal(val, strlen(val), "true") || ci_equal(val, strlen(val), "1") ||
                   ci_equal(val, strlen(val), "no") || ci_equal(val, strlen(val), "false") || ci_equal(val, strlen(val), "0");
          if (!ok) {
            snprintf(err, err_len, "unsupported option: '%s' is not a yes/no value for %s", val, key);
            return SOMRANDO_ERR_UNSUPPORTED;
          }
          sr_settings_set(s, key, string_to_bool(val) ? "yes" : "no");
        } else if (it->opt->type == OPT_LIST) {
          /* StringValueSettings accepts any text; the port only takes a non-empty comma separated subset of the legal values */
          const char *q = val;
          int ok = *q != 0;
          while (ok && *q) {
            const char *comma = strchr(q, ',');
            size_t len = comma ? (size_t)(comma - q) : strlen(q);
            if (!in_list(it->opt->legal, q, len) || len == 0) ok = 0;
            if (!comma) break;
            q = comma + 1;
            if (!*q) ok = 0;
          }
          if (!ok) {
            snprintf(err, err_len, "unsupported option: '%s' is not a list of legal values for %s (%s)", val, key, it->opt->legal);
            return SOMRANDO_ERR_UNSUPPORTED;
          }
          sr_settings_set(s, key, val);
        } else if (it->opt->type == OPT_NUM) {
          /* "min|max|step": a plain decimal number on the step grid inside the range (the C# accepts any text and parses it later) */
          char *end;
          double x = strtod(val, &end), lo, hi, st;
          int plain = val[0] && !*end && (isdigit((unsigned char)val[0])) && !strpbrk(val, "eExXnNiI+-");
          if (sscanf(it->opt->legal, "%lf|%lf|%lf", &lo, &hi, &st) != 3) plain = 0;
          double q = plain && st > 0 ? (x - lo) / st : 0;
          if (!plain || x < lo || x > hi || q != (double)(long)q) {
            snprintf(err, err_len, "unsupported option: '%s' is not a legal value for %s (%s)", val, key, it->opt->legal);
            return SOMRANDO_ERR_UNSUPPORTED;
          }
          sr_settings_set(s, key, val);
        } else if (it->opt->type == OPT_TEXT) {
          sr_settings_set(s, key, val);
        } else {
          if (!in_list(it->opt->legal, val, strlen(val))) {
            snprintf(err, err_len, "unsupported option: '%s' is not a legal value for %s (%s)", val, key, it->opt->legal);
            return SOMRANDO_ERR_UNSUPPORTED;
          }
          sr_settings_set(s, key, val);
        }
      }
    }
    if (!nl) break;
    p = nl + 1;
  }
  return 0;
}

void sr_settings_dump_defaults(const char *mode, FILE *f, int with_legal) {
  if (!sr_settings_known_mode(mode)) return;
  for (size_t i = 0; i < sizeof common_opts / sizeof *common_opts; i++) {
    if (common_opts[i].internal) continue;
    fprintf(f, "common %s=%s", common_opts[i].key, common_opts[i].def);
    if (with_legal) fprintf(f, " [%s]", common_opts[i].legal);
    fputc('\n', f);
  }
  int nm;
  const struct sr_opt *mopts = mode_opts(mode, &nm);
  for (int i = 0; i < nm; i++) {
    fprintf(f, "%s %s=%s", mode, mopts[i].key, mopts[i].def);
    if (with_legal) fprintf(f, " [%s]", mopts[i].legal);
    fputc('\n', f);
  }
}
