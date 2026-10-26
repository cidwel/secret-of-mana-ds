/* See som_companion.h.  All addresses are offsets into the 128 KiB WRAM image ($7E:xxxx = xxxx, $7F:xxxx = 0x10000+xxxx). */
#include "som_companion.h"
#include <string.h>

/* ---- WRAM map (docs/wram-companion-map.md) ---- */
#define HERO_BASE 0xE000u
#define HERO_STRIDE 0x200u
#define H_ACTIVE 0x000
#define H_ID 0x1E7
#define H_LEVEL 0x181
#define H_HP 0x182
#define H_HPMAX 0x184
#define H_MP 0x186
#define H_MPMAX 0x187
#define H_STATUS 0x190
#define H_PTIMER 0x1B6
#define H_WLEVELS 0x1C0
#define H_SLEVELS 0x1C4
#define H_SMASK 0x1C8
#define H_WTYPE 0x1E4
#define H_WROW 0x1E8
#define H_RECHARGE 0x1ED
#define H_SPELL_ID 0x170

#define A_EVENT 0xD0
#define A_PADMASK 0xD9
#define A_MAPID 0xDC
#define A_FADE_A 0xE4
#define A_FADE_B 0xE6
#define A_RING_OPEN 0x7D
#define A_RING_OWNER 0x1808
#define A_RING_CAT 0x1809
#define A_RING_CURSOR 0x180A
#define A_RING_COUNT 0x180B
#define A_RING_BUSY 0x1812
#define A_WEAPONS 0xCC54
#define A_ITEMS 0xCC48
#define A_GOLD 0xCC6A

static unsigned u8(const uint8_t *w, unsigned a) { return w[a]; }
static unsigned u16(const uint8_t *w, unsigned a) { return w[a] | (w[a + 1] << 8); }

/* What the player sees instead of "UNSAFE STATE" when a command is refused: why the game cannot take it now. */
const char *som_unsafe_reason(const uint8_t *w) {
  struct som_state s;
  som_read_state(w, &s);
  switch (s.mode) {
  case SOM_MODE_NO_GAME: return "NO GAME";
  case SOM_MODE_TRANSITION: return "WAIT";
  case SOM_MODE_EVENT: return "NOT IN EVENTS";
  case SOM_MODE_RING: return "CLOSE THE RING";
  case SOM_MODE_FIELD: break;
  }
  return s.pad_hero == 0xFF ? "WAIT" : "HERO IS DOWN";
}

const char *som_mode_name(enum som_mode m) {
  switch (m) {
  case SOM_MODE_NO_GAME: return "no-game";
  case SOM_MODE_TRANSITION: return "transition";
  case SOM_MODE_EVENT: return "event";
  case SOM_MODE_RING: return "ring";
  case SOM_MODE_FIELD: return "field";
  }
  return "?";
}

