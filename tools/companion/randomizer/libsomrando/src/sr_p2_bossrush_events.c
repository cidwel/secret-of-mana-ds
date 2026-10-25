/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Boss rush mode (group w8): boss order and the generated events.  Derived from processing/bossrush/BossOrderRandomizer.cs and
 * processing/bossrush/BossRushEventGenerator.cs (Author: Moppleton).  Translated to C 2026-10-18.
 */
#include <string.h>

#include "sr_core.h"
#include "sr_p2_bossrush.h"
#include "sr_p2_bossrush_int.h"
#include "sr_p2_bossrush_data.h"

#define SB(key) sr_settings_get_bool(c->settings, (key))

/* ------------------------------------------------------------------------------------------------ BossOrderRandomizer */
int sr_w8_hack_boss_order(struct sr_ctx *c) {
  int all_bosses[W8_NUM_BOSS_ENTRIES];
  int count = W8_NUM_BOSS_ENTRIES;
  char key[32];
  for (int i = 0; i < count; i++) all_bosses[i] = sr_w8_boss_maps[i].id;
  for (int i = 0; i < W8_NUM_BOSS_ENTRIES; i++) {
    int boss_index = sr_random_next(&c->rf) % count;
    int boss_num = all_bosses[boss_index];
    for (int k = boss_index; k < count - 1; k++) all_bosses[k] = all_bosses[k + 1];
    count--;
    snprintf(key, sizeof key, "bossNum%d", i);
    sr_work_set_int(c, key, boss_num);
  }
  /* end on mana beast */
  snprintf(key, sizeof key, "bossNum%d", W8_NUM_BOSS_ENTRIES);
  sr_work_set_int(c, key, 0x7F);
  return 1;
}

/* ------------------------------------------------------------------------------------------------- EventScript helpers */
#define EVC_PLAY_SOUND 0x40
#define EVC_ADD_INVENTORY_ITEM 0x1E
#define EVC_GRAPHICAL_EFFECT 0x2D
#define EVC_OPEN_GP 0x5D
#define EVC_SELL_MENU 0x0E
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
#define FLAG_BOSS_VISIBILITY 0x26
#define FLAG_TIMER_RUNNING 0x27
#define FLAG_CUSTOM_MUSIC 0x28
#define FLAG_PHANNA_HEALS 0x29
#define FLAG_MANAFORT_SWITCHES_1 0x4E
#define FLAG_TOTAL_MANA_POWER 0xFC
#define FLAG_FREEZE_BOSS_AI 0xFF

static void ev_add(struct sr_ctx *c, struct sr_bytes *e, int b) { sr_bytes_push(c, e, b); }

/* EventScript.ExtendedDoor */
static void ev_ext_door(struct sr_ctx *c, struct sr_bytes *e, int target_door) {
  int door_event_id = target_door + 0x800;
  ev_add(c, e, 0x18);
  ev_add(c, e, door_event_id);
  ev_add(c, e, door_event_id >> 8);
}

/* AddDialogue(text) = AddDialogue(text, 0): text bytes, then a sleep(0) that waits for a button */
static void ev_dialogue(struct sr_ctx *c, struct sr_bytes *e, const char *text) {
  sr_event_text_bytes(c, e, text);
  sr_ev_sleep(c, e, 0);
}

static void ev_dialogue_box(struct sr_ctx *c, struct sr_bytes *e, const char *text) {
  ev_add(c, e, 0x50);
  ev_dialogue(c, e, text);
  ev_add(c, e, 0x51);
}

/* AddDialogueBoxWithChoices with two choices "Yep"/"Nope" at x 2 and 8 */
static void ev_yep_nope_box(struct sr_ctx *c, struct sr_bytes *e, const char *text, int yes_jump, int no_jump, int cancel_jump) {
  uint8_t yes[2], no[2], cancel[2];
  sr_ev_jump_cmd(yes_jump, yes);
  sr_ev_jump_cmd(no_jump, no);
  sr_ev_jump_cmd(cancel_jump, cancel);
  ev_add(c, e, 0x50);
  sr_event_text_bytes(c, e, text);
  ev_add(c, e, 0x58);   /* option selection */
  ev_add(c, e, 0x5A);
  ev_add(c, e, 2);
  sr_event_text_bytes(c, e, "Yep");
  ev_add(c, e, 0x5A);
  ev_add(c, e, 8);
  sr_event_text_bytes(c, e, "Nope");
  ev_add(c, e, 0x5B);   /* option selection end */
  ev_add(c, e, cancel[0]);
  ev_add(c, e, cancel[1]);
  ev_add(c, e, yes[0]);
  ev_add(c, e, yes[1]);
  ev_add(c, e, no[0]);
  ev_add(c, e, no[1]);
}

