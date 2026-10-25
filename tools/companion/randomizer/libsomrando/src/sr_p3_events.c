/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Ancient Cave event maker.  Derived from processing/ancientcave/AncientCaveEventMaker.cs (class NpcTaggedEvent included), Author: Moppleton.
 * Translated to C 2026-10-18.  The comedian quote pools are separate data (data/, see sr_p3_quotes.h).
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "sr_p3_ac.h"
#include "sr_p3_events.h"
#include "sr_p3_quotes.h"

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
#define EVC_REMOVE_GOLD 0x37
#define EVC_NAME_CHARACTER 0x1F
#define EVC_ADD_CHARACTER 0x2B
#define EVC_REMOVE_CHARACTER 0x2C
#define EVC_CUSTOM_EVENT_COMMANDS 0x57

#define FLAG_UI_DISPLAY 0x00
#define FLAG_DEATH_TYPE 0x0B
#define FLAG_NOT_ENOUGH_GOLD 0x0F
#define FLAG_BOSS_VISIBILITY 0x26
#define FLAG_TIMER_RUNNING 0x27
#define FLAG_CUSTOM_MUSIC 0x28
#define FLAG_MANAFORT_SWITCHES_1 0x4E
#define FLAG_TOTAL_MANA_POWER 0xFC
#define FLAG_FREEZE_BOSS_AI 0xFF

#define START_EVENT_NUM 0x100
#define MANABEAST_WALKON_EVENT_NUM 0x101
#define EXTENDED_DOOR_CONTINUE (0xFFFF - 0x800)
#define STATISTICS_BASE_OFFSET 0x7E0400

/* AncientCaveRandomizer.MUSIC_REPLACEMENT_PATTERN, UNKNOWN_DOOR_REPLACEMENT_PATTERN */
static const uint8_t music_pattern[5] = {0x29, 0xFB, 0x30, 0xFB, 0x00};
static const uint8_t door_pattern[3] = {0xFF, 0xFF, 0xFF};

/* ------------------------------------------------------------------------------------------------------- difficulty */
double sr_p3_ev_diff(struct sr_ctx *c, const char *key) {
  const char *d = c->req ? c->req->difficulty : NULL;
  int col = 0;
  if (d && d[0] && strcmp(d, "casual")) {
    if (!strcmp(d, "hard")) col = 1;
    else if (!strcmp(d, "reallyhard")) col = 2;
    else sr_fail(c, SOMRANDO_ERR_UNSUPPORTED, "unsupported option: Ancient Cave difficulty '%s'", d);
  }
  for (int i = 0; i < sr_p3_ev_diff_row_count; i++)
    if (!strcmp(sr_p3_ev_diff_rows[i].key, key)) return sr_p3_ev_diff_rows[i].v[col];
  sr_fail(c, SOMRANDO_ERR_FAILED, "unknown difficulty key %s", key);
}

static int diff_int(struct sr_ctx *c, const char *key) { return (int)sr_p3_ev_diff(c, key); }

/* ------------------------------------------------------------------------------------------------ EventScript helpers */
static void ev_add(struct sr_ctx *c, struct sr_bytes *e, int b) { sr_bytes_push(c, e, b); }

static void ev_range(struct sr_ctx *c, struct sr_bytes *e, const uint8_t *p, size_t n) { sr_bytes_push_n(c, e, p, n); }

static void ev_ext_door(struct sr_ctx *c, struct sr_bytes *e, int target_door) {
  int door_event_id = target_door + 0x800;
  ev_add(c, e, 0x18);
  ev_add(c, e, door_event_id);
  ev_add(c, e, door_event_id >> 8);
}

/* AddDialogue(text) = text bytes, then sleep(0) to wait for a button */
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

static void ev_jsr_cmd(int target_event, uint8_t out[2]) {
  out[0] = (uint8_t)(EV_JUMP_SUBR_BASE + (target_event >> 8));
  out[1] = (uint8_t)target_event;
}

static void ev_play(struct sr_ctx *c, struct sr_bytes *e, int a, int b, int d, int f) {
  ev_add(c, e, EVC_PLAY_SOUND);
  ev_add(c, e, a);
  ev_add(c, e, b);
  ev_add(c, e, d);
  ev_add(c, e, f);
}

static void ev_set_attr(struct sr_ctx *c, struct sr_bytes *e, int ch, int attr, int val) {
  ev_add(c, e, EVC_SET_CHARACTER_ATTRIBUTE);
  ev_add(c, e, ch);
  ev_add(c, e, attr);
  ev_add(c, e, val);
}

