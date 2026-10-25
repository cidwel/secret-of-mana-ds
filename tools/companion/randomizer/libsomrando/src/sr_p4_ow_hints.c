/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Open World hints.  Derived from processing/openworld/randomization/OpenWorldHints.cs and the populateDependencies part of OpenWorldLocations.cs
 * (Author: Moppleton).  Translated to C 2026-10-18.
 *
 * Publishes workingData nesoPrize<N> / nesoHint<N> (strings, N = 0..2, consumed by the Nesoberi hack) and rewrites the hint events.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "sr_p4.h"
#include "sr_p4_ow.h"
#include "sr_p4_ow_inject_int.h"
#include "sr_p4_ow_hints_data.h"

#define NUM_HINT_EVENTS ((int)(sizeof HINT_EVENTS / sizeof *HINT_EVENTS))
#define NUM_HINT_PHRASES ((int)(sizeof HINT_PHRASES / sizeof *HINT_PHRASES))
#define NUM_DUMB_HINTS ((int)(sizeof DUMB_HINTS / sizeof *DUMB_HINTS))

/* ORBMAP_GRANDPALACE_FIRST..LAST */
#define GP_ORB_FIRST 420
#define GP_ORB_LAST 426

enum special_hint { FIRE_PLACE_FINAL_PRICE, MECH_RIDER_3_PRICE, TOTAL_ORB_ELEMENTS_GRAND_PALACE };

static char *dup_str(struct sr_ctx *c, const char *s) {
  size_t n = strlen(s) + 1;
  char *p = (char *)sr_alloc(c, n);
  memcpy(p, s, n);
  return p;
}

static char *cat2(struct sr_ctx *c, const char *a, const char *b) {
  size_t la = strlen(a), lb = strlen(b);
  char *p = (char *)sr_alloc(c, la + lb + 1);
  memcpy(p, a, la);
  memcpy(p + la, b, lb + 1);
  return p;
}

/* string.Replace(pat, repl) with an ordinal, non-empty pat */
static char *replace_all(struct sr_ctx *c, const char *s, const char *pat, const char *repl) {
  size_t pl = strlen(pat), rl = strlen(repl), n = 0;
  for (const char *q = s; (q = strstr(q, pat)) != NULL; q += pl) n++;
  char *out = (char *)sr_alloc(c, strlen(s) + n * rl + 1), *o = out;
  const char *q;
  while ((q = strstr(s, pat)) != NULL) {
    memcpy(o, s, (size_t)(q - s));
    o += q - s;
    memcpy(o, repl, rl);
    o += rl;
    s = q + pl;
  }
  strcpy(o, s);
  return out;
}

/* VanillaEventUtil.wordWrapText: the text is split on every ' ' (empty tokens kept), lines break at 27 characters */
char *sr_p4_inject_word_wrap(struct sr_ctx *c, const char *text) {
  if (strchr(text, '\n')) return dup_str(c, text);
  size_t len = strlen(text);
  char *out = (char *)sr_alloc(c, len * 2 + 8), *o = out;
  int width = 0;
  const char *t = text;
  for (;;) {
    const char *sp = strchr(t, ' ');
    size_t tl = sp ? (size_t)(sp - t) : strlen(t);
    if ((int)tl + width < 27) {
      width += (int)tl + 1;
    } else {
      *o++ = '\n';
      width = (int)tl + 1;
    }
    memcpy(o, t, tl);
    o += tl;
    *o++ = ' ';
    if (!sp) break;
    t = sp + 1;
  }
  *o = 0;
  return out;
}

static int rnd_mod(struct sr_ctx *c, int n) {
  int v = sr_random_next(&c->rf);
  if (n == 0) sr_fail(c, SOMRANDO_ERR_FAILED, "Attempted to divide by zero.");
  return v % n;
}

static int wd_bool(const struct sr_ctx *c, const char *key) {
  const char *s = sr_p4_inject_work_str(c, key);
  while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r') s++;
  size_t n = strlen(s);
  while (n && (s[n - 1] == ' ' || s[n - 1] == '\t' || s[n - 1] == '\n' || s[n - 1] == '\r')) n--;
  return (n == 3 && !strncasecmp(s, "yes", 3)) || (n == 4 && !strncasecmp(s, "true", 4)) || (n == 1 && s[0] == '1');
}

static const char *pname(const struct ow_prize *z) { return z->name ? z->name : ""; }

