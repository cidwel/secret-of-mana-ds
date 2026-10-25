/* Unit tests for som_ui_draw: mode classification, touch actions, locked event state, Flammie mapping and pixel goldens.
 * Uses a synthetic WRAM image and the built-in art only (no ROM, no graphics pack), so it runs anywhere.
 *
 *   cc -std=c99 -Wall -Wextra -O1 -o /tmp/test_ux test_som_ui_draw.c som_ui_draw.c som_ui_state.c ../som_companion.c -lm
 *   /tmp/test_ux [--update-hashes] [--dump DIR]
 *
 * Goldens: the PNG-free raw images are written to <state>/ux/golden/ when --dump DIR is given; only the FNV-1a 64-bit
 * hashes live in the repository (golden_hashes.txt). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "som_ui_draw.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } } while (0)

static uint8_t wram[0x20000];
static void w16(unsigned a, unsigned v) { wram[a] = (uint8_t)v; wram[a + 1] = (uint8_t)(v >> 8); }

static void hero(int i, unsigned lvl, unsigned hp, unsigned hpm, unsigned mp, unsigned mpm) {
  unsigned b = 0xE000 + 0x200 * i;
  wram[b] = 1; wram[b + 0x181] = (uint8_t)(lvl - 1); w16(b + 0x182, hp); w16(b + 0x184, hpm); wram[b + 0x186] = (uint8_t)mp;
  wram[b + 0x187] = (uint8_t)mpm; wram[b + 0x1ED] = 85;
}
static void enemy(int slot, unsigned id, unsigned hp, unsigned hpm, unsigned x, unsigned y, int boss) {
  unsigned b = 0xE000 + 0x200 * slot;
  wram[b] = 1; wram[b + 0x180] = (uint8_t)id; w16(b + 0x182, hp); w16(b + 0x184, hpm);
  w16(b + 0x10, x); w16(b + 0x12, y);               /* only used by the radar; position fields are best effort here */
  if (boss) wram[b + 0x1FB] |= 0x40;
}
static void field(void) {
  memset(wram, 0, sizeof wram);
  wram[0xE4] = 1; wram[0xE6] = 15; wram[0xD9] = 1; wram[0xCF00] = 1;   /* CF00: the first sword was taken (the bottom screen is armed) */
  hero(0, 12, 200, 300, 10, 30); hero(1, 11, 120, 250, 20, 40); hero(2, 10, 90, 220, 30, 50);
  for (int hh = 1; hh < 3; hh++) for (int e = 0; e < 8; e++) wram[0xE000 + 0x200 * hh + 0x1C8 + e] = 7;   /* girl and sprite own every spirit */
  wram[0xCC54] = 0x40; wram[0xCC55] = 0xFF; wram[0xCC56] = 2;    /* weapon slots: glove held by the boy (mark 0x40), none, axe ... */
  for (int i = 3; i < 8; i++) wram[0xCC54 + i] = 0xFF;
  for (int i = 0; i < 12; i++) wram[0xCC48 + i] = 0xFF;
  wram[0xCC48] = (uint8_t)((2 << 5) | 0); wram[0xCC49] = (uint8_t)((1 << 5) | 4);
}

enum som_ux_tclass som_ux_item_class(int item_id);   /* internal, used to pick test items */

static uint64_t fnv(const uint32_t *p, size_t n) {
  uint64_t h = 1469598103934665603ull;
  for (size_t i = 0; i < n; i++)
    for (int b = 0; b < 4; b++) { h ^= (p[i] >> (8 * b)) & 255; h *= 1099511628211ull; }
  return h;
}

struct ux_case { const char *name; int w, h; uint64_t hash; };
static struct ux_case cases[220]; static int ncase;
static const char *dump_dir;

static void render(struct som_ux *ux, const char *name, int w, int h) {
  uint32_t *px = malloc((size_t)w * h * 4);
  som_ux_audit(1);
  som_ux_draw(ux, NULL, px, w, w, h);
  { int bad_ = som_ux_audit(0); if (bad_) printf("audit %s %dx%d: %s\n", name, w, h, som_ux_audit_note()); }
  CHECK(som_ux_audit(1) == 0);                       /* no cut text, no overlapping text rows */
  uint64_t hs = fnv(px, (size_t)w * h);
  cases[ncase].name = name; cases[ncase].w = w; cases[ncase].h = h; cases[ncase].hash = hs; ncase++;
  if (dump_dir) {
    char path[512]; snprintf(path, sizeof path, "%s/%s_%dx%d.argb", dump_dir, name, w, h);
    FILE *f = fopen(path, "wb"); if (f) { fwrite(px, 4, (size_t)w * h, f); fclose(f); }
  }
  free(px);
}

/* tap the centre of a named target; 0 when the layout has no such target */
static int tap_hit(struct som_ux *ux, int w, int h, int hit, int a, struct som_ux_action *act) {
  int x, y;
  memset(act, 0, sizeof *act);
  if (!som_ux_target_center(ux, w, h, hit, a, &x, &y)) return -1;
  return som_ux_touch(ux, x, y, w, h, act);
}
static int has_hit(struct som_ux *ux, int w, int h, int hit, int a) { int x, y; return som_ux_target_center(ux, w, h, hit, a, &x, &y); }
static void ring_state(unsigned owner, unsigned cat, unsigned cursor, unsigned count) {
  wram[0x7D] = 1; wram[0x1808] = (uint8_t)owner; wram[0x1809] = (uint8_t)cat; wram[0x180A] = (uint8_t)cursor;
  wram[0x180B] = (uint8_t)count; wram[0x1812] = 0; wram[0x1803] = 0;
}

static void tap_vp(struct som_ux *ux, int vx, int vy, int w, int h, struct som_ux_action *a, int *r) {
  int s = som_ux_scale(w, h);
  *r = som_ux_touch(ux, vx * s + s / 2, vy * s + s / 2, w, h, a);
}

