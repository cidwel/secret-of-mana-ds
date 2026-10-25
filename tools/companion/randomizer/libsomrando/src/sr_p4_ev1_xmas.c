/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Open World christmas intros.  Derived from processing/openworld/events/{GiftDeliveryIntroEvent,ReindeerIntroEvent}.cs and
 * processing/hacks/openworld/XmasRandoData.cs (Author: Moppleton).  Translated to C 2026-10-18.  The data tables and the long texts are
 * generated (sr_p4_ev1_xmas_gen.c).
 * The statics of the C# (NpcLocationData.newEventBeforeGift/newEventGiveGift, ReplacementObject.index) are per-run state here: they live
 * in workingData under the keys xmasNpcBefore<n>, xmasNpcGive<n> and reindeerObjIndex<n>, read back through the accessors of sr_p4_ev1.h. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sr_p4.h"
#include "sr_p4_ev1.h"

#define GIFT_DELIVERY_INDEX_PREFIX "giftDelivery"
#define SELECTED_LOCATION_PREFIX "reindeerLocation"

static void work_set_indexed(struct sr_ctx *c, const char *prefix, int n, int v) {
  char key[48];
  snprintf(key, sizeof key, "%s%d", prefix, n);
  sr_work_set_int(c, key, v);
}

static int work_get_indexed(struct sr_ctx *c, const char *prefix, int n, int dflt) {
  char key[48];
  snprintf(key, sizeof key, "%s%d", prefix, n);
  const char *v = sr_work_get(c, key);
  return v ? atoi(v) : dflt;
}

static void play_ice_country_music(struct sr_ctx *c, struct sr_bytes *e) {
  sr_p4_ev1_add(c, e, EVC_PLAY_SOUND);
  sr_p4_ev1_add(c, e, 0x01);
  sr_p4_ev1_add(c, e, 0x24);
  sr_p4_ev1_add(c, e, 0x1c);
  sr_p4_ev1_add(c, e, 0x8f);
}

void sr_p4_ev1_gift_location(struct sr_ctx *c, int index, struct sr_p4_ev1_npc *out, int *new_event_before_gift, int *new_event_give_gift) {
  if (index < 0 || index >= sr_p4_ev1_gift_location_count) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was outside the bounds of the array.");
  *out = sr_p4_ev1_gift_locations[index];
  *new_event_before_gift = work_get_indexed(c, "xmasNpcBefore", index, 0);
  *new_event_give_gift = work_get_indexed(c, "xmasNpcGive", index, 0);
}

void sr_p4_ev1_reindeer_location(struct sr_ctx *c, int global_id, int *map_num, int *object_index) {
  if (global_id < 0 || global_id >= sr_p4_ev1_reindeer_location_count) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range.");
  *map_num = sr_p4_ev1_reindeer_locations[global_id].map_num;
  *object_index = work_get_indexed(c, "reindeerObjIndex", global_id, sr_p4_ev1_reindeer_locations[global_id].index);
}

