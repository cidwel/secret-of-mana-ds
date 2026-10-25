/* See som_gameplay.h.  Port of tools/gameplay/som_gameplay.py (plan, resolve, apply_recs, checksum); the routine emitters follow it line by line. */
#include <stdio.h>
#include <string.h>
#include "som_gameplay.h"

#define FREE_BASE 0x1C0540u
#define FREE_LEN 0x3C0u

static const char *const KEYS[SGP_COUNT] = {
  "weapon_recharge", "run_recharge", "charge_speed", "charge_frames_per_stage", "charge_speed_up_bonus", "charge_bonus_item_id",
  "weapon_level_rate", "monster_flinch", "hero_flinch", "hero_crit_bonus_girl", "hero_crit_bonus_others", "crit_multiplier", "walk_speed",
  "qol_chest_fast", "qol_fast_transitions", "qol_fast_fades", "qol_unready_damage", "qol_item_limit", "qol_levelup_mp_restore", "qol_dead_exp",
  "qol_magic_level_rate", "qol_scroll_deadzone", "qol_ring_speed", "qol_message_speed", "qol_shop_fill",
};

void sgp_defaults(struct sgp_opts *o) {
  for (int i = 0; i < SGP_COUNT; i++) o->v[i] = 0;
  o->v[SGP_CHARGE_FRAMES] = o->v[SGP_CHARGE_BONUS_ITEM] = o->v[SGP_CRIT_GIRL] = o->v[SGP_CRIT_OTHERS] = -1;
  o->v[SGP_WEAPON_LEVEL_RATE] = 1;
  o->v[SGP_CRIT_MULTIPLIER] = o->v[SGP_WALK_SPEED] = 2;
  o->v[SGP_QOL_ITEM_LIMIT] = 4;
  o->v[SGP_QOL_MAGIC_RATE] = 1;
  o->v[SGP_QOL_RING_SPEED] = 1;
  o->v[SGP_QOL_MESSAGE_SPEED] = 1;
}
int sgp_is_default(const struct sgp_opts *o) {
  struct sgp_opts d;
  sgp_defaults(&d);
  return memcmp(&d, o, sizeof d) == 0;
}
const char *sgp_key(int opt) { return opt >= 0 && opt < SGP_COUNT ? KEYS[opt] : ""; }
int sgp_find_key(const char *key) {
  for (int i = 0; i < SGP_COUNT; i++) if (strcmp(KEYS[i], key) == 0) return i;
  return -1;
}
int sgp_valid(int opt, int32_t v) {
  switch (opt) {
  case SGP_WEAPON_RECHARGE: case SGP_CHARGE_SPEED: return v >= 0 && v <= 3;
  case SGP_RUN_RECHARGE: case SGP_HERO_FLINCH: return v >= 0 && v <= 2;
  case SGP_MONSTER_FLINCH: return v >= 0 && v <= 3;
  case SGP_CHARGE_FRAMES: return v == -1 || (v >= 1 && v <= 90);
  case SGP_CHARGE_SPEEDUP_BONUS: return v == 0 || v == 1;
  case SGP_CHARGE_BONUS_ITEM: return v == -1 || (v >= 0 && v <= 254);
  case SGP_WEAPON_LEVEL_RATE: return v >= 1 && v <= 3;
  case SGP_CRIT_GIRL: case SGP_CRIT_OTHERS: return v == -1 || (v >= 0 && v <= 120);
  case SGP_CRIT_MULTIPLIER: case SGP_WALK_SPEED: return v >= 2 && v <= 4;
  case SGP_QOL_CHEST_FAST: case SGP_QOL_FAST_TRANSITIONS: case SGP_QOL_FAST_FADES: case SGP_QOL_UNREADY_DAMAGE:
  case SGP_QOL_LEVELUP_MP: case SGP_QOL_DEAD_EXP: case SGP_QOL_SHOP_FILL: return v == 0 || v == 1;
  case SGP_QOL_ITEM_LIMIT: return v == 4 || v == 7;
  case SGP_QOL_MAGIC_RATE: return v >= 1 && v <= 3;
  case SGP_QOL_SCROLL_DEADZONE: return v >= 0 && v <= 2;
  case SGP_QOL_RING_SPEED: return v == 1 || v == 2;
  case SGP_QOL_MESSAGE_SPEED: return v == 1 || v == 2 || v == 4;
  default: return 0;
  }
}

/* ---- tiny label-aware emitter for the 65816 routines of the free area */
enum { L_VAN, L_PASS, L_HERO, L_STRONG, L_STRONG16, L_YES, L_NO, L_STORE, L_UP, L_ALIVE, L_OK, L_PAY, L_PAID, L_MORE, L_LO, L_HI, L_AFFORD, L_DONE, L_COUNT };
struct asmb {
  uint8_t b[0x200];
  unsigned n, base;
  int32_t lab[L_COUNT];
  struct { unsigned pos; int lab; int abs16; } fix[24];
  int nfix, err;
};
static void a_init(struct asmb *a, unsigned base) {
  memset(a, 0, sizeof *a);
  a->base = base;
  for (int i = 0; i < L_COUNT; i++) a->lab[i] = -1;
}
static void a_byte(struct asmb *a, unsigned v) { if (a->n < sizeof a->b) a->b[a->n++] = (uint8_t)v; else a->err = 1; }
#define RAW(a, ...) do { static const uint8_t t_[] = {__VA_ARGS__}; for (unsigned i_ = 0; i_ < sizeof t_; i_++) a_byte(a, t_[i_]); } while (0)
static void a_label(struct asmb *a, int l) { a->lab[l] = (int32_t)(a->base + a->n); }
static void a_br(struct asmb *a, unsigned op, int l) {
  a_byte(a, op);
  if (a->nfix < 24) { a->fix[a->nfix].pos = a->n; a->fix[a->nfix].abs16 = 0; a->fix[a->nfix++].lab = l; } else a->err = 1;
  a_byte(a, 0);
}
static void a_jmp(struct asmb *a, int l) {   /* JMP abs inside the free bank */
  a_byte(a, 0x4C);
  if (a->nfix < 24) { a->fix[a->nfix].pos = a->n; a->fix[a->nfix].abs16 = 1; a->fix[a->nfix++].lab = l; } else a->err = 1;
  a_byte(a, 0); a_byte(a, 0);
}
static void a_long(struct asmb *a, unsigned off) { a_byte(a, off & 0xFF); a_byte(a, (off >> 8) & 0xFF); a_byte(a, 0xC0 + ((off >> 16) & 0x3F)); }
static void a_jml(struct asmb *a, unsigned off) { a_byte(a, 0x5C); a_long(a, off); }
static void a_jsl(struct asmb *a, unsigned off) { a_byte(a, 0x22); a_long(a, off); }
static int a_done(struct asmb *a) {
  for (int i = 0; i < a->nfix; i++) {
    if (a->lab[a->fix[i].lab] < 0) return -1;
    if (a->fix[i].abs16) {
      a->b[a->fix[i].pos] = (uint8_t)(a->lab[a->fix[i].lab] & 0xFF);
      a->b[a->fix[i].pos + 1] = (uint8_t)((a->lab[a->fix[i].lab] >> 8) & 0xFF);
      continue;
    }
    int rel = a->lab[a->fix[i].lab] - (int)(a->base + a->fix[i].pos + 1);
    if (rel < -128 || rel > 127) return -1;
    a->b[a->fix[i].pos] = (uint8_t)rel;
  }
  return a->err ? -1 : 0;
}