static const char *boss_name(int id) {
  switch (id) {
    case 0x57: return "Mantis Ant";
    case 0x58: return "Wall Face";
    case 0x59: return "Tropicallo";
    case 0x5A: return "Minotaur";
    case 0x5B: return "Spikey Tiger";
    case 0x5C: return "Jabberwocky";
    case 0x5D: return "Spring Beak";
    case 0x5E: return "Frost Gigas";
    case 0x5F: return "Snap Dragon";
    case 0x60: return "Mech Rider I";
    case 0x61: return "Doom's Wall";
    case 0x62: return "Vampire";
    case 0x63: return "Metal Mantis";
    case 0x64: return "Mech Rider II";
    case 0x65: return "Kilroy";
    case 0x66: return "Gorgon Bull";
    case 0x68: return "Boreal Face";
    case 0x69: return "Great Viper";
    case 0x6A: return "Lime Slime";
    case 0x6B: return "Blue Spike";
    case 0x6D: return "Hydra";
    case 0x6E: return "Aegagropilon";
    case 0x6F: return "Hexas";
    case 0x70: return "Kettle Kin";
    case 0x71: return "Tonpole";
    case 0x80: return "Three Tonpoles";
    case 0x72: return "Mech Rider III";
    case 0x73: return "Snow Dragon";
    case 0x74: return "Fire Gigas";
    case 0x75: return "Red Dragon";
    case 0x76: return "Axe Beak";
    case 0x77: return "Blue Dragon";
    case 0x78: return "Buffy";
    case 0x79: return "Dark Lich";
    case 0x7B: return "Dragon Worm";
    case 0x7C: return "Dread Slime";
    case 0x7D: return "Thunder Gigas";
    case 0x7F: return "Mana Beast";
  }
  return NULL;
}

static const char *const gear_names[64] = {
  "", "Bandanna", "Hair Ribbon", "Rabite Cap", "Head Gear", "Quill Cap", "Steel Cap", "Golden Tiara", "Raccoon Cap", "Quilted Hood",
  "Tiger Cap", "Circlet", "Ruby Armet", "Unicorn Helm", "Dragon Helm", "Duck Helm", "Needle Helm", "Cockatrice Cap", "Amulet Helm",
  "Griffin Helm", "Faerie Crown", "", "Overalls", "Kung Fu Suit", "Midge Robe", "Chain Vest", "Spiky Suit", "Kung Fu Dress",
  "Fancy Overalls", "Chest Guard", "Golden Vest", "Ruby Vest", "Tiger Suit", "Tiger Bikini", "Magical Armor", "Tortoise Mail",
  "Flower Suit", "Battle Suit", "Vestguard", "Vampire Cape", "Power Suit", "Faerie Cloak", "", "Faerie's Ring", "Elbow Pad",
  "Power Wrist", "Cobra Bracelet", "Wolf's Band", "Silver Band", "Golem Ring", "Frosty Ring", "Ivy Amulet", "Gold Bracelet",
  "Shield Ring", "Lazuri Ring", "Guardian Ring", "Gauntlet", "Ninja Gloves", "Dragon Ring", "Watcher Ring", "Imp's Ring",
  "Amulet Ring", "Wristband"};

static const char *const orb_names[8] = {"Glove", "Sword", "Axe", "Spear", "Whip", "Bow", "Boomerang", "Javelin"};

/* consumables in the C# Dictionary insertion order */
static const uint8_t consumable_keys[7] = {0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x4A};
static const char *const consumable_names[7] = {"Candy", "Chocolate", "Royal Jam", "Faerie Walnut", "Medical Herb", "Cup of Wishes", "Barrel"};

