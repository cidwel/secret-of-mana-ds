/* C port of tools/gameplay/som_gameplay.py (apply, --on-conflict skip).  See som_gameplay_apply.h.  Keep this file in step with the Python
 * tool: every routine, site and record below mirrors a function of the same name there. */
#include "som_gameplay_apply.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FREE_BASE 0x1C0540u
#define FREE_LEN 0x3C0u
#define CODE_MAX 2048

enum { O_WR, O_RR, O_MF, O_HF, O_CM, O_WL, O_WS, O_CS, O_GIRL, O_OTHERS, O_CHEST, O_TRANS, O_FADES, O_ITEMS, O_LVMP, O_DEADEXP, O_UNREADY, O_MAGIC, O_RING, O_MSG, O_SHOP, O_DEADZONE, O_COUNT };
static const char *const OPT_NAME[O_COUNT] = {"weapon_recharge", "run_recharge", "monster_flinch", "hero_flinch", "crit_multiplier",
                                              "weapon_level_rate", "walk_speed", "charge_speed", "hero_crit_bonus_girl", "hero_crit_bonus_others",
                                              "qol_chest_fast", "qol_fast_transitions", "qol_fast_fades", "qol_item_limit",
                                              "qol_levelup_mp_restore", "qol_dead_exp", "qol_unready_damage", "qol_magic_level_rate", "qol_ring_speed", "qol_message_speed", "qol_shop_fill", "qol_scroll_deadzone"};

static const char *const KEYS[] = {"weapon_recharge", "run_recharge", "monster_flinch", "charge_speed", "charge_frames_per_stage", "charge_speed_up_bonus",
                                   "hero_crit_bonus_girl", "hero_crit_bonus_others", "hero_flinch", "crit_multiplier", "walk_speed", "charge_bonus_item_id",
                                   "weapon_level_rate", "fix_checksum", "qol_chest_fast", "qol_fast_transitions", "qol_fast_fades",
                                   "qol_item_limit", "qol_levelup_mp_restore", "qol_dead_exp", "qol_unready_damage", "qol_magic_level_rate", "qol_scroll_deadzone", "qol_ring_speed", "qol_message_speed", "qol_shop_fill"};

int sga_supports(const char *key) {
  for (unsigned i = 0; i < sizeof KEYS / sizeof KEYS[0]; i++) if (!strcmp(KEYS[i], key)) return 1;
  return 0;
}

void sga_defaults(struct sga_opts *o) {
  memset(o, 0, sizeof *o);
  strcpy(o->weapon_recharge, "vanilla"); strcpy(o->run_recharge, "vanilla"); strcpy(o->monster_flinch, "vanilla");
  strcpy(o->hero_flinch, "vanilla"); strcpy(o->charge_speed, "vanilla"); strcpy(o->fix_checksum, "auto");
  o->charge_frames_per_stage = o->hero_crit_bonus_girl = o->hero_crit_bonus_others = o->charge_bonus_item_id = -1;
  o->crit_multiplier = 2; o->walk_speed = 2; o->weapon_level_rate = 1;
  o->qol_item_limit = 4; o->qol_magic_level_rate = 1; o->qol_ring_speed = 1; o->qol_message_speed = 1; strcpy(o->qol_scroll_deadzone, "vanilla");
}

int sga_is_default(const struct sga_opts *o) {
  struct sga_opts d;
  sga_defaults(&d);
  return !strcmp(o->weapon_recharge, d.weapon_recharge) && !strcmp(o->run_recharge, d.run_recharge) && !strcmp(o->monster_flinch, d.monster_flinch) &&
         !strcmp(o->hero_flinch, d.hero_flinch) && !strcmp(o->charge_speed, d.charge_speed) && o->charge_frames_per_stage < 0 && !o->charge_speed_up_bonus &&
         o->hero_crit_bonus_girl < 0 && o->hero_crit_bonus_others < 0 && o->crit_multiplier == 2 && o->walk_speed == 2 && o->weapon_level_rate == 1 &&
         o->charge_bonus_item_id < 0 && !o->qol_chest_fast && !o->qol_fast_transitions && !o->qol_fast_fades &&
         !o->qol_levelup_mp_restore && !o->qol_dead_exp && !o->qol_unready_damage && o->qol_item_limit == 4 && o->qol_magic_level_rate == 1 && o->qol_ring_speed == 1 && o->qol_message_speed == 1 && !o->qol_shop_fill && !strcmp(o->qol_scroll_deadzone, "vanilla");
}

/* ------------------------------------------------------------------ options file (flat JSON of scalars, optionally under "options") */
static const char *ws(const char *p) { while (*p && isspace((unsigned char)*p)) p++; return p; }

static const char *scan_str(const char *p, char *out, size_t n) {   /* p at the opening quote */
  size_t k = 0;
  for (p++; *p && *p != '"'; p++) {
    if (*p == '\\' && p[1]) p++;
    if (k + 1 < n) out[k++] = *p;
  }
  out[k] = 0;
  return *p == '"' ? p + 1 : NULL;
}

