/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Chaos mode (group w9): the generated events.  Derived from processing/chaos/ChaosEventMaker.cs (Author: Moppleton).
 * Translated to C 2026-10-18.
 */
#include <stdio.h>
#include <string.h>

#include "sr_core.h"
#include "sr_p2_chaos_int.h"

#define SB(key) sr_settings_get_bool(c->settings, (key))

#define EVC_PLAY_SOUND 0x40
#define EVC_ADD_INVENTORY_ITEM 0x1E
#define EVC_GRAPHICAL_EFFECT 0x2D
#define EVC_OPEN_GP 0x5D
#define EVC_SELL_MENU 0x0E
#define EVC_BUY_MENU 0x2E
#define EVC_CLOSE_GP 0x5E
#define EVC_WEAPON_UPGRADE_MENU 0x0F
#define EVC_HEAL 0x2F
#define EVC_SET_CHARACTER_ATTRIBUTE 0x39
#define EVC_ADD_GOLD 0x36
#define EVC_NAME_CHARACTER 0x1F
#define EVC_ADD_CHARACTER 0x2B
#define EVC_REMOVE_CHARACTER 0x2C
#define EVC_CUSTOM_EVENT_COMMANDS 0x57

#define FLAG_UI_DISPLAY 0x00
#define FLAG_DEATH_TYPE 0x0B
#define FLAG_TIMER_RUNNING 0x27
#define FLAG_CUSTOM_MUSIC 0x28
#define FLAG_MANAFORT_SWITCHES_1 0x4E
#define FLAG_TOTAL_MANA_POWER 0xFC
#define FLAG_FREEZE_BOSS_AI 0xFF

#define NUM_SHOPS 16
#define MAX_BOSSES 25

static void ev_add(struct sr_ctx *c, struct sr_bytes *e, int b) { sr_bytes_push(c, e, b); }

static void ev_dialogue(struct sr_ctx *c, struct sr_bytes *e, const char *text) {
  sr_event_text_bytes(c, e, text);
  sr_ev_sleep(c, e, 0);
}

static void ev_dialogue_box(struct sr_ctx *c, struct sr_bytes *e, const char *text) {
  ev_add(c, e, EV_OPEN_DIALOGUE);
  ev_dialogue(c, e, text);
  ev_add(c, e, EV_CLOSE_DIALOGUE);
}

/* AddDialogueBoxWithChoices; every command is a 2-byte command */
static void ev_choices_box(struct sr_ctx *c, struct sr_bytes *e, const char *text, int n, const uint8_t *xs, const char *const *labels,
                           const uint8_t cmds[][2], const uint8_t cancel[2]) {
  ev_add(c, e, EV_OPEN_DIALOGUE);
  sr_event_text_bytes(c, e, text);
  ev_add(c, e, 0x58);   /* OPTION_SELECTION */
  for (int i = 0; i < n; i++) {
    ev_add(c, e, 0x5A);   /* OPTION_AT */
    ev_add(c, e, xs[i]);
    sr_event_text_bytes(c, e, labels[i]);
  }
  ev_add(c, e, 0x5B);   /* OPTION_SELECTION_END */
  ev_add(c, e, cancel[0]);
  ev_add(c, e, cancel[1]);
  for (int i = 0; i < n; i++) {
    ev_add(c, e, cmds[i][0]);
    ev_add(c, e, cmds[i][1]);
  }
}

static void ev_play(struct sr_ctx *c, struct sr_bytes *e, int a, int b, int d, int f) {
  ev_add(c, e, EVC_PLAY_SOUND);
  ev_add(c, e, a);
  ev_add(c, e, b);
  ev_add(c, e, d);
  ev_add(c, e, f);
}

static void key_int(struct sr_ctx *c, const char *prefix, int i, int v) {
  char key[64];
  snprintf(key, sizeof key, "%s%d", prefix, i);
  sr_work_set_int(c, key, v);
}

