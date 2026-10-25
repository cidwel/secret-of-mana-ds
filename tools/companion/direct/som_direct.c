/* See som_direct.h. Addresses are offsets into the 128 KiB WRAM image ($7E:xxxx = xxxx, $7F:xxxx = 0x10000 + xxxx). */
#include "som_direct.h"
#include <string.h>

#define MBOX 0x1FF90u            /* 7F:FF90, 4 bytes: status, A lo, A hi, P */
#define STUB 0x1FF94u            /* 7F:FF94, 108 bytes */
#define NMI_VEC 0x100u           /* RAM trampoline JML $C0:C0A8 */
#define SPIN_RING 0x694Au        /* C0:694A..694E: the ring and menu loops poll $4212 for vblank here (sounds only) */
#define SPIN_RING_LEN 5u
#define SPIN_MASKED 0xB078u      /* (PC & 0xFFFC) of the main loop's BIT $EC / BNE wait at C0:B079 */

/* game routines the stub calls (same address on vanilla US, the Magno translation and Ancient Cave, checked by prefix) */
#define R_REBUILD_STATS 0xC00036u     /* JML to rebuild_actor_stats; X = hero record offset (slot * 0x200) */
#define R_WEAPON_GFX 0xC0E9F4u        /* refresh_actor_weapon_graphics_long: X = record offset */
#define R_ITEM_EFFECT 0xC0002Au       /* apply_native_consumable_effect: A = item id, X = target hero record offset */
#define R_ITEM_QTY 0xC078C3u          /* change_native_consumable_quantity_and_compact: A = 0xFF, X = record index */
#define R_CAST_PREP 0xC0001Bu         /* prepare_cast_level_and_check_mp: X = caster record offset; A = 0xFF if MP short */
#define R_CAST_START 0xC1DA1Eu        /* JSR $88C7 ; RTL inside the cast wrapper: long entry of prepare_guarded_actor_cast_request_and_party_graphics, X = caster offset */

#define HERO(i) (0xE000u + 0x200u * (unsigned)(i))
#define A_EVENT 0xD0
#define A_F1 0xF1
#define A_PADMASK 0xD9
#define A_FADE_A 0xE4
#define A_FADE_B 0xE6
#define A_RING_OPEN 0x7D
#define W_CC48 0xCC48u
#define W_CC54 0xCC54u

struct sig { uint32_t addr; uint8_t len; uint8_t b[8]; };
static const struct sig SIGS[] = {
  {0xC00036, 3, {0x4C, 0x1E, 0x38}}, {0xC0001B, 3, {0x4C, 0x03, 0x38}}, {0xC0002A, 3, {0x4C, 0x12, 0x38}},
  {0xC0E9F4, 4, {0x20, 0x19, 0xEA, 0x6B}}, {0xC188C7, 4, {0xE2, 0x20, 0x86, 0x14}}, {0xC1DA1E, 4, {0x20, 0xC7, 0x88, 0x6B}}, {0xC078C3, 4, {0x08, 0xE2, 0x30, 0xA0}},
  {0xC0B079, 4, {0x24, 0xEC, 0xD0, 0xFC}}, {0xC0C0A8, 3, {0xC2, 0x30, 0x48}},
};

static unsigned rom_off(uint32_t a) { return ((a >> 16) - 0xC0u) * 0x10000u + (a & 0xFFFFu); }

void somd_init(struct somd *d, const uint8_t *rom, size_t n) {
  memset(d, 0, sizeof *d);
  d->queue_timeout = 600;
  d->rom = rom; d->rom_size = n;
  d->rom_ok = n >= 0x200000;
  for (size_t i = 0; d->rom_ok && i < sizeof SIGS / sizeof SIGS[0]; i++)
    if (rom_off(SIGS[i].addr) + SIGS[i].len > n || memcmp(rom + rom_off(SIGS[i].addr), SIGS[i].b, SIGS[i].len)) d->rom_ok = 0;
}

int somd_busy(const struct somd *d) { return d->state == SOMD_RUNNING; }

const char *somd_err_name(enum somd_err e) {
  static const char *n[] = {"ok", "busy", "unsafe-state", "bad-argument", "not-owned", "no-mp", "caster-busy", "bad-target",
                            "no-target", "needs-ring", "timeout", "rejected", "unsupported-rom", "ko", "status", "expired", "cancelled"};
  return e <= SOMD_ERR_CANCELLED ? n[e] : "?";
}

/* ------------------------------------------------------------------ readers */
static unsigned u8(const uint8_t *w, unsigned a) { return w[a]; }
static unsigned u16(const uint8_t *w, unsigned a) { return w[a] | (w[a + 1] << 8); }

/* The game's selectable-target test (D0:DAA1) for actor slot `slot`, kind: 0 item, 1 spell. */
static int target_valid(const uint8_t *w, int slot, enum somd_class cls, int id, int is_spell) {
  unsigned b = HERO(slot);
  unsigned sx = u16(w, b + 0x20), sy = u16(w, b + 0x22);
  if (sx < 8 || sx >= 0xF8 || sy >= 0xD8) return 0;
  if (u8(w, b) != 1) return 0;
  int dead = (u8(w, b + 0x191) & 0x80) != 0;
  int ident = u8(w, b + 0x180);
  if (slot >= 3) {
    if (cls != SOMD_C_ENEMY_ONE_OR_ALL && cls != SOMD_C_ENEMY_ONE) return 0;
    if (ident & 0x80) return cls == SOMD_C_ENEMY_ONE && ident == 0xEA;   /* flag 0x20 case: Analyzer on object EA */
    return ident == 0x54 || ident == 0x55 || !dead;
  }
  if (cls == SOMD_C_ENEMY_ONE_OR_ALL || cls == SOMD_C_ENEMY_ONE || cls == SOMD_C_NONE || cls == SOMD_C_UNSUPPORTED) return 0;
  if (cls == SOMD_C_BOY_ONLY) return slot == 0 && (u8(w, W_CC54 + 1) & 0xC0) == 0x40;
  if (cls == SOMD_C_ALLY_STATUS_ONE) return (u16(w, b + 0x190) & 0x73FC) != 0;
  if (cls == SOMD_C_ALLY_DEAD_ONE) return dead;
  (void)id; (void)is_spell;
  return ident == 0x54 || ident == 0x55 || !dead;
}

void somd_scan_targets(const uint8_t *w, enum somd_class cls, int id, int is_spell, struct somd_targets *o) {
  memset(o, 0, sizeof *o);
  o->cls = (uint8_t)cls;
  for (int s = 0; s < 6; s++) {
    unsigned b = HERO(s);
    struct somd_actor *a = &o->actor[s];
    a->slot = (uint8_t)s;
    a->present = u8(w, b) == 1;
    a->sx = (uint16_t)u16(w, b + 0x20); a->sy = (uint16_t)u16(w, b + 0x22);
    a->wx = (int16_t)u16(w, b + 2); a->wy = (int16_t)u16(w, b + 4);
    a->on_screen = a->sx >= 8 && a->sx < 0xF8 && a->sy < 0xD8;
    a->id = (uint8_t)u8(w, b + 0x180);
    a->alive = a->present && (!(u8(w, b + 0x191) & 0x80) || a->id == 0x54 || a->id == 0x55);
    a->hp = (uint16_t)u16(w, b + 0x182); a->hp_max = (uint16_t)u16(w, b + 0x184);
    a->status = (uint16_t)u16(w, b + 0x190);
    a->valid = (uint8_t)target_valid(w, s, cls, id, is_spell);
    if (a->valid) { if (s < 3) o->n_allies_valid++; else o->n_enemies_valid++; }
  }
}

/* ------------------------------------------------------------------ classes */
enum somd_class somd_item_class(int id) {
  switch (id) {
  case 0: case 1: case 2: case 3: case 8: case 9: case 10: return SOMD_C_ALLY_ONE;
  case 4: return SOMD_C_ALLY_STATUS_ONE;
  case 5: return SOMD_C_ALLY_DEAD_ONE;
  default: return SOMD_C_UNSUPPORTED;     /* 6 Magic Rope, 7 Flammie Drum (modal), 11 and 12 have no effect */
  }
}