#define RET_CHARGE_STORE 0xB3DFu
#define RET_CHARGE_UP 0xB3CBu

static void recharge_routine(struct asmb *a, int mode) {   /* mode: 0 fast, 1 faster, 2 instant */
  RAW(a, 0xE0, 0x00, 0x06);
  a_br(a, 0xB0, L_VAN);
  if (mode == 2) RAW(a, 0xA9, 0x00);
  else if (mode == 1) RAW(a, 0x4A, 0x4A);
  RAW(a, 0x9D, 0xED, 0xE1, 0x6B);
  a_label(a, L_VAN);
  RAW(a, 0x18, 0x69, 0x32, 0x9D, 0xED, 0xE1, 0x6B);
}

/* mon: 0 vanilla 1 strong 2 heavy 3 never; hero: 0 vanilla 1 light 2 never */
static void flinch_routine(struct asmb *a, int mon, int hero, unsigned done, unsigned cont) {
  RAW(a, 0xE2, 0x20);
  RAW(a, 0xBD, 0xFB, 0xE1);
  a_br(a, 0x30, L_HERO);
  RAW(a, 0x29, 0xC1);
  a_br(a, 0xD0, L_PASS);
  if (mon == 0) {
    a_br(a, 0x80, L_PASS);
  } else {
    RAW(a, 0xC2, 0x20);
    RAW(a, 0xBD, 0x82, 0xE1);
    a_br(a, 0xF0, L_PASS);
    RAW(a, 0xE2, 0x20);
    if (mon != 3) {
      RAW(a, 0xBD, 0x6B, 0xE1);
      a_br(a, 0xD0, L_STRONG);
      RAW(a, 0xBD, 0xF0, 0xE1);
      a_br(a, 0x30, L_STRONG);
      if (mon == 2) {
        RAW(a, 0xC2, 0x20);
        RAW(a, 0xBD, 0x84, 0xE1, 0x4A, 0x4A);
        RAW(a, 0xC5, 0x89);
        a_br(a, 0x90, L_STRONG16);
        RAW(a, 0xE2, 0x20);
      }
    }
    RAW(a, 0x9E, 0x6B, 0xE1);
    a_jml(a, done);
    a_label(a, L_STRONG16);
    RAW(a, 0xE2, 0x20);
    a_label(a, L_STRONG);
    RAW(a, 0x9E, 0x6B, 0xE1);
  }
  a_label(a, L_PASS);
  RAW(a, 0xC2, 0x20, 0xBD, 0x90, 0xE1);
  a_jml(a, cont);
  a_label(a, L_HERO);
  if (hero == 0) {
    a_br(a, 0x80, L_PASS);
  } else {
    RAW(a, 0xC2, 0x20);
    RAW(a, 0xBD, 0x82, 0xE1);
    a_br(a, 0xF0, L_PASS);
    if (hero == 1) {
      RAW(a, 0xBD, 0x84, 0xE1, 0x4A, 0x4A);
      RAW(a, 0xC5, 0x89);
      a_br(a, 0x90, L_PASS);
    }
    RAW(a, 0xE2, 0x20);
    a_jml(a, done);
  }
}

static void zero_routine(struct asmb *a, unsigned target) {
  RAW(a, 0x48);
  RAW(a, 0xBD, 0xFB, 0xE1);
  RAW(a, 0x30, 0x05);
  RAW(a, 0xA9, 0x00);
  RAW(a, 0x9D, 0x6B, 0xE1);
  RAW(a, 0x68);
  a_jsl(a, target);
  RAW(a, 0x6B);
}
static void critmul_routine(struct asmb *a, int mult) {
  RAW(a, 0xC2, 0x20);
  if (mult == 3) RAW(a, 0xA5, 0x89, 0x0A, 0x18, 0x65, 0x89, 0x85, 0x89);
  else RAW(a, 0x06, 0x89, 0x06, 0x89);
  RAW(a, 0x6B);
}
static void wlevel_routine(struct asmb *a, int mult) {
  RAW(a, 0xA9, 0x09, 0x38, 0xF9, 0x9C, 0xE1);
  RAW(a, 0x85, 0xA6);
  if (mult == 2) RAW(a, 0x0A);
  else if (mult == 3) RAW(a, 0x0A, 0x18, 0x65, 0xA6);
  RAW(a, 0x85, 0xA6, 0x6B);
}
static void crit_routine(struct asmb *a, unsigned target) {
  RAW(a, 0x48);
  RAW(a, 0xBD, 0xFB, 0xE1);
  RAW(a, 0x30, 0x09);
  RAW(a, 0x29, 0x40);
  RAW(a, 0xD0, 0x05);
  RAW(a, 0xA9, 0x01);
  RAW(a, 0x9D, 0x6B, 0xE1);
  RAW(a, 0x68);
  a_jsl(a, target);
  RAW(a, 0x6B);
}
/* n acts per stage; nh acts when hasted (0 = none) */
static void charge_routine(struct asmb *a, unsigned n, unsigned nh, int buff, int item) {
  RAW(a, 0x1A);
  if (nh) {
    a_byte(a, 0xC9); a_byte(a, nh);
    a_br(a, 0x90, L_STORE);
    a_byte(a, 0xC9); a_byte(a, n);
    a_br(a, 0xB0, L_UP);
    RAW(a, 0x48);
    if (buff) {
      RAW(a, 0xBF, 0xB0, 0xE1, 0x7E);
      RAW(a, 0x29, 0x14, 0xC9, 0x14);
      a_br(a, 0xF0, L_YES);
    }
    if (item >= 0) {
      for (unsigned off = 0xE0; off <= 0xE2; off++) {
        RAW(a, 0xBF); a_byte(a, off); RAW(a, 0xE1, 0x7E);
        a_byte(a, 0xC9); a_byte(a, (unsigned)item);
        a_br(a, 0xF0, L_YES);
      }
    }
    a_br(a, 0x80, L_NO);
    a_label(a, L_YES);
    RAW(a, 0x68);
    a_br(a, 0x80, L_UP);
    a_label(a, L_NO);
    RAW(a, 0x68);
    a_label(a, L_STORE);
    a_jml(a, RET_CHARGE_STORE);
    a_label(a, L_UP);
    a_jml(a, RET_CHARGE_UP);
  } else {
    a_byte(a, 0xC9); a_byte(a, n);
    a_br(a, 0x90, L_STORE);
    a_jml(a, RET_CHARGE_UP);
    a_label(a, L_STORE);
    a_jml(a, RET_CHARGE_STORE);
  }
}

