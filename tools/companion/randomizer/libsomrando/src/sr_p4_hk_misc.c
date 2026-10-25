/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Open World hacks (Author of every upstream file: Moppleton), translated to C 2026-10-18.  Derived from SoMRandomizer/processing/hacks/openworld/:
 * ShopRandomizer.cs, NoNekoOverflow.cs, WattsWeaponLevelFix.cs, DropOnlyConsumables.cs, MenuIconsIfArmorImprovement.cs, AISwap.cs and
 * BlackMapPalettes.cs.  The assembly stretches were converted with tools/gen_p4_hmisc_xlate.py and reviewed.
 */
#include <stdio.h>
#include <string.h>

#include "sr_core.h"
#include "sr_p4.h"
#include "sr_p4_hk_neso.h"
#include "sr_p2_visual_color.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))
#define ENEMY_LEVEL_RAM 0x7E0440   /* CustomRamOffsets.OPENWORLD_CURRENT_ENEMY_LEVEL = STATISTICS_BASE_OFFSET + 64 */

/* NoNekoOverflow */
static int hmisc_no_neko_overflow(struct sr_ctx *c) {
    /*
    *  replace:
      $C0/7CEC C2 20       REP #$20                A:0099 X:FBA0 Y:0064 P:envMxdIZC
      $C0/7CEE 0E 22 18    ASL $1822  [$00:1822]   A:0099 X:FBA0 Y:0064 P:envmxdIZC
    */
    P(0x7CEC, 0x22);
    P(0x7CED, (uint8_t)(c->working_offset));
    P(0x7CEE, (uint8_t)(c->working_offset >> 8));
    P(0x7CEF, (uint8_t)((c->working_offset >> 16) + 0xC0));
    P(0x7CF0, 0xEA);

    E(0xC2); // replaced code
    E(0x20); // replaced code
    E(0x0E); // replaced code
    E(0x22); // replaced code
    E(0x18); // replaced code
    // BCS over (01) - check overflow on the shift
    E(0xB0);
    E(0x01);
    // RTL
    E(0x6B);
    // over: (overflow here)
    // LDA #FFFF .. actually use 65000 since FFFF seems to fuck up and show no price at all
    E(0xA9);
    E(0xE8);
    E(0xFD);
    // STA $1822
    E(0x8D);
    E(0x22);
    E(0x18);
    // RTL
    E(0x6B);
  return 1;
}

/* ShopRandomizer */
static void hmisc_armor_choices(struct sr_bytes *out, struct sr_ctx *c) {
  for (int i = 0x7c; i <= 0x8f; i++) sr_bytes_push(c, out, i);   /* hats */
  for (int i = 0x94; i <= 0xa4; i++) sr_bytes_push(c, out, i);   /* armors */
  for (int i = 0xa6; i <= 0xb9; i++) sr_bytes_push(c, out, i);   /* rings */
}

int sr_p4_hack_shop_randomizer(struct sr_ctx *c) {
  if (!sr_settings_get_bool(c->settings, "opShop")) return 0;

  sr_p4_nested(c, "Neko price overflow fix", hmisc_no_neko_overflow);

  /* a shop that pointed at the same list twice contributes its offsets twice, like the C# List */
  int *offsets = (int *)sr_alloc(c, 16 * 12 * sizeof(int));
  int n_offsets = 0;
  for (int i = 0; i < 16; i++) {
    int offset = 0x180000 + sr_peek(c, 0x18fc32 + i * 2) + (sr_peek(c, 0x18fc32 + i * 2 + 1) << 8);
    uint8_t shop_val = sr_peek(c, offset);
    int item_index = 0;
    /* shop 10 (southtown) actually exceeds 12 items and doesn't show the last one */
    while (shop_val != 0xFF && item_index < 12) {
      if (shop_val < 0xBA) offsets[n_offsets++] = offset;
      offset++;
      shop_val = sr_peek(c, offset);
      item_index++;
    }
  }

  struct sr_bytes remaining = {0, 0, 0};
  for (int k = 0; k < n_offsets; k++) {
    if (remaining.len == 0) hmisc_armor_choices(&remaining, c);
    int index = sr_random_next(&c->rf) % (int)remaining.len;
    uint8_t gear = remaining.p[index];
    P(offsets[k], gear);
    memmove(remaining.p + index, remaining.p + index + 1, remaining.len - (size_t)index - 1);
    remaining.len--;
  }
  return 1;
}

