/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Phase 2: BossSwaps, VanillaBossMaps (vanilla randomizer, swap mode).  Author of every upstream hack: Moppleton.  Translated to C 2026-10-18.
 * Writes no workingData keys.  ROM regions: see the comments of each block; the object bytes of the boss arenas are what BossZPosition reads.
 */
#include <math.h>
#include <string.h>

#include "sr_core.h"
#include "sr_p2_bosses_data.h"
#include "sr_p4_shared_boss_all.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))
#define S(key) sr_settings_get(c->settings, (key))
#define SB(key) sr_settings_get_bool(c->settings, (key))

#define BOSSID_WALLFACE 88
#define BOSSID_TROPICALLO 89
#define BOSSID_SPIKEY 91
#define BOSSID_SPRINGBEAK 93
#define BOSSID_SNAPDRAGON 95
#define BOSSID_DOOMSWALL 97
#define BOSSID_BOREAL 104
#define BOSSID_GREATVIPER 105
#define BOSSID_BLUESPIKE 107
#define BOSSID_CHAMBERSEYE 108
#define BOSSID_TONPOLE 113
#define BOSSID_AXEBEAK 118
#define BOSSID_BITINGLIZARD 122
#define BOSSID_DRAGONWORM 123
#define BOSSID_DOOMSEYE 126

#define N_MAPS ((int)(sizeof(sr_w6_bmaps) / sizeof(sr_w6_bmaps[0])))

static void bad_mode(struct sr_ctx *c, const char *what) { sr_fail(c, SOMRANDO_ERR_UNSUPPORTED, "unsupported option: %s", what); }

static int out_at(struct sr_ctx *c, int off) { return sr_peek(c, off); }

static void put16(struct sr_ctx *c, int off, int v) {
  P(off, v & 0xFF);
  P(off + 1, (v >> 8) & 0xFF);
}

/* offset of the (x, y) bytes of the boss object on the map (objsOffset + 8 + objNum * 8 + 2) */
static int boss_xy(struct sr_ctx *c, const struct sr_w6_bmap *m) {
  return sr_vanilla_object_offset(c->out, m->map_num) + 8 + m->obj_num * 8 + 2;
}

static void wall_xy(struct sr_ctx *c, const struct sr_w6_bmap *m, int boss, int x, int y) {
  if (boss == BOSSID_WALLFACE || boss == BOSSID_DOOMSWALL) {
    int o = boss_xy(c, m);
    P(o, x);
    P(o + 1, y);
  }
}

