/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Open World gift mode events.  Derived from processing/openworld/randomization/GiftModeProcessing.cs and the NPC / Santa event tables of
 * processing/openworld/events/GiftDeliveryIntroEvent.cs (Author: Moppleton).  Translated to C 2026-10-18.
 *
 * GiftDeliveryIntroEvent.process stores, for the n-th chosen delivery NPC, newEventBeforeGift = 0x7DF + 2n and newEventGiveGift = 0x7DF + 2n + 1
 * in static fields; they are recomputed here from the same rule instead of being read from that hack.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sr_p4.h"
#include "sr_p4_ow_inject_int.h"
#include "sr_p4_ow_gift_data.h"

#define FIRST_NPC_DIALOGUE_EVENT 0x7DF
#define FLAG_CHRISTMAS_PROGRESS 0x6F
#define FLAG_NOT_ENOUGH_GOLD 0x0F
#define FLAG_MIDGE_MALLET 0x5C
#define FLAG_MOOGLE_BELT 0x74
#define CMD_ADD_INVENTORY_ITEM 0x1E
#define CMD_REMOVE_GOLD 0x37
#define CMD_SET_CHARACTER_ATTRIBUTE 0x39

#define NUM_DELIVERY ((int)(sizeof DELIVERY_EVENT_IDS / sizeof *DELIVERY_EVENT_IDS))

static void push(struct sr_ctx *c, struct sr_bytes *e, int v) { sr_bytes_push(c, e, v); }

/* EventScript.AddDialogueBox */
static void dialogue_box(struct sr_ctx *c, struct sr_bytes *e, const char *text) {
  push(c, e, EV_OPEN_DIALOGUE);
  sr_ev_add_dialogue(c, e, text);
  sr_ev_sleep(c, e, 0);
  push(c, e, EV_CLOSE_DIALOGUE);
}

static void logic_jump(struct sr_ctx *c, struct sr_bytes *e, int flag, int low, int high, int target) {
  uint8_t cmd[2];
  sr_ev_jump_cmd(target, cmd);
  sr_ev_logic(c, e, flag, low, high, cmd);
}

static int rnd_mod(struct sr_ctx *c, int n) {
  int v = sr_random_next(&c->rf);
  if (n == 0) sr_fail(c, SOMRANDO_ERR_FAILED, "Attempted to divide by zero.");
  return v % n;
}

static char *dup_str(struct sr_ctx *c, const char *s) {
  size_t n = strlen(s) + 1;
  char *p = (char *)sr_alloc(c, n);
  memcpy(p, s, n);
  return p;
}

/* SomVanillaValues.elementOrbByteToName(value, false) */
static const char *element_name(int e) {
  switch (e) {
    case 0x81: return "gnome";
    case 0x82: return "undine";
    case 0x83: return "salamando";
    case 0x85: return "sylphid";
    case 0x84: return "lumina";
    case 0x86: return "shade";
    case 0x87: return "luna";
    case 0x88: return "dryad";
  }
  return "";
}

/* GiftModeProcessing.setOrbsOpen: walks crystalOrbColorMap (workingData orbElement<N> keys in insertion order) */
static void set_orbs_open(struct sr_ctx *c, const char *element, struct sr_bytes *e) {
  static const struct { int map, flag, value; } open[] = {
    {420, 0xE8, 0x01}, {421, 0xE9, 0x01}, {422, 0xEA, 0x01}, {423, 0xEB, 0x01}, {424, 0xEC, 0x01}, {425, 0xED, 0x01}, {426, 0xEE, 0x01},
    {307, 0x24, 0x04}, {291, 0x18, 0x02}, {240, 0x2B, 0x04}, {345, 0x2B, 0x04}, {348, 0x2C, 0x01}, {35, 0x3B, 0x02}, {41, 0x20, 0x04},
  };
  for (int k = 0; k < c->n_work; k++) {
    if (strncmp(c->work[k].key, "orbElement", 10)) continue;
    char *end;
    const char *suffix = c->work[k].key + 10;
    long map = strtol(suffix, &end, 10);
    if (!*suffix || *end) sr_fail(c, SOMRANDO_ERR_FAILED, "The input string '%s' was not in a correct format.", suffix);
    int value = (uint8_t)sr_p4_inject_work_int(c, c->work[k].key);
    if (strcmp(element_name(value), element)) continue;
    for (size_t j = 0; j < sizeof open / sizeof *open; j++)
      if (open[j].map == map) sr_ev_set_flag(c, e, open[j].flag, open[j].value);
  }
}

