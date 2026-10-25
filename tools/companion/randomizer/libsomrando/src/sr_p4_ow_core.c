/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Open World search entry point and shared helpers.  Derived from processing/openworld/randomization/{OpenWorldRandomizer,
 * StartingWeaponRandomizer,PrizeItem,PrizeLocation}.cs (Author: Moppleton).  Translated to C 2026-10-18.
 *
 * The plando setting is only supported empty (PlandoUtils then leaves context.plandoSettings empty), so every plando branch is absent. */
#include <ctype.h>
#include <math.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "sr_p4.h"
#include "sr_p4_ow.h"
#include "sr_p4_ow_core_int.h"

#define W_BOY_EXISTS "boyExists"
#define W_GIRL_EXISTS "girlExists"
#define W_SPRITE_EXISTS "spriteExists"
#define W_STARTING_CHARACTER "startingCharacter"
#define W_BOY_WEAPON "boyStartWeapon"
#define W_GIRL_WEAPON "girlStartWeapon"
#define W_SPRITE_WEAPON "spriteStartWeapon"

/* ------------------------------------------------------------------------------------------------------------ helpers */
const char **sr_p4_core_sa(struct sr_ctx *c, ...) {
  va_list ap;
  int n = 0;
  va_start(ap, c);
  while (va_arg(ap, const char *)) n++;
  va_end(ap);
  const char **a = (const char **)sr_alloc(c, (size_t)(n + 1) * sizeof *a);
  va_start(ap, c);
  for (int i = 0; i < n; i++) a[i] = va_arg(ap, const char *);
  va_end(ap);
  a[n] = NULL;
  return a;
}

const char *sr_p4_core_cat(struct sr_ctx *c, ...) {
  va_list ap;
  size_t len = 0;
  const char *s;
  va_start(ap, c);
  while ((s = va_arg(ap, const char *)) != NULL) len += strlen(s);
  va_end(ap);
  char *out = (char *)sr_alloc(c, len + 1);
  size_t o = 0;
  va_start(ap, c);
  while ((s = va_arg(ap, const char *)) != NULL) {
    size_t n = strlen(s);
    memcpy(out + o, s, n);
    o += n;
  }
  va_end(ap);
  out[o] = 0;
  return out;
}

void sr_p4_core_sl_add(struct sr_ctx *c, struct ow_slist *l, const char *s) {
  if (l->n == l->cap) {
    int nc = l->cap ? l->cap * 2 : 8;
    const char **n = (const char **)sr_alloc(c, (size_t)nc * sizeof *n);
    if (l->n) memcpy(n, l->p, (size_t)l->n * sizeof *n);
    l->p = n;
    l->cap = nc;
  }
  l->p[l->n++] = s;
}

const char **sr_p4_core_sl_arr(struct sr_ctx *c, const struct ow_slist *l) {
  const char **a = (const char **)sr_alloc(c, (size_t)(l->n + 1) * sizeof *a);
  if (l->n) memcpy(a, l->p, (size_t)l->n * sizeof *a);
  a[l->n] = NULL;
  return a;
}

int sr_p4_core_sl_contains(const struct ow_slist *l, const char *s) {
  for (int i = 0; i < l->n; i++)
    if (!strcmp(l->p[i], s)) return 1;
  return 0;
}

void sr_p4_core_il_add(struct sr_ctx *c, struct ow_ilist *l, int v) {
  if (l->n == l->cap) {
    int nc = l->cap ? l->cap * 2 : 8;
    int *n = (int *)sr_alloc(c, (size_t)nc * sizeof *n);
    if (l->n) memcpy(n, l->p, (size_t)l->n * sizeof *n);
    l->p = n;
    l->cap = nc;
  }
  l->p[l->n++] = v;
}

int sr_p4_core_ilist_contains(const struct ow_ilist *l, int v) {
  for (int i = 0; i < l->n; i++)
    if (l->p[i] == v) return 1;
  return 0;
}

int sr_p4_core_il_remove(struct ow_ilist *l, int v) {
  for (int i = 0; i < l->n; i++)
    if (l->p[i] == v) {
      memmove(l->p + i, l->p + i + 1, (size_t)(l->n - i - 1) * sizeof *l->p);
      l->n--;
      return 1;
    }
  return 0;
}