static int setval(struct sga_opts *o, const char *k, const char *v, int is_str, int is_null, char *err, size_t en) {
  char *end;
  long num = is_str || is_null ? 0 : strtol(v, &end, 10);
  int isnum = !is_str && !is_null && end != v && !*end;
  if (!strcmp(v, "true")) num = 1, isnum = 1;
  if (!strcmp(v, "false")) num = 0, isnum = 1;
#define STRKEY(name) if (!strcmp(k, #name)) { if (is_null) return 0; if (!is_str || strlen(v) >= sizeof o->name) goto bad; strcpy(o->name, v); return 0; }
#define INTKEY(name, nullval) if (!strcmp(k, #name)) { if (is_null) { o->name = nullval; return 0; } if (!isnum) goto bad; o->name = (int)num; return 0; }
  STRKEY(qol_scroll_deadzone) INTKEY(qol_chest_fast, 0) INTKEY(qol_fast_transitions, 0) INTKEY(qol_fast_fades, 0)
  INTKEY(qol_item_limit, 4) INTKEY(qol_levelup_mp_restore, 0) INTKEY(qol_dead_exp, 0) INTKEY(qol_unready_damage, 0) INTKEY(qol_magic_level_rate, 1)
  INTKEY(qol_ring_speed, 1) INTKEY(qol_message_speed, 1) INTKEY(qol_shop_fill, 0)
  STRKEY(weapon_recharge) STRKEY(run_recharge) STRKEY(monster_flinch) STRKEY(hero_flinch) STRKEY(charge_speed) STRKEY(fix_checksum)
  INTKEY(charge_frames_per_stage, -1) INTKEY(charge_speed_up_bonus, 0) INTKEY(hero_crit_bonus_girl, -1) INTKEY(hero_crit_bonus_others, -1)
  INTKEY(crit_multiplier, 2) INTKEY(walk_speed, 2) INTKEY(weapon_level_rate, 1) INTKEY(charge_bonus_item_id, -1)
  if (k[0] == '_') return 0;
  snprintf(err, en, "unknown option %s", k);
  return -1;
bad:
  snprintf(err, en, "bad value for %s", k);
  return -1;
}

int sga_parse(struct sga_opts *o, const char *json, char *err, size_t en) {
  const char *p = ws(json);
  sga_defaults(o);
  err[0] = 0;
  if (*p != '{') { snprintf(err, en, "not a JSON object"); return -1; }
  p = ws(p + 1);
  while (*p && *p != '}') {
    char key[64], val[96];
    if (*p != '"') { snprintf(err, en, "bad JSON"); return -1; }
    if (!(p = scan_str(p, key, sizeof key))) { snprintf(err, en, "bad JSON"); return -1; }
    p = ws(p);
    if (*p != ':') { snprintf(err, en, "bad JSON"); return -1; }
    p = ws(p + 1);
    if (*p == '{') {                                   /* {"options": {...}}: descend; any other object is an error */
      if (strcmp(key, "options")) { snprintf(err, en, "unknown option %s", key); return -1; }
      p = ws(p + 1);
      continue;
    }
    if (*p == '"') {
      if (!(p = scan_str(p, val, sizeof val))) { snprintf(err, en, "bad JSON"); return -1; }
      if (!strcmp(key, "options") || key[0] == '_') { p = ws(p); if (*p == ',') p = ws(p + 1); continue; }
      if (setval(o, key, val, 1, 0, err, en)) return -1;
    } else {
      size_t k = 0;
      while (*p && *p != ',' && *p != '}' && !isspace((unsigned char)*p)) { if (k + 1 < sizeof val) val[k++] = *p; p++; }
      val[k] = 0;
      if (setval(o, key, val, 0, !strcmp(val, "null"), err, en)) return -1;
    }
    p = ws(p);
    if (*p == ',') p = ws(p + 1);
  }
  return 0;
}

/* ------------------------------------------------------------------ tiny assembler */
enum { L_VAN, L_PASS, L_HERO, L_STRONG, L_STRONG16, L_STORE, L_UP, L_YES, L_NO, L_ALIVE, L_OK, L_PAY, L_PAID, L_MORE, L_LO, L_HI, L_AFFORD, L_DONE, L_COUNT };

struct asm_ {
  uint8_t *b; size_t n; uint32_t base;
  long label[L_COUNT];
  struct { size_t pos; int name; int abs16; } fix[32]; int nfix;
};

static void a_init(struct asm_ *a, uint8_t *buf, uint32_t base) {
  memset(a, 0, sizeof *a);
  a->b = buf; a->base = base;
  for (int i = 0; i < L_COUNT; i++) a->label[i] = -1;
}
static void a_raw(struct asm_ *a, const uint8_t *d, size_t n) { memcpy(a->b + a->n, d, n); a->n += n; }
#define RAW(a, ...) do { const uint8_t t_[] = {__VA_ARGS__}; a_raw((a), t_, sizeof t_); } while (0)
static void a_label(struct asm_ *a, int name) { a->label[name] = (long)(a->base + a->n); }
static void a_br(struct asm_ *a, uint8_t op, int name) {
  a->b[a->n++] = op;
  a->fix[a->nfix].pos = a->n; a->fix[a->nfix].abs16 = 0; a->fix[a->nfix++].name = name;
  a->b[a->n++] = 0;
}
static void a_jmp(struct asm_ *a, int name) {   /* JMP abs inside the free bank */
  a->b[a->n++] = 0x4C;
  a->fix[a->nfix].pos = a->n; a->fix[a->nfix].abs16 = 1; a->fix[a->nfix++].name = name;
  a->b[a->n++] = 0; a->b[a->n++] = 0;
}
static void snes_long(uint32_t off, uint8_t out[3]) { out[0] = off & 0xFF; out[1] = (off >> 8) & 0xFF; out[2] = (uint8_t)(0xC0 + ((off >> 16) & 0x3F)); }
static void a_jump(struct asm_ *a, uint8_t op, uint32_t target) { uint8_t l[3]; snes_long(target, l); RAW(a, op, l[0], l[1], l[2]); }
static int a_done(struct asm_ *a) {
  for (int i = 0; i < a->nfix; i++) {
    if (a->fix[i].abs16) {
      if (a->label[a->fix[i].name] < 0) return -1;
      a->b[a->fix[i].pos] = (uint8_t)(a->label[a->fix[i].name] & 0xFF);
      a->b[a->fix[i].pos + 1] = (uint8_t)((a->label[a->fix[i].name] >> 8) & 0xFF);
      continue;
    }
    long rel = a->label[a->fix[i].name] - (long)(a->base + a->fix[i].pos + 1);
    if (a->label[a->fix[i].name] < 0 || rel < -128 || rel > 127) return -1;
    a->b[a->fix[i].pos] = (uint8_t)(rel & 0xFF);
  }
  return 0;
}

