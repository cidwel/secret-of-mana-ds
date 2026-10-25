/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Phase 2: EnemySwaps, EnemyScaling, EnemyPositionAdjustments.  Author of every upstream hack: Moppleton.  Translated to C 2026-10-18.
 */
#include <math.h>
#include <string.h>

#include "sr_core.h"
#include "sr_p4.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))
#define S(key) sr_settings_get(c->settings, (key))
#define SB(key) sr_settings_get_bool(c->settings, (key))
#define STAT 0x101C00
#define WEAP 0x101000

static void bad_mode(struct sr_ctx *c, const char *what) { sr_fail(c, SOMRANDO_ERR_UNSUPPORTED, "unsupported option: %s", what); }

static int in_list(const int *l, int n, int v) {
  for (int i = 0; i < n; i++)
    if (l[i] == v) return 1;
  return 0;
}

static void list_remove_at(int *l, int *n, int idx) {
  memmove(l + idx, l + idx + 1, (size_t)(*n - idx - 1) * sizeof(int));
  (*n)--;
}

/* Swap tables of EnemySwaps: parallel arrays in Dictionary insertion order. */
struct swap_map {
  int old_id[84], new_id[84], n;
};

static void swap_stats(struct sr_ctx *c, const struct swap_map *m) {
  static const int eight[] = {0, 11, 26, 3, 4, 5, 6, 7, 8, 18, 19, 25};
  static const int sixteen[] = {1, 16, 27, 9, 12};
  for (int k = 0; k < m->n; k++) {
    int o = m->old_id[k], nw = m->new_id[k];
    for (int j = 0; j < 12; j++) P(STAT + nw * 29 + eight[j], c->orig[STAT + o * 29 + eight[j]]);
    for (int j = 0; j < 5; j++) {
      P(STAT + nw * 29 + sixteen[j], c->orig[STAT + o * 29 + sixteen[j]]);
      P(STAT + nw * 29 + sixteen[j] + 1, c->orig[STAT + o * 29 + sixteen[j] + 1]);
    }
  }
}

static void swap_weapons(struct sr_ctx *c, const struct swap_map *m, int location) {
  static const int idx_loc[] = {1, 2, 4, 5, 6, 7, 8, 9, 10, 11};
  static const int idx_other[] = {1, 2, 4, 5, 6, 7, 8, 11};
  const int *idx = location ? idx_loc : idx_other;
  int n = location ? 10 : 8;
  for (int k = 0; k < m->n; k++) {
    int o = m->old_id[k], nw = m->new_id[k];
    int old_w1 = c->orig[STAT + o * 29 + 23], old_w2 = c->orig[STAT + o * 29 + 24];
    int new_w1 = c->orig[STAT + nw * 29 + 23], new_w2 = c->orig[STAT + nw * 29 + 24];
    for (int j = 0; j < n; j++) {
      P(WEAP + new_w1 * 12 + idx[j], c->orig[WEAP + old_w1 * 12 + idx[j]]);
      P(WEAP + new_w2 * 12 + idx[j], c->orig[WEAP + old_w2 * 12 + idx[j]]);
    }
  }
}

static void swap_drops(struct sr_ctx *c, const struct swap_map *m) {
  for (int k = 0; k < m->n; k++)
    for (int i = 1; i < 5; i++) P(0x103A50 + m->new_id[k] * 5 + i, c->orig[0x103A50 + m->old_id[k] * 5 + i]);
}

int sr_p4_shared_no_enemies(struct sr_ctx *c);          /* sr_p4_shared_enemy.c */
int sr_p4_shared_enemy_randomizer(struct sr_ctx *c);

