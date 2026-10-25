/* Pure unit tests of the decoder and index helpers on synthetic WRAM images (no emulator). */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "som_companion.h"

static uint8_t w[SOM_WRAM_SIZE];

/* Minimal model of the weapon ring (measured on vanilla and Ancient Cave): owned types are listed 1..7 then 0, the
 * cursor byte starts at the held rank, the first direction press only resets it to 0 and the ring position is
 * (cursor + held rank) mod count.  Returns the weapon type held after the macro, or -1 if it did not finish. */
static int sim_equip(const uint8_t *base, int held, int wanted, int *taps_out) {
  static uint8_t v[SOM_WRAM_SIZE];
  static const uint8_t order[8] = {1, 2, 3, 4, 5, 6, 7, 0};
  struct som_state st; struct som_macro m; struct som_cmd c = {SOM_CMD_EQUIP_WEAPON, 0, (uint8_t)wanted, 0};
  memcpy(v, base, SOM_WRAM_SIZE);
  v[0xE1E4] = (uint8_t)held;
  for (int t = 0; t < 8; t++) v[0xCC54 + t] = 1;               /* every weapon owned */
  som_read_state(v, &st);
  int ring[8], n = 0, held_rank = -1, pos = 0, first = 1, taps = 0;
  for (int k = 0; k < 8; k++) { ring[n] = order[k]; if (order[k] == held) held_rank = n; n++; }
  uint16_t prev = 0;
  if (!som_macro_start(&m, v, &c)) return m.result == SOM_DONE ? held : -1;
  for (int f = 0; f < 4000 && som_macro_busy(&m); f++) {
    uint16_t pad = som_macro_step(&m, v), edge = (uint16_t)(pad & ~prev);
    prev = pad;
    if (v[0x7D] && (edge & (1u << SOM_PAD_UP))) {
      v[0x1809] = (uint8_t)((v[0x1809] + 1) & 3);
      v[0x180B] = v[0x1809] == SOM_CAT_WEAPON ? (uint8_t)n : 3;
      v[0x180A] = v[0x1809] == SOM_CAT_WEAPON ? (uint8_t)held_rank : 0;
      first = 1;
    }
    if (v[0x7D] && (edge & ((1u << SOM_PAD_LEFT) | (1u << SOM_PAD_RIGHT)))) {
      int d = (edge & (1u << SOM_PAD_LEFT)) ? 1 : n - 1;
      taps++;
      v[0x180A] = first ? 0 : (uint8_t)((v[0x180A] + d) % n);
      first = 0;
    }
    if (v[0x7D] && v[0x1809] == SOM_CAT_WEAPON && (edge & (1u << SOM_PAD_B))) {
      pos = (v[0x180A] + held_rank) % n;
      v[0xE1E4] = (uint8_t)ring[pos];
      v[0x7D] = 0;
    }
    if (v[0x7D] && (edge & ((1u << SOM_PAD_A) | (1u << SOM_PAD_Y)))) v[0x7D] = 0;
    if (!v[0x7D] && (edge & (1u << SOM_PAD_Y)) && m.result == SOM_RUNNING) {
      v[0x7D] = 1; v[0x1808] = 0; v[0x1809] = 0; v[0x180A] = 0; v[0x180B] = 3; v[0x1812] = 0; first = 1;
    }
  }
  if (taps_out) *taps_out = taps;
  return m.result == SOM_DONE ? v[0xE1E4] : -1;
}


/* Model of the ring for menu commands (docs/companion-ux-20261017.md "verified facts"): Y opens at the boy and closes, X moves the owner
 * 0 -> 1 -> 2 -> 1, UP cycles weapon -> item -> magic -> config (the boy has no magic), LEFT adds one to the cursor, B on config opens a screen.
 * Returns 1 when the macro ended DONE with the expected owner, category and (for entries) the screen opened. */
