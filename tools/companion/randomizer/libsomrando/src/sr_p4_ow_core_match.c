/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Open World logic: pairs locations with prizes and simulates the result.  Derived from processing/openworld/randomization/
 * {OpenWorldLocationPrizeMatcher,OpenWorldSimulator,PrizeLocation}.cs (Author: Moppleton).  Translated to C 2026-10-18.
 *
 * Only the goals without GiftModeProcessing are ported (the Christmas goals are refused by sr_p4_core_supports) and plando is absent, so
 * the filtered location list is the full list, plandoLocations is empty and bypassValidation is false.  PrizeLocation.getLockedByPrizes
 * depends only on constants of one generation here, so it is evaluated once per location. */
#include <stdlib.h>
#include <string.h>

#include "sr_p4.h"
#include "sr_p4_ow.h"
#include "sr_p4_ow_core_int.h"
#include "sr_p4_ev1.h"

#include "sr_p4_ow_core_lists.inc"

struct sr_p4_core_search {
  struct ow_locs *locs;        /* allLocations */
  struct ow_prizes *prizes;
  int n_all;
  const char ***all_locked;    /* getLockedByPrizes per entry of allLocations */
  int *n_all_locked;
  int gift;                    /* goal is the gift delivery mode */
  int num_gifts;
  struct ow_location **fl;     /* filteredLocations of the current attempt */
  int *src;                    /* index in allLocations of each filtered entry, -1 for the santa locations */
  int n;                       /* number of filtered locations */
  int flammie;
  int force100;                /* !opAllowLockedItems */
  int allow_locked;
  const char *upper;
  const char *goal;
  const char ***locked;        /* getLockedByPrizes(...) per location, NULL terminated */
  int *n_locked;
  struct ow_prize ***avail;    /* allAvailablePrizes: scratch pools, one per location */
  int *avail_n;
  struct ow_prize **placed;    /* itemPlacements per location index (NULL: absent) */
};

static int in_list(const char *const *l, int n, const char *s) {
  for (int i = 0; i < n; i++)
    if (!strcmp(l[i], s)) return 1;
  return 0;
}

/* PrizeLocation.getLockedByPrizes(flammieDrumInLogic, _upperLandElement, goal) */
static const char **locked_by_prizes(struct sr_ctx *c, const struct ow_location *l, int flammie, const char *upper, const char *goal, int *n_out) {
  struct ow_slist all = {0};
  for (int i = 0; i < l->n_locked_by_prizes; i++) sr_p4_core_sl_add(c, &all, l->locked_by_prizes[i]);
  if (flammie) {
    if (in_list(ow_flammie_required, OW_FLAMMIE_REQUIRED_N, l->name)) {
      sr_p4_core_sl_add(c, &all, "flammie drum");
      if (strcmp(upper, "no")) sr_p4_core_sl_add(c, &all, sr_p4_core_cat(c, upper, " spells", NULL));
    } else if (!strcmp(goal, "xmas2020") || !strcmp(goal, "xmas2020b")) {
      if (!in_list(ow_upperland_required, OW_UPPERLAND_REQUIRED_N, l->name) && strcmp(upper, "no"))
        sr_p4_core_sl_add(c, &all, sr_p4_core_cat(c, upper, " spells", NULL));
    } else if (in_list(ow_upperland_required, OW_UPPERLAND_REQUIRED_N, l->name) && strcmp(upper, "no")) {
      sr_p4_core_sl_add(c, &all, sr_p4_core_cat(c, upper, " spells", NULL));
    }
  }
  *n_out = all.n;
  return sr_p4_core_sl_arr(c, &all);
}

