/* Shared button-binding engine, see som_bind.h. */
#include "som_bind.h"

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ input tables */
static const struct som_in_info INFO[SOM_IN_COUNT] = {
  {SOM_IN_NONE, "none"},
  {SOM_IN_A, "A (south)"}, {SOM_IN_B, "B (east)"}, {SOM_IN_X, "X (west)"}, {SOM_IN_Y, "Y (north)"},
  {SOM_IN_L1, "L1 (left shoulder)"}, {SOM_IN_R1, "R1 (right shoulder)"}, {SOM_IN_L2, "L2 (left trigger)"}, {SOM_IN_R2, "R2 (right trigger)"},
  {SOM_IN_C, "C (extra button)"}, {SOM_IN_Z, "Z (extra button)"},
  {SOM_IN_THUMBL, "L3 (left stick click)"}, {SOM_IN_THUMBR, "R3 (right stick click)"}, {SOM_IN_MODE, "MODE"},
  {SOM_IN_SELECT, "SELECT"}, {SOM_IN_START, "START"},
  {SOM_IN_DUP, "D-pad up"}, {SOM_IN_DDOWN, "D-pad down"}, {SOM_IN_DLEFT, "D-pad left"}, {SOM_IN_DRIGHT, "D-pad right"},
  {SOM_IN_LS_UP, "left stick up"}, {SOM_IN_LS_DOWN, "left stick down"}, {SOM_IN_LS_LEFT, "left stick left"}, {SOM_IN_LS_RIGHT, "left stick right"},
  {SOM_IN_RS_UP, "right stick up"}, {SOM_IN_RS_DOWN, "right stick down"}, {SOM_IN_RS_LEFT, "right stick left"}, {SOM_IN_RS_RIGHT, "right stick right"},
};
static const char *const LABEL_THOR[SOM_IN_COUNT] = {
  "", "A", "B", "X", "Y", "L1", "R1", "L2", "R2", "C", "Z", "L3", "R3", "MODE", "SEL", "STRT", "UP", "DN", "LT", "RT",
  "LSU", "LSD", "LSL", "LSR", "RSU", "RSD", "RSL", "RSR"};
static const char *const LABEL_3DS[SOM_IN_COUNT] = {
  "", "A", "B", "X", "Y", "L", "R", "ZL", "ZR", "C", "Z", "L3", "R3", "MODE", "SEL", "STRT", "UP", "DN", "LT", "RT",
  "CPU", "CPD", "CPL", "CPR", "CSU", "CSD", "CSL", "CSR"};
static const char *const NAME_3DS[SOM_IN_COUNT] = {
  "none", "A", "B", "X", "Y", "L", "R", "ZL (New 3DS)", "ZR (New 3DS)", "C", "Z", "L3", "R3", "MODE", "SELECT", "START",
  "D-pad up", "D-pad down", "D-pad left", "D-pad right", "circle pad up", "circle pad down", "circle pad left", "circle pad right",
  "C-stick up (New 3DS)", "C-stick down (New 3DS)", "C-stick left (New 3DS)", "C-stick right (New 3DS)"};

const struct som_in_info *som_bind_input_info(int in) { return in > 0 && in < SOM_IN_COUNT ? &INFO[in] : NULL; }

uint32_t som_bind_platform_inputs(int plat) {
  uint32_t all = (1u << (SOM_IN_COUNT - 1)) - 1;
  uint32_t none_3ds = SOM_IN_BIT(SOM_IN_C) | SOM_IN_BIT(SOM_IN_Z) | SOM_IN_BIT(SOM_IN_THUMBL) | SOM_IN_BIT(SOM_IN_THUMBR) | SOM_IN_BIT(SOM_IN_MODE);
  uint32_t no_new = SOM_IN_BIT(SOM_IN_L2) | SOM_IN_BIT(SOM_IN_R2) | SOM_IN_BIT(SOM_IN_RS_UP) | SOM_IN_BIT(SOM_IN_RS_DOWN) |
                    SOM_IN_BIT(SOM_IN_RS_LEFT) | SOM_IN_BIT(SOM_IN_RS_RIGHT);
  if (plat == SOM_PLAT_3DS_NEW) return all & ~none_3ds;
  if (plat == SOM_PLAT_3DS_OLD) return all & ~none_3ds & ~no_new;
  return all;
}