static int sim_menu(const uint8_t *base, int owner0, int hero, int cat, int entry, int *taps_out) {
  static uint8_t v[SOM_WRAM_SIZE];
  static const int order[4] = {SOM_CAT_WEAPON, SOM_CAT_ITEM, SOM_CAT_MAGIC, SOM_CAT_CONFIG};
  struct som_macro m; struct som_cmd c = {SOM_CMD_OPEN_MENU, (uint8_t)hero, (uint8_t)cat, (uint8_t)entry};
  memcpy(v, base, SOM_WRAM_SIZE);
  v[0xE391] = 0; v[0xE582] = 20; v[0xE584] = 40; v[0xE400] = 1;           /* all three heroes alive */
  int opened = 0, taps = 0, ci = 0;
  if (owner0 >= 0) { v[0x7D] = 1; v[0x1808] = (uint8_t)owner0; v[0x1809] = SOM_CAT_WEAPON; v[0x180A] = 0; v[0x180B] = 4; }
  if (!som_macro_start(&m, v, &c)) return -1;
  uint16_t prev = 0;
  for (int f = 0; f < 4000 && som_macro_busy(&m); f++) {
    uint16_t pad = som_macro_step(&m, v), edge = (uint16_t)(pad & ~prev);
    prev = pad;
    if (edge) taps++;
    if (edge & (1u << SOM_PAD_Y)) {
      if (!v[0x7D]) { v[0x7D] = 1; v[0x1808] = 0; v[0x1809] = SOM_CAT_WEAPON; v[0x180A] = 0; v[0x180B] = 4; ci = 0; }
      else v[0x7D] = 0;
    }
    if (v[0x7D] && (edge & (1u << SOM_PAD_X))) v[0x1808] = v[0x1808] == 0 ? 1 : v[0x1808] == 1 ? 2 : 1;
    if (v[0x7D] && (edge & (1u << SOM_PAD_UP))) {
      do ci = (ci + 1) & 3; while (order[ci] == SOM_CAT_MAGIC && v[0x1808] == 0);
      v[0x1809] = (uint8_t)order[ci]; v[0x180B] = v[0x1809] == SOM_CAT_CONFIG ? 7 : 3; v[0x180A] = 0;
    }
    if (v[0x7D] && v[0x1809] == SOM_CAT_CONFIG && (edge & (1u << SOM_PAD_LEFT))) v[0x180A] = (uint8_t)((v[0x180A] + 1) % 7);
    if (v[0x7D] && v[0x1809] == SOM_CAT_CONFIG && (edge & (1u << SOM_PAD_RIGHT))) v[0x180A] = (uint8_t)((v[0x180A] + 6) % 7);
    if (v[0x7D] && v[0x1809] == SOM_CAT_CONFIG && (edge & (1u << SOM_PAD_B))) opened = 1;
  }
  if (taps_out) *taps_out = taps;
  if (m.result != SOM_DONE || !v[0x7D] || v[0x1808] != hero) return 0;
  if (cat != 0xFF && v[0x1809] != cat) return 0;
  if (entry != 0xFF) return opened && v[0x180A] == entry;
  return 1;
}