static void recharge_routine(struct asm_ *a, int mode /* 0 fast, 1 faster, 2 instant */) {
  RAW(a, 0xE0, 0x00, 0x06);
  a_br(a, 0xB0, L_VAN);
  if (mode == 2) RAW(a, 0xA9, 0x00);
  else if (mode == 1) RAW(a, 0x4A, 0x4A);
  RAW(a, 0x9D, 0xED, 0xE1, 0x6B);
  a_label(a, L_VAN);
  RAW(a, 0x18, 0x69, 0x32, 0x9D, 0xED, 0xE1, 0x6B);
}

/* mon: 0 vanilla, 1 strong, 2 heavy, 3 never; hero: 0 vanilla, 1 light, 2 never */
static void flinch_routine(struct asm_ *a, int mon, int hero, uint32_t done, uint32_t cont) {
  RAW(a, 0xE2, 0x20);
  RAW(a, 0xBD, 0xFB, 0xE1);
  a_br(a, 0x30, L_HERO);
  RAW(a, 0x29, 0xC1);
  a_br(a, 0xD0, L_PASS);
  if (mon == 0) a_br(a, 0x80, L_PASS);
  else {
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
    a_jump(a, 0x5C, done);
    a_label(a, L_STRONG16);
    RAW(a, 0xE2, 0x20);
    a_label(a, L_STRONG);
    RAW(a, 0x9E, 0x6B, 0xE1);
  }
  a_label(a, L_PASS);
  RAW(a, 0xC2, 0x20, 0xBD, 0x90, 0xE1);
  a_jump(a, 0x5C, cont);
  a_label(a, L_HERO);
  if (hero == 0) a_br(a, 0x80, L_PASS);
  else {
    RAW(a, 0xC2, 0x20);
    RAW(a, 0xBD, 0x82, 0xE1);
    a_br(a, 0xF0, L_PASS);
    if (hero == 1) {
      RAW(a, 0xBD, 0x84, 0xE1, 0x4A, 0x4A);
      RAW(a, 0xC5, 0x89);
      a_br(a, 0x90, L_PASS);
    }
    RAW(a, 0xE2, 0x20);
    a_jump(a, 0x5C, done);
  }
}

static void zero_routine(struct asm_ *a, uint32_t target) {
  RAW(a, 0x48);
  RAW(a, 0xBD, 0xFB, 0xE1);
  RAW(a, 0x30, 0x05);
  RAW(a, 0xA9, 0x00);
  RAW(a, 0x9D, 0x6B, 0xE1);
  RAW(a, 0x68);
  a_jump(a, 0x22, target);
  RAW(a, 0x6B);
}

static void critmul_routine(struct asm_ *a, int mult) {
  RAW(a, 0xC2, 0x20);
  if (mult == 3) RAW(a, 0xA5, 0x89, 0x0A, 0x18, 0x65, 0x89, 0x85, 0x89);
  else RAW(a, 0x06, 0x89, 0x06, 0x89);
  RAW(a, 0x6B);
}

static void wlevel_routine(struct asm_ *a, int mult) {
  RAW(a, 0xA9, 0x09, 0x38, 0xF9, 0x9C, 0xE1);
  RAW(a, 0x85, 0xA6);
  if (mult == 2) RAW(a, 0x0A);
  else if (mult == 3) RAW(a, 0x0A, 0x18, 0x65, 0xA6);
  RAW(a, 0x85, 0xA6, 0x6B);
}

static void crit_routine(struct asm_ *a, uint32_t target) {
  RAW(a, 0x48);
  RAW(a, 0xBD, 0xFB, 0xE1);
  RAW(a, 0x30, 0x09);
  RAW(a, 0x29, 0x40);
  RAW(a, 0xD0, 0x05);
  RAW(a, 0xA9, 0x01);
  RAW(a, 0x9D, 0x6B, 0xE1);
  RAW(a, 0x68);
  a_jump(a, 0x22, target);
  RAW(a, 0x6B);
}

#define RET_CHARGE_STORE 0xB3DFu
#define RET_CHARGE_UP 0xB3CBu

static void charge_routine(struct asm_ *a, int n, int nh /* -1 none */, int buff, int item /* -1 none */) {
  RAW(a, 0x1A);
  if (nh >= 0) {
    RAW(a, 0xC9, (uint8_t)nh);
    a_br(a, 0x90, L_STORE);
    RAW(a, 0xC9, (uint8_t)n);
    a_br(a, 0xB0, L_UP);
    RAW(a, 0x48);
    if (buff) {
      RAW(a, 0xBF, 0xB0, 0xE1, 0x7E);
      RAW(a, 0x29, 0x14, 0xC9, 0x14);
      a_br(a, 0xF0, L_YES);
    }
    if (item >= 0)
      for (int k = 0; k < 3; k++) {
        RAW(a, 0xBF, (uint8_t)(0xE0 + k), 0xE1, 0x7E);
        RAW(a, 0xC9, (uint8_t)item);
        a_br(a, 0xF0, L_YES);
      }
    a_br(a, 0x80, L_NO);
    a_label(a, L_YES);
    RAW(a, 0x68);
    a_br(a, 0x80, L_UP);
    a_label(a, L_NO);
    RAW(a, 0x68);
    a_label(a, L_STORE);
    a_jump(a, 0x5C, RET_CHARGE_STORE);
    a_label(a, L_UP);
    a_jump(a, 0x5C, RET_CHARGE_UP);
  } else {
    RAW(a, 0xC9, (uint8_t)n);
    a_br(a, 0x90, L_STORE);
    a_jump(a, 0x5C, RET_CHARGE_UP);
    a_label(a, L_STORE);
    a_jump(a, 0x5C, RET_CHARGE_STORE);
  }
}