int somd_spell_id(int hero, int spirit, int slot) { return 6 * spirit + (hero == 1 ? 3 : 0) + slot; }

#define SPELL_ROW 0x102AD0u
int somd_spell_mp_cost(const struct somd *d, int id) {
  if (id < 0 || id > 41 || SPELL_ROW + 64u * id + 16 > d->rom_size) return 255;
  return d->rom[SPELL_ROW + 64u * id + 15];
}

enum somd_class somd_spell_class(const struct somd *d, int id) {
  if (id < 0 || id > 41 || SPELL_ROW + 64u * id + 16 > d->rom_size) return SOMD_C_UNSUPPORTED;
  if (id == 0x21) return SOMD_C_ALLY_DEAD_ONE;
  switch (d->rom[SPELL_ROW + 64u * id + 8]) {
  case 0x81: return SOMD_C_ENEMY_ONE_OR_ALL;
  case 0x01: return SOMD_C_ALLY_ONE_OR_ALL;
  case 0x00: return SOMD_C_ALLY_ONE;
  case 0x40: return SOMD_C_BOY_ONLY;
  case 0xA0: return SOMD_C_ENEMY_ONE;
  default: return SOMD_C_UNSUPPORTED;
  }
}

/* ------------------------------------------------------------------ safety */
static int field_safe(const uint8_t *w) {
  if (u8(w, HERO(0)) != 1) return 0;                           /* no game */
  if (u8(w, A_FADE_B) != 0x0F || u8(w, A_FADE_A) != 1) return 0;
  if (u8(w, A_EVENT) != 0 || u8(w, A_RING_OPEN) == 1) return 0;
  if (u8(w, A_F1) & 0x80) return 0;
  unsigned pm = u8(w, A_PADMASK);
  for (int i = 0; i < 3; i++) if (pm & (1u << i)) return !(u16(w, HERO(i) + 0x190) & 0x8000);
  return 0;
}

/* ------------------------------------------------------------------ stub assembler */
struct emit { uint8_t b[112]; int n; uint32_t base; };
static void e1(struct emit *e, int v) { e->b[e->n++] = (uint8_t)v; }
static void e3(struct emit *e, int op, unsigned v) { e1(e, op); e1(e, v & 255); e1(e, (v >> 8) & 255); }
static void e4(struct emit *e, int op, unsigned v) { e1(e, op); e1(e, v & 255); e1(e, (v >> 8) & 255); e1(e, (v >> 16) & 255); }
static unsigned L(unsigned off) { return 0x7E0000u + off; }

/* Two-part stub.  Part 1 runs from the NMI trampoline: if the interrupted PC is the main loop spin it copies the stacked
 * return address into part 2, points the stacked return at part 2, puts the trampoline back and lets the game's own NMI run
 * to its RTI.  The RTI then lands in part 2, which runs in the main thread's context after the whole NMI body (queue
 * flush, forced-blank DMA, HDMA setup) finished inside vblank.  Part 2 calls ONE game routine, publishes the status and
 * jumps to the spin.  Running the routine inside the NMI instead delayed the NMI body past vblank and its forced-blank
 * DMA showed up as a black band on the top screen. */
static int build_stub(const struct somd *d, const struct somd_step *s, const uint8_t orig[4], uint8_t *out, int want_a, int wide) {
  struct emit e; e.n = 0; e.base = STUB;
  (void)d;
  unsigned orig_target = orig[1] | (orig[2] << 8) | (orig[3] << 16);
  unsigned p2 = 0, jmlop = 0;
  for (int pass = 0; pass < 2; pass++) {
    e.n = 0;
    e1(&e, 0xC2); e1(&e, 0x30);                  /* REP #$30 */
    e1(&e, 0x48);                                /* PHA */
    e1(&e, 0xA3); e1(&e, 0x04);                  /* LDA $04,S : interrupted PC */
    e3(&e, 0x29, 0xFFFC);                        /* AND #$FFFC */
    e3(&e, 0xC9, SPIN_MASKED);                   /* CMP #spin */
    int bne = -1, bcs = -1, beq = -1;
    if (wide) {
      e1(&e, 0xF0); beq = e.n; e1(&e, 0);        /* BEQ parked */
      e1(&e, 0xA3); e1(&e, 0x04);                /* LDA $04,S */
      e1(&e, 0x38); e3(&e, 0xE9, SPIN_RING);     /* SEC SBC #ring vblank wait */
      e3(&e, 0xC9, SPIN_RING_LEN);               /* CMP #len */
      e1(&e, 0xB0); bcs = e.n; e1(&e, 0);        /* BCS tail */
      e.b[beq] = (uint8_t)(e.n - (beq + 1));
    } else {
      e1(&e, 0xD0); bne = e.n; e1(&e, 0);        /* BNE tail : not at the spin, NMI runs unchanged and the hook stays armed */
    }
    e1(&e, 0xA3); e1(&e, 0x04);                  /* LDA $04,S */
    e4(&e, 0x8F, 0x7F0000u | ((jmlop + 1) & 0xFFFF));   /* STA long: operand of part 2's final JML */
    e3(&e, 0xA9, p2);                            /* LDA #part2 */
    e1(&e, 0x83); e1(&e, 0x04);                  /* STA $04,S : the RTI returns into part 2 */
    e3(&e, 0xA9, orig[0] | (orig[1] << 8)); e4(&e, 0x8F, L(NMI_VEC));          /* trampoline back */
    e3(&e, 0xA9, orig[2] | (orig[3] << 8)); e4(&e, 0x8F, L(NMI_VEC + 2));
    e1(&e, 0xE2); e1(&e, 0x20);                  /* SEP #$20 */
    e1(&e, 0xA9); e1(&e, 0x7F);
    e1(&e, 0x83); e1(&e, 0x06);                  /* STA $06,S : return bank 7F (the spin is in bank C0) */
    if (bne >= 0) e.b[bne] = (uint8_t)(e.n - (bne + 1));
    if (bcs >= 0) e.b[bcs] = (uint8_t)(e.n - (bcs + 1));
    e1(&e, 0xC2); e1(&e, 0x20);                  /* REP #$20 */
    e1(&e, 0x68);                                /* PLA */
    e4(&e, 0x5C, orig_target);                   /* JML original NMI */
    p2 = (STUB + (unsigned)e.n) & 0xFFFF;
    /* ---- part 2 (main thread) */
    e1(&e, 0x08);                                /* PHP */
    e1(&e, 0xC2); e1(&e, 0x30);                  /* REP #$30 */
    e1(&e, 0x48); e1(&e, 0xDA); e1(&e, 0x5A); e1(&e, 0x0B); e1(&e, 0x8B);    /* PHA PHX PHY PHD PHB */
    e3(&e, 0xF4, 0x7E7E); e1(&e, 0xAB); e1(&e, 0xAB);   /* PEA $7E7E PLB PLB : DB = $7E */
    e3(&e, 0xF4, 0x0000); e1(&e, 0x2B);          /* PEA $0000 PLD */
    if (s->has_a) e3(&e, 0xA9, s->a_reg);
    if (s->has_x) e3(&e, 0xA2, s->x_reg);
    e4(&e, 0x22, s->a);                          /* JSL routine */
    if (want_a) e4(&e, 0x8F, L(MBOX + 1));       /* STA A result (the low byte is what the checks use) */
    e1(&e, 0xE2); e1(&e, 0x20); e1(&e, 0xA9); e1(&e, 0x02);
    e4(&e, 0x8F, L(MBOX));                       /* status = 2 */
    e1(&e, 0xC2); e1(&e, 0x30);
    e1(&e, 0xAB); e1(&e, 0x2B); e1(&e, 0x7A); e1(&e, 0xFA); e1(&e, 0x68);    /* PLB PLD PLY PLX PLA */
    e1(&e, 0x28);                                /* PLP */
    jmlop = STUB + (unsigned)e.n;
    e4(&e, 0x5C, 0xC00000u);                     /* JML C0:xxxx, the interrupted PC (low 16 bits patched by part 1) */
    if (STUB + e.n > SOMD_WRAM_SIZE) return -1;
  }
  jmlop &= 0xFFFF;
  memcpy(out, e.b, (size_t)e.n);
  return e.n;
}