int main(void) {
  struct som_state s;
  memset(w, 0, sizeof w);
  som_read_state(w, &s);
  assert(s.mode == SOM_MODE_NO_GAME && !s.safe_to_inject);

  w[0xE000] = 1; w[0xE200] = 1;
  w[0xE182] = 50; w[0xE184] = 50; w[0xE186] = 4; w[0xE187] = 9; w[0xE181] = 6;
  w[0xE1E4] = 2; w[0xE1E8] = 20; w[0xE1ED] = 0x22;
  w[0xE1C0] = 0x38; w[0xE1C3] = 0x01;
  w[0xE182 + 0x200] = 10; w[0xE184 + 0x200] = 45; w[0xE391] = 0x80;
  w[0xE3C4] = 0x12; w[0xE3C8] = 0x3F; w[0xE3CD] = 0x01; w[0xE3CE] = 0x3F; w[0xE3CF] = 0x01;
  w[0xE4] = 1; w[0xE6] = 0x0F; w[0xD9] = 1; w[0xDC] = 131;
  memset(w + 0xCC48, 0xFF, 12); w[0xCC48] = 0x60; w[0xCC4A] = 0x45;
  memset(w + 0xCC54, 0xFF, 8); w[0xCC55] = 0x40; w[0xCC56] = 2; w[0xCC5B] = 3;
  w[0xCC6A] = 0x34; w[0xCC6B] = 0x12;
  som_read_state(w, &s);
  assert(s.mode == SOM_MODE_FIELD && s.safe_to_inject && s.map_id == 131 && s.pad_hero == 0);
  assert(s.hero[0].level == 7 && s.hero[0].hp == 50 && s.hero[0].mp == 4 && s.hero[0].mp_max == 9);
  assert(s.hero[0].recharge_pct == 60);                    /* 0x22 = 34 -> 100 - 34 * 100 / 85 */
  assert(s.hero[0].weapon_level[0] == 3 && s.hero[0].weapon_level[1] == 8 && s.hero[0].weapon_level[7] == 1);
  assert(s.hero[1].present && !s.hero[1].alive && !s.hero[2].present);
  assert(s.gold == 0x1234);
  assert(s.item[0].id == 0 && s.item[0].qty == 3 && s.item[1].qty == 0 && s.item[2].id == 5 && s.item[2].qty == 2);
  assert(som_weapon_ring_index(&s, 1) == 0 && som_weapon_ring_index(&s, 2) == 1 && som_weapon_ring_index(&s, 7) == 2);
  assert(som_weapon_ring_index(&s, 3) == -1);
  assert(som_item_ring_index(&s, 0) == 0 && som_item_ring_index(&s, 2) == 1 && som_item_ring_index(&s, 1) == -1);
  assert(s.hero[1].spirit_level[0] == 1 && s.hero[1].spirit_level[1] == 2);
  assert(som_spirit_ring_index(&s, 1, 0) == 0 && som_spirit_ring_index(&s, 1, 5) == 1);
  assert(som_spirit_ring_index(&s, 1, 6) == -1);           /* byte 6 is never listed for the girl */
  assert(som_spirit_ring_index(&s, 0, 0) == -1);           /* the boy has no magic ring */
  assert(som_spirit_ring_index(&s, 1, 7) == 2);

  /* modes */
  w[0x7D] = 1; som_read_state(w, &s); assert(s.mode == SOM_MODE_RING);
  w[0xD0] = 0x81; som_read_state(w, &s); assert(s.mode == SOM_MODE_EVENT && !s.safe_to_inject);
  w[0xD0] = 0; w[0xE6] = 0x08; som_read_state(w, &s); assert(s.mode == SOM_MODE_TRANSITION);
  w[0xE6] = 0x0F; w[0x7D] = 0;

  /* refusals */
  struct som_macro m; struct som_cmd c = {SOM_CMD_USE_ITEM, 0, 1, 0};
  som_macro_init(&m);
  assert(!som_macro_start(&m, w, &c) && m.result == SOM_FAIL_BAD_ARGUMENT);   /* record 1 empty */
  c.arg = 0; w[0xD0] = 1;
  assert(!som_macro_start(&m, w, &c) && m.result == SOM_FAIL_UNSAFE);
  w[0xD0] = 0;
  c.hero = 1;
  assert(!som_macro_start(&m, w, &c) && m.result == SOM_FAIL_BAD_ARGUMENT);   /* hero 1 is dead */
  c.hero = 0;
  assert(som_macro_start(&m, w, &c) && som_macro_busy(&m));
  /* first press: Y for 3 frames */
  assert(som_macro_step(&m, w) == (1u << SOM_PAD_Y));
  assert(som_macro_step(&m, w) == (1u << SOM_PAD_Y));
  assert(som_macro_step(&m, w) == (1u << SOM_PAD_Y));
  assert(som_macro_step(&m, w) == 0);
  som_macro_abort(&m);
  for (int i = 0; i < 2000 && som_macro_busy(&m); i++) som_macro_step(&m, w);
  assert(!som_macro_busy(&m) && m.result == SOM_ABORTED);

  /* ring order: types 1..7 then 0 */
  struct som_state all; memcpy(&all, &s, sizeof all);
  for (int t = 0; t < 8; t++) all.weapon_slot[t] = 1;
  static const int rank_of[8] = {7, 0, 1, 2, 3, 4, 5, 6};
  for (int t = 0; t < 8; t++) assert(som_weapon_ring_index(&all, t) == rank_of[t]);
  all.weapon_slot[3] = 0xFF;                                 /* a gap shifts only the later ranks */
  assert(som_weapon_ring_index(&all, 4) == 2 && som_weapon_ring_index(&all, 0) == 6 && som_weapon_ring_index(&all, 3) == -1);

  /* equip matrix: every (held, wanted) pair of the 8 x 8 */
  for (int held = 0; held < 8; held++)
    for (int want = 0; want < 8; want++) {
      int taps = 0, got = sim_equip(w, held, want, &taps);
      if (got != want) { printf("equip held %d wanted %d -> %d\n", held, want, got); return 1; }
      if (held == want) assert(taps == 0);                    /* already held: no ring opened */
    }

  /* menu commands: every start state (closed, or open at each owner) x every hero x category, then every config entry */
  for (int own = -1; own < 3; own++)
    for (int hero = 0; hero < 3; hero++)
      for (int cat = 0; cat < 4; cat++) {
        if (cat == SOM_CAT_MAGIC && hero == 0) continue;
        int r = sim_menu(w, own, hero, cat, 0xFF, NULL);
        if (r != 1) { printf("menu owner %d hero %d cat %d -> %d\n", own, hero, cat, r); return 1; }
      }
  for (int own = -1; own < 3; own++)
    for (int hero = 0; hero < 3; hero++)
      for (int e = 0; e < 7; e++) {
        int r = sim_menu(w, own, hero, SOM_CAT_CONFIG, e, NULL);
        if (r != 1) { printf("submenu owner %d hero %d entry %d -> %d\n", own, hero, e, r); return 1; }
      }
  assert(sim_menu(w, -1, 1, 0xFF, 0xFF, NULL) == 1);        /* avatar: just the owner */
  assert(sim_menu(w, -1, 0, SOM_CAT_MAGIC, 0xFF, NULL) == -1);   /* the boy has no magic ring */
  /* A disabling status on the controlled hero (sleep 0x10 here) keeps the game from opening the ring: the macro waits
   * for it to end instead of timing out, and gives up with a clear note after 90 s. */
  for (int clear_at = 0; clear_at <= 5000; clear_at += 5000) {
    static uint8_t v[SOM_WRAM_SIZE]; struct som_macro m; struct som_cmd c = {SOM_CMD_OPEN_MENU, 0, 0xFF, 0xFF};
    memcpy(v, w, SOM_WRAM_SIZE); v[0xE190] = 0x10;
    if (!som_macro_start(&m, v, &c)) { printf("start refused: %s\n", m.note); return 1; }
    uint16_t prev = 0; int f;
    for (f = 0; f < 7000 && som_macro_busy(&m); f++) {
      if (clear_at && f == clear_at) v[0xE190] = 0;
      uint16_t pad = som_macro_step(&m, v), edge = (uint16_t)(pad & ~prev); prev = pad;
      if (!v[0x7D] && (edge & (1u << SOM_PAD_Y)) && !(v[0xE190] & 0x4173)) { v[0x7D] = 1; v[0x1808] = 0; v[0x1809] = 0; v[0x180A] = 0; v[0x180B] = 3; v[0x1812] = 0; }
    }
    if (clear_at) assert(m.result == SOM_DONE);
    else assert(m.result == SOM_FAIL_TIMEOUT && strstr(m.note, "disabled"));
  }
  {   /* the panel highlight follows the $CC54 holder mark: exactly one hero per owned type, none for a hero without a weapon */
    struct som_state w;
    memset(&w, 0, sizeof w);
    for (int t = 0; t < 8; t++) w.weapon_slot[t] = 0x08;
    w.weapon_slot[1] = 0x48; w.weapon_slot[4] = 0x88; w.weapon_slot[6] = 0xC8; w.weapon_slot[7] = 0xFF;
    for (int h = 0; h < 3; h++) for (int t = 0; t < 8; t++) {
      int want = (h == 0 && t == 1) || (h == 1 && t == 4) || (h == 2 && t == 6);
      assert(som_weapon_held_by(&w, h, t) == want);
    }
    w.hero[1].weapon_type = 0;   /* a hero holding nothing reports type 0: the glove must not look held */
    assert(!som_weapon_held_by(&w, 1, 0));
  }
  puts("som_companion unit tests passed");
  return 0;
}
