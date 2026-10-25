/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Phase 2: RabiteColorRandomizer here; CharacterPaletteRandomizer in sr_p2_visual_palette.c, StatusGlow in sr_p2_visual_glow.c.
 * Author of every upstream hack: Moppleton.  Translated to C 2026-10-18.
 */
#include <string.h>

#include "sr_core.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))
#define SB(key) sr_settings_get_bool(c->settings, (key))

/* RabiteColorRandomizer (processing/hacks/common/other/RabiteColorRandomizer.cs): author Moppleton. */
static void rabite_call_random_subr(struct sr_ctx *c) {
  // call the vanilla RNG subroutine, which puts a "random" value 00-FF into A

  // phd
  E(0x0B);
  // rep 20
  E(0xC2);
  E(0x20);
  // LDA #0300
  E(0xA9);
  E(0x00);
  E(0x03);
  // PHA
  E(0x48);
  // PLD
  E(0x2B);
  // sep 20
  E(0xE2);
  E(0x20);
  // JSL
  E(0x22);
  E(0x9C);
  E(0x38);
  E(0xC0);
  // PLD
  E(0x2B);
}

static void rabite_darken_to_75(struct sr_ctx *c, int src, int dst) {
  // //////////////////
  // RED
  // //////////////////

  // LDA src,y  (lsb = red/green byte)
  E(0xB9);
  E((uint8_t)src);
  E((uint8_t)(src>>8));

  // AND #1F - isolate the red component
  E(0x29);
  E(0x1F);

  // LSR
  E(0x4A);
  // LSR
  E(0x4A);

  // PHA - push value * .25
  E(0x48);

  // LDA src,y  (lsb = red/green byte)
  E(0xB9);
  E((uint8_t)src);
  E((uint8_t)(src >> 8));

  // AND #1F - isolate the red component
  E(0x29);
  E(0x1F);

  // SEC - prepare to subtract
  E(0x38);

  // SBC $01,s - subtract value * .25 from full value
  E(0xE3);
  E(0x01);

  // STA $01,s - stick back into stack
  E(0x83);
  E(0x01);

  // PLA - extract 75% scaled value and even out the stack
  E(0x68);

  // STA dst,y  (lsb = red/green byte)
  E(0x99);
  E((uint8_t)dst);
  E((uint8_t)(dst >> 8));


  // //////////////////
  // GREEN
  // //////////////////

  // again, green is the complicated one, because it spans both color bytes.

  // LDA src,y  (lsb = red/green byte)
  E(0xB9);
  E((uint8_t)src);
  E((uint8_t)(src >> 8));

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
  // PHA - save off partially created green value
  E(0x48);

  // LDA src+1,y  (msb = green/blue byte)
  E(0xB9);
  E((uint8_t)(src+1));
  E((uint8_t)((src+1) >> 8));

  // AND #03
  E(0x29);
  E(0x03);

  // ASL
  E(0x0A);
  // ASL
  E(0x0A);
  // ASL
  E(0x0A);

  // ORA $01,s - "or" with the partial value off the first byte
  E(0x03);
  E(0x01);

  // STA $01,s - write the full green value back to the stack
  E(0x83);
  E(0x01);

  // PLA - pull the same value back out to even out the stack
  E(0x68);

  // PHA - push back original green value
  E(0x48);

  // LSR
  E(0x4A);
  // LSR
  E(0x4A);

  // PHA - push value * .25
  E(0x48);

  // A3 02  LDA $02,s - load full value from back earlier
  E(0xA3);
  E(0x02);

  // MOPPLE: shouldn't this code be SEC-ing before SBC? is the green slightly off because of this?

  // SBC $01,s - subtract value * .25 that was pushed to the stack from full value
  E(0xE3);
  E(0x01);

  // STA $01,s - store back into the stack
  E(0x83);
  E(0x01);

  // ASL
  E(0x0A);
  // ASL
  E(0x0A);
  // ASL
  E(0x0A);
  // ASL
  E(0x0A);
  // ASL
  E(0x0A);

  // ORA dst,y - "or" w/ red value
  E(0x19);
  E((uint8_t)(dst));
  E((uint8_t)((dst) >> 8));

  // STA dst,y  (lsb = red/green byte)
  E(0x99);
  E((uint8_t)(dst));
  E((uint8_t)((dst) >> 8));

  // PLA - grab 75% value again
  E(0x68);

  // LSR
  E(0x4A);
  // LSR
  E(0x4A);
  // LSR
  E(0x4A);

  // STA dst+1,y  (msb = green/blue byte)
  E(0x99);
  E((uint8_t)(dst + 1));
  E((uint8_t)((dst + 1) >> 8));

  // PLA - even out the stack
  E(0x68);


  // //////////////////
  // BLUE
  // //////////////////

  // LDA src+1, y  (msb = green/blue byte)
  E(0xB9);
  E((uint8_t)(src + 1));
  E((uint8_t)((src + 1) >> 8));

  // LSR
  E(0x4A);
  // LSR
  E(0x4A);
  // LSR
  E(0x4A);
  // LSR
  E(0x4A);

  // PHA - push value * .25
  E(0x48);

  // LDA src+1, y  (msb = green/blue byte)
  E(0xB9);
  E((uint8_t)(src + 1));
  E((uint8_t)((src + 1) >> 8));

  // LSR
  E(0x4A);
  // LSR
  E(0x4A);

  // SEC
  E(0x38);

  // E3 01  SBC $01,s - subtract value * .25 from full value
  E(0xE3);
  E(0x01);

  // 83 01  STA $01,s - stick back into stack then pull out
  E(0x83);
  E(0x01);

  // PLA
  E(0x68);

  // ASL
  E(0x0A);
  // ASL
  E(0x0A);
  // ORA dst+1, y - "or" w/ existing green value from earlier
  E(0x19);
  E((uint8_t)(dst + 1));
  E((uint8_t)((dst + 1) >> 8));

  // STA dst+1, y  (msb = green/blue byte)
  E(0x99);
  E((uint8_t)(dst + 1));
  E((uint8_t)((dst + 1) >> 8));
}

