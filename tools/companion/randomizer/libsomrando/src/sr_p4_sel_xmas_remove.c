/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Derived from processing/hacks/openworld/RemoveItemEventCommand.cs (Author: Moppleton), the comments are those of the C#.  Translated to C
 * 2026-10-18.  Event command 1E extension used by the Christmas goals so that the player can give items away.
 */
#include "sr_p4_sel_int.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))

void sr_p4_sel_remove_item_command(struct sr_ctx *c) {

  // this is mainly for the christmas mode so you can give shit away.

  // subroutine to erase an item, at current "x" index in 7ECCxx, and shift all the other items back
  int subEraseItemOffset = c->working_offset;
  // 16 bit xy, 8 bit A here
  // back:
  // LDA 7ECC01,x
  E(0xBF);
  E(0x01);
  E(0xCC);
  E(0x7E);
  // STA 7ECC00,x
  E(0x9F);
  E(0x00);
  E(0xCC);
  E(0x7E);
  // INX
  E(0xE8);
  // INY
  E(0xC8);
  // CPY #000A - last item; B is the trashcan
  E(0xC0);
  E(0x0A);
  E(0x00);
  // BCS/BGE out
  E(0xB0);
  E(0x02);
  // BRA back
  E(0x80);
  E(0xEF);
  // out:
  // LDA #FF
  E(0xA9);
  E(0xFF);
  // STA 7ECC00,x
  E(0x9F);
  E(0x00);
  E(0xCC);
  E(0x7E);
  // RTL
  E(0x6B);


  // subroutine to remove a specific item by ID in our inventory. ID can be found in A.
  // set 7ECF0F based on whether we were able to remove it.  0=success, 1=fail.
  int subDirectMatchOffset = c->working_offset;
  // loop:
  // CMP 7ECC00,x
  E(0xDF);
  E(0x00);
  E(0xCC);
  E(0x7E);
  // BEQ match
  E(0xF0);
  E(0x09);
  // INX
  E(0xE8);
  // INY
  E(0xC8);
  // CPY #000C
  E(0xC0);
  E(0x0C);
  E(0x00);
  // BEQ nomatch
  E(0xF0);
  E(0x0D);
  // BRA loop
  E(0x80);
  E(0xF1);

  // match:
  // JSL sub_eraseItem
  E(0x22);
  E((uint8_t)subEraseItemOffset);
  E((uint8_t)(subEraseItemOffset >> 8));
  E((uint8_t)((subEraseItemOffset >> 16) + 0xC0));
  // LDA #00
  E(0xA9);
  E(0x00);
  // STA $7ECF0F
  E(0x8F);
  E(0x0F);
  E(0xCF);
  E(0x7E);
  // RTL
  E(0x6B);

  // nomatch:
  // LDA #01
  E(0xA9);
  E(0x01);
  // STA $7ECF0F
  E(0x8F);
  E(0x0F);
  E(0xCF);
  E(0x7E);
  // RTL
  E(0x6B);


  int subRemoveConsumableOffset = c->working_offset;
  // PHA
  E(0x48);
  // LDX #48
  E(0xA2);
  E(0x48);
  E(0x00);
  // loop:
  // LDA 7ECC00,x
  E(0xBF);
  E(0x00);
  E(0xCC);
  E(0x7E);
  // AND #1F
  E(0x29);
  E(0x1F);
  // CMP $01,s
  E(0xC3);
  E(0x01);
  // BEQ match
  E(0xF0);
  E(0x09);

  // INX
  E(0xE8);
  // INY
  E(0xC8);
  // CPY #000C
  E(0xC0);
  E(0x0C);
  E(0x00);
  // BEQ nomatch
  E(0xF0);
  E(0x1F);
  // BRA loop
  E(0x80);
  E(0xED);

  // match:
  // PLA  - even out the stack
  E(0x68);
  // LDA 7ECC00,x
  E(0xBF);
  E(0x00);
  E(0xCC);
  E(0x7E);
  // SEC
  E(0x38);
  // SBC #20
  E(0xE9);
  E(0x20);
  // CMP #20
  E(0xC9);
  E(0x20);
  // BGE/BCS hadMoreThanOne
  E(0xB0);
  E(0x06);

  // JSL sub_eraseItem
  E(0x22);
  E((uint8_t)subEraseItemOffset);
  E((uint8_t)(subEraseItemOffset >> 8));
  E((uint8_t)((subEraseItemOffset >> 16) + 0xC0));
  // BRA end (04)
  E(0x80);
  E(0x04);
  // hadMoreThanOne:
  // STA 7ECC00,x
  E(0x9F);
  E(0x00);
  E(0xCC);
  E(0x7E);
  // end:
  // LDA #00
  E(0xA9);
  E(0x00);
  // STA 7ECF0F
  E(0x8F);
  E(0x0F);
  E(0xCF);
  E(0x7E);
  // RTL
  E(0x6B);

  // nomatch:
  // PLA  - even out the stack
  E(0x68);
  // LDA #01
  E(0xA9);
  E(0x01);
  // STA 7ECF0F
  E(0x8F);
  E(0x0F);
  E(0xCF);
  E(0x7E);
  // RTL
  E(0x6B);


  // remove first hat in inventory
  int subRemoveHatOffset = c->working_offset;
  // LDX #0024
  E(0xA2);
  E(0x24);
  E(0x00);
  // loop:
  // LDA 7ECC00,x
  E(0xBF);
  E(0x00);
  E(0xCC);
  E(0x7E);
  // CMP #15
  E(0xC9);
  E(0x15);
  // BCC/BLT found
  E(0x90);
  E(0x0E);

  // INX
  E(0xE8);
  // INY
  E(0xC8);
  // CPY #000C
  E(0xC0);
  E(0x0C);
  E(0x00);
  // BNE loop
  E(0xD0);
  E(0xF1);

  // LDA #01
  E(0xA9);
  E(0x01);
  // STA 7ECF0F
  E(0x8F);
  E(0x0F);
  E(0xCF);
  E(0x7E);
  // RTL
  E(0x6B);

  // found:
  // LDA #00
  E(0xA9);
  E(0x00);
  // STA 7ECF0F
  E(0x8F);
  E(0x0F);
  E(0xCF);
  E(0x7E);
  // JSL sub_eraseItem
  E(0x22);
  E((uint8_t)subEraseItemOffset);
  E((uint8_t)(subEraseItemOffset >> 8));
  E((uint8_t)((subEraseItemOffset >> 16) + 0xC0));
  // RTL
  E(0x6B);


  // remove first armor in inventory
  int subRemoveArmorOffset = c->working_offset;
  // LDX #0030
  E(0xA2);
  E(0x30);
  E(0x00);
  // loop:
  // LDA 7ECC00,x
  E(0xBF);
  E(0x00);
  E(0xCC);
  E(0x7E);
  // CMP #15
  E(0xC9);
  E(0x15);
  // BCC/BLT increment
  E(0x90);
  E(0x04);

  // CMP #2A
  E(0xC9);
  E(0x2A);
  // BCC found
  E(0x90);
  E(0x0E);

  // increment:
  // INX
  E(0xE8);
  // INY
  E(0xC8);
  // CPY #000C
  E(0xC0);
  E(0x0C);
  E(0x00);
  // BNE loop
  E(0xD0);
  E(0xED);

  // LDA #01
  E(0xA9);
  E(0x01);
  // STA 7ECF0F
  E(0x8F);
  E(0x0F);
  E(0xCF);
  E(0x7E);
  // RTL
  E(0x6B);

  // found:
  // LDA #00
  E(0xA9);
  E(0x00);
  // STA 7ECF0F
  E(0x8F);
  E(0x0F);
  E(0xCF);
  E(0x7E);
  // JSL sub_eraseItem
  E(0x22);
  E((uint8_t)subEraseItemOffset);
  E((uint8_t)(subEraseItemOffset >> 8));
  E((uint8_t)((subEraseItemOffset >> 16) + 0xC0));
  // RTL
  E(0x6B);


  // remove first accessory in inventory
  int subRemoveAccessoryOffset = c->working_offset;
  // LDX #003C
  E(0xA2);
  E(0x3C);
  E(0x00);
  // loop:
  // LDA 7ECC00,x
  E(0xBF);
  E(0x00);
  E(0xCC);
  E(0x7E);
  // CMP #2A
  E(0xC9);
  E(0x2A);
  // BCC/BLT increment
  E(0x90);
  E(0x04);
  // CMP #3F
  E(0xC9);
  E(0x3F);
  // BCC found
  E(0x90);
  E(0x0E);

  // increment:
  // INX
  E(0xE8);
  // INY
  E(0xC8);
  // CPY #000C
  E(0xC0);
  E(0x0C);
  E(0x00);
  // BNE loop
  E(0xD0);
  E(0xED);

  // LDA #01
  E(0xA9);
  E(0x01);
  // STA 7ECF0F
  E(0x8F);
  E(0x0F);
  E(0xCF);
  E(0x7E);
  // RTL
  E(0x6B);

  // found:
  // LDA #00
  E(0xA9);
  E(0x00);
  // STA 7ECF0F
  E(0x8F);
  E(0x0F);
  E(0xCF);
  E(0x7E);
  // JSL sub_eraseItem
  E(0x22);
  E((uint8_t)subEraseItemOffset);
  E((uint8_t)(subEraseItemOffset >> 8));
  E((uint8_t)((subEraseItemOffset >> 16) + 0xC0));
  // RTL
  E(0x6B);


  // main replacement:
  P(0x1EB5C, 0x22);
  P(0x1EB5D, (uint8_t)c->working_offset);
  P(0x1EB5E, (uint8_t)(c->working_offset >> 8));
  P(0x1EB5F, (uint8_t)((c->working_offset >> 16) + 0xC0));

  // CMP #FF
  E(0xC9);
  E(0xFF);
  // BEQ over [5]
  E(0xF0);
  E(0x05);
  // JSL C00051 - not 0xFF; just do vanilla behavior
  E(0x22);
  E(0x51);
  E(0x00);
  E(0xC0);
  // RTL
  E(0x6B);

  // over:
  // LDA [$D1] - read our extra byte
  E(0xA7);
  E(0xD1);
  // LDX $D1
  E(0xA6);
  E(0xD1);
  // INX - increment the event by one more byte
  E(0xE8);
  // STX $D1
  E(0x86);
  E(0xD1);
  // now a similar loop to what adding does in bank 0 (C0/64BD), except we're trying to remove
  // starting around C0/64D5:   A00000     LDY #$0000
  // LDY #0000    A0 00 00
  E(0xA0);
  E(0x00);
  E(0x00);
  // CMP #15
  E(0xC9);
  E(0x15);
  // BCS/BGE over_hats (8)
  E(0xB0);
  E(0x08);
  // LDX #0024
  E(0xA2);
  E(0x24);
  E(0x00);
  // JSL sub_remove_direct_match
  E(0x22);
  E((uint8_t)subDirectMatchOffset);
  E((uint8_t)(subDirectMatchOffset >> 8));
  E((uint8_t)((subDirectMatchOffset >> 16) + 0xC0));
  // RTL
  E(0x6B);

  // over_hats:
  // CMP #2A
  E(0xC9);
  E(0x2A);
  // BCS over_armors
  E(0xB0);
  E(0x08);
  // LDX #0030
  E(0xA2);
  E(0x30);
  E(0x00);
  // JSL sub_remove_direct_match
  E(0x22);
  E((uint8_t)subDirectMatchOffset);
  E((uint8_t)(subDirectMatchOffset >> 8));
  E((uint8_t)((subDirectMatchOffset >> 16) + 0xC0));
  // RTL
  E(0x6B);

  // over_armors:
  // CMP #3F
  E(0xC9);
  E(0x3F);
  // BCS over_acc
  E(0xB0);
  E(0x08);
  // LDX #003C
  E(0xA2);
  E(0x3C);
  E(0x00);
  // JSL sub_remove_direct_match
  E(0x22);
  E((uint8_t)subDirectMatchOffset);
  E((uint8_t)(subDirectMatchOffset >> 8));
  E((uint8_t)((subDirectMatchOffset >> 16) + 0xC0));
  // RTL
  E(0x6B);

  // over_acc:
  // consumables in range 0x40 - 0x4C here
  // CMP #4C
  E(0xC9);
  E(0x4C);
  // BCS over_consumable
  E(0xB0);
  E(0x07);
  // AND #1F
  E(0x29);
  E(0x1F);
  // JSL sub_remove_consumable
  E(0x22);
  E((uint8_t)subRemoveConsumableOffset);
  E((uint8_t)(subRemoveConsumableOffset >> 8));
  E((uint8_t)((subRemoveConsumableOffset >> 16) + 0xC0));
  // RTL
  E(0x6B);

  // CMP #FF - any hat
  E(0xC9);
  E(0xFF);
  // BNE over_hat
  E(0xD0);
  E(0x05);
  // JSL sub_remove_hat
  E(0x22);
  E((uint8_t)subRemoveHatOffset);
  E((uint8_t)(subRemoveHatOffset >> 8));
  E((uint8_t)((subRemoveHatOffset >> 16) + 0xC0));
  // RTL
  E(0x6B);

  // over_hat:
  // CMP #FE - any armor
  E(0xC9);
  E(0xFE);
  // BNE over_armor
  E(0xD0);
  E(0x05);
  // JSL sub_remove_armor
  E(0x22);
  E((uint8_t)subRemoveArmorOffset);
  E((uint8_t)(subRemoveArmorOffset >> 8));
  E((uint8_t)((subRemoveArmorOffset >> 16) + 0xC0));
  // RTL
  E(0x6B);

  // over_armor:
  // CMP #FD - any accessory
  E(0xC9);
  E(0xFD);
  // BNE over_accessory
  E(0xD0);
  E(0x05);
  // JSL sub_remove_accessory
  E(0x22);
  E((uint8_t)subRemoveAccessoryOffset);
  E((uint8_t)(subRemoveAccessoryOffset >> 8));
  E((uint8_t)((subRemoveAccessoryOffset >> 16) + 0xC0));
  // RTL
  E(0x6B);

  // over_accessory:
  // LDA #01
  E(0xA9);
  E(0x01);
  // STA 7eCF0F - done and we didn't do anything
  E(0x8F);
  E(0x0F);
  E(0xCF);
  E(0x7E);
  // RTL
  E(0x6B);

}