const char *som_bind_label(int plat, int in) {
  if (in <= 0 || in >= SOM_IN_COUNT) return "";
  return plat == SOM_PLAT_THOR ? LABEL_THOR[in] : LABEL_3DS[in];
}
const char *som_bind_name(int plat, int in) {
  if (in <= 0 || in >= SOM_IN_COUNT) return "none";
  return plat == SOM_PLAT_THOR ? INFO[in].name : NAME_3DS[in];
}

/* Game mapping.  Thor: south = SNES B, west = Y, east = A, north = X (the positions of the SNES layout are not kept, as in the
 * app since the first release).  3DS: the four face buttons keep their SNES positions. */
int som_bind_game_bit(int plat, int in) {
  switch (in) {
  case SOM_IN_A: return plat == SOM_PLAT_THOR ? SOM_SB_B : SOM_SB_A;
  case SOM_IN_B: return plat == SOM_PLAT_THOR ? SOM_SB_A : SOM_SB_B;
  case SOM_IN_X: return plat == SOM_PLAT_THOR ? SOM_SB_Y : SOM_SB_X;
  case SOM_IN_Y: return plat == SOM_PLAT_THOR ? SOM_SB_X : SOM_SB_Y;
  case SOM_IN_L1: return SOM_SB_L;
  case SOM_IN_R1: return SOM_SB_R;
  case SOM_IN_SELECT: return SOM_SB_SELECT;
  case SOM_IN_START: return SOM_SB_START;
  case SOM_IN_DUP: return SOM_SB_UP;
  case SOM_IN_DDOWN: return SOM_SB_DOWN;
  case SOM_IN_DLEFT: return SOM_SB_LEFT;
  case SOM_IN_DRIGHT: return SOM_SB_RIGHT;
  /* the left stick / circle pad moves the hero like the D-pad until a binding takes the direction */
  case SOM_IN_LS_UP: return SOM_SB_UP;
  case SOM_IN_LS_DOWN: return SOM_SB_DOWN;
  case SOM_IN_LS_LEFT: return SOM_SB_LEFT;
  case SOM_IN_LS_RIGHT: return SOM_SB_RIGHT;
  default: return -1;
  }
}
uint16_t som_bind_game_mask(int plat, uint32_t held) {
  uint16_t m = 0;
  for (int i = 1; i < SOM_IN_COUNT; i++) {
    int b = (held & SOM_IN_BIT(i)) ? som_bind_game_bit(plat, i) : -1;
    if (b >= 0) m |= (uint16_t)(1u << b);
  }
  return m;
}

/* Buttons the game reads, per mode (docs/companion-bindings-20261017.md, "Used buttons").
 * FIELD and RING were measured: the state is saved, run for 90 frames with and without each button held for 12 frames, and the
 * WRAM compared (the emulator is deterministic, the no-input replay is identical).  The native menu pages, name entry and the
 * controller layout page were read from src/rom (pressed-edge consumers).  Event, shop, transition and boot were not reached
 * on the device, so every button but L and R is conservatively treated as used there. */
#define M(b) (1u << (b))
uint16_t som_bind_used_mask(int mode) {
  const uint16_t all_but_lr = M(SOM_SB_B) | M(SOM_SB_Y) | M(SOM_SB_SELECT) | M(SOM_SB_START) | M(SOM_SB_UP) | M(SOM_SB_DOWN) |
                              M(SOM_SB_LEFT) | M(SOM_SB_RIGHT) | M(SOM_SB_A) | M(SOM_SB_X);
  switch (mode) {
  case SOM_BM_RING: return M(SOM_SB_Y) | M(SOM_SB_X) | M(SOM_SB_UP) | M(SOM_SB_DOWN) | M(SOM_SB_LEFT) | M(SOM_SB_RIGHT);
  default: return all_but_lr;
  }
}
const char *som_bind_mode_name(int mode) {
  static const char *const n[SOM_BM_COUNT] = {"field", "ring", "menu", "event", "shop", "transition", "boot"};
  return mode >= 0 && mode < SOM_BM_COUNT ? n[mode] : "?";
}
int som_bind_canonical(int plat, int in) {
  int b = som_bind_game_bit(plat, in);
  if (in >= SOM_IN_LS_UP && in <= SOM_IN_LS_RIGHT) return 0;   /* a stick direction is free: binding it needs no confirmation */
  return b >= 0 && (som_bind_used_mask(SOM_BM_FIELD) >> b & 1);
}
int som_bind_swallow_ok(int plat, int in, int mode) { return !som_bind_canonical(plat, in) || mode == SOM_BM_FIELD; }