/* quality-of-life group */
static void levelup_routine(struct asmb *a, int mp) {
  RAW(a, 0xBD, 0x84, 0xE1);
  RAW(a, 0x9D, 0x82, 0xE1);
  RAW(a, 0xE2, 0x20);
  if (mp) RAW(a, 0xBD, 0x87, 0xE1, 0x9D, 0x86, 0xE1);
  RAW(a, 0x6B);
}
static void deadguard_routine(struct asmb *a) {
  RAW(a, 0xA5, 0x80, 0x09, 0x40);
  RAW(a, 0x48);
  RAW(a, 0xBD, 0x91, 0xE1);
  a_br(a, 0x10, L_ALIVE);
  RAW(a, 0xC2, 0x20);
  RAW(a, 0x9E, 0x82, 0xE1);
  RAW(a, 0xE2, 0x20);
  a_label(a, L_ALIVE);
  RAW(a, 0x68, 0x6B);
}
static void shopfill_routine(struct asmb *a, unsigned fail) {
  RAW(a, 0xC2, 0x30);
  a_br(a, 0xF0, L_OK);
  RAW(a, 0x68);
  RAW(a, 0xE2, 0x20, 0x68, 0xC2, 0x30);
  a_jml(a, fail);
  a_label(a, L_OK);
  RAW(a, 0xF4, 0x08, 0x00);
  a_label(a, L_PAY);
  RAW(a, 0xC2, 0x30);
  RAW(a, 0xAF, 0x6A, 0xCC, 0x7E);
  RAW(a, 0x38, 0xED, 0x22, 0x18);
  RAW(a, 0x8F, 0x6A, 0xCC, 0x7E);
  a_br(a, 0xB0, L_PAID);
  RAW(a, 0xE2, 0x20);
  RAW(a, 0xAF, 0x6C, 0xCC, 0x7E);
  RAW(a, 0x3A);
  RAW(a, 0x8F, 0x6C, 0xCC, 0x7E);
  RAW(a, 0xC2, 0x30);
  a_label(a, L_PAID);
  RAW(a, 0xA3, 0x01, 0x3A, 0x83, 0x01);
  a_br(a, 0xD0, L_MORE);
  a_jmp(a, L_DONE);
  a_label(a, L_MORE);
  RAW(a, 0xAD, 0x0A, 0x18);
  RAW(a, 0x29, 0xFF, 0x00);
  RAW(a, 0x18, 0x6D, 0x3C, 0x18);
  RAW(a, 0xAA);
  RAW(a, 0xBF, 0x00, 0x00, 0xD8);
  RAW(a, 0x29, 0xFF, 0x00);
  RAW(a, 0xC9, 0xBA, 0x00);
  a_br(a, 0xB0, L_LO);
  a_jmp(a, L_DONE);
  a_label(a, L_LO);
  RAW(a, 0xC9, 0xC6, 0x00);
  a_br(a, 0x90, L_HI);
  a_jmp(a, L_DONE);
  a_label(a, L_HI);
  RAW(a, 0x38, 0xE9, 0x7A, 0x00);
  RAW(a, 0x48);
  RAW(a, 0xAF, 0x6A, 0xCC, 0x7E);
  RAW(a, 0x38, 0xED, 0x22, 0x18);
  a_br(a, 0xB0, L_AFFORD);
  RAW(a, 0xE2, 0x20);
  RAW(a, 0xAF, 0x6C, 0xCC, 0x7E);
  RAW(a, 0x3A, 0xC9, 0xFF);
  RAW(a, 0xC2, 0x20);
  a_br(a, 0xD0, L_AFFORD);
  RAW(a, 0x68);
  a_jmp(a, L_DONE);
  a_label(a, L_AFFORD);
  RAW(a, 0x68);
  a_jsl(a, 0x51);
  RAW(a, 0xE2, 0x20);
  RAW(a, 0xAF, 0x0F, 0xCF, 0x7E);
  RAW(a, 0x48, 0xA9, 0x00, 0x8F, 0x0F, 0xCF, 0x7E, 0x68);
  RAW(a, 0xC2, 0x20);
  RAW(a, 0x29, 0xFF, 0x00);
  a_br(a, 0xD0, L_DONE);
  a_jmp(a, L_PAY);
  a_label(a, L_DONE);
  RAW(a, 0xC2, 0x30, 0x68);
  RAW(a, 0x6B);
}
static void magic_rate_routine(struct asmb *a, int mult) {
  RAW(a, 0xA9, 0x09, 0x38, 0xE5, 0x9D);
  if (mult == 2) RAW(a, 0x0A);
  else if (mult == 3) RAW(a, 0x85, 0xAC, 0x0A, 0x18, 0x65, 0xAC);
  RAW(a, 0x85, 0xAC, 0x6B);
}
static void fade_routine(struct asmb *a) {
  RAW(a, 0xA5, 0xE5, 0x4A);
  a_br(a, 0xD0, L_OK);
  RAW(a, 0x1A);
  a_label(a, L_OK);
  RAW(a, 0x85, 0xE4, 0x6B);
}