/* ---- GiftDeliveryIntroEvent */
int sr_p4_ev_intro_gift(struct sr_ctx *c) {
  if (strcmp(sr_p4_ev1_wstr(c, "openWorldGoalShortName"), "xmas2020")) return 0;
  struct sr_random *r = &c->rf;
  struct sr_bytes *eventData = sr_p4_ev1_event_get(c, 0x106);
  const char *startingChar = sr_p4_ev1_wstr(c, "startingCharacter");
  int restrictiveLogic = !strcmp(sr_settings_get(c->settings, "opLogic"), "restrictive");
  int numGifts = sr_settings_get_int(c->settings, "opXmasGifts");
  /* the C# throws on the first array access beyond eight gifts, and on santaDeliveryDialogue[0] without any */
  if (numGifts < 1 || numGifts > 8) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range.");

  int chosen[8];
  for (int i = 0; i < numGifts; i++) {
    int giftIndex = sr_random_next(r) % sr_p4_ev1_gift_location_count;
    for (;;) {
      int dup = 0, restricted = 0;
      for (int j = 0; j < i; j++) dup |= chosen[j] == giftIndex;
      for (int j = 0; j < sr_p4_ev1_gift_no_restrictive_count; j++)
        restricted |= !strcmp(sr_p4_ev1_gift_no_restrictive[j], sr_p4_ev1_gift_locations[giftIndex].key);
      if (!(dup || (restricted && restrictiveLogic))) break;
      giftIndex = sr_random_next(r) % sr_p4_ev1_gift_location_count;
    }
    work_set_indexed(c, GIFT_DELIVERY_INDEX_PREFIX, i, giftIndex);
    chosen[i] = giftIndex;
  }

  for (int id = 0; id < numGifts; id++) {
    work_set_indexed(c, "xmasNpcBefore", chosen[id], sr_p4_ev1_gift_npc_dialogue_events[id * 2 + 0]);
    work_set_indexed(c, "xmasNpcGive", chosen[id], sr_p4_ev1_gift_npc_dialogue_events[id * 2 + 1]);
  }

  play_ice_country_music(c, eventData);
  const char *characterName = sr_p4_ev1_cat(c, 3, "(", startingChar, ")");

  struct sr_bytes *e = sr_event_new(c, 0x2fa);
  sr_ev_jump(c, e, 0x711);
  sr_ev_end(c, e);
  e = sr_event_new(c, 0x2fb);
  sr_ev_door(c, e, 0x276);
  sr_ev_end(c, e);

  struct sr_bytes *santa_main = sr_event_new(c, 0x37e);
  const char *firstDelivery = NULL;
  for (int i = 0; i < numGifts; i++) {
    int a = i * 2, b = i * 2 + 1;
    sr_ev_logic(c, santa_main, EVF_OPEN_WORLD_CHRISTMAS_PROGRESS, a, a, EV1_JUMP_CMD(sr_p4_ev1_santa_dialogue_events[a]));
    sr_ev_logic(c, santa_main, EVF_OPEN_WORLD_CHRISTMAS_PROGRESS, b, b, EV1_JUMP_CMD(sr_p4_ev1_santa_dialogue_events[b]));

    const char *delivery, *prize;
    sr_p4_ev1_gift_strings(c, i, numGifts, &sr_p4_ev1_gift_locations[chosen[i]], characterName, &delivery, &prize);
    if (i == 0) firstDelivery = delivery;

    struct sr_bytes *delivery_info = sr_event_new(c, sr_p4_ev1_santa_dialogue_events[a]);
    sr_p4_ev1_dialogue_box(c, delivery_info, delivery);
    sr_ev_end(c, delivery_info);

    struct sr_bytes *reward = sr_event_new(c, sr_p4_ev1_santa_dialogue_events[b]);
    sr_p4_ev1_dialogue_box(c, delivery_info, prize);   /* upstream appends the prize text to the delivery event, after its End */
    sr_ev_incr_flag(c, reward, EVF_OPEN_WORLD_CHRISTMAS_PROGRESS);
    if (i < numGifts - 1) sr_p4_ev1_inject_pattern(c, reward, 0, 1);
    else sr_ev_jump(c, reward, 0x42f);   /* credits */
    sr_ev_end(c, reward);
  }
  sr_ev_end(c, santa_main);

  int numLines = 1;
  for (const char *p = firstDelivery; *p; p++) numLines += *p == '\n';
  char pad[4] = "";
  for (int i = 0; i < numLines % 3; i++) strcat(pad, "\n");
  const char *finalString = sr_p4_ev1_cat(c, 2, firstDelivery, pad);

  sr_p4_ev1_auto_dialogue_box(c, eventData, sr_p4_ev1_gift_intro_text(c, characterName, finalString), 0x0A);
  sr_ev_jsr(c, eventData, 0x107);
  sr_ev_jump(c, eventData, 0x103);
  sr_ev_end(c, eventData);
  return 1;
}