/* ------------------------------------------------------------------ names */
static const char *const POLICY[SOM_POL_COUNT] = {"ASK", "NEAREST ENEMY", "ALL ENEMIES", "LOWEST ALLY", "WHOLE PARTY", "SELF"};
static const char *const WEAPON[8] = {"GLOVE", "SWORD", "AXE", "SPEAR", "WHIP", "BOW", "BOOMERANG", "JAVELIN"};
static const char *const ITEMS[11] = {"CANDY", "CHOCOLATE", "ROYAL JAM", "WALNUT", "HERB", "CUP OF WISHES", "MAGIC ROPE", "FLAMMIE DRUM",
                                      "MOOGLE BELT", "MIDGE MALLET", "BARREL"};
static const char *const SPIRIT[8] = {"GNOME", "UNDINE", "SALAMANDO", "SYLPHID", "LUNA", "DRYAD", "SHADE", "LUMINA"};
static const char *const HERO[3] = {"BOY", "GIRL", "SPRITE"};
const char *som_bind_policy_name(int p) { return p >= 0 && p < SOM_POL_COUNT ? POLICY[p] : "?"; }
const char *som_bind_weapon_name(int t) { return WEAPON[t & 7]; }
const char *som_bind_item_name(int id) { return id >= 0 && id < 11 ? ITEMS[id] : "ITEM"; }
const char *som_bind_spirit_name(int i) { return SPIRIT[i & 7]; }
const char *som_bind_hero_name(int h) { return HERO[(h % 3 + 3) % 3]; }

/* Spirit position 6 is Lumina for the girl and Shade for the sprite; 7 is the girl's Lumina in the macro numbering. */
static const char *spirit_of(int hero, int sp) {
  if (sp >= 6) return hero == 1 ? "LUMINA" : "SHADE";
  return som_bind_spirit_name(sp);
}
void som_bind_action_label(const struct som_bind *b, char *out, size_t n) {
  switch (b->kind) {
  case SOM_BK_CAST: snprintf(out, n, "CAST %s %s %d [%s]", som_bind_hero_name(b->a), spirit_of(b->a, b->b), b->c + 1, som_bind_policy_name(b->policy)); break;
  case SOM_BK_EQUIP: snprintf(out, n, "EQUIP %s", som_bind_weapon_name(b->a)); break;
  case SOM_BK_CYCLE_WEAPON: snprintf(out, n, "%s WEAPON", b->a > 0 ? "NEXT" : "PREV"); break;
  case SOM_BK_ITEM: snprintf(out, n, "USE %s [%s]", som_bind_item_name(b->a), som_bind_policy_name(b->policy)); break;
  case SOM_BK_SELECT_SPIRIT: snprintf(out, n, "SPIRIT %s (%s)", spirit_of(b->a, b->b), som_bind_hero_name(b->a)); break;
  case SOM_BK_CYCLE_SPIRIT: snprintf(out, n, "%s SPIRIT", b->a > 0 ? "NEXT" : "PREV"); break;
  default: snprintf(out, n, "-"); break;
  }
}
void som_bind_trigger_label(int plat, const struct som_bind *b, char *out, size_t n) {
  snprintf(out, n, "%s%s%s%s", b->mod ? som_bind_label(plat, b->mod) : "", b->mod ? "+" : "", som_bind_label(plat, b->key),
           b->press == SOM_PRESS_LONG ? " (LONG)" : "");
}
void som_bind_badge(int plat, const struct som_bind *b, char *out, size_t n) {
  char t[24];
  const char *k = som_bind_label(plat, b->key), *m = b->mod ? som_bind_label(plat, b->mod) : "";
  if (b->mod) snprintf(t, sizeof t, "%.1s+%s", m, k);
  else snprintf(t, sizeof t, "%s", k);
  if (b->press == SOM_PRESS_LONG && strlen(t) < 4) strcat(t, ".");
  snprintf(out, n, "%.4s", t);
}