/* HEAL 04 / 40 / 84 and the status clears (attributes 90 and 91 of the three characters) shared by the restore and continue events */
static void ev_full_restore(struct sr_ctx *c, struct sr_bytes *e) {
  ev_add(c, e, EVC_HEAL);
  ev_add(c, e, 0x04);   /* all hp */
  ev_add(c, e, EVC_HEAL);
  ev_add(c, e, 0x40);   /* all mp? */
  ev_add(c, e, EVC_HEAL);
  ev_add(c, e, 0x84);   /* ?? */
  for (int ch = 1; ch <= 3; ch++) {
    ev_set_attr(c, e, ch, 0x90, 0x00);
    ev_set_attr(c, e, ch, 0x91, 0x00);
  }
}

static void key_int(struct sr_ctx *c, const char *prefix, int i, int v) {
  char key[64];
  snprintf(key, sizeof key, "%s%d", prefix, i);
  sr_work_set_int(c, key, v);
}

/* string.Replace(pat, repl) for an equal-length replacement, in place */
static void replace_same_len(char *s, const char *pat, const char *repl) {
  size_t n = strlen(pat);
  char *p = s;
  while ((p = strstr(p, pat)) != NULL) {
    memcpy(p, repl, n);
    p += n;
  }
}

static struct ac_npc_event *new_npc_event(struct sr_ctx *c) {
  struct ac_npc_event *e = (struct ac_npc_event *)sr_alloc(c, sizeof *e);
  e->flag_max = 0x0F;   /* NpcTaggedEvent defaults: npcId 0, flag id/min 0, max 0x0F, mobile */
  e->mobile = 1;
  return e;
}