struct sr_p4_core_search *sr_p4_core_search_new(struct sr_ctx *c, struct ow_locs *locs, struct ow_prizes *prizes) {
  struct sr_p4_core_search *s = (struct sr_p4_core_search *)sr_alloc(c, sizeof *s);
  s->locs = locs;
  s->prizes = prizes;
  s->n_all = locs->n;
  s->goal = sr_p4_core_w_str(c, "openWorldGoalShortName");
  s->gift = !strcmp(s->goal, "xmas2020");
  s->num_gifts = s->gift ? sr_settings_get_int(c->settings, "opXmasGifts") : 0;
  s->flammie = sr_settings_get_bool(c->settings, "opFlammieDrumInLogic");
  s->allow_locked = sr_settings_get_bool(c->settings, "opAllowLockedItems");
  s->force100 = !s->allow_locked;
  s->upper = "no";
  if (s->flammie) {
    int any_spells = sr_p4_core_w_bool(c, "anyMagicExists");
    s->upper = !any_spells ? "no" : sr_p4_core_element_name(sr_p4_core_orb_element(c, 41));
  }
  /* Dictionary<PrizeLocation, ...> keys compare by name: the table must not contain two equal names */
  for (int i = 0; i < locs->n; i++)
    for (int j = i + 1; j < locs->n; j++)
      if (!strcmp(locs->v[i]->name, locs->v[j]->name)) sr_fail(c, SOMRANDO_ERR_UNSUPPORTED, "unsupported option: duplicate location name %s", locs->v[i]->name);
  int cap = locs->n + 8;
  s->all_locked = (const char ***)sr_alloc(c, (size_t)(locs->n + 1) * sizeof *s->all_locked);
  s->n_all_locked = (int *)sr_alloc(c, (size_t)(locs->n + 1) * sizeof *s->n_all_locked);
  s->fl = (struct ow_location **)sr_alloc(c, (size_t)cap * sizeof *s->fl);
  s->src = (int *)sr_alloc(c, (size_t)cap * sizeof *s->src);
  s->locked = (const char ***)sr_alloc(c, (size_t)cap * sizeof *s->locked);
  s->n_locked = (int *)sr_alloc(c, (size_t)cap * sizeof *s->n_locked);
  s->avail = (struct ow_prize ***)sr_alloc(c, (size_t)cap * sizeof *s->avail);
  s->avail_n = (int *)sr_alloc(c, (size_t)cap * sizeof *s->avail_n);
  s->placed = (struct ow_prize **)sr_alloc(c, (size_t)cap * sizeof *s->placed);
  for (int i = 0; i < locs->n; i++) {
    s->all_locked[i] = locked_by_prizes(c, locs->v[i], s->flammie, s->upper, s->goal, &s->n_all_locked[i]);
    s->avail[i] = (struct ow_prize **)sr_alloc(c, (size_t)(prizes->n + 1) * sizeof **s->avail);
  }
  for (int i = locs->n; i < cap; i++) s->avail[i] = (struct ow_prize **)sr_alloc(c, (size_t)(prizes->n + 1) * sizeof **s->avail);
  return s;
}

/* GiftModeProcessing.processLocations: fills out (capacity n_all + 8) and returns the count; the santa locations are fresh objects */
static int filter_locations(struct sr_ctx *c, const struct ow_locs *all, int gift, int num_gifts, struct ow_location **out, int *src) {
  int n = 0;
  if (!gift) {
    for (int i = 0; i < all->n; i++) {
      if (src) src[n] = i;
      out[n++] = all->v[i];
    }
    return n;
  }
  if (num_gifts < 0 || num_gifts > 8) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range.");
  int npc_events[8];
  for (int g = 0; g < num_gifts; g++) {
    char key[40];
    snprintf(key, sizeof key, "giftDelivery%d", g);
    int npc_id = sr_p4_core_w_int(c, key);
    if (npc_id < 0 || npc_id >= sr_p4_ev1_gift_location_count) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was outside the bounds of the array.");
    npc_events[g] = sr_p4_ev1_gift_locations[npc_id].event_id;
  }
  for (int i = 0; i < all->n; i++) {
    int removed = 0;
    for (int g = 0; g < num_gifts; g++)
      if (npc_events[g] == all->v[i]->event_num) removed = 1;
    if (!removed) {
      if (src) src[n] = i;
      out[n++] = all->v[i];
    }
  }
  static const char *hints[] = {"from Santa", NULL};
  for (int i = 0; i < num_gifts - 1; i++) {
    char name[40];
    snprintf(name, sizeof name, "santa gift %d", i);
    char *nn = (char *)sr_alloc(c, strlen(name) + 1);
    memcpy(nn, name, strlen(name) + 1);
    if (src) src[n] = -1;
    out[n] = sr_p4_core_loc_new(c, nn, -1, -1, sr_p4_ev1_santa_dialogue_events[i * 2 + 1], 0, sr_p4_core_sa(c, NULL), hints, sr_p4_core_sa(c, NULL), 1.0 - i * 0.1);
    n++;
  }
  return n;
}