/* ------------------------------------------------------------------ store */
void som_bind_init(struct som_bind_ctx *c, int plat) {
  memset(c, 0, sizeof *c);
  c->plat = plat;
  memset(c->short_idx, -1, sizeof c->short_idx);
  memset(c->long_idx, -1, sizeof c->long_idx);
}
static void runtime_reset(struct som_bind_ctx *c) {
  c->prev = c->swallowed = c->chord_used = c->long_fired = 0;
  memset(c->short_idx, -1, sizeof c->short_idx);
  memset(c->long_idx, -1, sizeof c->long_idx);
}
int som_bind_find(const struct som_bind_ctx *c, int mod, int key, int press) {
  for (int i = 0; i < c->n; i++) if (c->b[i].mod == mod && c->b[i].key == key && c->b[i].press == press) return i;
  return -1;
}
int som_bind_find_entry(const struct som_bind_ctx *c, int kind, int a, int b, int cc) {
  for (int i = 0; i < c->n; i++) {
    const struct som_bind *x = &c->b[i];
    if (x->kind != kind) continue;
    if (kind == SOM_BK_EQUIP || kind == SOM_BK_ITEM) { if (x->a == a) return i; }
    else if (kind == SOM_BK_CAST) { if (x->a == a && x->b == b && x->c == cc) return i; }
    else if (kind == SOM_BK_SELECT_SPIRIT) { if (x->a == a && x->b == b) return i; }
  }
  return -1;
}
int som_bind_add(struct som_bind_ctx *c, const struct som_bind *b) {
  if (b->key <= 0 || b->key >= SOM_IN_COUNT || b->mod >= SOM_IN_COUNT || b->mod == b->key || b->kind <= 0 || b->kind >= SOM_BK_COUNT) return -1;
  int i = som_bind_find(c, b->mod, b->key, b->press);
  if (i < 0) {
    if (c->n >= SOM_BIND_MAX) return -1;
    i = c->n++;
  }
  c->b[i] = *b;
  runtime_reset(c);
  return i;
}
void som_bind_remove(struct som_bind_ctx *c, int idx) {
  if (idx < 0 || idx >= c->n) return;
  memmove(&c->b[idx], &c->b[idx + 1], (size_t)(c->n - idx - 1) * sizeof c->b[0]);
  c->n--;
  runtime_reset(c);
}
void som_bind_clear(struct som_bind_ctx *c) { c->n = 0; runtime_reset(c); }

void som_bind_defaults(struct som_bind_ctx *c) {
  uint32_t have = som_bind_platform_inputs(c->plat);
  struct som_bind d;
  c->n = 0;
  memset(&d, 0, sizeof d);
  d.kind = SOM_BK_CYCLE_WEAPON; d.key = SOM_IN_L1; d.a = -1; som_bind_add(c, &d);
  d.key = SOM_IN_R1; d.a = 1; som_bind_add(c, &d);
  if (have & SOM_IN_BIT(SOM_IN_L2)) { d.kind = SOM_BK_CYCLE_SPIRIT; d.key = SOM_IN_L2; d.a = -1; som_bind_add(c, &d); }
  if (have & SOM_IN_BIT(SOM_IN_R2)) { d.kind = SOM_BK_CYCLE_SPIRIT; d.key = SOM_IN_R2; d.a = 1; som_bind_add(c, &d); }
}

size_t som_bind_serialize(const struct som_bind_ctx *c, char *out, size_t n) {
  size_t len = 0;
  if (n) out[0] = 0;
  for (int i = 0; i < c->n; i++) {
    const struct som_bind *b = &c->b[i];
    int w = snprintf(out + len, n > len ? n - len : 0, "%d %d %d %d %d %d %d %d\n", b->mod, b->key, b->press, b->kind, b->a, b->b, b->c, b->policy);
    if (w < 0 || len + (size_t)w >= n) break;
    len += (size_t)w;
  }
  return len;
}
int som_bind_parse(struct som_bind_ctx *c, const char *text) {
  struct som_bind_ctx tmp;
  som_bind_init(&tmp, c->plat);
  const char *p = text;
  while (*p) {
    int v[8], used = 0;
    if (*p == '\n' || *p == ' ' || *p == '\r' || *p == '\t') { p++; continue; }   /* the prefs XML adds trailing blanks */
    if (sscanf(p, "%d %d %d %d %d %d %d %d%n", &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7], &used) != 8) return -1;
    struct som_bind b = {(uint8_t)v[0], (uint8_t)v[1], (uint8_t)v[2], (uint8_t)v[3], (int16_t)v[4], (int16_t)v[5], (int16_t)v[6], (uint8_t)v[7]};
    if (b.press > 1 || b.policy >= SOM_POL_COUNT || som_bind_add(&tmp, &b) < 0) return -1;
    p += used;
  }
  c->n = tmp.n;
  memcpy(c->b, tmp.b, sizeof c->b);
  runtime_reset(c);
  return c->n;
}