int sr_hack_enemy_swaps(struct sr_ctx *c) {
  const int is_open = !strcmp(S("mode"), "open");
  if (!is_open && strcmp(S("mode"), "rando")) { bad_mode(c, "enemy randomizer outside the vanilla randomizer and open world"); return 0; }
  int fixed_enemy = -1;
  const char *status;
  if (is_open) {
    status = S("opStatusAilments");
    const char *kind = S("opEnemies");
    if (strstr(kind, "oops")) {
      const char *every = S("oopsAllThis");
      if (!strcmp(every, "random")) {
        fixed_enemy = sr_random_next(&c->rf) % 83;
        while (fixed_enemy == 29 || fixed_enemy == 15 || fixed_enemy == 9 || fixed_enemy == 48) fixed_enemy = sr_random_next(&c->rf) % 83;
      } else {
        fixed_enemy = sr_settings_get_int(c->settings, "oopsAllThis");
        if (!every[0] || (fixed_enemy == 0 && strcmp(every, "0"))) sr_fail(c, SOMRANDO_ERR_FAILED, "Input string was not in a correct format.");
      }
    }
    const int do_swaps = strstr(kind, "swap") || fixed_enemy != -1;
    if (strstr(kind, "none")) {
      sr_p4_nested(c, "No enemies mode", sr_p4_shared_no_enemies);
      return 0;
    }
    if (strstr(kind, "random")) {
      sr_p4_nested(c, "Random enemy spawns hack", sr_p4_shared_enemy_randomizer);
      return 0;
    }
    if (!do_swaps) return 0;
  } else {
    if (!SB("rEnemies")) return 0;
    status = S("rStatusAilments");
    if (!strncmp(status, "random", 6)) { bad_mode(c, "rStatusAilments=random*"); return 0; }
  }
  int location = !strcmp(status, "location");

  struct { int summoner, n; int loc[5]; } summoned[] = {
    {34, 1, {0x106D2F}},
    {64, 3, {0x108A58, 0x108A4E, 0x108A50}},
    {19, 1, {0x1060FC}},
    {47, 1, {0x1078F5}},
    {74, 4, {0x1096A9, 0x1096A5, 0x1096B1, 0x1096B5}},
    {7, 1, {0x105564}},
    {26, 1, {0x106787}},
    {11, 2, {0x105810, 0x1058D4}},
    {69, 1, {0x108FAC}},
    {37, 3, {0x107025, 0x10705F, 0x10706D}},
    {58, 1, {0x108498}},
    {66, 1, {0x108C2F}},
    {75, 1, {0x109818}},
    {76, 1, {0x10999C}},
    {79, 5, {0x109CF3, 0x109CFB, 0x109D03, 0x109D0B, 0x109D10}},
  };
  const int n_summoned = (int)(sizeof summoned / sizeof summoned[0]);
  static const int unhittable[] = {9, 15, 29, 48};
  static const int no_swap[] = {32, 46, 63, 50};
  static const int no_summon[] = {0x51, 0x10, 0x1C, 0x39, 0x16, 0x47};

  int all_summoners[32], n_all = 0;
  for (int i = 0; i < n_summoned; i++) all_summoners[n_all++] = summoned[i].summoner;
  all_summoners[n_all++] = 0x08;
  all_summoners[n_all++] = 0x27;
  all_summoners[n_all++] = 0x38;
  all_summoners[n_all++] = 0x44;
  all_summoners[n_all++] = 0x26;

  struct swap_map idx = {{0}, {0}, 0}, idx_un = {{0}, {0}, 0};
  int src[84], dst_left[84], n_src = 0, n_dst = 0;
  int src_un[84], dst_un[84], n_src_un = 0, n_dst_un = 0;
  for (int i = 0; i < 84; i++) {
    if (in_list(no_swap, 4, i)) continue;
    if (!in_list(unhittable, 4, i)) {
      src[n_src++] = i;
      dst_left[n_dst++] = i;
    } else {
      src_un[n_src_un++] = i;
      dst_un[n_dst_un++] = i;
    }
  }

  for (int k = 0; k < n_src; k++) {
    int key = src[k];
    if (!in_list(no_summon, 6, key)) continue;
    int d = sr_random_next(&c->rf) % n_dst;
    int iters = 0;
    while (in_list(all_summoners, n_all, dst_left[d]) && iters < 100) {
      d = sr_random_next(&c->rf) % n_dst;
      iters++;
    }
    idx.old_id[idx.n] = key;
    idx.new_id[idx.n++] = fixed_enemy == -1 ? dst_left[d] : fixed_enemy;
    list_remove_at(dst_left, &n_dst, d);
  }
  for (int k = 0; k < n_src; k++) {
    int key = src[k];
    if (in_list(no_summon, 6, key)) continue;
    int d = sr_random_next(&c->rf) % n_dst;
    idx.old_id[idx.n] = key;
    idx.new_id[idx.n++] = fixed_enemy == -1 ? dst_left[d] : fixed_enemy;
    list_remove_at(dst_left, &n_dst, d);
  }
  for (int k = 0; k < n_src_un; k++) {
    int d = sr_random_next(&c->rf) % n_dst_un;
    idx_un.old_id[idx_un.n] = src_un[k];
    idx_un.new_id[idx_un.n++] = dst_un[d];
    list_remove_at(dst_un, &n_dst_un, d);
  }

  for (int map_num = 16; map_num < 500; map_num++) {
    int o1 = 0x80000 + c->out[0x87000 + map_num * 2] + (c->out[0x87000 + map_num * 2 + 1] << 8);
    int o2 = 0x80000 + c->out[0x87000 + map_num * 2 + 2] + (c->out[0x87000 + map_num * 2 + 3] << 8);
    o1 += 8;
    while (o1 < o2) {
      int type = c->orig[o1 + 5];
      for (int k = 0; k < idx.n; k++)
        if (idx.old_id[k] == type) P(o1 + 5, idx.new_id[k]);
      for (int k = 0; k < idx_un.n; k++)
        if (idx_un.old_id[k] == type) P(o1 + 5, idx_un.new_id[k]);
      o1 += 8;
    }
  }

  swap_stats(c, &idx);
  swap_stats(c, &idx_un);
  swap_weapons(c, &idx, location);
  swap_weapons(c, &idx_un, location);
  swap_drops(c, &idx);
  swap_drops(c, &idx_un);

  for (int s = 0; s < n_summoned; s++) {
    int new_level = c->out[STAT + summoned[s].summoner * 29];
    int used[8], n_used = 0;
    for (int i = 0; i < summoned[s].n; i++) {
      int id = -1, lev = -1;
      for (int k = 0; k < idx.n; k++) {
        int en = idx.old_id[k];
        if (in_list(used, n_used, en)) continue;
        int this_level = c->out[STAT + en * 29];
        if (this_level < new_level && (id == -1 || (new_level - this_level < new_level - lev))) {
          lev = this_level;
          id = en;
        }
      }
      if (id != -1) {
        P(summoned[s].loc[i], id);
        used[n_used++] = id;
      } else {
        P(summoned[s].loc[i], summoned[s].summoner);
      }
    }
  }
  return 1;
}