/* VanillaBossMap.process: the per-map post-processing lambda */
static void map_process(struct sr_ctx *c, int idx, int boss) {
  const struct sr_w6_bmap *m = &sr_w6_bmaps[idx];
  int o;
  switch (idx) {
    case BM_WALLFACE:
      o = boss_xy(c, m);
      if (boss == BOSSID_WALLFACE || boss == BOSSID_DOOMSWALL) {
        P(o, 14);
        P(o + 1, 6);
      } else {
        P(o, 14);
        P(o + 1, 10);
      }
      break;
    case BM_TROPICALLO:
      P(0xDD632, 0xAF);
      P(0xDD637, 0xAF);
      P(0xDD63D, 0x14);
      P(0xDD650, 0x2B);
      P(0xDD668, 0x2B);
      P(0xDD672, 0x2B);
      P(0xDD687, 0x2B);
      P(0xDD684, 0x6A);
      break;
    case BM_SPIKEY:
      if (boss == BOSSID_SPRINGBEAK || boss == BOSSID_AXEBEAK) {
        P(0x7F7F0, 0);
        P(0x7F7F1, 0);
      }
      if (boss == BOSSID_GREATVIPER || boss == BOSSID_DRAGONWORM) {
        o = sr_vanilla_object_offset(c->out, m->map_num) + 8 + m->obj_num * 8 + 3;
        P(o, out_at(c, o) - 1);
      }
      break;
    case BM_SNAPDRAGON:
      if (boss == BOSSID_GREATVIPER || boss == BOSSID_DRAGONWORM) P(boss_xy(c, m) + 1, 23);
      break;
    case BM_DOOMSWALL: wall_xy(c, m, boss, 14, 8); break;
    case BM_VAMPIRE: {
      static const uint8_t pieces[12] = {0xFE, 0x00, 0x0F, 0x6C, 0x00, 0x01, 0xFF, 0xFE, 0x0F, 0x6C, 0x00, 0x01};
      int ho = sr_vanilla_object_offset(c->out, m->map_num);
      P(ho + 2, out_at(c, ho + 2) | 0x40);
      P(ho + 4, 0);
      P(ho + 5, 5);
      int po = 0x80000 + out_at(c, 0x85000 + m->map_num * 2) + (out_at(c, 0x85000 + m->map_num * 2 + 1) << 8);
      for (int i = 0; i < 12; i++) P(po + i, pieces[i]);
      break;
    }
    case BM_KILROY: {
      int ho = sr_vanilla_object_offset(c->out, m->map_num) + 4;
      P(ho, out_at(c, ho) | 0x06);
      break;
    }
    case BM_BOREAL: P(boss_xy(c, m) + 1, 11); break;
    case BM_GREATVIPER: {
      /* upstream condition (id != viper || id != dragon worm) is always true */
      static const uint8_t pieces[32] = {0xFE, 0x00, 0x0F, 0x65, 0x01, 0x00, 0xFF, 0x00, 0x0F, 0xE9, 0x00, 0x00, 0x00, 0x0F, 0x68, 0x01,
                                         0x00, 0x00, 0x0F, 0x68, 0x31, 0x00, 0x00, 0x0F, 0x68, 0x01, 0x38, 0x00, 0x0F, 0x68, 0x31, 0x38};
      o = boss_xy(c, m);
      P(o, 0x16);
      P(o + 1, 0x0D);
      for (int i = 0; i < 32; i++) P(0x87DBE + i, pieces[i]);
      P(0x8CDCA, 0x90);
      P(0x8CDCD, 0x0D);
      P(0x83532, 0x30);
      P(0x835B6, 0x10);
      break;
    }
    case BM_HYDRA: {
      int ho = sr_vanilla_object_offset(c->out, m->map_num);
      P(ho + 2, out_at(c, ho + 2) | 0x40);
      P(ho + 2, out_at(c, ho + 2) & 0x7F);
      P(ho + 5, 13);
      int po = 0x80000 + out_at(c, 0x85000 + m->map_num * 2) + (out_at(c, 0x85000 + m->map_num * 2 + 1) << 8);
      P(po + 0x03, 0x07);
      P(po + 0x08, 0xE4);
      P(po + 0x0E, 0x06);
      P(po + 0x13, 0xE3);
      int door = 0x83000 + 0x31A * 4;
      P(door + 2, out_at(c, door + 2) & 0xFE);
      break;
    }
    case BM_AEGRTOJNFSDGHOPALNIAPLEIAN: wall_xy(c, m, boss, 14, 4); break;
    case BM_TONPOLE: {
      int mo = sr_vanilla_object_offset(c->out, 361) + 8;
      P(mo + 8 * 0 + 5, boss);
      P(mo + 8 * 1 + 5, boss);
      P(mo + 8 * 2 + 5, boss);
      P(0xD635F, 0xAF);
      P(0xD636A, 0xAF);
      break;
    }
    case BM_SNOWDRAGON: wall_xy(c, m, boss, 13, 7); break;
    case BM_FIREGIGAS: P(boss_xy(c, m) + 1, 17); break;
    case BM_REDDRAGON: wall_xy(c, m, boss, 18, 8); break;
    case BM_AXEBEAK:
      o = boss_xy(c, m);
      P(o, 20);
      P(o + 1, 23);
      break;
    case BM_BLUEDRAGON: wall_xy(c, m, boss, 13, 9); break;
    case BM_DARKLICH: wall_xy(c, m, boss, 18, 9); break;
    case BM_DRAGONWORM:
      /* upstream condition is always true as well */
      o = boss_xy(c, m);
      P(o, 21);
      P(o + 1, 12);
      break;
    default: break;
  }
}