void sr_p4_core_sg_set(int *key, int *val, int *n, int k, int v) {
  for (int i = 0; i < *n; i++)
    if (key[i] == k) { val[i] = v; return; }
  key[*n] = k;
  val[*n] = v;
  (*n)++;
}

static int count_strs(const char **a) {
  int n = 0;
  while (a[n]) n++;
  return n;
}

struct ow_location *sr_p4_core_loc_new(struct sr_ctx *c, const char *name, int map, int obj, int ev, int idx, const char **opts,
                                       const char **hints, const char **locked, double reach) {
  struct ow_location *l = (struct ow_location *)sr_alloc(c, sizeof *l);
  l->name = name;
  l->map_num = map;
  l->obj_num = obj;
  l->event_num = ev;
  l->event_replacement_index = idx;
  l->prize_type_options = opts;
  l->n_prize_type_options = count_strs(opts);
  l->location_hints = hints;
  l->n_location_hints = count_strs(hints);
  l->locked_by_prizes = locked;
  l->n_locked_by_prizes = count_strs(locked);
  l->reachability = reach;
  return l;
}

void sr_p4_core_loc_add(struct sr_ctx *c, struct ow_locs *L, const char *name, int map, int obj, int ev, int idx, const char **opts,
                        const char **hints, const char **locked, double reach) {
  struct ow_location *l = sr_p4_core_loc_new(c, name, map, obj, ev, idx, opts, hints, locked, reach);
  if (L->n == L->cap) {
    int nc = L->cap ? L->cap * 2 : 128;
    struct ow_location **n = (struct ow_location **)sr_alloc(c, (size_t)nc * sizeof *n);
    if (L->n) memcpy(n, L->v, (size_t)L->n * sizeof *n);
    L->v = n;
    L->cap = nc;
  }
  L->v[L->n++] = l;
}

void sr_p4_core_prize_add(struct sr_ctx *c, struct ow_prizes *P, const char *name, const char *type, const struct sr_bytes *data,
                          const char *hint, int flag, double value) {
  struct ow_prize *p = (struct ow_prize *)sr_alloc(c, sizeof *p);
  p->name = name;
  p->type = type;
  p->hint_name = hint;
  p->event_len = (int)data->len;
  p->event_data = (uint8_t *)sr_alloc(c, data->len ? data->len : 1);
  if (data->len) memcpy(p->event_data, data->p, data->len);
  p->got_item_event_flag = flag & 0xFF;
  p->value = value;
  p->uid = P->n;
  if (P->n == P->cap) {
    int nc = P->cap ? P->cap * 2 : 128;
    struct ow_prize **n = (struct ow_prize **)sr_alloc(c, (size_t)nc * sizeof *n);
    if (P->n) memcpy(n, P->v, (size_t)P->n * sizeof *n);
    P->v = n;
    P->cap = nc;
  }
  P->v[P->n++] = p;
}

/* ---------------------------------------------------------------------------------------------- workingData accessors */
int sr_p4_core_w_bool(const struct sr_ctx *c, const char *key) {
  const char *s = sr_work_get(c, key);
  if (!s) return 0;
  while (*s && isspace((unsigned char)*s)) s++;
  size_t n = strlen(s);
  while (n && isspace((unsigned char)s[n - 1])) n--;
  char b[8];
  if (n >= sizeof b) return 0;
  for (size_t i = 0; i < n; i++) b[i] = (char)tolower((unsigned char)s[i]);
  b[n] = 0;
  return !strcmp(b, "yes") || !strcmp(b, "true") || !strcmp(b, "1");
}

const char *sr_p4_core_w_str(const struct sr_ctx *c, const char *key) {
  const char *s = sr_work_get(c, key);
  return s ? s : "";
}