static uint8_t hat_ids[64], armor_ids[64], accessory_ids[64];

static void init_gear_tables(void) {
  for (int j = 0; j < 64; j++) {
    hat_ids[j] = (uint8_t)(0x01 + ((0x14 - 0x01 + 1) * j / 64.0));
    armor_ids[j] = (uint8_t)(0x16 + ((0x29 - 0x16 + 1) * j / 64.0));
    accessory_ids[j] = (uint8_t)(0x2b + ((0x3e - 0x2b + 1) * j / 64.0));
    /* faerie ring and wrist band wtf */
    if (accessory_ids[j] == 0x3E) accessory_ids[j] = 0x2B;
    else if (accessory_ids[j] == 0x2B) accessory_ids[j] = 0x3E;
    /* overalls, kung fu suit, midge robe .. never drop since we start with them */
    if (armor_ids[j] == 0x16 || armor_ids[j] == 0x17 || armor_ids[j] == 0x18) armor_ids[j] = 0x19;
    if (hat_ids[j] == 0x01) hat_ids[j] = 0x04;
    if (hat_ids[j] == 0x02 || hat_ids[j] == 0x03) hat_ids[j] = 0x05;
    if (accessory_ids[j] == 0x3E) accessory_ids[j] = 0x2D;
  }
}

/* appends s to the buffer (a fixed 512 byte string builder for the treasure dialogue) */
static void sb_add(char *buf, size_t cap, const char *s) {
  size_t n = strlen(buf);
  size_t m = strlen(s);
  if (n + m + 1 > cap) m = cap - n - 1;
  memcpy(buf + n, s, m);
  buf[n + m] = 0;
}

static void make_next_floor_event(struct sr_ctx *c, const int *boss_ids, int current_floor, int boss_event, int *event_num) {
  int boss_num = boss_ids[current_floor];
  const char *pronoun = "it";
  if (boss_num == 0x80) pronoun = "them";   /* three! */
  struct sr_bytes *maybe = sr_event_new(c, (*event_num)++);
  char text[160];
  snprintf(text, sizeof text, "Up next:\n%s!\n\nFight %s now?\n", boss_name(boss_num), pronoun);
  ev_yep_nope_box(c, maybe, text, *event_num, 0, 0);
  sr_ev_end(c, maybe);

  struct sr_bytes *confirm = sr_event_new(c, (*event_num)++);
  if (boss_num == 0x7F) {
    /* mana beast start */
    sr_ev_set_flag(c, confirm, FLAG_MANAFORT_SWITCHES_1, 8);
    sr_ev_jsr(c, confirm, 0x739);   /* play the mana beast theme */
    ev_ext_door(c, confirm, 0xbc);
    sr_ev_end(c, confirm);
  } else {
    sr_ev_jump(c, confirm, boss_event);
    sr_ev_end(c, confirm);
  }
}