int sr_hack_enemy_scaling(struct sr_ctx *c) {
  int pct = sr_settings_get_int(c->settings, "rDifficulty");
  if (pct == 100) return 0;   /* no scale changes; drop out */
  double scale = pct / 100.0;
  double hp_scale = 1.0 + ((scale - 1) / 2.0);
  static const int eight[] = {4, 5, 6, 7, 8, 11};
  for (int i = 0; i < 128; i++) {
    for (int j = 0; j < 6; j++) {
      int off = STAT + i * 29 + eight[j];
      int ns = (int)((c->out[off] + 5) * scale);
      if (ns > 255) ns = 255;
      P(off, ns);
    }
    {
      int off = STAT + i * 29 + 12;
      int ns = (int)((sr_ushort_from_bytes(c->out, off) + 5) * hp_scale);
      if (ns > 65535) ns = 65535;
      sr_ushort_to_bytes(c->out, off, ns);
    }
    {
      int off = STAT + i * 29 + 1;
      int ns = (int)(sr_ushort_from_bytes(c->out, off) * hp_scale);
      if (ns > 65535) ns = 65535;
      sr_ushort_to_bytes(c->out, off, ns);
    }
  }
  for (int i = 72; i <= 189; i++) {
    int off = WEAP + i * 12 + 8;
    int ns = (int)(c->out[off] * scale);
    if (ns > 255) ns = 255;
    P(off, ns);
  }
  for (int i = 0; i < 115; i++) {
    int off = 0x10BDC1 + i * 7 + 3;
    int ns = (int)(c->out[off] * scale);
    if (ns > 255) ns = 255;
    P(off, ns);
  }
  return 1;
}