/* ------------------------------------------------------------------ program building */
enum { S_WRITE = 1, S_CALL = 2, S_CHECK_CAST = 3, S_CHECK_MP_RESULT = 4 };

static void st_write(struct somd *d, unsigned addr, unsigned v) {
  struct somd_step *s = &d->step[d->nstep++]; s->type = S_WRITE; s->a = addr; s->b = v;
}
static void st_call(struct somd *d, uint32_t addr, int rts, int has_a, unsigned a, int has_x, unsigned x) {
  struct somd_step *s = &d->step[d->nstep++]; memset(s, 0, sizeof *s);
  s->type = S_CALL; s->a = addr; s->rts = (uint8_t)rts; s->has_a = (uint8_t)has_a; s->a_reg = (uint16_t)a; s->has_x = (uint8_t)has_x; s->x_reg = (uint16_t)x;
}

static int begin(struct somd *d, const uint8_t *w, int kind) {
  if (!d->rom_ok) return SOMD_ERR_UNSUPPORTED_ROM;
  if (d->state == SOMD_RUNNING) return SOMD_ERR_BUSY;
  if (!field_safe(w)) return SOMD_ERR_UNSAFE_STATE;
  d->state = SOMD_IDLE; d->err = SOMD_OK; d->kind = (uint8_t)kind; d->nstep = d->cur = 0; d->armed = 0; d->frames = 0;
  d->undo_addr_n = 0; d->note[0] = 0;
  return SOMD_OK;
}
static int launch(struct somd *d) { d->state = SOMD_RUNNING; return SOMD_OK; }

/* hero record offset for the weapon marks: which weapon types does `hero` hold (owner bits (hero+1) << 6) */
static int held_type(const uint8_t *w, int hero) {
  for (int t = 0; t < 8; t++) { unsigned v = u8(w, W_CC54 + t); if (v != 0xFF && (v & 0xC0) == ((unsigned)(hero + 1) << 6)) return t; }
  return -1;
}

int somd_equip(struct somd *d, const uint8_t *w, int hero, int t) {
  if (hero < 0 || hero > 2 || t < 0 || t > 7) return SOMD_ERR_BAD_ARGUMENT;
  int r = begin(d, w, 1); if (r) return r;
  unsigned v = u8(w, W_CC54 + t);
  if (v == 0xFF) return SOMD_ERR_NOT_OWNED;
  if (u8(w, HERO(hero)) != 1) return SOMD_ERR_BAD_ARGUMENT;
  if (u8(w, HERO(hero) + 0x60) != 0) return SOMD_ERR_CASTER_BUSY;
  int cur = held_type(w, hero);
  int other = (v & 0xC0) ? (int)(v >> 6) - 1 : -1;
  if (other == hero) { d->state = SOMD_FINISHED; strcpy(d->note, "already held"); return SOMD_OK; }
  unsigned tier = v & 0x3F;
  if (other >= 0) {
    if (u8(w, HERO(other)) != 1 || u8(w, HERO(other) + 0x60) != 0) return SOMD_ERR_CASTER_BUSY;
    /* exchange: the other hero receives what this hero holds (or nothing) */
    if (cur >= 0) {
      unsigned cv = u8(w, W_CC54 + cur), ct = cv & 0x3F, row = (unsigned)cur * 9 + ct;
      st_write(d, W_CC54 + cur, (ct | ((unsigned)(other + 1) << 6)));
      st_write(d, HERO(other) + 0x1E3, row); st_write(d, HERO(other) + 0x1E8, row); st_write(d, HERO(other) + 0x1E4, (unsigned)cur);
    } else {
      return SOMD_ERR_BAD_ARGUMENT;      /* taking the other hero's only weapon with nothing to give back */
    }
  } else if (cur >= 0) {
    st_write(d, W_CC54 + cur, u8(w, W_CC54 + cur) & 0x3F);
  }
  unsigned row = (unsigned)t * 9 + tier;
  st_write(d, W_CC54 + t, tier | ((unsigned)(hero + 1) << 6));
  st_write(d, HERO(hero) + 0x1E3, row); st_write(d, HERO(hero) + 0x1E8, row); st_write(d, HERO(hero) + 0x1E4, (unsigned)t);
  st_call(d, R_REBUILD_STATS, 0, 0, 0, 1, 0x200u * hero);
  if (other >= 0) st_call(d, R_REBUILD_STATS, 0, 0, 0, 1, 0x200u * other);
  st_write(d, HERO(hero) + 0x69, 0); st_write(d, HERO(hero) + 0x104, 0);
  st_call(d, R_WEAPON_GFX, 0, 0, 0, 1, 0x200u * hero);
  if (other >= 0) {
    st_write(d, HERO(other) + 0x69, 0); st_write(d, HERO(other) + 0x104, 0);
    st_call(d, R_WEAPON_GFX, 0, 0, 0, 1, 0x200u * other);
  }
  return launch(d);
}

int somd_use_item(struct somd *d, const uint8_t *w, int rec, struct somd_target t) {
  if (rec < 0 || rec > 11) return SOMD_ERR_BAD_ARGUMENT;
  int r = begin(d, w, 2); if (r) return r;
  unsigned v = u8(w, W_CC48 + rec);
  if (v == 0xFF || (v >> 5) == 0) return SOMD_ERR_BAD_ARGUMENT;
  int id = (int)(v & 0x1F);
  enum somd_class cls = somd_item_class(id);
  if (cls == SOMD_C_UNSUPPORTED) return SOMD_ERR_NEEDS_RING;
  if (t.kind != SOMD_T_ONE || t.slot > 2) return SOMD_ERR_BAD_TARGET;
  if (!target_valid(w, t.slot, cls, id, 0)) return SOMD_ERR_BAD_TARGET;
  st_call(d, R_ITEM_EFFECT, 0, 1, (unsigned)id, 1, 0x200u * t.slot);
  if (id != 8 && id != 9) st_call(d, R_ITEM_QTY, 0, 1, 0xFF, 1, (unsigned)rec);
  return launch(d);
}


/* ------------------------------------------------------------------ cast availability, queue, revive */
#define RING_STATUS_MASK 0xD1F0u    /* D9:FD12 + 4 * category 1: the magic ring is greyed out when owner status & mask != 0 */
#define DISABLING_STATUS (RING_STATUS_MASK | 0x0400u)   /* plus the Barrel disguise 0x0400: the real ring cannot complete a cast with it (no timer) */

static int timer_eta(const uint8_t *w, unsigned b, unsigned status) {
  static const struct { unsigned mask; unsigned off; } T[] = {{0x007C, 0x1B2}, {0x0180, 0x1B3}, {0x1800, 0x1B5}, {0x6000, 0x1B6}};
  int eta = 0;
  for (int i = 0; i < 4; i++) {
    if (!(status & T[i].mask & RING_STATUS_MASK)) continue;
    unsigned t = u8(w, b + T[i].off);
    int e = (t == 0 ? 255 : (int)t) * 20;          /* a zero timer wraps to 255 and counts down; one drop per 4 ticks of 5 frames */
    if (e > eta) eta = e;
  }
  return eta;
}