/* WattsWeaponLevelFix */
int sr_p4_hack_watts_weapon_fix(struct sr_ctx *c) {
    // $C0/7C20 9F E8 E1 7E STA $7EE1E8,x[$7E:E1E8] A:001C X:0000 Y:0003 P:envMxdIzc
    // basically just need to update 1E3, too
    P(0x7C20, 0x22);
    P(0x7C21, (uint8_t)(c->working_offset));
    P(0x7C22, (uint8_t)(c->working_offset >> 8));
    P(0x7C23, (uint8_t)((c->working_offset >> 16) + 0xC0));
    // STA $7EE1E8
    E(0x9F); // replaced code
    E(0xE8); // replaced code
    E(0xE1); // replaced code
    E(0x7E); // replaced code
    // STA $7EE1E3
    E(0x9F); // new code
    E(0xE3); // new code
    E(0xE1); // new code
    E(0x7E); // new code
    // RTL
    E(0x6B);
  return 1;
}

/* MenuIconsIfArmorImprovement */
int sr_p4_hack_menu_icons_armor(struct sr_ctx *c) {
    // here: reading the character wearability value
    // $C0/7A1A BF D5 3E D0 LDA $D03ED5,x[$D0:3F93] A:0000 X:00BE Y:008E P:envMxdIZc - replace this
    // $C0/7A1E 29 E0       AND #$E0                A:0080 X:00BE Y:008E P:eNvMxdIzc
    P(0x7A1A, 0x22);
    P(0x7A1B, (uint8_t)(c->working_offset));
    P(0x7A1C, (uint8_t)(c->working_offset >> 8));
    P(0x7A1D, (uint8_t)((c->working_offset >> 16) + 0xC0));

    // 7ee[1,3,5]e[0,1,2] for current equips - hat, armor, accessory
    // < C9, it's a hat, otherwise < 19B, it's an armor, otherwise accessory

    // PHB [B]
    E(0x8B);
    // PHA [BA]
    E(0x48);
    // LDA #7E
    E(0xA9);
    E(0x7E);
    // PHA [BAA]
    E(0x48);
    // PLB [BA]
    E(0xAB);
    // PLA [B]
    E(0x68);

    // removed code
    E(0xBF);
    E(0xD5);
    E(0x3E);
    E(0xD0);

    // if 7E1847 is not 01 (buy menu), we quit
    // PHA [BA]
    E(0x48);
    // LDA $1847
    E(0xAD);
    E(0x47);
    E(0x18);
    // CMP #01
    E(0xC9);
    E(0x01);
    // BEQ dontReturn (3)
    E(0xF0);
    E(0x03);
    // PLA [B]
    E(0x68);
    // PLB []
    E(0xAB);
    // RTL
    E(0x6B);

    // dontReturn:
    // PLA [B]
    E(0x68);

    // PHY [BYy]
    E(0x5A);
    // PHX [BYyXx]
    E(0xDA);
    // LDY #0000
    E(0xA0);
    E(0x00);
    E(0x00);
    // CPX #C9
    E(0xE0);
    E(0xC9);
    E(0x00);
    // BLT over
    E(0x90);
    E(0x0B);
    // LDY #0001
    E(0xA0);
    E(0x01);
    E(0x00);
    // CPX #19B
    E(0xE0);
    E(0x9B);
    E(0x01);
    // BLT over
    E(0x90);
    E(0x03);
    // LDY #0002
    E(0xA0);
    E(0x02);
    E(0x00);

    // bits on item id indicating usable by character
    // 0x80 for boy
    // 0x40 for girl
    // 0x20 for sprite
    static const uint8_t masks1[] = { 0x80, 0x40, 0x20 };
    // inverse of above - &ed with the item to indicate unusable by character, because its defense is ass
    static const uint8_t masks2[] = { 0x7F, 0xBF, 0xDF };
    static const uint8_t playerDataOffset[] = { 0xE1, 0xE3, 0xE5 };

    for (int i = 0; i < 3; i++)
    {
      // over:
      // BIT #masks1
      E(0x89);
      E(masks1[i]);
      // BEQ doneWithChar - not usable to begin with, don't bother
      E(0xF0);
      E(0x34 + 0x0D);
      // PHA [BYyXxA]
      E(0x48);
      // -- multiply by 10
      // LDA ExE1,y
      E(0xB9);
      E(0xE1);
      E(playerDataOffset[i]);
      // PHA [BYyXxAA]
      E(0x48);
      // LDA #00
      E(0xA9);
      E(0x00);
      // STA ExE1,y
      E(0x99);
      E(0xE1);
      E(playerDataOffset[i]);
      // LDA ExE0,y
      E(0xB9);
      E(0xE0);
      E(playerDataOffset[i]);

      // REP 20
      E(0xC2);
      E(0x20);
      // AND #00FF
      E(0x29);
      E(0xFF);
      E(0x00);
      // ASL
      E(0x0A);
      // ASL
      E(0x0A);
      // ASL
      E(0x0A);
      // CLC
      E(0x18);
      // ADC ExE0,y
      E(0x79);
      E(0xE0);
      E(playerDataOffset[i]);
      // CLC
      E(0x18);
      // ADC ExE0,y
      E(0x79);
      E(0xE0);
      E(playerDataOffset[i]);
      // TAX
      E(0xAA);
      // SEP 20
      E(0xE2);
      E(0x20);
      // PLA [BYyXxA]
      E(0x68);
      // STA ExE1,y
      E(0x99);
      E(0xE1);
      E(playerDataOffset[i]);
      // LDA $D03ED1,x - def of shop item -> current item
      E(0xBF);
      E(0xD1);
      E(0x3E);
      E(0xD0);
      // PHA - for comparison [BYyXxAA]
      E(0x48);
      // REP 20
      E(0xC2);
      E(0x20);
      // LDA $03,s for original x value
      E(0xA3);
      E(0x03);
      // TAX
      E(0xAA);
      // SEP 20
      E(0xE2);
      E(0x20);
      // LDA $D03ED1,x - existing def -> shop def
      E(0xBF);
      E(0xD1);
      E(0x3E);
      E(0xD0);
      // DEC
      E(0x3A);
      // CMP $01,s
      E(0xC3);
      E(0x01);
      // BLT skip (5) -> BGE skip
      E(0xB0);
      E(0x06);
      // PLA - shop def comparison [BYyXxA]
      E(0x68);
      // PLA - original a [BYyXx]
      E(0x68);
      // AND #masks2 - mark this as unusable for the character
      E(0x29);
      E(masks2[i]);
      // BRA 02 - skip the PLAs
      E(0x80);
      E(0x02);

      // skip:
      // PLA - shop def comparison [BYyXxA]
      E(0x68);
      // PLA - original a [BYyXx]
      E(0x68);
      // doneWithChar:
    }

    // PLX [BYy]
    E(0xFA);
    // PLY [B]
    E(0x7A);
    // PLB []
    E(0xAB);
    // RTL
    E(0x6B);
  return 1;
}