/* EnemyPositionAdjustments: {map, object, direction (0 left, 1 right, 2 up, 3 down), distance} in the order of the C# add() calls */
int sr_hack_enemy_position_adjust(struct sr_ctx *c) {
  if (!SB("adjustEnemyPos")) return 0;
  if (!strcmp(S("mode"), "bossrush") || !strcmp(S("mode"), "chaos") || !strcmp(S("mode"), "ancientcave")) return 0;   /* the C# only adjusts vanilla and open world */
  if (strcmp(S("mode"), "rando") && strcmp(S("mode"), "open")) { bad_mode(c, "enemy position adjustment outside the vanilla randomizer"); return 0; }
  enum { L, R, U, D };
  static const int adj[][4] = {
    {400, 1, U, 2}, {400, 2, D, 2},
    {401, 2, L, 2}, {401, 2, D, 2}, {401, 3, R, 2}, {401, 3, D, 2}, {401, 4, L, 2}, {401, 4, D, 2},
    {401, 5, R, 2}, {401, 5, D, 2}, {401, 6, L, 2}, {401, 6, D, 2}, {401, 7, R, 2}, {401, 7, D, 2},
    {401, 13, U, 2}, {401, 14, U, 2}, {401, 17, D, 2}, {401, 17, L, 1}, {401, 18, D, 2}, {401, 18, R, 1},
    {404, 5, R, 2}, {404, 6, L, 2},
    {405, 0, D, 2}, {405, 1, D, 3}, {405, 1, L, 3}, {405, 2, R, 3}, {405, 2, D, 3},
    {379, 0, D, 3},
    {385, 3, R, 3}, {385, 5, U, 2},
    {417, 4, D, 2}, {417, 4, R, 4}, {417, 5, D, 2}, {417, 5, R, 4}, {417, 6, D, 2}, {417, 6, L, 4}, {417, 7, D, 2}, {417, 7, L, 4},
    {385, 3, R, 3}, {385, 5, U, 3}, {385, 2, U, 3},
    {436, 0, R, 3},
    {23, 24, R, 5},
    {25, 13, L, 7}, {25, 13, U, 10},
    {216, 0, L, 4}, {216, 1, R, 4},
    {62, 5, L, 4},
    {403, 1, U, 3}, {403, 2, U, 3},
  };
  for (size_t k = 0; k < sizeof adj / sizeof adj[0]; k++) {
    int map = adj[k][0], obj = adj[k][1], dir = adj[k][2], dist = adj[k][3];
    int start = sr_vanilla_object_offset(c->out, map) + 8;
    int end = sr_vanilla_object_offset(c->out, map + 1);
    int off = start + obj * 8;
    if (off + 8 > end) { sr_fail(c, SOMRANDO_ERR_UNSUPPORTED, "enemy position adjustment: object out of range"); return 0; }
    /* xpos is byte 2 bits 0x7F, ypos byte 3 bits 0x7F; the C# set() ORs the unmasked byte value over the other bits */
    int byte_i = (dir == L || dir == R) ? 2 : 3;
    int cur = c->out[off + byte_i] & 0x7F;
    int nv = (dir == R || dir == D) ? cur + dist : cur - dist;
    int other = c->out[off + byte_i] & ~0x7F;
    P(off + byte_i, (uint8_t)((uint8_t)nv | other));
  }
  return 1;
}