int sr_p4_core_filtered_count(struct sr_ctx *c, const struct ow_locs *locs) {
  struct ow_location **tmp = (struct ow_location **)sr_alloc(c, (size_t)(locs->n + 8) * sizeof *tmp);
  const char *goal = sr_p4_core_w_str(c, "openWorldGoalShortName");
  int gift = !strcmp(goal, "xmas2020");
  return filter_locations(c, locs, gift, gift ? sr_settings_get_int(c->settings, "opXmasGifts") : 0, tmp, NULL);
}

/* List<PrizeItem>.Remove(item): first occurrence */
static void pool_remove(struct ow_prize **pool, int *n, const struct ow_prize *item) {
  for (int i = 0; i < *n; i++)
    if (pool[i] == item) {
      memmove(pool + i, pool + i + 1, (size_t)(*n - i - 1) * sizeof *pool);
      (*n)--;
      return;
    }
}

/* removeByName: every prize with that name */
static void remove_by_name(struct ow_prize **pool, int *n, const char *name) {
  int o = 0;
  for (int i = 0; i < *n; i++)
    if (strcmp(pool[i]->name, name)) pool[o++] = pool[i];
  *n = o;
}

static struct ow_prize *lookup_by_name(const struct ow_prizes *prizes, const char *name) {
  for (int i = 0; i < prizes->n; i++)
    if (!strcmp(prizes->v[i]->name, name)) return prizes->v[i];
  return NULL;
}