/* `strict` 1 = the answer used by the casting functions (environment included). */
static int cast_reason(const struct somd *d, const uint8_t *w, int hero, int spirit, int slot, struct somd_cast_info *ci, int strict) {
  struct somd_cast_info tmp; if (!ci) ci = &tmp;
  memset(ci, 0, sizeof *ci);
  (void)strict;
  if (hero < 1 || hero > 2 || spirit < 0 || spirit > 6 || slot < 0 || slot > 2) { ci->reason = SOMD_CAST_BAD_ARGUMENT; ci->permanent = 1; return ci->reason; }
  int id = somd_spell_id(hero, spirit, slot);
  if (id > 41) { ci->reason = SOMD_CAST_BAD_ARGUMENT; ci->permanent = 1; return ci->reason; }
  unsigned b = HERO(hero);
  ci->spell_id = (uint8_t)id; ci->cls = (uint8_t)somd_spell_class(d, id);
  ci->mp_cost = (uint8_t)somd_spell_mp_cost(d, id); ci->mp_have = (uint8_t)u8(w, b + 0x186);
  ci->status = (uint16_t)u16(w, b + 0x190);
  if (u8(w, b) != 1) { ci->reason = SOMD_CAST_BAD_ARGUMENT; ci->permanent = 1; return ci->reason; }
  if (!u8(w, b + 0x1C8 + (hero == 1 && spirit == 6 ? 7 : spirit))) { ci->reason = SOMD_CAST_NOT_OWNED; ci->permanent = 1; return ci->reason; }
  if ((ci->status & 0x8000) || (u8(w, b + 0x191) & 0x80)) { ci->reason = SOMD_CAST_KO; ci->permanent = 1; return ci->reason; }
  if (ci->mp_have < ci->mp_cost) { ci->reason = SOMD_CAST_NO_MP; ci->permanent = 1; return ci->reason; }
  ci->will_clear = 1;
  if (u8(w, HERO(0)) != 1) { ci->reason = SOMD_CAST_UNSAFE; ci->will_clear = 0; return ci->reason; }
  if (u8(w, A_RING_OPEN) == 1) { ci->reason = SOMD_CAST_RING_OPEN; return ci->reason; }
  if (u8(w, A_EVENT) != 0) { ci->reason = SOMD_CAST_EVENT; return ci->reason; }
  if (u8(w, A_FADE_A) != 1 || u8(w, A_FADE_B) != 0x0F) { ci->reason = SOMD_CAST_FADE; ci->eta_frames = 30; return ci->reason; }
  if (u8(w, A_F1) & 0x80) { ci->reason = SOMD_CAST_INPUT_LOCK; return ci->reason; }
  if (ci->status & DISABLING_STATUS) {
    ci->reason = (ci->status & 0x0080) ? SOMD_CAST_SILENCED : SOMD_CAST_STATUS;
    ci->eta_frames = (uint16_t)timer_eta(w, b, ci->status);
    ci->will_clear = (ci->status & RING_STATUS_MASK) ? 1 : 0;      /* the Barrel disguise stays until it is toggled off */
    return ci->reason;
  }
  if (u8(w, b + 0x60) != 0 || u8(w, b + 0x1C) != 0) { ci->reason = SOMD_CAST_BUSY; ci->eta_frames = 90; return ci->reason; }
  if (d->state == SOMD_RUNNING) { ci->reason = SOMD_CAST_COMMAND; ci->eta_frames = 4; return ci->reason; }
  { unsigned pm = u8(w, A_PADMASK); int alive = 0;
    for (int i = 0; i < 3; i++) if ((pm & (1u << i)) && !(u16(w, HERO(i) + 0x190) & 0x8000)) alive = 1;
    if (!alive) {       /* the game hands the pad to a living hero within a few frames; a wiped party never clears */
      int any = 0;
      for (int i = 0; i < 3; i++) if (u8(w, HERO(i)) == 1 && !(u16(w, HERO(i) + 0x190) & 0x8000)) any = 1;
      ci->reason = SOMD_CAST_UNSAFE; ci->will_clear = (uint8_t)any; ci->eta_frames = any ? 10 : 0; return ci->reason; } }
  ci->will_clear = 0;
  ci->reason = SOMD_CAST_OK;
  return ci->reason;
}

int somd_can_cast(const struct somd *d, const uint8_t *w, int hero, int spirit, int slot, struct somd_cast_info *info) {
  return cast_reason(d, w, hero, spirit, slot, info, 1);
}

static int resolve_target(const struct somd *d, const uint8_t *w, int hero, int spirit, int slot, struct somd_policy p, struct somd_target *t) {
  int id = somd_spell_id(hero, spirit, slot);
  enum somd_class cls = somd_spell_class(d, id);
  int enemy = cls == SOMD_C_ENEMY_ONE_OR_ALL || cls == SOMD_C_ENEMY_ONE;
  int group_ok = cls == SOMD_C_ENEMY_ONE_OR_ALL || cls == SOMD_C_ALLY_ONE_OR_ALL;
  int lo = enemy ? 3 : 0, hi = enemy ? 6 : 3;
  unsigned cb = HERO(hero);
  if (p.kind == SOMD_POL_SLOT) {
    if (p.slot > 5 || (p.slot >= 3) != enemy || !target_valid(w, p.slot, cls, id, 1)) return SOMD_ERR_BAD_TARGET;
    t->kind = SOMD_T_ONE; t->slot = p.slot; return SOMD_OK;
  }
  if (p.kind == SOMD_POL_ALL) {
    int n = 0;
    for (int s = lo; s < hi; s++) n += target_valid(w, s, cls, id, 1);
    if (!group_ok) return SOMD_ERR_BAD_TARGET;
    if (!n) return SOMD_ERR_NO_TARGET;
    t->kind = enemy ? SOMD_T_ALL_ENEMIES : SOMD_T_ALL_ALLIES; t->slot = 0; return SOMD_OK;
  }
  int best = -1; long bv = 0;
  for (int s = lo; s < hi; s++) {
    if (!target_valid(w, s, cls, id, 1)) continue;
    unsigned sb = HERO(s);
    long v;
    if (p.kind == SOMD_POL_LOWEST_HP) v = (long)u16(w, sb + 0x182);
    else { long dx = (long)(int16_t)u16(w, sb + 0x20) - (int16_t)u16(w, cb + 0x20), dy = (long)(int16_t)u16(w, sb + 0x22) - (int16_t)u16(w, cb + 0x22); v = dx * dx + dy * dy; }
    if (best < 0 || v < bv) { best = s; bv = v; }
  }
  if (best < 0) return SOMD_ERR_NO_TARGET;
  t->kind = SOMD_T_ONE; t->slot = (uint8_t)best;
  return SOMD_OK;
}

static int queue_fire(struct somd *d, const uint8_t *w, int hero) {
  struct somd_queued *q = &d->queue[hero];
  struct somd_target t;
  struct somd_policy p; p.kind = q->pol_kind; p.slot = q->pol_slot;
  int r = resolve_target(d, w, hero, q->spirit, q->slot, p, &t);
  if (r == SOMD_OK) r = somd_cast(d, w, hero, q->spirit, q->slot, t);
  if (r == SOMD_OK) { q->state = SOMD_Q_FIRED; q->err = SOMD_OK; }
  else { q->state = SOMD_Q_DROPPED; q->err = (uint8_t)r; }
  return r;
}

static void queue_tick(struct somd *d, const uint8_t *w) {
  int fired = 0;
  for (int h = 1; h <= 2; h++) {
    struct somd_queued *q = &d->queue[h];
    if (q->state != SOMD_Q_WAITING) continue;
    q->waited++;
    struct somd_cast_info ci;
    int why = cast_reason(d, w, h, q->spirit, q->slot, &ci, 1);
    q->reason = (uint8_t)why;
    if (why != SOMD_CAST_OK && ci.permanent) {
      q->state = SOMD_Q_DROPPED;
      q->err = (uint8_t)(why == SOMD_CAST_KO ? SOMD_ERR_KO : why == SOMD_CAST_NO_MP ? SOMD_ERR_NO_MP : SOMD_ERR_BAD_ARGUMENT);
      continue;
    }
    if (why == SOMD_CAST_OK && !fired) { queue_fire(d, w, h); fired = 1; continue; }
    if (q->waited > q->timeout) { q->state = SOMD_Q_DROPPED; q->err = SOMD_ERR_EXPIRED; }
  }
}

