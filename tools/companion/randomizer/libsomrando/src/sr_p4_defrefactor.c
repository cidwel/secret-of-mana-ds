/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * DefenseRefactor, derived from processing/hacks/openworld/DefenseRefactor.cs (Author: Moppleton).  The byte-emitting parts were converted
 * mechanically (comments are those of the C#), the two tables use libm pow() like Math.Pow.  Translated to C 2026-10-18.
 */
#include <math.h>

#include "sr_core.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))

static void inject_new_def_code(struct sr_ctx *c, int defenseTableOffset, int mul16BitSubrOffset, int subtractor) {

  // PHX
  E(0xDA);
  // PHY
  E(0x5A);
  // LDA $94 - defense
  E(0xA5);
  E(0x94);
  // TAX
  E(0xAA);
  // LDA defenseTableOffset,x
  E(0xBF);
  E((defenseTableOffset));
  E((defenseTableOffset >> 8));
  E(((defenseTableOffset >> 16) + 0xC0));

  // AND #00FF
  E(0x29);
  E(0xFF);
  E(0x00);

  // TAY
  E(0xA8);

  // SEP #10
  E(0xE2);
  E(0x10);

  // LDA $89
  E(0xA5);
  E(0x89);

  // JSL mul16BitSubrOffset
  E(0x22);
  E((mul16BitSubrOffset));
  E((mul16BitSubrOffset >> 8));
  E(((mul16BitSubrOffset >> 16) + 0xC0));
  // LSR
  E(0x4A);
  // LSR
  E(0x4A);
  // LSR
  E(0x4A);
  // LSR
  E(0x4A);
  // LSR
  E(0x4A);
  // LSR
  E(0x4A);
  // REP #10
  E(0xC2);
  E(0x10);

  // PLY
  E(0x7A);
  // PLX
  E(0xFA);
  // PHA
  E(0x48);
  // SEP 20
  E(0xE2);
  E(0x20);
  // apply rng too
  // JSL C03872
  E(0x22);
  E(0x72);
  E(0x38);
  E(0xC0);
  // REP 20
  E(0xC2);
  E(0x20);
  // AND #000F
  E(0x29);
  E(0x0F);
  E(0x00);
  // STA $89
  E(0x85);
  E(0x89);
  // PLA
  E(0x68);

  // CMP #0040
  E(0xC9);
  E(0x40);
  E(0x00);
  // BGE over
  E(0xB0);
  E(0x00);
  // PHA
  E(0x48);
  // LDA $89
  E(0xA5);
  E(0x89);
  // AND #0003
  E(0x29);
  E(0x03);
  E(0x00);
  // STA $89
  E(0x85);
  E(0x89);
  // PLA
  E(0x68);

  // over:
  // CLC
  E(0x38);
  // ADC $89
  E(0x65);
  E(0x89);
  // SEC
  E(0x38);
  // SBC #0008 -> 18
  E(0xE9);
  E(subtractor);
  E(0x00);

  // more experimentals
  // PHA
  E(0x48);
  // LDA $94
  E(0xA5);
  E(0x94);
  // LSR
  E(0x4A);
  // LSR
  E(0x4A);
  // LSR
  E(0x4A);
  // LSR
  E(0x4A);
  // STA $89
  E(0x85);
  E(0x89);
  // PLA
  E(0x68);
  // SEC
  E(0x38);
  // SBC $89
  E(0xE5);
  E(0x89);
  // RTL
  E(0x6B);
}

/* Defense refactor - upstream processing/hacks/openworld/DefenseRefactor.cs */
int sr_hack_defense_refactor(struct sr_ctx *c) {
  if (!sr_settings_get_bool(c->settings, "defRefactor")) return 0;

  sr_ensure_space_in_bank(&c->working_offset, 100);

  // utility method to do a 16 bit * 8 bit multiply, which isn't directly supported by SNES architecture
  // performs: (16bit) A *= (8bit) Y
  int mul16BitSubrOffset = c->working_offset;

  // SEP 20
  E(0xE2);
  E(0x20);
  // TYX
  E(0xBB); /* X = [0x56] (copy of Y) */
  // PHB - save off previous bank
  E(0x8B);
  // PHA - save off A temporarily
  E(0x48);
  // LDA #00
  E(0xA9);
  E(0x00);
  // PHA - push 0 to restore as the bank
  E(0x48);
  // PLB - set bank to 0
  E(0xAB);
  // PLA - pull saved A
  E(0x68);
  // multiply lower 8 bits of A * Y
  // sta 4202
  E(0x8D); /* $4202 = [0x34] */
  E(0x02);
  E(0x42);
  // sty 4203
  E(0x8C); /* $4203 = [0x56] */
  E(0x03);
  E(0x42);
  // nop to wait
  E(0xEA); /* $4216 = [0x78] (LSB of result 0x34 * 0x56 = 0x1178) */
  E(0xEA); /* $4217 = [0x11] (MSB of result 0x34 * 0x56 = 0x1178) */
  E(0xEA);
  E(0xEA);
  // lda 4216 - A = LSB of result
  E(0xAD); /* A = [0x12,0x78] (set LSB only since we're SEP 20) */
  E(0x16);
  E(0x42);
  // ldy 4217 - Y = MSB of result
  E(0xAC); /* Y = [0x11] */
  E(0x17);
  E(0x42);
  // xba - swap LSB and MSB of A
  E(0xEB); /* A = [0x78,0x12] */
  // multiply upper 8 bits of A * Y (copied in X)
  // sta 4202
  E(0x8D); /* $4202 = [0x12] */
  E(0x02);
  E(0x42);
  // stx 4203
  E(0x8E); /* $4203 = [0x56] */
  E(0x03);
  E(0x42);
  // nop to wait for the multiply
  E(0xEA); /* $4216 = [0x0C] (LSB of result 0x12 * 0x56 = 0x60C) */
  E(0xEA); /* $4217 = [0x06] (MSB of result 0x12 * 0x56 = 0x60C) */
  E(0xEA);
  E(0xEA);
  E(0xEA);
  E(0xEA);
  // tya
  E(0x98); /* A = [0x78,0x11] (again, set LSB only because 8-bit A mode) */
  // clc
  E(0x18); /* prepare to add 0x0C to */
  // adc 4216
  E(0x6D); /* A = [0x78,0x1D] */
  E(0x16);
  E(0x42);
  // LDY $4217
  E(0xAC); /* Y = [0x06] */
  E(0x17);
  E(0x42);
  // BCC/BLT over the INY
  E(0x90);
  E(0x01);
  // INY for addition overflow (numeric example here doesn't encounter this)
  E(0xC8); /* Y = [0x06] */
  // XBA
  E(0xEB); /* A = [0x1D,0x78] */
  // REP 20
  E(0xC2);
  E(0x20);
  // PLB - restore previous bank
  E(0xAB);
  // RTL - done
  E(0x6B);

  // table of values representing the percentage of damage allowed through defense at each defense value (out of 64)
  sr_ensure_space_in_bank(&c->working_offset, 1000);
  int defenseTableOffset = c->working_offset;
  int maxDef = 500;
  for (int i = 0; i < 1000; i++) {
    int def = i;
    if (def > maxDef) def = maxDef;
    double ratio = pow(((maxDef + 1) - def) / (double)maxDef, 1.5);
    int damageMinimum = 1;
    int damageMaximum = 63;
    E((int)(damageMinimum + ratio * (damageMaximum - damageMinimum)));
  }

  // the same table for magic damage
  sr_ensure_space_in_bank(&c->working_offset, 1000);
  int magicDefenseTableOffset = c->working_offset;
  for (int i = 0; i < 1000; i++) {
    int def = i;
    if (def > maxDef) def = maxDef;
    double ratio = pow(((maxDef + 1) - def) / (double)maxDef, 1.1);
    int damageMinimum = 4;
    int damageMaximum = 63;
    E((int)(damageMinimum + ratio * (damageMaximum - damageMinimum)));
  }

  // replace: (load damage in $89, and subtract phys defense in $94)
  sr_ensure_space_in_bank(&c->working_offset, 100);
  P(0x516B, 0x22);
  P(0x516C, c->working_offset);
  P(0x516D, c->working_offset >> 8);
  P(0x516E, (c->working_offset >> 16) + 0xC0);
  P(0x516F, 0xEA);
  inject_new_def_code(c, defenseTableOffset, mul16BitSubrOffset, 0x0c);

  // replace: (load damage in $89, and subtract magic defense in $94)
  sr_ensure_space_in_bank(&c->working_offset, 100);
  P(0x8e9e1, 0x22);
  P(0x8e9e2, c->working_offset);
  P(0x8e9e3, c->working_offset >> 8);
  P(0x8e9e4, (c->working_offset >> 16) + 0xC0);
  P(0x8e9e5, 0xEA);
  inject_new_def_code(c, magicDefenseTableOffset, mul16BitSubrOffset, 0x04);
  return 1;
}