/* DropOnlyConsumables */
int sr_p4_hack_drop_only_consumables(struct sr_ctx *c) {
  const int code_size = 200;
  const int drop_table_size = 0;
  sr_ensure_space_in_bank(&c->working_offset, code_size + drop_table_size);
  const int drop_table_offset = 0x103A50;
  const char *trap_mode = sr_settings_get(c->settings, "opTraps");
  int no_walking_chests = sr_settings_get_bool(c->settings, "opChestsDontRun");
  const char *chest_freq = sr_settings_get(c->settings, "opChestFreq");
  /* 50 * multiplier gold per level.  max of 255 since it's an 8 bit load. */
  int gold_multiplier = sr_settings_get_int(c->settings, "opGoldDropMul") * 50;
  gold_multiplier = sr_clamp_to_endpoints(gold_multiplier, 0, 255);
  P(0x428E, 0x0F);   /* more drops */

  /* drop items 40-45, 4A (consumables) */
  static const uint8_t items[] = {0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x4A};
  for (int i = 0; i <= 0x53; i++) {
    int item1_gold = (sr_random_next(&c->rf) % 2) == 0;
    int item2_gold = (sr_random_next(&c->rf) % 2) == 0;
    /* control byte: xx000000 item/gold, x0000000 gold/item, 0x000000 item/item, 00000000 gold/gold (common/rare) */
    uint8_t item_type_control = 0x00;
    if (item1_gold && item2_gold) item_type_control = 0x00;
    else if (item1_gold && !item2_gold) item_type_control = 0x80;
    else if (!item1_gold && !item2_gold) item_type_control = 0x40;
    else if (!item1_gold && item2_gold) item_type_control = 0xC0;

    const int e = drop_table_offset + i * 5;
    P(e + 2, 0x38);
    if (item1_gold) P(e + 3, 250);
    else P(e + 3, items[sr_random_next(&c->rf) % 7]);
    if (item2_gold) P(e + 4, 250);
    else P(e + 4, items[sr_random_next(&c->rf) % 7]);

    P(e + 2, sr_peek(c, e + 2) | item_type_control);

    /* turn off 0x80 bit so none of them get up and walk */
    if (no_walking_chests) P(e + 0, sr_peek(c, e + 0) & 0x7F);

    if (!strcmp(trap_mode, "none")) P(e + 1, 0x10);
    else if (!strcmp(trap_mode, "many")) P(e + 1, 0xF3);

    if (!strcmp(chest_freq, "none")) {
      P(e + 0, sr_peek(c, e + 0) & 0xF0);
    } else if (!strcmp(chest_freq, "low")) {
      P(e + 0, sr_peek(c, e + 0) & 0xF0);
      P(e + 0, sr_peek(c, e + 0) | 0x02);
    } else if (!strcmp(chest_freq, "high")) {
      P(e + 0, sr_peek(c, e + 0) & 0xF0);
      P(e + 0, sr_peek(c, e + 0) | 0x08);
    } else if (!strcmp(chest_freq, "highest")) {
      P(e + 0, sr_peek(c, e + 0) & 0xF0);
      P(e + 0, sr_peek(c, e + 0) | 0x0F);
    }
  }

    // gold drop amount dependent on level
    // $C8/E1E0 22 CF 39 C0 JSL $C039CF[$C0:39CF]   A:00FA X:0078 Y:0000 P:envmxdIzc - add gold
    P(0x8e1e0, 0x22);
    P(0x8e1e1, (uint8_t)(c->working_offset));
    P(0x8e1e2, (uint8_t)(c->working_offset >> 8));
    P(0x8e1e3, (uint8_t)((c->working_offset >> 16) + 0xC0));
    // LDA OPENWORLD_CURRENT_ENEMY_LEVEL
    E(0xAF);
    E((uint8_t)ENEMY_LEVEL_RAM);
    E((uint8_t)(ENEMY_LEVEL_RAM >> 8));
    E((uint8_t)(ENEMY_LEVEL_RAM >> 16));
    // AND #00FF
    E(0x29);
    E(0xFF);
    E(0x00);
    // SEP 20
    E(0xE2);
    E(0x20);
    // INC A
    E(0x1A);
    // STA 4202 (mul A)
    E(0x8F);
    E(0x02);
    E(0x42);
    E(0x00);
    // LDA #x32
    E(0xA9);
    E((uint8_t)gold_multiplier);
    // STA 4203 (mul B)
    E(0x8F);
    E(0x03);
    E(0x42);
    E(0x00);
    // 8 nops to let it multiply
    for (int i = 0; i < 8; i++)
    {
      E(0xEA);
    }
    // REP 20
    E(0xC2);
    E(0x20);
    // LDA 4216 (result)
    E(0xAF);
    E(0x16);
    E(0x42);
    E(0x00);
    // overwrite the stack value we stored off earlier
    E(0x83);
    E(0x04);
    // call the gold thing we replaced
    // 22 CF 39 C0
    E(0x22);
    E(0xCF);
    E(0x39);
    E(0xC0);
    // RTL
    E(0x6B);
  /* the next four stretches are copies of vanilla subroutines turned into long calls (RTS -> RTL) */
#define COPY_SUBR(var, first, last)                                \
  int var = c->working_offset;                                     \
  for (int i = (first); i <= (last); i++) {                        \
    uint8_t b = sr_peek(c, i);                                     \
    if (i == (last)) b = 0x6B;                                     \
    E(b);                                                          \
  }
  COPY_SUBR(subr5c2a_5c37_newLoc, 0x5c2a, 0x5c37)
  COPY_SUBR(subr5c6c_5cc4_newLoc, 0x5c6c, 0x5cc4)
  COPY_SUBR(subr5d33_5d45_newLoc, 0x5d33, 0x5d45)
#undef COPY_SUBR

    // 5d08 - rewrite manually to change the JSR $5D33 to a long subr call and return long; otherwise no changes from vanilla
    int subr5d08_newLoc = c->working_offset;
    // LDA $A2
    E(0xA5);
    E(0xA2);
    // DEC A
    E(0x3A);
    // BPL $02
    E(0x10);
    E(0x02);
    // LDA #0B
    E(0xA9);
    E(0x0B);
    // STA $A2
    E(0x85);
    E(0xA2);
    // STA $004202 - multiply
    E(0x8F);
    E(0x02);
    E(0x42);
    E(0x00);
    // LDA #1E
    E(0xA9);
    E(0x1E);
    // STA $004203 - multiply
    E(0x8F);
    E(0x03);
    E(0x42);
    E(0x00);
    // REP 20
    E(0xC2);
    E(0x20);
    // NOP to wait for multiply
    E(0xEA);
    E(0xEA);
    E(0xEA);
    // LDA $004216 - load result
    E(0xAF);
    E(0x16);
    E(0x42);
    E(0x00);
    // TAY
    E(0xA8);
    // JSR $5D33 -> JSL subr5d33_5d45_newLoc
    E(0x22);
    E((uint8_t)(subr5d33_5d45_newLoc));
    E((uint8_t)(subr5d33_5d45_newLoc >> 8));
    E((uint8_t)((subr5d33_5d45_newLoc >> 16) + 0xC0));
    // SEP 20
    E(0xE2);
    E(0x20);
    // LDA #60
    E(0xA9);
    E(0x60);
    // STA $FE00,y
    E(0x99);
    E(0x00);
    E(0xFE);
    // INY
    E(0xC8);
    // INC $9F
    E(0xE6);
    E(0x9F);
    // RTS -> RTL
    E(0x6B);


    // 58c8 - rewrite manually
    /*
    $C0/58C8 48          PHA                     A:041A X:00C8 Y:0000 P:envMxdIzC - 8 bit
    $C0/58C9 20 08 5D    JSR $5D08  [$C0:5D08]   A:041A X:00C8 Y:0000 P:envMxdIzC
    $C0/58CC 68          PLA                     A:0060 X:00C8 Y:003D P:envMxdIzC
    $C0/58CD 85 D0       STA $D0    [$00:03D0]   A:001A X:00C8 Y:003D P:envMxdIzC - value we're printing
    $C0/58CF 64 D1       STZ $D1    [$00:03D1]   A:001A X:00C8 Y:003D P:envMxdIzC - msb of value (zero)
    $C0/58D1 20 6C 5C    JSR $5C6C  [$C0:5C6C]   A:001A X:00C8 Y:003D P:envMxdIzC - print 16 bit value from $D0
    $C0/58D4 A2 94 62    LDX #$6294              A:00BB X:00C8 Y:003F P:envMxdIzC - " GP inside!" message
    $C0/58D7 20 2A 5C    JSR $5C2A  [$C0:5C2A]   A:00BB X:6294 Y:003F P:envMxdIzC
    $C0/58DA 6B          RTL                     A:0000 X:629F Y:004A P:envMxdIZC
    */
    int subr58c8_newLoc = c->working_offset;
    // PHA
    E(0x48);
    // add sep 20
    E(0xE2);
    E(0x20);
    // JSR $5D08 -> JSL subr5d08_newLoc
    E(0x22);
    E((uint8_t)(subr5d08_newLoc));
    E((uint8_t)(subr5d08_newLoc >> 8));
    E((uint8_t)((subr5d08_newLoc >> 16) + 0xC0));
    // add rep 20
    E(0xC2);
    E(0x20);
    // PLA
    E(0x68);
    // STA $D0
    E(0x85);
    E(0xD0);
    // add sep 20
    E(0xE2);
    E(0x20);
    // JSR $5C6C -> JSL subr5c6c_5cc4_newLoc - print 16-bit value from $D0
    E(0x22);
    E((uint8_t)(subr5c6c_5cc4_newLoc));
    E((uint8_t)(subr5c6c_5cc4_newLoc >> 8));
    E((uint8_t)((subr5c6c_5cc4_newLoc >> 16) + 0xC0));
    // LDX #6294 - " GP inside!" message
    E(0xA2);
    E(0x94);
    E(0x62);
    // JSR $5C2A -> JSL subr5c2a_5c37_newLoc
    E(0x22);
    E((uint8_t)(subr5c2a_5c37_newLoc));
    E((uint8_t)(subr5c2a_5c37_newLoc >> 8));
    E((uint8_t)((subr5c2a_5c37_newLoc >> 16) + 0xC0));
    // RTL
    E(0x6B);

    // 8e1e8 - jump to our new 58c8 instead
    /*
      $C8/E1E6 E2 20       SEP #$20                A:041A X:00C8 Y:0000 P:envmxdIzC
      $C8/E1E8 22 C8 58 C0 JSL $C058C8[$C0:58C8]   A:041A X:00C8 Y:0000 P:envMxdIzC
    */
    // the sep 20 gets removed here so that we save the whole 16-bit A inside.
    // the other subroutines are vanilla and need to be grabbed so they can be called
    // from this other bank (and returned to properly)
    P(0x8e1e6, 0xEA);
    P(0x8e1e7, 0xEA);
    P(0x8e1e8, 0x22);
    P(0x8e1e9, (uint8_t)(subr58c8_newLoc));
    P(0x8e1ea, (uint8_t)(subr58c8_newLoc >> 8));
    P(0x8e1eb, (uint8_t)((subr58c8_newLoc >> 16) + 0xC0));
  return 1;
}