int sr_p4_core_place_items(struct sr_ctx *c, struct sr_p4_core_search *s, struct ow_placements *out) {
  struct sr_random *r = &c->rf;
  struct ow_location **fl = s->fl;
  int *src = s->src;
  s->n = filter_locations(c, s->locs, s->gift, s->num_gifts, fl, src);
  int n = s->n;
  for (int i = 0; i < n; i++) {
    if (src[i] >= 0) {
      s->locked[i] = s->all_locked[src[i]];
      s->n_locked[i] = s->n_all_locked[src[i]];
    } else {
      s->locked[i] = locked_by_prizes(c, fl[i], s->flammie, s->upper, s->goal, &s->n_locked[i]);
    }
  }
  /* default to everything available everywhere */
  for (int i = 0; i < n; i++) {
    struct ow_prize **pool = s->avail[i];
    int pn = s->prizes->n;
    memcpy(pool, s->prizes->v, (size_t)pn * sizeof *pool);
    if (in_list(ow_flammie_required, OW_FLAMMIE_REQUIRED_N, fl[i]->name)) {
      struct ow_prize *flammie = NULL;
      for (int k = 0; k < pn; k++)
        if (!strcmp(pool[k]->name, "flammie drum")) flammie = pool[k];
      if (flammie) pool_remove(pool, &pn, flammie);
    }
    s->avail_n[i] = pn;
    s->placed[i] = NULL;
  }
  out->v = (struct ow_placement *)sr_alloc(c, (size_t)(n + 1) * sizeof *out->v);
  out->n = 0;

  for (int i = 0; i < n; i++) {
    const struct ow_location *loc = fl[i];
    struct ow_prize **avail = s->avail[i];
    int *an = &s->avail_n[i];
    if (!strcmp(loc->name, "starter weapon (main)")) {
      *an = 0;
      avail[(*an)++] = lookup_by_name(s->prizes, "starter weapon (main)");
    } else if (!strcmp(loc->name, "starter weapon (alt)")) {
      *an = 0;
      avail[(*an)++] = lookup_by_name(s->prizes, "starter weapon (alt)");
    } else {
      remove_by_name(avail, an, "starter weapon (main)");
      remove_by_name(avail, an, "starter weapon (alt)");
    }
    if (strstr(loc->name, "chest")) remove_by_name(avail, an, "nothing");
    else if (!strcmp(loc->name, "watts (axe)")) remove_by_name(avail, an, "nothing");

    const char **pl_locked = s->locked[i];
    for (int k = 0; k < s->n_locked[i]; k++) {
      const char *lock = pl_locked[k];
      remove_by_name(avail, an, lock);
      if (!strcmp(lock, "lumina spells")) remove_by_name(avail, an, "girl");
      else if (strstr(lock, " spells")) remove_by_name(avail, an, "sprite");
    }
    if (*an == 0) return 0;

    struct ow_prize *this_prize = avail[sr_random_next(r) % *an];
    if (this_prize == NULL) sr_fail(c, SOMRANDO_ERR_FAILED, "Object reference not set to an instance of an object.");
    s->placed[i] = this_prize;
    out->v[out->n].loc = fl[i];
    out->v[out->n].prize = this_prize;
    out->n++;

    for (int j = 0; j < n; j++) {
      pool_remove(s->avail[j], &s->avail_n[j], this_prize);
      if (in_list(s->locked[j], s->n_locked[j], this_prize->name)) {
        struct ow_prize **avail2 = s->avail[j];
        const char *existing = "";
        if (s->placed[j]) existing = s->placed[j]->name;
        if (s->force100) {
          for (int k = 0; k < s->n_locked[i]; k++) {
            remove_by_name(avail2, &s->avail_n[j], pl_locked[k]);
            if (!strcmp(existing, pl_locked[k])) return 0;
          }
        }
      }
    }
  }
  /* inject santa gifts' dependencies based on what all we picked */
  if (s->gift) {
    static const char *const possible_gifts[] = {
      "midge mallet", "moogle belt", "dryad spells", "luna spells", "lumina spells", "shade spells", "undine spells", "salamando spells", "gnome spells",
      "sylphid spells", "1000 gold", "5000 gold", "10000 gold", "hat", "hat#", "accessory", "accessory#", "armor", "armor#", "candy", "chocolate",
      "royaljam", "walnut", "barrel"};
    const char *pool[24];
    int np = (int)(sizeof possible_gifts / sizeof *possible_gifts);
    memcpy(pool, possible_gifts, sizeof possible_gifts);
    const char *xmas[8];
    for (int i = 0; i < 8; i++) {
      int id = sr_random_next(r) % np;
      char key[40];
      xmas[i] = pool[id];
      snprintf(key, sizeof key, "giftSelection%d", i);
      sr_work_set(c, key, pool[id]);
      memmove(pool + id, pool + id + 1, (size_t)(np - id - 1) * sizeof *pool);
      np--;
    }
    struct ow_slist running = {0};
#define RUN_ADD(dep_) do { if (!sr_p4_core_sl_contains(&running, (dep_))) sr_p4_core_sl_add(c, &running, (dep_)); } while (0)
    for (int i = 0; i < s->num_gifts - 1; i++) {
      char key[40];
      snprintf(key, sizeof key, "giftDelivery%d", i);
      int npc_id = sr_p4_core_w_int(c, key);
      const struct sr_p4_ev1_npc *npc = &sr_p4_ev1_gift_locations[npc_id];
      for (int d = 0; d < npc->n_item_dependencies; d++) RUN_ADD(npc->item_dependencies[d]);
      for (int a = 0; a < s->n_all; a++)
        if (s->locs->v[a]->event_num == npc->event_id)
          for (int d = 0; d < s->n_all_locked[a]; d++) RUN_ADD(s->all_locked[a][d]);
      char santa[40];
      snprintf(santa, sizeof santa, "santa gift %d", i);
      for (int k = 0; k < out->n; k++) {
        if (strcmp(out->v[k].loc->name, santa)) continue;
        for (int l = 0; l < n; l++)
          if (!strcmp(fl[l]->name, out->v[k].loc->name)) {
            for (int d = 0; d < s->n_locked[l]; d++) RUN_ADD(s->locked[l][d]);
            break;
          }
        break;
      }
      size_t xl = strlen(xmas[i]);
      if ((xl >= 6 && !strcmp(xmas[i] + xl - 6, "spells")) || !strcmp(xmas[i], "moogle belt") || !strcmp(xmas[i], "midge mallet")) {
        RUN_ADD(xmas[i]);
        for (int k = 0; k < out->n; k++) {
          if (strcmp(out->v[k].prize->name, xmas[i])) continue;
          for (int l = 0; l < n; l++)
            if (!strcmp(fl[l]->name, out->v[k].loc->name)) {
              for (int d = 0; d < s->n_locked[l]; d++) RUN_ADD(s->locked[l][d]);
              break;
            }
          break;
        }
      }
      for (int l = 0; l < n; l++)
        if (!strcmp(fl[l]->name, santa)) {
          struct ow_location *sl = fl[l];
          sl->locked_by_prizes = sr_p4_core_sl_arr(c, &running);
          sl->n_locked_by_prizes = running.n;
          s->locked[l] = locked_by_prizes(c, sl, s->flammie, s->upper, s->goal, &s->n_locked[l]);
        }
    }
#undef RUN_ADD
  }
  return 1;
}

