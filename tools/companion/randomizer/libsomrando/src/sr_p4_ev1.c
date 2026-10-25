/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Open World events, hand written part.  Derived from processing/openworld/events/{NekoEvents,PokedexEvent}.cs (Author: Moppleton).
 * Translated to C 2026-10-18.  The straight-line hacks are generated (sr_p4_ev1_gen.c).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sr_p4.h"
#include "sr_p4_ev1.h"

/* ---- NekoEvents */
static void neko_buy_sell(struct sr_ctx *c, struct sr_bytes *e, int buy_menu_event) {
  static const uint8_t xs[2] = {5, 10};
  static const char *const text[2] = {"Buy", "Sell )"};
  uint8_t cmds[2][2];
  memcpy(cmds[0], EV1_JUMP_CMD(buy_menu_event), 2);
  memcpy(cmds[1], EV1_JUMP_CMD(0x313), 2);
  sr_p4_ev1_dialogue_choices(c, e, "Buy some more crap?\n   (", 2, xs, text, (const uint8_t(*)[2])cmds, EV1_JUMP_CMD(0xFF));
}

static void neko_save_buy_sell(struct sr_ctx *c, struct sr_bytes *e, int buy_menu_event) {
  static const uint8_t xs[3] = {5, 12, 18};
  static const char *const text[3] = {"Save", "Buy", "Sell )"};
  uint8_t cmds[3][2];
  memcpy(cmds[0], EV1_JUMP_CMD(0x4F), 2);
  memcpy(cmds[1], EV1_JUMP_CMD(buy_menu_event), 2);
  memcpy(cmds[2], EV1_JUMP_CMD(0x313), 2);
  sr_p4_ev1_dialogue_choices(c, e, "   Never trust autosave!\n    Buy some more crap?\n   (", 3, xs, text, (const uint8_t(*)[2])cmds, EV1_JUMP_CMD(0xFF));
}

static void neko_remove_at(const char **neko, int *n_neko, int idx) {
  for (int i = idx; i < *n_neko - 1; i++) neko[i] = neko[i + 1];
  (*n_neko)--;
}