int sr_w9_hack_chaos_events(struct sr_ctx *c) {
  static const int seed_old_events[8] = {0x588, 0x58b, 0x58a, 0x589, 0x58c, 0x58d, 0x58e, 0x58f};
  struct sr_random *r = &c->rf;

  int event_num = 0x100;
  sr_work_set_int(c, "initialWalkonEventNum", event_num);
  struct sr_bytes *intro = sr_event_new(c, event_num++);
  uint8_t jump0[2];
  sr_ev_jump_cmd(0, jump0);
  /* skip out if 00 is 1+ */
  sr_ev_logic(c, intro, FLAG_UI_DISPLAY, 0x1, 0xF, jump0);
  /* ice castle theme */
  ev_play(c, intro, 0x01, 0x15, 0x12, 0x8F);
  /* enable UI */
  sr_ev_incr_flag(c, intro, FLAG_UI_DISPLAY);
  sr_ev_incr_flag(c, intro, FLAG_UI_DISPLAY);
  /* flag 0x0B -> 0x05 for normal death */
  sr_ev_set_flag(c, intro, FLAG_DEATH_TYPE, 5);
  static const uint8_t start_gear[7] = {0x80, 0x92, 0x9B, 0xA4, 0xAD, 0xB6, 0xBF};
  for (int i = 0; i < 7; i++) {
    ev_add(c, intro, EVC_ADD_INVENTORY_ITEM);
    ev_add(c, intro, start_gear[i]);
  }
  /* add gold */
  int starting_gold = w8_diff_int(c, "startingGold");
  if (starting_gold > 65535) starting_gold = 65535;
  if (starting_gold > 0) {
    ev_add(c, intro, EVC_ADD_GOLD);
    ev_add(c, intro, (uint8_t)starting_gold);
    ev_add(c, intro, (uint8_t)(starting_gold >> 8));
  }
  /* name girl/sprite */
  if (SB("chGirl")) {
    ev_add(c, intro, EVC_NAME_CHARACTER);
    ev_add(c, intro, 0x01);
  }
  if (SB("chSprite")) {
    ev_add(c, intro, EVC_NAME_CHARACTER);
    ev_add(c, intro, 0x02);
  }
  /* Add p2, p3 */
  if (SB("chGirl")) {
    ev_add(c, intro, EVC_ADD_CHARACTER);
    ev_add(c, intro, 0x01);
  }
  if (SB("chSprite")) {
    ev_add(c, intro, EVC_ADD_CHARACTER);
    ev_add(c, intro, 0x02);
  }
  int starting_consumables = w8_diff_int(c, "startingConsumables");
  /* candy, chocolate, jam, herb, walnut, cup */
  static const uint8_t starting_items[6] = {0x40, 0x41, 0x42, 0x43, 0x44, 0x45};
  for (int k = 0; k < 6; k++)
    for (int i = 0; i < starting_consumables; i++) {
      ev_add(c, intro, EVC_ADD_INVENTORY_ITEM);
      ev_add(c, intro, starting_items[k]);
    }
  /* flammie drum for music changing */
  ev_add(c, intro, EVC_ADD_INVENTORY_ITEM);
  ev_add(c, intro, 0x47);
  /* restore everyone's hp */
  ev_add(c, intro, EVC_HEAL);
  ev_add(c, intro, 0x00);
  double mana_power_base = w8_diff(c, "manaPower_base");
  /* mana power at starting value */
  sr_ev_set_flag(c, intro, FLAG_TOTAL_MANA_POWER, (uint8_t)mana_power_base);
  for (int i = 0; i < (int)mana_power_base; i++) sr_ev_set_flag(c, intro, 0x90 + i, 1);
  ev_dialogue_box(c, intro,
                  "Chaos mode should work\n"
                  "a little better than\n"
                  "Ancient Cave, but may\n"
                  "still have some major\n"
                  "stat balancing issues.\n"
                  "Enjoy!\n");
  /* remove boy character if applicable */
  if (!SB("chBoy")) {
    ev_add(c, intro, EVC_REMOVE_CHARACTER);
    ev_add(c, intro, 0x00);
  }
  /* set timer flag on */
  sr_ev_set_flag(c, intro, FLAG_TIMER_RUNNING, 1);
  sr_ev_end(c, intro);

  /* Watts event */
  sr_work_set_int(c, "wattsEventNum", event_num);
  struct sr_bytes *watts = sr_event_new(c, event_num++);
  ev_add(c, watts, EVC_OPEN_GP);
  ev_add(c, watts, 0x5F);
  ev_dialogue_box(c, watts, "WATTS:Okay!\n Which one's ready?");
  ev_add(c, watts, EVC_WEAPON_UPGRADE_MENU);
  ev_add(c, watts, EVC_CLOSE_GP);
  sr_ev_end(c, watts);

  int cancel_event_num = event_num;
  struct sr_bytes *cancel = sr_event_new(c, event_num++);
  ev_add(c, cancel, 0x5E);   /* close GP */
  ev_add(c, cancel, 0x00);   /* end */

  int sell_event_num = event_num;
  struct sr_bytes *sell = sr_event_new(c, event_num++);
  /* center camera */
  ev_add(c, sell, EVC_GRAPHICAL_EFFECT);
  ev_add(c, sell, 0x08);
  ev_add(c, sell, EVC_SELL_MENU);
  ev_add(c, sell, EVC_CLOSE_GP);
  sr_ev_end(c, sell);

  uint8_t cancel_cmd[2];
  sr_ev_jump_cmd(cancel_event_num, cancel_cmd);
  for (int i = 0; i < NUM_SHOPS; i++) {
    key_int(c, "shopEventId", i, event_num);
    struct sr_bytes *shop = sr_event_new(c, event_num++);
    ev_add(c, shop, EVC_OPEN_GP);
    ev_add(c, shop, 0x5F);
    {
      static const uint8_t xs[3] = {2, 8, 0x13};
      static const char *const labels[3] = {"Buy", "Sell all", "Sell"};
      uint8_t cmds[3][2];
      sr_ev_jump_cmd(event_num, cmds[0]);
      sr_ev_jump_cmd(event_num + 1, cmds[1]);
      sr_ev_jump_cmd(sell_event_num, cmds[2]);
      ev_choices_box(c, shop, "Shop? Meow.\n", 3, xs, labels, cmds, cancel_cmd);
    }
    sr_ev_end(c, shop);

    /* buy event */
    struct sr_bytes *buy = sr_event_new(c, event_num++);
    ev_add(c, buy, EVC_GRAPHICAL_EFFECT);
    ev_add(c, buy, 0x08);
    ev_add(c, buy, EVC_BUY_MENU);
    ev_add(c, buy, i);
    ev_add(c, buy, EVC_CLOSE_GP);
    sr_ev_end(c, buy);

    /* sell all confirmation event */
    struct sr_bytes *confirm = sr_event_new(c, event_num++);
    {
      static const uint8_t xs[2] = {2, 8};
      static const char *const labels[2] = {"Yep", "Nope"};
      uint8_t cmds[2][2];
      sr_ev_jump_cmd(event_num, cmds[0]);
      sr_ev_jump_cmd(cancel_event_num, cmds[1]);
      ev_choices_box(c, confirm, "Sell all unequipped gear?\n", 2, xs, labels, cmds, cancel_cmd);
    }
    sr_ev_end(c, confirm);

    /* sellall event */
    struct sr_bytes *sell_all = sr_event_new(c, event_num++);
    ev_add(c, sell_all, EVC_CUSTOM_EVENT_COMMANDS);
    ev_add(c, sell_all, w8_command_index(c, "sell all"));
    ev_add(c, sell_all, EV_CLOSE_DIALOGUE);
    ev_add(c, sell_all, EVC_CLOSE_GP);
    sr_ev_end(c, sell_all);
  }

  /* 4 is lumina 5 is shade */
  static const uint8_t element_attribute_flags[8] = {0xC8, 0xC9, 0xCA, 0xCB, 0xCF, 0xCE, 0xCC, 0xCD};
  for (int i = 0; i < 8; i++) {
    key_int(c, "elementEventId", i, event_num);
    struct sr_bytes *el = sr_event_new(c, event_num++);
    uint8_t cmd[2];
    sr_ev_jump_cmd(event_num, cmd);
    /* event flag check - skip this and tell me to go away (next event) */
    sr_ev_logic(c, el, 0x10 + i, 0x1, 0xF, cmd);
    sr_ev_incr_flag(c, el, 0x10 + i);
    if (i != 5) {   /* girl; not shade */
      ev_add(c, el, EVC_SET_CHARACTER_ATTRIBUTE);
      ev_add(c, el, 0x02);
      ev_add(c, el, element_attribute_flags[i]);
      ev_add(c, el, i == 7 ? 0x18 : 0x38);   /* dryad: no mana magic */
    }
    if (i != 4) {   /* sprite; not lumina */
      ev_add(c, el, EVC_SET_CHARACTER_ATTRIBUTE);
      ev_add(c, el, 0x03);
      ev_add(c, el, element_attribute_flags[i]);
      ev_add(c, el, i == 7 ? 0x03 : 0x07);
    }
    ev_dialogue_box(c, el, "Here, have some magic.");
    sr_ev_end(c, el);

    /* next event: already got it */
    struct sr_bytes *away = sr_event_new(c, event_num++);
    ev_dialogue_box(c, away, "Ok go away now.");
    sr_ev_end(c, away);
  }

  /* boss entry / skip / death events; the entry and skip events carry placeholder door commands for the map generator */
  int boss_event_flag = 0x30;
  int boss_event_value = 1;
  sr_work_set_int(c, "bossEntryEventNum", MAX_BOSSES);
  sr_work_set_int(c, "bossDeathEventNum", MAX_BOSSES);
  for (int i = 0; i <= MAX_BOSSES; i++) {
    if (i < MAX_BOSSES) key_int(c, "bossEntryEventId", i, event_num);
    else sr_work_set_int(c, "manaBeastWalkonEventNum", event_num);
    struct sr_bytes *entry = sr_event_new(c, event_num++);
    uint8_t cmd[2];
    sr_ev_jump_cmd(event_num, cmd);
    /* skip if event flag > bossEventValue */
    sr_ev_logic(c, entry, boss_event_flag, boss_event_value + 1, 0xF, cmd);
    /* BOSS_DOOR_INJECTION_PATTERN */
    ev_add(c, entry, 0xEE);
    ev_add(c, entry, 0xEE);
    ev_add(c, entry, 0xEE);
    /* disable custom music */
    sr_ev_set_flag(c, entry, FLAG_CUSTOM_MUSIC, 0);
    if (i == MAX_BOSSES) sr_ev_jsr(c, entry, 0x739);   /* the mana beast theme */
    else ev_play(c, entry, 0x01, 0x04, 0x04, 0xFF);
    sr_ev_end(c, entry);

    int boss_skip_event_num = event_num;
    struct sr_bytes *skip = sr_event_new(c, event_num++);
    /* NEXT_FLOOR_DOOR_INJECTION_PATTERN */
    ev_add(c, skip, 0xFF);
    ev_add(c, skip, 0xFF);
    ev_add(c, skip, 0xFF);
    sr_ev_end(c, skip);

    if (i < MAX_BOSSES) key_int(c, "bossDeathEventId", i, event_num);
    struct sr_bytes *death = sr_event_new(c, event_num++);
    /* random orb prize */
    sr_ev_jsr(c, death, 0x500 + (uint8_t)(sr_random_next(r) % 8));
    sr_ev_set_flag(c, death, FLAG_CUSTOM_MUSIC, 0);
    sr_ev_set_flag(c, death, boss_event_flag, boss_event_value + 1);
    ev_play(c, death, 0x01, 0x15, 0x12, 0x8F);
    sr_ev_jump(c, death, boss_skip_event_num);
    sr_ev_end(c, death);

    boss_event_value++;
    if (boss_event_value == 0x0F) {
      boss_event_value = 0x01;
      boss_event_flag++;
    }
  }

  sr_work_set_int(c, "manaBeastDeathEventNum", event_num);
  struct sr_bytes *mb = sr_event_new(c, event_num++);
  sr_ev_incr_flag(c, mb, FLAG_MANAFORT_SWITCHES_1);
  sr_ev_incr_flag(c, mb, FLAG_FREEZE_BOSS_AI);
  sr_ev_set_flag(c, mb, FLAG_TIMER_RUNNING, 0);
  /* placeholder for door to ending map */
  ev_add(c, mb, 0xFF);
  ev_add(c, mb, 0xFF);
  ev_add(c, mb, 0xFF);
  ev_play(c, mb, 0x01, 0x3a, 0x1b, 0x8f);
  ev_add(c, mb, EV_OPEN_DIALOGUE);
  ev_dialogue(c, mb, "Hooray! You did it.\nTime: ");
  ev_add(c, mb, EVC_CUSTOM_EVENT_COMMANDS);
  ev_add(c, mb, w8_command_index(c, "print timer"));
  ev_dialogue(c, mb, "\nThe end!");
  ev_add(c, mb, EV_CLOSE_DIALOGUE);
  ev_add(c, mb, 0x1f);   /* end game */
  ev_add(c, mb, 0x11);
  sr_ev_end(c, mb);

  /* seed event fix: add the missing mana level increment (25 94) after 25 90 in events 588-58f as needed */
  for (int i = 0; i < 8; i++) {
    key_int(c, "seedEventId", i, event_num);
    int start = sr_event_start_offset(c, c->orig, seed_old_events[i]);
    int next = sr_event_start_offset(c, c->orig, seed_old_events[i] + 1);
    int count = next - start;
    const uint8_t *copied = c->orig + start;
    struct sr_bytes *ne = sr_event_new(c, event_num++);
    for (int o = 0; o < count; o++) {
      ev_add(c, ne, copied[o]);
      if (o > 0 && o < count - 2 && copied[o - 1] == 0x25 && copied[o] == 0x90 && (copied[o + 1] != 0x25 || copied[o + 2] != 0x94)) {
        ev_add(c, ne, 0x25);
        ev_add(c, ne, 0x94);
      }
    }
  }
  return 1;
}