/* ------------------------------------------------------------------------------------------------------------- simulator */
static const char *weapon_lower(int w) {
  static const char *const n[] = {"glove", "sword", "axe", "spear", "whip", "bow", "boomerang", "javelin"};
  return w >= 0 && w < 8 ? n[w] : "";
}

struct sim_names {
  const char **p;
  int n;
};

static int has(const struct sim_names *g, const char *s) {
  for (int i = 0; i < g->n; i++)
    if (!strcmp(g->p[i], s)) return 1;
  return 0;
}

static int has_any(const struct sim_names *g, const char *const *l, int n) {
  int any = 0;
  for (int i = 0; i < n; i++)
    if (has(g, l[i])) any = 1;
  return any;
}

struct ow_sim_result *sr_p4_core_run_simulation(struct sr_ctx *c, struct sr_p4_core_search *s, const struct ow_placements *p) {
  struct ow_sim_result *result = (struct ow_sim_result *)sr_alloc(c, sizeof *result);
  int startWithBoy = sr_p4_core_w_bool(c, "startWithBoy");
  int startWithGirl = sr_p4_core_w_bool(c, "startWithGirl");
  int startWithSprite = sr_p4_core_w_bool(c, "startWithSprite");
  const char *boyClass = sr_p4_core_w_str(c, "boyClass");
  const char *girlClass = sr_p4_core_w_str(c, "girlClass");
  const char *spriteClass = sr_p4_core_w_str(c, "spriteClass");
  int allowLockedItems = s->allow_locked;
  const char *goal = s->goal;
  int mtrManaSeeds = sr_p4_core_w_int(c, "manaSeedsRequired");
  int boyStarterWeapon = sr_p4_core_w_int(c, "boyStartWeapon");
  int girlStarterWeapon = sr_p4_core_w_int(c, "girlStartWeapon");
  int spriteStarterWeapon = sr_p4_core_w_int(c, "spriteStartWeapon");
  int n = p->n;

  int cap = 16 + 2 * n;
  struct sim_names gotten = {(const char **)sr_alloc(c, (size_t)cap * sizeof(char *)), 0};
  struct sim_names temp = {(const char **)sr_alloc(c, (size_t)cap * sizeof(char *)), 0};
  int *visited = (int *)sr_alloc(c, (size_t)(n + 1) * sizeof *visited);
  int n_visited = 0;
  result->score = 0.0;
  result->cycle_len = (int *)sr_alloc(c, (size_t)(n + 2) * sizeof(int));
  result->cycles = (const char ***)sr_alloc(c, (size_t)(n + 2) * sizeof(char **));
  const char **pool = (const char **)sr_alloc(c, (size_t)(n + 1) * sizeof(char *));
  int pool_used = 0;
  result->n_cycles = 0;

  if (startWithBoy) {
    gotten.p[gotten.n++] = "boy";
    gotten.p[gotten.n++] = weapon_lower(boyStarterWeapon);
  }
  if (startWithGirl) {
    gotten.p[gotten.n++] = "girl";
    gotten.p[gotten.n++] = weapon_lower(girlStarterWeapon);
  }
  if (startWithSprite) {
    gotten.p[gotten.n++] = "sprite";
    gotten.p[gotten.n++] = weapon_lower(spriteStarterWeapon);
  }

  const char *girl_chars[3], *sprite_chars[3];
  int n_girl = 0, n_sprite = 0;
  if (!strcmp(boyClass, "OGgirl")) girl_chars[n_girl++] = "boy";
  if (!strcmp(girlClass, "OGgirl")) girl_chars[n_girl++] = "girl";
  if (!strcmp(spriteClass, "OGgirl")) girl_chars[n_girl++] = "sprite";
  if (!strcmp(boyClass, "OGsprite")) sprite_chars[n_sprite++] = "boy";
  if (!strcmp(girlClass, "OGsprite")) sprite_chars[n_sprite++] = "girl";
  if (!strcmp(spriteClass, "OGsprite")) sprite_chars[n_sprite++] = "sprite";
  /* girlSpellChecks / spriteSpellChecks are the same lists as girlSpellsChars / spriteSpellsChars */

  static const char *const girl_only[] = {"lumina"};
  static const char *const both_spells[] = {"sylphid", "salamando"};
  static const char *const sprite_only[] = {"undine", "gnome", "shade", "dryad", "luna"};