/* ---- site location: instruction signatures inside bank C0 (first 64 KiB), '??' wildcards are 0x100 */
static int locate(const uint8_t *body, const int *pat, int n, int first, unsigned *found) {
  int count = 0;
  for (int o = 0; o + n <= 0x10000; o++) {
    int ok = 1;
    for (int k = 0; k < n && ok; k++) if (pat[k] != 0x100 && body[o + k] != pat[k]) ok = 0;
    if (ok) { if (count == 0) *found = (unsigned)o; count++; }
  }
  (void)first;
  return count;
}
#define W 0x100
static const int PAT_FLINCH[] = {0xC2,0x20,0xBD,0x90,0xE1,0x89,0x78,0xC5,0xE2,0x20,0xD0,0x55,0xA9,0x01,0x85,0xAC,0xA9,0x0D,0x85,0xA4,0xBD,0xFB,0xE1,0x89,0x81};
static const int PAT_CRIT[] = {0x22,W,W,0xC0,0xA6,0x85,0xA4,0x83,0xC2,0x20,0x06,0x89,0xE2,0x20,0x60};
static const int PAT_CRITBONUS[] = {0xB9,0x96,0xE1,0x85,0xA4,0xBD,0x90,0xE1,0x89,0x10,0xF0,0x02,0x06,0xA4,0xB9,0xFB,0xE1,0x10,0x16,0xA5,0x82,0xC9,0x01,0xD0,0x09,0xA5,0xA4,0x18,0x69,0x0A,0x85,0xA4,0x80,0x07,0xA5,0xA4,0x18,0x69,0x05,0x85,0xA4};
static const int PAT_ZERO[] = {0xE2,0x20,0x22,W,W,0xC0,0xBD,0xFB,0xE1,0x10,0x01,0x60};
static const int PAT_WLEVEL[] = {0xA9,0x09,0x38,0xF9,0x9C,0xE1,0x85,0xA6};
static const int PAT_LEVELUP[] = {0xBD,0x84,0xE1,0x9D,0x82,0xE1,0xE2,0x20,0xA5,0x80,0x09,0x40,0x85,0x80};
static const int PAT_HALVE[] = {0xC2,0x20,0x46,0x89,0x64,0xCC,0x64,0xD8,0xE2,0x20,0x64,0xCE};
static const int PAT_EXPGATE[] = {0xA6,0x83,0xBD,0x00,0xE0,0xC9,0x01,0xD0,0x07,0xBC,0x82,0xE1,0xF0,0x02,0x80,0x07};

static const int PAT_LVTAIL[] = {0xE2,0x20,0xA5,0x80,0x09,0x40,0x85,0x80};
static const int PAT_LV_RANDO[] = {0x22,W,W,W,0xEA,0xEA,0xE2,0x20,0xA5,0x80,0x09,0x40,0x85,0x80};
static const int PAT_SHOP[] = {0x20,W,W,0xD0,W,0x20,W,W,0x20,W,W,0xA2,W,W,0x4C};
static const int PAT_MSG[] = {0xE2,0x20,0xA9,0x60,0x99,0x00,0xFE,0xC8,0xE6,0x9F,0xE6,0xA1};
static const int PAT_RING_PHASE[] = {0xC9,0x10,0x00,0x90,0x2A,0x18,0x69,0xF6,0xFF,0x80,0x1F,0xC9,0x24,0x00,0xB0,0x22,0x18,0x69,0x0A,0x00,0x80,0x14,0xC9,0xFE,0x00,0xB0,0x14,0x18,0x69,0x0A,0x00,0x80,0x09,0xC9,0x24,0x00,0x90,0x0C,0x18,0x69,0xF6,0xFF};
static const int PAT_RING_STEP[] = {0xA9,0x12,0x00,0x8D,0x24,0x18,0xA9,0x48,0x08,0x8D,0x32,0x18};
static const int PAT_RING_SUB[] = {0xB5,0x08,0x38,0xE9,0x12,0x00,0x29,0x00,0xFC,0xF0,0x21};
static const int PAT_RING_ADD[] = {0xB5,0x08,0x18,0x69,0x12,0x00,0xC9,0x00,0x04,0x90,0x10};
static const uint8_t RANDO_MP_ROUTINE[] = {0xBD,0x84,0xE1,0x9D,0x82,0xE1,0xE2,0x20,0xBD,0x87,0xE1,0x9D,0x86,0xE1,0xC2,0x20,0x6B};
static const uint8_t RANDO_MAGIC_ROUTINE[] = {0xA9,0x09,0x38,0xE5,0x9D,0x0A,0x0A,0x85,0xAC,0x6B};

struct sites { int ok_flinch, ok_crit, ok_cb, ok_zero; unsigned flinch, crit, crit_target, crit_girl, crit_other, zero, zero_target, wlevel, levelup, expgate, halve,
               lvtail, msg, ring_phase, ring_step, ring_sub, ring_add, shop, shop_fail; uint8_t shop_old[5]; int ok_shop, rando_mp, rando_magic; };

static int routine_at(const uint8_t *body, size_t blen, unsigned t, const uint8_t *want, size_t n) {
  return (size_t)t + n <= blen && memcmp(body + t, want, n) == 0;
}