int somd_queue_cast(struct somd *d, const uint8_t *w, int hero, int spirit, int slot, struct somd_policy policy, int timeout_frames,
                    struct somd_cast_info *info) {
  struct somd_cast_info ci;
  int why = cast_reason(d, w, hero, spirit, slot, &ci, 1);
  if (info) *info = ci;
  if (why != SOMD_CAST_OK && ci.permanent) {
    return why == SOMD_CAST_KO ? SOMD_ERR_KO : why == SOMD_CAST_NO_MP ? SOMD_ERR_NO_MP : SOMD_ERR_BAD_ARGUMENT;
  }
  struct somd_queued *q = &d->queue[hero];
  int rep = q->state == SOMD_Q_WAITING, repid = q->spell_id;   /* one queue per caster: a second spell of the same caster replaces the first */
  memset(q, 0, sizeof *q);
  q->replaced = (uint8_t)rep; q->replaced_id = (uint8_t)repid;
  q->state = SOMD_Q_WAITING; q->reason = (uint8_t)why; q->spirit = (uint8_t)spirit; q->slot = (uint8_t)slot; q->spell_id = ci.spell_id;
  q->pol_kind = policy.kind; q->pol_slot = policy.slot;
  q->timeout = (uint16_t)(timeout_frames > 0 ? timeout_frames : d->queue_timeout);
  if (why == SOMD_CAST_OK) {
    int r = queue_fire(d, w, hero);
    return r == SOMD_OK ? SOMD_OK : r;       /* a target that is already gone is reported at once (the entry shows DROPPED) */
  }
  return SOMD_OK;
}

int somd_queue_state(const struct somd *d, int hero, struct somd_queue_info *out) {
  if (hero < 0 || hero > 2) return SOMD_Q_EMPTY;
  const struct somd_queued *q = &d->queue[hero];
  if (out) { memset(out, 0, sizeof *out); out->state = q->state; out->reason = q->reason; out->err = q->err; out->spell_id = q->spell_id; out->waited = q->waited; out->timeout = q->timeout; out->replaced = q->replaced; out->replaced_id = q->replaced_id; }
  return q->state;
}
void somd_queue_cancel(struct somd *d, int hero) {
  if (hero < 0 || hero > 2) return;
  struct somd_queued *q = &d->queue[hero];
  if (q->state == SOMD_Q_WAITING) { q->state = SOMD_Q_DROPPED; q->err = SOMD_ERR_CANCELLED; }
}
void somd_queue_ack(struct somd *d, int hero) {
  if (hero < 0 || hero > 2) return;
  if (d->queue[hero].state != SOMD_Q_WAITING) memset(&d->queue[hero], 0, sizeof d->queue[hero]);
}

int somd_revive_options(const struct somd *d, const uint8_t *w, int hero, struct somd_revive_option out[4]) {
  int n = 0;
  if (hero < 0 || hero > 2 || u8(w, HERO(hero)) != 1 || !(u8(w, HERO(hero) + 0x191) & 0x80)) return 0;
  for (int i = 0; i < 12 && n < 3; i++) {
    unsigned v = u8(w, W_CC48 + (unsigned)i);
    if (v != 0xFF && (v & 0x1F) == 5 && (v >> 5) > 0) {
      struct somd_revive_option *o = &out[n++]; memset(o, 0, sizeof *o);
      o->kind = 1; o->id = 5; o->qty = (uint8_t)(v >> 5); o->record = (uint8_t)i; o->usable = 1;
    }
  }
  /* Revivifier: spell 33 = girl (hero 1), Dryad (spirit position 5), slot 0 */
  if (n < 4) {
    struct somd_cast_info ci;
    int why = cast_reason(d, w, 1, 5, 0, &ci, 1);
    struct somd_revive_option *o = &out[n++]; memset(o, 0, sizeof *o);
    o->kind = 2; o->id = 33; o->caster = 1; o->spirit = 5; o->slot = 0; o->mp_cost = ci.mp_cost; o->reason = (uint8_t)why;
    o->usable = why == SOMD_CAST_OK && hero != 1;
    if (hero == 1 && why == SOMD_CAST_OK) o->reason = SOMD_CAST_KO;   /* the fallen girl cannot cast it on herself */
  }
  return n;
}

int somd_cast(struct somd *d, const uint8_t *w, int hero, int spirit, int slot, struct somd_target t) {
  if (hero < 1 || hero > 2 || spirit < 0 || spirit > 6 || slot < 0 || slot > 2) return SOMD_ERR_BAD_ARGUMENT;
  int r = begin(d, w, 3); if (r) return r;
  unsigned b = HERO(hero);
  int id = somd_spell_id(hero, spirit, slot);
  if (id > 41) return SOMD_ERR_BAD_ARGUMENT;
  {
    struct somd_cast_info ci;
    int why = cast_reason(d, w, hero, spirit, slot, &ci, 1);
    switch (why) {
    case SOMD_CAST_OK: break;
    case SOMD_CAST_KO: return SOMD_ERR_KO;
    case SOMD_CAST_NO_MP: return SOMD_ERR_NO_MP;
    case SOMD_CAST_SILENCED: case SOMD_CAST_STATUS: return SOMD_ERR_STATUS;
    case SOMD_CAST_BUSY: return SOMD_ERR_CASTER_BUSY;
    case SOMD_CAST_BAD_ARGUMENT: case SOMD_CAST_NOT_OWNED: return SOMD_ERR_BAD_ARGUMENT;
    default: return SOMD_ERR_UNSAFE_STATE;
    }
  }
  enum somd_class cls = somd_spell_class(d, id);
  if (cls == SOMD_C_UNSUPPORTED) return SOMD_ERR_BAD_ARGUMENT;
  int enemy = cls == SOMD_C_ENEMY_ONE_OR_ALL || cls == SOMD_C_ENEMY_ONE;
  int group_ok = cls == SOMD_C_ENEMY_ONE_OR_ALL || cls == SOMD_C_ALLY_ONE_OR_ALL;
  uint8_t ids[3]; int n = 0;
  if (t.kind == SOMD_T_ONE) {
    if (t.slot > 5 || (t.slot >= 3) != enemy || !target_valid(w, t.slot, cls, id, 1)) return SOMD_ERR_BAD_TARGET;
    ids[n++] = (uint8_t)(t.slot + 1);
  } else if (t.kind == SOMD_T_ALL_ALLIES || t.kind == SOMD_T_ALL_ENEMIES) {
    if (!group_ok || (t.kind == SOMD_T_ALL_ENEMIES) != enemy) return SOMD_ERR_BAD_TARGET;
    for (int s = enemy ? 3 : 0; s < (enemy ? 6 : 3); s++) if (target_valid(w, s, cls, id, 1)) ids[n++] = (uint8_t)(s + 1);
    if (!n) return SOMD_ERR_NO_TARGET;
  } else {
    return SOMD_ERR_BAD_TARGET;
  }
  d->cast_hero = (uint8_t)hero;
  st_write(d, b + 0x170, (unsigned)id); st_write(d, b + 0x177, (unsigned)spirit);
  st_write(d, b + 0x176, (unsigned)n);
  for (int i = 0; i < 3; i++) st_write(d, b + 0x178 + i, i < n ? ids[i] : 0);
  st_call(d, R_CAST_PREP, 0, 0, 0, 1, 0x200u * hero);
  struct somd_step *s = &d->step[d->nstep++]; memset(s, 0, sizeof *s); s->type = S_CHECK_MP_RESULT;
  st_call(d, R_CAST_START, 0, 0, 0, 1, 0x200u * hero);
  s = &d->step[d->nstep++]; memset(s, 0, sizeof *s); s->type = S_CHECK_CAST;
  if (d->queue[hero].state == SOMD_Q_WAITING) { d->queue[hero].state = SOMD_Q_DROPPED; d->queue[hero].err = SOMD_ERR_CANCELLED; }
  return launch(d);
}

/* ------------------------------------------------------------------ frame hooks */
static void wr(struct somd *d, uint8_t *w, unsigned addr, unsigned v, int undo) {
  if (undo && d->undo_addr_n < 12) { d->undo_addr[d->undo_addr_n] = addr; d->undo_val[d->undo_addr_n++] = w[addr]; }
  w[addr] = (uint8_t)v;
}

/* The trampoline goes back at once; the bytes under the stub are given back one frame later because part 2 may still be
 * executing its last instructions when the frame ends. */
static void disarm(struct somd *d, uint8_t *w, int immediate) {
  if (d->armed) {
    memcpy(w + NMI_VEC, d->orig_nmi, 4);
    if (immediate) memcpy(w + MBOX, d->stub_save, 0x70); else d->release = 1;
    d->armed = 0;
  }
}