/* ------------------------------------------------------------------ engine */
static void fire(struct som_bind_out *o, int idx) { if (idx >= 0 && o->nfire < SOM_BIND_FIRES) o->fire[o->nfire++] = idx; }
static int is_modifier(const struct som_bind_ctx *c, int in) {
  for (int i = 0; i < c->n; i++) if (c->b[i].mod == in) return 1;
  return 0;
}
static void arm(struct som_bind_ctx *c, struct som_bind_out *o, int in, int s, int l, int defer) {
  if (l >= 0) c->long_idx[in] = (int8_t)l;
  if (s >= 0) { if (defer) c->short_idx[in] = (int8_t)s; else fire(o, s); }
}

static void press_one(struct som_bind_ctx *c, struct som_bind_out *o, int in, uint32_t held, uint32_t now, int mode) {
  uint32_t bit = SOM_IN_BIT(in);
  c->down_ms[in] = now; c->short_idx[in] = c->long_idx[in] = -1; c->long_fired &= ~bit;
  if (!som_bind_swallow_ok(c->plat, in, mode)) return;   /* a game button outside free play goes to the game */
  for (int m = 1; m < SOM_IN_COUNT; m++) {
    if (m == in || !(held & SOM_IN_BIT(m))) continue;
    int s = som_bind_find(c, m, in, SOM_PRESS_SHORT), l = som_bind_find(c, m, in, SOM_PRESS_LONG);
    if (s < 0 && l < 0) continue;
    c->chord_used |= SOM_IN_BIT(m);
    c->swallowed |= bit;
    arm(c, o, in, s, l, l >= 0);
    return;
  }
  int s = som_bind_find(c, 0, in, SOM_PRESS_SHORT), l = som_bind_find(c, 0, in, SOM_PRESS_LONG);
  if (s < 0 && l < 0) return;
  c->swallowed |= bit;
  arm(c, o, in, s, l, l >= 0 || is_modifier(c, in));
}
static void release_one(struct som_bind_ctx *c, struct som_bind_out *o, int in) {
  uint32_t bit = SOM_IN_BIT(in);
  if ((c->swallowed & bit) && c->short_idx[in] >= 0 && !(c->chord_used & bit) && !(c->long_fired & bit)) fire(o, c->short_idx[in]);
  c->swallowed &= ~bit; c->chord_used &= ~bit; c->long_fired &= ~bit;
  c->short_idx[in] = c->long_idx[in] = -1;
}

static int lowest(uint32_t m) { for (int i = 1; i < SOM_IN_COUNT; i++) if (m & SOM_IN_BIT(i)) return i; return 0; }

static void cap_end(struct som_bind_ctx *c, uint32_t held) {
  c->cap_on = 0; c->cap_phase = 0;
  c->swallowed |= held;   /* what is still down stays hidden from the game until it is released */
}
static void cap_commit(struct som_bind_ctx *c, struct som_bind_out *o, uint32_t held, int mod, int key, int press) {
  struct som_bind t;
  memset(&t, 0, sizeof t);
  t.mod = (uint8_t)mod; t.key = (uint8_t)key; t.press = (uint8_t)press;
  o->trigger = t;
  int canon = som_bind_canonical(c->plat, key) || (mod && som_bind_canonical(c->plat, mod));
  if (canon && !(c->cap_conf_valid && c->cap_conf.mod == t.mod && c->cap_conf.key == t.key && c->cap_conf.press == t.press)) {
    c->cap_conf = t; c->cap_conf_valid = 1; c->cap_phase = 0;
    o->event = SOM_BE_CAPTURE_CONFIRM;
    return;
  }
  o->event = SOM_BE_CAPTURE_DONE;
  cap_end(c, held);
}
static void capture_update(struct som_bind_ctx *c, struct som_bind_out *o, uint32_t held, uint32_t now) {
  uint32_t pressed = held & ~c->prev, released = c->prev & ~held;
  if (now - c->cap_start >= SOM_BIND_CAPTURE_MS) { o->event = SOM_BE_CAPTURE_CANCEL; cap_end(c, held); return; }
  if (c->cap_phase == 0 && pressed) {
    c->cap_first = lowest(pressed); c->cap_t1 = now; c->cap_phase = 1;
    pressed &= ~SOM_IN_BIT(c->cap_first);
  }
  if (c->cap_phase == 1) {
    if (pressed) { c->cap_second = lowest(pressed); c->cap_t2 = now; c->cap_phase = 2; }
    else if (released & SOM_IN_BIT(c->cap_first)) { cap_commit(c, o, held, 0, c->cap_first, SOM_PRESS_SHORT); return; }
    else if (now - c->cap_t1 >= SOM_BIND_LONG_MS) { cap_commit(c, o, held, 0, c->cap_first, SOM_PRESS_LONG); return; }
  }
  if (c->cap_phase == 2) {
    if (released & SOM_IN_BIT(c->cap_second)) cap_commit(c, o, held, c->cap_first, c->cap_second, now - c->cap_t2 >= SOM_BIND_LONG_MS ? SOM_PRESS_LONG : SOM_PRESS_SHORT);
    else if (now - c->cap_t2 >= SOM_BIND_LONG_MS) cap_commit(c, o, held, c->cap_first, c->cap_second, SOM_PRESS_LONG);
  }
}

