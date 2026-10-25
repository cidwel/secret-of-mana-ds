/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49.  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * StatusGlow (processing/hacks/common/enhancement/StatusGlow.cs): ff6-like palette glow for status conditions; author Moppleton.
 * Translated to C 2026-10-18; the sine/cube math keeps the C# operation order (compiled with -ffp-contract=off).
 */
#include <math.h>
#include <string.h>

#include "sr_core.h"
#include "sr_p2_visual_color.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))
#define SB(key) sr_settings_get_bool(c->settings, (key))

#define SG_BYTE_1 (0x7E0400 + 41)
#define SG_BYTE_2 (0x7E0400 + 44)
#define SG_PI 3.14159265358979323846

int sr_hack_status_glow(struct sr_ctx *c) {
  if (!SB("statusGlow")) return 0;

  int christmas_mode = 0;
  if (!strcmp(sr_settings_get(c->settings, "mode"), "rando")) christmas_mode = !strcmp(sr_settings_get(c->settings, "rHoliday"), "xmas");

  /* data first: 16 palettes for each status condition, taken from the (possibly randomized) output palettes */
  int palette_location_start = c->working_offset;
  for (int status_type = 0; status_type <= 9; status_type++) {
    for (int char_num = 0; char_num <= 2; char_num++) {
      for (int pal_num = 0; pal_num < 16; pal_num++) {
        E(0);
        E(0);
        for (int pal_index = 1; pal_index <= 15; pal_index++) {
          double sine_scale = fabs(sin((pal_num + pal_index) * SG_PI / 16.0));
          double sine_scale_offset1 = fabs(sin((pal_num + pal_index + 5) * SG_PI / 16.0));
          double sine_scale_offset2 = fabs(sin((pal_num + pal_index + 10) * SG_PI / 16.0));
          double sine_scale_fast = fabs(sin((pal_num + pal_index) * SG_PI / 8.0));
          double sine_scale_fast_offset1 = fabs(sin((pal_num + pal_index + 5) * SG_PI / 8.0));
          double sine_scale_fast_offset2 = fabs(sin((pal_num + pal_index + 10) * SG_PI / 8.0));
          int is_outline = (char_num == 0 && pal_index == 1) || (char_num == 1 && pal_index == 1) ||
                           (char_num == 2 && (pal_index == 1 || pal_index == 2 || pal_index == 9)) || pal_index == 15;
          int is_skin = (char_num == 0 && (pal_index == 5 || pal_index == 6 || pal_index == 7)) ||
                        (char_num == 1 && (pal_index == 5 || pal_index == 6 || pal_index == 7)) ||
                        (char_num == 2 && (pal_index == 6 || pal_index == 7));

          struct sr_w2_color k;
          int src = 0x80FFE + (0x80 + char_num) * 0x1E + pal_index * 2;
          k.b[0] = sr_peek(c, src);
          k.b[1] = sr_peek(c, src + 1);
          if (char_num == 2 && (pal_index == 1 || pal_index == 2 || pal_index == 9)) {
            sr_w2_set_red(&k, 48);
            sr_w2_set_green(&k, 48);
            sr_w2_set_blue(&k, 48);
          }

          double sine_amp1 = 200;
          double sine_amp2 = -100;
          double s3 = sine_scale * sine_scale * sine_scale;
          double so1 = sine_scale_offset1 * sine_scale_offset1 * sine_scale_offset1;
          double so2 = sine_scale_offset2 * sine_scale_offset2 * sine_scale_offset2;
          double sf = sine_scale_fast * sine_scale_fast * sine_scale_fast;
          double sf1 = sine_scale_fast_offset1 * sine_scale_fast_offset1 * sine_scale_fast_offset1;
          double sf2 = sine_scale_fast_offset2 * sine_scale_fast_offset2 * sine_scale_fast_offset2;
          switch (status_type) {
            case 0: /* slow: light */
              if (!is_outline) {
                sr_w2_color_add(&k, 48, 48, 48);
                sr_w2_color_add(&k, (int)(s3 * sine_amp1), (int)(s3 * sine_amp1), (int)(s3 * sine_amp1));
              }
              break;
            case 1: /* mute: dark */
              if (!is_outline) {
                sr_w2_color_add(&k, -48, -48, -48);
                sr_w2_color_add(&k, (int)(s3 * sine_amp2), (int)(s3 * sine_amp2), (int)(s3 * sine_amp2));
              }
              break;
            case 2: /* poison: animated purple skin */
              if (is_skin) {
                sr_w2_color_add(&k, 48, -48, 48);
                sr_w2_color_add(&k, (int)(s3 * sine_amp1), (int)(s3 * sine_amp2), (int)(s3 * sine_amp1));
              }
              break;
            case 3: /* moon energy: rainbow */
              if (!is_outline) {
                sr_w2_color_add(&k, -48, -48, -48);
                sr_w2_color_add(&k, (int)(sf * sine_amp1), (int)(sf1 * sine_amp1), (int)(sf2 * sine_amp1));
              }
              break;
            case 4: /* acid rain: gross green */
              if (!is_outline) {
                sr_w2_color_add(&k, -96, -64, -128);
                sr_w2_color_add(&k, (int)(s3 * sine_amp1), 0, (int)(s3 * sine_amp2));
              }
              break;
            case 5: /* speedup: red */
              if (!is_outline) {
                sr_w2_color_add(&k, 96, -32, -32);
                sr_w2_color_add(&k, (int)(s3 * sine_amp1 * 2), (int)(so1 * sine_amp2), -(int)(so2 * sine_amp2));
              }
              break;
            case 6: /* lunar boost: fast orange */
              if (!is_outline) {
                sr_w2_color_add(&k, 48, 0, -48);
                sr_w2_color_add(&k, (int)(sf * sine_amp1 * 1.5), 0, (int)(sf * sine_amp2 * 1.5));
              }
              break;
            case 7: /* lucid barrier: yellow */
              if (!is_outline) {
                sr_w2_color_add(&k, 48, 48, -48);
                sr_w2_color_add(&k, (int)(s3 * sine_amp1), (int)(so1 * sine_amp1), (int)(so2 * sine_amp2));
              }
              break;
            case 8: /* wall: green */
              if (!is_outline) {
                sr_w2_color_add(&k, -48, 48, -48);
                sr_w2_color_add(&k, (int)(s3 * sine_amp2), (int)(so1 * sine_amp1), (int)(so2 * sine_amp2));
              }
              break;
            case 9: /* defender: blue */
              if (!is_outline) {
                sr_w2_color_add(&k, 0, -24, 96);
                sr_w2_color_add(&k, (int)(s3 * sine_amp2), (int)(so1 * sine_amp2), (int)(so2 * sine_amp1));
              }
              break;
          }
          E(k.b[0]);
          E(k.b[1]);
        }
      }
    }
  }
  // new code:

  int resetPaletteSubr = c->working_offset;
  // x 16 bit, a 8 bit
  // x is the 0x000, 0x200, 0x400 - nope, it's 0,1,2
  // PHY - push starting y
  E(0x5A);
  // PHX - push starting x
  E(0xDA);

  // REP 20
  E(0xC2);
  E(0x20);

  // PHA - push starting a
  E(0x48);

  //
  // TXA
  E(0x8A);
  // ASL * 5
  E(0x0A);
  E(0x0A);
  E(0x0A);
  E(0x0A);
  E(0x0A);

  // PHA - push 00,20,40
  E(0x48);
  // TXA
  E(0x8A);
  // ASL
  E(0x0A);
  // EOR #FFFF
  E(0x49);
  E(0xFF);
  E(0xFF);
  // INC A - minus 00,02,04
  E(0x1A);
  // CLC
  E(0x18);
  // ADC $01,s - this should be 00,1E,3C
  E(0x63);
  E(0x01);
  // TAX
  E(0xAA);
  // PLY - y is 00, 20, 40
  E(0x7A);
  //
  // loop:
  // LDA #C81EFE,x
  E(0xBF);
  E(0xFE);
  E(0x1E);
  E(0xC8);
  // STA $DF20,y
  E(0x99);
  E(0x20);
  E(0xDF);
  // STA $0720,y
  E(0x99);
  E(0x20);
  E(0x07);
  // INY
  E(0xC8);
  // INY
  E(0xC8);
  // INX
  E(0xE8);
  // INX
  E(0xE8);
  // TYA
  E(0x98);
  // AND #001F
  E(0x29);
  E(0x1F);
  E(0x00);
  // BEQ out (02)
  E(0xF0);
  E(0x02);
  // BRA loop
  E(0x80);
  E(0xEA);
  //
  // out:
  // PLA
  E(0x68);

  // SEP 20
  E(0xE2);
  E(0x20);

  // PLX
  E(0xFA);
  // PLY
  E(0x7A);
  // RTL
  E(0x6B);




  int zeroSubAddr = c->working_offset;
  // ////////////////////////
  // timerZeroSub:
  // ////////////////////////
  //
  //             PHY
  E(0x5A);
  //             REP 20  - temporary 16 bit a
  E(0xC2);
  E(0x20);
  //             TXA
  E(0x8A);
  //             XBA
  E(0xEB);
  //             LSR
  E(0x4A);
  //             TAY     - y is 0, 1, 2 based on character
  E(0xA8);
  // 64 A4       STZ $A4 - bits indicating current status (16 bit)
  E(0x64);
  E(0xA4);
  //             SEP 20  - restore 8 bit a
  E(0xE2);
  E(0x20);
  // 64 98       STZ $98 - counter of how many statuses we've got
  E(0x64);
  E(0x98);
  E(0x64);
  E(0x99);
  //
  // BD 90 E1    LDA $e190,x
  E(0xBD);
  E(0x90);
  E(0xE1);
  // 29 04       AND #04  -  slow
  E(0x29);
  E(0x04);
  // F0 08       BEQ 08
  E(0xF0);
  E(0x08);
  // E6 98       INC $98
  E(0xE6);
  E(0x98);
  //             LDA $A4
  E(0xA5);
  E(0xA4);
  //             ORA #01
  E(0x09);
  E(0x01);
  //             STA $A4
  E(0x85);
  E(0xA4);
  //
  // BD 90 E1    LDA $e190,x
  E(0xBD);
  E(0x90);
  E(0xE1);
  // 29 80       AND #80  -  mute
  E(0x29);
  E(0x80);
  // F0 08       BEQ 08
  E(0xF0);
  E(0x08);
  // E6 98       INC $98
  E(0xE6);
  E(0x98);
  //             LDA $A4
  E(0xA5);
  E(0xA4);
  //             ORA #02
  E(0x09);
  E(0x02);
  //             STA $A4
  E(0x85);
  E(0xA4);
  //
  // BD 91 E1    LDA $e191,x
  E(0xBD);
  E(0x91);
  E(0xE1);
  // 29 20       AND #20  -  poison
  E(0x29);
  E(0x20);
  // F0 08       BEQ 08
  E(0xF0);
  E(0x08);
  // E6 98       INC $98
  E(0xE6);
  E(0x98);
  //             LDA $A4
  E(0xA5);
  E(0xA4);
  //             ORA #04
  E(0x09);
  E(0x04);
  //             STA $A4
  E(0x85);
  E(0xA4);
  //
  // BD AE E1    LDA $e1ae,x
  E(0xBD);
  E(0xAE);
  E(0xE1);
  // 29 10       AND #10  -  moon energy
  E(0x29);
  E(0xF0);
  // F0 08       BEQ 08
  E(0xF0);
  E(0x08);
  // E6 98       INC $98
  E(0xE6);
  E(0x98);
  //             LDA $A4
  E(0xA5);
  E(0xA4);
  //             ORA #08
  E(0x09);
  E(0x08);
  //             STA $A4
  E(0x85);
  E(0xA4);
  //
  // BD B0 E1    LDA $e1b0,x
  E(0xBD);
  E(0xB0);
  E(0xE1);
  // 29 02       AND #02  -  acid rain
  E(0x29);
  E(0x02);
  // F0 08       BEQ 08
  E(0xF0);
  E(0x08);
  // E6 98       INC $98
  E(0xE6);
  E(0x98);
  //             LDA $A4
  E(0xA5);
  E(0xA4);
  //             ORA #10
  E(0x09);
  E(0x10);
  //             STA $A4
  E(0x85);
  E(0xA4);
  //
  // BD B0 E1    LDA $e1b0,x
  E(0xBD);
  E(0xB0);
  E(0xE1);
  // 29 14       AND #14  -  speedup
  E(0x29);
  E(0x14);
  // F0 08       BEQ 08
  E(0xF0);
  E(0x08);
  // E6 98       INC $98
  E(0xE6);
  E(0x98);
  //             LDA $A4
  E(0xA5);
  E(0xA4);
  //             ORA #20
  E(0x09);
  E(0x20);
  //             STA $A4
  E(0x85);
  E(0xA4);
  //
  // BD B0 E1    LDA $e1b0,x
  E(0xBD);
  E(0xB0);
  E(0xE1);
  // 29 48       AND #48  -  lunar boost
  E(0x29);
  E(0x48);
  // F0 08       BEQ 08
  E(0xF0);
  E(0x08);
  // E6 98       INC $98
  E(0xE6);
  E(0x98);
  //             LDA $A4
  E(0xA5);
  E(0xA4);
  //             ORA #40
  E(0x09);
  E(0x40);
  //             STA $A4
  E(0x85);
  E(0xA4);
  //
  // BD B1 E1    LDA $e1b1,x
  E(0xBD);
  E(0xB1);
  E(0xE1);
  // 29 10       AND #10  -  lucid barrier
  E(0x29);
  E(0x10);
  // F0 08       BEQ 08
  E(0xF0);
  E(0x08);
  // E6 98       INC $98
  E(0xE6);
  E(0x98);
  //             LDA $A4
  E(0xA5);
  E(0xA4);
  //             ORA #80
  E(0x09);
  E(0x80);
  //             STA $A4
  E(0x85);
  E(0xA4);
  //
  // BD B1 E1    LDA $e1b1,x
  E(0xBD);
  E(0xB1);
  E(0xE1);
  // 29 40       AND #40  -  wall
  E(0x29);
  E(0x40);
  // F0 08       BEQ 08
  E(0xF0);
  E(0x08);
  // E6 98       INC $98
  E(0xE6);
  E(0x98);
  //             LDA $A5
  E(0xA5);
  E(0xA5);
  //             ORA #01
  E(0x09);
  E(0x01);
  //             STA $A5
  E(0x85);
  E(0xA5);
  //
  // BD B1 E1    LDA $e1b0,x
  E(0xBD);
  E(0xB0);
  E(0xE1);
  // 29 40       AND #01  -  defender
  E(0x29);
  E(0x01);
  // F0 08       BEQ 08
  E(0xF0);
  E(0x08);
  // E6 98       INC $98
  E(0xE6);
  E(0x98);
  //             LDA $A5
  E(0xA5);
  E(0xA5);
  //             ORA #02
  E(0x09);
  E(0x02);
  //             STA $A5
  E(0x85);
  E(0xA5);
  //
  // PHX
  E(0xDA);
  // TYX
  E(0xBB);
  // LDA $98
  E(0xA5);
  E(0x98);
  // BNE over
  E(0xD0);
  E(0x13);

  // ////////////////////
  // LDA 7ECF2A,x
  E(0xBF);
  E((uint8_t)SG_BYTE_1);
  E((uint8_t)(SG_BYTE_1 >> 8));
  E((uint8_t)(SG_BYTE_1 >> 16));
  // BEQ over (4)
  E(0xF0);
  E(0x04);

  // JSL clear palette
  E(0x22);
  E((uint8_t)resetPaletteSubr);
  E((uint8_t)(resetPaletteSubr >> 8));
  E((uint8_t)((resetPaletteSubr >> 16) + 0xC0));

  // ////////////////////

  // STZ 7ECF2A,x
  E(0xA9);
  E(0x00);

  E(0x9F);
  E((uint8_t)SG_BYTE_1);
  E((uint8_t)(SG_BYTE_1 >> 8));
  E((uint8_t)(SG_BYTE_1 >> 16));


  // PLX
  E(0xFA);
  // PLY
  E(0x7A);

  // ////////////////////////////

  // RTL
  E(0x6B);
  //
  // over:
  //
  // LDA 7ECF2A,x
  E(0xBF);
  E((uint8_t)SG_BYTE_1);
  E((uint8_t)(SG_BYTE_1 >> 8));
  E((uint8_t)(SG_BYTE_1 >> 16));

  // CMP $98
  E(0xC5);
  E(0x98);
  // BNE over2 -> BLT
  E(0x90);
  E(0x02);
  // LDA #00
  E(0xA9);
  E(0x00);
  // over2:
  // INC A
  E(0x1A);
  // STA 7ECF2A,x
  E(0x9F);
  E((uint8_t)SG_BYTE_1);
  E((uint8_t)(SG_BYTE_1 >> 8));
  E((uint8_t)(SG_BYTE_1 >> 16));


  //
  // now pop bits off of $A4 and $A5 until we hit $98, and the bit index we put .. somewhere else as the palette we pull in the nonzero block
  // LDY #00
  E(0xA0);
  E(0x00);
  E(0x00);

  // ////////////////////
  // STZ $98
  E(0x64);
  E(0x98);
  // REP 20
  E(0xC2);
  E(0x20);
  //
  // loop:
  // LDA $A4
  E(0xA5);  /////////////////////
  E(0xA4);  /////////////////////
  // INY
  E(0xC8);
  // LSR - note the last bit goes into carry here
  E(0x4A);
  // STA $A4
  E(0x85);  //////////////////////
  E(0xA4);  //////////////////////
  // BCC skip
  E(0x90);
  E(0x04 + 0x07);  ////////
  // SEP 20
  E(0xE2);
  E(0x20);
  // INC $99
  E(0xE6);
  E(0x99);
  // ///////////////////////////
  // CMP #01
  E(0xC9);
  E(0x01);
  // BNE skip - need to change other skip jump too
  E(0xD0);
  E(0x03);
  // TYA
  E(0x98);
  // STA $98
  E(0x85);
  E(0x98);
  // ///////////////////////////

  //
  // skip:
  // REP 20
  E(0xC2);
  E(0x20);
  // SEP 20
  E(0xE2);
  E(0x20);
  // LDA $99
  E(0xA5);
  E(0x99);
  // BEQ overcheck [6] - no $99 yet?  don't break out with 0
  E(0xF0);
  E(0x06);
  // CMP 7ECF2D,x -> 2A
  E(0xDF);
  E((uint8_t)SG_BYTE_1);
  E((uint8_t)(SG_BYTE_1 >> 8));
  E((uint8_t)(SG_BYTE_1 >> 16));

  // BEQ out - BGE instead
  E(0xB0);
  E(0x0A + 0x07);
  //
  // CPY #000A - number of statuses
  E(0xC0);
  E(0x0A);
  E(0x00);
  // BEQ loopEnd
  E(0xF0);
  E(0x02);
  // BRA loop
  E(0x80);
  E(0xD6);
  //
  // loopEnd:
  // REP 20
  E(0xC2);
  E(0x20);
  // LDA $98
  E(0xA5);
  E(0x98);
  // AND #00FF
  E(0x29);
  E(0xFF);
  E(0x00);
  // TAY
  E(0xA8);
  // SEP 20
  E(0xE2);
  E(0x20);

  // out:
  // TYA
  E(0x98);
  // DEC
  E(0x3A);
  // STA 7ECF2D,x
  E(0x9F);
  E((uint8_t)SG_BYTE_2);
  E((uint8_t)(SG_BYTE_2 >> 8));
  E((uint8_t)(SG_BYTE_2 >> 16));

  // PLX
  E(0xFA);
  // PLY
  E(0x7A);
  // RTL
  E(0x6B);



  int nonZeroSubAddr = c->working_offset;
  // ////////////////////////
  // timerNonZeroSub:
  // ////////////////////////
  //             PHX
  E(0xDA);
  //             REP 20  - temporary 16 bit a
  E(0xC2);
  E(0x20);
  //             TXA
  E(0x8A);
  //             XBA
  E(0xEB);
  //             LSR
  E(0x4A);
  //             TAX
  E(0xAA);
  //             SEP 20  - 8 bit a
  E(0xE2);
  E(0x20);
  //             LDA 7ECF2A,x
  E(0xBF);
  E((uint8_t)SG_BYTE_1);
  E((uint8_t)(SG_BYTE_1 >> 8));
  E((uint8_t)(SG_BYTE_1 >> 16));

  //             BNE over
  E(0xD0);
  E(0x02);
  //             PLX
  E(0xFA);
  //             RTL
  E(0x6B);
  // over:
  //             PLX
  E(0xFA);
  // BD 90 E1    LDA $e190,x
  E(0xBD);
  E(0x90);
  E(0xE1);
  // 29 20       AND #20  -  snowman
  E(0x29);
  E(0x20);
  //             BNE skip
  E(0xD0);
  E(0x10);
  // BD 90 E1    LDA $e190,x
  E(0xBD);
  E(0x90);
  E(0xE1);
  // 29 40       AND #40  -  petrify
  E(0x29);
  E(0x40);
  //             BNE skip
  E(0xD0);
  E(0x09);
  // BD 91 E1    LDA $e191,x
  E(0xBD);
  E(0x91);
  E(0xE1);
  // 29 80       AND #80  -  dead
  E(0x29);
  E(0x80);
  //             BNE skip
  E(0xD0);
  E(0x02);
  //             BRA loadPalette
  E(0x80);
  E(0x01);
  // skip:
  // 6B          RTL
  E(0x6B);
  // loadPalette:
  // - so here, the palette i want is ...
  // - 7ECF2D/E/F * 16 * 3 * [0x20] + charNum * 16 * [0x20] + ((counter&1F)>>2) * x20 (palette size)
  // - so palettes should be arranged:
  // - boy slow palette 1 [0x0000]
  // - boy slow palette 2 [0x0020]
  // ...
  // - boy slow palette 16 [0x01E0]
  // - girl slow palette 1 [0x0200]
  // ..
  // - sprite slow palettes [0x0400]
  // - mute palettes [0x0600]
  // - poison palettes
  // .. lucid barrier palettes [0x3000]
  // - total palettes: # statuses (9) * num chars (3) * num palettes each (64) = 1728, 55296 bytes
  // - maybe we'll cut the palettes by 4 by dividing the counter by 4, so it's 16 each. 13824 bytes
  // - index on paletteLocationStart
  //
  //             PHY
  E(0x5A);
  //             PHX
  E(0xDA);
  //             REP 20  - temporary 16 bit a
  E(0xC2);
  E(0x20);
  //             TXA
  E(0x8A);
  //             XBA
  E(0xEB);
  //             LSR
  E(0x4A);
  //             TAX
  E(0xAA);
  //             ASL
  E(0x0A);
  //             ASL
  E(0x0A);
  //             ASL
  E(0x0A);
  //             ASL
  E(0x0A);
  //             ASL - multiply by 32 for palette output index
  E(0x0A);
  //             TAY
  E(0xA8);
  //             LDA #0000
  E(0xA9);
  E(0x00);
  E(0x00);
  //             SEP 20
  E(0xE2);
  E(0x20);
  //             LDA 7ECF2D,x
  E(0xBF);
  E((uint8_t)SG_BYTE_2);
  E((uint8_t)(SG_BYTE_2 >> 8));
  E((uint8_t)(SG_BYTE_2 >> 16));

  //             ASL
  E(0x0A);
  //             CLC
  E(0x18);
  //             ADC 7ECF2D,x - multiply by 3
  E(0x7F);
  E((uint8_t)SG_BYTE_2);
  E((uint8_t)(SG_BYTE_2 >> 8));
  E((uint8_t)(SG_BYTE_2 >> 16));

  //             REP 20
  E(0xC2);
  E(0x20);
  // -             ASL
  // -             ASL
  //             ASL
  E(0x0A);
  //             ASL
  E(0x0A);
  //             ASL
  E(0x0A);
  //             ASL - multiply by 64 -> 16
  E(0x0A);

  // ///////////////////// add multiply by 0x20 for palette size
  E(0x0A);
  E(0x0A);
  E(0x0A);
  E(0x0A);
  E(0x0A);
  // /////////////////////

  //             PHA
  E(0x48);
  //             TXA - char number
  E(0x8A);
  // -             ASL
  // -             ASL
  //             ASL
  E(0x0A);
  //             ASL
  E(0x0A);
  //             ASL
  E(0x0A);
  //             ASL - multiply by 64 -> 16
  E(0x0A);

  // ///////////////////// add multiply by 0x20 for palette size
  E(0x0A);
  E(0x0A);
  E(0x0A);
  E(0x0A);
  E(0x0A);
  // /////////////////////

  //             CLC
  E(0x18);
  //             ADC $02,s
  E(0x63);
  E(0x01);
  //             STA $02,s - add to what we pushed before
  E(0x83);
  E(0x01);
  //             PLA - so we can get to our saved x
  E(0x68);
  //             PLX - 0x000, 0x200, 0x400
  E(0xFA);
  //             PHA - back in it goes
  E(0x48);
  // BD B9 E1    LDA E1B9, x - load timer
  E(0xBD);
  E(0xB9);
  E(0xE1);
  //             AND #003F - since we're 16 bit, and repeating.
  E(0x29);
  E(0x0F);  // 0x3F;
  E(0x00);
  //             ASL
  E(0x0A);
  //             ASL
  E(0x0A);
  //             ASL
  E(0x0A);
  //             ASL
  E(0x0A);
  //             ASL       - multiply by 32, our palette size
  E(0x0A);
  //             CLC
  E(0x18);
  //             ADC $02,s
  E(0x63);
  E(0x01);
  //             STA $02,s - add to what we pushed before
  E(0x83);
  E(0x01);
  //             PLA
  E(0x68);
  //             PHX - push x back in
  E(0xDA);
  //             TAX - use this thing as a big ass index
  E(0xAA);
  // - now dump the palette in both spots .. we don't really have a loop counter available, so fuck it, just dump 16 of these in
  // - 7EDF20, 7E0720
  for (int i = 0; i < 16; i++)
  {
  int srcAddr = palette_location_start + i * 2;
  int dest1 = 0xDF20 + i * 2;
  int dest2 = 0x0720 + i * 2;
  // {
  //             LDA paletteLocationStart,x (24 bit, from rom)
  E(0xBF);
  E((uint8_t)srcAddr);
  E((uint8_t)(srcAddr >> 8));
  E((uint8_t)((srcAddr >> 16) + 0xC0));
  //             STA DF20,y
  E(0x99);
  E((uint8_t)dest1);
  E((uint8_t)(dest1>>8));
  //             STA 0720,y
  E(0x99);
  E((uint8_t)dest2);
  E((uint8_t)(dest2 >> 8));
  // } - 16 of these; increment all 3 addresses by 2 each loop
  }

  //             SEP 20
  E(0xE2);
  E(0x20);
  // -             PLX
  E(0xFA);
  //             PLY
  E(0x7A);

  // 6B          RTL
  E(0x6B);


  P(0x3B25, 0x22);
  P(0x3B26, (uint8_t)c->working_offset);
  P(0x3B27, (uint8_t)(c->working_offset >> 8));
  P(0x3B28, (uint8_t)((c->working_offset >> 16) + 0xC0));

  // ////////////////////////
  // main subr:
  // ////////////////////////
  // 22 7B 4F C0 JSL $C04F7B - replaced code
  E(0x22);
  E(0x7B);
  E(0x4F);
  E(0xC0);

  // //////////////////
  // CPX #0600
  E(0xE0);
  E(0x00);
  E(0x06);
  // BGE over
  E(0xB0);
  E(0x0F);
  // //////////////////
  // BD B9 E1    LDA E1B9, x - load timer
  E(0xBD);
  E(0xB9);
  E(0xE1);

  //             AND #3F
  E(0x29);
  E(0x0F);

  //             BNE nonzero
  E(0xD0);
  E(0x04);
  // zero:
  // 22 xx xx xx JSL timerZeroSub
  E(0x22);
  E((uint8_t)zeroSubAddr);
  E((uint8_t)(zeroSubAddr >> 8));
  E((uint8_t)((zeroSubAddr >> 16) + 0xC0));
  // nonzero:
  // 22 xx xx xx JSL timerNonZeroSub
  E(0x22);
  E((uint8_t)nonZeroSubAddr);
  E((uint8_t)(nonZeroSubAddr >> 8));
  E((uint8_t)((nonZeroSubAddr >> 16) + 0xC0));

  // over: (enemies)
  // 6B          RTL
  E(0x6B);

  // for these, compare x to x600 .. if less, call the zeroSubAddr

  // triggers to update the thingy right away:
  // [wall]
  // C8 / ED5B:	0940    	ORA #$40
  // C8 / ED5D:	9DB1E1 STA $E1B1,X
  P(0x8ED5B, 0x22);
  P(0x8ED5C, (uint8_t)c->working_offset);
  P(0x8ED5D, (uint8_t)(c->working_offset >> 8));
  P(0x8ED5E, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x8ED5F, 0xEA);
  E(0x09);
  E(0x40);
  E(0x9D);
  E(0xB1);
  E(0xE1);

  // cpx #600
  E(0xE0);
  E(0x00);
  E(0x06);
  // bge 04
  E(0xB0);
  E(0x04);

  E(0x22);
  E((uint8_t)zeroSubAddr);
  E((uint8_t)(zeroSubAddr >> 8));
  E((uint8_t)((zeroSubAddr >> 16) + 0xC0));
  E(0x6B);

  // [lucid barrier]
  // C8 / EEC3:	0910    	ORA #$10
  // C8 / EEC5:	9DB1E1 STA $E1B1,X
  P(0x8EEC3, 0x22);
  P(0x8EEC4, (uint8_t)c->working_offset);
  P(0x8EEC5, (uint8_t)(c->working_offset >> 8));
  P(0x8EEC6, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x8EEC7, 0xEA);
  E(0x09);
  E(0x10);
  E(0x9D);
  E(0xB1);
  E(0xE1);

  // cpx #600
  E(0xE0);
  E(0x00);
  E(0x06);
  // bge 04
  E(0xB0);
  E(0x04);

  E(0x22);
  E((uint8_t)zeroSubAddr);
  E((uint8_t)(zeroSubAddr >> 8));
  E((uint8_t)((zeroSubAddr >> 16) + 0xC0));
  E(0x6B);

  // [slow, other statuses? if non zero, run the thing]
  // $C8 / E31C 25 A4       AND $A4    [$00:03A4]   A:0004 X:0000 Y:0400 P:envmxdIzC
  // $C8 / E31E 9D 90 E1 STA $E190,x[$7E:E190]   A: 0004 X: 0000 Y: 0400 P: envmxdIzC
  // 98, a4 (16bit) need to be preserved?
  P(0x8E31C, 0x22);
  P(0x8E31D, (uint8_t)c->working_offset);
  P(0x8E31E, (uint8_t)(c->working_offset >> 8));
  P(0x8E31F, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x8E320, 0xEA);
  E(0x25);
  E(0xA4);

  // ////////////////// only do this stuff if the status changed
  // CMP $E190,X
  E(0xDD);
  E(0x90);
  E(0xE1);
  // BNE continue (01)
  E(0xD0);
  E(0x01);
  // RTL
  E(0x6B);
  // //////////////////

  // continue:
  E(0x9D);
  E(0x90);
  E(0xE1);

  // cpx #600
  E(0xE0);
  E(0x00);
  E(0x06);
  // bge 04
  E(0xB0);
  E(0x12);

  // /////////////////
  // lda $98
  E(0xA5);
  E(0x98);
  // pha
  E(0x48);
  // lda $a4
  E(0xA5);
  E(0xA4);
  // pha
  E(0x48);
  // /////////////////

  E(0x22);
  E((uint8_t)zeroSubAddr);
  E((uint8_t)(zeroSubAddr >> 8));
  E((uint8_t)((zeroSubAddr >> 16) + 0xC0));
  // /////////////////
  E(0xC2);
  E(0x20);
  // pla
  E(0x68);
  // sta $a4
  E(0x85);
  E(0xA4);
  // pla
  E(0x68);
  // sta $98
  E(0x85);
  E(0x98);
  // /////////////////

  E(0x6B);


  // [acid storm]
  // C8 / EB49:	0902    	ORA #$02		[Set Acid Storm]
  // C8 / EB4B:	9DB0E1 STA $E1B0,X[Store into Stat Buffs]
  P(0x8EB49, 0x22);
  P(0x8EB4A, (uint8_t)c->working_offset);
  P(0x8EB4B, (uint8_t)(c->working_offset >> 8));
  P(0x8EB4C, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x8EB4D, 0xEA);
  int freezingRainEnabled = 1;

  if (christmas_mode && freezingRainEnabled)
  {
  E(0x09);
  E(0x02);
  E(0x9D);
  E(0xB0);
  E(0xE1);

  // slow
  E(0xBD);
  E(0x90);
  E(0xE1);

  E(0x09);
  E(0x04);
  E(0x9D);
  E(0x90);
  E(0xE1);
  }
  else
  {
  E(0x09);
  E(0x02);
  E(0x9D);
  E(0xB0);
  E(0xE1);
  }
  // cpx #600
  E(0xE0);
  E(0x00);
  E(0x06);
  // bge 04
  E(0xB0);
  E(0x04);

  E(0x22);
  E((uint8_t)zeroSubAddr);
  E((uint8_t)(zeroSubAddr >> 8));
  E((uint8_t)((zeroSubAddr >> 16) + 0xC0));
  E(0x6B);

  // [speedup]
  // C8/EB12:	0914    	ORA #$14
  // C8 / EB14:	9DB0E1 STA $E1B0,X
  P(0x8EB12, 0x22);
  P(0x8EB13, (uint8_t)c->working_offset);
  P(0x8EB14, (uint8_t)(c->working_offset >> 8));
  P(0x8EB15, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x8EB16, 0xEA);
  E(0x09);
  E(0x14);
  E(0x9D);
  E(0xB0);
  E(0xE1);

  // cpx #600
  E(0xE0);
  E(0x00);
  E(0x06);
  // bge 04
  E(0xB0);
  E(0x04);

  E(0x22);
  E((uint8_t)zeroSubAddr);
  E((uint8_t)(zeroSubAddr >> 8));
  E((uint8_t)((zeroSubAddr >> 16) + 0xC0));
  E(0x6B);

  // [lunar boost]
  // C8/ECC3:	0948    	ORA #$48
  // C8 / ECC5:	9DB0E1 STA $E1B0,X
  P(0x8ECC3, 0x22);
  P(0x8ECC4, (uint8_t)c->working_offset);
  P(0x8ECC5, (uint8_t)(c->working_offset >> 8));
  P(0x8ECC6, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x8ECC7, 0xEA);
  E(0x09);
  E(0x48);
  E(0x9D);
  E(0xB0);
  E(0xE1);

  // cpx #600
  E(0xE0);
  E(0x00);
  E(0x06);
  // bge 04
  E(0xB0);
  E(0x04);

  E(0x22);
  E((uint8_t)zeroSubAddr);
  E((uint8_t)(zeroSubAddr >> 8));
  E((uint8_t)((zeroSubAddr >> 16) + 0xC0));
  E(0x6B);

  // [moon energy]
  // C8/ECE4:	05A4    	ORA $A4
  // C8 / ECE6:	9DAEE1 STA $E1AE,X
  P(0x8ECE4, 0x22);
  P(0x8ECE5, (uint8_t)c->working_offset);
  P(0x8ECE6, (uint8_t)(c->working_offset >> 8));
  P(0x8ECE7, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x8ECE8, 0xEA);
  E(0x05);
  E(0xA4);
  E(0x9D);
  E(0xAE);
  E(0xE1);

  // cpx #600
  E(0xE0);
  E(0x00);
  E(0x06);
  // bge 04
  E(0xB0);
  E(0x04);

  E(0x22);
  E((uint8_t)zeroSubAddr);
  E((uint8_t)(zeroSubAddr >> 8));
  E((uint8_t)((zeroSubAddr >> 16) + 0xC0));
  E(0x6B);

  // [defender]
  // C8/EB2E:	0901    	ORA #$01
  // C8 / EB30:	9DB0E1 STA $E1B0,X
  P(0x8EB2E, 0x22);
  P(0x8EB2F, (uint8_t)c->working_offset);
  P(0x8EB30, (uint8_t)(c->working_offset >> 8));
  P(0x8EB31, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x8EB32, 0xEA);
  E(0x09);
  E(0x01);
  E(0x9D);
  E(0xB0);
  E(0xE1);

  // cpx #600
  E(0xE0);
  E(0x00);
  E(0x06);
  // bge 04
  E(0xB0);
  E(0x04);

  E(0x22);
  E((uint8_t)zeroSubAddr);
  E((uint8_t)(zeroSubAddr >> 8));
  E((uint8_t)((zeroSubAddr >> 16) + 0xC0));
  E(0x6B);

  // menu opening and closing
  // $C0/692F A9 01 00    LDA #$0001              A:0000 X:0000 Y:0000 P:envmxdIZC
  // $C0/6932 8D 26 18    STA $1826  [$00:1826]   A:0001 X:0000 Y:0000 P:envmxdIzC
  // -> - 7EDF20, 7E0720
  P(0x692F, 0x22);
  P(0x6930, (uint8_t)c->working_offset);
  P(0x6931, (uint8_t)(c->working_offset >> 8));
  P(0x6932, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x6933, 0xEA);
  P(0x6934, 0xEA);

  // replace with:
  //
  // PHX
  E(0xDA);
  E(0x08);
  E(0xC2);
  E(0x10);
  // LDX #0000
  E(0xA2);
  E(0x00);
  E(0x00);
  // loop1:
  // LDA C81EFE,X - boy palette
  E(0xBF);
  E(0xFE);
  E(0x1E);
  E(0xC8);
  // STA $DF20,x
  E(0x9D);
  E(0x20);
  E(0xDF);
  // STA $0720,x
  E(0x9D);
  E(0x20);
  E(0x07);
  // INX
  E(0xE8);
  // INX
  E(0xE8);
  // CPX #0020
  E(0xE0);
  E(0x20);
  E(0x00);
  // BLT loop1
  E(0x90);
  E(0xEF);
  //
  // LDX #0000
  E(0xA2);
  E(0x00);
  E(0x00);
  // loop2:
  // LDA C81F1C,X - girl palette
  E(0xBF);
  E(0x1C);
  E(0x1F);
  E(0xC8);
  // STA $DF40,x
  E(0x9D);
  E(0x40);
  E(0xDF);
  // STA $0740,x
  E(0x9D);
  E(0x40);
  E(0x07);
  // INX
  E(0xE8);
  // INX
  E(0xE8);
  // CPX #0020
  E(0xE0);
  E(0x20);
  E(0x00);
  // BLT loop2
  E(0x90);
  E(0xEF);
  //
  // LDX #0000
  E(0xA2);
  E(0x00);
  E(0x00);
  // loop3:
  // LDA C81F3A,X - sprite palette
  E(0xBF);
  E(0x3A);
  E(0x1F);
  E(0xC8);
  // STA $DF60,x
  E(0x9D);
  E(0x60);
  E(0xDF);
  // STA $0760,x
  E(0x9D);
  E(0x60);
  E(0x07);
  // INX
  E(0xE8);
  // INX
  E(0xE8);
  // CPX #0020
  E(0xE0);
  E(0x20);
  E(0x00);
  // BLT loop3
  E(0x90);
  E(0xEF);

  // new: plp
  E(0x28);
  // PLX
  E(0xFA);
  // A9 01 00 - original code
  E(0xA9);
  E(0x01);
  E(0x00);
  // 8D 26 18 - original code
  E(0x8D);
  E(0x26);
  E(0x18);
  // RTL
  E(0x6B);


  // menu close call not needed


  int subr1D90Dcopy = c->working_offset;
  for(int srcAddr = 0x1D90D; srcAddr <= 0x1D941; srcAddr++)
  {
  int orig = sr_peek(c, srcAddr);
  if(orig == 0x60)
  {
  // convert rts to rtl
  orig = 0x6B;
  }
  E(orig);
  }
  // energy recharge glow removal
  // $01/D934 99 00 07    STA $0700,y[$7E:0722]   A:14A5 X:0000 Y:0022 P:envmxdIzc
  // $01/D937 E6 0D       INC $0D    [$00:000D] A:14A5 X:0000 Y:0022 P:envmxdIzc
  // also:
  // $00/ED98 99 00 07    STA $0700,y[$7E:0722]   A:14A5

  // $00/EB68 BD 00 E1    LDA $E100,x[$7E:E100]   A:7389 X:0000 Y:0000 P:eNvMxdIzC
  // $00 / EB6B 10 03       BPL $03[$EB70]      A: 7389 X: 0000 Y: 0000 P: eNvMxdIzC
  // $00/EB6D 20 08 ED    JSR $ED08  [$00:ED08]   A:7389 X:0000 Y:0000 P:eNvMxdIzC
  // $00/EB70 BD 04 E1    LDA $E104,x[$7E:E104]   A:7FFF X:0000 Y:0040 P:envMxdIZc

  // $01/CA44 09 88       ORA #$88                A:7301 X:0000 Y:0000 P:envMxdIzc
  // $01 / CA46 9D 00 E1 STA $E100,x[$7E:E100]   A: 7389 X: 0000 Y: 0000 P: eNvMxdIzc
  // $01/CA49 20 0D D9    JSR $D90D  [$01:D90D]   A:7389 X:0000 Y:0000 P:eNvMxdIzc
  // replace this.
  P(0x1CA44, 0x22);
  P(0x1CA45, (uint8_t)c->working_offset);
  P(0x1CA46, (uint8_t)(c->working_offset >> 8));
  P(0x1CA47, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x1CA48, 0xEA);
  P(0x1CA49, 0xEA);
  P(0x1CA4A, 0xEA);
  P(0x1CA4B, 0xEA);

  // REP 20
  E(0xC2);
  E(0x20);
  // PHA
  E(0x48);
  // PHX
  E(0xDA);

  // CPX #0600
  E(0xE0);
  E(0x00);
  E(0x06);
  // BLT skipXCheck (e)
  E(0x90);
  E(0x0E);
  // (enemy .. do default action)
  // PLX
  E(0xFA);
  // PLA
  E(0x68);
  // SEP 20
  E(0xE2);
  E(0x20);

  // replaced code
  E(0x09);
  E(0x88);

  E(0x9D);
  E(0x00);
  E(0xE1);
  // replacement subr
  E(0x22);
  E((uint8_t)subr1D90Dcopy);
  E((uint8_t)(subr1D90Dcopy >> 8));
  E((uint8_t)((subr1D90Dcopy >> 16) + 0xC0));

  // RTL
  E(0x6B);
  // skipXCheck:

  // TXA
  E(0x8A);
  // XBA
  E(0xEB);
  // LSR
  E(0x4A);
  // TAX
  E(0xAA);
  // SEP 20
  E(0xE2);
  E(0x20);
  // LDA 7ECF2A,x
  E(0xBF);
  E((uint8_t)SG_BYTE_1);
  E((uint8_t)(SG_BYTE_1 >> 8));
  E((uint8_t)(SG_BYTE_1 >> 16));

  // REP 20
  E(0xC2);
  E(0x20);

  // BNE skip
  E(0xD0);
  E(0x0E);
  // (palette not active)
  // PLX
  E(0xFA);
  // PLA
  E(0x68);
  // SEP 20
  E(0xE2);
  E(0x20);

  // replaced code
  E(0x09);
  E(0x88);

  E(0x9D);
  E(0x00);
  E(0xE1);
  // replacement subr
  E(0x22);
  E((uint8_t)subr1D90Dcopy);
  E((uint8_t)(subr1D90Dcopy >> 8));
  E((uint8_t)((subr1D90Dcopy >> 16) + 0xC0));

  // RTL
  E(0x6B);


  // skip: (palette active)
  // PLX
  E(0xFA);
  // PLA
  E(0x68);
  // SEP 20
  E(0xE2);
  E(0x20);
  // RTL
  E(0x6B);

  return 1;
}