static void fail(struct somd *d, uint8_t *w, enum somd_err e, int rollback) {
  disarm(d, w, w[MBOX] == 0);
  if (rollback) while (d->undo_addr_n) { d->undo_addr_n--; w[d->undo_addr[d->undo_addr_n]] = d->undo_val[d->undo_addr_n]; }
  d->state = SOMD_FAILED; d->err = e;
}

static void menu_pre(struct somd *d, uint8_t *w);
static void menu_post(struct somd *d, uint8_t *w);

void somd_pre_frame(struct somd *d, uint8_t *w) {
  if (d->release) { memcpy(w + MBOX, d->stub_save, 0x70); d->release = 0; }
  queue_tick(d, w);
  if (d->state != SOMD_RUNNING) return;
  if (d->kind >= 5) { menu_pre(d, w); return; }
  d->frames++;
  if (!d->armed) {
    /* a game that leaves field mode before the first game call started is abandoned; later it must run to the end */
    int first_call_done = 0;
    for (int i = 0; i < d->cur; i++) if (d->step[i].type == S_CALL) first_call_done = 1;
    if (d->kind != 4 && !first_call_done && !field_safe(w)) { fail(d, w, SOMD_ERR_UNSAFE_STATE, 1); return; }
    while (d->cur < d->nstep && d->step[d->cur].type == S_WRITE) {
      wr(d, w, d->step[d->cur].a, d->step[d->cur].b, 1); d->cur++;
    }
    if (d->cur >= d->nstep) { d->state = SOMD_FINISHED; return; }
    const struct somd_step *s = &d->step[d->cur];
    if (s->type != S_CALL) return;
    uint8_t stub[112]; memcpy(d->orig_nmi, w + NMI_VEC, 4);
    int want_a = d->cur + 1 < d->nstep && d->step[d->cur + 1].type == S_CHECK_MP_RESULT;
    int n = build_stub(d, s, d->orig_nmi, stub, want_a, d->kind == 4);
    if (n < 0) { fail(d, w, SOMD_ERR_UNSUPPORTED_ROM, 1); return; }
    memcpy(d->stub_save, w + MBOX, 0x70);
    memset(w + MBOX, 0, 4);
    memcpy(w + STUB, stub, (size_t)n);
    w[NMI_VEC] = 0x5C; w[NMI_VEC + 1] = STUB & 255; w[NMI_VEC + 2] = (STUB >> 8) & 255; w[NMI_VEC + 3] = 0x7F;
    d->armed = 1; d->armed_frames = 0;
  }
}

void somd_post_frame(struct somd *d, uint8_t *w) {
  if (d->state == SOMD_RUNNING && d->kind >= 5) { menu_post(d, w); return; }
  if (d->state != SOMD_RUNNING || !d->armed) return;
  d->armed_frames++;
  if (w[MBOX] == 2) {
    d->last_a_low = w[MBOX + 1];
    disarm(d, w, 0);
    d->cur++;
    /* result steps */
    while (d->cur < d->nstep && d->step[d->cur].type != S_CALL && d->step[d->cur].type != S_WRITE) {
      int t = d->step[d->cur].type;
      if (t == S_CHECK_MP_RESULT && d->last_a_low == 0xFF) { fail(d, w, SOMD_ERR_NO_MP, 0); return; }
      if (t == S_CHECK_CAST) {
        unsigned b = HERO(d->cast_hero);
        if (u8(w, b + 0x1C) != 0x40 || u8(w, b + 0x11) != 0x94) { fail(d, w, SOMD_ERR_REJECTED, 0); return; }
      }
      d->cur++;
    }
    if (d->cur >= d->nstep) d->state = SOMD_FINISHED;
    return;
  }
  /* trampoline back to the game's own vector means part 1 ran and part 2 is on its way; give it a few frames more */
  int rerouted = memcmp(w + NMI_VEC, d->orig_nmi, 4) == 0;
  if (d->armed_frames >= (rerouted ? 120 : 30)) {
    int calls_done = 0;
    for (int i = 0; i < d->cur; i++) if (d->step[i].type == S_CALL) calls_done = 1;
    fail(d, w, SOMD_ERR_TIMEOUT, !calls_done && !rerouted);
  }
}

/* ------------------------------------------------------------------ menu sounds */
/* The ten fixed-id menu audio presets at D0:CEF8 + 8 * n (docs/event-menu-ring-audio.md); each preserves A and P. */
static const uint8_t SFX_IDS[10] = {0x0C, 0x0D, 0x0E, 0x29, 0x2A, 0x2B, 0x2C, 0x4B, 0x28, 0xA3};

int somd_sound(struct somd *d, const uint8_t *w, int sfx_id) {
  int n = -1;
  for (int i = 0; i < 10; i++) if (SFX_IDS[i] == sfx_id) n = i;
  if (n < 0) return SOMD_ERR_BAD_ARGUMENT;
  if (d->state == SOMD_RUNNING) return SOMD_ERR_BUSY;
  if (!d->rom_ok) return SOMD_ERR_UNSUPPORTED_ROM;
  uint32_t addr = 0xD0CEF8u + 8u * (unsigned)n;
  const uint8_t *code = d->rom + rom_off(addr);
  static const uint8_t pre[5] = {0x48, 0x08, 0xE2, 0x20, 0xA9};
  if (rom_off(addr) + 8 > d->rom_size || memcmp(code, pre, 5) || code[5] != sfx_id) return SOMD_ERR_UNSUPPORTED_ROM;
  if (u8(w, HERO(0)) != 1) return SOMD_ERR_UNSAFE_STATE;      /* no game running: the audio driver is not up */
  d->state = SOMD_IDLE; d->err = SOMD_OK; d->kind = 4; d->nstep = d->cur = 0; d->armed = 0; d->frames = 0;
  d->undo_addr_n = 0; d->note[0] = 0;
  st_call(d, addr, 0, 0, 0, 0, 0);
  return launch(d);
}


/* ------------------------------------------------------------------ direct menus */
#define A_PAD1_EDGE 0xCC          /* pad 1 edge flags: bit 1 field hero switch (X), bit 2 open the ring (Y) */
#define A_PAD_HERO 0xD9           /* bit n = hero n bound to pad 1 */
#define R_PHASE 0x1812u
#define R_OWNER 0x1808u
#define R_CAT 0x1809u
#define R_FLAGS 0x1803u
#define R_COUNT 0x180Bu
#define R_ACTIVE 0x1826u          /* 1 while the ring loop runs; 0 asks it to leave */
#define R_MAIL 0x19F0u            /* ring exit mailbox: C0 = a page was requested */
#define R_SAVED_CAT 0x1870u       /* last category per hero */
#define R_RANK_ITEM 0x1873u
#define R_RANK_GIRL 0x1874u
#define R_RANK_SPRITE 0x1875u
#define R_RANK_CONFIG 0x1876u
#define R_SPELL_SEL 0x1839u
#define PAD_B 0x0001u
#define PAD_Y 0x0002u
#define PAD_SELECT 0x0004u

static int ring_open(const uint8_t *w) { return u8(w, A_RING_OPEN) == 1; }
static int ring_idle(const uint8_t *w) { return ring_open(w) && u8(w, R_PHASE) == 0 && u8(w, R_CAT) != 0x55; }
static int ring_top(const uint8_t *w) { return ring_idle(w) && u8(w, R_ACTIVE) == 1 && (u8(w, R_FLAGS) & 0xE8) == 0 && u8(w, R_CAT) < 4; }
static int field_ready(const uint8_t *w) {
  return !ring_open(w) && u8(w, A_EVENT) == 0 && u8(w, A_FADE_A) == 1 && u8(w, A_FADE_B) == 0x0F;
}

/* a Y or X tap sent while a page was closing can arrive after the page is gone: drop that latched edge so the ring does not reopen */
static void menu_done(struct somd *d, uint8_t *w, int clear_edges) {
  if (clear_edges) w[A_PAD1_EDGE] &= (uint8_t)~6u;
  d->state = SOMD_FINISHED; d->hide = 0; d->mpad = 0;
}

static int pad_hero_of(const uint8_t *w) {
  unsigned m = u8(w, A_PAD_HERO);
  for (int i = 0; i < 3; i++) if (m & (1u << i)) return i;
  return -1;
}