/* AISwap */
int sr_p4_hack_ai_swap(struct sr_ctx *c) {
  if (strcmp(sr_settings_get(c->settings, "opAiRando"), "swap")) return 0;
  int ids[84], n = 0;
  for (int i = 0; i < 84; i++)
    if (i != 32 && i != 46 && i != 63) ids[n++] = i;   /* shadow xx */

  for (int i = 0; i < 84; i++) {
    if (i == 32 || i == 46 || i == 63) continue;
    int pick = sr_random_next(&c->rf) % n;
    int source = ids[pick];
    for (int k = pick; k < n - 1; k++) ids[k] = ids[k + 1];
    n--;
    P(0x100000 + i * 16 + 9, sr_peek_orig(c, 0x100000 + source * 16 + 9));
    P(0x100000 + i * 16 + 10, sr_peek_orig(c, 0x100000 + source * 16 + 10));
    if (source == 0x08) P(0x1055ce, i);   /* green drop */
    if (source == 0x26) P(0x10712e, i);   /* emberman */
    if (source == 0x27) P(0x1071df, i);   /* red drop */
    if (source == 0x38) P(0x108275, i);   /* blue drop */
    if (source == 0x44) P(0x108e94, i);   /* tsunami */
  }
  return 1;
}

/* BlackMapPalettes */
static void hmisc_black_color(struct sr_ctx *c, int off) {
  struct sr_w2_color k = {{sr_peek(c, off), sr_peek(c, off + 1)}};
  sr_w2_set_red(&k, 0);
  sr_w2_set_green(&k, 0);
  sr_w2_set_blue(&k, 0);
  P(off, k.b[0]);
  P(off + 1, k.b[1]);
}