static void resolve(const uint8_t *body, size_t blen, struct sites *s) {
  unsigned f;
  memset(s, 0, sizeof *s);
  if (locate(body, PAT_FLINCH, sizeof PAT_FLINCH / sizeof *PAT_FLINCH, 0, &f) == 1 && body[f + 0x61] == 0x60) { s->ok_flinch = 1; s->flinch = f; }
  if (locate(body, PAT_CRIT, sizeof PAT_CRIT / sizeof *PAT_CRIT, 0, &f) == 1) { s->ok_crit = 1; s->crit = f; s->crit_target = (unsigned)(body[f + 1] | body[f + 2] << 8); }
  if (locate(body, PAT_CRITBONUS, sizeof PAT_CRITBONUS / sizeof *PAT_CRITBONUS, 0, &f) == 1) { s->ok_cb = 1; s->crit_girl = f + 29; s->crit_other = f + 38; }
  if (locate(body, PAT_ZERO, sizeof PAT_ZERO / sizeof *PAT_ZERO, 0, &f) == 1) { s->ok_zero = 1; s->zero = f + 2; s->zero_target = (unsigned)(body[f + 3] | body[f + 4] << 8); }
  s->wlevel = locate(body, PAT_WLEVEL, sizeof PAT_WLEVEL / sizeof *PAT_WLEVEL, 0, &f) == 1 ? f : 0x4358;
  s->levelup = locate(body, PAT_LEVELUP, sizeof PAT_LEVELUP / sizeof *PAT_LEVELUP, 0, &f) == 1 ? f : 0x44D3;
  s->halve = locate(body, PAT_HALVE, sizeof PAT_HALVE / sizeof *PAT_HALVE, 0, &f) == 1 ? f + 2 : 0x5383;
  s->expgate = locate(body, PAT_EXPGATE, sizeof PAT_EXPGATE / sizeof *PAT_EXPGATE, 0, &f) == 1 ? f + 12 : 0x446B;
  s->lvtail = locate(body, PAT_LVTAIL, sizeof PAT_LVTAIL / sizeof *PAT_LVTAIL, 0, &f) == 1 ? f + 2 : 0x44DB;
  if (locate(body, PAT_SHOP, sizeof PAT_SHOP / sizeof *PAT_SHOP, 0, &f) == 1) {
    int rel = body[f + 4]; if (rel > 127) rel -= 256;
    s->ok_shop = 1; s->shop = f + 3; s->shop_fail = (unsigned)((int)f + 5 + rel);
    memcpy(s->shop_old, body + f + 3, 5);
  }
  s->msg = locate(body, PAT_MSG, sizeof PAT_MSG / sizeof *PAT_MSG, 0, &f) == 1 ? f + 3 : 0x5CF4;
  s->ring_phase = locate(body, PAT_RING_PHASE, sizeof PAT_RING_PHASE / sizeof *PAT_RING_PHASE, 0, &f) == 1 ? f : 0x6C1D;
  s->ring_step = locate(body, PAT_RING_STEP, sizeof PAT_RING_STEP / sizeof *PAT_RING_STEP, 0, &f) == 1 ? f : 0x6C7B;
  s->ring_sub = locate(body, PAT_RING_SUB, sizeof PAT_RING_SUB / sizeof *PAT_RING_SUB, 0, &f) == 1 ? f : 0x6CEA;
  s->ring_add = locate(body, PAT_RING_ADD, sizeof PAT_RING_ADD / sizeof *PAT_RING_ADD, 0, &f) == 1 ? f : 0x6CFB;
  if (locate(body, PAT_LV_RANDO, sizeof PAT_LV_RANDO / sizeof *PAT_LV_RANDO, 0, &f) == 1) {
    unsigned t = (unsigned)(body[f + 1] | body[f + 2] << 8 | (body[f + 3] & 0x3F) << 16);
    s->rando_mp = routine_at(body, blen, t, RANDO_MP_ROUTINE, sizeof RANDO_MP_ROUTINE);
  }
  if (blen > 0x104DF3 && body[0x104DEC] == 0x22 && body[0x104DF0] == 0xEA && body[0x104DF1] == 0xEA && body[0x104DF2] == 0xEA) {
    unsigned t = (unsigned)(body[0x104DED] | body[0x104DEE] << 8 | (body[0x104DEF] & 0x3F) << 16);
    s->rando_magic = routine_at(body, blen, t, RANDO_MAGIC_ROUTINE, sizeof RANDO_MAGIC_ROUTINE);
  }
}

/* ---- records */
struct rec { uint32_t off; uint8_t len; int opt; int routine; uint8_t before[8], after[8]; const uint8_t *code; };
struct plan {
  struct rec r[SGP_MAX_RECS]; int n;
  uint8_t code[FREE_LEN]; unsigned code_len;
  uint32_t unresolved;   /* options whose site could not be found exactly once */
  const uint8_t *ff;
};
static void add_small(struct plan *p, unsigned off, int opt, const uint8_t *before, const uint8_t *after, unsigned len) {
  if (p->n >= SGP_MAX_RECS || len > 8) return;
  struct rec *r = &p->r[p->n++];
  r->off = off; r->len = (uint8_t)len; r->opt = opt; r->routine = 0;
  memcpy(r->before, before, len); memcpy(r->after, after, len);
}
static void jsl_rec(uint8_t *out, unsigned base, int op) { out[0] = (uint8_t)op; out[1] = base & 0xFF; out[2] = (base >> 8) & 0xFF; out[3] = (uint8_t)(0xC0 + ((base >> 16) & 0x3F)); }

/* routines are queued with their owner; records for them are added at the end, in emission order */
struct routine { int owner; unsigned pos, len; };

static int emit(struct plan *p, struct routine *rt, int *nrt, int owner, struct asmb *a) {
  if (a_done(a) != 0 || p->code_len + a->n > FREE_LEN) return -1;
  rt[*nrt].owner = owner; rt[*nrt].pos = p->code_len; rt[*nrt].len = a->n; (*nrt)++;
  memcpy(p->code + p->code_len, a->b, a->n);
  p->code_len += a->n;
  return (int)(FREE_BASE + p->code_len - a->n);
}

static const uint8_t RECHARGE_V[5] = {0x69, 0x32, 0x9D, 0xED, 0xE1};
static const uint8_t FLINCH_V[5] = {0xC2, 0x20, 0xBD, 0x90, 0xE1};
static const uint8_t CHARGE_V[4] = {0x1A, 0xC9, 0x2D, 0x90};
static const uint8_t WLEVEL_V[8] = {0xA9, 0x09, 0x38, 0xF9, 0x9C, 0xE1, 0x85, 0xA6};