int sr_p4_ev_neko(struct sr_ctx *c) {
  struct sr_random *r = &c->rf;
  const char *neko[16];
  int n_neko = 0;
#include "sr_p4_ev1_neko_gen.inc"

  /* 0x1d7: gaia's navel neko dialogue */
  struct sr_bytes *e = sr_event_new(c, 0x1d7);
  sr_p4_ev1_open_dialogue(c, e);
  int idx = sr_random_next(r) % n_neko;
  sr_p4_ev1_dialogue_nowait(c, e, neko[idx]);
  neko_remove_at(neko, &n_neko, idx);
  sr_ev_jump(c, e, 0x1DF);
  sr_ev_end(c, e);

  /* 0x318: mountain neko dialogue (the line is not removed from the pool) */
  idx = sr_random_next(r) % n_neko;
  e = sr_event_new(c, 0x318);
  sr_ev_set_flag(c, e, EVF_BUY_SHOP_ID_FLAG, 0xC);
  sr_p4_ev1_add(c, e, EVC_OPEN_DIALOGUE);
  sr_p4_ev1_dialogue(c, e, neko[idx], 0);
  sr_p4_ev1_add(c, e, EVC_CLEAR_DIALOGUE);
  sr_p4_ev1_add(c, e, 0x06);
  sr_p4_ev1_add(c, e, EVC_GRAPHICAL_EFFECT);
  sr_p4_ev1_add(c, e, 0x08);
  sr_p4_ev1_add(c, e, EVC_OPEN_GP);
  sr_p4_ev1_add(c, e, 0x5F);
  neko_save_buy_sell(c, e, 0x312);
  sr_ev_end(c, e);

  /* 0x319: lost continent neko dialogue */
  e = sr_event_new(c, 0x319);
  sr_p4_ev1_open_dialogue(c, e);
  idx = sr_random_next(r) % n_neko;
  sr_p4_ev1_dialogue(c, e, neko[idx], 0);
  neko_remove_at(neko, &n_neko, idx);
  sr_ev_jsr(c, e, 0x480);
  sr_p4_ev1_add(c, e, EVC_OPEN_GP);
  sr_p4_ev1_add(c, e, 0x5F);
  neko_buy_sell(c, e, 0x30F);
  sr_ev_end(c, e);

  /* 0x1de: witch's castle neko dialogue */
  e = sr_event_new(c, 0x1de);
  sr_p4_ev1_open_dialogue(c, e);
  idx = sr_random_next(r) % n_neko;
  sr_p4_ev1_dialogue_nowait(c, e, neko[idx]);
  neko_remove_at(neko, &n_neko, idx);
  sr_ev_jump(c, e, 0x1DF);
  sr_ev_end(c, e);

  /* 0x23c: upper land neko dialogue */
  e = sr_event_new(c, 0x23c);
  sr_p4_ev1_open_dialogue(c, e);
  idx = sr_random_next(r) % n_neko;
  sr_p4_ev1_dialogue(c, e, neko[idx], 0);
  neko_remove_at(neko, &n_neko, idx);
  sr_p4_ev1_add(c, e, EVC_CLEAR_DIALOGUE);
  sr_ev_jsr(c, e, 0x480);
  sr_p4_ev1_add(c, e, EVC_OPEN_GP);
  sr_p4_ev1_add(c, e, 0x5F);
  neko_save_buy_sell(c, e, 0x306);
  sr_ev_end(c, e);

  /* 0x65f: ice country neko dialogue */
  e = sr_event_new(c, 0x65f);
  sr_ev_set_flag(c, e, EVF_BUY_SHOP_ID_FLAG, 8);
  sr_p4_ev1_add(c, e, 0x06);
  sr_p4_ev1_add(c, e, EVC_GRAPHICAL_EFFECT);
  sr_p4_ev1_add(c, e, 0x08);
  sr_p4_ev1_open_dialogue(c, e);
  idx = sr_random_next(r) % n_neko;
  sr_p4_ev1_dialogue(c, e, neko[idx], 0);
  neko_remove_at(neko, &n_neko, idx);
  sr_p4_ev1_add(c, e, EVC_CLEAR_DIALOGUE);
  sr_ev_jsr(c, e, 0x480);
  sr_p4_ev1_add(c, e, EVC_OPEN_GP);
  sr_p4_ev1_add(c, e, 0x5F);
  neko_save_buy_sell(c, e, 0x308);
  sr_ev_end(c, e);
  return 1;
}

/* ---- PokedexEvent */
int sr_p4_ev_pokedex(struct sr_ctx *c) {
  struct sr_random *r = &c->rf;
  if (sr_res_pokedex_count <= 0) sr_fail(c, SOMRANDO_ERR_UNSUPPORTED, "unsupported option: pokedex resource is not built into this library");
  const struct sr_p4_ev1_pokemon *p = &sr_res_pokedex[sr_random_next(r) % sr_res_pokedex_count];
  int poke_num = atoi(p->id), generation = 0;
  if (poke_num >= 1 && poke_num <= 151) generation = 1;
  if (poke_num >= 152 && poke_num <= 251) generation = 2;
  if (poke_num >= 252 && poke_num <= 386) generation = 3;
  if (poke_num >= 387 && poke_num <= 493) generation = 4;
  if (strcmp(p->id, "0") && p->name[0] && p->description[0] && generation) {
    struct sr_bytes *e = sr_event_new(c, 0x143);   /* lemon-lime colored lady in the upper right of pandora town */
    char head[256];
    snprintf(head, sizeof head, "Pokemon of the Day:\n No. %s: %s\n Generation %d\n", p->id, p->name, generation);
    sr_p4_ev1_dialogue_box(c, e, sr_p4_ev1_concat(c, head, sr_p4_ev1_word_wrap(c, p->description)));
    sr_ev_end(c, e);
  }
  return 1;
}

/* Every option the event hacks read is reproduced; only the build-time resource can be missing. */
int sr_p4_ev1_supports(const struct sr_settings *s, char *why, size_t why_len) {
  (void)s;
  if (sr_res_pokedex_count <= 0) {
    snprintf(why, why_len, "unsupported option: pokedex resource is not built into this library");
    return 0;
  }
  return 1;
}