int sr_p4_hack_black_map_palettes(struct sr_ctx *c) {
  if (!sr_settings_get_bool(c->settings, "opPlatinumMode")) return 0;
  const int pal_start = 0xC7FE0;
  /* for all palette sets that are used; i think the rest are not */
  for (int set = 24; set < 120; set++)
    for (int palnum = 1; palnum < 8; palnum++)
      for (int col = 1; col < 16; col++) hmisc_black_color(c, pal_start + set * 30 * 7 + palnum * 30 + col * 2);

  /* displayset bg colors = black */
  for (int d = 0; d < 64; d++) {
    P(0x80100 + d * 9 + 7, 0);
    P(0x80100 + d * 9 + 8, 0);
  }

  /* world map: 0 = clouds, 1-4 = sky, 5-12 = tiles */
  static const int world_map_pal_offsets[] = {0x67DFE, 0x6483E};
  for (int w = 0; w < 2; w++)
    for (int palnum = 1; palnum < 13; palnum++)
      for (int col = 1; col < 16; col++) hmisc_black_color(c, world_map_pal_offsets[w] + palnum * 30 + col * 2);
  return 1;
}

/* Every option value read by these hacks is reproduced; only the nesoberi images (generated at build time) can be missing. */
int sr_p4_hmisc_supports(const struct sr_settings *s, char *why, size_t why_len) {
  (void)s;
  if (!sr_p4_hmisc_neso_available()) {
    snprintf(why, why_len, "unsupported option: mode 'open' needs the nesoberi resources, which are not built in");
    return 0;
  }
  return 1;
}