  int simulating = 1;
  while (simulating) {
    int visited_before = n_visited;
    temp.n = 0;
    const char **cycle = pool + pool_used;
    int cycle_n = 0;
    for (int k = 0; k < n; k++) {
      if (visited[k]) continue;
      const struct ow_location *location = p->v[k].loc;
      const char **reqs = s->locked[k];
      int haveAllReqs = 1;
      for (int q = 0; q < s->n_locked[k]; q++) {
        const char *req = reqs[q];
        int haveThisReq = 0;
        if (!strcmp(req, "no spells") || !strcmp(req, " spells")) {
          haveThisReq = 1;
        } else if (!strcmp(req, "elinee")) {
          if (has(&gotten, "axe") || (has(&gotten, "whip") && (has(&gotten, "axe") || has(&gotten, "sword")))) haveThisReq = 1;
        } else if (!strcmp(req, "girlCaster")) {
          for (int i = 0; i < n_girl; i++)
            if (has(&gotten, girl_chars[i])) haveThisReq = 1;
        } else if (!strcmp(req, "spriteCaster")) {
          for (int i = 0; i < n_sprite; i++)
            if (has(&gotten, sprite_chars[i])) haveThisReq = 1;
        } else if (!strcmp(req, "cuttingWeapon")) {
          if (has(&gotten, "axe") || has(&gotten, "sword")) haveThisReq = 1;
        } else if (!strcmp(req, "matango")) {
          if (has(&gotten, "axe") || has(&gotten, "flammie drum")) haveThisReq = 1;
        } else {
          for (int g = 0; g < gotten.n; g++) {
            const char *item = gotten.p[g];
            if (!strcmp(item, req)) {
              haveThisReq = 1;
              if (strstr(item, " spells")) {
                char element[32];
                size_t el = strcspn(item, " ");
                if (el >= sizeof element) el = sizeof element - 1;
                memcpy(element, item, el);
                element[el] = 0;
                if (in_list(girl_only, 1, element)) {
                  if (!has_any(&gotten, girl_chars, n_girl)) haveThisReq = 0;
                } else if (in_list(sprite_only, 5, element)) {
                  if (!has_any(&gotten, sprite_chars, n_sprite)) haveThisReq = 0;
                } else if (in_list(both_spells, 2, element)) {
                  if (!(has_any(&gotten, girl_chars, n_girl) || has_any(&gotten, sprite_chars, n_sprite))) haveThisReq = 0;
                }
              }
            }
          }
        }
        if (!haveThisReq) haveAllReqs = 0;
      }

      if (haveAllReqs) {
        const struct ow_prize *prize = p->v[k].prize;
        double locationScore = location->reachability;
        double itemScore = prize->value;
        result->score += locationScore * itemScore;
        visited[k] = 1;
        n_visited++;
        cycle[cycle_n++] = location->name;
        temp.p[temp.n++] = prize->name;
        if (!strcmp(prize->name, "boy")) temp.p[temp.n++] = weapon_lower(boyStarterWeapon);
        if (!strcmp(prize->name, "girl")) temp.p[temp.n++] = weapon_lower(girlStarterWeapon);
        if (!strcmp(prize->name, "sprite")) temp.p[temp.n++] = weapon_lower(spriteStarterWeapon);
      }
    }
    if (visited_before == n_visited) simulating = 0;
    result->cycles[result->n_cycles] = cycle;
    result->cycle_len[result->n_cycles] = cycle_n;
    result->n_cycles++;
    pool_used += cycle_n;
    for (int i = 0; i < temp.n; i++) gotten.p[gotten.n++] = temp.p[i];
  }

  int simSuccessCondition = n_visited == n;
  if (allowLockedItems) {
    if (!strcmp(goal, "manabeast")) {
      simSuccessCondition = 0;
      for (int k = 0; k < n; k++)
        if (visited[k] && !strcmp(p->v[k].loc->name, "dread slime (new item)")) simSuccessCondition = 1;
    } else if (!strcmp(goal, "manatree")) {
      int canReachManaTree = 0, numSeeds = 0;
      for (int k = 0; k < n; k++)
        if (visited[k] && !strcmp(p->v[k].loc->name, "blue dragon (new item)")) canReachManaTree = 1;
      for (int g = 0; g < gotten.n; g++)
        if (strstr(gotten.p[g], " seed")) numSeeds++;
      simSuccessCondition = canReachManaTree && numSeeds >= mtrManaSeeds;
    }
  }
  if (!simSuccessCondition) result->score = -1;
  return result;
}