/* crystalOrbColorMap[mapNum] as built by ElementSwaps.getCrystalOrbElementMap: workingData orbElement<mapNum> cast to byte */
static int orb_color(struct sr_ctx *c, int map_num) {
  char key[32];
  snprintf(key, sizeof key, "orbElement%d", map_num);
  if (!sr_work_get(c, key)) sr_fail(c, SOMRANDO_ERR_FAILED, "The given key '%d' was not present in the dictionary.", map_num);
  return (uint8_t)sr_p4_inject_work_int(c, key);
}

static const char *element_name(int e) {
  switch (e) {
    case 0x81: return "gnome";
    case 0x82: return "undine";
    case 0x83: return "salamando";
    case 0x85: return "sylphid";
    case 0x84: return "lumina";
    case 0x86: return "shade";
    case 0x87: return "luna";
    case 0x88: return "dryad";
  }
  return "";
}

/* OpenWorldLocations.populateDependencies, grandPalaceBossDependencies only (the hints use no other list); sets *has_name per element name */
struct gp_deps {
  const char *items[32];
  int n;
};
static void deps_add(struct gp_deps *d, const char *s) { d->items[d->n++] = s; }
static int deps_has(const struct gp_deps *d, const char *s) {
  for (int i = 0; i < d->n; i++)
    if (!strcmp(d->items[i], s)) return 1;
  return 0;
}

static void populate_gp_dependencies(struct sr_ctx *c, struct gp_deps *d, int randomize_gp, int girl, int sprite) {
  deps_add(d, "whip");
  if (randomize_gp) {
    static const char *const names[8] = {"gnome spells", "undine spells", "salamando spells", "lumina spells", "sylphid spells", "shade spells", "luna spells", "dryad spells"};
    int any_valid = 0;
    if (girl || sprite) {
      for (int m = GP_ORB_FIRST; m <= GP_ORB_LAST; m++) {
        char key[32];
        snprintf(key, sizeof key, "orbElement%d", m);
        if (!sr_work_get(c, key)) continue;   /* the C# iterates the keys that exist */
        int e = orb_color(c, m);
        const char *item;
        if (e >= 0x81 && e <= 0x88) item = names[e - 0x81];
        else if (e == 0xFF) item = "no";
        else sr_fail(c, SOMRANDO_ERR_FAILED, "The given key '%d' was not present in the dictionary.", e);
        if (strcmp(item, "no")) {
          if (!deps_has(d, item)) deps_add(d, item);
          any_valid = 1;
        }
      }
    }
    if (any_valid) {
      if (girl) deps_add(d, "girlCaster");
      if (sprite) deps_add(d, "spriteCaster");
    }
  } else {
    if (girl && sprite) {
      static const char *const all[7] = {"gnome spells", "undine spells", "salamando spells", "sylphid spells", "lumina spells", "shade spells", "luna spells"};
      for (int i = 0; i < 7; i++) deps_add(d, all[i]);
      deps_add(d, "girlCaster");
      deps_add(d, "spriteCaster");
    } else if (girl) {
      deps_add(d, "salamando spells");
      deps_add(d, "sylphid spells");
      deps_add(d, "lumina spells");
      deps_add(d, "girlCaster");
    } else if (sprite) {
      static const char *const sp[6] = {"gnome spells", "undine spells", "salamando spells", "sylphid spells", "shade spells", "luna spells"};
      for (int i = 0; i < 6; i++) deps_add(d, sp[i]);
      deps_add(d, "spriteCaster");
    }
  }
}

static const char *get_hint_for_location(const struct ow_placements *p, const char *loc_name) {
  for (int i = 0; i < p->n; i++)
    if (!strcmp(p->v[i].loc->name, loc_name)) return p->v[i].prize && p->v[i].prize->hint_name ? p->v[i].prize->hint_name : "";
  return "";
}

static int has_item_at_location(const struct ow_placements *p, const char *loc_name) {
  for (int i = 0; i < p->n; i++)
    if (!strcmp(p->v[i].loc->name, loc_name)) return 1;
  return 0;
}

static void add_hint_event(struct sr_ctx *c, const char *phrase, int hint_event) {
  char *wrapped = sr_p4_inject_word_wrap(c, phrase);
  struct sr_bytes bytes = {0};
  sr_event_text_bytes(c, &bytes, wrapped);
  sr_p4_inject_replace_event_data(c, sr_p4_inject_event_get(c, hint_event), HINT_INJECTION_PATTERN, 4, bytes.p, bytes.len);
}