static void levelup_routine(struct asm_ *a, int mp) {
  RAW(a, 0xBD, 0x84, 0xE1);
  RAW(a, 0x9D, 0x82, 0xE1);
  RAW(a, 0xE2, 0x20);
  if (mp) RAW(a, 0xBD, 0x87, 0xE1, 0x9D, 0x86, 0xE1);
  RAW(a, 0x6B);
}

static void deadguard_routine(struct asm_ *a) {
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

static void shopfill_routine(struct asm_ *a, unsigned fail) {
  RAW(a, 0xC2, 0x30);
  a_br(a, 0xF0, L_OK);
  RAW(a, 0x68);
  RAW(a, 0xE2, 0x20, 0x68, 0xC2, 0x30);
  a_jump(a, 0x5C, fail);
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
  a_jump(a, 0x22, 0x51);
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
static void magic_rate_routine(struct asm_ *a, int mult) {
  RAW(a, 0xA9, 0x09, 0x38, 0xE5, 0x9D);
  if (mult == 2) RAW(a, 0x0A);
  else if (mult == 3) RAW(a, 0x85, 0xAC, 0x0A, 0x18, 0x65, 0xAC);
  RAW(a, 0x85, 0xAC, 0x6B);
}

static void fade_routine(struct asm_ *a) {
  RAW(a, 0xA5, 0xE5, 0x4A);
  a_br(a, 0xD0, L_OK);
  RAW(a, 0x1A);
  a_label(a, L_OK);
  RAW(a, 0x85, 0xE4, 0x6B);
}

/* ------------------------------------------------------------------ site resolution */
static int hexval(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c - 'a' + 10; }

/* pattern: hex pairs, '?' pairs are wildcards, spaces ignored; counts matches inside the first 64 KiB */
static int locate(const uint8_t *body, size_t blen, const char *pat, long *first_off) {
  uint8_t val[48] = {0}, mask[48] = {0}; int nt = 0, found = 0;
  for (const char *p = pat; *p;) {
    if (*p == ' ') { p++; continue; }
    if (p[0] == '?') { val[nt] = 0; mask[nt++] = 0; p += 2; }
    else { val[nt] = (uint8_t)(hexval(p[0]) * 16 + hexval(p[1])); mask[nt++] = 1; p += 2; }
  }
  int f = 0;
  while (!mask[f]) f++;
  for (long o = 0; o + nt <= 0x10000 && (size_t)(o + nt) <= blen; o++) {
    int ok = 1;
    for (int k = f; k < nt && ok; k++) if (mask[k] && body[o + k] != val[k]) ok = 0;
    if (ok) { if (!found) *first_off = o; found++; }
  }
  return found;
}

struct sites {
  long flinch, crit, crit_target, crit_girl, crit_other, zero, zero_target, wlevel, halve, levelup, expgate, lvtail, msg, ring_phase, ring_step, ring_sub, ring_add, shop, shop_fail;
  uint8_t shop_old[5];
  int rando_mp, rando_magic;
  const char *e_shop;
  const char *e_flinch, *e_crit, *e_bonus, *e_zero;     /* non-NULL: the site could not be located (Python: a Conflict raised on access) */
};

static void resolve(const uint8_t *body, size_t blen, struct sites *r) {
  long o = 0;
  int n;
  memset(r, 0, sizeof *r);
  n = locate(body, blen, "C220BD90E1 89 78 C5 E220 D055 A901 85AC A90D 85A4 BDFBE1 8981", &o);
  if (n != 1) r->e_flinch = "hit-reaction routine (C0:4EBC signature) not found exactly once in this ROM: a hack rewrote it or the ROM is not SoM US";
  else if (body[o + 0x61] != 0x60) r->e_flinch = "hit-reaction routine has an unexpected length";
  else r->flinch = o;
  n = locate(body, blen, "22????C0 A685 A483 C220 0689 E220 60", &o);
  if (n != 1) r->e_crit = "critical notice site not found exactly once in this ROM: a hack rewrote it or the ROM is not SoM US";
  else { r->crit = o; r->crit_target = body[o + 1] | body[o + 2] << 8; }
  n = locate(body, blen, "B996E1 85A4 BD90E1 8910 F002 06A4 B9FBE1 1016 A582 C901 D009 A5A4 18 690A 85A4 8007 A5A4 18 6905 85A4", &o);
  if (n != 1) r->e_bonus = "critical chance routine not found exactly once in this ROM: a hack rewrote it or the ROM is not SoM US";
  else { r->crit_girl = o + 29; r->crit_other = o + 38; }
  n = locate(body, blen, "E220 22????C0 BDFBE1 1001 60", &o);
  if (n != 1) r->e_zero = "zero-damage reaction site not found exactly once in this ROM: a hack rewrote it or the ROM is not SoM US";
  else { r->zero = o + 2; r->zero_target = body[o + 3] | body[o + 4] << 8; }
  n = locate(body, blen, "C220 4689 64CC 64D8 E220 64CE", &o);
  r->halve = n == 1 ? o + 2 : 0x5383;
  n = locate(body, blen, "BD84E1 9D82E1 E220 A580 0940 8580", &o);
  r->levelup = n == 1 ? o : 0x44D3;
  n = locate(body, blen, "A683 BD00E0 C901 D007 BC82E1 F002 8007", &o);
  r->expgate = n == 1 ? o + 12 : 0x446B;
  n = locate(body, blen, "A90938F99CE185A6", &o);
  r->wlevel = n == 1 ? o : -1;
  n = locate(body, blen, "E220 A580 0940 8580", &o);
  r->lvtail = n == 1 ? o + 2 : 0x44DB;
  n = locate(body, blen, "E220 A960 9900FE C8 E69F E6A1", &o);
  r->msg = n == 1 ? o + 3 : 0x5CF4;
  n = locate(body, blen, "C91000 902A 18 69F6FF 801F C92400 B022 18 690A00 8014 C9FE00 B014 18 690A00 8009 C92400 900C 18 69F6FF", &o);
  r->ring_phase = n == 1 ? o : 0x6C1D;
  n = locate(body, blen, "A91200 8D2418 A94808 8D3218", &o);
  r->ring_step = n == 1 ? o : 0x6C7B;
  n = locate(body, blen, "B508 38 E91200 2900FC F021", &o);
  r->ring_sub = n == 1 ? o : 0x6CEA;
  n = locate(body, blen, "B508 18 691200 C90004 9010", &o);
  r->ring_add = n == 1 ? o : 0x6CFB;
  n = locate(body, blen, "20???? D0?? 20???? 20???? A2???? 4C", &o);
  if (n != 1) r->e_shop = "buy-shop purchase site not found exactly once";
  else {
    int rel = body[o + 4];
    r->shop = o + 3; r->shop_fail = o + 5 + (rel > 127 ? rel - 256 : rel);
    memcpy(r->shop_old, body + o + 3, 5);
  }
  n = locate(body, blen, "22?????? EAEA E220 A580 0940 8580", &o);
  if (n == 1) {
    static const uint8_t MPR[] = {0xBD,0x84,0xE1,0x9D,0x82,0xE1,0xE2,0x20,0xBD,0x87,0xE1,0x9D,0x86,0xE1,0xC2,0x20,0x6B};
    size_t t = (size_t)(body[o + 1] | body[o + 2] << 8 | (body[o + 3] & 0x3F) << 16);
    r->rando_mp = t + sizeof MPR <= blen && !memcmp(body + t, MPR, sizeof MPR);
  }
  if (blen > 0x104DF3 && body[0x104DEC] == 0x22 && body[0x104DF0] == 0xEA && body[0x104DF1] == 0xEA && body[0x104DF2] == 0xEA) {
    static const uint8_t MGR[] = {0xA9,0x09,0x38,0xE5,0x9D,0x0A,0x0A,0x85,0xAC,0x6B};
    size_t t = (size_t)(body[0x104DED] | body[0x104DEE] << 8 | (body[0x104DEF] & 0x3F) << 16);
    r->rando_magic = t + sizeof MGR <= blen && !memcmp(body + t, MGR, sizeof MGR);
  }
}

/* ------------------------------------------------------------------ plan */
struct rec {
  uint32_t off; uint16_t len; uint8_t opt; int routine;
  uint8_t before[8], after[8];
  const uint8_t *rafter;       /* routine records: the code blob; before is 0xFF * len */
};

struct plan {
  struct rec r[96]; int n;
  uint8_t code[CODE_MAX]; size_t code_n;
};

static void rec_small(struct plan *p, uint32_t off, const uint8_t *before, const uint8_t *after, size_t len, int opt) {
  struct rec *r = &p->r[p->n++];
  memset(r, 0, sizeof *r);
  r->off = off; r->len = (uint16_t)len; r->opt = (uint8_t)opt;
  memcpy(r->before, before, len); memcpy(r->after, after, len);
}
static void rec_jump(struct plan *p, uint32_t off, const uint8_t *before, size_t blen, uint8_t op, uint32_t target, int pad_nop, int opt) {
  uint8_t after[8], l[3];
  size_t n = 0;
  snes_long(target, l);
  after[n++] = op; after[n++] = l[0]; after[n++] = l[1]; after[n++] = l[2];
  for (int i = 0; i < pad_nop; i++) after[n++] = 0xEA;
  rec_small(p, off, before, after, blen, opt);
}

static int pidx(const char *v, const char *const *list, int n) { for (int i = 0; i < n; i++) if (!strcmp(v, list[i])) return i; return -1; }

#define EMIT(var, call) do { struct asm_ a_; a_init(&a_, p->code + p->code_n, FREE_BASE + (uint32_t)p->code_n); call; if (a_done(&a_)) { snprintf(err, en, "branch out of range"); return -1; } \
    var = a_.base; emitted[ne].opt = owner_; emitted[ne].pos = p->code_n; emitted[ne++].len = a_.n; p->code_n += a_.n; } while (0)

static int build_plan(struct plan *p, const struct sga_opts *o, const uint8_t *body, size_t blen, char *err, size_t en) {
  struct sites R;
  struct { int opt; size_t pos, len; } emitted[16];
  int ne = 0, owner_ = 0;
  static const char *const REC[] = {"vanilla", "fast", "faster", "instant"}, *const RUN[] = {"vanilla", "fast", "none"},
                          *const MF[] = {"vanilla", "strong", "heavy", "never"}, *const HF[] = {"vanilla", "light", "never"},
                          *const CS[] = {"vanilla", "fast", "faster", "instant"};
  static const uint8_t RECHARGE_V[] = {0x69, 0x32, 0x9D, 0xED, 0xE1}, FLINCH_V[] = {0xC2, 0x20, 0xBD, 0x90, 0xE1}, CHARGE_V[] = {0x1A, 0xC9, 0x2D, 0x90},
                       WLEVEL_V[] = {0xA9, 0x09, 0x38, 0xF9, 0x9C, 0xE1, 0x85, 0xA6}, CRITMUL_V[] = {0xC2, 0x20, 0x06, 0x89};
  static const uint32_t RECHARGE_SITES[] = {0xF942, 0x1CADE, 0x2C248};
  static const struct { uint32_t off; uint8_t van; } WALK[] = {{0xB71A, 0x02}, {0xB720, 0x82}, {0xB728, 0x02}, {0xB72E, 0x82}};
  int wr, rr, mf, hf, cm, wl, wsp, cs;
  uint32_t base;
  p->n = 0; p->code_n = 0;
  resolve(body, blen, &R);
#define NEED(e) do { if ((e)) { snprintf(err, en, "conflicts: %s", (e)); return -1; } } while (0)

  wr = pidx(o->weapon_recharge, REC, 4);
  if (wr < 0) { snprintf(err, en, "weapon_recharge: %s", o->weapon_recharge); return -1; }
  if (wr != 0) {
    owner_ = O_WR;
    EMIT(base, recharge_routine(&a_, wr == 1 ? 0 : wr == 2 ? 1 : 2));
    for (int i = 0; i < 3; i++) rec_jump(p, RECHARGE_SITES[i], RECHARGE_V, 5, 0x22, base, 1, O_WR);
  }
  rr = pidx(o->run_recharge, RUN, 3);
  if (rr < 0) { snprintf(err, en, "run_recharge: %s", o->run_recharge); return -1; }
  if (rr != 0) {
    owner_ = O_RR;
    EMIT(base, recharge_routine(&a_, rr == 2 ? 2 : 0));
    rec_jump(p, 0xB788, RECHARGE_V, 5, 0x22, base, 1, O_RR);
  }
  mf = pidx(o->monster_flinch, MF, 4);
  if (mf < 0) { snprintf(err, en, "monster_flinch: %s", o->monster_flinch); return -1; }
  hf = pidx(o->hero_flinch, HF, 3);
  if (hf < 0) { snprintf(err, en, "hero_flinch: %s", o->hero_flinch); return -1; }
  if (mf != 0 || hf != 0) {
    int owner = mf != 0 ? O_MF : O_HF;
    NEED(R.e_flinch);
    owner_ = owner;
    EMIT(base, flinch_routine(&a_, mf, hf, (uint32_t)R.flinch + 0x61, (uint32_t)R.flinch + 5));
    rec_jump(p, (uint32_t)R.flinch, FLINCH_V, 5, 0x5C, base, 1, owner);
    if (mf == 1 || mf == 2) {
      uint8_t cb4[4];
      NEED(R.e_crit);
      owner_ = O_MF;
      cb4[0] = 0x22; cb4[1] = R.crit_target & 255; cb4[2] = R.crit_target >> 8; cb4[3] = 0xC0;
      EMIT(base, crit_routine(&a_, (uint32_t)R.crit_target));
      { uint8_t after[4], l[3]; snes_long(base, l); after[0] = 0x22; memcpy(after + 1, l, 3); rec_small(p, (uint32_t)R.crit, cb4, after, 4, O_MF); }
      NEED(R.e_zero);
      cb4[1] = R.zero_target & 255; cb4[2] = R.zero_target >> 8;
      EMIT(base, zero_routine(&a_, (uint32_t)R.zero_target));
      { uint8_t after[4], l[3]; snes_long(base, l); after[0] = 0x22; memcpy(after + 1, l, 3); rec_small(p, (uint32_t)R.zero, cb4, after, 4, O_MF); }
    }
  }
  cm = o->crit_multiplier;
  if (cm != 2 && cm != 3 && cm != 4) { snprintf(err, en, "crit_multiplier must be 2, 3 or 4"); return -1; }
  if (cm != 2) {
    NEED(R.e_crit);
    owner_ = O_CM;
    EMIT(base, critmul_routine(&a_, cm));
    rec_jump(p, (uint32_t)R.crit + 8, CRITMUL_V, 4, 0x22, base, 0, O_CM);
  }
  wl = o->weapon_level_rate;
  if (wl < 1 || wl > 3) { snprintf(err, en, "weapon_level_rate must be 1, 2 or 3"); return -1; }
  if (wl != 1) {
    owner_ = O_WL;
    EMIT(base, wlevel_routine(&a_, wl));
    rec_jump(p, R.wlevel >= 0 ? (uint32_t)R.wlevel : 0x4358, WLEVEL_V, 8, 0x22, base, 4, O_WL);
  }
  wsp = o->walk_speed;
  if (wsp != 2 && wsp != 3 && wsp != 4) { snprintf(err, en, "walk_speed must be 2, 3 or 4"); return -1; }
  if (wsp != 2)
    for (int i = 0; i < 4; i++) { uint8_t b = WALK[i].van, a = (uint8_t)((b & 0x80) | wsp); rec_small(p, WALK[i].off, &b, &a, 1, O_WS); }
  {   /* charge_params */
    int f = o->charge_frames_per_stage, reload_, acts, haste = -1, item = o->charge_bonus_item_id;
    if (f == -1) {
      cs = pidx(o->charge_speed, CS, 4);
      if (cs < 0) { snprintf(err, en, "charge_speed: %s", o->charge_speed); return -1; }
      f = cs == 0 ? 90 : cs == 1 ? 45 : cs == 2 ? 22 : 1;
    }
    if (f < 1 || f > 90) { snprintf(err, en, "charge_frames_per_stage must be 1..90"); return -1; }
    reload_ = f > 45;
    acts = reload_ == 0 ? f : (f + 1) / 2;
    if (o->charge_speed_up_bonus || item >= 0) { haste = acts / 2 > 1 ? acts / 2 : 1; if (haste >= acts) haste = -1; }
    if (item > 254) { snprintf(err, en, "charge_bonus_item_id must be 0..254"); return -1; }
    if (!(reload_ == 1 && acts == 45 && haste == -1)) {
      owner_ = O_CS;
      EMIT(base, charge_routine(&a_, acts, haste, o->charge_speed_up_bonus != 0, item));
      rec_jump(p, 0xB3C6, CHARGE_V, 4, 0x5C, base, 0, O_CS);
      if (reload_ == 0) { uint8_t b = 1, a = 0; rec_small(p, 0xB3A7 + 1, &b, &a, 1, O_CS); }
    }
  }
  NEED(R.e_bonus);
  {
    int v[2] = {o->hero_crit_bonus_girl, o->hero_crit_bonus_others};
    long off[2] = {R.crit_girl, R.crit_other};
    uint8_t van[2] = {0x0A, 0x05};
    for (int k = 0; k < 2; k++) {
      if (v[k] < 0) continue;
      if (v[k] > 120) { snprintf(err, en, "%s must be 0..120", OPT_NAME[O_GIRL + k]); return -1; }
      if (v[k] != van[k]) { uint8_t a = (uint8_t)v[k]; rec_small(p, (uint32_t)off[k], &van[k], &a, 1, O_GIRL + k); }
    }
  }
  {   /* quality-of-life group (T11) */
    static const struct { uint32_t off; uint8_t van; } CHEST[] = {{0x115FB8, 0x83}, {0x11ED01, 0x8F}, {0x11EC84, 0x8F}};
    static const uint8_t FADE_V[] = {0xA5, 0xE5, 0x85, 0xE4}, ITEM_V[] = {0xC9, 0x9F},
                         LEVELUP_V[] = {0xBD, 0x84, 0xE1, 0x9D, 0x82, 0xE1, 0xE2, 0x20}, GATE_V[] = {0xF0, 0x02}, MAGIC_V[] = {0xA9, 0x09, 0x38, 0xE5, 0x9D, 0x85, 0xAC},
                         NOPS[] = {0xEA, 0xEA, 0xEA, 0xEA};
    static const uint32_t DZ_OFF[] = {0xD903, 0xD90A, 0xD952, 0xD959};
    static const uint8_t DZ_VAN[] = {0x38, 0xC8, 0x48, 0xB0}, DZ_TIGHT[] = {0x50, 0xB0, 0x50, 0xA0}, DZ_TIGHTER[] = {0x60, 0xA0, 0x60, 0x90};
    int bools[] = {o->qol_chest_fast, o->qol_fast_transitions, o->qol_fast_fades, o->qol_levelup_mp_restore, o->qol_dead_exp, o->qol_unready_damage, o->qol_shop_fill};
    int mp = o->qol_levelup_mp_restore != 0, dead = o->qol_dead_exp != 0, ml = o->qol_magic_level_rate, dz;
    for (unsigned i = 0; i < sizeof bools / sizeof bools[0]; i++) if (bools[i] != 0 && bools[i] != 1) { snprintf(err, en, "qol option must be true or false"); return -1; }
    if (o->qol_chest_fast)
      for (int i = 0; i < 3; i++) { uint8_t a = 0xFF, b = CHEST[i].van; rec_small(p, CHEST[i].off, &b, &a, 1, O_CHEST); }
    if (o->qol_unready_damage) { static const uint8_t HV[] = {0x46, 0x89}; rec_small(p, (uint32_t)R.halve, HV, NOPS, 2, O_UNREADY); }
    if (o->qol_fast_transitions) { uint8_t b = 0xE6, a = 0xE7; rec_small(p, 0x8B76, &b, &a, 1, O_TRANS); }
    if (o->qol_fast_fades) {
      owner_ = O_FADES;
      EMIT(base, fade_routine(&a_));
      rec_jump(p, 0x8B71, FADE_V, 4, 0x22, base, 0, O_FADES);
    }
    if (o->qol_item_limit != 4 && o->qol_item_limit != 7) { snprintf(err, en, "qol_item_limit must be 4 or 7"); return -1; }
    if (o->qol_item_limit == 7) rec_small(p, 0x6559, ITEM_V, NOPS, 2, O_ITEMS);
    if (mp && !R.rando_mp) {
      owner_ = O_LVMP;
      EMIT(base, levelup_routine(&a_, 1));
      rec_jump(p, (uint32_t)R.levelup, LEVELUP_V, 8, 0x22, base, 4, O_LVMP);
    }
    if (dead) {
      static const uint8_t TAIL_V[] = {0xA5, 0x80, 0x09, 0x40};
      owner_ = O_DEADEXP;
      rec_small(p, (uint32_t)R.expgate, GATE_V, NOPS, 2, O_DEADEXP);
      EMIT(base, deadguard_routine(&a_));
      rec_jump(p, (uint32_t)R.lvtail, TAIL_V, 4, 0x22, base, 0, O_DEADEXP);
    }
    if (ml < 1 || ml > 3) { snprintf(err, en, "qol_magic_level_rate must be 1, 2 or 3"); return -1; }
    if (ml != 1 && !R.rando_magic) {
      owner_ = O_MAGIC;
      EMIT(base, magic_rate_routine(&a_, ml));
      rec_jump(p, 0x104DEC, MAGIC_V, 7, 0x22, base, 3, O_MAGIC);
    }
    if (o->qol_ring_speed != 1 && o->qol_ring_speed != 2) { snprintf(err, en, "qol_ring_speed must be 1 or 2"); return -1; }
    if (o->qol_ring_speed == 2) {
      uint8_t b12 = 0x12, a24 = 0x24, bp = 0x0A, ap = 0x14, bn = 0xF6, an = 0xEC;
      rec_small(p, (uint32_t)R.ring_step + 1, &b12, &a24, 1, O_RING);
      rec_small(p, (uint32_t)R.ring_sub + 4, &b12, &a24, 1, O_RING);
      rec_small(p, (uint32_t)R.ring_add + 4, &b12, &a24, 1, O_RING);
      rec_small(p, (uint32_t)R.ring_phase + 0x07, &bn, &an, 1, O_RING);
      rec_small(p, (uint32_t)R.ring_phase + 0x12, &bp, &ap, 1, O_RING);
      rec_small(p, (uint32_t)R.ring_phase + 0x1D, &bp, &ap, 1, O_RING);
      rec_small(p, (uint32_t)R.ring_phase + 0x28, &bn, &an, 1, O_RING);
    }
    if (o->qol_message_speed != 1 && o->qol_message_speed != 2 && o->qol_message_speed != 4) { snprintf(err, en, "qol_message_speed must be 1, 2 or 4"); return -1; }
    if (o->qol_message_speed != 1) { uint8_t b60 = 0x60, a1 = (uint8_t)(0x60 / o->qol_message_speed); rec_small(p, (uint32_t)R.msg, &b60, &a1, 1, O_MSG); }
    if (o->qol_shop_fill != 0 && o->qol_shop_fill != 1) { snprintf(err, en, "qol_shop_fill must be true or false"); return -1; }
    if (o->qol_shop_fill) {
      NEED(R.e_shop);
      owner_ = O_SHOP;
      EMIT(base, shopfill_routine(&a_, (uint32_t)R.shop_fail));
      rec_jump(p, (uint32_t)R.shop, R.shop_old, 5, 0x22, base, 1, O_SHOP);
    }
    dz = pidx(o->qol_scroll_deadzone, (const char *const[]){"vanilla", "tight", "tighter"}, 3);
    if (dz < 0) { snprintf(err, en, "qol_scroll_deadzone must be vanilla, tight or tighter"); return -1; }
    if (dz != 0)
      for (int i = 0; i < 4; i++) rec_small(p, DZ_OFF[i], &DZ_VAN[i], dz == 1 ? &DZ_TIGHT[i] : &DZ_TIGHTER[i], 1, O_DEADZONE);
  }
  if (p->code_n > FREE_LEN) { snprintf(err, en, "free area overflow"); return -1; }
  for (int i = 0; i < ne; i++) {
    struct rec *r = &p->r[p->n++];
    uint32_t off = FREE_BASE;
    for (int j = 0; j < i; j++) off += (uint32_t)emitted[j].len;
    memset(r, 0, sizeof *r);
    r->off = off; r->len = (uint16_t)emitted[i].len; r->opt = (uint8_t)emitted[i].opt; r->routine = 1; r->rafter = p->code + emitted[i].pos;
  }
  return 0;
}

/* ------------------------------------------------------------------ apply */
static int checksum_ok(const uint8_t *body, size_t n) {
  unsigned comp = body[0xFFDC] | body[0xFFDD] << 8, chk = body[0xFFDE] | body[0xFFDF] << 8;
  unsigned long s = 0;
  for (size_t i = 0; i < n; i++) s += body[i];
  return (comp ^ chk) == 0xFFFF && chk == (s & 0xFFFF);
}
static void set_checksum(uint8_t *body, size_t n) {
  unsigned long s = 0;
  unsigned c;
  body[0xFFDC] = 0xFF; body[0xFFDD] = 0xFF; body[0xFFDE] = 0; body[0xFFDF] = 0;
  for (size_t i = 0; i < n; i++) s += body[i];
  s &= 0xFFFF;
  body[0xFFDE] = (uint8_t)(s & 0xFF); body[0xFFDF] = (uint8_t)(s >> 8);
  c = (unsigned)(s ^ 0xFFFF);
  body[0xFFDC] = (uint8_t)(c & 0xFF); body[0xFFDD] = (uint8_t)(c >> 8);
}

long sga_apply(uint8_t *rom, size_t len, const struct sga_opts *o, char *err, size_t en, char *skipped, size_t sn) {
  static struct plan P;
  size_t hdr = len % 0x8000 == 0 ? 0 : len % 0x8000 == 512 ? 512 : (size_t)-1, blen;
  uint8_t *body;
  int bad[O_COUNT] = {0}, st[96], was_valid, changed = 0;
  long bytes = 0;
  err[0] = 0;
  if (skipped && sn) skipped[0] = 0;
  if (hdr == (size_t)-1) { snprintf(err, en, "unexpected ROM size %zu", len); return -1; }
  body = rom + hdr; blen = len - hdr;
  if (blen <= FREE_BASE + FREE_LEN) { snprintf(err, en, "ROM too small"); return -1; }
  if (build_plan(&P, o, body, blen, err, en)) return -1;
  was_valid = checksum_ok(body, blen);
  for (int i = 0; i < P.n; i++) {
    const struct rec *r = &P.r[i];
    uint8_t before[8];
    const uint8_t *after = r->routine ? r->rafter : r->after;
    const uint8_t *cur = body + r->off;
    if (r->routine) memset(before, 0xFF, sizeof before);
    else memcpy(before, r->before, sizeof before);
    {
      int eq_b = 1;
      if (r->routine) { for (int k = 0; k < r->len; k++) if (cur[k] != 0xFF) { eq_b = 0; break; } }
      else eq_b = !memcmp(cur, before, r->len);
      st[i] = eq_b ? 0 : !memcmp(cur, after, r->len) ? 1 : 2;
    }
    if (st[i] == 2) bad[r->opt] = 1;
  }
  for (int i = 0; i < P.n; i++) {
    const struct rec *r = &P.r[i];
    if (bad[r->opt]) continue;
    memcpy(body + r->off, r->routine ? r->rafter : r->after, r->len);
    if (st[i] == 0) { changed = 1; bytes += r->len; }
  }
  if (skipped && sn) {
    size_t k = 0;
    for (int i = 0; i < O_COUNT; i++) if (bad[i]) k += (size_t)snprintf(skipped + k, k < sn ? sn - k : 0, "%s%s", k ? "," : "", OPT_NAME[i]);
  }
  if (changed && (!strcmp(o->fix_checksum, "always") || (!strcmp(o->fix_checksum, "auto") && was_valid))) set_checksum(body, blen);
  return bytes;
}