void som_read_state(const uint8_t *w, struct som_state *s) {
  memset(s, 0, sizeof *s);
  s->pad_hero = 0xFF;
  for (int i = 0; i < SOM_PARTY; i++) {
    unsigned b = HERO_BASE + HERO_STRIDE * i;
    struct som_hero *h = &s->hero[i];
    h->present = u8(w, b + H_ACTIVE) == 1;
    h->id = (uint8_t)u8(w, b + H_ID);
    h->level = (uint8_t)(u8(w, b + 0x181) + 1);
    h->hp = (uint16_t)u16(w, b + H_HP);
    h->hp_max = (uint16_t)u16(w, b + H_HPMAX);
    h->mp = (uint8_t)u8(w, b + H_MP);
    h->mp_max = (uint8_t)u8(w, b + H_MPMAX);
    h->status = (uint16_t)u16(w, b + H_STATUS);
    h->alive = h->present && !(h->status & 0x8000);
    h->poison_timer = (uint8_t)u8(w, b + H_PTIMER);
    h->recharge = (uint8_t)u8(w, b + H_RECHARGE);
    h->recharge_pct = h->recharge >= 85 ? 0 : (uint8_t)(100 - h->recharge * 100 / 85);
    h->weapon_type = (uint8_t)u8(w, b + H_WTYPE);
    h->weapon_row = (uint8_t)u8(w, b + H_WROW);
    for (int t = 0; t < SOM_WEAPON_TYPES; t++) {
      unsigned v = u8(w, b + H_WLEVELS + t / 2);
      h->weapon_level[t] = (uint8_t)((t & 1) ? (v & 15) : (v >> 4));
      h->weapon_prog[t] = (uint8_t)u8(w, b + 0x1D0 + (unsigned)t);
    }
    h->str = (uint8_t)u8(w, b + 0x188);
    for (int e = 0; e < SOM_SPIRITS; e++) {
      unsigned v = u8(w, b + H_SLEVELS + e / 2);
      h->spirit_level[e] = (uint8_t)((e & 1) ? (v & 15) : (v >> 4));
      h->spirit_mask[e] = (uint8_t)u8(w, b + H_SMASK + e);
    }
  }
  for (int t = 0; t < SOM_WEAPON_TYPES; t++) s->weapon_slot[t] = (uint8_t)u8(w, A_WEAPONS + t);
  for (int i = 0; i < SOM_ITEM_RECORDS; i++) {
    unsigned v = u8(w, A_ITEMS + i);
    if (v == 0xFF) continue;
    s->item[i].id = (uint8_t)(v & 31);
    s->item[i].qty = (uint8_t)(v >> 5);
  }
  s->gold = u8(w, A_GOLD) | (u8(w, A_GOLD + 1) << 8) | (u8(w, A_GOLD + 2) << 16);
  s->map_id = (uint16_t)u16(w, A_MAPID);
  unsigned pm = u8(w, A_PADMASK);
  for (int i = 0; i < SOM_PARTY; i++) if (pm & (1u << i)) { s->pad_hero = (uint8_t)i; break; }

  s->ring.open = u8(w, A_RING_OPEN) == 1;
  s->ring.owner = (uint8_t)u8(w, A_RING_OWNER);
  s->ring.category = (uint8_t)u8(w, A_RING_CAT);
  s->ring.cursor = (uint8_t)u8(w, A_RING_CURSOR);
  s->ring.count = (uint8_t)u8(w, A_RING_COUNT);
  s->ring.ready = s->ring.open && u8(w, A_RING_BUSY) == 0 && s->ring.category < 4 && s->ring.owner < SOM_PARTY &&
                  s->ring.count != 0x55;

  s->valid = s->hero[0].present;
  if (!s->valid) s->mode = SOM_MODE_NO_GAME;
  else if (u8(w, A_FADE_B) != 0x0F || u8(w, A_FADE_A) != 1) s->mode = SOM_MODE_TRANSITION;
  else if (u8(w, A_EVENT) != 0) s->mode = SOM_MODE_EVENT;
  else if (s->ring.open) s->mode = SOM_MODE_RING;
  else s->mode = SOM_MODE_FIELD;
  s->safe_to_inject = s->mode == SOM_MODE_FIELD && s->pad_hero != 0xFF && s->hero[s->pad_hero].alive;
}

/* ---- ring entry indices ---- */
/* The weapon ring lists owned types in the order 1..7 and then 0 (the glove comes last). */
int som_weapon_ring_index(const struct som_state *s, int type) {
  static const uint8_t order[SOM_WEAPON_TYPES] = {1, 2, 3, 4, 5, 6, 7, 0};
  if (type < 0 || type >= SOM_WEAPON_TYPES || s->weapon_slot[type] == 0xFF) return -1;
  int idx = 0;
  for (int k = 0; k < SOM_WEAPON_TYPES; k++) {
    if (order[k] == type) return idx;
    if (s->weapon_slot[order[k]] != 0xFF) idx++;
  }
  return -1;
}

int som_item_ring_index(const struct som_state *s, int rec) {
  if (rec < 0 || rec >= SOM_ITEM_RECORDS || s->item[rec].qty == 0) return -1;
  int idx = 0;
  for (int r = 0; r < rec; r++) if (s->item[r].qty) idx++;
  return idx;
}

/* The magic ring lists the spirits whose mask byte is non-zero, in byte order.  One byte is never listed: byte 6 for
 * hero 1 (girl) and byte 7 for hero 2 (sprite); hero 0 (boy) has no magic ring. */