static int mpush(struct somd *d, int kind) {
  d->state = SOMD_RUNNING; d->err = SOMD_OK; d->kind = (uint8_t)kind; d->nstep = d->cur = 0; d->armed = 0; d->frames = 0;
  d->mphase = 0; d->mtimer = 0; d->mpad = 0; d->mtotal = 0; d->mtries = 0; d->hide = 1; d->note[0] = 0;
  return SOMD_OK;
}

int somd_open_menu(struct somd *d, const uint8_t *w, int kind, int hero, int arg) {
  if (d->state == SOMD_RUNNING) return SOMD_ERR_BUSY;
  if (kind < SOMD_MENU_RING || kind > SOMD_MENU_PAGE || hero < 0 || hero > 2) return SOMD_ERR_BAD_ARGUMENT;
  if (kind == SOMD_MENU_RING && (arg < 0 || arg > 3)) return SOMD_ERR_BAD_ARGUMENT;
  if (kind == SOMD_MENU_SPIRIT && (hero == 0 || arg < 0 || arg > 7)) return SOMD_ERR_BAD_ARGUMENT;
  if (kind == SOMD_MENU_PAGE && (arg < 0 || arg > 6)) return SOMD_ERR_BAD_ARGUMENT;
  if (u8(w, HERO(0)) != 1 || u8(w, HERO(hero)) != 1 || (u16(w, HERO(hero) + 0x190) & 0x8000)) return SOMD_ERR_BAD_ARGUMENT;
  if (!ring_open(w) && !field_safe(w)) return SOMD_ERR_UNSAFE_STATE;    /* an open ring, spell list or page is closed first */
  /* the entry must exist, otherwise the game would build an empty ring */
  if (kind == SOMD_MENU_SPIRIT) {
    int n = 0, rank = -1;
    for (int b = 0; b < 8; b++) {
      if (hero == 1 && b == 6) continue;
      if (hero == 2 && b == 7) continue;
      if (u8(w, HERO(hero) + 0x1C8 + (unsigned)b)) { if (b == arg) rank = n; n++; }
    }
    if (rank < 0) return SOMD_ERR_NOT_OWNED;
  }
  if (kind == SOMD_MENU_RING && arg == 0) {
    int n = 0; for (int i = 0; i < 12; i++) { unsigned v = u8(w, W_CC48 + (unsigned)i); if (v != 0xFF && (v & 0x1F) < 13) n++; }
    if (!n) return SOMD_ERR_NO_TARGET;
  }
  if (kind == SOMD_MENU_RING && arg == 1) {
    if (hero == 0) return SOMD_ERR_BAD_ARGUMENT;
    int n = 0; for (int b = 0; b < 8; b++) if (!((hero == 1 && b == 6) || (hero == 2 && b == 7)) && u8(w, HERO(hero) + 0x1C8 + (unsigned)b)) n++;
    if (!n) return SOMD_ERR_NO_TARGET;
  }
  mpush(d, 5);
  d->mkind = (uint8_t)kind; d->mhero = (uint8_t)hero; d->marg = (uint8_t)arg;
  d->mphase = ring_open(w) ? 10 : 0;          /* an open ring is closed first */
  return SOMD_OK;
}

int somd_close_menu(struct somd *d, const uint8_t *w) {
  if (d->state == SOMD_RUNNING) return SOMD_ERR_BUSY;
  if (!ring_open(w)) { d->state = SOMD_FINISHED; d->err = SOMD_OK; d->frames = 0; d->hide = 0; strcpy(d->note, "not open"); return SOMD_OK; }
  mpush(d, 6);
  d->mkind = 0; d->mphase = 10;
  return SOMD_OK;
}

uint16_t somd_menu_pad(const struct somd *d) { return d->state == SOMD_RUNNING && d->kind >= 5 ? d->mpad : 0; }
/* Menu jumps play in real time: no frame is hidden or skipped (round 3).  A test harness may ask for the old fast mode. */
void somd_set_fast_menu(struct somd *d, int on) { d->fast = on ? 1 : 0; }
int somd_hide_video(const struct somd *d) { return d->fast && d->state == SOMD_RUNNING && d->kind >= 5 && d->hide; }

/* Makes `hero` the pad-controlled hero with the game's own field hero switch (the X edge flag, repeated until $D9 names the hero).  Kind 7. */
int somd_switch_hero(struct somd *d, const uint8_t *w, int hero) {
  if (d->state == SOMD_RUNNING) return SOMD_ERR_BUSY;
  if (hero < 0 || hero > 2 || u8(w, HERO(0)) != 1 || u8(w, HERO(hero)) != 1 || (u16(w, HERO(hero) + 0x190) & 0x8000)) return SOMD_ERR_BAD_ARGUMENT;
  if (!field_safe(w)) return SOMD_ERR_UNSAFE_STATE;
  if (pad_hero_of(w) == hero) { d->state = SOMD_FINISHED; d->err = SOMD_OK; d->frames = 0; d->hide = 0; strcpy(d->note, "already"); return SOMD_OK; }
  mpush(d, 7);
  d->hide = 0; d->mkind = 0; d->mhero = (uint8_t)hero; d->mphase = 0;
  return SOMD_OK;
}

/* Name entry, direct text: writes the letter into the game's name tile buffer exactly as its append routine does (C0:318E with the tile
 * taken from the keyboard grid at $9246, row 3 = A..Z) without moving the visible cursor.  Returns SOMD_OK, UNSAFE_STATE (no name entry),
 * NO_TARGET (name full), BAD_ARGUMENT. */
#define NE_COUNT 0xA1CCu
#define NE_ROW 0xA156u
#define NE_COL 0xA157u
#define NE_FLAGS 0xA20Au
#define NE_GRID 0x9246u
#define NE_BUF 0x9000u
static int name_entry_up(const uint8_t *w) { return w[0x9C00] != 0xF8 && w[0x9C01] < 0xE0 && (w[0xA20A] & 0x10) && u8(w, 0xE4) == 0 && u16(w, 0xE184) == 0; }
static int name_limit(const struct somd *d) {
  return d->rom && d->rom_size > 0x319C && d->rom[0x319B] == 0xC9 ? d->rom[0x319C] : 6;   /* the CMP #n of the append routine */
}
int somd_name_letter(struct somd *d, uint8_t *w, int letter) {
  if (letter < 0 || letter > 25) return SOMD_ERR_BAD_ARGUMENT;
  if (!name_entry_up(w)) return SOMD_ERR_UNSAFE_STATE;
  unsigned n = u8(w, NE_COUNT);
  if (n >= (unsigned)name_limit(d)) return SOMD_ERR_NO_TARGET;
  unsigned tile = u16(w, NE_GRID + 192u + 2u * (unsigned)letter);
  if (!tile) return SOMD_ERR_REJECTED;
  unsigned x = u8(w, NE_ROW) * 64u + u8(w, NE_COL);
  if (x + 0x42u >= 0x600u) return SOMD_ERR_REJECTED;
  w[NE_BUF + x] = (uint8_t)tile; w[NE_BUF + x + 1] = (uint8_t)(tile >> 8);
  w[NE_BUF + 0x40u + x] = (uint8_t)(tile + 2u); w[NE_BUF + 0x41u + x] = (uint8_t)((tile + 2u) >> 8);
  w[NE_FLAGS] |= 2; w[NE_COL] = (uint8_t)(w[NE_COL] + 2); w[NE_COUNT] = (uint8_t)(n + 1);
  return SOMD_OK;
}
/* Deletes the last letter (what the game's right-button handler does). */
int somd_name_delete(struct somd *d, uint8_t *w) {
  (void)d;
  if (!name_entry_up(w)) return SOMD_ERR_UNSAFE_STATE;
  unsigned n = u8(w, NE_COUNT);
  if (!n) return SOMD_ERR_NO_TARGET;
  w[NE_COL] = (uint8_t)(w[NE_COL] - 2);
  unsigned x = u8(w, NE_ROW) * 64u + u8(w, NE_COL);
  if (x + 0x42u >= 0x600u) return SOMD_ERR_REJECTED;
  w[NE_BUF + x] = w[NE_BUF + x + 1] = w[NE_BUF + 0x40u + x] = w[NE_BUF + 0x41u + x] = 0;
  w[NE_FLAGS] |= 2; w[NE_COUNT] = (uint8_t)(n - 1);
  return SOMD_OK;
}