static int make_plan(const uint8_t *body, size_t blen, const struct sgp_opts *o, struct plan *p) {
  struct routine rt[12];
  int nrt = 0;
  struct asmb a;
  struct sites S;
  uint8_t aft[8];
  memset(p, 0, sizeof *p);
  resolve(body, blen, &S);
  const int32_t *v = o->v;
#define BASE_OR_FAIL(owner) do { if (base < 0) return -1; (void)(owner); } while (0)
  int base;
  if (v[SGP_WEAPON_RECHARGE] != 0) {
    a_init(&a, FREE_BASE + p->code_len);
    recharge_routine(&a, v[SGP_WEAPON_RECHARGE] - 1);
    base = emit(p, rt, &nrt, SGP_WEAPON_RECHARGE, &a); BASE_OR_FAIL(0);
    jsl_rec(aft, (unsigned)base, 0x22); aft[4] = 0xEA;
    static const unsigned sites[3] = {0xF942, 0x1CADE, 0x2C248};
    for (int i = 0; i < 3; i++) add_small(p, sites[i], SGP_WEAPON_RECHARGE, RECHARGE_V, aft, 5);
  }
  if (v[SGP_RUN_RECHARGE] != 0) {
    a_init(&a, FREE_BASE + p->code_len);
    recharge_routine(&a, v[SGP_RUN_RECHARGE] == 2 ? 2 : 0);
    base = emit(p, rt, &nrt, SGP_RUN_RECHARGE, &a); BASE_OR_FAIL(0);
    jsl_rec(aft, (unsigned)base, 0x22); aft[4] = 0xEA;
    add_small(p, 0xB788, SGP_RUN_RECHARGE, RECHARGE_V, aft, 5);
  }
  int mf = v[SGP_MONSTER_FLINCH], hf = v[SGP_HERO_FLINCH];
  if (mf != 0 || hf != 0) {
    int owner = mf != 0 ? SGP_MONSTER_FLINCH : SGP_HERO_FLINCH;
    if (!S.ok_flinch) { p->unresolved |= 1u << owner; }
    else {
      a_init(&a, FREE_BASE + p->code_len);
      flinch_routine(&a, mf, hf, S.flinch + 0x61, S.flinch + 5);
      base = emit(p, rt, &nrt, owner, &a); BASE_OR_FAIL(0);
      jsl_rec(aft, (unsigned)base, 0x5C); aft[4] = 0xEA;
      add_small(p, S.flinch, owner, FLINCH_V, aft, 5);
      if (mf == 1 || mf == 2) {
        if (!S.ok_crit || !S.ok_zero) p->unresolved |= 1u << SGP_MONSTER_FLINCH;
        else {
          uint8_t bef[4];
          a_init(&a, FREE_BASE + p->code_len);
          crit_routine(&a, S.crit_target);
          base = emit(p, rt, &nrt, SGP_MONSTER_FLINCH, &a); BASE_OR_FAIL(0);
          bef[0] = 0x22; bef[1] = S.crit_target & 255; bef[2] = S.crit_target >> 8; bef[3] = 0xC0;
          jsl_rec(aft, (unsigned)base, 0x22);
          add_small(p, S.crit, SGP_MONSTER_FLINCH, bef, aft, 4);
          a_init(&a, FREE_BASE + p->code_len);
          zero_routine(&a, S.zero_target);
          base = emit(p, rt, &nrt, SGP_MONSTER_FLINCH, &a); BASE_OR_FAIL(0);
          bef[1] = S.zero_target & 255; bef[2] = S.zero_target >> 8;
          jsl_rec(aft, (unsigned)base, 0x22);
          add_small(p, S.zero, SGP_MONSTER_FLINCH, bef, aft, 4);
        }
      }
    }
  }
  if (v[SGP_CRIT_MULTIPLIER] != 2) {
    if (!S.ok_crit) p->unresolved |= 1u << SGP_CRIT_MULTIPLIER;
    else {
      static const uint8_t bef[4] = {0xC2, 0x20, 0x06, 0x89};
      a_init(&a, FREE_BASE + p->code_len);
      critmul_routine(&a, v[SGP_CRIT_MULTIPLIER]);
      base = emit(p, rt, &nrt, SGP_CRIT_MULTIPLIER, &a); BASE_OR_FAIL(0);
      jsl_rec(aft, (unsigned)base, 0x22);
      add_small(p, S.crit + 8, SGP_CRIT_MULTIPLIER, bef, aft, 4);
    }
  }
  if (v[SGP_WEAPON_LEVEL_RATE] != 1) {
    a_init(&a, FREE_BASE + p->code_len);
    wlevel_routine(&a, v[SGP_WEAPON_LEVEL_RATE]);
    base = emit(p, rt, &nrt, SGP_WEAPON_LEVEL_RATE, &a); BASE_OR_FAIL(0);
    jsl_rec(aft, (unsigned)base, 0x22); aft[4] = aft[5] = aft[6] = aft[7] = 0xEA;
    add_small(p, S.wlevel, SGP_WEAPON_LEVEL_RATE, WLEVEL_V, aft, 8);
  }
  if (v[SGP_WALK_SPEED] != 2) {
    static const struct { unsigned off; uint8_t van; } WS[4] = {{0xB71A, 0x02}, {0xB720, 0x82}, {0xB728, 0x02}, {0xB72E, 0x82}};
    for (int i = 0; i < 4; i++) {
      uint8_t b1 = WS[i].van, a1 = (uint8_t)((WS[i].van & 0x80) | v[SGP_WALK_SPEED]);
      add_small(p, WS[i].off, SGP_WALK_SPEED, &b1, &a1, 1);
    }
  }
  {   /* charge_params */
    int f = v[SGP_CHARGE_FRAMES];
    if (f < 0) { static const int T[4] = {90, 45, 22, 1}; f = T[v[SGP_CHARGE_SPEED]]; }
    int reload = f > 45, acts = reload ? (f + 1) / 2 : f, haste = 0;
    int item = v[SGP_CHARGE_BONUS_ITEM];
    if (v[SGP_CHARGE_SPEEDUP_BONUS] || item >= 0) {
      haste = acts / 2 > 1 ? acts / 2 : 1;
      if (haste >= acts) haste = 0;
    }
    if (!(reload == 1 && acts == 45 && haste == 0)) {
      a_init(&a, FREE_BASE + p->code_len);
      charge_routine(&a, (unsigned)acts, (unsigned)haste, v[SGP_CHARGE_SPEEDUP_BONUS], item);
      base = emit(p, rt, &nrt, SGP_CHARGE_SPEED, &a); BASE_OR_FAIL(0);
      jsl_rec(aft, (unsigned)base, 0x5C);
      add_small(p, 0xB3C6, SGP_CHARGE_SPEED, CHARGE_V, aft, 4);
      if (reload == 0) { static const uint8_t b1 = 1, a1 = 0; add_small(p, 0xB3A7 + 1, SGP_CHARGE_SPEED, &b1, &a1, 1); }
    }
  }
  {
    static const unsigned van[2] = {0x0A, 0x05};
    for (int i = 0; i < 2; i++) {
      int val = v[i ? SGP_CRIT_OTHERS : SGP_CRIT_GIRL];
      if (val >= 0 && val != (int)van[i]) {
        int opt = i ? SGP_CRIT_OTHERS : SGP_CRIT_GIRL;
        if (!S.ok_cb) { p->unresolved |= 1u << opt; continue; }
        uint8_t b1 = (uint8_t)van[i], a1 = (uint8_t)val;
        add_small(p, i ? S.crit_other : S.crit_girl, opt, &b1, &a1, 1);
      }
    }
  }
  {   /* quality-of-life group */
    static const struct { unsigned off; uint8_t van; } CHEST[3] = {{0x115FB8, 0x83}, {0x11ED01, 0x8F}, {0x11EC84, 0x8F}};
    static const uint8_t ff = 0xFF;
    if (v[SGP_QOL_CHEST_FAST]) for (int i = 0; i < 3; i++) add_small(p, CHEST[i].off, SGP_QOL_CHEST_FAST, &CHEST[i].van, &ff, 1);
    if (v[SGP_QOL_FAST_TRANSITIONS]) { static const uint8_t b1 = 0xE6, a1 = 0xE7; add_small(p, 0x8B76, SGP_QOL_FAST_TRANSITIONS, &b1, &a1, 1); }
    if (v[SGP_QOL_FAST_FADES]) {
      static const uint8_t bef[4] = {0xA5, 0xE5, 0x85, 0xE4};
      a_init(&a, FREE_BASE + p->code_len);
      fade_routine(&a);
      base = emit(p, rt, &nrt, SGP_QOL_FAST_FADES, &a); BASE_OR_FAIL(0);
      jsl_rec(aft, (unsigned)base, 0x22);
      add_small(p, 0x8B71, SGP_QOL_FAST_FADES, bef, aft, 4);
    }
    if (v[SGP_QOL_UNREADY_DAMAGE]) { static const uint8_t bef[2] = {0x46, 0x89}, nop[2] = {0xEA, 0xEA}; add_small(p, S.halve, SGP_QOL_UNREADY_DAMAGE, bef, nop, 2); }
    if (v[SGP_QOL_ITEM_LIMIT] == 7) { static const uint8_t bef[2] = {0xC9, 0x9F}, nop[2] = {0xEA, 0xEA}; add_small(p, 0x6559, SGP_QOL_ITEM_LIMIT, bef, nop, 2); }
    int mp = v[SGP_QOL_LEVELUP_MP], dead = v[SGP_QOL_DEAD_EXP];
    if (mp && !S.rando_mp) {
      static const uint8_t bef[8] = {0xBD, 0x84, 0xE1, 0x9D, 0x82, 0xE1, 0xE2, 0x20};
      a_init(&a, FREE_BASE + p->code_len);
      levelup_routine(&a, 1);
      base = emit(p, rt, &nrt, SGP_QOL_LEVELUP_MP, &a); BASE_OR_FAIL(0);
      jsl_rec(aft, (unsigned)base, 0x22); aft[4] = aft[5] = aft[6] = aft[7] = 0xEA;
      add_small(p, S.levelup, SGP_QOL_LEVELUP_MP, bef, aft, 8);
    }
    if (dead) {
      static const uint8_t bef[2] = {0xF0, 0x02}, nop[2] = {0xEA, 0xEA}, tail[4] = {0xA5, 0x80, 0x09, 0x40};
      add_small(p, S.expgate, SGP_QOL_DEAD_EXP, bef, nop, 2);
      a_init(&a, FREE_BASE + p->code_len);
      deadguard_routine(&a);
      base = emit(p, rt, &nrt, SGP_QOL_DEAD_EXP, &a); BASE_OR_FAIL(0);
      jsl_rec(aft, (unsigned)base, 0x22);
      add_small(p, S.lvtail, SGP_QOL_DEAD_EXP, tail, aft, 4);
    }
    if (v[SGP_QOL_MAGIC_RATE] != 1 && !S.rando_magic) {
      static const uint8_t bef[7] = {0xA9, 0x09, 0x38, 0xE5, 0x9D, 0x85, 0xAC};
      a_init(&a, FREE_BASE + p->code_len);
      magic_rate_routine(&a, v[SGP_QOL_MAGIC_RATE]);
      base = emit(p, rt, &nrt, SGP_QOL_MAGIC_RATE, &a); BASE_OR_FAIL(0);
      jsl_rec(aft, (unsigned)base, 0x22); aft[4] = aft[5] = aft[6] = 0xEA;
      add_small(p, 0x104DEC, SGP_QOL_MAGIC_RATE, bef, aft, 7);
    }
    if (v[SGP_QOL_RING_SPEED] == 2) {
      static const uint8_t b12 = 0x12, a24 = 0x24, bp = 0x0A, ap = 0x14, bn = 0xF6, an = 0xEC;
      add_small(p, S.ring_step + 1, SGP_QOL_RING_SPEED, &b12, &a24, 1);
      add_small(p, S.ring_sub + 4, SGP_QOL_RING_SPEED, &b12, &a24, 1);
      add_small(p, S.ring_add + 4, SGP_QOL_RING_SPEED, &b12, &a24, 1);
      add_small(p, S.ring_phase + 0x07, SGP_QOL_RING_SPEED, &bn, &an, 1);
      add_small(p, S.ring_phase + 0x12, SGP_QOL_RING_SPEED, &bp, &ap, 1);
      add_small(p, S.ring_phase + 0x1D, SGP_QOL_RING_SPEED, &bp, &ap, 1);
      add_small(p, S.ring_phase + 0x28, SGP_QOL_RING_SPEED, &bn, &an, 1);
    }
    if (v[SGP_QOL_MESSAGE_SPEED] != 1) {
      static const uint8_t b60 = 0x60;
      uint8_t a1 = (uint8_t)(0x60 / v[SGP_QOL_MESSAGE_SPEED]);
      add_small(p, S.msg, SGP_QOL_MESSAGE_SPEED, &b60, &a1, 1);
    }
    if (v[SGP_QOL_SHOP_FILL]) {
      if (!S.ok_shop) p->unresolved |= 1u << SGP_QOL_SHOP_FILL;
      else {
        a_init(&a, FREE_BASE + p->code_len);
        shopfill_routine(&a, S.shop_fail);
        base = emit(p, rt, &nrt, SGP_QOL_SHOP_FILL, &a); BASE_OR_FAIL(0);
        jsl_rec(aft, (unsigned)base, 0x22); aft[4] = 0xEA;
        add_small(p, S.shop, SGP_QOL_SHOP_FILL, S.shop_old, aft, 5);
      }
    }
    if (v[SGP_QOL_SCROLL_DEADZONE]) {
      static const uint8_t Z[2][4] = {{0x50, 0xB0, 0x50, 0xA0}, {0x60, 0xA0, 0x60, 0x90}};
      static const struct { unsigned off; uint8_t van; } ZS[4] = {{0xD903, 0x38}, {0xD90A, 0xC8}, {0xD952, 0x48}, {0xD959, 0xB0}};
      for (int i = 0; i < 4; i++) add_small(p, ZS[i].off, SGP_QOL_SCROLL_DEADZONE, &ZS[i].van, &Z[v[SGP_QOL_SCROLL_DEADZONE] - 1][i], 1);
    }
  }
  for (int i = 0; i < nrt && p->n < SGP_MAX_RECS; i++) {
    struct rec *r = &p->r[p->n++];
    r->off = FREE_BASE + rt[i].pos; r->len = (uint8_t)(rt[i].len > 255 ? 255 : rt[i].len); r->opt = rt[i].owner; r->routine = 1;
    r->code = p->code + rt[i].pos;
    r->len = (uint8_t)rt[i].len;
  }
  return 0;
}