int som_spirit_ring_index(const struct som_state *s, int hero, int spirit) {
  if (hero < 1 || hero >= SOM_PARTY || spirit < 0 || spirit >= SOM_SPIRITS) return -1;
  int skip = hero == 1 ? 6 : 7;
  if (spirit == skip) return -1;
  const struct som_hero *h = &s->hero[hero];
  if (!h->spirit_mask[spirit]) return -1;
  int idx = 0;
  for (int e = 0; e < spirit; e++) if (e != skip && h->spirit_mask[e]) idx++;
  return idx;
}

/* ---- macro state machine ---- */
enum { P_IDLE, P_OPEN, P_READY, P_OWNER, P_CAT, P_CURSOR, P_CONFIRM, P_SUBRING, P_CLOSE, P_VERIFY, P_RECOVER, P_FINISHED, P_REOPEN };

#define TAP_FRAMES 3
#define SETTLE_FRAMES 12
#define TIMEOUT_OPEN 150
#define TIMEOUT_STEP 120
#define VERIFY_FRAMES 330
/* Status bits for which the game itself refuses the ring (measured on the vanilla ROM: poison/paralysis 0x01, 0x02,
 * sleep 0x10, 0x20, 0x40, balloon 0x100, 0x4000).  They end by themselves; the macro waits instead of timing out. */
#define RING_BLOCK_STATUS 0x4173
#define TIMEOUT_BLOCKED 5400

const char *som_result_name(enum som_result r) {
  static const char *n[] = {"running", "done", "done-unverified", "fail-unsafe", "fail-bad-argument", "fail-timeout",
                            "fail-unavailable", "aborted"};
  return n[r];
}

void som_macro_init(struct som_macro *m) { memset(m, 0, sizeof *m); m->phase = P_IDLE; }
int som_macro_busy(const struct som_macro *m) { return m->phase != P_IDLE && m->phase != P_FINISHED; }

static void fail(struct som_macro *m, enum som_result r, const char *note) {
  m->result = r;
  strncpy(m->note, note, sizeof m->note - 1);
  /* recovery: close the ring if it is open before reporting; the final result stays r */
  m->phase = P_RECOVER; m->timer = 0; m->wait = 0; m->taps = 0; m->hold = 0;
}

int som_macro_start(struct som_macro *m, const uint8_t *w, const struct som_cmd *c) {
  struct som_state s;
  som_read_state(w, &s);
  memset(m, 0, sizeof *m);
  m->cmd = *c;
  m->phase = P_FINISHED;
  int menu = c->kind == SOM_CMD_OPEN_MENU;
  if (menu ? !((s.mode == SOM_MODE_FIELD && s.safe_to_inject) || s.mode == SOM_MODE_RING) : (s.mode != SOM_MODE_FIELD || !s.safe_to_inject)) { m->result = SOM_FAIL_UNSAFE; strcpy(m->note, som_mode_name(s.mode)); return 0; }
  if (c->hero >= SOM_PARTY || !s.hero[c->hero].alive) { m->result = SOM_FAIL_BAD_ARGUMENT; strcpy(m->note, "hero absent or dead"); return 0; }
  switch (c->kind) {
  case SOM_CMD_EQUIP_WEAPON:
    m->want_cat = SOM_CAT_WEAPON; m->want_index = (uint8_t)som_weapon_ring_index(&s, c->arg);
    if (som_weapon_ring_index(&s, c->arg) < 0) { m->result = SOM_FAIL_BAD_ARGUMENT; strcpy(m->note, "weapon not owned"); return 0; }
    if (s.hero[c->hero].weapon_type == c->arg) {   /* nothing to do: opening the ring would never close it */
      m->result = SOM_DONE; strcpy(m->note, "already held"); return 1;
    }
    break;
  case SOM_CMD_USE_ITEM:
    m->want_cat = SOM_CAT_ITEM;
    if (som_item_ring_index(&s, c->arg) < 0) { m->result = SOM_FAIL_BAD_ARGUMENT; strcpy(m->note, "empty item record"); return 0; }
    m->want_index = (uint8_t)som_item_ring_index(&s, c->arg);
    break;
  case SOM_CMD_CAST_MAGIC:
    m->want_cat = SOM_CAT_MAGIC;
    if (som_spirit_ring_index(&s, c->hero, c->arg) < 0 || c->spell > 2 || s.hero[c->hero].mp == 0) {
      m->result = SOM_FAIL_BAD_ARGUMENT; strcpy(m->note, "spirit not listed, bad spell slot or no MP"); return 0; }
    m->want_index = (uint8_t)som_spirit_ring_index(&s, c->hero, c->arg);
    break;
  case SOM_CMD_OPEN_MENU:
    m->want_cat = c->arg; m->want_index = c->spell;
    if (c->arg != 0xFF && c->arg > SOM_CAT_WEAPON) { m->result = SOM_FAIL_BAD_ARGUMENT; strcpy(m->note, "bad ring category"); return 0; }
    if (c->arg == SOM_CAT_MAGIC && c->hero == 0) { m->result = SOM_FAIL_BAD_ARGUMENT; strcpy(m->note, "the boy has no magic ring"); return 0; }
    break;
  default: m->result = SOM_FAIL_BAD_ARGUMENT; strcpy(m->note, "no command"); return 0;
  }
  m->before = s;
  m->result = SOM_RUNNING;
  m->phase = s.ring.open ? P_READY : P_OPEN; m->timer = 0; m->wait = 0; m->taps = 0; m->hold = 0;
  return 1;
}