static int in_list(const char *const *l, int n, const char *s) {
  for (int i = 0; i < n; i++)
    if (!strcmp(l[i], s)) return 1;
  return 0;
}

void sr_p4_ow_add_hints(struct sr_ctx *c, const struct ow_placements *p, const struct ow_sim_result *sim) {
  const char *goal = sr_p4_inject_work_str(c, "openWorldGoalShortName");
  int allow_missed = sr_settings_get_bool(c->settings, "opAllowLockedItems");
  int random_gp = sr_settings_get_bool(c->settings, "opGPEleRando");
  int suppress = sr_settings_get_bool(c->settings, "opDisableHints");
  int more_gp_hints = !strcmp(goal, "manabeast") && !wd_bool(c, "fastManaFort");
  int any_spells = wd_bool(c, "anyMagicExists");
  int girl_spells = wd_bool(c, "girlMagicExists");
  int sprite_spells = wd_bool(c, "spriteMagicExists");
  struct gp_deps *gp = (struct gp_deps *)sr_alloc(c, sizeof *gp);
  int n_valid = (sprite_spells ? 7 : 0) + (girl_spells ? 1 + (sprite_spells ? 0 : 2) : 0);   /* ElementSwaps.GetValidOrbElements().Count */
  populate_gp_dependencies(c, gp, random_gp, girl_spells, sprite_spells);

  const char *important[48];
  int n_imp = 0;
  static const char *const base[9] = {"boy", "boy", "girl", "girl", "sprite", "sprite", "whip", "axe", "sword"};
  for (int i = 0; i < 9; i++) important[n_imp++] = base[i];
  if (!strcmp(goal, "manatree")) {
    static const char *const seeds[8] = {"water seed", "earth seed", "wind seed", "fire seed", "light seed", "dark seed", "moon seed", "dryad seed"};
    for (int i = 0; i < 8; i++) important[n_imp++] = seeds[i];
  }
  if (!strcmp(goal, "xmas2020")) {
    for (int i = 0; i < 8; i++) {
      char key[32];
      snprintf(key, sizeof key, "giftSelection%d", i);
      const char *gift = sr_p4_inject_work_str(c, key);
      if (strstr(gift, "spells") || !strcmp(gift, "moogle belt") || !strcmp(gift, "midge mallet")) important[n_imp++] = gift;
    }
  } else {
    static const char *const sp[8] = {"undine spells", "gnome spells", "sylphid spells", "salamando spells", "lumina spells", "shade spells", "luna spells", "dryad spells"};
    for (int i = 0; i < 8; i++) important[n_imp++] = sp[i];
  }

  /* nesoberi hints */
  const char *neso_prize[3], *neso_hint[3];
  int n_neso = 0, attempts = 0;
  while (n_neso < 3 && attempts < 100) {
    const char *hint_prize = n_imp == 0 ? "" : important[rnd_mod(c, n_imp)];
    for (int i = 0; i < n_imp; i++)
      if (!strcmp(important[i], hint_prize)) {
        memmove(important + i, important + i + 1, (size_t)(n_imp - i - 1) * sizeof *important);
        n_imp--;
        break;
      }
    for (int k = 0; k < p->n; k++) {
      const struct ow_location *loc = p->v[k].loc;
      if (!strcmp(pname(p->v[k].prize), hint_prize)) {
        const char *hint = loc->location_hints[rnd_mod(c, loc->n_location_hints)];
        int have = 0;
        for (int j = 0; j < n_neso; j++)
          if (!strcmp(neso_prize[j], hint_prize)) have = 1;
        if (hint[0] != 0 && !have) {
          neso_prize[n_neso] = hint_prize;
          neso_hint[n_neso] = hint;
          n_neso++;
        }
      }
    }
    attempts++;
  }
  for (int i = 0; i < n_neso; i++) {
    char key[32];
    snprintf(key, sizeof key, "nesoPrize%d", i);
    sr_work_set(c, key, neso_prize[i]);
    snprintf(key, sizeof key, "nesoHint%d", i);
    sr_work_set(c, key, neso_hint[i]);
  }

  const char **hinted = (const char **)sr_alloc(c, (size_t)(NUM_HINT_EVENTS + 1) * sizeof *hinted);
  int n_hinted = 0;
  int gp_chance = more_gp_hints ? 3 : 5;

  int remaining[3], n_rem = 0;
  remaining[n_rem++] = MECH_RIDER_3_PRICE;
  if (has_item_at_location(p, "fire seed")) remaining[n_rem++] = FIRE_PLACE_FINAL_PRICE;
  if (random_gp && any_spells) remaining[n_rem++] = TOTAL_ORB_ELEMENTS_GRAND_PALACE;

  for (int h = 0; h < NUM_HINT_EVENTS; h++) {
    int hint_event = HINT_EVENTS[h];
    int useful = !suppress && sr_random_next_max(&c->rf, 100) < 80;
    if (useful) {
      char *phrase;
      int special = n_rem > 0 && sr_random_next_max(&c->rf, 5) == 0;
      if (special) {
        int ri = sr_random_next_max(&c->rf, n_rem);
        if (ri < 0 || ri >= n_rem) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range. Must be non-negative and less than the size of the collection.");
        int type = remaining[ri];
        switch (type) {
          case FIRE_PLACE_FINAL_PRICE: phrase = cat2(c, "Fire palace final prize: ", get_hint_for_location(p, "fire seed")); break;
          case MECH_RIDER_3_PRICE: phrase = cat2(c, "Mech rider 3 prize: ", get_hint_for_location(p, "mech rider 3 (new item)")); break;
          default: {
            int uniq[7], nu = 0;
            for (int m = GP_ORB_FIRST; m <= GP_ORB_LAST; m++) {
              int e = orb_color(c, m), dup = 0;
              for (int j = 0; j < nu; j++)
                if (uniq[j] == e) dup = 1;
              if (!dup) uniq[nu++] = e;
            }
            char num[16];
            snprintf(num, sizeof num, "%d", nu);
            phrase = cat2(c, "Total elements needed for grand palace: ", num);
            break;
          }
        }
        memmove(remaining + ri, remaining + ri + 1, (size_t)(n_rem - ri - 1) * sizeof *remaining);
        n_rem--;
      } else if (random_gp && any_spells && sr_random_next_max(&c->rf, gp_chance) == 0) {
        int ele = (uint8_t)(0x81 + sr_random_next_max(&c->rf, n_valid));
        char dep[32];
        snprintf(dep, sizeof dep, "%s spells", element_name(ele));
        phrase = cat2(c, deps_has(gp, dep) ? "Needed for grand palace: " : "Not needed for grand palace: ", element_name(ele));
      } else {
        phrase = dup_str(c, HINT_PHRASES[rnd_mod(c, NUM_HINT_PHRASES)]);
        int ki = rnd_mod(c, p->n);
        const struct ow_location *location = p->v[ki].loc;
        const struct ow_prize *prize_item = p->v[ki].prize;
        const char *prize = pname(prize_item);
        int i = 0;
        if (strstr(prize, "seed")) {
          ki = rnd_mod(c, p->n);
          location = p->v[ki].loc;
          prize_item = p->v[ki].prize;
          prize = pname(prize_item);
        }
        while (i < 100 && (strstr(prize, "starter weapon") || strstr(prize, "nothing") || strstr(prize, "orb") || strstr(prize, "GP") || in_list(hinted, n_hinted, prize))) {
          ki = rnd_mod(c, p->n);
          location = p->v[ki].loc;
          prize_item = p->v[ki].prize;
          prize = pname(prize_item);
          i++;
        }
        hinted[n_hinted++] = prize;
        const char *hint_loc = location->location_hints[rnd_mod(c, location->n_location_hints)];
        if (allow_missed) {
          if (!sim) sr_fail(c, SOMRANDO_ERR_FAILED, "Object reference not set to an instance of an object.");
          int found = 0;
          for (int a = 0; a < sim->n_cycles && !found; a++)
            for (int b = 0; b < sim->cycle_len[a]; b++)
              if (!strcmp(sim->cycles[a][b], location->name)) {
                found = 1;
                break;
              }
          if (!found) hint_loc = "nowhere";
        }
        phrase = replace_all(c, phrase, "%1", prize_item->hint_name ? prize_item->hint_name : "");
        phrase = replace_all(c, phrase, "%2", hint_loc);
      }
      add_hint_event(c, phrase, hint_event);
    } else {
      add_hint_event(c, DUMB_HINTS[rnd_mod(c, NUM_DUMB_HINTS)], hint_event);
    }
  }
}