static int rec_state(const uint8_t *body, size_t blen, const struct rec *r) {   /* 0 ok, 1 already, 2 conflict */
  if (r->off + r->len > blen) return 2;
  const uint8_t *cur = body + r->off;
  const uint8_t *after = r->routine ? r->code : r->after;
  int is_before = 1, is_after = 1;
  for (unsigned i = 0; i < r->len; i++) {
    uint8_t bv = r->routine ? 0xFF : r->before[i];
    if (cur[i] != bv) is_before = 0;
    if (cur[i] != after[i]) is_after = 0;
  }
  return is_before ? 0 : is_after ? 1 : 2;
}

static int checksum_ok(const uint8_t *b, size_t n) {
  unsigned comp = b[0xFFDC] | b[0xFFDD] << 8, chk = b[0xFFDE] | b[0xFFDF] << 8;
  uint32_t s = 0;
  for (size_t i = 0; i < n; i++) s += b[i];
  return ((comp ^ chk) == 0xFFFF) && chk == (s & 0xFFFF);
}
static void set_checksum(uint8_t *b, size_t n) {
  b[0xFFDC] = 0xFF; b[0xFFDD] = 0xFF; b[0xFFDE] = 0; b[0xFFDF] = 0;
  uint32_t s = 0;
  for (size_t i = 0; i < n; i++) s += b[i];
  s &= 0xFFFF;
  b[0xFFDE] = (uint8_t)s; b[0xFFDF] = (uint8_t)(s >> 8);
  b[0xFFDC] = (uint8_t)(s ^ 0xFFFF); b[0xFFDD] = (uint8_t)((s ^ 0xFFFF) >> 8);
}