int sr_p4_core_w_int(struct sr_ctx *c, const char *key) {
  const char *s = sr_work_get(c, key);
  char *end;
  if (!s || !*s) sr_fail(c, SOMRANDO_ERR_FAILED, "workingData %s: input string was not in a correct format", key);
  long v = strtol(s, &end, 10);
  if (*end || v > 2147483647L || v < -2147483647L - 1) sr_fail(c, SOMRANDO_ERR_FAILED, "workingData %s: input string was not in a correct format", key);
  return (int)v;
}

const char *sr_p4_core_weapon_name(int weapon) {
  static const char *const n[] = {"Glove", "Sword", "Axe", "Spear", "Whip", "Bow", "Boomerang", "Javelin"};
  return weapon >= 0 && weapon < 8 ? n[weapon] : "";
}

const char *sr_p4_core_element_name(int element) {
  switch (element) {
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

int sr_p4_core_orb_has(struct sr_ctx *c, int map_num) {
  char key[32];
  snprintf(key, sizeof key, "orbElement%d", map_num);
  return sr_work_get(c, key) != NULL;
}

int sr_p4_core_orb_element(struct sr_ctx *c, int map_num) {
  char key[32];
  snprintf(key, sizeof key, "orbElement%d", map_num);
  if (!sr_work_get(c, key)) sr_fail(c, SOMRANDO_ERR_FAILED, "The given key '%d' was not present in the dictionary.", map_num);
  return sr_p4_core_w_int(c, key) & 0xFF;
}

/* ------------------------------------------------------------------------------------------------ StartingWeaponRandomizer */
void sr_p4_core_set_starting_weapons(struct sr_ctx *c) {
  int boy_exists = sr_p4_core_w_bool(c, W_BOY_EXISTS), girl_exists = sr_p4_core_w_bool(c, W_GIRL_EXISTS);
  int sprite_exists = sr_p4_core_w_bool(c, W_SPRITE_EXISTS);
  const char *starting_char = sr_p4_core_w_str(c, W_STARTING_CHARACTER);
  int force = sr_settings_get_int(c->settings, "opStartWeapon");
  struct sr_random *r = &c->rf;
  struct ow_ilist idx = {0};
  for (int i = 0; i < 8; i++) sr_p4_core_il_add(c, &idx, i);
  int boy = -1, girl = -1, sprite = -1;

  if (boy_exists) {
    if (!strcmp(starting_char, "boy") && force != -1) {
      boy = force;
    } else {
      boy = idx.p[sr_random_next(r) % idx.n];
      while (force != -1 && boy == force) boy = idx.p[sr_random_next(r) % idx.n];
    }
    sr_p4_core_il_remove(&idx, boy);
  }
  if (girl_exists) {
    if (!strcmp(starting_char, "girl") && force != -1) {
      girl = force;
    } else {
      girl = idx.p[sr_random_next(r) % idx.n];
      while (force != -1 && girl == force) girl = idx.p[sr_random_next(r) % idx.n];
    }
    sr_p4_core_il_remove(&idx, girl);
  }
  if (sprite_exists) {
    if (!strcmp(starting_char, "sprite") && force != -1) {
      sprite = force;
    } else {
      sprite = idx.p[sr_random_next(r) % idx.n];
    }
    sr_p4_core_il_remove(&idx, sprite);
  }
  sr_work_set_int(c, W_BOY_WEAPON, boy);
  sr_work_set_int(c, W_GIRL_WEAPON, girl);
  sr_work_set_int(c, W_SPRITE_WEAPON, sprite);
}

/* --------------------------------------------------------------------------------------------------------- debug dump */
#ifdef SOMRANDO_DEBUG
static FILE *dump_file(void) {
  static FILE *f;
  static int tried;
  if (!tried) {
    const char *p = getenv("SOM_CORE_DUMP");
    tried = 1;
    if (p && *p) f = fopen(p, "w");
  }
  return f;
}
int sr_p4_core_dump_enabled(void) { return dump_file() != NULL; }
void sr_p4_core_dump(const char *fmt, ...) {
  FILE *f = dump_file();
  if (!f) return;
  va_list ap;
  va_start(ap, fmt);
  vfprintf(f, fmt, ap);
  va_end(ap);
  fputc('\n', f);
  fflush(f);
}
void sr_p4_core_dump_rng(struct sr_ctx *c, const char *tag) {
  FILE *f = dump_file();
  if (!f) return;
  fprintf(f, "%s %d %d", tag, c->rf.inext, c->rf.inextp);
  for (int i = 0; i < 56; i++) fprintf(f, " %d", (int)c->rf.seed_array[i]);
  fputc('\n', f);
  fflush(f);
}
static unsigned long long dbits(double d) {
  unsigned long long u;
  memcpy(&u, &d, sizeof u);
  return u;
}
static void dump_list(const char *pre, const char **a, int n) {
  FILE *f = dump_file();
  fprintf(f, " %s[", pre);
  for (int i = 0; i < n; i++) fprintf(f, "%s%s", i ? ";" : "", a[i]);
  fputc(']', f);
}
void sr_p4_core_dump_prizes(const struct ow_locs *locs, const struct ow_prizes *prizes) {
  FILE *f = dump_file();
  if (!f) return;
  for (int i = 0; i < prizes->n; i++) {
    const struct ow_prize *p = prizes->v[i];
    fprintf(f, "prize %d %s|%s|%s|%d|%016llx|", i, p->name, p->type, p->hint_name, p->got_item_event_flag, dbits(p->value));
    for (int k = 0; k < p->event_len; k++) fprintf(f, "%02x", p->event_data[k]);
    fputc('\n', f);
  }
  for (int i = 0; i < locs->n; i++) {
    const struct ow_location *l = locs->v[i];
    fprintf(f, "loc %d %s|%d|%d|%d|%d|%016llx", i, l->name, l->map_num, l->obj_num, l->event_num, l->event_replacement_index, dbits(l->reachability));
    dump_list("opts", l->prize_type_options, l->n_prize_type_options);
    dump_list("hints", l->location_hints, l->n_location_hints);
    dump_list("locked", l->locked_by_prizes, l->n_locked_by_prizes);
    fputc('\n', f);
  }
  fflush(f);
}
#else
int sr_p4_core_dump_enabled(void) { return 0; }
void sr_p4_core_dump(const char *fmt, ...) { (void)fmt; }
void sr_p4_core_dump_rng(struct sr_ctx *c, const char *tag) { (void)c; (void)tag; }
void sr_p4_core_dump_prizes(const struct ow_locs *locs, const struct ow_prizes *prizes) { (void)locs; (void)prizes; }
#endif

/* ------------------------------------------------------------------------------------------------------ OpenWorldRandomizer */
#define MAX_ITERS 1000

/* attemptRando: placeItems, then the simulation; NULL when placeItems failed */
static struct ow_sim_result *attempt_rando(struct sr_ctx *c, struct sr_p4_core_search *s, struct ow_placements *placed) {
  int placed_items = sr_p4_core_place_items(c, s, placed);
  if (placed_items) return sr_p4_core_run_simulation(c, s, placed);
  return NULL;
}

int sr_p4_hack_open_randomizer(struct sr_ctx *c) {
  char why[200];
  if (!sr_p4_core_supports(c->settings, why, sizeof why)) sr_fail(c, SOMRANDO_ERR_UNSUPPORTED, "%s", why);
  sr_p4_core_dump_rng(c, "rf-start");
  sr_p4_core_set_starting_weapons(c);
  sr_p4_core_dump("weapons %d %d %d", sr_p4_core_w_int(c, W_BOY_WEAPON), sr_p4_core_w_int(c, W_GIRL_WEAPON), sr_p4_core_w_int(c, W_SPRITE_WEAPON));
  struct ow_locs *locs = (struct ow_locs *)sr_alloc(c, sizeof *locs);
  struct ow_prizes *prizes = (struct ow_prizes *)sr_alloc(c, sizeof *prizes);
  sr_p4_core_locations(c, locs);
  sr_p4_core_prizes(c, prizes, locs);
  sr_p4_core_dump_prizes(locs, prizes);
  sr_p4_core_dump_rng(c, "rf-prizes");
  const char *complexity = sr_settings_get(c->settings, "opComplexity");
  struct sr_p4_core_search *search = sr_p4_core_search_new(c, locs, prizes);

  struct ow_placements *final_rand = NULL;
  struct ow_sim_result *final_result = NULL, *last_result = NULL;
  int iter = 0;
  if (!strcmp(complexity, "easy") || !strcmp(complexity, "hard")) {
    int hard = !strcmp(complexity, "hard");
    struct ow_sim_result *best_result = NULL;
    struct ow_placements *best_prizes = NULL;
    while (iter < MAX_ITERS) {
      struct ow_placements *placed = (struct ow_placements *)sr_alloc(c, sizeof *placed);
      struct ow_sim_result *result = attempt_rando(c, search, placed);
      if (result != NULL) {
        last_result = result;
        if (result->score > 0 && (best_result == NULL || (hard ? result->score < best_result->score : result->score > best_result->score))) {
          best_prizes = placed;
          best_result = result;
        }
      }
      iter++;
    }
    final_rand = best_prizes;
    final_result = best_result;
  } else {
    while (iter < MAX_ITERS) {
      struct ow_placements *placed = (struct ow_placements *)sr_alloc(c, sizeof *placed);
      struct ow_sim_result *result = attempt_rando(c, search, placed);
      if (result != NULL) {
        last_result = result;
        if (result->score > 0) {
          final_rand = placed;
          final_result = result;
          break;
        }
      }
      iter++;
    }
  }
  (void)last_result;
  if (final_rand == NULL) sr_fail(c, SOMRANDO_ERR_FAILED, "Open world generation failed!");

  if (sr_p4_core_dump_enabled()) {
    for (int i = 0; i < final_rand->n; i++) sr_p4_core_dump("place %s -> %s", final_rand->v[i].loc->name, final_rand->v[i].prize->name);
    {
      double sc = final_result->score;
      unsigned long long u;
      memcpy(&u, &sc, sizeof u);
      sr_p4_core_dump("score %016llx", u);
    }
    for (int i = 0; i < final_result->n_cycles; i++) {
      char line[8192];
      size_t o = (size_t)snprintf(line, sizeof line, "cycle %d:", i);
      for (int j = 0; j < final_result->cycle_len[i] && o < sizeof line - 200; j++) o += (size_t)snprintf(line + o, sizeof line - o, " %s;", final_result->cycles[i][j]);
      sr_p4_core_dump("%s", line);
    }
    sr_p4_core_dump("iter %d", iter);
    sr_p4_core_dump_rng(c, "rf-end");
  }
  sr_p4_ow_inject(c, final_rand);
  sr_p4_ow_add_hints(c, final_rand, final_result);
  sr_p4_ow_log_spoilers(c, final_rand, final_result);
  return 1;
}

/* ------------------------------------------------------------------------------------------------------------ preflight */
int sr_p4_core_supports(const struct sr_settings *s, char *why, size_t why_len) {
  const char *goal = sr_settings_get(s, "opGoal");
  if (strcmp(goal, "vshort") && strcmp(goal, "vlong") && strcmp(goal, "mtr") && strcmp(goal, "gift") && strcmp(goal, "reindeer")) {
    snprintf(why, why_len, "unsupported option: opGoal=%s", goal);
    return 0;
  }
  const char *cx = sr_settings_get(s, "opComplexity");
  if (strcmp(cx, "easy") && strcmp(cx, "dontcare") && strcmp(cx, "hard")) {
    snprintf(why, why_len, "unsupported option: opComplexity=%s", cx);
    return 0;
  }
  const char *lg = sr_settings_get(s, "opLogic");
  if (strcmp(lg, "basic") && strcmp(lg, "restrictive")) {
    snprintf(why, why_len, "unsupported option: opLogic=%s", lg);
    return 0;
  }
  const char *plando = sr_settings_get(s, "plandoSettings");
  if (plando && *plando) {
    snprintf(why, why_len, "unsupported option: plandoSettings is not empty");
    return 0;
  }
  double mul = sr_settings_get_double(s, "opGoldCheckMul");
  if (!isfinite(mul) || mul < 0 || mul > 50) {
    snprintf(why, why_len, "unsupported option: opGoldCheckMul=%s", sr_settings_get(s, "opGoldCheckMul"));
    return 0;
  }
  return 1;
}