static void remove_item(struct sr_ctx *c, struct sr_bytes *e, int item, int before) {
  push(c, e, CMD_ADD_INVENTORY_ITEM);
  push(c, e, 0xFF);
  push(c, e, item);
  logic_jump(c, e, FLAG_NOT_ENOUGH_GOLD, 0x1, 0x1, before);   /* 0F set if removal failed */
}

void sr_p4_inject_gift_events(struct sr_ctx *c) {
  static const struct { const char *name; int flag, attr; } spells[] = {
    {"undine", 0xA9, 0xC9}, {"gnome", 0xA8, 0xC8}, {"sylphid", 0xAB, 0xCB}, {"salamando", 0xAA, 0xCA},
    {"lumina", 0xAC, 0xCF}, {"shade", 0xAD, 0xCE}, {"luna", 0xAE, 0xCC}, {"dryad", 0xAF, 0xCD},
  };
  int flag_value = 0;
  int num_gifts = sr_settings_get_int(c->settings, "opXmasGifts");
  const char **processed = (const char **)sr_alloc(c, sizeof POSSIBLE_XMAS_GIFTS / sizeof *POSSIBLE_XMAS_GIFTS * sizeof *processed);
  int n_gifts = (int)(sizeof POSSIBLE_XMAS_GIFTS / sizeof *POSSIBLE_XMAS_GIFTS);
  for (int i = 0; i < n_gifts; i++) processed[i] = POSSIBLE_XMAS_GIFTS[i].text;

  struct sr_bytes pattern = {0};
  sr_event_text_bytes(c, &pattern, "%GIFT");

  /* the dictionary lookup throws for a key that is not a possible gift */
#define LOOKUP(var, name_)                                                                              \
  do {                                                                                                \
    var = -1;                                                                                         \
    for (int q = 0; q < n_gifts; q++)                                                                 \
      if (!strcmp(POSSIBLE_XMAS_GIFTS[q].key, (name_))) var = q;                                        \
    if (var < 0) sr_fail(c, SOMRANDO_ERR_FAILED, "The given key '%s' was not present in the dictionary.", (name_)); \
  } while (0)

  {
    int gi;
    const char *g0 = sr_p4_inject_work_str(c, "giftSelection0");
    LOOKUP(gi, g0);
    struct sr_bytes desc = {0};
    sr_event_text_bytes(c, &desc, processed[gi]);
    sr_p4_inject_replace_event_data(c, sr_p4_inject_event_get(c, 0x106), pattern.p, (int)pattern.len, desc.p, desc.len);
  }

  for (int i = 0; i < num_gifts; i++) {
    char key[32];
    snprintf(key, sizeof key, "giftDelivery%d", i);
    int npc_id = sr_p4_inject_work_int(c, key);
    if (npc_id < 0 || npc_id >= NUM_DELIVERY) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was outside the bounds of the array.");
    int event_id = DELIVERY_EVENT_IDS[npc_id];
    int before_id = FIRST_NPC_DIALOGUE_EVENT + i * 2, gift_id = FIRST_NPC_DIALOGUE_EVENT + i * 2 + 1;

    struct sr_bytes *main_ev = sr_event_new(c, event_id);
    if (flag_value > 0) logic_jump(c, main_ev, FLAG_CHRISTMAS_PROGRESS, 0x0, (uint8_t)(flag_value - 1), before_id);
    logic_jump(c, main_ev, FLAG_CHRISTMAS_PROGRESS, (uint8_t)flag_value, (uint8_t)flag_value, gift_id);
    dialogue_box(c, main_ev, "Thanks for the thing");
    sr_ev_end(c, main_ev);

    struct sr_bytes *gift_ev = sr_event_new(c, gift_id);
    snprintf(key, sizeof key, "giftSelection%d", i);
    const char *this_gift = sr_p4_inject_work_str(c, key);
    int gi;
    LOOKUP(gi, this_gift);
    const char *gift_description = processed[gi];

    if (strstr(this_gift, "spells")) {
      char *ele = dup_str(c, this_gift);
      char *sp = strchr(ele, ' ');
      if (sp) *sp = 0;
      for (char *q = ele; *q; q++)
        if (*q >= 'A' && *q <= 'Z') *q = (char)(*q + 32);
      set_orbs_open(c, ele, gift_ev);
      for (size_t s = 0; s < sizeof spells / sizeof *spells; s++) {
        if (strcmp(ele, spells[s].name)) continue;
        logic_jump(c, gift_ev, spells[s].flag, 0x0, 0x0, before_id);
        for (int ch = 1; ch <= 3; ch++) {
          push(c, gift_ev, CMD_SET_CHARACTER_ATTRIBUTE);
          push(c, gift_ev, ch);
          push(c, gift_ev, spells[s].attr);
          push(c, gift_ev, 0x00);   /* no spells */
        }
        break;
      }
    } else if (strstr(this_gift, "gold")) {
      int amount = atoi(this_gift);
      sr_ev_set_flag(c, gift_ev, FLAG_NOT_ENOUGH_GOLD, 0);
      push(c, gift_ev, CMD_REMOVE_GOLD);
      push(c, gift_ev, amount);
      push(c, gift_ev, amount >> 8);
      logic_jump(c, gift_ev, FLAG_NOT_ENOUGH_GOLD, 0x1, 0x1, before_id);
    } else if (!strcmp(this_gift, "midge mallet") || !strcmp(this_gift, "moogle belt")) {
      int is_mallet = !strcmp(this_gift, "midge mallet");
      logic_jump(c, gift_ev, is_mallet ? FLAG_MIDGE_MALLET : FLAG_MOOGLE_BELT, 0x0, 0x0, before_id);
      sr_ev_set_flag(c, gift_ev, FLAG_NOT_ENOUGH_GOLD, 0);
      remove_item(c, gift_ev, is_mallet ? 0x49 : 0x48, before_id);
    } else if (!strcmp(this_gift, "hat#") || !strcmp(this_gift, "armor#") || !strcmp(this_gift, "accessory#")) {
      const char *const *names;
      int count, first;
      if (this_gift[0] == 'h') { names = HATNAMES; count = (int)(sizeof HATNAMES / sizeof *HATNAMES); first = 1; }
      else if (this_gift[0] == 'a' && this_gift[1] == 'r') { names = ARMORNAMES; count = (int)(sizeof ARMORNAMES / sizeof *ARMORNAMES); first = 22; }
      else { names = ACCESSORYNAMES; count = (int)(sizeof ACCESSORYNAMES / sizeof *ACCESSORYNAMES); first = 43; }
      int id = first + rnd_mod(c, count);
      processed[gi] = names[id - first];
      gift_description = names[id - first];
      sr_ev_set_flag(c, gift_ev, FLAG_NOT_ENOUGH_GOLD, 0);
      remove_item(c, gift_ev, id, before_id);
    } else {
      /* "hat" / "armor" / "accessory" remove any piece of that kind (FF, FE, FD); the foods and the barrel remove one specific item */
      static const struct { const char *name; int item; } plain[] = {
        {"hat", 0xFF}, {"armor", 0xFE}, {"accessory", 0xFD}, {"candy", 0x40}, {"chocolate", 0x41}, {"royaljam", 0x42}, {"walnut", 0x43}, {"barrel", 0x4A},
      };
      for (size_t s = 0; s < sizeof plain / sizeof *plain; s++) {
        if (strcmp(this_gift, plain[s].name)) continue;
        sr_ev_set_flag(c, gift_ev, FLAG_NOT_ENOUGH_GOLD, 0);
        remove_item(c, gift_ev, plain[s].item, before_id);
        break;
      }
    }

    {
      struct sr_bytes desc = {0};
      sr_event_text_bytes(c, &desc, gift_description);
      sr_p4_inject_replace_event_data(c, sr_p4_inject_event_get(c, SANTA_DIALOGUE_EVENTS[i * 2]), pattern.p, (int)pattern.len, desc.p, desc.len);
    }

    {
      const char *fmt[3] = {"Oh sweet, is that %s? Thanks!", "Oh, this is just what I wanted! %s!", "You brought me %s? I've been wanting this!"};
      char text[256];
      snprintf(text, sizeof text, fmt[rnd_mod(c, 3)], gift_description);
      dialogue_box(c, gift_ev, sr_p4_inject_word_wrap(c, text));
      sr_ev_incr_flag(c, gift_ev, FLAG_CHRISTMAS_PROGRESS);
      sr_ev_end(c, gift_ev);
    }

    struct sr_bytes *before_ev = sr_event_new(c, before_id);
    {
      static const char *const before[4] = {"Have you got a gift for me?", "I didn't get a gift this year!", "I think Santa forgot about me.", "Hmm, I didn't want anything for Christmas anyway."};
      dialogue_box(c, before_ev, sr_p4_inject_word_wrap(c, before[rnd_mod(c, 4)]));
      sr_ev_end(c, before_ev);
    }
    flag_value += 2;
  }
#undef LOOKUP
}