void som_macro_close_ring(struct som_macro *m) {
  memset(m, 0, sizeof *m);
  m->result = SOM_DONE; strcpy(m->note, "ring closed");
  m->phase = P_RECOVER;
}

void som_macro_abort(struct som_macro *m) {
  if (m->phase == P_IDLE || m->phase == P_FINISHED) return;
  if (m->result == SOM_RUNNING) { m->result = SOM_ABORTED; strcpy(m->note, "aborted by caller"); }
  m->phase = P_RECOVER; m->timer = 0; m->wait = 0; m->taps = 0; m->hold = 0;
}

/* Taps are "press for TAP_FRAMES frames then release for SETTLE frames".  Returns the mask to apply this frame. */
static uint16_t tap(struct som_macro *m, int button) {
  m->hold = TAP_FRAMES;
  return (uint16_t)(1u << button);
}

static int verify_effect(const struct som_macro *m, const struct som_state *now, const uint8_t *w) {
  const struct som_cmd *c = &m->cmd;
  switch (c->kind) {
  case SOM_CMD_EQUIP_WEAPON: return now->hero[c->hero].weapon_type == c->arg;
  case SOM_CMD_USE_ITEM: {
    unsigned before = m->before.item[c->arg].qty;
    return now->item[c->arg].qty != before || now->item[c->arg].id != m->before.item[c->arg].id;
  }
  case SOM_CMD_CAST_MAGIC:
    return now->hero[c->hero].mp < m->before.hero[c->hero].mp ||
           u16(w, HERO_BASE + HERO_STRIDE * c->hero + H_SPELL_ID) != 0;
  default: return 0;
  }
}