/* ---- ReindeerIntroEvent */
static int in_list(const int *v, int n, int x) {
  for (int i = 0; i < n; i++)
    if (v[i] == x) return 1;
  return 0;
}

int sr_p4_ev_intro_reindeer(struct sr_ctx *c) {
  if (strcmp(sr_p4_ev1_wstr(c, "openWorldGoalShortName"), "xmas2020b")) return 0;
  struct sr_random *r = &c->rf;
  struct sr_bytes *eventData = sr_p4_ev1_event_get(c, 0x106);
  play_ice_country_music(c, eventData);
  const char *characterName = sr_p4_ev1_cat(c, 3, "(", sr_p4_ev1_wstr(c, "startingCharacter"), ")");
  uint8_t *rom = c->out;

  struct sr_bytes *e = sr_event_new(c, 0x2fa);
  sr_ev_jump(c, e, 0x711);
  sr_ev_end(c, e);
  e = sr_event_new(c, 0x2fb);
  sr_ev_door(c, e, 0x276);
  sr_ev_end(c, e);

  int difficulty[8];
  for (int i = 0; i < 8; i++) {
    int id = sr_random_next(r) % 8;
    while (in_list(difficulty, i, id)) id = sr_random_next(r) % 8;
    difficulty[i] = id;
  }

  int n_loc = sr_p4_ev1_reindeer_location_count;
  int *cur_index = (int *)sr_alloc(c, (size_t)n_loc * sizeof(int));   /* ReplacementObject.index, mutated by the intro */
  for (int i = 0; i < n_loc; i++) cur_index[i] = sr_p4_ev1_reindeer_locations[i].index;
  int chosen[8];
  for (int i = 0; i < 8; i++) {
    int base, count;
    if (difficulty[i] < 2) {
      base = 0;
      count = sr_p4_ev1_reindeer_count_easy;
    } else if (difficulty[i] < 6) {
      base = sr_p4_ev1_reindeer_count_easy;
      count = sr_p4_ev1_reindeer_count_medium;
    } else {
      base = sr_p4_ev1_reindeer_count_easy + sr_p4_ev1_reindeer_count_medium;
      count = n_loc - base;
    }
    int id = sr_random_next(r) % count;
    int breakout = 0;
    while (in_list(chosen, i, base + id)) {
      id = sr_random_next(r) % count;
      if (++breakout > 1000) break;
    }
    int globalId = base + id;
    const struct sr_p4_ev1_reindeer_loc *loc = &sr_p4_ev1_reindeer_locations[globalId];
    work_set_indexed(c, SELECTED_LOCATION_PREFIX, i, globalId);
    chosen[i] = globalId;
    int mapObjOffset = sr_vanilla_object_offset(rom, loc->map_num) + 8;

    int objNum = cur_index[globalId];
    if (objNum == -1) {
      int nextMapObjOffset = sr_vanilla_object_offset(rom, loc->map_num + 1) + 8;
      int numObjects = (nextMapObjOffset - mapObjOffset) / 8 - 1;
      if (numObjects == 0) sr_fail(c, SOMRANDO_ERR_FAILED, "Attempted to divide by zero.");
      objNum = sr_random_next(r) % numObjects;
      int isEnemyId = sr_peek(c, mapObjOffset + 8 * objNum + 5) <= 83;
      long guard = 0;
      while (in_list(loc->index_exceptions, loc->n_index_exceptions, objNum) || !isEnemyId) {
        if (++guard > 1000000) sr_fail(c, SOMRANDO_ERR_FAILED, "no enemy object to replace on map %d", loc->map_num);
        objNum = sr_random_next(r) % numObjects;
        isEnemyId = sr_peek(c, mapObjOffset + 8 * objNum + 5) <= 83;
      }
      /* clear the nearby enemies (event flag) so the reindeer spawns */
      int xPos = sr_peek(c, mapObjOffset + 8 * objNum + 2) & 0x7F;
      int yPos = sr_peek(c, mapObjOffset + 8 * objNum + 3) & 0x7F;
      for (int k = 0; k < numObjects; k++) {
        int isObjEnemy = sr_peek(c, mapObjOffset + 8 * k + 5) <= 83;
        if (isObjEnemy && k != objNum) {
          int thisX = sr_peek(c, mapObjOffset + 8 * k + 2) & 0x7F;
          int thisY = sr_peek(c, mapObjOffset + 8 * k + 3) & 0x7F;
          int xDist = abs(thisX - xPos), yDist = abs(thisY - yPos);
          if (xDist < 25 && yDist < 25) {
            sr_poke(c, mapObjOffset + 8 * k + 0, sr_p4_ev1_xmas_extra_flags[i]);
            sr_poke(c, mapObjOffset + 8 * k + 1, 0x1F);
          }
        }
      }
      cur_index[globalId] = objNum;
      work_set_indexed(c, "reindeerObjIndex", globalId, objNum);
    }

    int findEventNum = sr_p4_ev1_santa_dialogue_events[i + 8];
    struct sr_bytes *findEvent = sr_event_new(c, findEventNum);
    sr_p4_ev1_dialogue_box(c, findEvent, sr_p4_ev1_cat(c, 3, "It's ", sr_p4_ev1_reindeer_names[i], "!"));
    sr_ev_set_flag(c, findEvent, sr_p4_ev1_xmas_extra_flags[i], 1);
    sr_p4_ev1_add(c, findEvent, EVC_REFRESH_MAP);
    sr_p4_ev1_add(c, findEvent, EVC_REFRESH_OBJECTS);
    sr_ev_end(c, findEvent);

    sr_poke(c, mapObjOffset + 8 * objNum + 0, sr_p4_ev1_xmas_extra_flags[i]);
    sr_poke(c, mapObjOffset + 8 * objNum + 1, 0x00);
    sr_poke(c, mapObjOffset + 8 * objNum + 5, 0xAD);   /* rudolph */
    sr_poke(c, mapObjOffset + 8 * objNum + 3, sr_peek(c, mapObjOffset + 8 * objNum + 3) & 0x7F);
    sr_poke(c, mapObjOffset + 8 * objNum + 6, findEventNum);
    sr_poke(c, mapObjOffset + 8 * objNum + 7, (findEventNum >> 8) | 0x40);
  }

  sr_p4_ev1_auto_dialogue_box(c, eventData, sr_p4_ev1_reindeer_intro_text(c, characterName), 0x0A);
  sr_ev_jsr(c, eventData, 0x107);
  sr_ev_jump(c, eventData, 0x103);
  sr_ev_end(c, eventData);

  struct sr_bytes *santa_main = sr_event_new(c, 0x37e);
  for (int i = 0; i < 8; i++) {
    sr_ev_logic(c, santa_main, sr_p4_ev1_xmas_extra_flags[i], 0, 0, EV1_JUMP_CMD(sr_p4_ev1_santa_dialogue_events[i]));
    struct sr_bytes *dlg = sr_event_new(c, sr_p4_ev1_santa_dialogue_events[i]);
    const char *hint = sr_p4_ev1_reindeer_locations[chosen[i]].hint_locations[0];
    sr_p4_ev1_dialogue_box(c, dlg, sr_p4_ev1_word_wrap(c, sr_p4_ev1_cat(c, 5, "Still haven't found ", sr_p4_ev1_reindeer_names[i], "? Tried looking ", hint, "?")));
    sr_ev_end(c, dlg);
  }
  sr_p4_ev1_dialogue_box(c, santa_main, "You did the thing!");
  sr_ev_jump(c, santa_main, 0x42f);   /* credits */
  sr_ev_end(c, santa_main);
  return 1;
}