static void make_starting_event(struct sr_ctx *c, int *event_num) {
  struct sr_bytes *e = sr_event_new(c, (*event_num)++);
  uint8_t jump0[2];
  sr_ev_jump_cmd(0, jump0);
  /* skip out if 00 is 1+ */
  sr_ev_logic(c, e, FLAG_UI_DISPLAY, 0x1, 0xF, jump0);
  /* enable UI */
  sr_ev_incr_flag(c, e, FLAG_UI_DISPLAY);
  sr_ev_incr_flag(c, e, FLAG_UI_DISPLAY);
  /* flag 0x0B -> 0x05 for normal death */
  sr_ev_set_flag(c, e, FLAG_DEATH_TYPE, 5);
  /* starting weapons: glove, axe, spear, whip, bow, boomerang, javelin */
  static const uint8_t weapons[7] = {0x80, 0x92, 0x9B, 0xA4, 0xAD, 0xB6, 0xBF};
  for (int i = 0; i < 7; i++) {
    ev_add(c, e, EVC_ADD_INVENTORY_ITEM);
    ev_add(c, e, weapons[i]);
  }
  int starting_gold = w8_diff_int(c, "startingGold");
  if (starting_gold > 65535) starting_gold = 65535;
  if (starting_gold > 0) {
    ev_add(c, e, EVC_ADD_GOLD);
    ev_add(c, e, starting_gold);
    ev_add(c, e, starting_gold >> 8);
  }
  /* name girl/sprite */
  if (SB("brGirl")) {
    ev_add(c, e, EVC_NAME_CHARACTER);
    ev_add(c, e, 0x01);
  }
  if (SB("brSprite")) {
    ev_add(c, e, EVC_NAME_CHARACTER);
    ev_add(c, e, 0x02);
  }
  /* Add p2, p3 */
  if (SB("brGirl")) {
    ev_add(c, e, EVC_ADD_CHARACTER);
    ev_add(c, e, 0x01);
  }
  if (SB("brSprite")) {
    ev_add(c, e, EVC_ADD_CHARACTER);
    ev_add(c, e, 0x02);
  }
  int starting_consumables = w8_diff_int(c, "startingConsumables");
  /* candy, chocolate, jam, herb, walnut, cup */
  static const uint8_t starting_items[6] = {0x40, 0x41, 0x42, 0x43, 0x44, 0x45};
  for (int k = 0; k < 6; k++)
    for (int i = 0; i < starting_consumables; i++) {
      ev_add(c, e, EVC_ADD_INVENTORY_ITEM);
      ev_add(c, e, starting_items[k]);
    }
  /* flammie drum */
  ev_add(c, e, EVC_ADD_INVENTORY_ITEM);
  ev_add(c, e, 0x47);
  /* 4 is lumina 5 is shade */
  static const uint8_t element_attribute_flags[8] = {0xC8, 0xC9, 0xCA, 0xCB, 0xCF, 0xCE, 0xCC, 0xCD};
  for (int i = 0; i < 8; i++) {
    /* girl spells */
    if (i != 5) {
      ev_add(c, e, EVC_SET_CHARACTER_ATTRIBUTE);
      ev_add(c, e, 0x02);
      ev_add(c, e, element_attribute_flags[i]);
      ev_add(c, e, i == 7 ? 0x18 : 0x38);   /* cut mana magic / all three spells */
    }
    /* sprite spells */
    if (i != 4) {
      ev_add(c, e, EVC_SET_CHARACTER_ATTRIBUTE);
      ev_add(c, e, 0x03);
      ev_add(c, e, element_attribute_flags[i]);
      ev_add(c, e, i == 7 ? 0x03 : 0x07);
    }
  }
  /* and magic rope. */
  ev_add(c, e, EVC_ADD_INVENTORY_ITEM);
  ev_add(c, e, 0x46);
  /* restore everyone's hp */
  ev_add(c, e, EVC_HEAL);
  ev_add(c, e, 0x00);
  /* song */
  ev_add(c, e, EVC_PLAY_SOUND);
  ev_add(c, e, 0x01);
  ev_add(c, e, 0x02);
  ev_add(c, e, 0x0d);
  ev_add(c, e, 0xff);
  /* full mana power */
  const double mana_power_base = 8.0;
  sr_ev_set_flag(c, e, FLAG_TOTAL_MANA_POWER, (uint8_t)mana_power_base);
  for (int i = 0; i < (int)mana_power_base; i++) sr_ev_set_flag(c, e, 0x90 + i, 1);
  ev_dialogue_box(c, e,
                  "The stats in this mode\n"
                  "probably need some work\n"
                  "to make it appropriately\n"
                  "difficult. Feedback on\n"
                  "this is appreciated!\n");
  /* remove boy character if applicable */
  if (!SB("brBoy")) {
    ev_add(c, e, EVC_REMOVE_CHARACTER);
    ev_add(c, e, 0x00);
  }
  /* set timer flag on */
  sr_ev_set_flag(c, e, FLAG_TIMER_RUNNING, 1);
  sr_ev_end(c, e);
}