/* Foreign uses of the monster byte $E16B (the critical mark): the same heuristic as scan_mark_use in the Python tool. */
static int mark_use_hits(const uint8_t *b, size_t n) {
  static const uint8_t abs_ops[] = {0x0D,0x0E,0x1D,0x1E,0x2C,0x2D,0x2E,0x3C,0x3D,0x3E,0x4D,0x4E,0x5D,0x6D,0x6E,0x7D,0x7E,0x8C,0x8D,0x8E,0x9C,0x9D,0x9E,
    0xAC,0xAD,0xAE,0xBC,0xBD,0xBE,0xCC,0xCD,0xCE,0xDD,0xDE,0xEC,0xED,0xEE,0xFD,0xFE,0x19,0x39,0x59,0x79,0x99,0xB9,0xD9,0xF9,0x1C,0x0C,0xA0,0xA2,0xC0,0xE0};
  int hits = 0;
  for (size_t i = 1; i + 1 < n; i++) {
    size_t o = i - 1;
    if (b[i] == 0x6B && b[i + 1] == 0x01) {
      if (o == 0x12A283 || o == 0x1D9906 || (o >= FREE_BASE && o < FREE_BASE + FREE_LEN)) continue;
      for (unsigned k = 0; k < sizeof abs_ops; k++) if (b[o] == abs_ops[k]) { hits++; break; }
    } else if (i + 2 < n && b[i] == 0x6B && b[i + 1] == 0xE1 && b[i + 2] == 0x7E) {
      if (o == 0x12A283 || o == 0x1D9906 || (o >= FREE_BASE && o < FREE_BASE + FREE_LEN)) continue;
      if ((b[o] & 0x0F) == 0x0F) hits++;
    }
  }
  return hits;
}

int sgp_apply(uint8_t *rom, size_t len, const struct sgp_opts *o, struct sgp_report *rep) {
  static struct plan P;
  struct sgp_report local;
  if (!rep) rep = &local;
  memset(rep, 0, sizeof *rep);
  size_t hdr = len % 0x8000;
  if (hdr != 0 && hdr != 512) { snprintf(rep->why, sizeof rep->why, "unexpected ROM size %lu", (unsigned long)len); return -1; }
  uint8_t *body = rom + hdr;
  size_t blen = len - hdr;
  if (blen <= FREE_BASE + FREE_LEN || blen < 0x10000) { snprintf(rep->why, sizeof rep->why, "ROM too small"); return -1; }
  for (int i = 0; i < SGP_COUNT; i++)
    if (!sgp_valid(i, o->v[i])) { snprintf(rep->why, sizeof rep->why, "bad value for %s", KEYS[i]); return -1; }
  if (sgp_is_default(o)) return 0;
  if (make_plan(body, blen, o, &P) != 0) { snprintf(rep->why, sizeof rep->why, "free area overflow or branch out of range"); return -1; }
  int state[SGP_MAX_RECS];
  uint32_t bad = P.unresolved;
  for (int i = 0; i < P.n; i++) {
    state[i] = rec_state(body, blen, &P.r[i]);
    if (state[i] == 2) {
      if (!(bad & (1u << P.r[i].opt)) && !rep->why[0])
        snprintf(rep->why, sizeof rep->why, "%s: bytes at 0x%06X differ from vanilla", KEYS[P.r[i].opt], (unsigned)P.r[i].off);
      bad |= 1u << P.r[i].opt;
    }
  }
  if (P.unresolved && !rep->why[0]) {
    for (int i = 0; i < SGP_COUNT; i++) if (P.unresolved & (1u << i)) { snprintf(rep->why, sizeof rep->why, "%s: code site not found in this ROM", KEYS[i]); break; }
  }
  int changed = 0;
  int was_valid = checksum_ok(body, blen);
  if (o->v[SGP_MONSTER_FLINCH] == 1 || o->v[SGP_MONSTER_FLINCH] == 2) rep->mark_use_hits = mark_use_hits(body, blen);
  for (int i = 0; i < P.n; i++) {
    struct rec *r = &P.r[i];
    if (bad & (1u << r->opt)) continue;
    memcpy(body + r->off, r->routine ? r->code : r->after, r->len);
    rep->nrecs++;
    if (state[i] == 0) { changed = 1; rep->nchanged_bytes += r->len; }
  }
  rep->skipped = bad;
  if (changed && was_valid) { set_checksum(body, blen); rep->checksum_updated = 1; }
  return 0;
}