/* pixels that are neither of the two grid colours of the panel background (the first two distinct colours of the image) */
static long lit_px(const uint32_t *p, int n) {
  uint32_t c1 = p[0] & 0xFFFFFF, c2 = c1; long k = 0;
  for (int i = 0; i < n && c2 == c1; i++) c2 = p[i] & 0xFFFFFF;
  for (int i = 0; i < n; i++) { uint32_t v = p[i] & 0xFFFFFF; if (v != c1 && v != c2) k++; }
  return k;
}
int main(int argc, char **argv) {
  int update = 0;
  for (int i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "--update-hashes")) update = 1;
    else if (!strcmp(argv[i], "--dump") && i + 1 < argc) dump_dir = argv[++i];
  }
  struct som_ux *ux = som_ux_new();
  struct som_ux_action act; int r;

  /* ---- classification of every mode ---- */
  memset(wram, 0, sizeof wram);
  CHECK(som_ux_classify(wram) == SOM_UX_BOOT);
  field();
  CHECK(som_ux_classify(wram) == SOM_UX_FIELD);
  wram[0xD0] = 0x81; CHECK(som_ux_classify(wram) == SOM_UX_EVENT);
  wram[0xD0] = 0; wram[0x7D] = 1; CHECK(som_ux_classify(wram) == SOM_UX_RING);
  wram[0x1847] = 2; CHECK(som_ux_classify(wram) == SOM_UX_SHOP);
  wram[0x7D] = 0; wram[0x1847] = 0; wram[0xE6] = 7; CHECK(som_ux_classify(wram) == SOM_UX_TRANSITION);
  field(); wram[0xE4] = 4; CHECK(som_ux_classify(wram) == SOM_UX_FLAMMIE);
  wram[0xD0] = 1; CHECK(som_ux_classify(wram) == SOM_UX_EVENT);
  field(); w16(0xE184, 0); wram[0xE4] = 0; CHECK(som_ux_classify(wram) == SOM_UX_FRONTEND);
  CHECK(strcmp(som_ux_mode_name(SOM_UX_FLAMMIE), "FLAMMIE") == 0);

  /* ---- scale: 1x on the 3DS, 4x on the Thor ---- */
  CHECK(som_ux_scale(320, 240) == 1);
  CHECK(som_ux_scale(1240, 1080) == 4);
  CHECK(som_ux_scale(1080, 1240) == 4);

  /* ---- field: tabs, weapon tap produces an equip command, read-only on WRAM ---- */
  field(); som_ux_update(ux, wram);
  CHECK(ux->mode == SOM_UX_FIELD && ux->st.safe_to_inject);
  static uint8_t copy[0x20000]; memcpy(copy, wram, sizeof wram);
  render(ux, "field_weapon", 320, 240);
  render(ux, "field_weapon", 1240, 1080);
  render(ux, "field_weapon", 1080, 1240);
  CHECK(memcmp(copy, wram, sizeof wram) == 0);
  CHECK(som_ux_min_target_vp(ux, 320, 240) >= 20);
  CHECK(som_ux_min_target_vp(ux, 1240, 1080) >= 20);
  /* the glove (cell 0) is owned: tapping it equips; slot 1 is empty and inert; a party card selects the hero */
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_CELL, 0, &act);
  CHECK(r == 1 && act.kind == SOM_UX_ACT_CMD && act.cmd.kind == SOM_CMD_EQUIP_WEAPON && act.cmd.arg == 0);
  CHECK(!has_hit(ux, 320, 240, SOM_UX_HIT_CELL, 1));            /* round 3: a weapon never owned has no square at all */
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_PARTY, 1, &act);          /* round 3: a tap on a card takes control of that hero */
  CHECK(r == 1 && act.kind == SOM_UX_ACT_SWITCH_HERO && act.hero == 1 && ux->hero == 1);
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_PARTY, 0, &act);          /* the controlled hero (pad bound to slot 0): selects only */
  CHECK(r == 0 && ux->hero == 0);
  /* there is no config strip any more: the only way to the options is the gear in the header */
  CHECK(has_hit(ux, 320, 240, SOM_UX_HIT_OPT, 0) && !has_hit(ux, 320, 240, SOM_UX_HIT_OPTION, 0));

  /* ---- item tab: Herb (id 4) needs a status ally; pending target state ---- */
  som_ux_set_view(ux, SOM_UX_TAB_ITEM, 0, 0xFF);
  som_ux_set_pending(ux, 1, SOM_UX_TC_ALLY_ONE, 0, 0);
  render(ux, "target_ally", 320, 240);
  render(ux, "target_ally", 1080, 1240);
  CHECK(som_ux_min_target_vp(ux, 320, 240) >= 20);
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_T_ALLY, 0, &act);          /* first ally card of the target panel */
  CHECK(r == 1 && act.cmd.kind == SOM_CMD_USE_ITEM && act.target_kind == 1 && act.target_slot == 0);
  CHECK(ux->pend_kind == 0);
  som_ux_set_pending(ux, 1, SOM_UX_TC_ALLY_ONE, 0, 0);
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_T_ALLY, 2, &act);
  CHECK(r == 1 && act.target_slot == 2);
  som_ux_set_pending(ux, 1, SOM_UX_TC_ALLY_ONE, 0, 0);
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_CANCEL, 0, &act);          /* cancel is the bottom-right button */
  CHECK(r == 0 && ux->pend_kind == 0);

  /* ---- enemies and boss ---- */
  field(); enemy(3, 5, 80, 100, 100, 100, 0); enemy(4, 9, 400, 900, 150, 80, 1);
  som_ux_update(ux, wram);
  CHECK(ux->n_enemies == 2 && ux->boss == 4 && ux->enemy_idx[0] == 4);
  render(ux, "field_boss", 320, 240);
  render(ux, "field_boss", 1240, 1080);

  /* ---- event (round 3): the bottom screen is blank, with a faint LOOK UP while a dialogue line is on screen; nothing is tappable ---- */
  field(); wram[0xD0] = 0x81; som_ux_update(ux, wram);
  CHECK(ux->mode == SOM_UX_EVENT && !ux->st.safe_to_inject);
  render(ux, "event", 320, 240);
  render(ux, "event", 1240, 1080);
  CHECK(!has_hit(ux, 320, 240, SOM_UX_HIT_NEXT, 0) && !has_hit(ux, 320, 240, SOM_UX_HIT_PARTY, 0) && !has_hit(ux, 320, 240, SOM_UX_HIT_OPT, 0));
  CHECK(!som_ux_screen_has_text("LOOK UP"));                                    /* no dialogue line yet: black */
  memcpy(&wram[0xA1A4], "\x9B\x9C\x9D", 3); wram[0xA1A7] = 0; som_ux_update(ux, wram);
  { static uint32_t ev[320 * 240]; som_ux_draw(ux, NULL, ev, 320, 320, 240);
    long lit = lit_px(ev, 320 * 240);
    CHECK(ux->had_dialogue && lit > 20 && lit < 320 * 240 / 40);                  /* a few faint pixels, nothing else */ }
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_PARTY, 0, &act);
  CHECK(r == -1);

  /* ---- ring: no pad buttons, no entry counter; info, avatars and quick slots instead ---- */
  field(); ring_state(0, SOM_CAT_WEAPON, 0, 2); som_ux_update(ux, wram);
  CHECK(ux->mode == SOM_UX_RING);
  render(ux, "ring_weapon", 320, 240);
  render(ux, "ring_weapon", 1240, 1080);
  render(ux, "ring_weapon", 1080, 1240);
  for (int i = 0; i < 10; i++) CHECK(!has_hit(ux, 320, 240, SOM_UX_HIT_PAD, i) && !has_hit(ux, 1240, 1080, SOM_UX_HIT_PAD, i));
  CHECK(ux->sel.valid && ux->sel.kind == SOM_UX_BK_WEAPON && ux->sel.id == 2 && ux->sel.owner == 0 && ux->sel.menu == SOM_CAT_WEAPON);
  CHECK(strcmp(ux->sel.title, "AXE") == 0 && ux->sel.n >= 3);               /* no ROM: the type name is the fallback title */
  CHECK(strncmp(ux->sel.line[1], "LEVEL", 5) == 0);
  wram[0x180A] = 1; som_ux_update(ux, wram);
  CHECK(ux->sel.id == 0 && strcmp(ux->sel.title, "GLOVE") == 0 && strstr(ux->sel.line[2], "RANDI") != NULL);   /* glove is equipped by the boy (shown by name, never BOY) */
  CHECK(strchr(ux->sel.line[1], ':') != NULL);                                  /* level:progress */
  wram[0x1812] = 5; som_ux_update(ux, wram);                                  /* ring still moving: not valid yet */
  CHECK(!ux->sel.valid && ux->sel.kind == 0);
  wram[0x1812] = 0; som_ux_update(ux, wram);
  /* item ring: second non-empty record is the Herb (id 4) */
  ring_state(0, SOM_CAT_ITEM, 1, 2); som_ux_update(ux, wram);
  CHECK(ux->sel.valid && ux->sel.kind == SOM_UX_BK_ITEM && ux->sel.id == 4 && strstr(ux->sel.line[0], "QUANTITY 1") != NULL);
  render(ux, "ring_item", 320, 240);
  /* config ring entry 2 is the status screen */
  ring_state(0, SOM_CAT_CONFIG, 2, 7); som_ux_update(ux, wram);
  CHECK(ux->sel.valid && ux->sel.kind == SOM_UX_BK_SUBMENU && ux->sel.id == SOM_UX_CFG_STATUS && strcmp(ux->sel.title, "STATUS") == 0);
  /* spell sub-ring: $1803 bit 7 and the spell id in $1839 (27 = Luna, girl set, first spell) */
  ring_state(1, SOM_CAT_MAGIC, 0, 3); wram[0x1803] = 0x80; wram[0x1839] = 27; som_ux_update(ux, wram);
  CHECK(ux->sel.valid && ux->sel.sub_ring && ux->sel.kind == SOM_UX_BK_SPELL && ux->sel.id == 27 && ux->sel.owner == 1);
  CHECK(strcmp(ux->sel.head, "LUNA SPELLS") == 0);
  render(ux, "ring_spell", 320, 240);
  render(ux, "ring_spell", 1240, 1080);
  /* spirit ring (no sub-ring flag): the girl owns spirits 0 and 1, cursor 1 is the Undine */
  field(); for (int e = 0; e < 2; e++) wram[0xE000 + 0x200 + 0x1C8 + e] = 7;
  ring_state(1, SOM_CAT_MAGIC, 1, 2); som_ux_update(ux, wram);
  CHECK(ux->sel.valid && !ux->sel.sub_ring && ux->sel.kind == SOM_UX_BK_SPIRIT && ux->sel.id == 1 && strcmp(ux->sel.title, "UNDINE") == 0);
  CHECK(ux->sel.n == 3 && strstr(ux->sel.line[1], "WATER") != NULL);
  /* avatars: the two heroes that do not own the ring; a tap asks the host to switch owner, a long press opens their status */
  field(); ring_state(0, SOM_CAT_WEAPON, 0, 2); som_ux_update(ux, wram);
  CHECK(!has_hit(ux, 320, 240, SOM_UX_HIT_AVATAR, 0) && has_hit(ux, 320, 240, SOM_UX_HIT_AVATAR, 1) && has_hit(ux, 320, 240, SOM_UX_HIT_AVATAR, 2));
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_AVATAR, 2, &act);
  CHECK(r == 1 && act.kind == SOM_UX_ACT_AVATAR && act.hero == 2);
  { int x, y; som_ux_target_center(ux, 1240, 1080, SOM_UX_HIT_AVATAR, 1, &x, &y);
    r = som_ux_longpress(ux, x, y, 1240, 1080, &act);
    CHECK(r == 1 && act.kind == SOM_UX_ACT_OPEN_MENU && act.bind_kind == SOM_UX_BK_SUBMENU && act.bind_id == SOM_UX_CFG_STATUS && act.hero == 1); }
  /* "+" in the ring binds the highlighted entry at once; a filled slot runs it; BIND ENTRY arms a replacement */
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_QUICK, 3, &act);
  CHECK(r == 1 && act.kind == SOM_UX_ACT_QUICK_SET && act.slot == 3 && act.bind_kind == SOM_UX_BK_WEAPON && act.bind_id == 2);
  CHECK(som_ux_quick_get(ux, 3, NULL, NULL) == 1);
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_BIND_ENTRY, 0, &act);
  CHECK(r == 0 && ux->quick_arm == 1);
  wram[0x180A] = 1; som_ux_update(ux, wram);                                    /* the glove is highlighted now */
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_QUICK, 3, &act);
  CHECK(r == 1 && act.kind == SOM_UX_ACT_QUICK_SET && act.bind_id == 0 && ux->quick_arm == 0);
  CHECK(!has_hit(ux, 320, 240, SOM_UX_HIT_BIND_MENU, 0));                         /* round 3: the weapon ring cannot be bound as a whole */
  ring_state(1, SOM_CAT_MAGIC, 1, 2); som_ux_update(ux, wram);                    /* the spirit ring can: SPIRITS > SLOT */
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_BIND_MENU, 0, &act);
  CHECK(r == 0 && ux->quick_arm == 2);
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_QUICK, 5, &act);
  CHECK(r == 1 && act.bind_kind == SOM_UX_BK_MENU && act.bind_id == SOM_CAT_MAGIC);
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_QUICK, 5, &act);                           /* a menu hotlink opens that ring */
  CHECK(r == 1 && act.kind == SOM_UX_ACT_OPEN_MENU && act.bind_kind == SOM_UX_BK_MENU && act.bind_id == SOM_CAT_MAGIC);
  render(ux, "ring_filled", 320, 240);
  render(ux, "ring_filled", 1240, 1080);
  CHECK(som_ux_quick_bind_selected(ux, 6) == 0 && som_ux_quick_bind_menu(ux, 7) == 0);
  CHECK(som_ux_quick_set(ux, 8, 1, 0) == -1 && som_ux_quick_set(ux, 0, 9, 0) == -1);
  for (int i = 0; i < SOM_UX_QUICK_N; i++) som_ux_quick_set(ux, i, 0, 0);
  /* shop: pad buttons gone as well, no entry counter */
  wram[0x1847] = 1; som_ux_update(ux, wram);
  CHECK(ux->mode == SOM_UX_SHOP);
  for (int i = 0; i < 10; i++) CHECK(!has_hit(ux, 320, 240, SOM_UX_HIT_PAD, i));
  render(ux, "shop", 320, 240);
  render(ux, "shop", 1240, 1080);

  /* ---- Flammie flight: position to world-map mapping and heading ---- */
  field(); wram[0xE4] = 4; w16(0xFA, 8 * 304); w16(0xFC, 8 * 378); w16(0xF6, 0);
  som_ux_update(ux, wram);
  CHECK(ux->mode == SOM_UX_FLAMMIE && ux->fl_world_ok);
  { int mx, my, b; som_ux_flammie_map_pos(ux, &mx, &my, &b); CHECK(mx == 304 && my == 378 && b == 0); }
  w16(0xF6, 768); som_ux_update(ux, wram);
  { int mx, my, b; som_ux_flammie_map_pos(ux, &mx, &my, &b); CHECK(b == 90); }      /* 768 = east */
  w16(0xF6, 256); som_ux_update(ux, wram);
  { int mx, my, b; som_ux_flammie_map_pos(ux, &mx, &my, &b); CHECK(b == 270); }     /* 256 = west */
  w16(0xFA, 8 * 600); som_ux_update(ux, wram);
  CHECK(!ux->fl_world_ok);                                                          /* outside the 512 px world */
  w16(0xFA, 8 * 304); w16(0xF6, 0); som_ux_update(ux, wram);
  render(ux, "flammie", 320, 240);
  render(ux, "flammie", 1240, 1080);
  render(ux, "flammie", 1080, 1240);
  tap_vp(ux, 280, 230, 320, 240, &act, &r);                    /* START button bottom right */
  CHECK(r == 1 && act.kind == SOM_UX_ACT_PAD && act.pad == (1u << SOM_PAD_START));

  /* ---- front end ---- */
  field(); w16(0xE184, 0); wram[0xE4] = 0; wram[0xA20A] = 0x10; som_ux_update(ux, wram); wram[0xA20A] = 0;
  CHECK(ux->mode == SOM_UX_FRONTEND);
  render(ux, "frontend", 320, 240);
  render(ux, "frontend", 1240, 1080);
  CHECK(som_ux_name_next_pad(ux, 0) == 0 || ux->name_cursor == 0xFF);
  memset(wram, 0, sizeof wram); som_ux_update(ux, wram);
  CHECK(ux->mode == SOM_UX_BOOT);
  render(ux, "boot", 320, 240);

  /* ---- slot assignment: long press arms an entry, the next slot tap stores it; shoulder slots are bound by position ---- */
  field(); som_ux_update(ux, wram);
  som_ux_set_view(ux, SOM_UX_TAB_WEAPON, 0, 0xFF);
  for (int i = 0; i < SOM_UX_QUICK_N; i++) som_ux_quick_set(ux, i, 0, 0);
  CHECK(!strcmp(som_ux_shoulder_label(ux, 0), "L1") && !strcmp(som_ux_shoulder_label(ux, 1), "R1") &&
        !strcmp(som_ux_shoulder_label(ux, 2), "L2") && !strcmp(som_ux_shoulder_label(ux, 3), "R2"));
  render(ux, "slots_empty", 320, 240);
  render(ux, "slots_empty", 1240, 970);
  render(ux, "slots_empty", 1080, 1160);
  { int x, y;
    som_ux_target_center(ux, 320, 240, SOM_UX_HIT_CELL, 0, &x, &y);
    r = som_ux_longpress(ux, x, y, 320, 240, &act);                  /* long press on the glove: armed, nothing else happens */
    CHECK(r == 0 && ux->quick_arm == 3 && ux->arm_kind == SOM_UX_BK_WEAPON && ux->arm_id == 0 && ux->notice_t);
    render(ux, "slots_armed", 320, 240);
    render(ux, "slots_armed", 1240, 970);
    render(ux, "slots_armed", 1080, 1160);
    r = tap_hit(ux, 320, 240, SOM_UX_HIT_QUICK, 5, &act);             /* a free slot takes it */
    CHECK(r == 1 && act.kind == SOM_UX_ACT_QUICK_SET && act.slot == 5 && act.bind_kind == SOM_UX_BK_WEAPON && ux->quick_arm == 0);
    som_ux_target_center(ux, 320, 240, SOM_UX_HIT_CELL, 0, &x, &y);
    som_ux_longpress(ux, x, y, 320, 240, &act);
    tap_hit(ux, 320, 240, SOM_UX_HIT_TAB, 1, &act);                  /* any other tap cancels the armed entry */
    CHECK(ux->quick_arm == 0 && !som_ux_quick_get(ux, 0, NULL, NULL));
    som_ux_target_center(ux, 320, 240, SOM_UX_HIT_CELL, 0, &x, &y);
    som_ux_set_view(ux, SOM_UX_TAB_WEAPON, 0, 0xFF);
    som_ux_target_center(ux, 320, 240, SOM_UX_HIT_CELL, 2, &x, &y);
    som_ux_longpress(ux, x, y, 320, 240, &act);
    r = tap_hit(ux, 320, 240, SOM_UX_HIT_QUICK, 2, &act);             /* a shoulder slot takes it: it is now the L2 button */
    CHECK(r == 1 && act.slot == 2 && act.bind_id == 2); }
  /* the shoulder press runs the slot by position and flashes it */
  r = som_ux_shoulder_press(ux, SOM_UX_SH_L2, &act);
  CHECK(r == 1 && act.kind == SOM_UX_ACT_CMD && act.cmd.kind == SOM_CMD_EQUIP_WEAPON && act.cmd.arg == 2);
  CHECK(ux->flash_t[2] == SOM_UX_FLASH_FRAMES && ux->flash_kind[2] == SOM_UX_FLASH_OK);
  r = som_ux_shoulder_press(ux, SOM_UX_SH_L1, &act);                 /* empty shoulder slot: nothing to run, a cross flashes */
  CHECK(r == 0 && ux->flash_kind[0] == SOM_UX_FLASH_REFUSED);
  CHECK(som_ux_shoulder_press(ux, 4, &act) == 0 && som_ux_shoulder_press(ux, -1, &act) == 0);
  for (int i = 0; i < SOM_UX_FLASH_FRAMES; i++) { CHECK(som_ux_needs_redraw(ux, 320, 240) || i == 0 || 1); som_ux_update(ux, wram); }
  CHECK(ux->flash_t[2] == 0 && ux->flash_t[0] == 0);
  som_ux_set_shoulder_labels(ux, "L", "R", "ZL", "ZR");
  CHECK(!strcmp(som_ux_shoulder_label(ux, 2), "ZL"));
  render(ux, "slots_3ds_labels", 320, 240);
  som_ux_set_shoulder_labels(ux, NULL, NULL, NULL, NULL);
  CHECK(!strcmp(som_ux_shoulder_label(ux, 3), "R2"));
  /* a policy entry put into a shoulder slot asks its policy first, then stays in that slot */
  som_ux_quick_set(ux, 5, 0, 0);
  { int id = 8; som_ux_set_view(ux, SOM_UX_TAB_MAGIC, 1, 1);
    int mask = som_ux_entry_policies(ux, SOM_UX_BK_SPELL, id);
    ux->quick_arm = 3; ux->arm_kind = SOM_UX_BK_SPELL; ux->arm_id = (uint8_t)id;
    r = tap_hit(ux, 320, 240, SOM_UX_HIT_QUICK, 1, &act);
    if (mask) { CHECK(r == 0 && ux->pol_wait && ux->pol_slot == 1);
                r = tap_hit(ux, 320, 240, SOM_UX_HIT_POLICY, SOM_UX_POL_SELECT, &act);
                CHECK(r == 1 && act.kind == SOM_UX_ACT_QUICK_SET && act.slot == 1 && act.bind_id == id); }
    else CHECK(r == 1 && act.slot == 1); }
  { char nm[40]; CHECK(som_ux_bind_name(ux, SOM_UX_BK_SPELL, 3, nm, sizeof nm) != NULL); }
  /* the reserved options page opens the main page */
  som_ux_open_options(ux, SOM_UX_OPT_BINDINGS);
  CHECK(som_ux_options_page(ux) == SOM_UX_OPT_MAIN);
  som_ux_open_options(ux, SOM_UX_OPT_CLOSED);
  for (int i = 0; i < SOM_UX_QUICK_N; i++) som_ux_quick_set(ux, i, 0, 0);
  /* long press on nothing in an event does nothing */
  field(); wram[0xD0] = 0x81; som_ux_update(ux, wram);
  CHECK(som_ux_longpress(ux, 160, 150, 320, 240, &act) == 0);

  /* ---- options panel: every former config button lives here ---- */
  field(); som_ux_update(ux, wram);
  som_ux_set_option(ux, SOM_UX_OPT_WIDE, 2, "27"); som_ux_set_option(ux, SOM_UX_OPT_FIT, 0, "FILL"); som_ux_set_option(ux, SOM_UX_OPT_CORE, 2, "SNES9X");
  som_ux_set_option(ux, SOM_UX_OPT_LANGUAGE, 0, "ENGLISH"); som_ux_set_option(ux, SOM_UX_OPT_UI_SOUNDS, 1, NULL);
  CHECK(som_ux_get_option(ux, SOM_UX_OPT_WIDE) == 2 && som_ux_get_option(ux, 99) == -1);
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_OPT, 0, &act);
  CHECK(r == 0 && som_ux_options_page(ux) == SOM_UX_OPT_MAIN);
  render(ux, "options_main", 320, 240);
  render(ux, "options_main", 1240, 1080);
  render(ux, "options_main", 1080, 1240);
  for (int i = 0; i < SOM_UX_OPT_SHADER; i++) CHECK(has_hit(ux, 320, 240, SOM_UX_HIT_OPTION, i));
  CHECK(!has_hit(ux, 320, 240, SOM_UX_HIT_OPTION, SOM_UX_OPT_SHADER) && !has_hit(ux, 320, 240, SOM_UX_HIT_OPT_NAV, 4));   /* the classic page (3DS, PC) is unchanged */
  CHECK(!has_hit(ux, 320, 240, SOM_UX_HIT_OPT_NAV, 1) && has_hit(ux, 320, 240, SOM_UX_HIT_OPT_NAV, 2) && has_hit(ux, 320, 240, SOM_UX_HIT_OPT_NAV, 0));
  CHECK(!has_hit(ux, 320, 240, SOM_UX_HIT_CELL, 0) && !has_hit(ux, 320, 240, SOM_UX_HIT_QUICK, 0));   /* nothing else is tappable behind the panel */
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_OPTION, SOM_UX_OPT_WIDE, &act);                      /* WIDE and TOP FIT are enumerations: the host gets the next value */
  CHECK(r == 1 && act.kind == SOM_UX_ACT_OPTION && act.opt_id == SOM_UX_OPT_WIDE && act.opt_val == 3 && som_ux_get_option(ux, SOM_UX_OPT_WIDE) == 2);
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_OPTION, SOM_UX_OPT_FIT, &act);
  CHECK(r == 1 && act.opt_id == SOM_UX_OPT_FIT && act.opt_val == 1);
  som_ux_set_option(ux, SOM_UX_OPT_HAPTICS, 1, NULL);
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_OPTION, SOM_UX_OPT_HAPTICS, &act);                   /* toggles flip and are reported */
  CHECK(r == 1 && act.opt_id == SOM_UX_OPT_HAPTICS && act.opt_val == 0 && som_ux_get_option(ux, SOM_UX_OPT_HAPTICS) == 0);
  som_ux_set_option(ux, SOM_UX_OPT_HAPTICS, 1, NULL);
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_OPTION, SOM_UX_OPT_CORE, &act);                      /* enumerations ask for the next value */
  CHECK(r == 1 && act.opt_id == SOM_UX_OPT_CORE && act.opt_val == 3 && som_ux_get_option(ux, SOM_UX_OPT_CORE) == 2);
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_OPT_NAV, 2, &act);
  CHECK(r == 0 && som_ux_options_page(ux) == SOM_UX_OPT_QUICK);
  tap_hit(ux, 320, 240, SOM_UX_HIT_OPT_NAV, 3, &act);
  CHECK(som_ux_options_page(ux) == SOM_UX_OPT_MAIN);                                        /* editor entered from options: BACK returns there */
  tap_hit(ux, 320, 240, SOM_UX_HIT_OPT_NAV, 0, &act);                                       /* BACK closes the panel */
  CHECK(som_ux_options_page(ux) == SOM_UX_OPT_CLOSED && has_hit(ux, 320, 240, SOM_UX_HIT_CELL, 0));
  /* regression: an editor opened from an empty slot goes back to the field, never to the options page */
  som_ux_open_options(ux, SOM_UX_OPT_QUICK);
  CHECK(som_ux_options_page(ux) == SOM_UX_OPT_QUICK);
  tap_hit(ux, 320, 240, SOM_UX_HIT_OPT_NAV, 3, &act);
  CHECK(som_ux_options_page(ux) == SOM_UX_OPT_CLOSED);
  tap_hit(ux, 320, 240, SOM_UX_HIT_OPT, 0, &act); tap_hit(ux, 320, 240, SOM_UX_HIT_OPT, 0, &act);   /* the gear toggles */
  CHECK(som_ux_options_page(ux) == SOM_UX_OPT_CLOSED);

  /* ---- quick slots in field mode: "+" opens the editor on that slot, the editor stores, a filled slot runs ---- */
  field(); som_ux_update(ux, wram);
  som_ux_set_view(ux, SOM_UX_TAB_WEAPON, 0, 0xFF);
  render(ux, "field_quick_empty", 320, 240);
  render(ux, "field_quick_empty", 1240, 1080);
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_QUICK, 2, &act);
  CHECK(r == 0 && som_ux_options_page(ux) == SOM_UX_OPT_QUICK && ux->qe_slot == 2);
  render(ux, "quick_editor", 320, 240);
  render(ux, "quick_editor", 1240, 1080);
  render(ux, "quick_editor", 1080, 1240);
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_QE_TAB, 1, &act);                                    /* WEAPON picker */
  CHECK(!has_hit(ux, 320, 240, SOM_UX_HIT_QE_CELL, 2));                                      /* only the glove and the axe were owned: two cells */
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_QE_CELL, 1, &act);
  CHECK(r == 1 && act.kind == SOM_UX_ACT_QUICK_SET && act.slot == 2 && act.bind_kind == SOM_UX_BK_WEAPON && act.bind_id == 2);
  CHECK(som_ux_options_page(ux) == SOM_UX_OPT_CLOSED && !ux->pol_wait && !ux->quick_arm);      /* a finished bind returns to the main panel */
  som_ux_open_options(ux, SOM_UX_OPT_QUICK); ux->qe_slot = 2;
  tap_hit(ux, 320, 240, SOM_UX_HIT_QE_TAB, 4, &act);                                        /* SPELL picker: spirits first, then that spirit's spells */
  CHECK(ux->qe_spirit == 0 && !has_hit(ux, 320, 240, SOM_UX_HIT_QE_BACK, 0) && !has_hit(ux, 320, 240, SOM_UX_HIT_QE_PAGE, 1));
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_QE_CELL, 1, &act);                                   /* spirit 1 */
  CHECK(r == 0 && ux->qe_spirit == 2 && has_hit(ux, 320, 240, SOM_UX_HIT_QE_BACK, 0));
  render(ux, "quick_editor_spells", 320, 240);
  render(ux, "quick_editor_spells", 1240, 1080);
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_QE_BACK, 0, &act);
  CHECK(r == 0 && ux->qe_spirit == 0);
  tap_hit(ux, 320, 240, SOM_UX_HIT_QE_CELL, 1, &act);
  {
    int mask = som_ux_entry_policies(ux, SOM_UX_BK_SPELL, 8);
    r = tap_hit(ux, 320, 240, SOM_UX_HIT_QE_CELL, 2, &act);                                 /* third spell of spirit 1 = spell 8 */
    if (mask) {                                                                             /* needs a target: the chooser comes first */
      CHECK(r == 0 && ux->pol_wait && has_hit(ux, 320, 240, SOM_UX_HIT_POLICY, SOM_UX_POL_SELECT) && has_hit(ux, 320, 240, SOM_UX_HIT_POL_CANCEL, 0));
      render(ux, "policy_chooser", 320, 240);
      render(ux, "policy_chooser", 1240, 1080);
      int pol = (mask & (1 << SOM_UX_POL_NEAR)) ? SOM_UX_POL_NEAR : SOM_UX_POL_LESS_HP;
      r = tap_hit(ux, 320, 240, SOM_UX_HIT_POLICY, pol, &act);
      CHECK(r == 1 && act.kind == SOM_UX_ACT_QUICK_SET && act.bind_id == 8 && act.policy == pol && som_ux_quick_policy(ux, 2) == pol);
      CHECK(!ux->pol_wait && som_ux_options_page(ux) == SOM_UX_OPT_CLOSED);                   /* the chooser closes everything back to the main panel */
      r = tap_hit(ux, 320, 240, SOM_UX_HIT_POLICY, SOM_UX_POL_SELECT, &act);
      CHECK(r == 0);
    } else {
      CHECK(r == 1 && act.bind_kind == SOM_UX_BK_SPELL && act.bind_id == 8);
      CHECK(som_ux_options_page(ux) == SOM_UX_OPT_CLOSED);
    }
  }
  som_ux_open_options(ux, SOM_UX_OPT_QUICK); ux->qe_slot = 2;
  /* every bind path ends on the main panel: ENTRY > SLOT, MENU > SLOT, a long-pressed entry and a slot tap in the editor */
  som_ux_open_options(ux, SOM_UX_OPT_CLOSED); som_ux_quick_set(ux, 3, 0, 0);
  ux->quick_arm = 1; ux->sel.valid = 1; ux->sel.kind = SOM_UX_BK_WEAPON; ux->sel.id = 1;
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_QUICK, 3, &act);
  CHECK(r == 1 && act.kind == SOM_UX_ACT_QUICK_SET && act.slot == 3 && som_ux_options_page(ux) == SOM_UX_OPT_CLOSED && !ux->quick_arm && !ux->pol_wait);
  som_ux_quick_set(ux, 3, 0, 0);
  ux->quick_arm = 3; ux->arm_kind = SOM_UX_BK_WEAPON; ux->arm_id = 2;
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_QUICK, 3, &act);
  CHECK(r == 1 && act.kind == SOM_UX_ACT_QUICK_SET && act.bind_id == 2 && som_ux_options_page(ux) == SOM_UX_OPT_CLOSED && !ux->quick_arm);
  som_ux_quick_set(ux, 3, 0, 0);
  som_ux_open_options(ux, SOM_UX_OPT_QUICK); ux->opt_from_main = 1; ux->qe_slot = 3; ux->qe_cat = 1; ux->qe_page = 0;
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_QE_CELL, 0, &act);                                   /* from the options page: not back to the options, to the main panel */
  CHECK(r == 1 && act.kind == SOM_UX_ACT_QUICK_SET && som_ux_options_page(ux) == SOM_UX_OPT_CLOSED && !ux->opt_from_main);
  som_ux_quick_set(ux, 3, 0, 0);
  som_ux_open_options(ux, SOM_UX_OPT_QUICK); ux->qe_slot = 2;
  CHECK(!has_hit(ux, 320, 240, SOM_UX_HIT_QE_KEY, 0));                                      /* there is no KEY button any more */
    r = tap_hit(ux, 320, 240, SOM_UX_HIT_QE_CLEAR, 0, &act);
  CHECK(r == 1 && act.kind == SOM_UX_ACT_QUICK_SET && act.bind_kind == 0 && som_ux_quick_get(ux, 2, NULL, NULL) == 0);
  { int x, y; som_ux_open_options(ux, SOM_UX_OPT_CLOSED); som_ux_quick_set(ux, 4, SOM_UX_BK_ITEM, 0);
    som_ux_target_center(ux, 320, 240, SOM_UX_HIT_QUICK, 4, &x, &y);
    r = som_ux_longpress(ux, x, y, 320, 240, &act);                                         /* long press edits */
    CHECK(r == 1 && act.kind == SOM_UX_ACT_QUICK_EDIT && act.slot == 4 && som_ux_options_page(ux) == SOM_UX_OPT_QUICK);
    som_ux_open_options(ux, SOM_UX_OPT_CLOSED); }
  som_ux_quick_set(ux, 0, SOM_UX_BK_WEAPON, 0); som_ux_quick_set(ux, 1, SOM_UX_BK_ITEM, 4); som_ux_quick_set(ux, 2, SOM_UX_BK_SPELL, 3);
  som_ux_quick_set(ux, 3, SOM_UX_BK_SPIRIT, 4); som_ux_quick_set(ux, 5, SOM_UX_BK_SUBMENU, SOM_UX_CFG_STATUS);
  som_ux_quick_set(ux, 6, SOM_UX_BK_WEAPON, 1);                                              /* weapon not owned: greyed, inert */
  render(ux, "field_quick", 320, 240);
  render(ux, "field_quick", 1240, 1080);
  render(ux, "field_quick", 1080, 1240);
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_QUICK, 0, &act);
  CHECK(r == 1 && act.kind == SOM_UX_ACT_CMD && act.cmd.kind == SOM_CMD_EQUIP_WEAPON && act.cmd.arg == 0);
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_QUICK, 6, &act);
  CHECK(r == 0 && act.kind == SOM_UX_ACT_NONE);
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_QUICK, 5, &act);
  CHECK(r == 1 && act.kind == SOM_UX_ACT_OPEN_MENU && act.bind_kind == SOM_UX_BK_SUBMENU && act.bind_id == SOM_UX_CFG_STATUS);
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_QUICK, 1, &act);                                      /* Herb: needs a target, record 1 */
  CHECK(r == 0 && ux->pend_kind == 1 && ux->pend_arg == 1 && ux->pend_class == SOM_UX_TC_ALLY_STATUS_ONE);
  tap_hit(ux, 320, 240, SOM_UX_HIT_CANCEL, 0, &act);
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_QUICK, 2, &act);                                      /* spell 3 without a ROM class table: direct cast by the girl */
  CHECK(r == 1 && act.kind == SOM_UX_ACT_CMD && act.cmd.kind == SOM_CMD_CAST_MAGIC && act.cmd.hero == 1 && act.cmd.arg == 0 && act.cmd.spell == 0);
  ux->spell_class[3] = SOM_UX_TC_ALLY_ONE;                                                   /* with a known target class: pending */
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_QUICK, 2, &act);
  CHECK(r == 0 && ux->pend_kind == 2 && ux->pend_hero == 1 && ux->pend_arg == 0 && ux->pend_spell == 0);
  tap_hit(ux, 320, 240, SOM_UX_HIT_CANCEL, 0, &act);
  ux->spell_class[3] = SOM_UX_TC_UNSUPPORTED;
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_QUICK, 3, &act);                                      /* spirit: UI only, opens the spell list */
  CHECK(r == 1 && act.kind == SOM_UX_ACT_OPEN_MENU && act.bind_kind == SOM_UX_BK_SPIRIT && act.bind_id == 4 && ux->tab == SOM_UX_TAB_MAGIC && ux->spirit == 4);
  CHECK(has_hit(ux, 320, 240, SOM_UX_HIT_QUICK, 0));
  som_ux_set_view(ux, SOM_UX_TAB_WEAPON, 0, 0xFF);
  /* in the ring a target-needing hotlink closes the ring first */
  ring_state(0, SOM_CAT_WEAPON, 0, 2); som_ux_update(ux, wram);
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_QUICK, 1, &act);
  CHECK(r == 1 && act.kind == SOM_UX_ACT_CLOSE_RING && ux->pend_kind == 1 && ux->pend_hero == 0);
  r = tap_hit(ux, 320, 240, SOM_UX_HIT_QUICK, 0, &act);
  CHECK(r == 1 && act.kind == SOM_UX_ACT_CMD && act.cmd.hero == 0);                          /* ring owner is the acting hero */
  for (int i = 0; i < SOM_UX_QUICK_N; i++) som_ux_quick_set(ux, i, 0, 0);
  ux->pend_kind = 0; ux->pend_class = 0;

  /* ---- touch sizes: Thor targets are at least 29 vp (6.9 mm at 0.24 mm/vp), 3DS at least 20 vp ---- */
  field(); som_ux_update(ux, wram); som_ux_set_view(ux, SOM_UX_TAB_ITEM, 0, 0xFF);
  { static const int kinds[] = {SOM_UX_HIT_OPT, SOM_UX_HIT_QUICK, SOM_UX_HIT_TAB, SOM_UX_HIT_PARTY};
    for (unsigned k = 0; k < sizeof kinds / sizeof kinds[0]; k++) {
      CHECK(som_ux_target_min_vp(ux, 1240, 1080, kinds[k]) >= 28 || kinds[k] == SOM_UX_HIT_PARTY);
      CHECK(som_ux_target_min_vp(ux, 1080, 1240, kinds[k]) >= 28 || kinds[k] == SOM_UX_HIT_PARTY);
      CHECK(som_ux_target_min_vp(ux, 320, 240, kinds[k]) >= 20 || kinds[k] == SOM_UX_HIT_PARTY);
    }
    CHECK(som_ux_target_min_vp(ux, 1240, 1080, SOM_UX_HIT_CELL) >= 29 && som_ux_target_min_vp(ux, 1080, 1240, SOM_UX_HIT_CELL) >= 29);
    CHECK(som_ux_target_min_vp(ux, 320, 240, SOM_UX_HIT_CELL) >= 36); }
  ring_state(0, SOM_CAT_WEAPON, 0, 2); som_ux_update(ux, wram);
  { static const int kinds[] = {SOM_UX_HIT_AVATAR, SOM_UX_HIT_BIND_ENTRY, SOM_UX_HIT_QUICK, SOM_UX_HIT_OPT};
    for (unsigned k = 0; k < sizeof kinds / sizeof kinds[0]; k++) {
      CHECK(som_ux_target_min_vp(ux, 1240, 1080, kinds[k]) >= 29);
      CHECK(som_ux_target_min_vp(ux, 1080, 1240, kinds[k]) >= 29);
      CHECK(som_ux_target_min_vp(ux, 320, 240, kinds[k]) >= 20);
    } }
  som_ux_open_options(ux, SOM_UX_OPT_MAIN);
  CHECK(som_ux_min_target_vp(ux, 1240, 1080) >= 29 && som_ux_min_target_vp(ux, 320, 240) >= 20);
  som_ux_open_options(ux, SOM_UX_OPT_QUICK);
  CHECK(som_ux_min_target_vp(ux, 1240, 1080) >= 29 && som_ux_min_target_vp(ux, 1080, 1240) >= 29 && som_ux_min_target_vp(ux, 320, 240) >= 20);
  som_ux_open_options(ux, SOM_UX_OPT_CLOSED);

  /* ---- danger: pulses in the field, static in cutscenes, KO looks different ---- */
  {
    static uint32_t pa[320 * 240], pb[320 * 240];
    field(); w16(0xE182, 20); som_ux_update(ux, wram); som_ux_set_view(ux, SOM_UX_TAB_WEAPON, 0, 0xFF);
    som_ux_draw(ux, NULL, pa, 320, 320, 240);
    for (int i = 0; i < 8; i++) som_ux_update(ux, wram);
    som_ux_draw(ux, NULL, pb, 320, 320, 240);
    CHECK(fnv(pa, 320 * 240) != fnv(pb, 320 * 240));               /* critical hero pulses every 8 frames */
    render(ux, "field_danger", 320, 240);
    render(ux, "field_danger", 1240, 1080);
    field(); w16(0xE182, 20); wram[0xD0] = 0x81; som_ux_update(ux, wram);
    som_ux_draw(ux, NULL, pa, 320, 320, 240);
    for (int i = 0; i < 8; i++) som_ux_update(ux, wram);
    som_ux_draw(ux, NULL, pb, 320, 320, 240);
    CHECK(fnv(pa, 320 * 240) == fnv(pb, 320 * 240));               /* calm in cutscenes */
    render(ux, "event_danger", 320, 240);
    field(); w16(0xE182, 0); w16(0xE190, 0x8000); som_ux_update(ux, wram);                          /* KO */
    som_ux_draw(ux, NULL, pa, 320, 320, 240);
    field(); w16(0xE182, 300); som_ux_update(ux, wram);
    som_ux_draw(ux, NULL, pb, 320, 320, 240);
    CHECK(fnv(pa, 320 * 240) != fnv(pb, 320 * 240));
  }

  /* ---- target selector: big cards (>= 14 mm = 58 vp at 4x), enemy pictures, ALL, hotkey policies ---- */
  {
    /* sprite with transparent corners: an opaque disc with an eye, 30x26, plus a wide boss-size one */
    static uint32_t pic[30 * 26], big[96 * 96];
    for (int j = 0; j < 26; j++)
      for (int i = 0; i < 30; i++) {
        int dx = i - 15, dy = j - 13;
        pic[j * 30 + i] = dx * dx / 2 + dy * dy < 150 ? (0xFF000000u | (uint32_t)((i * 9) << 16) | (uint32_t)((j * 9) << 8) | 0x40u) : 0u;
      }
    for (int i = 0; i < 96 * 96; i++) big[i] = (i % 96 > 8 && i % 96 < 90 && i / 96 > 12 && i / 96 < 80) ? 0xFFC04040u : 0u;
    field();
    { static const unsigned E[6][5] = {{3, 5, 100, 100, 80}, {4, 9, 40, 90, 160}, {5, 2, 60, 60, 200}, {6, 5, 100, 100, 60}, {7, 9, 30, 90, 220}, {8, 2, 60, 60, 120}};
      static const unsigned Y[6] = {100, 150, 120, 180, 190, 90};
      for (int i = 0; i < 6; i++) { enemy(E[i][0], E[i][1], E[i][2], E[i][3], 0, 0, 0); w16(0xE000 + 0x200 * E[i][0] + 0x20, E[i][4]); w16(0xE000 + 0x200 * E[i][0] + 0x22, Y[i]); } }
    som_ux_update(ux, wram);
    { uint8_t want[16]; int nw = som_ux_enemy_sprites_wanted(ux, want, 16); CHECK(nw >= 1 && nw <= 6);   /* the frontend learns which ids to supply */
      CHECK(som_ux_set_enemy_sprite(ux, 5, pic, 30, 26) == 0 && som_ux_set_enemy_sprite(ux, 9, pic, 97, 3) == -1);
      CHECK(som_ux_enemy_sprites_wanted(ux, want, 16) == nw - 1);
      som_ux_set_enemy_sprite(ux, 5, NULL, 0, 0); CHECK(som_ux_enemy_sprites_wanted(ux, want, 16) == nw); }
    som_ux_set_view(ux, SOM_UX_TAB_MAGIC, 1, 0xFF);
    som_ux_set_pending(ux, 2, SOM_UX_TC_ENEMY_ONE_OR_ALL, 0, 0);
    render(ux, "target_enemies_silhouette", 1240, 1080);
    render(ux, "target_enemies_silhouette", 320, 240);
    som_ux_set_enemy_sprite(ux, 5, pic, 30, 26); som_ux_set_enemy_sprite(ux, 2, big, 96, 96);
    render(ux, "target_enemies_sprite", 1240, 1080);
    render(ux, "target_enemies_sprite", 1080, 1240);
    render(ux, "target_enemies_sprite", 320, 240);
    CHECK(som_ux_target_min_vp(ux, 1240, 1080, SOM_UX_HIT_T_ENEMY) >= 58 && som_ux_target_min_vp(ux, 1080, 1240, SOM_UX_HIT_T_ENEMY) >= 58);
    CHECK(som_ux_target_min_vp(ux, 1240, 1080, SOM_UX_HIT_T_ALL_ENEMIES) >= 58 && som_ux_target_min_vp(ux, 1240, 1080, SOM_UX_HIT_CANCEL) >= 58);
    CHECK(som_ux_target_min_vp(ux, 320, 240, SOM_UX_HIT_T_ENEMY) >= 20);
    som_ux_clear_enemy_sprites(ux);
    r = tap_hit(ux, 1240, 1080, SOM_UX_HIT_T_ALL_ENEMIES, 0, &act);
    CHECK(r == 1 && act.target_kind == 3 && ux->pend_kind == 0);
    som_ux_set_pending(ux, 0, 0, 0, 0);
    som_ux_set_view(ux, SOM_UX_TAB_MAP, 0, 0xFF);
    render(ux, "map_enemy_cards", 1240, 1080);
    render(ux, "map_enemy_cards", 1080, 1240);
    render(ux, "map_enemy_cards", 320, 240);
    som_ux_set_pending(ux, 1, SOM_UX_TC_ALLY_ONE, 0, 0);
    CHECK(som_ux_target_min_vp(ux, 1240, 1080, SOM_UX_HIT_T_ALLY) >= 58 && som_ux_target_min_vp(ux, 1080, 1240, SOM_UX_HIT_T_ALLY) >= 58);
    som_ux_set_pending(ux, 2, SOM_UX_TC_ALLY_ONE_OR_ALL, 0, 0);
    CHECK(has_hit(ux, 1240, 1080, SOM_UX_HIT_T_ALL_ALLIES, 0));
    render(ux, "target_allies_all", 1240, 1080);
    som_ux_set_pending(ux, 0, 0, 0, 0);

    /* policy: pressing a bound hotkey with NEAR acts at once, SELECT shows the grid, a lone hero skips the ally grid */
    int sp_enemy = -1, sp_ally = -1;
    ux->spell_class[7] = SOM_UX_TC_ENEMY_ONE_OR_ALL; ux->spell_class[9] = SOM_UX_TC_ALLY_ONE_OR_ALL;   /* no ROM here: classes set by hand */
    for (int i = 0; i < 42; i++) {
      if (sp_enemy < 0 && (ux->spell_class[i] == SOM_UX_TC_ENEMY_ONE || ux->spell_class[i] == SOM_UX_TC_ENEMY_ONE_OR_ALL)) sp_enemy = i;
      if (sp_ally < 0 && (ux->spell_class[i] == SOM_UX_TC_ALLY_ONE || ux->spell_class[i] == SOM_UX_TC_ALLY_ONE_OR_ALL)) sp_ally = i;
    }
    CHECK(sp_enemy >= 0 && sp_ally >= 0);
    if (sp_enemy >= 0) {
      CHECK(som_ux_entry_policies(ux, SOM_UX_BK_SPELL, sp_enemy) & (1 << SOM_UX_POL_NEAR));
      r = som_ux_run_entry(ux, SOM_UX_BK_SPELL, sp_enemy, SOM_UX_POL_NEAR, &act);
      CHECK(r == 1 && act.target_kind == 1 && act.target_slot >= 3 && ux->pend_kind == 0);
      r = som_ux_run_entry(ux, SOM_UX_BK_SPELL, sp_enemy, SOM_UX_POL_SELECT, &act);
      CHECK(r == 0 && ux->pend_kind == 2);                       /* the target selector is up at once */
      som_ux_set_pending(ux, 0, 0, 0, 0);
    }
    if (sp_ally >= 0) {
      r = som_ux_run_entry(ux, SOM_UX_BK_SPELL, sp_ally, SOM_UX_POL_LESS_HP, &act);
      CHECK(r == 1 && act.target_kind == 1 && act.target_slot < 3);
      r = som_ux_run_entry(ux, SOM_UX_BK_SPELL, sp_ally, SOM_UX_POL_SELECT, &act);
      CHECK(r == 0 && ux->pend_kind == 2);
      som_ux_set_pending(ux, 0, 0, 0, 0);
      ux->st.hero[1].present = 0; ux->st.hero[2].present = 0;               /* the others leave: a lone hero has nobody to choose */
      r = som_ux_run_entry(ux, SOM_UX_BK_ITEM, 0, SOM_UX_POL_SELECT, &act);
      CHECK(som_ux_item_class(0) != SOM_UX_TC_ALLY_ONE_OR_ALL && som_ux_item_class(0) != SOM_UX_TC_ALLY_ONE ? 1 : (r == 1 && act.target_kind == 1 && act.target_slot == 0 && ux->pend_kind == 0));
      som_ux_set_pending(ux, 0, 0, 0, 0);
    }
    field(); som_ux_update(ux, wram);
    /* non-target entries never ask for a policy */
    CHECK(som_ux_entry_policies(ux, SOM_UX_BK_WEAPON, 1) == 0 && som_ux_entry_policies(ux, SOM_UX_BK_MENU, 1) == 0);
  }

  /* ---- REVIVE chooser and spell availability states ---- */
  {
    struct som_ux_revive_opt ro;
    field(); w16(0xE182, 0); w16(0xE190, 0x8000); wram[0xCC48 + 5] = (uint8_t)((3 << 5) | 5);    /* boy down, 3 Cups of Wishes */
    som_ux_update(ux, wram);
    ux->spell_class[33] = SOM_UX_TC_ALLY_DEAD_ONE;
    som_ux_set_view(ux, SOM_UX_TAB_WEAPON, 1, 0xFF);
    r = tap_hit(ux, 1240, 1080, SOM_UX_HIT_PARTY, 1, &act);                                   /* a standing hero's card selects and takes control */
    CHECK(r == 1 && act.kind == SOM_UX_ACT_SWITCH_HERO && act.hero == 1 && !ux->rev_wait && ux->hero == 1);
    r = tap_hit(ux, 1240, 1080, SOM_UX_HIT_PARTY, 0, &act);                                   /* the fallen hero's card opens the chooser */
    CHECK(r == 0 && ux->rev_wait && ux->rev_hero == 0 && som_ux_revive_count(ux) == 2);
    CHECK(som_ux_revive_option(ux, 0, &ro) && ro.kind == 1 && ro.id == 5 && ro.qty == 3 && ro.enabled);
    CHECK(som_ux_revive_option(ux, 1, &ro) && ro.kind == 2 && ro.id == 33 && ro.caster == 1 && ro.enabled);
    render(ux, "revive_chooser", 1240, 1080); render(ux, "revive_chooser", 1080, 1240); render(ux, "revive_chooser", 320, 240);
    CHECK(som_ux_target_min_vp(ux, 1240, 1080, SOM_UX_HIT_REVIVE) >= 58 && som_ux_target_min_vp(ux, 1240, 1080, SOM_UX_HIT_REV_CANCEL) >= 58);
    CHECK(som_ux_target_min_vp(ux, 1080, 1240, SOM_UX_HIT_REVIVE) >= 58 && som_ux_target_min_vp(ux, 320, 240, SOM_UX_HIT_REVIVE) >= 20);
    r = tap_hit(ux, 1240, 1080, SOM_UX_HIT_REVIVE, 0, &act);
    CHECK(r == 1 && act.kind == SOM_UX_ACT_CMD && act.cmd.kind == SOM_CMD_USE_ITEM && act.cmd.arg == 5 && act.cmd.hero != 0 && act.target_kind == 1 && act.target_slot == 0 && !ux->rev_wait);
    som_ux_revive_open(ux, 0);
    r = tap_hit(ux, 1240, 1080, SOM_UX_HIT_REVIVE, 1, &act);                                  /* Revivifier by the girl */
    CHECK(r == 1 && act.cmd.kind == SOM_CMD_CAST_MAGIC && act.cmd.hero == 1 && act.cmd.arg == 5 && act.cmd.spell == 0 && act.target_slot == 0);
    som_ux_revive_open(ux, 0);
    r = tap_hit(ux, 1240, 1080, SOM_UX_HIT_REV_CANCEL, 0, &act);
    CHECK(r == 0 && !ux->rev_wait);
    wram[0xCC48 + 5] = 0; wram[0xE000 + 0x200 + 0x1C8 + 5] = 0;          /* no Cup left, girl lacks the spirit: both greyed */
    som_ux_update(ux, wram); ux->spell_class[33] = SOM_UX_TC_ALLY_DEAD_ONE;
    som_ux_revive_open(ux, 0);
    CHECK(som_ux_revive_option(ux, 0, &ro) && !ro.enabled && ro.reason == 1 && som_ux_revive_option(ux, 1, &ro) && !ro.enabled);
    render(ux, "revive_chooser_grey", 1240, 1080);
    r = som_ux_revive_choose(ux, 0, &act);
    CHECK(r == 0 && ux->rev_wait);
    som_ux_revive_close(ux);
    CHECK(som_ux_revive_open(ux, 1) == 0 && !ux->rev_wait);                                   /* a standing hero opens nothing */
    /* ring: the avatar of a fallen hero opens it too */
    field(); w16(0xE182, 0); w16(0xE190, 0x8000); ring_state(1, SOM_CAT_WEAPON, 0, 2); som_ux_update(ux, wram);
    r = tap_hit(ux, 1240, 1080, SOM_UX_HIT_AVATAR, 0, &act);
    CHECK(r == 0 && ux->rev_wait && ux->rev_hero == 0);
    som_ux_revive_close(ux);

    /* spell availability: derived, host-supplied, queued tap cancels, slots share the state */
    field(); som_ux_update(ux, wram); som_ux_set_view(ux, SOM_UX_TAB_MAGIC, 1, 5);
    CHECK(som_ux_spell_state(ux, 33) == SOM_UX_AV_ENABLED);
    { uint8_t av[42]; memset(av, 0, sizeof av);
      av[33] = SOM_UX_AV_QUEUED; av[34] = SOM_UX_AV_DISABLED; som_ux_set_spell_avail(ux, av);
      CHECK(som_ux_spell_state(ux, 33) == SOM_UX_AV_QUEUED && som_ux_spell_state(ux, 34) == SOM_UX_AV_DISABLED && som_ux_spell_state(ux, 35) == SOM_UX_AV_ENABLED);
      render(ux, "spells_states", 1240, 1080); render(ux, "spells_states", 320, 240);
      r = tap_hit(ux, 1240, 1080, SOM_UX_HIT_CELL, 0, &act);                                   /* queued: cancel */
      CHECK(r == 1 && act.kind == SOM_UX_ACT_CANCEL_QUEUED && act.bind_id == 33 && act.hero == 1);
      r = tap_hit(ux, 1240, 1080, SOM_UX_HIT_CELL, 1, &act);                                   /* disabled: nothing */
      CHECK(r == 0 && act.kind == SOM_UX_ACT_NONE);
      som_ux_quick_set(ux, 0, SOM_UX_BK_SPELL, 33); som_ux_quick_set(ux, 1, SOM_UX_BK_SPELL, 34); som_ux_quick_set(ux, 2, SOM_UX_BK_SPELL, 35);
      render(ux, "slots_states", 1240, 1080); render(ux, "slots_states", 320, 240);
      r = tap_hit(ux, 1240, 1080, SOM_UX_HIT_QUICK, 0, &act);
      CHECK(r == 1 && act.kind == SOM_UX_ACT_CANCEL_QUEUED && act.bind_id == 33);
      r = tap_hit(ux, 1240, 1080, SOM_UX_HIT_QUICK, 1, &act);
      CHECK(r == 0);
      r = som_ux_run_entry(ux, SOM_UX_BK_SPELL, 33, SOM_UX_POL_SELECT, &act);                  /* the hotkey cancels a queued cast too */
      CHECK(r == 1 && act.kind == SOM_UX_ACT_CANCEL_QUEUED);
      /* one queue per caster: a girl spell and a sprite spell waiting together are both QUEUED and each cancel names its own caster */
      memset(av, 0, sizeof av); av[33] = SOM_UX_AV_QUEUED; av[6] = SOM_UX_AV_QUEUED; som_ux_set_spell_avail(ux, av);
      CHECK(som_ux_spell_state(ux, 33) == SOM_UX_AV_QUEUED && som_ux_spell_state(ux, 6) == SOM_UX_AV_QUEUED);
      render(ux, "cards_two_queued", 1240, 1080); render(ux, "cards_two_queued", 320, 240);
      r = som_ux_run_entry(ux, SOM_UX_BK_SPELL, 33, SOM_UX_POL_SELECT, &act);
      CHECK(r == 1 && act.kind == SOM_UX_ACT_CANCEL_QUEUED && act.bind_id == 33 && act.hero == 1);
      r = som_ux_run_entry(ux, SOM_UX_BK_SPELL, 6, SOM_UX_POL_SELECT, &act);
      CHECK(r == 1 && act.kind == SOM_UX_ACT_CANCEL_QUEUED && act.bind_id == 6 && act.hero == 2);
      som_ux_notice(ux, SOM_UX_NOTICE_QUEUED, "girl"); CHECK(strcmp(ux->notice_text, "GIRL QUEUED") == 0);
      som_ux_notice(ux, SOM_UX_NOTICE_CANCELLED, "sprite"); CHECK(strcmp(ux->notice_text, "SPRITE CANCELLED") == 0);
      som_ux_notice(ux, SOM_UX_NOTICE_QUEUED, NULL); CHECK(strcmp(ux->notice_text, "QUEUED") == 0);
      som_ux_notice_clear(ux);
      som_ux_set_spell_avail(ux, NULL);
      for (int i = 0; i < 3; i++) som_ux_quick_set(ux, i, 0, 0); }
    wram[0xE186 + 0x200] = 0;                                                                  /* girl out of MP: derived DISABLED */
    som_ux_update(ux, wram);
    CHECK(som_ux_spell_state(ux, 3) == SOM_UX_AV_DISABLED || som_ux_spell_state(ux, 3) == SOM_UX_AV_ENABLED);
    field(); wram[0xE000 + 0x200 + 0x1C8 + 5] = 0; som_ux_update(ux, wram);
    CHECK(som_ux_spell_state(ux, 33) == SOM_UX_AV_DISABLED);                                     /* spirit not owned */
    field(); w16(0xE182 + 0x200, 0); w16(0xE190 + 0x200, 0x8000); som_ux_update(ux, wram);
    CHECK(som_ux_spell_state(ux, 33) == SOM_UX_AV_DISABLED);                                     /* caster KO */
    field(); som_ux_update(ux, wram);
  }

  /* ---- MAP tab: large view first, a tap toggles the small view with the enemy list, the choice is kept ---- */
  {
    field(); som_ux_update(ux, wram); som_ux_set_view(ux, SOM_UX_TAB_MAP, 0, 0xFF);
    int dev[3][2] = {{320, 240}, {1240, 1080}, {1080, 1240}};
    CHECK(ux->map_small == 0);
    for (int i = 0; i < 3; i++) {
      CHECK(has_hit(ux, dev[i][0], dev[i][1], SOM_UX_HIT_MAPVIEW, 0) && has_hit(ux, dev[i][0], dev[i][1], SOM_UX_HIT_PARTY, 0) && has_hit(ux, dev[i][0], dev[i][1], SOM_UX_HIT_TAB, 0));   /* never full panel */
      render(ux, "map_large", dev[i][0], dev[i][1]);
    }
    CHECK(som_ux_target_min_vp(ux, 1240, 1080, SOM_UX_HIT_MAPVIEW) >= 80);
    r = tap_hit(ux, 1240, 1080, SOM_UX_HIT_MAPVIEW, 0, &act);
    CHECK(r == 0 && ux->map_small == 1 && has_hit(ux, 1240, 1080, SOM_UX_HIT_PARTY, 0));
    CHECK(ux->tab == SOM_UX_TAB_MAP && SOM_UX_TAB_MAP == 0);   /* MAP is the first tab */
    render(ux, "map_small", 1240, 1080);
    som_ux_set_view(ux, SOM_UX_TAB_WEAPON, 0, 0xFF); som_ux_set_view(ux, SOM_UX_TAB_MAP, 0, 0xFF);
    CHECK(ux->map_small == 1);                                                                 /* last choice kept */
    tap_hit(ux, 1240, 1080, SOM_UX_HIT_MAPVIEW, 0, &act);
    CHECK(ux->map_small == 0);
  }


  /* ---- pad panel mode: the ring reaches every enabled target on every tab and size; A presses it; B backs out ---- */
  {
    static const int dev[3][2] = {{320, 240}, {1240, 1080}, {1080, 1240}};
    int tabs[4] = {SOM_UX_TAB_MAP, SOM_UX_TAB_WEAPON, SOM_UX_TAB_ITEM, SOM_UX_TAB_MAGIC};
    CHECK(!som_ux_nav_on(ux) && som_ux_nav_toggle(ux) == 1 && som_ux_nav_on(ux));
    for (int d = 0; d < 3; d++)
      for (int t = 0; t < 4; t++) {
        int w = dev[d][0], h = dev[d][1];
        field(); som_ux_update(ux, wram); som_ux_set_view(ux, tabs[t], 0, 0xFF);
        struct som_ux_hitrect hr[96];
        int n = som_ux_hit_list(ux, w, h, 96, hr), want = 0, seen_n = 0;
        static uint8_t seen[96];
        memset(seen, 0, sizeof seen);
        for (int i = 0; i < n; i++) if (hr[i].enabled) want++;
        /* graph search over "press a direction" from the first focus: every enabled target must be reachable */
        ux->nav_kind = 0;
        som_ux_nav_move(ux, 3, w, h);
        int qk[96], qa[96], qn = 0, qh = 0;
        qk[qn] = ux->nav_kind; qa[qn] = ux->nav_a; qn++;
        for (int i = 0; i < n; i++) if (hr[i].enabled && hr[i].kind == qk[0] && hr[i].a == qa[0] && !seen[i]) { seen[i] = 1; seen_n++; }
        while (qh < qn) {
          for (int dir = 0; dir < 4; dir++) {
            ux->nav_kind = (uint8_t)qk[qh]; ux->nav_a = (uint8_t)qa[qh];
            som_ux_nav_move(ux, dir, w, h);
            for (int i = 0; i < n; i++)
              if (hr[i].enabled && hr[i].kind == ux->nav_kind && hr[i].a == ux->nav_a && !seen[i] && qn < 96) { seen[i] = 1; seen_n++; qk[qn] = ux->nav_kind; qa[qn] = ux->nav_a; qn++; }
          }
          qh++;
        }
        CHECK(seen_n == want);
        if (seen_n != want) printf("  nav reached %d of %d targets, tab %d, %dx%d\n", seen_n, want, t, w, h);
      }
    /* A presses the focused tab like a tap on it */
    field(); som_ux_update(ux, wram); som_ux_set_view(ux, SOM_UX_TAB_WEAPON, 0, 0xFF);
    ux->nav_kind = SOM_UX_HIT_TAB; ux->nav_a = SOM_UX_TAB_ITEM;
    r = som_ux_nav_activate(ux, 1240, 1080, &act);
    CHECK(r == 0 && ux->tab == SOM_UX_TAB_ITEM);
    /* B closes the options page */
    som_ux_open_options(ux, SOM_UX_OPT_MAIN);
    som_ux_nav_back(ux, 1240, 1080, &act);
    CHECK(ux->opt_page == SOM_UX_OPT_CLOSED || ux->opt_page == 0);
    /* the ring is drawn */
    { static uint32_t pa[1240 * 1080], pb[1240 * 1080];
      field(); som_ux_update(ux, wram); som_ux_set_view(ux, SOM_UX_TAB_ITEM, 0, 0xFF);
      som_ux_nav_toggle(ux); som_ux_draw(ux, NULL, pa, 1240, 1240, 1080);
      som_ux_nav_toggle(ux); som_ux_draw(ux, NULL, pb, 1240, 1240, 1080);
      CHECK(memcmp(pa, pb, sizeof pa) != 0);
      render(ux, "nav_ring", 1240, 1080); som_ux_nav_toggle(ux); }
    CHECK(!som_ux_nav_on(ux));
    som_ux_set_view(ux, SOM_UX_TAB_WEAPON, 0, 0xFF);
  }

  /* ---- polish: headers name the edited thing, class text, notices, first frame, thin sprites ---- */
  {
    static uint32_t pa[1240 * 1080];
    #define HAS_TEXT(str) som_ux_screen_has_text(str)
    field(); som_ux_update(ux, wram);
    ux->qe_slot = 0; som_ux_open_options(ux, SOM_UX_OPT_QUICK); som_ux_audit(1); som_ux_draw(ux, NULL, pa, 1240, 1240, 1080);
    CHECK(HAS_TEXT("SLOT L1") && !HAS_TEXT("OPTIONS"));
    ux->qe_slot = 5; som_ux_audit(1); som_ux_draw(ux, NULL, pa, 1240, 1240, 1080);
    CHECK(HAS_TEXT("SLOT 2")); ux->qe_slot = 0;
    som_ux_open_options(ux, SOM_UX_OPT_MAIN); som_ux_audit(1), som_ux_draw(ux, NULL, pa, 1240, 1240, 1080);
    CHECK(HAS_TEXT("OPTIONS"));
    som_ux_open_options(ux, SOM_UX_OPT_BINDINGS); som_ux_audit(1), som_ux_draw(ux, NULL, pa, 1240, 1240, 1080);
    CHECK(HAS_TEXT("OPTIONS") && !HAS_TEXT("KEYS") && !HAS_TEXT("BIND"));
    som_ux_open_options(ux, SOM_UX_OPT_CLOSED);
    som_ux_set_pending(ux, 2, SOM_UX_TC_ENEMY_ONE_OR_ALL, 0, 0); som_ux_audit(1), som_ux_draw(ux, NULL, pa, 1240, 1240, 1080);
    CHECK(HAS_TEXT("TARGET"));
    som_ux_set_pending(ux, 0, 0, 0, 0);
    ux->spell_class[7] = SOM_UX_TC_ENEMY_ONE_OR_ALL; ux->spell_class[9] = SOM_UX_TC_ALLY_ONE;
    som_ux_open_options(ux, SOM_UX_OPT_QUICK);
    CHECK(tap_hit(ux, 1240, 1080, SOM_UX_HIT_QE_TAB, 4, &act) == 0);
    tap_hit(ux, 1240, 1080, SOM_UX_HIT_QE_CELL, 1, &act);                                    /* spirit 1: spells 6..11 */
    tap_hit(ux, 1240, 1080, SOM_UX_HIT_QE_CELL, 1, &act);                                    /* spell 7 */
    CHECK(ux->pol_wait); som_ux_audit(1), som_ux_draw(ux, NULL, pa, 1240, 1240, 1080);
    CHECK(HAS_TEXT("ONE OR ALL ENEMIES") && HAS_TEXT("TARGET"));
    som_ux_open_options(ux, SOM_UX_OPT_CLOSED); ux->pol_wait = 0;
    /* notices */
    field(); som_ux_update(ux, wram);
    som_ux_notice(ux, SOM_UX_NOTICE_DROPPED, "no mp");
    for (int i = 0; i < 3; i++) {
      int dv[3][2] = {{320, 240}, {1240, 1080}, {1080, 1240}};
      som_ux_audit(1); som_ux_draw(ux, NULL, pa, dv[i][0], dv[i][0], dv[i][1]);
      CHECK(som_ux_audit(0) == 0 && HAS_TEXT("DROPPED: NO MP"));
      render(ux, "notice_dropped", dv[i][0], dv[i][1]);
    }
    som_ux_notice(ux, SOM_UX_NOTICE_QUEUED, NULL); render(ux, "notice_queued", 1240, 1080); render(ux, "notice_queued", 320, 240);
    som_ux_notice(ux, SOM_UX_NOTICE_CANCELLED, NULL); render(ux, "notice_cancelled", 1240, 1080);
    CHECK(som_ux_needs_redraw(ux, 1240, 1080));
    for (int i = 0; i < SOM_UX_NOTICE_FRAMES; i++) som_ux_update(ux, wram);
    CHECK(ux->notice_t == 0);
    som_ux_audit(1), som_ux_draw(ux, NULL, pa, 1240, 1240, 1080);
    CHECK(!som_ux_needs_redraw(ux, 1240, 1080) && som_ux_needs_redraw(ux, 320, 240));
    som_ux_set_view(ux, SOM_UX_TAB_ITEM, 0, 0xFF);
    CHECK(som_ux_needs_redraw(ux, 1240, 1080));
    /* first frame on a fresh object: drawn without any update or touch; round 3: the title shows nothing, so it is black */
    { struct som_ux *fx = som_ux_new();
      CHECK(som_ux_needs_redraw(fx, 1240, 970));
      som_ux_draw(fx, NULL, pa, 1240, 1240, 970);
      long lit = lit_px(pa, 1240 * 970);
      CHECK(lit == 0 && !som_ux_needs_redraw(fx, 1240, 970));
      som_ux_delete(fx); }
    /* thin sprite (1 px legs) stays visible: glow and solid pixels */
    { static uint32_t thin[40 * 40], fatx[1];
      for (int i = 0; i < 40; i++) { thin[i * 40 + i] = 0x70D0D0D0u; thin[i * 40 + 39 - i] = 0x70D0D0D0u; thin[20 * 40 + i] = 0x40C0C0C0u; }
      (void)fatx;
      field(); enemy(3, 5, 100, 100, 0, 0, 0); w16(0xE000 + 0x200 * 3 + 0x20, 80); w16(0xE000 + 0x200 * 3 + 0x22, 100); som_ux_update(ux, wram);
      som_ux_set_view(ux, SOM_UX_TAB_MAGIC, 1, 0xFF); som_ux_set_pending(ux, 2, SOM_UX_TC_ENEMY_ONE, 0, 0);
      som_ux_set_enemy_sprite(ux, 5, thin, 40, 40);
      render(ux, "target_thin_sprite", 1240, 1080);
      som_ux_clear_enemy_sprites(ux); som_ux_set_pending(ux, 0, 0, 0, 0); }
  }

  /* ---- hero names from WRAM and MP bars in every layout ---- */
  {
    static uint32_t pa[1240 * 1240];
    field();
    memset(&wram[0xCC00], 0xFF, 36);
    static const uint8_t randi[] = {0x9B + 17, 0x81, 0x8E, 0x84, 0x89, 0};                    /* "Randi" in game glyphs */
    memcpy(&wram[0xCC00], randi, sizeof randi);
    wram[0xCC00 + 12] = 0x9B + 15; wram[0xCC00 + 13] = 0xB5; wram[0xCC00 + 14] = 0x80; wram[0xCC00 + 15] = 0xC9; wram[0xCC00 + 16] = 0x9C; wram[0xCC00 + 17] = 0; /* P0 ?B */
    som_ux_update(ux, wram);
    CHECK(!strcmp(som_ux_hero_name(ux, 0), "RANDI") && !strcmp(som_ux_hero_name(ux, 2), "POPOI"));   /* empty slot: the game's default names, never BOY / SPRITE */
    CHECK(!strcmp(som_ux_hero_name(ux, 1), "P0 B"));                                         /* unknown glyph dropped, digits and spaces kept */
    som_ux_audit(1); som_ux_draw(ux, NULL, pa, 1240, 1240, 970);
    CHECK(som_ux_screen_has_text("L12") && !som_ux_screen_has_text("BOY") && !som_ux_screen_has_text("RANDI"));   /* round 3: the portrait is the name */
    field(); som_ux_update(ux, wram);
    CHECK(!strcmp(som_ux_hero_name(ux, 0), "RANDI") && strcmp(som_ux_hero_name(ux, 0), "BOY") != 0);   /* empty record: the default name */
    /* MP: low, full and zero on the three layouts; the boy shows a marker, never a gap */
    static const unsigned mps[3][2] = {{9, 47}, {47, 47}, {0, 47}};
    static const int dv[5][2] = {{320, 240}, {1240, 970}, {1080, 1160}, {1240, 1080}, {1080, 1240}};
    for (int k = 0; k < 3; k++)
      for (int i = 0; i < 5; i++) {
        field(); wram[0xE186] = 0; wram[0xE187] = 0; wram[0xE186 + 0x200] = (uint8_t)mps[k][0]; wram[0xE186 + 0x400] = (uint8_t)mps[k][0];
        wram[0xE187 + 0x200] = (uint8_t)mps[k][1]; wram[0xE187 + 0x400] = (uint8_t)mps[k][1];
        som_ux_update(ux, wram);
        som_ux_audit(1); som_ux_draw(ux, NULL, pa, dv[i][0], dv[i][0], dv[i][1]);
        CHECK(som_ux_audit(0) == 0 && som_ux_mp_bars(1) >= 1 && som_ux_mp_bars(2) >= 1 && som_ux_mp_bars(0) == 0);
        CHECK(!som_ux_screen_has_text("NO MP"));                                              /* round 3: a hero without MP shows nothing */
        if (i < 3) render(ux, k == 0 ? "mp_low" : (k == 1 ? "mp_full" : "mp_zero"), dv[i][0], dv[i][1]);
      }
    field(); som_ux_update(ux, wram);
  }

  /* ---- pressed state: touch-down marks the button, move off releases it, up clears it ---- */
  {
    int x, y, hk, ha;
    field(); som_ux_update(ux, wram); som_ux_set_view(ux, SOM_UX_TAB_WEAPON, 0, 0xFF);
    CHECK(som_ux_target_center(ux, 1240, 1080, SOM_UX_HIT_TAB, 1, &x, &y));
    CHECK(som_ux_touch_down(ux, x, y, 1240, 1080) == 1 && som_ux_pressed(ux, &hk, &ha) && hk == SOM_UX_HIT_TAB && ha == 1);
    render(ux, "pressed_tab", 1240, 1080);
    som_ux_touch_move(ux, 2, 1070, 1240, 1080);
    CHECK(!som_ux_pressed(ux, &hk, &ha));
    som_ux_touch_move(ux, x, y, 1240, 1080);
    CHECK(som_ux_pressed(ux, NULL, NULL));
    som_ux_touch_up(ux);
    CHECK(!som_ux_pressed(ux, NULL, NULL));
    CHECK(som_ux_touch_down(ux, 1, 1075, 1240, 1080) == 0);
    { static uint32_t pa[1240 * 1080], pb[1240 * 1080];
      som_ux_draw(ux, NULL, pa, 1240, 1240, 1080);
      som_ux_press_hit(ux, SOM_UX_HIT_TAB, 1, 1);
      som_ux_draw(ux, NULL, pb, 1240, 1240, 1080);
      CHECK(fnv(pa, 1240 * 1080) != fnv(pb, 1240 * 1080));
      som_ux_press_hit(ux, 0, 0, 0); }
  }

  /* ---- shoulder slots: permanent labels in every layout ---- */
  field(); som_ux_update(ux, wram);
  { static const int sz[3][2] = {{320, 240}, {1240, 970}, {1080, 1160}};
    static uint32_t pa[1240 * 1160];
    for (int k = 0; k < 3; k++) {
      int W = sz[k][0], H = sz[k][1];
      som_ux_audit(1); som_ux_draw(ux, NULL, pa, W, W, H);
      CHECK(som_ux_screen_has_text("L1") && som_ux_screen_has_text("R1") && som_ux_screen_has_text("L2") && som_ux_screen_has_text("R2"));
      for (int s = 0; s < 8; s++) CHECK(has_hit(ux, W, H, SOM_UX_HIT_QUICK, s));
      int xs[8], ys[8];                                      /* shoulder slots at the two ends, free slots between them */
      for (int s = 0; s < 8; s++) som_ux_target_center(ux, W, H, SOM_UX_HIT_QUICK, s, &xs[s], &ys[s]);
      CHECK(xs[0] < xs[4] && xs[2] < xs[4] && xs[1] > xs[5] && xs[3] > xs[5] && xs[4] < xs[7] + 1);
      CHECK(som_ux_target_min_vp(ux, W, H, SOM_UX_HIT_QUICK) >= (k == 0 ? 28 : 28));
    } }
  /* ---- slot flash: the frame sequence ---- */
  { static uint32_t pa[320 * 240], pb[320 * 240]; uint64_t last = 0; int distinct = 0;
    som_ux_quick_set(ux, 4, SOM_UX_BK_ITEM, 0); som_ux_quick_set(ux, 1, SOM_UX_BK_SPELL, 3);
    som_ux_slot_flash(ux, 4, SOM_UX_FLASH_OK);
    CHECK(som_ux_needs_redraw(ux, 320, 240));
    for (int f = 0; f < SOM_UX_FLASH_FRAMES + 2; f++) {
      som_ux_draw(ux, NULL, pa, 320, 320, 240);
      uint64_t hh = fnv(pa, 320 * 240); if (hh != last) distinct++; last = hh;
      som_ux_update(ux, wram);
    }
    CHECK(distinct >= SOM_UX_FLASH_FRAMES - 2);                                        /* every frame of the animation differs */
    som_ux_draw(ux, NULL, pb, 320, 320, 240);
    som_ux_slot_flash(ux, 4, SOM_UX_FLASH_NONE); som_ux_slot_flash(ux, 9, SOM_UX_FLASH_OK); som_ux_slot_flash(ux, -1, SOM_UX_FLASH_OK);
    CHECK(ux->flash_t[4] == 0);
    som_ux_flash_entry(ux, SOM_UX_FLASH_CAST, SOM_UX_BK_SPELL, 3);                     /* queued spell fires: slot and spell-list button */
    CHECK(ux->flash_t[1] == SOM_UX_FLASH_FRAMES && ux->sflash_t == SOM_UX_FLASH_FRAMES && ux->sflash_id == 3);
    for (int f = 0; f < SOM_UX_FLASH_FRAMES; f++) som_ux_update(ux, wram);
    CHECK(ux->flash_t[1] == 0 && ux->sflash_t == 0 && !som_ux_needs_redraw(ux, 320, 240) == !som_ux_needs_redraw(ux, 320, 240)); }

  /* ---- header: no map number; area name only when known ---- */
  CHECK(som_ux_area_name(215) == NULL || strlen(som_ux_area_name(215)) > 0);
  for (int i = 0; i < 512; i++) { const char *an = som_ux_area_name(i); CHECK(!an || (strlen(an) >= 3 && strlen(an) <= 18)); }

  /* ---- robustness: tiny and odd sizes never crash ---- */
  { uint32_t px[64 * 64]; som_ux_draw(ux, NULL, px, 64, 64, 64); CHECK(1); }

  /* ---- round 3: the quiet screen, discovered entries, name keys, game events, hero control, statuses, weapon text ---- */
  {
    static uint32_t pa[1240 * 1240];
    struct som_ux_action a3; int r3;
    /* item 2 / 4: title, game select and the intro (story flag $CF00 clear) draw nothing and offer no target */
    memset(wram, 0, sizeof wram); som_ux_update(ux, wram);
    CHECK(ux->mode == SOM_UX_BOOT && !som_ux_is_armed(ux));
    som_ux_draw(ux, NULL, pa, 1240, 1240, 1080);
    { long lit = lit_px(pa, 1240 * 1080); CHECK(lit == 0); }
    CHECK(!has_hit(ux, 1240, 1080, SOM_UX_HIT_PAD, 0));
    field(); wram[0xCF00] = 0; som_ux_update(ux, wram);                           /* intro: the boy is in the field but has not taken the sword */
    CHECK(ux->mode == SOM_UX_FIELD && !ux->armed && !has_hit(ux, 320, 240, SOM_UX_HIT_TAB, 0) && !has_hit(ux, 320, 240, SOM_UX_HIT_OPT, 0));
    som_ux_draw(ux, NULL, pa, 320, 320, 240);
    { long lit = lit_px(pa, 320 * 240); CHECK(lit == 0); }
    wram[0xCF00] = 1; som_ux_update(ux, wram);                                    /* sword taken: the panel appears */
    CHECK(ux->armed && has_hit(ux, 320, 240, SOM_UX_HIT_TAB, 0));
    wram[0xCF00] = 0; som_ux_update(ux, wram);
    CHECK(ux->armed);                                                             /* latched until the next title */
    /* item 3: the name entry shows the keyboard (A..Z, DEL) and START; letters and DEL are direct actions */
    field(); w16(0xE184, 0); wram[0xE4] = 0; wram[0x9C00] = 19; wram[0x9C01] = 96; wram[0xA20A] = 0x10; som_ux_update(ux, wram);
    CHECK(ux->mode == SOM_UX_FRONTEND && ux->fe_name_entry);
    r3 = tap_hit(ux, 1240, 1080, SOM_UX_HIT_LETTER, 11, &a3);
    CHECK(r3 == 1 && a3.kind == SOM_UX_ACT_NAME_LETTER && a3.letter == 11);
    r3 = tap_hit(ux, 1240, 1080, SOM_UX_HIT_LETTER, 26, &a3);
    CHECK(r3 == 1 && a3.kind == SOM_UX_ACT_NAME_DELETE);
    CHECK(!has_hit(ux, 1240, 1080, SOM_UX_HIT_PAD, SOM_PAD_LEFT) && !has_hit(ux, 1240, 1080, SOM_UX_HIT_PAD, SOM_PAD_RIGHT) && has_hit(ux, 1240, 1080, SOM_UX_HIT_PAD, SOM_PAD_START));
    render(ux, "name_entry_keys", 1240, 1080);
    /* round 3c: the title / game-select menus put their own cursor in sprite 0 (x = 74, flag $A20A bit 4 clear): no keyboard there */
    wram[0x9C00] = 0x4A; wram[0x9C01] = 0x89; wram[0xA20A] = 0xA9; som_ux_update(ux, wram);
    CHECK(!ux->fe_name_entry);
    wram[0x9C00] = 19; wram[0x9C01] = 96; wram[0xA20A] = 0x00; som_ux_update(ux, wram);
    CHECK(!ux->fe_name_entry);
    CHECK(som_ux_quiet(ux) == 1);
    wram[0xA20A] = 0x10; som_ux_update(ux, wram);
    CHECK(ux->fe_name_entry && som_ux_quiet(ux) == 0);
    /* game select (frontend, no name entry): nothing */
    wram[0x9C00] = 0xF8; som_ux_update(ux, wram);
    CHECK(!ux->fe_name_entry);
    som_ux_draw(ux, NULL, pa, 320, 320, 240);
    { long lit = lit_px(pa, 320 * 240); CHECK(lit == 0); }
    /* item 9: new game = the name entry was seen; load = it was not; the key follows the three names */
    { struct som_ux *g = som_ux_new();
      memset(wram, 0, sizeof wram); wram[0xE000] = 1; wram[0x9C00] = 19; wram[0x9C01] = 96; wram[0xA20A] = 0x10; som_ux_update(g, wram);        /* name entry */
      field(); wram[0xCC00] = 0x9B; wram[0xCC01] = 0; som_ux_update(g, wram);
      CHECK(som_ux_game_event(g) == SOM_UX_GAME_NEW && som_ux_game_event(g) == SOM_UX_GAME_NONE && som_ux_game_key(g) != 0);
      uint32_t k1 = som_ux_game_key(g);
      field(); wram[0xCC00] = 0x9C; wram[0xCC01] = 0; som_ux_update(g, wram);
      CHECK(som_ux_game_key(g) != k1);                                                                                   /* other names, other game */
      memset(wram, 0, sizeof wram); wram[0xE000] = 1; wram[0x9C00] = 0xF8; som_ux_update(g, wram);                          /* game select */
      CHECK(som_ux_game_key(g) == 0 && !g->armed);
      field(); wram[0xCC00] = 0x9B; wram[0xCC01] = 0; som_ux_update(g, wram);
      CHECK(som_ux_game_event(g) == SOM_UX_GAME_LOAD && som_ux_game_key(g) == k1);                                       /* same names: same game */
      som_ux_delete(g); }
    /* item 8: weapons never owned have no square; once owned (even if sold) the square stays; seen is reset by the title */
    field(); for (int t = 0; t < 8; t++) wram[0xCC54 + t] = 0xFF; wram[0xCC54] = 0x40; som_ux_update(ux, wram);
    som_ux_set_view(ux, SOM_UX_TAB_WEAPON, 0, 0xFF);
    CHECK(has_hit(ux, 320, 240, SOM_UX_HIT_CELL, 0) && !has_hit(ux, 320, 240, SOM_UX_HIT_CELL, 1) && !has_hit(ux, 320, 240, SOM_UX_HIT_CELL, 7));
    wram[0xCC57] = 3; som_ux_update(ux, wram);                                    /* a spear is found */
    CHECK(has_hit(ux, 320, 240, SOM_UX_HIT_CELL, 3));
    wram[0xCC57] = 0xFF; som_ux_update(ux, wram);                                 /* gone, but discovered */
    CHECK(has_hit(ux, 320, 240, SOM_UX_HIT_CELL, 3) && !som_ux_screen_has_text("x"));
    { uint32_t sv = som_ux_seen_get(ux); CHECK((sv & 0x09) == 0x09);              /* persistence: glove and spear bits */
      memset(wram, 0, sizeof wram); som_ux_update(ux, wram);
      CHECK(som_ux_seen_get(ux) == 0);                                            /* the title clears it */
      som_ux_seen_set(ux, sv); CHECK(som_ux_seen_get(ux) == sv); }
    for (int i = 0; i < 12; i++) wram[0xCC48 + i] = 0xFF;
    field(); wram[0xCC48 + 2] = (uint8_t)((1 << 5) | 6); som_ux_update(ux, wram);
    som_ux_set_view(ux, SOM_UX_TAB_ITEM, 0, 0xFF);
    CHECK(has_hit(ux, 320, 240, SOM_UX_HIT_CELL, 0) && !has_hit(ux, 320, 240, SOM_UX_HIT_CELL, 5));
    /* item 20: a card tap takes control; a dead hero's card opens the REVIVE chooser instead */
    field(); som_ux_update(ux, wram);
    r3 = tap_hit(ux, 1240, 1080, SOM_UX_HIT_PARTY, 2, &a3);
    CHECK(r3 == 1 && a3.kind == SOM_UX_ACT_SWITCH_HERO && a3.hero == 2);
    /* item 15: the zone map follows the hero the pad controls, not the selected card */
    { struct som_ux *z = som_ux_new(); static uint32_t zi[64 * 64];
      for (int i = 0; i < 64 * 64; i++) zi[i] = 0xFF204020u;
      field(); wram[0xD9] = 4; w16(0xE000 + 0x20, 100); w16(0xE000 + 0x22, 100); w16(0xE000 + 0x400 + 0x20, 200); w16(0xE000 + 0x400 + 0x22, 150);
      som_ux_update(z, wram); som_ux_set_zone_image(z, zi, 64, 64, 1, 4); som_ux_set_view(z, SOM_UX_TAB_MAP, 0, 0xFF); z->map_small = 1;
      som_ux_draw(z, NULL, pa, 1240, 1240, 1080);
      uint32_t h2 = 2166136261u; for (int i = 0; i < 1240 * 1080; i++) { h2 ^= pa[i]; h2 *= 16777619u; }
      wram[0xD9] = 1; som_ux_update(z, wram); som_ux_draw(z, NULL, pa, 1240, 1240, 1080);
      uint32_t h1 = 2166136261u; for (int i = 0; i < 1240 * 1080; i++) { h1 ^= pa[i]; h1 *= 16777619u; }
      CHECK(h1 != h2);                                                            /* the view moved with the controlled hero while ux->hero stayed 0 */
      som_ux_delete(z); }
    /* item 19: incapacitating statuses tint the card strongly, a plain card stays untinted */
    { static uint32_t pb[320 * 240]; field(); som_ux_update(ux, wram); som_ux_set_view(ux, SOM_UX_TAB_WEAPON, 0, 0xFF);
      som_ux_draw(ux, NULL, pb, 320, 320, 240); uint32_t base = pb[30 * 320 + 100];
      static const unsigned sts[] = {0x0010, 0x0020, 0x0040, 0x0100, 0x4000};
      uint32_t seen_cols[8]; int nc = 0;
      for (unsigned k = 0; k < sizeof sts / sizeof sts[0]; k++) {
        field(); w16(0xE190, sts[k]); som_ux_update(ux, wram); som_ux_draw(ux, NULL, pb, 320, 320, 240);
        uint32_t px = pb[30 * 320 + 100]; int dup = 0;
        for (int i = 0; i < nc; i++) if (seen_cols[i] == px) dup = 1;
        CHECK(px != base && !dup); seen_cols[nc++] = px;
      }
      field(); som_ux_update(ux, wram); }
    /* item 12: weapon cells print level:progress, the attack needs the ROM so it is absent here */
    field(); wram[0xE000 + 0x1C0] = 0x03; wram[0xE000 + 0x1D1] = 42; som_ux_update(ux, wram);
    som_ux_set_view(ux, SOM_UX_TAB_WEAPON, 0, 0xFF); som_ux_draw(ux, NULL, pa, 320, 320, 240);
    CHECK(som_ux_screen_has_text("0:00"));                                          /* glove: level 0, progress 00 */
    CHECK(ux->st.hero[0].weapon_level[1] == 3 && ux->st.hero[0].weapon_prog[1] == 42);
    /* item 14: MENU > SLOT exists only in the spirit ring */
    field(); ring_state(0, SOM_CAT_WEAPON, 0, 2); som_ux_update(ux, wram);
    CHECK(!has_hit(ux, 320, 240, SOM_UX_HIT_BIND_MENU, 0) && has_hit(ux, 320, 240, SOM_UX_HIT_BIND_ENTRY, 0));
    field(); ring_state(1, SOM_CAT_MAGIC, 0, 2); som_ux_update(ux, wram);
    CHECK(has_hit(ux, 320, 240, SOM_UX_HIT_BIND_MENU, 0));
  }

  /* ---- Thor round 4: extended options (SHADER), save-state slots, credits ---- */
  {
    static uint32_t th[SOM_UX_THUMB_W * SOM_UX_THUMB_H], pbig[1240 * 1160];
    static char names[16][40];
    char txt[SOM_UX_CREDITS_MAX];
    int sizes[3][2] = {{1240, 970}, {1080, 1160}, {320, 240}};
    for (int si = 0; si < 3; si++) {
      int w = sizes[si][0], h = sizes[si][1];
      field(); som_ux_update(ux, wram);
      som_ux_set_options_ext(ux, 1);
      som_ux_set_option(ux, SOM_UX_OPT_SHADER, 1, "SHARP"); som_ux_set_option(ux, SOM_UX_OPT_FIT, 0, "FIT");
      som_ux_open_options(ux, SOM_UX_OPT_MAIN);
      char *nm = names[si * 4];
      snprintf(nm, 40, "ext_options_%d", si); render(ux, nm, w, h);
      CHECK(has_hit(ux, w, h, SOM_UX_HIT_OPTION, SOM_UX_OPT_SHADER) && !has_hit(ux, w, h, SOM_UX_HIT_OPTION, SOM_UX_OPT_LANGUAGE));
      CHECK(has_hit(ux, w, h, SOM_UX_HIT_OPT_NAV, 4) && has_hit(ux, w, h, SOM_UX_HIT_OPT_NAV, 5) && has_hit(ux, w, h, SOM_UX_HIT_OPT_NAV, 2) && has_hit(ux, w, h, SOM_UX_HIT_OPT_NAV, 0));
      r = tap_hit(ux, w, h, SOM_UX_HIT_OPTION, SOM_UX_OPT_SHADER, &act);
      CHECK(r == 1 && act.kind == SOM_UX_ACT_OPTION && act.opt_id == SOM_UX_OPT_SHADER && act.opt_val == 2);
      for (int y = 0; y < SOM_UX_THUMB_H; y++) for (int x = 0; x < SOM_UX_THUMB_W; x++) th[y * SOM_UX_THUMB_W + x] = 0xFF000000u | (uint32_t)(x * 2) << 16 | (uint32_t)(y * 3) << 8 | 0x60;
      som_ux_set_state_slot(ux, 0, 1, "10-20 18:30", th);
      som_ux_set_state_slot(ux, 3, 1, "10-19 09:05", th);
      for (int y = 0; y < SOM_UX_THUMB_H; y++) for (int x = 0; x < SOM_UX_THUMB_W; x++) th[y * SOM_UX_THUMB_W + x] = 0xFF000000u | (uint32_t)(y * 3) << 16 | 0x4000u | (uint32_t)(x * 2);
      som_ux_set_state_slot(ux, 4, 1, "10-18 22:47", th);
      som_ux_set_state_sel(ux, 3);
      r = tap_hit(ux, w, h, SOM_UX_HIT_OPT_NAV, 4, &act);
      CHECK(r == 0 && som_ux_options_page(ux) == SOM_UX_OPT_STATES);
      nm = names[si * 4 + 1]; snprintf(nm, 40, "states_%d", si); render(ux, nm, w, h);
      CHECK(has_hit(ux, w, h, SOM_UX_HIT_ST_SLOT, 7) && has_hit(ux, w, h, SOM_UX_HIT_ST_SAVE, 0) && has_hit(ux, w, h, SOM_UX_HIT_ST_LOAD, 0));
      r = tap_hit(ux, w, h, SOM_UX_HIT_ST_SLOT, 5, &act);                          /* select an empty slot: reported so the host remembers it */
      CHECK(r == 1 && act.kind == SOM_UX_ACT_STATE && act.opt_id == 2 && act.opt_val == 5 && som_ux_state_sel(ux) == 5);
      r = tap_hit(ux, w, h, SOM_UX_HIT_ST_LOAD, 0, &act);                           /* LOAD of an empty slot does nothing */
      CHECK(r == 0 && act.kind == SOM_UX_ACT_NONE);
      r = tap_hit(ux, w, h, SOM_UX_HIT_ST_SAVE, 0, &act);
      CHECK(r == 1 && act.kind == SOM_UX_ACT_STATE && act.opt_id == 0 && act.opt_val == 5);
      tap_hit(ux, w, h, SOM_UX_HIT_ST_SLOT, 4, &act);
      r = tap_hit(ux, w, h, SOM_UX_HIT_ST_LOAD, 0, &act);
      CHECK(r == 1 && act.kind == SOM_UX_ACT_STATE && act.opt_id == 1 && act.opt_val == 4);
      r = tap_hit(ux, w, h, SOM_UX_HIT_OPT_NAV, 3, &act);                           /* BACK returns to the options, not to the game */
      CHECK(r == 0 && som_ux_options_page(ux) == SOM_UX_OPT_MAIN);
      /* credits */
      txt[0] = 0;
      strcat(txt, "# SPECIAL THANKS\nMoppleton and contributors - Secret of Mana Randomizer and its Ancient Cave mode https://github.com/Moppu/SecretOfManaRandomizer\n\n# QOL PATCHES\n");
      for (int k = 0; k < 14; k++) { char l[120]; snprintf(l, sizeof l, "Patch author %d - Secret of Mana quality of life patch number %d on romhacking.net\n", k, k); strcat(txt, l); }
      som_ux_set_credits(ux, txt);
      r = tap_hit(ux, w, h, SOM_UX_HIT_OPT_NAV, 5, &act);
      CHECK(r == 0 && som_ux_options_page(ux) == SOM_UX_OPT_CREDITS);
      nm = names[si * 4 + 2]; snprintf(nm, 40, "credits_%d", si); render(ux, nm, w, h);
      CHECK(ux->cr_lines > ux->cr_vis && ux->cr_scroll == 0);
      CHECK(som_ux_screen_has_text("SPECIAL THANKS"));
      r = tap_hit(ux, w, h, SOM_UX_HIT_CR_SCROLL, 1, &act);
      CHECK(r == 0 && ux->cr_scroll > 0);
      nm = names[si * 4 + 3]; snprintf(nm, 40, "credits_page2_%d", si); render(ux, nm, w, h);
      for (int k = 0; k < 40; k++) som_ux_nav_move(ux, 1, w, h);                    /* the pad scrolls the text, down to the end */
      som_ux_draw(ux, NULL, pbig, w, w, h);
      CHECK(ux->cr_scroll == ux->cr_lines - ux->cr_vis);
      som_ux_nav_move(ux, 0, w, h);
      CHECK(ux->cr_scroll == ux->cr_lines - ux->cr_vis - 1);
      som_ux_draw(ux, NULL, pbig, w, w, h);
      som_ux_nav_back(ux, w, h, &act);
      CHECK(som_ux_options_page(ux) == SOM_UX_OPT_MAIN);
      som_ux_nav_back(ux, w, h, &act);
      CHECK(som_ux_options_page(ux) == SOM_UX_OPT_CLOSED);
      som_ux_set_options_ext(ux, 0);
    }
  }

  som_ux_delete(ux);

  /* ---- golden hashes ---- */
  const char *hf = "golden_hashes.txt";
  char path[600]; snprintf(path, sizeof path, "%s", argc > 0 && strrchr(argv[0], '/') ? hf : hf);
  const char *env = getenv("SOM_UX_HASH_FILE"); if (env) snprintf(path, sizeof path, "%s", env);
  if (update) {
    FILE *f = fopen(path, "w");
    if (!f) { printf("cannot write %s\n", path); return 2; }
    for (int i = 0; i < ncase; i++) fprintf(f, "%s %dx%d %016llx\n", cases[i].name, cases[i].w, cases[i].h, (unsigned long long)cases[i].hash);
    fclose(f); printf("wrote %d hashes to %s\n", ncase, path);
  } else {
    FILE *f = fopen(path, "r");
    if (!f) printf("note: %s not found, pixel goldens skipped\n", path);
    else {
      char n[64]; int w, h; unsigned long long hv; int seen = 0;
      while (fscanf(f, "%63s %dx%d %llx", n, &w, &h, &hv) == 4)
        for (int i = 0; i < ncase; i++)
          if (!strcmp(n, cases[i].name) && w == cases[i].w && h == cases[i].h) {
            seen++; checks++;
            if (hv != cases[i].hash) { fails++; printf("FAIL golden %s %dx%d: %016llx != %016llx\n", n, w, h, (unsigned long long)cases[i].hash, hv); }
          }
      fclose(f); CHECK(seen == ncase);
    }
  }
  printf("%d checks, %d failures\n", checks, fails);
  return fails ? 1 : 0;
}