void sr_p3_event_maker(struct sr_ctx *c, struct ac_npc_events *out) {
  memset(out, 0, sizeof *out);

  int boss_floors[AC_MAX_FLOORS];
  int n_boss = 0;

  const char *length = sr_settings_get(c->settings, "acLength");
  int num_maps;
  if (!strcmp(length, "short")) num_maps = 8;
  else if (!strcmp(length, "medium")) num_maps = 16;
  else if (!strcmp(length, "long")) num_maps = 24;
  else sr_fail(c, SOMRANDO_ERR_FAILED, "The given key '%s' was not present in the dictionary.", length);
  double scaling_rate = ceil(24.0 / num_maps);

  int walk_on[65];
  int event_num = START_EVENT_NUM;
  /* intro event */
  walk_on[0] = event_num;
  key_int(c, "walkonEvent", 0, event_num);
  sr_work_set_int(c, "walkonEventNum", 65);

  /* -------------------------------- event 0x100: game start */
  struct sr_bytes *cur = sr_event_new(c, event_num);
  uint8_t jump0[2];
  sr_ev_jump_cmd(0, jump0);

  /* skip out if 00 is 1+ */
  sr_ev_logic(c, cur, FLAG_UI_DISPLAY, 0x1, 0xF, jump0);
  /* enable UI */
  sr_ev_incr_flag(c, cur, FLAG_UI_DISPLAY);
  sr_ev_incr_flag(c, cur, FLAG_UI_DISPLAY);
  /* flag 0x0B -> 0x05 for normal death */
  sr_ev_set_flag(c, cur, FLAG_DEATH_TYPE, 5);
  /* glove, axe, spear, whip, bow, boomerang, javelin */
  {
    static const uint8_t gear[7] = {0x80, 0x92, 0x9B, 0xA4, 0xAD, 0xB6, 0xBF};
    for (int i = 0; i < 7; i++) {
      ev_add(c, cur, EVC_ADD_INVENTORY_ITEM);
      ev_add(c, cur, gear[i]);
    }
  }
  int starting_gold = diff_int(c, "startingGold");
  if (starting_gold > 65535) starting_gold = 65535;
  if (starting_gold > 0) {
    ev_add(c, cur, EVC_ADD_GOLD);
    ev_add(c, cur, starting_gold);
    ev_add(c, cur, starting_gold >> 8);
  }
  /* name girl/sprite */
  if (SB("acGirl")) {
    ev_add(c, cur, EVC_NAME_CHARACTER);
    ev_add(c, cur, 0x01);
  }
  if (SB("acSprite")) {
    ev_add(c, cur, EVC_NAME_CHARACTER);
    ev_add(c, cur, 0x02);
  }
  /* Add p2, p3 */
  if (SB("acGirl")) {
    ev_add(c, cur, EVC_ADD_CHARACTER);
    ev_add(c, cur, 0x01);
  }
  if (SB("acSprite")) {
    ev_add(c, cur, EVC_ADD_CHARACTER);
    ev_add(c, cur, 0x02);
  }
  int starting_consumables = diff_int(c, "startingConsumables");
  /* candy, chocolate, jam, herb, walnut, cup */
  {
    static const uint8_t items[6] = {0x40, 0x41, 0x42, 0x43, 0x44, 0x45};
    for (int k = 0; k < 6; k++)
      for (int i = 0; i < starting_consumables; i++) {
        ev_add(c, cur, EVC_ADD_INVENTORY_ITEM);
        ev_add(c, cur, items[k]);
      }
  }
  /* flammie drum, magic rope */
  ev_add(c, cur, EVC_ADD_INVENTORY_ITEM);
  ev_add(c, cur, 0x47);
  ev_add(c, cur, EVC_ADD_INVENTORY_ITEM);
  ev_add(c, cur, 0x46);
  /* restore everyone's hp */
  ev_add(c, cur, EVC_HEAL);
  ev_add(c, cur, 0x00);
  /* music injection pattern that the tileset for the map will replace */
  ev_range(c, cur, music_pattern, sizeof music_pattern);

  double mana_power_base = sr_p3_ev_diff(c, "manaPower_base");
  double mana_power_mul = sr_p3_ev_diff(c, "manaPower_growth") * scaling_rate;
  double mana_power_exp = sr_p3_ev_diff(c, "manaPower_exp");

  /* mana power base level, and the seeds up to it */
  sr_ev_set_flag(c, cur, FLAG_TOTAL_MANA_POWER, (uint8_t)(int)mana_power_base);
  for (int i = 0; i < (int)mana_power_base; i++) sr_ev_set_flag(c, cur, (uint8_t)(0x90 + i), 1);

  ev_dialogue_box(c, cur,
                  "Ancient Cave mode still\n"
                  "needs a lot of work, so\n"
                  "please be aware that not\n"
                  "everything may work well,\n"
                  "balancing issues may\n"
                  "exist, and softlocks\n"
                  "might happen.  Enjoy!\n");
  /* remove boy character if applicable */
  if (!SB("acBoy")) {
    ev_add(c, cur, EVC_REMOVE_CHARACTER);
    ev_add(c, cur, 0x00);
  }
  /* set timer flag on */
  sr_ev_set_flag(c, cur, FLAG_TIMER_RUNNING, 1);
  sr_ev_end(c, cur);

  /* -------------------------------- event 0x101: mana beast warp/intro */
  event_num++;
  cur = sr_event_new(c, event_num);
  sr_ev_set_flag(c, cur, FLAG_MANAFORT_SWITCHES_1, 8);
  /* play the mana beast theme by JSRing to vanilla event x739 */
  sr_ev_jsr(c, cur, 0x739);
  sr_ev_end(c, cur);

  double mana_power = mana_power_base;
  event_num++;   /* 0x102 */
  for (int i = 1; i <= 64; i++) {
    /* -------------------------------- event 0x102+: floor 2, 3, 4... entry events */
    cur = sr_event_new(c, event_num);
    double mana_power_this_floor = mana_power_base + pow((double)i, mana_power_exp) * mana_power_mul;
    if ((int)mana_power_this_floor > (int)mana_power) {
      sr_ev_set_flag(c, cur, FLAG_TOTAL_MANA_POWER, (uint8_t)(int)mana_power_this_floor);
      for (int j = (int)mana_power; j < (int)mana_power_this_floor && j <= 8; j++) sr_ev_set_flag(c, cur, (uint8_t)(0x90 + j), 1);
    }
    mana_power = mana_power_this_floor;

    /* return from event if custom music set */
    sr_ev_logic(c, cur, FLAG_CUSTOM_MUSIC, 0x1, 0xF, jump0);
    /* otherwise set music, then end */
    ev_range(c, cur, music_pattern, sizeof music_pattern);
    sr_ev_end(c, cur);

    walk_on[i] = event_num;
    key_int(c, "walkonEvent", i, event_num);
    event_num++;
  }

  /* -------------------------------- event 0x142: mana beast death, ending, statistics list */
  cur = sr_event_new(c, event_num);
  int mana_beast_death_event = event_num;
  sr_work_set_int(c, "ManaBeastDeathEvent", mana_beast_death_event);
  sr_work_set_int(c, "ManaBeastWalkonEvent", MANABEAST_WALKON_EVENT_NUM);

  sr_ev_incr_flag(c, cur, FLAG_MANAFORT_SWITCHES_1);
  sr_ev_incr_flag(c, cur, FLAG_FREEZE_BOSS_AI);
  /* set timer flag off */
  sr_ev_set_flag(c, cur, FLAG_TIMER_RUNNING, 0);
  /* modified for changes in DoorExpansion.cs */
  ev_ext_door(c, cur, 0x3FF);
  /* ending music */
  ev_play(c, cur, 0x01, 0x3a, 0x1b, 0x8f);

  ev_add(c, cur, EV_OPEN_DIALOGUE);
  ev_dialogue(c, cur, "Hooray! You did it.\nTime: ");
  ev_add(c, cur, EVC_CUSTOM_EVENT_COMMANDS);
  ev_add(c, cur, w8_command_index(c, "print timer"));
  ev_dialogue(c, cur, "\nOther statistics:");
  sr_ev_sleep(c, cur, 0);   /* DialogueWait */

  {
    static const char *const messages[12] = {"Total kills:", "Total deaths:", "", "Damage dealt:", "(Physical)", "(Magical)",
                                             "Damage taken:", "(Physical)", "(Magical)", "Distance walked:", "Chests opened:", ""};
    static const int offsets[10] = {STATISTICS_BASE_OFFSET + 0,  STATISTICS_BASE_OFFSET + 2,  STATISTICS_BASE_OFFSET + 10,
                                    STATISTICS_BASE_OFFSET + 14, STATISTICS_BASE_OFFSET + 18, STATISTICS_BASE_OFFSET + 22,
                                    STATISTICS_BASE_OFFSET + 26, STATISTICS_BASE_OFFSET + 30, STATISTICS_BASE_OFFSET + 6,
                                    STATISTICS_BASE_OFFSET + 4};
    static const int is32[10] = {0, 0, 1, 1, 1, 1, 1, 1, 1, 0};
    int actual = 0;
    for (int i = 0; i < 12; i++) {
      const char *msg = messages[i];
      if (msg[0]) {
        int bit32 = is32[actual];
        int total_len = bit32 ? 19 : 22;
        char str[64];
        int len = snprintf(str, sizeof str, "\n%s", msg);
        int padding = total_len - (len - 1);
        for (int j = 0; j < padding && len < (int)sizeof str - 1; j++) str[len++] = ' ';
        str[len] = 0;
        ev_dialogue(c, cur, str);
        int offset = offsets[actual];
        ev_add(c, cur, EVC_CUSTOM_EVENT_COMMANDS);
        ev_add(c, cur, w8_command_index(c, bit32 ? "print 32 bit value" : "print 16 bit value"));
        ev_add(c, cur, offset);
        ev_add(c, cur, offset >> 8);
        actual++;
      } else {
        ev_dialogue(c, cur, "\n");
      }
      /* wait for button press every 3 lines */
      if ((i % 3) == 2) sr_ev_sleep(c, cur, 0);
    }
  }
  ev_dialogue(c, cur, "\nThe end!");
  sr_ev_sleep(c, cur, 0);
  ev_add(c, cur, EV_CLOSE_DIALOGUE);
  ev_add(c, cur, 0x1f);   /* end game */
  ev_add(c, cur, 0x11);
  sr_ev_end(c, cur);

  /* -------------------------------- event 0x280+: NPC dialogue that is just comedian quotes */
  event_num = 0x280;
  {
    static const char *const fallback[1] = {"Quotes didn't load!"};
    const char *const *quotes = fallback;
    int num_quotes = 1;
    const char *src = sr_settings_get(c->settings, "acDialogue");
    if (!strcmp(src, "mitch")) {
      quotes = sr_quotes_mitch;
      num_quotes = sr_quotes_mitch_count;
    } else if (!strcmp(src, "demetri")) {
      quotes = sr_quotes_demetri;
      num_quotes = sr_quotes_demetri_count;
    }
    sr_work_set_int(c, "numDialogueEvents", num_quotes);
    for (int q = 0; q < num_quotes; q++) {
      cur = sr_event_new(c, event_num);
      size_t qlen = strlen(quotes[q]);
      char *quote = (char *)sr_alloc(c, qlen + 1);
      memcpy(quote, quotes[q], qlen + 1);
      if (SB("acFilter")) {
        replace_same_len(quote, "Fuck", "F---");
        replace_same_len(quote, "Shit", "S---");
        replace_same_len(quote, "fuck", "f---");
        replace_same_len(quote, "shit", "s---");
      }
      ev_dialogue_box(c, cur, quote);
      sr_ev_end(c, cur);
      event_num++;
    }
  }

  int use_bosses = 1, randomize_bosses = 1;
  {
    const char *freq = sr_settings_get(c->settings, "BossFrequency");
    if (!strcmp(freq, "every")) {
      use_bosses = 1;
      randomize_bosses = 0;
    } else if (!strcmp(freq, "everyfew")) {
      use_bosses = 1;
      randomize_bosses = 1;
    } else if (!strcmp(freq, "final")) {
      use_bosses = 0;
    }
  }

  int boss_granularity = num_maps / 8;
  if (use_bosses) {
    int boss_floor = randomize_bosses ? (AC_RND(c, 3) + boss_granularity) : 0;
    /* don't put a boss on the final floor */
    while (boss_floor < num_maps - 1) {
      if (n_boss >= AC_MAX_FLOORS) sr_fail(c, SOMRANDO_ERR_FAILED, "too many boss floors");
      boss_floors[n_boss++] = boss_floor;
      boss_floor += randomize_bosses ? (AC_RND(c, 3) + boss_granularity) : 1;
    }
  }

  /* -------------------------------- event 0x180 - 0x18f: element dialogue & already got it events */
  event_num = 0x180;
  int element_events[8];
  {
    static const uint8_t attribute_flags[8] = {0xC8, 0xC9, 0xCA, 0xCB, 0xCF, 0xCE, 0xCC, 0xCD};   /* sorted in order of npc graphics */
    for (int i = 0; i < 8; i++) {
      cur = sr_event_new(c, event_num);
      element_events[i] = event_num;
      uint8_t next_cmd[2];
      sr_ev_jump_cmd(event_num + 1, next_cmd);
      /* event flag check - skip this and tell me to go away (next event) */
      sr_ev_logic(c, cur, 0x10 + i, 0x1, 0xF, next_cmd);
      sr_ev_incr_flag(c, cur, 0x10 + i);
      if (i != 5) ev_set_attr(c, cur, 0x02, attribute_flags[i], i == 7 ? 0x18 : 0x38);   /* girl; not shade; dryad has no mana magic */
      if (i != 4) ev_set_attr(c, cur, 0x03, attribute_flags[i], i == 7 ? 0x03 : 0x07);   /* sprite; not lumina */
      ev_dialogue_box(c, cur, "Here, have some magic.");
      sr_ev_end(c, cur);
      event_num++;

      /* next event: already got it */
      cur = sr_event_new(c, event_num);
      ev_dialogue_box(c, cur, "Ok go away now.");
      sr_ev_end(c, cur);
      event_num++;
    }
  }

  /* -------------------------------- event 0x190: watts */
  int watts_event_num = event_num;
  cur = sr_event_new(c, event_num);
  ev_add(c, cur, EVC_OPEN_GP);
  ev_add(c, cur, 0x5F);
  ev_dialogue_box(c, cur, "WATTS:Okay!\n Which one's ready?");
  ev_add(c, cur, EVC_WEAPON_UPGRADE_MENU);   /* open the watts menu */
  ev_add(c, cur, EVC_CLOSE_GP);
  sr_ev_end(c, cur);
  event_num++;

  /* -------------------------------- event 0x191, 0x192, 0x193: phanna HP restore, confirm, deny */
  int restore_event_num = event_num;
  cur = sr_event_new(c, event_num);
  int restore_gold = diff_int(c, "restoreCost");
  if (restore_gold > 65535) restore_gold = 65535;
  ev_add(c, cur, EVC_OPEN_GP);
  ev_add(c, cur, 0x5F);
  {
    static const uint8_t xs[2] = {2, 8};
    static const char *const labels[2] = {"Sure", "Nah"};
    uint8_t cmds[2][2], cancel[2];
    char text[64];
    sr_ev_jump_cmd(event_num + 1, cmds[0]);
    sr_ev_jump_cmd(event_num + 2, cmds[1]);
    sr_ev_jump_cmd(event_num + 2, cancel);
    snprintf(text, sizeof text, "HP MP Restore,\n%d Gold.\n", restore_gold);
    ev_choices_box(c, cur, text, 2, xs, labels, cmds, cancel);
  }
  sr_ev_end(c, cur);

  /* event 0x192: confirm hp restore */
  event_num++;
  cur = sr_event_new(c, event_num);
  sr_ev_set_flag(c, cur, FLAG_NOT_ENOUGH_GOLD, 0);
  ev_add(c, cur, EVC_REMOVE_GOLD);
  ev_add(c, cur, restore_gold);
  ev_add(c, cur, restore_gold >> 8);
  ev_add(c, cur, 0x5F);   /* refresh GP display, i think? */
  {
    uint8_t not_enough[2];
    sr_ev_jump_cmd(0x337, not_enough);   /* 0x337 is the vanilla "not enough gold" event; the flag is set by REMOVE_GOLD */
    sr_ev_logic(c, cur, FLAG_NOT_ENOUGH_GOLD, 0x1, 0xF, not_enough);
  }
  ev_full_restore(c, cur);
  ev_play(c, cur, 0x02, 0x07, 0x00, 0x88);   /* recovery sound effect */
  ev_add(c, cur, 0x35);                      /* update display */
  ev_add(c, cur, EVC_CLOSE_GP);
  sr_ev_end(c, cur);

  /* event 0x193: decline hp restore */
  event_num++;
  cur = sr_event_new(c, event_num);
  int cancel_event_num = event_num;
  ev_add(c, cur, EVC_CLOSE_GP);
  sr_ev_end(c, cur);
  event_num++;

  /* -------------------------------- event 0x194-0x1c3: shop events. 16 of: base shop + sell, buy, sell all, sell all confirmation */
  int sell_event_num = event_num;
  cur = sr_event_new(c, event_num);
  ev_add(c, cur, EVC_GRAPHICAL_EFFECT);   /* center camera */
  ev_add(c, cur, 0x08);
  ev_add(c, cur, EVC_SELL_MENU);
  ev_add(c, cur, EVC_CLOSE_GP);
  sr_ev_end(c, cur);
  event_num++;

  int shop_event_ids[16];
  uint8_t cancel_cmd[2];
  sr_ev_jump_cmd(cancel_event_num, cancel_cmd);
  for (int i = 0; i < 16; i++) {
    /* base shop + sell */
    shop_event_ids[i] = event_num;
    cur = sr_event_new(c, event_num);
    ev_add(c, cur, EVC_OPEN_GP);
    ev_add(c, cur, 0x5F);
    {
      static const uint8_t xs[3] = {0x2, 0x8, 0x13};
      static const char *const labels[3] = {"Buy", "Sell all", "Sell"};
      uint8_t cmds[3][2];
      sr_ev_jump_cmd(event_num + 1, cmds[0]);
      sr_ev_jump_cmd(event_num + 2, cmds[1]);
      sr_ev_jump_cmd(sell_event_num, cmds[2]);
      ev_choices_box(c, cur, "Shop? Meow.\n", 3, xs, labels, cmds, cancel_cmd);
    }
    sr_ev_end(c, cur);
    event_num++;

    /* buy event */
    cur = sr_event_new(c, event_num);
    ev_add(c, cur, EVC_GRAPHICAL_EFFECT);
    ev_add(c, cur, 0x08);
    ev_add(c, cur, EVC_BUY_MENU);
    ev_add(c, cur, i);
    ev_add(c, cur, EVC_CLOSE_GP);
    sr_ev_end(c, cur);
    event_num++;

    /* sell all selection */
    cur = sr_event_new(c, event_num);
    {
      static const uint8_t xs[2] = {2, 8};
      static const char *const labels[2] = {"Yep", "Nope"};
      uint8_t cmds[2][2];
      sr_ev_jump_cmd(event_num + 1, cmds[0]);
      sr_ev_jump_cmd(cancel_event_num, cmds[1]);
      ev_choices_box(c, cur, "Sell all unequipped gear?\n", 2, xs, labels, cmds, cancel_cmd);
    }
    sr_ev_end(c, cur);
    event_num++;

    /* sell all confirmation */
    cur = sr_event_new(c, event_num);
    ev_add(c, cur, EVC_CUSTOM_EVENT_COMMANDS);
    ev_add(c, cur, w8_command_index(c, "sell all"));
    ev_add(c, cur, EV_CLOSE_DIALOGUE);
    ev_add(c, cur, EVC_CLOSE_GP);
    sr_ev_end(c, cur);
    event_num++;
  }

  /* -------------------------------- event 0x1c4+: boss events, 2 per boss: entry and death */
  sr_work_set_int(c, "bossEventNum", n_boss);
  for (int k = 0; k < n_boss; k++) {
    int floor_num = boss_floors[k];
    /* boss entry event */
    cur = sr_event_new(c, event_num);
    key_int(c, "bossEventFloor", k, floor_num);
    key_int(c, "bossEntryEvent", k, event_num);
    sr_ev_set_flag(c, cur, FLAG_CUSTOM_MUSIC, 0);   /* disable custom music */
    ev_play(c, cur, 0x01, 0x04, 0x04, 0xFF);        /* boss theme (copied from event 704) */
    sr_ev_set_flag(c, cur, FLAG_BOSS_VISIBILITY, 1);
    ev_range(c, cur, door_pattern, sizeof door_pattern);   /* doorway placeholder, filled in later */
    sr_ev_end(c, cur);
    event_num++;

    /* boss death event */
    cur = sr_event_new(c, event_num);
    key_int(c, "bossDeathEvent", k, event_num);
    /* inc event flag 26 (starts at 1 to make boss appear) */
    sr_ev_incr_flag(c, cur, FLAG_BOSS_VISIBILITY);
    sr_ev_logic(c, cur, FLAG_BOSS_VISIBILITY, 0x1, 0x1, jump0);   /* boss wasn't visible before? */
    /* random orb prize, subr 500 - 507 */
    sr_ev_jsr(c, cur, 0x500 + (uint8_t)AC_RND(c, 8));
    sr_ev_set_flag(c, cur, FLAG_BOSS_VISIBILITY, 0);
    sr_ev_set_flag(c, cur, FLAG_CUSTOM_MUSIC, 0);
    ev_range(c, cur, door_pattern, sizeof door_pattern);
    /* call walkon event for next map, since those seem to break when i do event doorways */
    if (floor_num + 1 >= 0 && floor_num + 1 <= 64) sr_ev_jump(c, cur, walk_on[floor_num + 1]);
    sr_ev_end(c, cur);
    event_num++;
  }

  int prev4[4] = {-1, -1, -1, -1};
  for (int i = 0; i < num_maps; i++) {
    int shop_id = (i * 16) / num_maps;
    struct ac_list *floor_events = &out->floor[i];
    out->has[i] = 1;
    int num_elements = diff_int(c, "numElements");
    if (num_elements > 8) num_elements = 8;

    /* always draw 8 so the rng stays the same for any number of elements */
    int element_ids[8];
    for (int j = 0; j < 8; j++) element_ids[j] = AC_RND(c, 8);

    for (int j = 0; j < num_elements; j++) {
      int element_id = element_ids[j];
      /* reroll once if recent element */
      for (int k = 0; k < 4; k++)
        if (prev4[k] == element_id) {
          element_id = element_ids[(j + 1) % 8];
          break;
        }
      prev4[0] = prev4[1];
      prev4[1] = prev4[2];
      prev4[2] = prev4[3];
      prev4[3] = element_id;

      struct ac_npc_event *ev = new_npc_event(c);
      ev->npc_id = (uint8_t)(0x90 + element_id);
      ev->flag_id = (uint8_t)(0x10 + element_id);
      ev->flag_min = 0x00;
      ev->flag_max = 0x00;
      ev->event_id = element_events[element_id];
      ev->mobile = 0;
      ac_list_add(c, floor_events, AC_VAL(ev));
    }

    int num_watts = diff_int(c, "numWatts");
    for (int j = 0; j < num_watts; j++) {
      struct ac_npc_event *ev = new_npc_event(c);
      ev->npc_id = 0x9C;
      ev->event_id = watts_event_num;
      ac_list_add(c, floor_events, AC_VAL(ev));
    }

    int num_restores = diff_int(c, "numPhannas");
    for (int j = 0; j < num_restores; j++) {
      struct ac_npc_event *ev = new_npc_event(c);
      ev->event_id = restore_event_num;
      ev->npc_id = 0xA6;   /* phanna as heal npc for now */
      ac_list_add(c, floor_events, AC_VAL(ev));
    }

    struct ac_npc_event *shop = new_npc_event(c);
    shop->event_id = shop_event_ids[shop_id];
    shop->npc_id = 0x99;   /* neko */
    ac_list_add(c, floor_events, AC_VAL(shop));

    int ids[128], npcs[128];
    int n = floor_events->n;
    if (n > 128) sr_fail(c, SOMRANDO_ERR_FAILED, "too many events on a floor");
    for (int k = 0; k < n; k++) {
      const struct ac_npc_event *ev = (const struct ac_npc_event *)AC_PTR(floor_events->p[k]);
      ids[k] = ev->event_id;
      npcs[k] = ev->npc_id;
    }
    {
      char key[64];
      snprintf(key, sizeof key, "usefulEvents%d", i);
      sr_work_set_int_array(c, key, ids, n);
      snprintf(key, sizeof key, "usefulEventsNpc%d", i);
      sr_work_set_int_array(c, key, npcs, n);
    }
  }

  /* -------------------------------- replacement death event -> allow continues */
  int new_death_event = event_num;
  cur = sr_event_new(c, event_num);
  /* if mana beast flag set, warp to a different map, then unset it */
  {
    uint8_t jsr_cmd[2];
    ev_jsr_cmd(event_num + 4, jsr_cmd);
    sr_ev_logic(c, cur, FLAG_MANAFORT_SWITCHES_1, 0x8, 0x8, jsr_cmd);
  }
  sr_ev_jsr(c, cur, 0x7C7);   /* music */
  /* fade out */
  ev_add(c, cur, EVC_GRAPHICAL_EFFECT);
  ev_add(c, cur, 0x06);
  ev_add(c, cur, 0xFF);
  ev_add(c, cur, 0xFF);
  /* gp view */
  ev_add(c, cur, EVC_OPEN_GP);
  ev_add(c, cur, 0x5F);
  int continue_cost = diff_int(c, "continueCost");
  {
    static const uint8_t xs[2] = {2, 8};
    static const char *const labels[2] = {"Yep", "Nope"};
    uint8_t cmds[2][2], cancel[2];
    char text[64];
    sr_ev_jump_cmd(event_num + 1, cmds[0]);
    sr_ev_jump_cmd(event_num + 2, cmds[1]);
    sr_ev_jump_cmd(event_num + 2, cancel);
    snprintf(text, sizeof text, "You died!\nContinue for %d GP?\n", continue_cost);
    ev_choices_box(c, cur, text, 2, xs, labels, cmds, cancel);
  }
  sr_ev_end(c, cur);
  event_num++;

  /* continue: yes */
  cur = sr_event_new(c, event_num);
  sr_ev_set_flag(c, cur, FLAG_NOT_ENOUGH_GOLD, 0);
  ev_add(c, cur, EVC_REMOVE_GOLD);
  ev_add(c, cur, continue_cost);
  ev_add(c, cur, continue_cost >> 8);
  ev_add(c, cur, 0x5F);   /* refresh gp display, i think */
  {
    uint8_t no_money[2];
    sr_ev_jump_cmd(event_num + 2, no_money);
    sr_ev_logic(c, cur, FLAG_NOT_ENOUGH_GOLD, 0x1, 0xF, no_money);
  }
  ev_add(c, cur, EVC_CLOSE_GP);
  ev_full_restore(c, cur);
  ev_play(c, cur, 0x02, 0x07, 0x00, 0x88);   /* recovery sound effect */
  ev_ext_door(c, cur, EXTENDED_DOOR_CONTINUE);
  sr_ev_end(c, cur);
  event_num++;

  /* continue: no/cancel */
  cur = sr_event_new(c, event_num);
  ev_dialogue_box(c, cur, "Ok game over then.");
  ev_add(c, cur, 0x1F);   /* reset */
  ev_add(c, cur, 0x07);
  sr_ev_end(c, cur);
  event_num++;

  /* continue: no because no money */
  cur = sr_event_new(c, event_num);
  ev_dialogue_box(c, cur, "Not enough money!\nGame over.");
  ev_add(c, cur, 0x1F);   /* reset */
  ev_add(c, cur, 0x07);
  sr_ev_end(c, cur);

  /* mana beast map escape so we can show dialog windows */
  event_num++;
  cur = sr_event_new(c, event_num);
  ev_ext_door(c, cur, EXTENDED_DOOR_CONTINUE);
  sr_ev_set_flag(c, cur, FLAG_MANAFORT_SWITCHES_1, 0);   /* mana beast existence flag */
  ev_add(c, cur, EV_RETURN);
  sr_ev_end(c, cur);

  /* the event that runs when you die */
  sr_poke(c, 0x19459, new_death_event);
  sr_poke(c, 0x1945A, new_death_event >> 8);
}