int sr_hack_rabite_color(struct sr_ctx *c) {
  if (!SB("rainbowRabites")) return 0;
  // replace:
  // $00/E03E 22 3F 00 C0 JSL $C0003F[$C0:003F]   A:7F05 X:0600 Y:0170 P:envMxdIzC
  // A is 8bit, xy is 16bit
  // need to preserve A and X, but don't think Y matters
  P(0xE03E, 0x22);
  P(0xE03F, c->working_offset & 0xFF);
  P(0xE040, (uint8_t)(c->working_offset >> 8));
  P(0xE041, (uint8_t)((c->working_offset >> 16) + 0xC0));

  // REP 20
  E(0xC2);
  E(0x20);

  // PHA
  E(0x48);

  // SEP 20
  E(0xE2);
  E(0x20);

  // LDA $E180,x[$7E:E380] - load species (8 bit)
  E(0xBD);
  E(0x80);
  E(0xE1);

  // CMP #00
  E(0xC9);
  E(0x00);

  // BNE end - not a rabite; don't do anything
  E(0xF0);
  E(0x0A);

  // REP 20
  E(0xC2);
  E(0x20);

  // PLA - clean up stack, restore A
  E(0x68);

  // SEP 20
  E(0xE2);
  E(0x20);

  // replaced code
  E(0x22);
  E(0x3F);
  E(0x00);
  E(0xC0);

  // RTL
  E(0x6B);

  // over:
  // LDY $E0F8,x - load palette offset
  E(0xBC);
  E(0xF8);
  E(0xE0);

  // ////////////////////////////////////////////
  // generate a random color for the rabite, by calling the vanilla RNG subroutine, and
  // "and"ing the result with 0x0F.  Do this three times, one for red, one for green, one for
  // blue.  Add 0x10 to each value, since it will be scaled down to make darker colors.
  //
  // Scale this RGB value down to make darker versions of the same color for different
  // palette indexes.
  // Used for indexes 2,11,12,13,14; locations x02,x14,16,18,1A
  //
  // note: snes color format: xBBBBBGG GGGRRRRR (msb, lsb)

  // ///////////////////////////////////////
  // generate random red value 0x10 - 0x1F
  // ///////////////////////////////////////
  rabite_call_random_subr(c);

  // AND #0F
  E(0x29);
  E(0x1F);

  // CLC
  E(0x18);

  // ADC #10
  E(0x69);
  E(0x08);

  // CMP #1F
  E(0xC9);
  E(0x20);

  // BCC over
  E(0x90);
  E(0x02);

  E(0xA9);
  E(0x1F);

  // STA $DF1A,y  (lsb = red/green byte)
  E(0x99);
  E(0x1A);
  E(0xDF);

  // ///////////////////////////////////////
  // generate random green value 0x10 - 0x1F
  // ///////////////////////////////////////

  // green is the complicated one since it spans both bytes
  rabite_call_random_subr(c);

  // AND #0F
  E(0x29);
  E(0x1F);

  // CLC
  E(0x18);

  // ADC #10
  E(0x69);
  E(0x08);

  // CMP #1F
  E(0xC9);
  E(0x20);

  // BCC over
  E(0x90);
  E(0x02);

  // LDA #1F
  E(0xA9);
  E(0x1F);

  // PHA - save off the green value since we need to split it up
  E(0x48);

  // ASL x 5
  E(0x0A);
  E(0x0A);
  E(0x0A);
  E(0x0A);
  E(0x0A);

  // ORA $DF1A,y  (lsb = red/green byte) - "or" with the red value stored earlier
  E(0x19);
  E(0x1A);
  E(0xDF);

  // STA $DF1A,y  (lsb = red/green byte)
  E(0x99);
  E(0x1A);
  E(0xDF);

  // PLA - pull out the saved green value
  E(0x68);

  // LSR x 3
  E(0x4A);
  E(0x4A);
  E(0x4A);

  // STA $DF1B,y  (msb = green/blue byte)
  E(0x99);
  E(0x1B);
  E(0xDF);

  // ///////////////////////////////////////
  // generate random blue value 0x10 - 0x1F
  // ///////////////////////////////////////
  rabite_call_random_subr(c);

  // AND #0F
  E(0x29);
  E(0x1F);

  // CLC
  E(0x18);

  // ADC #10
  E(0x69);
  E(0x08);

  // CMP #1F
  E(0xC9);
  E(0x20);

  // BCC over
  E(0x90);
  E(0x02);

  // LDA #1F
  E(0xA9);
  E(0x1F);

  // over:
  // ASL
  E(0x0A);
  // ASL
  E(0x0A);

  // 19 1B DF   ORA $DF1B, y - or w/ green
  E(0x19);
  E(0x1B);
  E(0xDF);

  // 99 1B DF   STA $DF1B, y - msb green+blue
  E(0x99);
  E(0x1B);
  E(0xDF);

  // generate darker versions of the color for other indexes
  rabite_darken_to_75(c, 0xDF1A, 0xDF18);
  rabite_darken_to_75(c, 0xDF18, 0xDF16);
  rabite_darken_to_75(c, 0xDF16, 0xDF14);
  rabite_darken_to_75(c, 0xDF14, 0xDF02);

  // for now, the white = the color before it
  E(0xB9);  // LDA
  E(0x1A);
  E(0xDF);
  E(0x99);  // STA
  E(0x1C);
  E(0xDF);
  E(0xB9);  // LDA
  E(0x1B);
  E(0xDF);
  E(0x99);  // STA
  E(0x1D);
  E(0xDF);

  // copy DF1A values to 071A .. 14,15,16,17,18,19,1A,1B
  // the four yellowish colors in OG rabite palette
  for (int lsb = 0x14; lsb <= 0x1D; lsb++)
  {
  E(0xB9);  // LDA
  E(lsb);
  E(0xDF);
  E(0x99);  // STA
  E(lsb);
  E(0x07);
  }

  // this one, too - gold tinted outline of OG rabite
  for (int lsb = 0x02; lsb <= 0x03; lsb++)
  {
  E(0xB9);  // LDA
  E(lsb);
  E(0xDF);
  E(0x99);  // STA
  E(lsb);
  E(0x07);
  }

  // REP 20
  E(0xC2);
  E(0x20);

  // PLA
  E(0x68);

  // SEP 20
  E(0xE2);
  E(0x20);

  // JSL $C0003F - replaced code
  E(0x22);
  E(0x3F);
  E(0x00);
  E(0xC0);

  // RTL
  E(0x6B);

  // this is a block that resets palettes when the target is poisoned - presumably only for players,
  // but it applies to every object here.  doing this re-loads the vanilla palette for rabites,
  // so modify it to skip the load for object type 0 == rabite.

  // $01/946A 20 22 E0    JSR $E022  [$01:E022]   A:2000 X:0600 Y:2000 P:envMxdIZC
  // $01/946D E2 20       SEP #$20                A:7FFF X:0600 Y:2000 P:envmxdIzC

  P(0x1946A, 0x22);
  P(0x1946B, c->working_offset & 0xFF);
  P(0x1946C, (uint8_t)(c->working_offset >> 8));
  P(0x1946D, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x1946E, 0xEA);

  // LDA 7EE180,X
  E(0xBF);
  E(0x80);
  E(0xE1);
  E(0x7E);
  // BNE over [01]
  E(0xD0);
  E(0x01);
  // RTL
  E(0x6B);
  // over:
  // copy vanilla subroutine: 0x01E022->0x01E06E
  for (int i=0x1E022; i <= 0x1E06E; i++)
  {
  int newByte = sr_peek(c, i);
  if(newByte == 0x60)
  {
  // SEP 20 - replaced code
  E(0xE2);
  E(0x20);
  // RTL
  newByte = 0x6B;
  }
  E(newByte);
  }
  return 1;
}