void som_bind_update(struct som_bind_ctx *c, uint32_t held, uint32_t now, int mode, struct som_bind_out *o) {
  memset(o, 0, sizeof *o);
  held &= (1u << (SOM_IN_COUNT - 1)) - 1;
  if (c->cap_on) {
    capture_update(c, o, held, now);
    o->swallow = c->cap_on ? held : (c->swallowed & held);
    c->prev = held;
    return;
  }
  uint32_t pressed = held & ~c->prev, released = c->prev & ~held;
  for (int i = 1; i < SOM_IN_COUNT; i++) if (released & SOM_IN_BIT(i)) release_one(c, o, i);
  for (int i = 1; i < SOM_IN_COUNT; i++) if (pressed & SOM_IN_BIT(i)) press_one(c, o, i, held, now, mode);
  for (int i = 1; i < SOM_IN_COUNT; i++) {
    uint32_t bit = SOM_IN_BIT(i);
    if (!(held & bit) || c->long_idx[i] < 0 || (c->long_fired & bit) || (c->chord_used & bit)) continue;
    if (now - c->down_ms[i] >= SOM_BIND_LONG_MS) { c->long_fired |= bit; fire(o, c->long_idx[i]); }
  }
  o->swallow = c->swallowed & held;
  c->prev = held;
}

void som_bind_capture_begin(struct som_bind_ctx *c, uint32_t now) {
  c->cap_on = 1; c->cap_phase = 0; c->cap_start = now; c->cap_conf_valid = 0;
  c->swallowed |= c->prev;   /* buttons already down are not part of the capture */
  c->chord_used = c->long_fired = 0;
  memset(c->short_idx, -1, sizeof c->short_idx);
  memset(c->long_idx, -1, sizeof c->long_idx);
}
void som_bind_capture_cancel(struct som_bind_ctx *c) { if (c->cap_on) cap_end(c, c->prev); }
int som_bind_capturing(const struct som_bind_ctx *c) { return c->cap_on; }

/* ------------------------------------------------------------------ analog helpers */
unsigned som_bind_stick_dirs(unsigned prev, float x, float y) {
  unsigned r = 0;
  float up_th = (prev & 1) ? 0.35f : 0.55f, dn_th = (prev & 2) ? 0.35f : 0.55f, lt_th = (prev & 4) ? 0.35f : 0.55f, rt_th = (prev & 8) ? 0.35f : 0.55f;
  if (-y > up_th) r |= 1;
  if (y > dn_th) r |= 2;
  if (-x > lt_th) r |= 4;
  if (x > rt_th) r |= 8;
  return r;
}
int som_bind_trigger(int prev, float v) { return prev ? v > 0.4f : v > 0.6f; }

void som_bind_rom_variant(const uint8_t *rom, size_t len, char *out, size_t n) {
  snprintf(out, n, "none");
  if (!rom || len < 0x8000) return;
  size_t hdr = (len & 0x200) ? 0x200 : 0;
  if (hdr + 0x7FE0 > len) return;
  const uint8_t *h = rom + hdr + 0x7FC0;
  snprintf(out, n, "%zx-%02x%02x%02x%02x", len, h[0x1C], h[0x1D], h[0x1E], h[0x1F]);
}