static void mfail(struct somd *d, uint8_t *w, enum somd_err e) {
  if (u8(w, A_PAD1_EDGE) & 4) w[A_PAD1_EDGE] &= (uint8_t)~4u;     /* never leave a pending Y edge behind */
  if (u8(w, A_PAD1_EDGE) & 2) w[A_PAD1_EDGE] &= (uint8_t)~2u;
  d->state = SOMD_FAILED; d->err = e; d->hide = 0; d->mpad = 0;
}

static void write_presets(struct somd *d, uint8_t *w) {
  int hero = d->mhero, cat = d->mkind == SOMD_MENU_RING ? d->marg : d->mkind == SOMD_MENU_SPIRIT ? 1 : 2;
  w[R_SAVED_CAT + (unsigned)hero] = (uint8_t)cat;
  if (d->mkind == SOMD_MENU_SPIRIT) {
    int n = 0, rank = 0;
    for (int b = 0; b < 8; b++) {
      if (hero == 1 && b == 6) continue;
      if (hero == 2 && b == 7) continue;
      if (u8(w, HERO(hero) + 0x1C8 + (unsigned)b)) { if (b == d->marg) rank = n; n++; }
    }
    w[hero == 1 ? R_RANK_GIRL : R_RANK_SPRITE] = (uint8_t)rank;
  }
  if (d->mkind == SOMD_MENU_PAGE) w[R_RANK_CONFIG] = (uint8_t)d->marg;
}

/* before the frame runs */
static void menu_pre(struct somd *d, uint8_t *w) {
  d->mpad = 0;
  switch (d->mphase) {
  case 0:      /* bind the pad to the hero with the game's own field hero switch (X edge) */
    if (pad_hero_of(w) == d->mhero) { if (d->kind == 7) { menu_done(d, w, 1); return; } d->mphase = 1; }
    else if (!(u8(w, A_PAD1_EDGE) & 2) && d->mtimer % 10 == 0) {
      if (!field_safe(w)) { mfail(d, w, SOMD_ERR_UNSAFE_STATE); return; }
      w[A_PAD1_EDGE] |= 2;
    }
    if (d->mphase != 1) break;
    /* fall through */
  case 1:      /* presets, then the field's Y edge */
    if (!field_safe(w)) { mfail(d, w, SOMD_ERR_UNSAFE_STATE); return; }
    write_presets(d, w);
    w[A_PAD1_EDGE] |= 4;
    d->mphase = 2; d->mtimer = 0;
    break;
  case 3:      /* confirm on the entry */
    if (d->mtimer < 3) d->mpad = PAD_B;
    break;
  case 10:     /* close: leave sub-levels with Y, pages with Y, B or SELECT in turn (the controller page only leaves with SELECT/START), then ask the ring loop to leave */
  case 11:
    if (ring_top(w) && d->mphase == 10) { w[R_ACTIVE] = 0; d->mphase = 11; d->mtimer = 0; break; }
    if (ring_idle(w) && u8(w, R_ACTIVE) == 1 && d->mphase == 10) {
      if ((d->mtimer % 24) < 3) d->mpad = PAD_Y;
    } else if (d->mphase == 10 && ring_open(w) && u8(w, R_ACTIVE) == 0) {
      if ((d->mtimer % 40) < 6) { static const uint16_t cyc[3] = {PAD_Y, PAD_B, PAD_SELECT}; d->mpad = cyc[(d->mtimer / 40) % 3]; }
    }
    break;
  default: break;
  }
  d->mtimer++;
}

/* after the frame ran */
static void menu_post(struct somd *d, uint8_t *w) {
  d->mtotal++;
  d->frames = d->mtotal;
  if (d->mtotal > 900) { mfail(d, w, SOMD_ERR_TIMEOUT); return; }
  int cat_want = d->mkind == SOMD_MENU_RING ? d->marg : d->mkind == SOMD_MENU_SPIRIT ? 1 : 2;
  switch (d->mphase) {
  case 0: if (d->mtimer > 90) mfail(d, w, SOMD_ERR_BAD_ARGUMENT); break;
  case 2:
    if (ring_idle(w) && u8(w, R_ACTIVE) == 1 && u8(w, R_CAT) == (unsigned)cat_want && u8(w, R_OWNER) == d->mhero) {
      if (d->mkind == SOMD_MENU_RING) menu_done(d, w, 0);
      else { d->mphase = 3; d->mtimer = 0; }
    } else if (d->mtimer > 150) mfail(d, w, SOMD_ERR_TIMEOUT);
    break;
  case 3:
    if (d->mtimer >= 4) { d->mphase = 4; d->mtimer = 0; }
    break;
  case 4:
    if (d->mkind == SOMD_MENU_SPIRIT) {
      if (ring_idle(w) && (u8(w, R_FLAGS) & 0x80) && u8(w, R_COUNT) == 3) menu_done(d, w, 0);
    } else {   /* page: entry 0 is a sub-ring, 1 a ring mode, the others leave the ring with the mailbox set */
      int a = d->marg, done = 0;
      if (a == 0) done = ring_idle(w) && u8(w, R_CAT) == 5;
      else if (a == 1) {
        done = ring_idle(w) && (u8(w, R_FLAGS) & 0x20) && d->mtimer >= 6;
        if (!done && d->mtimer >= 30 && ring_idle(w) && u8(w, R_ACTIVE) == 1 && !(u8(w, R_FLAGS) & 0x20)) { mfail(d, w, SOMD_ERR_NO_TARGET); return; }   /* nothing to target: the ring stays as it was */
      }
      else done = ring_open(w) && u8(w, R_ACTIVE) == 0 && u8(w, R_MAIL) == 0xC0 && d->mtimer >= 64;
      if (done) menu_done(d, w, 0);
    }
    if (d->state == SOMD_RUNNING && d->mtimer > 200) mfail(d, w, SOMD_ERR_TIMEOUT);
    break;
  case 11:
    if (field_ready(w)) {
      if (d->kind == 6) menu_done(d, w, 1);
      else { w[A_PAD1_EDGE] &= (uint8_t)~6u; d->mphase = 0; d->mtimer = 0; }   /* closed: now open the target */
    } else if (d->mtimer > 200) mfail(d, w, SOMD_ERR_TIMEOUT);
    break;
  case 10:
    if (field_ready(w)) {
      if (d->kind == 6) menu_done(d, w, 1);
      else { w[A_PAD1_EDGE] &= (uint8_t)~6u; d->mphase = 0; d->mtimer = 0; }
    } else if (d->mtimer > 600) mfail(d, w, SOMD_ERR_TIMEOUT);
    break;
  default: break;
  }
}

/* ------------------------------------------------------------------ mini-map */
void somd_minimap_point(const struct somd_actor *a, int map_w, int map_h, int *px, int *py) {
  int x = a->sx > 0x7FFF ? (int)a->sx - 0x10000 : (int)a->sx, y = a->sy > 0x7FFF ? (int)a->sy - 0x10000 : (int)a->sy;
  *px = x * map_w / 256; *py = y * map_h / 224;
}

int somd_pick(const struct somd_targets *t, int tx, int ty, int mw, int mh, int radius, struct somd_target *out) {
  int best = -1; long bd = (long)radius * radius + 1;
  for (int s = 0; s < 6; s++) {
    const struct somd_actor *a = &t->actor[s];
    if (!a->valid) continue;
    int px, py; somd_minimap_point(a, mw, mh, &px, &py);
    long d = (long)(px - tx) * (px - tx) + (long)(py - ty) * (py - ty);
    if (d < bd) { bd = d; best = s; }
  }
  if (best < 0) return 0;
  out->kind = SOMD_T_ONE; out->slot = (uint8_t)best;
  return 1;
}

int somd_group_card(const struct somd_targets *t) {
  if (t->cls == SOMD_C_ALLY_ONE_OR_ALL && t->n_allies_valid > 1) return 1;
  if (t->cls == SOMD_C_ENEMY_ONE_OR_ALL && t->n_enemies_valid > 1) return 2;
  return 0;
}