/* ------------------------------------------------------------------------------------------------ BossRushEventGenerator */
int sr_w8_hack_boss_rush_events(struct sr_ctx *c) {
  int event_num = 0x180;
  struct sr_random *random = &c->rf;
  const int num_maps = W8_NUM_BOSS_ENTRIES;
  int bosses_in_order[W8_NUM_BOSS_ENTRIES + 1];
  char key[48];
  init_gear_tables();
  for (int i = 0; i <= num_maps; i++) {
    snprintf(key, sizeof key, "bossNum%d", i);
    bosses_in_order[i] = w8_wget_int(c, key);
  }

  /* make intro to first floor event */
  sr_work_set_int(c, "restMapEntryEventId0", event_num);
  make_starting_event(c, &event_num);

  const int num_bosses = num_maps + 1;
  uint8_t num_orbs_given[8] = {0, 0, 0, 0, 0, 0, 0, 0};

  for (int floor_num = 0; floor_num < num_bosses; floor_num++) {
    int next_floor = floor_num + 1;
    int rest_map_entry_event = event_num;
    snprintf(key, sizeof key, "restMapEntryEventId%d", next_floor);
    sr_work_set_int(c, key, event_num);
    struct sr_bytes *floor_entry = sr_event_new(c, event_num++);
    /* music */
    ev_add(c, floor_entry, EVC_PLAY_SOUND);
    ev_add(c, floor_entry, 0x01);
    ev_add(c, floor_entry, 0x02);
    ev_add(c, floor_entry, 0x0d);
    ev_add(c, floor_entry, 0xff);

    const int num_orbs = 2, num_usables = 3, num_gear_pieces = 2;
    char treasure[512];
    snprintf(treasure, sizeof treasure, "Floor %d Treasures:\n", next_floor);

    for (int j = 0; j < num_orbs; j++) {
      int orb_id = sr_random_next(random) % 8;
      int num_currently = num_orbs_given[orb_id];
      int reroll_attempts = 0;
      while (num_currently >= 8 && reroll_attempts < 1000) {
        orb_id = sr_random_next(random) % 8;
        num_currently = num_orbs_given[orb_id];
        reroll_attempts++;
      }
      if (reroll_attempts < 1000) {
        num_orbs_given[orb_id]++;
        sr_ev_incr_flag(c, floor_entry, 0xB8 + orb_id);
        sb_add(treasure, sizeof treasure, orb_names[orb_id]);
        sb_add(treasure, sizeof treasure, "'s Orb\n");
      }
    }

    for (int j = 0; j < num_usables; j++) {
      int consumable_id = sr_random_next(random) % 7;
      ev_add(c, floor_entry, EVC_ADD_INVENTORY_ITEM);
      ev_add(c, floor_entry, consumable_keys[consumable_id]);
      sb_add(treasure, sizeof treasure, consumable_names[consumable_id]);
      sb_add(treasure, sizeof treasure, "\n");
    }

    int base_index = (int)(next_floor * 64 / 37.0);
    for (int j = 0; j < num_gear_pieces; j++) {
      /* range base-4 -> base+16 */
      int index = base_index - 4 + (sr_random_next(random) % 20);
      if (index < 0) index = 0;
      if (index > 63) index = 63;
      int gear_type = sr_random_next(random) % 3;
      if (gear_type == 0) {
        ev_add(c, floor_entry, EVC_ADD_INVENTORY_ITEM);
        ev_add(c, floor_entry, hat_ids[index]);
        sb_add(treasure, sizeof treasure, gear_names[hat_ids[index]]);
        sb_add(treasure, sizeof treasure, "\n");
      } else if (gear_type == 1) {
        ev_add(c, floor_entry, EVC_ADD_INVENTORY_ITEM);
        ev_add(c, floor_entry, armor_ids[index]);
        sb_add(treasure, sizeof treasure, gear_names[armor_ids[index]]);
        sb_add(treasure, sizeof treasure, "\n");
      } else {
        ev_add(c, floor_entry, EVC_ADD_INVENTORY_ITEM);
        ev_add(c, floor_entry, accessory_ids[index]);
        sb_add(treasure, sizeof treasure, gear_names[accessory_ids[index]]);
        sb_add(treasure, sizeof treasure, "\n");
      }
    }
    ev_dialogue_box(c, floor_entry, treasure);
    sr_ev_end(c, floor_entry);

    /* Boss room walkon event */
    int boss_entry_event_num = event_num;
    struct sr_bytes *boss_entry = sr_event_new(c, event_num++);
    sr_ev_set_flag(c, boss_entry, FLAG_CUSTOM_MUSIC, 0);   /* disable custom music */
    /* play boss theme (copied from event 704) */
    ev_add(c, boss_entry, EVC_PLAY_SOUND);
    ev_add(c, boss_entry, 0x01);
    ev_add(c, boss_entry, 0x04);
    ev_add(c, boss_entry, 0x04);
    ev_add(c, boss_entry, 0xFF);
    sr_ev_set_flag(c, boss_entry, FLAG_BOSS_VISIBILITY, 1);   /* set event flag for visibility */
    int entry_door = 10 + floor_num * 2 + 1;
    ev_ext_door(c, boss_entry, entry_door);
    sr_ev_end(c, boss_entry);

    /* Boss death event */
    snprintf(key, sizeof key, "bossDeathEventId%d", floor_num);
    sr_work_set_int(c, key, event_num);
    struct sr_bytes *boss_death = sr_event_new(c, event_num++);
    uint8_t jump0[2];
    sr_ev_jump_cmd(0, jump0);
    /* inc event flag 26 (starts at 1 to make boss appear); if it is 1, jump to event 00 (triple tonpole) */
    sr_ev_incr_flag(c, boss_death, FLAG_BOSS_VISIBILITY);
    sr_ev_logic(c, boss_death, FLAG_BOSS_VISIBILITY, 0x1, 0x1, jump0);
    sr_ev_set_flag(c, boss_death, FLAG_BOSS_VISIBILITY, 0);
    sr_ev_set_flag(c, boss_death, FLAG_CUSTOM_MUSIC, 0);
    int boss_death_door = 10 + floor_num * 2 + 2;
    ev_ext_door(c, boss_death, boss_death_door);
    /* call walkon event for next map, since those seem to break when i do event doorways */
    sr_ev_jump(c, boss_death, rest_map_entry_event);
    sr_ev_end(c, boss_death);
    snprintf(key, sizeof key, "nextFloorEventId%d", floor_num);
    sr_work_set_int(c, key, event_num);
    make_next_floor_event(c, bosses_in_order, floor_num, boss_entry_event_num, &event_num);
  }

  /* Neko event */
  struct sr_bytes *neko = NULL;
  sr_work_set_int(c, "nekoEventNum", event_num);
  neko = sr_event_new(c, event_num++);
  ev_dialogue_box(c, neko, "Sell some stuff?\n");
  ev_add(c, neko, EVC_GRAPHICAL_EFFECT);   /* center camera */
  ev_add(c, neko, 0x08);
  ev_add(c, neko, EVC_OPEN_GP);
  ev_add(c, neko, 0x5F);
  ev_add(c, neko, EVC_SELL_MENU);
  ev_add(c, neko, EVC_CLOSE_GP);
  sr_ev_end(c, neko);

  /* Watts event */
  sr_work_set_int(c, "wattsEventNum", event_num);
  struct sr_bytes *watts = sr_event_new(c, event_num++);
  ev_add(c, watts, EVC_OPEN_GP);
  ev_add(c, watts, 0x5F);
  ev_dialogue_box(c, watts, "WATTS:Okay!\n Which one's ready?");
  ev_add(c, watts, EVC_WEAPON_UPGRADE_MENU);
  ev_add(c, watts, EVC_CLOSE_GP);
  sr_ev_end(c, watts);

  /* Phanna event */
  sr_work_set_int(c, "phannaEventNum", event_num);
  struct sr_bytes *phanna = sr_event_new(c, event_num++);
  int num_restores = w8_diff_int(c, "restoresLimit");
  if (num_restores > 15) num_restores = 15;
  /* main event - jump if flag x29 is 1, 2, 3, etc .. continue if it's 0 (none used yet) */
  for (int i = 1; i <= num_restores; i++) {
    uint8_t cmd[2];
    sr_ev_jump_cmd(event_num + i, cmd);
    sr_ev_logic(c, phanna, FLAG_PHANNA_HEALS, i, i, cmd);
  }
  int yep_event = event_num;
  char text[96];
  snprintf(text, sizeof text, "Restore? You have %d\nremaining.\n", num_restores);
  ev_yep_nope_box(c, phanna, text, yep_event, 0, 0);
  sr_ev_end(c, phanna);

  struct sr_bytes *confirm = sr_event_new(c, event_num++);
  /* restore then increment x29 by one */
  ev_add(c, confirm, EVC_HEAL); ev_add(c, confirm, 0x04);   /* all hp */
  ev_add(c, confirm, EVC_HEAL); ev_add(c, confirm, 0x40);   /* all mp? */
  ev_add(c, confirm, EVC_HEAL); ev_add(c, confirm, 0x84);   /* ?? */
  for (int ch = 1; ch <= 3; ch++) {
    /* status? */
    ev_add(c, confirm, EVC_SET_CHARACTER_ATTRIBUTE); ev_add(c, confirm, ch); ev_add(c, confirm, 0x90); ev_add(c, confirm, 0x00);
    ev_add(c, confirm, EVC_SET_CHARACTER_ATTRIBUTE); ev_add(c, confirm, ch); ev_add(c, confirm, 0x91); ev_add(c, confirm, 0x00);
  }
  /* recovery sound effect */
  ev_add(c, confirm, EVC_PLAY_SOUND);
  ev_add(c, confirm, 0x02);
  ev_add(c, confirm, 0x07);
  ev_add(c, confirm, 0x00);
  ev_add(c, confirm, 0x88);
  ev_add(c, confirm, 0x35);   /* update display */
  sr_ev_incr_flag(c, confirm, FLAG_PHANNA_HEALS);
  sr_ev_end(c, confirm);

  /* now make all the sub-events */
  for (int i = 1; i <= num_restores; i++) {
    struct sr_bytes *sub = sr_event_new(c, event_num++);
    if (i == num_restores) {
      ev_dialogue_box(c, sub, "No restores left!");
      sr_ev_end(c, sub);
    } else {
      snprintf(text, sizeof text, "Restore? You have %d\nremaining.\n", num_restores - i);
      ev_yep_nope_box(c, sub, text, yep_event, 0, 0);
      sr_ev_end(c, sub);
    }
  }

  sr_work_set_int(c, "ManaBeastDeathEvent", event_num);
  sr_work_set_int(c, "ManaBeastWalkonEvent", -1);
  struct sr_bytes *mb = sr_event_new(c, event_num++);
  sr_ev_incr_flag(c, mb, FLAG_MANAFORT_SWITCHES_1);
  sr_ev_incr_flag(c, mb, FLAG_FREEZE_BOSS_AI);
  sr_ev_set_flag(c, mb, FLAG_TIMER_RUNNING, 0);   /* set timer flag off */
  ev_ext_door(c, mb, 0x3FF);                      /* modified for changes in DoorExpansion.cs */
  /* ending music */
  ev_add(c, mb, EVC_PLAY_SOUND);
  ev_add(c, mb, 0x01);
  ev_add(c, mb, 0x3a);
  ev_add(c, mb, 0x1b);
  ev_add(c, mb, 0x8f);
  ev_add(c, mb, 0x50);
  ev_dialogue(c, mb, "Hooray! You did it.\nTime: ");
  ev_add(c, mb, EVC_CUSTOM_EVENT_COMMANDS);
  ev_add(c, mb, w8_command_index(c, "print timer"));
  ev_dialogue(c, mb, "\nThe end. ");
  ev_add(c, mb, 0x51);
  ev_add(c, mb, 0x1f);   /* end game */
  ev_add(c, mb, 0x11);
  sr_ev_end(c, mb);

  /* A978A in event 7F8 - remove restore jump; replace with spaces.  An event that is not in replacementEvents is only modified as a
   * throw-away copy by the C#, so there is nothing to do for it. */
  for (int k = 0; k < c->n_events; k++) {
    if (c->events[k].id != 0x7F8) continue;
    struct sr_bytes *e7 = c->events[k].data;
    for (size_t i = 0; i + 1 < e7->len; i++) {
      /* JSR 0x41B */
      if (e7->p[i] == 0x20 + 0x04 && e7->p[i + 1] == 0x1B) {
        e7->p[i] = 0x80;
        e7->p[i + 1] = 0x80;
        break;
      }
    }
  }
  return 1;
}