uint16_t som_macro_step(struct som_macro *m, const uint8_t *w) {
  if (m->phase == P_IDLE || m->phase == P_FINISHED) return 0;
  struct som_state s;
  som_read_state(w, &s);
  m->frames++;

  if (m->hold > 0) {            /* button currently held */
    m->hold--;
    return m->pad;
  }
  m->pad = 0;
  if (m->wait > 0) { m->wait--; return 0; }

  /* leaving field/ring mode (event, fade, game over) aborts, except during recovery */
  if (m->phase != P_RECOVER && m->phase != P_VERIFY && s.mode != SOM_MODE_FIELD && s.mode != SOM_MODE_RING) {
    fail(m, SOM_FAIL_UNSAFE, som_mode_name(s.mode));
    return 0;
  }

  m->timer++;
  switch (m->phase) {
  case P_OPEN:
    if (s.ring.open) { m->phase = P_READY; m->timer = 0; break; }
    if (s.pad_hero != 0xFF && (s.hero[s.pad_hero].status & RING_BLOCK_STATUS)) {
      if (++m->blocked > TIMEOUT_BLOCKED) { fail(m, SOM_FAIL_TIMEOUT, "hero disabled by a status"); break; }
      m->timer = 2; break;
    }
    if (m->blocked) { m->blocked = 0; m->timer = 0; break; }
    if (m->timer > TIMEOUT_OPEN) { fail(m, SOM_FAIL_TIMEOUT, "ring did not open"); break; }
    if (m->timer == 1 || m->timer % 60 == 0) { m->pad = tap(m, SOM_PAD_Y); m->hold = TAP_FRAMES - 1; return m->pad; }
    break;
  case P_READY:
    if (m->cmd.kind == SOM_CMD_OPEN_MENU && s.ring.open && (w[0x1803] & 0x80) && m->taps < 4) {   /* close the spell sub-ring first */
      m->taps++; m->timer = 0; m->wait = 45; m->pad = tap(m, SOM_PAD_A); m->hold = TAP_FRAMES - 1; return m->pad;
    }
    if (s.ring.ready) { m->wait = SETTLE_FRAMES; m->phase = P_OWNER; m->timer = 0; m->taps = 0; break; }
    if (m->timer > TIMEOUT_OPEN) fail(m, SOM_FAIL_TIMEOUT, "ring never became idle");
    break;
  case P_OWNER:
    if (!s.ring.ready) { if (m->timer > TIMEOUT_STEP) fail(m, SOM_FAIL_TIMEOUT, "owner step"); break; }
    if (s.ring.owner == m->cmd.hero) { m->phase = P_CAT; m->timer = 0; m->taps = 0; m->sub = 0; break; }
    if (m->cmd.hero == 0) {   /* X never returns to the boy: close the ring and open it again */
      if (m->cmd.kind != SOM_CMD_OPEN_MENU) { fail(m, SOM_FAIL_UNAVAILABLE, "hero cannot own the ring"); break; }
      m->phase = P_REOPEN; m->timer = 0; m->taps = 0; break;
    }
    if (m->taps >= 4) { fail(m, SOM_FAIL_UNAVAILABLE, "hero cannot own the ring"); break; }
    m->taps++; m->timer = 0; m->wait = 80;
    m->pad = tap(m, SOM_PAD_X); m->hold = TAP_FRAMES - 1; return m->pad;
  case P_CAT:
    if (!s.ring.ready) { if (m->timer > TIMEOUT_STEP) fail(m, SOM_FAIL_TIMEOUT, "category step"); break; }
    if (m->cmd.kind == SOM_CMD_OPEN_MENU && m->want_cat == 0xFF) { strcpy(m->note, "ring opened"); m->result = SOM_DONE; m->phase = P_FINISHED; break; }
    if (s.ring.category == m->want_cat) {
      if (m->cmd.kind == SOM_CMD_OPEN_MENU && m->want_index == 0xFF) { strcpy(m->note, "ring opened"); m->result = SOM_DONE; m->phase = P_FINISHED; break; }
      m->phase = P_CURSOR; m->timer = 0; m->taps = 0; m->wait = SETTLE_FRAMES; break; }
    if (m->taps >= 5) { fail(m, SOM_FAIL_UNAVAILABLE, "ring category not reachable"); break; }
    m->taps++; m->timer = 0; m->wait = 40;
    m->pad = tap(m, SOM_PAD_UP); m->hold = TAP_FRAMES - 1; return m->pad;
  case P_CURSOR: {
    unsigned want = m->sub ? m->cmd.spell : m->want_index;
    if (!s.ring.ready) { if (m->timer > TIMEOUT_STEP) fail(m, SOM_FAIL_TIMEOUT, "cursor step"); break; }
    if (m->cmd.kind == SOM_CMD_EQUIP_WEAPON) {
      /* The weapon ring is relative to the held weapon: the cursor byte starts at the held rank, the first direction
       * press only resets it to 0, and the ring position is (cursor + held rank) mod count.  Sync with one tap, then
       * aim at the wanted rank minus the held rank. */
      if (!m->sub) {
        m->sub = 1; m->taps++; m->timer = 0; m->wait = 22;
        m->pad = tap(m, SOM_PAD_LEFT); m->hold = TAP_FRAMES - 1; return m->pad;
      }
      int held = som_weapon_ring_index(&m->before, m->before.hero[m->cmd.hero].weapon_type);
      if (held < 0) held = 0;                 /* held type not in the ring (hero without a weapon): measured as rank 0 */
      want = (m->want_index + s.ring.count - (unsigned)held) % s.ring.count;
    }
    if (s.ring.count == 0 || want >= s.ring.count) { fail(m, SOM_FAIL_BAD_ARGUMENT, "entry beyond ring size"); break; }
    if (s.ring.cursor == want && m->cmd.kind == SOM_CMD_OPEN_MENU) {   /* config entry reached: one B opens its screen */
      if (m->taps >= 90) { strcpy(m->note, "screen opened"); m->result = SOM_DONE; m->phase = P_FINISHED; break; }
      m->taps = 90; m->timer = 0; m->wait = 40; m->pad = tap(m, SOM_PAD_B); m->hold = TAP_FRAMES - 1; return m->pad;
    }
    if (s.ring.cursor == want) { m->phase = P_CONFIRM; m->timer = 0; m->taps = 0; m->wait = SETTLE_FRAMES; break; }
    if (m->taps > 16) { fail(m, SOM_FAIL_TIMEOUT, "cursor not converging"); break; }
    /* LEFT adds one (mod count), RIGHT subtracts one */
    unsigned fwd = (want + s.ring.count - s.ring.cursor) % s.ring.count;
    int btn = fwd <= s.ring.count - fwd ? SOM_PAD_LEFT : SOM_PAD_RIGHT;
    /* wait for the cursor byte to react (it lags the press by about 9 frames) before the next decision */
    m->taps++; m->timer = 0; m->wait = 22;
    m->pad = tap(m, btn); m->hold = TAP_FRAMES - 1; return m->pad;
  }
  case P_CONFIRM:
    if (!s.ring.open) { m->phase = P_VERIFY; m->timer = 0; break; }
    if (!s.ring.ready) { if (m->timer > TIMEOUT_STEP) fail(m, SOM_FAIL_TIMEOUT, "confirm step"); break; }
    if (m->cmd.kind == SOM_CMD_CAST_MAGIC && m->taps == 1 && !m->sub) {
      /* first B went to the spell ring when the entry count changed */
      m->sub = 1; m->phase = P_CURSOR; m->timer = 0; m->taps = 0; m->wait = SETTLE_FRAMES; break;
    }
    if (m->taps >= 6) { fail(m, SOM_FAIL_TIMEOUT, "ring did not close"); break; }
    m->taps++; m->timer = 0; m->wait = (m->cmd.kind == SOM_CMD_CAST_MAGIC && m->sub) ? 70 : 60;
    m->pad = tap(m, SOM_PAD_B); m->hold = TAP_FRAMES - 1; return m->pad;
  case P_VERIFY:
    if (verify_effect(m, &s, w)) { m->result = SOM_DONE; m->phase = P_FINISHED; break; }
    if (s.mode == SOM_MODE_EVENT || s.mode == SOM_MODE_TRANSITION) { m->result = SOM_DONE_UNVERIFIED; m->phase = P_FINISHED; break; }
    if (m->timer > VERIFY_FRAMES) { m->result = SOM_DONE_UNVERIFIED; strcpy(m->note, "effect not seen in WRAM"); m->phase = P_FINISHED; }
    break;
  case P_REOPEN:   /* menu commands for the boy while the ring shows another owner: close it (A for sub-rings, Y), then open it again */
    if (!s.ring.open) { m->phase = P_OPEN; m->timer = 0; m->taps = 0; m->wait = SETTLE_FRAMES; break; }
    if (m->taps >= 10) { fail(m, SOM_FAIL_TIMEOUT, "ring would not close"); break; }
    m->wait = 45;
    m->pad = tap(m, (w[0x1803] & 0x80) ? SOM_PAD_A : SOM_PAD_Y); m->taps++; m->hold = TAP_FRAMES - 1; return m->pad;
  case P_RECOVER:
    /* close the ring: A cancels sub-rings and target selection, Y toggles the top-level ring */
    if (!s.ring.open) { m->phase = P_FINISHED; break; }
    if (m->taps >= 12) { m->phase = P_FINISHED; if (m->result == SOM_ABORTED || m->result == SOM_RUNNING) m->result = SOM_FAIL_TIMEOUT; strcat(m->note, " (ring still open)"); break; }
    m->wait = 45;
    m->pad = tap(m, (m->taps++ % 3 == 2) ? SOM_PAD_Y : SOM_PAD_A); m->hold = TAP_FRAMES - 1; return m->pad;
  default: break;
  }
  return 0;
}