static int in_list(const uint8_t *l, int n, int v) {
  for (int i = 0; i < n; i++)
    if (l[i] == v) return 1;
  return 0;
}

static void need_pos(struct sr_ctx *c, int n) {
  if (n <= 0) sr_fail(c, SOMRANDO_ERR_FAILED, "Object reference not set to an instance of an object.");
}

int sr_hack_boss_swaps(struct sr_ctx *c) {
  const int is_open = !strcmp(S("mode"), "open");
  if (!is_open && strcmp(S("mode"), "rando")) { bad_mode(c, "boss randomizer outside the vanilla randomizer and open world"); return 0; }
  int bypass, preserve_early = 0, swap_only = 1;
  if (is_open) {
    const char *boss_type = S("opBosses");
    swap_only = !strcmp(boss_type, "swap");
    bypass = !strcmp(boss_type, "vanilla");
    if (sr_settings_get(c->settings, "plandoSettings")[0]) { bad_mode(c, "plandoSettings (boss plando)"); return 0; }
  } else {
    bypass = !SB("rBosses");
    preserve_early = SB("rPreserve");
  }
  sr_names_set(c, NAMES_INDEX_ENEMIES_START + 96, "Mech Rider I");
  sr_names_set(c, NAMES_INDEX_ENEMIES_START + 100, "Mech Rider II");
  sr_names_set(c, NAMES_INDEX_ENEMIES_START + 114, "Mech Rider III");
  if (bypass) return 0;   /* "Bypassing boss rando - configured not to randomize" */

  static const int tropicallo_x[] = {0x249F3, 0x249F7, 0x249FB, 0x249FF, 0x24A03, 0x24A07, 0x24A0B, 0x24A0F, 0x24A13, 0x24A17, 0x24A1B, 0x24A1F};
  static const int tropicallo_y[] = {0x249F5, 0x249F9, 0x249FD, 0x24A01, 0x24A05, 0x24A09, 0x24A0D, 0x24A11, 0x24A15, 0x24A19, 0x24A1D, 0x24A21};
  static const int tropicallo_bx[] = {0x24643, 0x24647};
  static const int tropicallo_by[] = {0x24645, 0x24649};
  static const int boreal_x[] = {0x24A33, 0x24A37, 0x24A3B, 0x24A3F, 0x24A43, 0x24A47, 0x24A4B, 0x24A4F, 0x24A53, 0x24A57, 0x24A5B, 0x24A5F};
  static const int boreal_y[] = {0x24A35, 0x24A39, 0x24A3D, 0x24A41, 0x24A45, 0x24A49, 0x24A4D, 0x24A51, 0x24A55, 0x24A59, 0x24A5D, 0x24A61};
  static const int boreal_bx[] = {0x2464B, 0x2464D};
  static const int boreal_by[] = {0x2464F, 0x24651};
  const int spikey_mx = 0x2DCBA, spikey_lx = 0x2DCCD, spikey_rx = 0x2DCE0, spikey_my = 0x2DCBF, spikey_ly = 0x2DCD2, spikey_ry = 0x2DCE5;
  const int bspike_lx = 0x2DCFE, bspike_rx = 0x2DD11, bspike_mx = 0x2DD24, bspike_ly = 0x2DD03, bspike_ry = 0x2DD16, bspike_my = 0x2DD29;
  const int spring_cx = 0x1CE398, spring_cy = 0x1CE39A, axe_cx = 0x1CE39C, axe_cy = 0x1CE39E;
  static const int spring_pod_x[] = {0x1CE3D0, 0x1CE3D4}, spring_pod_y[] = {0x1CE3D2, 0x1CE3D6};
  static const int axe_pod_x[] = {0x1CE3D8, 0x1CE3DC}, axe_pod_y[] = {0x1CE3DA, 0x1CE3DE};
  static const int spring_jump_x[] = {0x1CE3A0, 0x1CE3A4, 0x1CE3A8, 0x1CE3AC, 0x1CE3B0, 0x1CE3B4, 0x2E4F5, 0x2E51E};
  static const int spring_jump_y[] = {0x1CE3A2, 0x1CE3A6, 0x1CE3AA, 0x1CE3AE, 0x1CE3B2, 0x1CE3B6, 0x2E4FA, 0x2E523};
  static const int axe_jump_x[] = {0x1CE3B8, 0x1CE3BC, 0x1CE3C0, 0x1CE3C4, 0x1CE3C8, 0x1CE3CC, 0x2E553, 0x2E579};
  static const int axe_jump_y[] = {0x1CE3BA, 0x1CE3BE, 0x1CE3C2, 0x1CE3C6, 0x1CE3CA, 0x1CE3CE, 0x2E558, 0x2E57E};

  int swap_maps[40];
  int n_swap = 0;
  swap_maps[n_swap++] = BM_MANTISANT;
  if (!preserve_early) {
    swap_maps[n_swap++] = BM_TROPICALLO;
    swap_maps[n_swap++] = BM_SPIKEY;
    swap_maps[n_swap++] = BM_TONPOLE;
  }
  static const int rest[] = {BM_MINOTAUR, BM_JABBER, BM_SPRINGBEAK, BM_FROSTGIGAS, BM_SNAPDRAGON, BM_MECHRIDER1, BM_VAMPIRE, BM_METALMANTIS,
                             BM_MECHRIDER2, BM_KILROY, BM_GORGON, BM_BOREAL, BM_GREATVIPER, BM_BLUESPIKE, BM_HYDRA, BM_HEXAS, BM_KETTLEKIN,
                             BM_MECHRIDER3, BM_FIREGIGAS, BM_AXEBEAK, BM_BUFFY, BM_DRAGONWORM, BM_THUNDERGIGAS,
                             BM_AEGRTOJNFSDGHOPALNIAPLEIAN, BM_WALLFACE, BM_DOOMSWALL, BM_SNOWDRAGON, BM_REDDRAGON, BM_BLUEDRAGON, BM_DARKLICH};
  for (size_t i = 0; i < sizeof(rest) / sizeof(rest[0]); i++) swap_maps[n_swap++] = rest[i];

  /* newBossNums, parallel to swap_maps */
  uint8_t new_boss[40];
  for (int i = 0; i < n_swap; i++) {
    const struct sr_w6_bmap *m = &sr_w6_bmaps[swap_maps[i]];
    new_boss[i] = sr_peek_orig(c, sr_vanilla_object_offset(c->orig, m->map_num) + 8 + m->obj_num * 8 + 5);
  }

  /* make vampire behave like buffy collision-wise */
  P(0x26b24, 0xEA);
  P(0x26b25, 0xEA);
  /* also fix hitting enemies with ranged weapons on vampire map */
  P(0x2c5c7, 0xEA);
  P(0x2c5c8, 0xEA);
  /* new display settings */
  static const uint8_t disp[9] = {0x00, 0x15, 0x00, 0x00, 0x15, 0x10, 0x7D, 0x00, 0x00};
  for (int i = 0; i < 9; i++) P(0x801E1 + i, disp[i]);

  /* swap pass; plando settings are refused above */
  static const uint8_t restricted[] = {88, 89, 91, 93, 104, 107, 118};
  static const uint8_t layer2_bosses[] = {88, 97, 110, 115, 117, 119, 121};
  uint8_t restricted_left[7];
  int n_left = 7;
  memcpy(restricted_left, restricted, sizeof restricted);
  for (int i = 0; i < n_swap; i++) {
    const struct sr_w6_bmap *m1 = &sr_w6_bmaps[swap_maps[i]];
    if (swap_only) {
      for (int t = 0; t < 100; t++) {
        int j = sr_random_next(&c->rf) % n_swap;
        const struct sr_w6_bmap *m2 = &sr_w6_bmaps[swap_maps[j]];
        if (in_list(m1->swap, m1->n_swap, new_boss[j]) && in_list(m2->swap, m2->n_swap, new_boss[i])) {
          uint8_t b1 = new_boss[i], b2 = new_boss[j];
          new_boss[i] = b2;
          new_boss[j] = b1;
          break;
        }
      }
    } else {
      const int n_sup = sr_p4_all_bosses[swap_maps[i]].n;
      const uint8_t *sup = sr_p4_all_bosses[swap_maps[i]].p;
      uint8_t boss = sup[sr_random_next(&c->rf) % n_sup];
      while (in_list(restricted, 7, boss) && !in_list(restricted_left, n_left, boss)) boss = sup[sr_random_next(&c->rf) % n_sup];
      for (int k = 0; k < n_left; k++)
        if (restricted_left[k] == boss) {
          memmove(restricted_left + k, restricted_left + k + 1, (size_t)(n_left - k - 1));
          n_left--;
          break;
        }
      new_boss[i] = boss;
      if (!in_list(layer2_bosses, 7, new_boss[i])) {
        static const struct { int bm; int off; } l2[] = {{BM_WALLFACE, 0x8B66D}, {BM_AEGRTOJNFSDGHOPALNIAPLEIAN, 0x8BF05}, {BM_BLUEDRAGON, 0x8BE9D}, {BM_REDDRAGON, 0x8BE8D}, {BM_SNOWDRAGON, 0x8BE7D}, {BM_DARKLICH, 0x8BF5D}};
        for (size_t k = 0; k < sizeof l2 / sizeof l2[0]; k++)
          if (swap_maps[i] == l2[k].bm) P(l2[k].off, 25);
      }
    }
  }

  for (int mi = 0; mi < n_swap; mi++) {
    int idx = swap_maps[mi];
    const struct sr_w6_bmap *tm = &sr_w6_bmaps[idx];
    int boss = new_boss[mi];
    map_process(c, idx, boss);
    switch (boss) {
      case BOSSID_WALLFACE:
        put16(c, 0x27DF5, tm->wall_y);
        break;
      case BOSSID_TROPICALLO:
      case BOSSID_BOREAL: {
        const int *xl = boss == BOSSID_TROPICALLO ? tropicallo_x : boreal_x;
        const int *yl = boss == BOSSID_TROPICALLO ? tropicallo_y : boreal_y;
        const int *bxl = boss == BOSSID_TROPICALLO ? tropicallo_bx : boreal_bx;
        const int *byl = boss == BOSSID_TROPICALLO ? tropicallo_by : boreal_by;
        need_pos(c, tm->n_trop);
        need_pos(c, tm->n_bram);
        for (int i = 0; i < 12; i++) {
          put16(c, xl[i], tm->trop[i % tm->n_trop][0]);
          put16(c, yl[i], tm->trop[i % tm->n_trop][1]);
        }
        for (int i = 0; i < 2; i++) {
          int k = i % tm->n_trop;
          if (k >= tm->n_bram) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range.");
          put16(c, bxl[i], tm->bram[k][0]);
          put16(c, byl[i], tm->bram[k][1]);
        }
        break;
      }
      case BOSSID_SPIKEY:
      case BOSSID_BLUESPIKE: {
        int mx = boss == BOSSID_SPIKEY ? spikey_mx : bspike_mx, my = boss == BOSSID_SPIKEY ? spikey_my : bspike_my;
        int lx = boss == BOSSID_SPIKEY ? spikey_lx : bspike_lx, ly = boss == BOSSID_SPIKEY ? spikey_ly : bspike_ly;
        int rx = boss == BOSSID_SPIKEY ? spikey_rx : bspike_rx, ry = boss == BOSSID_SPIKEY ? spikey_ry : bspike_ry;
        need_pos(c, tm->sp_c[0] | tm->sp_c[1]);
        put16(c, mx, tm->sp_c[0]);
        put16(c, my, tm->sp_c[1]);
        put16(c, lx, tm->sp_l[0]);
        put16(c, ly, tm->sp_l[1]);
        put16(c, rx, tm->sp_r[0]);
        put16(c, ry, tm->sp_r[1]);
        break;
      }
      case BOSSID_SPRINGBEAK:
      case BOSSID_AXEBEAK: {
        int sp = boss == BOSSID_SPRINGBEAK;
        need_pos(c, tm->n_pod);
        need_pos(c, tm->n_jump);
        need_pos(c, tm->beak_c[0] | tm->beak_c[1]);
        put16(c, sp ? spring_cx : axe_cx, tm->beak_c[0]);
        put16(c, sp ? spring_cy : axe_cy, tm->beak_c[1]);
        const int *pxl = sp ? spring_pod_x : axe_pod_x, *pyl = sp ? spring_pod_y : axe_pod_y;
        for (int i = 0; i < 2; i++) {
          put16(c, pxl[i], tm->pod[i % tm->n_pod][0]);
          put16(c, pyl[i], tm->pod[i % tm->n_pod][1]);
        }
        const int *jxl = sp ? spring_jump_x : axe_jump_x, *jyl = sp ? spring_jump_y : axe_jump_y;
        for (int i = 0; i < 8; i++) {
          put16(c, jxl[i], tm->jump[i % tm->n_jump][0]);
          put16(c, jyl[i], tm->jump[i % tm->n_jump][1]);
        }
        break;
      }
      default: break;
    }

    /* swap the boss object and its stats */
    int objs = sr_vanilla_object_offset(c->out, tm->map_num) + 8 + tm->obj_num * 8 + 5;
    int old = out_at(c, objs);
    P(objs, boss);
    static const int eight[] = {0, 11, 26, 3, 4, 5, 6, 7, 8, 18, 19};
    static const int sixteen[] = {1, 16, 27, 9, 12};
    for (int k = 0; k < 11; k++) {
      int si = eight[k];
      int nv = sr_peek_orig(c, 0x101C00 + old * 29 + si);
      if (si == 3 && (boss == BOSSID_TONPOLE || boss == BOSSID_SNAPDRAGON || boss == BOSSID_BITINGLIZARD)) nv = 6;
      P(0x101C00 + boss * 29 + si, nv);
      if (boss == BOSSID_TONPOLE) P(0x101C00 + BOSSID_BITINGLIZARD * 29 + si, nv);
      /* upstream compares the constant BOSSID_WALLFACE here instead of bossNum */
      if (old == BOSSID_DOOMSWALL) {
        nv = sr_peek_orig(c, 0x101C00 + BOSSID_DOOMSEYE * 29 + si);
        P(0x101C00 + BOSSID_CHAMBERSEYE * 29 + si, nv);
      }
      if (boss == BOSSID_DOOMSWALL && old == BOSSID_WALLFACE) {
        nv = sr_peek_orig(c, 0x101C00 + BOSSID_CHAMBERSEYE * 29 + si);
        P(0x101C00 + BOSSID_DOOMSEYE * 29 + si, nv);
      }
    }
    for (int k = 0; k < 5; k++) {
      int si = sixteen[k];
      P(0x101C00 + boss * 29 + si, sr_peek_orig(c, 0x101C00 + old * 29 + si));
      P(0x101C00 + boss * 29 + si + 1, sr_peek_orig(c, 0x101C00 + old * 29 + si + 1));
      if (boss == BOSSID_TONPOLE) {
        P(0x101C00 + BOSSID_BITINGLIZARD * 29 + si, sr_peek_orig(c, 0x101C00 + old * 29 + si));
        P(0x101C00 + BOSSID_BITINGLIZARD * 29 + si + 1, sr_peek_orig(c, 0x101C00 + old * 29 + si + 1));
      }
      if (boss == BOSSID_WALLFACE && old == BOSSID_DOOMSWALL) {
        P(0x101C00 + BOSSID_CHAMBERSEYE * 29 + si, sr_peek_orig(c, 0x101C00 + BOSSID_DOOMSEYE * 29 + si));
        P(0x101C00 + BOSSID_CHAMBERSEYE * 29 + si + 1, sr_peek_orig(c, 0x101C00 + BOSSID_DOOMSEYE * 29 + si + 1));
      }
      if (boss == BOSSID_DOOMSWALL && old == BOSSID_WALLFACE) {
        P(0x101C00 + BOSSID_DOOMSEYE * 29 + si, sr_peek_orig(c, 0x101C00 + BOSSID_CHAMBERSEYE * 29 + si));
        P(0x101C00 + BOSSID_DOOMSEYE * 29 + si + 1, sr_peek_orig(c, 0x101C00 + BOSSID_CHAMBERSEYE * 29 + si + 1));
      }
    }
  }

  /* triple tonpole spawns one at a time (vanilla rando only) */
  if (!is_open) {
    struct sr_bytes *e6 = sr_event_new(c, 0x6);
    sr_bytes_push(c, e6, EV_REFRESH_OBJECTS);
    sr_ev_end(c, e6);

    int mo = sr_vanilla_object_offset(c->out, 361) + 8;
    P(mo + 8 * 0 + 0, 0x25);
    P(mo + 8 * 0 + 1, 0x00);
    P(mo + 8 * 1 + 0, 0x25);
    P(mo + 8 * 1 + 1, 0x11);
    P(mo + 8 * 2 + 0, 0x25);
    P(mo + 8 * 2 + 1, 0x22);

    struct sr_bytes *e = sr_event_new(c, 0x66a);
    uint8_t jmp[2];
    sr_ev_incr_flag(c, e, 0x25);
    sr_ev_jump_cmd(6, jmp);
    sr_ev_logic(c, e, 0x25, 0x0, 0x2, jmp);
    sr_ev_set_flag(c, e, 0x56, 5);
    sr_ev_set_flag(c, e, 0x26, 3);
    sr_ev_door(c, e, 0x2c3);
    sr_ev_jump(c, e, 0x715);
    sr_ev_end(c, e);
  }

  /* weapon loader reads str instead of weapon power: LDA $E188,y */
  P(0x462C, 0xB9);
  P(0x462D, 0x88);
  P(0x462E, 0xE1);
  P(0x462F, 0xEA);

  /* replace subroutine [0B8B-0BB7] with a JSL to the new code plus JSL wrappers for C2 subroutines */
  P(0x20B8B, 0x22);
  P(0x20B8C, c->working_offset & 0xFF);
  P(0x20B8D, (c->working_offset >> 8) & 0xFF);
  P(0x20B8E, ((c->working_offset >> 16) + 0xC0) & 0xFF);
  P(0x20B8F, 0x60);
  static const uint8_t wrappers[16] = {0x20, 0x11, 0x0C, 0x6B, 0x20, 0x33, 0x0C, 0x6B, 0x20, 0x89, 0x0C, 0x6B, 0x20, 0x63, 0x39, 0x6B};
  for (int i = 0; i < 16; i++) P(0x20B90 + i, wrappers[i]);
  const int jsl_0c11 = 0x20B90, jsl_0c33 = 0x20B94, jsl_0c89 = 0x20B98, jsl_3963 = 0x20B9C;

#define JSL(o) do { E(0x22); E((o) & 0xFF); E(((o) >> 8) & 0xFF); E((((o) >> 16) + 0xC0) & 0xFF); } while (0)
  E(0xA6); E(0x87);
  E(0xBD); E(0x96); E(0x00);
  E(0xC9); E(0x18); E(0x00);
  E(0xD0); E(0x12 + 9);
  E(0xAF); E(0xDC); E(0x00); E(0x7E);
  E(0xC9); E(0x69); E(0x01);
  E(0xF0); E(0x07);
  E(0xA9); E(0xF8); E(0x07);
  JSL(jsl_0c11);
  E(0xA6); E(0x87);
  E(0x22); E(0x6F); E(0x00); E(0xC0);
  E(0xA6); E(0x87);
  E(0xBD); E(0x96); E(0x00);
  E(0xC9); E(0x34); E(0x00);
  E(0xB0); E(0x0F);
  E(0x29); E(0x03); E(0x00);
  E(0xD0); E(0x04);
  JSL(jsl_0c33);
  JSL(jsl_0c89);
  E(0x18);
  E(0x6B);
  E(0x9E); E(0xA7); E(0x00);
  E(0xFE); E(0x94); E(0x00);
  E(0x9E); E(0x96); E(0x00);
  E(0xA9); E(0x00); E(0x8A);
  JSL(jsl_3963);
  E(0x18);
  E(0x6B);
#undef JSL
  (void)N_MAPS;
  return 1;
}
