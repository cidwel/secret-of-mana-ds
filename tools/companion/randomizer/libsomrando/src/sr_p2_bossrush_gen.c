/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Boss rush mode (group w8): the machine-code generating hacks.  Derived from the C# sources named at each function (Author of every
 * upstream file: Moppleton), translated to C 2026-10-18.  Comments are those of the C#.
 */
#include <math.h>
#include <string.h>

#include "sr_core.h"
#include "sr_p2_bossrush.h"
#include "sr_p2_bossrush_int.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))
#define W8_FLAG_TIMER_RUNNING 0x27
#define W8_RAM_STATISTICS_BASE_OFFSET 0x7E0400
#define W8_RAM_PLAYER_KILLS_OFFSET_16BIT (W8_RAM_STATISTICS_BASE_OFFSET + 0)
#define W8_RAM_ENEMY_KILLS_OFFSET_16BIT (W8_RAM_STATISTICS_BASE_OFFSET + 2)
#define W8_RAM_NUM_CHESTS_OFFSET_16BIT (W8_RAM_STATISTICS_BASE_OFFSET + 4)
#define W8_RAM_MOVE_DIST_32BIT (W8_RAM_STATISTICS_BASE_OFFSET + 6)
#define W8_RAM_TOTAL_DAMAGE_DEALT_32BIT (W8_RAM_STATISTICS_BASE_OFFSET + 10)
#define W8_RAM_TOTAL_P_DAMAGE_DEALT_32BIT (W8_RAM_STATISTICS_BASE_OFFSET + 14)
#define W8_RAM_TOTAL_M_DAMAGE_DEALT_32BIT (W8_RAM_STATISTICS_BASE_OFFSET + 18)
#define W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT (W8_RAM_STATISTICS_BASE_OFFSET + 22)
#define W8_RAM_TOTAL_P_DAMAGE_TAKEN_32BIT (W8_RAM_STATISTICS_BASE_OFFSET + 26)
#define W8_RAM_TOTAL_M_DAMAGE_TAKEN_32BIT (W8_RAM_STATISTICS_BASE_OFFSET + 30)
#define BYTES(...) (const uint8_t[]){__VA_ARGS__}, (int)sizeof((const uint8_t[]){__VA_ARGS__})


/* upstream hacks/common/procgen/DoorExpansion.cs */

int sr_w8_hack_door_expansion(struct sr_ctx *c) {
            const int numFloors = w9_num_floors(c, W8_NUM_BOSS_ENTRIES);

            // move the doors for procgen modes
            sr_work_set_int(c, W8_KEY_DOOR_LOCATION, 0x68000);

            // doors from 83xxx to 68xxx+
            int startDoorsOffset = c->working_offset;
            sr_work_set_int(c, W8_KEY_START_DOOR_OFFSET, startDoorsOffset);
            int startDoorsRomOffset = startDoorsOffset + 0xC00000;
            c->working_offset += numFloors * 2; // 16bit door, 16bit intro event

            int startMusicOffset = c->working_offset;
            sr_work_set_int(c, W8_KEY_START_MUSIC_OFFSET, startMusicOffset);
            int startMusicRomOffset = startMusicOffset + 0xC00000;
            c->working_offset += numFloors * 4;

            int startMusicRomOffsetP1 = startMusicRomOffset + 1;
            int startMusicRomOffsetP2 = startMusicRomOffset + 2;

            // first copy the existing doors
            for (int i = 0; i < 0x1000; i++)
            {
                P(0x68000 + i, c->out[0x83000 + i]);
            }

            // remove this BCS: i actually don't know what trigger IDs > 0xC00 do - i think nothing, so we don't support them anymore here
            // $01/E7C9 C9 00 0C    CMP #$0C00          A:080B X:0000 Y:0028 P:envmxdIzC
            // $01/E7CC B0 35       BCS $35[$E803]      A:080B X:0000 Y:0028 P:eNvmxdIzc
            P(0x1E7CC, 0xEA);
            P(0x1E7CD, 0xEA);

            // replace this block with a custom handler for the door value
            // $01/E7D6 29 FF 03     AND #$03FF            A:080B X:FFFE Y:0090 P:envmxdIzC
            // $01/E7D9 8D 0E 01     STA $010E[$7E:010E]   A:000B X:FFFE Y:0090 P:envmxdIzC
            P(0x1E7D6, 0x22);
            P(0x1E7D7, (uint8_t)(c->working_offset));
            P(0x1E7D8, (uint8_t)(c->working_offset >> 8));
            P(0x1E7D9, (uint8_t)((c->working_offset >> 16) + 0xC0));
            P(0x1E7DA, 0xEA);
            P(0x1E7DB, 0xEA);

            // special-case door 0xFFFF, and load the start door for the current floor instead of doing the sbc below

            // C9 FF FF      CMP FFFF
            E(0xC9);
            E(0xFF);
            E(0xFF);
            // D0 16         BNE past this shit
            E(0xD0);
            E(0x44);
            // AF DC 00 7E   LDA 7E00DC ; load mapnum
            E(0xAF);
            E(0xDC);
            E(0x00);
            E(0x7E);

            // AND 00FF
            E(0x29);
            E(0xFF);
            E(0x00);

            // LSR
            E(0x4A);

            // specialcase manabeast map - FD/2 = 7E - load numMaps-1 instead
            // CMP #007E
            E(0xC9);
            E(0x7E);
            E(0x00);

            // BNE 03
            E(0xD0);
            E(0x03);

            // LDA #numMaps-1
            E(0xA9);
            E((uint8_t)(numFloors - 1));
            E(0x00);

            // ASL: mul by two for 16 bit offset
            E(0x0A);
            // then two again because we have 4 bytes now
            E(0x0A);
            // PHX
            E(0xDA);
            // TAX
            E(0xAA);

            // first, play the song
            // SEP 20
            E(0xE2);
            E(0x20);

            // LDA #01
            E(0xA9);
            E(0x01);

            // STA 7E1E00
            E(0x8F);
            E(0x00);
            E(0x1E);
            E(0x7E);

            // LDA songloc, x
            E(0xBF);
            E((uint8_t)startMusicRomOffset);
            E((uint8_t)(startMusicRomOffset >> 8));
            E((uint8_t)(startMusicRomOffset >> 16));
            // STA 7E1E01
            E(0x8F);
            E(0x01);
            E(0x1E);
            E(0x7E);
            // LDA songlocp1, x
            E(0xBF);
            E((uint8_t)startMusicRomOffsetP1);
            E((uint8_t)(startMusicRomOffsetP1 >> 8));
            E((uint8_t)(startMusicRomOffsetP1 >> 16));
            // STA 7E1E02
            E(0x8F);
            E(0x02);
            E(0x1E);
            E(0x7E);
            // LDA songlocp2, x
            E(0xBF);
            E((uint8_t)startMusicRomOffsetP2);
            E((uint8_t)(startMusicRomOffsetP2 >> 8));
            E((uint8_t)(startMusicRomOffsetP2 >> 16));
            // STA 7E1E03
            E(0x8F);
            E(0x03);
            E(0x1E);
            E(0x7E);
            // 22 04 00 C3 JSL $C30004 ; play song subr
            E(0x22);
            E(0x04);
            E(0x00);
            E(0xC3);

            // REP 20
            E(0xC2);
            E(0x20);

            // TXA
            E(0x8A);
            // LSR
            E(0x4A);
            // TAX
            E(0xAA);

            // BF xx xx xx   LDA startDoorsOffset,x
            E(0xBF);
            E((uint8_t)startDoorsRomOffset);
            E((uint8_t)(startDoorsRomOffset>>8));
            E((uint8_t)(startDoorsRomOffset>>16));

            // PLX
            E(0xFA);
            // BRA to the STA
            E(0x80);
            E(0x04);

            // SEC
            E(0x38);

            // SBC 0x800
            E(0xE9);
            E(0x00);
            E(0x08);

            // STA from original block
            E(0x8D);
            E(0x0E);
            E(0x01);

            // RTL
            E(0x6B);

            // change the rom load addrs: rom offset 0x68000 (C6:8000)
            P(0x1E7E1, 0x80);
            P(0x1E7E2, 0xC6);

            P(0x1E7EA, 0x80);
            P(0x1E7EB, 0xC6);

            P(0x1E7F4, 0x80);
            P(0x1E7F5, 0xC6);

            // now, handle this with event code 0x18 (formerly 19, 1a, 1b also, these will no longer be used)
            // instead of a single byte, this will now take two, and we'll execute that door.

            // replace this block, which loads the 0x18 xx, 0x19 xx, etc and extracts the doorway by &ing it with 0x3FF and adding 0x800:
            // $01/EB09 E6 D1 INC $D1[$00:00D1] A:BC30 X:0030 Y:00FE P:envmXdIzc
            // $01/EB0B E6 D1 INC $D1[$00:00D1] A:BC30 X:0030 Y:00FE P:envmXdIzc
            // $01/EB0D A5 04       LDA $04    [$00:0004]
            // $01/EB0F EB          XBA A:BC18 X:0030 Y:00FE P:eNvmXdIzc
            // $01/EB10 29 FF 03    AND #$03FF              A:18BC X:0030 Y:00FE P:eNvmXdIzc
            // $01/EB13 09 00 08    ORA #$0800              A:00BC X:0030 Y:00FE P:envmXdIzc

            // change to:
            // E6 D1  INC $D1
            // A7 D1  LDA [$D1]
            // E6 D1  INC $D1
            // E6 D1  INC $D1
            // starting at 0x1EB09 (13 src bytes -> 8)
            // doors in events will now have to be [18 BC 08] for mana beast door 0xBC for example

            // skip ahead to the 16-bit door value parameter
            // INC $D1
            P(0x1EB09, 0xE6);
            P(0x1EB0A, 0xD1);

            // load the 16-bit door value
            // LDA [$D1]
            P(0x1EB0B, 0xA7);
            P(0x1EB0C, 0xD1);

            // skip the door value for the next event command
            // INC $D1
            P(0x1EB0D, 0xE6);
            P(0x1EB0E, 0xD1);

            // INC $D1
            P(0x1EB0F, 0xE6);
            P(0x1EB10, 0xD1);

            // remove the rest
            P(0x1EB11, 0xEA);
            P(0x1EB12, 0xEA);
            P(0x1EB13, 0xEA);
            P(0x1EB14, 0xEA);
            P(0x1EB15, 0xEA);
            return 1;
}



/* upstream hacks/common/other/AnimatedPaletteSimplification.cs */

void sr_w8_copy_palette_animation_settings(struct sr_ctx *c, int og_palette_set, int new_palette_set) {
            // for generated palettes, steal the animation settings of one from the original game
            int tableOffset = w8_wget_int(c, W8_KEY_ANIM_PAL_OFFSET);
            P(tableOffset + new_palette_set * 2, sr_peek_orig(c, 0x80802 + og_palette_set));
            P(tableOffset + new_palette_set * 2 + 1, sr_peek_orig(c, 0x80802 + og_palette_set + 1));
}

int sr_w8_hack_animated_palette(struct sr_ctx *c) {
            // modify the table of map palette animation data to not share start/end for sequential offsets, and as such not need
            // to be defined sequentially, so custom palette sets can simply reference existing palette animation data blocks from
            // the vanilla palettes they were copied from.
            // in vanilla, 8-bit offsets for these start at 0x80802 and are relative to 0x8087b, and the data runs from the offset
            // to the next palette set's offset.
            // this hack moves the 0x80802 table to a new section and adds a distinct start/end for each palette set. these still
            // point into the 0x8087b block, which should not need to change.

            sr_ensure_space_in_bank(&c->working_offset, 300);
            // tell other hacks where to find the table if they want to change animation settings
            sr_work_set_int(c, W8_KEY_ANIM_PAL_OFFSET, c->working_offset);
            // x77 of these
            // copy originals to new spot
            int newTableOffset = c->working_offset + 0xC00000;
            int newTableOffsetP1 = c->working_offset + 0xC00000 + 1;
            for (int i=0; i < 0x78; i++)
            {
                E(c->orig[0x80802 + i]);
                E(c->orig[0x80802 + i + 1]);
            }

            // add code to read from the modified table
            P(0x2BCA7, 0x22);
            P(0x2BCA8, (uint8_t)(c->working_offset));
            P(0x2BCA9, (uint8_t)(c->working_offset >> 8));
            P(0x2BCAA, (uint8_t)((c->working_offset >> 16) + 0xC0));
            P(0x2BCAB, 0xEA);
            P(0x2BCAC, 0xEA);
            P(0x2BCAD, 0xEA);
            P(0x2BCAE, 0xEA);
            P(0x2BCAF, 0xEA);
            P(0x2BCB0, 0xEA);
            P(0x2BCB1, 0xEA);
            P(0x2BCB2, 0xEA);

            // TXA
            E(0x8A);
            // ASL
            E(0x0A);
            // TAX
            E(0xAA);
            // LDA table+1,x
            E(0xBF);
            E((uint8_t)(newTableOffsetP1));
            E((uint8_t)(newTableOffsetP1 >> 8));
            E((uint8_t)(newTableOffsetP1 >> 16));
            // STA $10
            E(0x85);
            E(0x10);
            // STZ $11
            E(0x64);
            E(0x11);
            // LDA table,x
            E(0xBF);
            E((uint8_t)(newTableOffset));
            E((uint8_t)(newTableOffset >> 8));
            E((uint8_t)(newTableOffset >> 16));
            // RTL
            E(0x6B);
            return 1;
}



/* upstream hacks/common/procgen/Layer3Changes.cs */

static void l3_generate_increment_code(struct sr_ctx *c, const uint8_t *replaced_code, int replaced_code_len, int hoursLoc, int minutesLoc, int secondsLoc, int framesLoc) {
            // SEP 20
            E(0xE2);
            E(0x20);

            // use event flag x27 to turn timer on and off
            // LDA 7ECF27
            E(0xAF);
            E(W8_FLAG_TIMER_RUNNING);
            E(0xCF);
            E(0x7E);

            // BEQ skip -> 6 + 19 + 19 + 9 + 13 = x42
            E(0xF0);
            E(0x42);


            // LDA framesLocation
            E(0xAF);
            E((uint8_t)framesLoc);
            E((uint8_t)(framesLoc >> 8));
            E((uint8_t)((framesLoc >> 16)));
            // INC A - add a frame, here in the vblank handler code, after one frame has passed
            E(0x1A);
            // STA framesLocation
            E(0x8F);
            E((uint8_t)framesLoc);
            E((uint8_t)(framesLoc >> 8));
            E((uint8_t)((framesLoc >> 16)));
            // CMP #3C
            E(0xC9);
            E(0x3C);
            // BNE skip -> 6 + 19 + 19 + 9
            E(0xD0);
            E(0x35);
            // at this point, the frames have rolled over, so we set them to 0 and increment the seconds.
            // LDA #00
            E(0xA9);
            E(0x00);
            // STA framesLocation
            E(0x8F);
            E(0x1B);
            E(0xCF);
            E(0x7E);


            // LDA secondsLocation
            E(0xAF);
            E((uint8_t)secondsLoc);
            E((uint8_t)(secondsLoc >> 8));
            E((uint8_t)((secondsLoc >> 16)));
            // INC A - add one second to the timer
            E(0x1A);
            // STA secondsLocation
            E(0x8F);
            E((uint8_t)secondsLoc);
            E((uint8_t)(secondsLoc >> 8));
            E((uint8_t)((secondsLoc >> 16)));
            // CMP #3C
            E(0xC9);
            E(0x3C);
            // BNE skip -> 6 + 19 + 9
            E(0xD0);
            E(0x22);
            // at this point, the seconds have rolled over, so we set them to 0 and increment the minutes.
            // LDA #00
            E(0xA9);
            E(0x00);
            // STA secondsLocation
            E(0x8F);
            E(0x1A);
            E(0xCF);
            E(0x7E);


            // LDA minutesLocation
            E(0xAF);
            E((uint8_t)minutesLoc);
            E((uint8_t)(minutesLoc >> 8));
            E((uint8_t)((minutesLoc >> 16)));
            // INC A - add one minute to the timer
            E(0x1A);
            // STA minutesLocation
            E(0x8F);
            E((uint8_t)minutesLoc);
            E((uint8_t)(minutesLoc >> 8));
            E((uint8_t)((minutesLoc >> 16)));
            // CMP #3C
            E(0xC9);
            E(0x3C);
            // BNE skip -> 6 + 9
            E(0xD0);
            E(0x0F);
            // at this point, the minutes have rolled over, so we set them to 0 and increment the hours.
            // LDA #00
            E(0xA9);
            E(0x00);
            // STA minutesLocation
            E(0x8F);
            E(0x19);
            E(0xCF);
            E(0x7E);


            // LDA hoursLocation
            E(0xAF);
            E((uint8_t)hoursLoc);
            E((uint8_t)(hoursLoc >> 8));
            E((uint8_t)((hoursLoc >> 16)));
            // INC A
            E(0x1A);
            // STA hoursLocation
            E(0x8F);
            E((uint8_t)hoursLoc);
            E((uint8_t)(hoursLoc >> 8));
            E((uint8_t)((hoursLoc >> 16)));


            // put back the code code we replaced:
            for (int b_i = 0; b_i < replaced_code_len; b_i++) { const uint8_t b = replaced_code[b_i];
                E(b);
            }

            E(0x6B);
}

int sr_w8_hack_layer3_changes(struct sr_ctx *c) {
            const int showFloorNum = !w9_is_chaos(c);

            // For VRAM and multiply/divide address reference, see: https://en.wikibooks.org/wiki/Super_NES_Programming/SNES_Hardware_Registers

            // copy 01/E3FA subroutine to divide by 10, but as a JSL sub
            // calculates: A = Y % 10
            // also, $4214 is the divide result if you need that too
            int div10SubrOffset = c->working_offset;
            sr_work_set_int(c, W8_KEY_DIV10_SUBR_OFFSET, c->working_offset);
            // STY $4204
            E(0x8C);
            E(0x04);
            E(0x42);
            // LDA #0A
            E(0xA9);
            E(0x0A);
            // STA $4206
            E(0x8D);
            E(0x06);
            E(0x42);
            // wait for the div/mod calculation
            E(0xEA);
            E(0xEA);
            E(0xEA);
            E(0xEA);
            E(0xEA);
            E(0xEA);
            E(0xEA);
            E(0xEA);
            // LDA $4216
            E(0xAD);
            E(0x16);
            E(0x42);
            // RTL
            E(0x6B);

            // font data for "FLOR:." so we can show floor number .. number characters already exist
            static const uint8_t fontData[] = {
                0xFF, 0x00, 0x81, 0x7E, 0xFF, 0x60, 0xFC, 0x78, 0xFC, 0x60, 0xF0, 0x60, 0x90, 0x60, 0xF0, 0x00, // F
                0xF0, 0x00, 0x90, 0x60, 0xF0, 0x60, 0xF0, 0x60, 0xF0, 0x60, 0xFF, 0x60, 0x81, 0x7E, 0xFF, 0x00, // L
                0x3C, 0x00, 0x42, 0x3C, 0xFF, 0x66, 0xFF, 0x66, 0xFF, 0x66, 0xFF, 0x66, 0xC3, 0x3C, 0x3C, 0x00, // O
                0xFE, 0x00, 0x83, 0x7C, 0xFF, 0x66, 0xFF, 0x66, 0xFE, 0x7C, 0xFF, 0x66, 0x99, 0x66, 0xFF, 0x00, // R
                0x00, 0x00, 0x3C, 0x00, 0x24, 0x18, 0x3C, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x24, 0x18, 0x3C, 0x00, // :
                0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x24, 0x18, 0x3C, 0x00, // .
            };


            // upload custom font characters and show floor number (if applicable)

            int showMapNumSubrOffset = c->working_offset;
            // LDX #5AE7
            E(0xA2);
            E(0xE7);
            E(0x5A);
            // STX $2116
            E(0x8E);
            E(0x16);
            E(0x21);
            // LDA #00
            E(0xA9);
            E(0x00);
            // XBA
            E(0xEB);

            // LDA $7E00DC - load mapnum
            E(0xAF);
            E(0xDC);
            E(0x00);
            E(0x7E);

            // CMP #B4 - check for credits map
            E(0xC9);
            E(0xB4);
            // BNE over
            E(0xD0);
            E(0x08);
            // -- replaced code
            // E2 20      SEP 20
            E(0xE2);
            E(0x20);
            // A9 43      LDA #43
            E(0xA9);
            E(0x43);
            // 8D 55 0A   STA $0A55
            E(0x8D);
            E(0x55);
            E(0x0A);
            // RTL
            E(0x6B);
            // over:
            // LSR
            E(0x4A);
            // INC A
            E(0x1A);
            // TAY
            E(0xA8);
            // A = Y % 10
            E(0x22);
            E((uint8_t)div10SubrOffset);
            E((uint8_t)(div10SubrOffset >> 8));
            E((uint8_t)((div10SubrOffset >> 16) + 0xC0));
            // STA $0A55 - least significant digit of floor number
            E(0x8D);
            E(0x55);
            E(0x0A);
            // LDA $4214 - the divide result, and most significant digit of floor number
            E(0xAD);
            E(0x14);
            E(0x42);
            // REP 20
            E(0xC2);
            E(0x20);
            // CLC
            E(0x18);
            // ADC #2008 - for tile number reference
            E(0x69);
            E(0x08);
            E(0x20);

            if (showFloorNum)
            {
                // STA $2118
                E(0x8D);
                E(0x18);
                E(0x21);
                // LDA $0A55
                E(0xAD);
                E(0x55);
                E(0x0A);
                // AND #00FF
                E(0x29);
                E(0xFF);
                E(0x00);
                // CLC
                E(0x18);
                // ADC #2008 - for tile number reference
                E(0x69);
                E(0x08);
                E(0x20);
                // STA $2118
                E(0x8D);
                E(0x18);
                E(0x21);
            }

            // LDX #4008 - VRAM target address, in 16bit chunks - $4008 = $8010
            E(0xA2);
            E(0x08);
            E(0x40);
            // STX $2116 - VRAM transfer
            E(0x8E);
            E(0x16);
            E(0x21);

            for (int i = 0; i < (int)sizeof fontData; i += 2)
            {
                // LDA #fontDataByte
                E(0xA9);
                E(fontData[i]);
                E(fontData[i + 1]);
                // STA $2118
                E(0x8D);
                E(0x18);
                E(0x21);
            }

            // show the "floor" character by character
            if (showFloorNum)
            {
                // LDX #5AE1
                E(0xA2);
                E(0xE1);
                E(0x5A);
                // STX $2116
                E(0x8E);
                E(0x16);
                E(0x21);
                // LDA #2001 "F"
                E(0xA9);
                E(0x01);
                E(0x20);
                // STA $2118
                E(0x8D);
                E(0x18);
                E(0x21);
                // LDA #2002 "L"
                E(0xA9);
                E(0x02);
                E(0x20);
                // STA $2118
                E(0x8D);
                E(0x18);
                E(0x21);
                // LDA #2003 "O"
                E(0xA9);
                E(0x03);
                E(0x20);
                // STA $2118
                E(0x8D);
                E(0x18);
                E(0x21);
                // LDA #2003 "O"
                E(0xA9);
                E(0x03);
                E(0x20);
                // STA $2118
                E(0x8D);
                E(0x18);
                E(0x21);
                // LDA #2003 "R"
                E(0xA9);
                E(0x04);
                E(0x20);
                // STA $2118
                E(0x8D);
                E(0x18);
                E(0x21);
            }

            // SEP 20
            E(0xE2);
            E(0x20);

            // (replaced code) LDA 7EE000
            E(0xAF);
            E(0x00);
            E(0xE0);
            E(0x7E);
            // RTL
            E(0x6B);

            // replace this vanilla call with the above:
            // $01/E11E AF 00 E0 7E LDA $7EE000[$7E:E000]   A:5B01 X:00FF Y:0000 P:envMxdIzc
            P(0x1E11E, 0x22);
            P(0x1E11F, (uint8_t)showMapNumSubrOffset);
            P(0x1E120, (uint8_t)(showMapNumSubrOffset >> 8));
            P(0x1E121, (uint8_t)((showMapNumSubrOffset >> 16) + 0xC0));

            // add timer increment code to a couple vblank handlers.
            // The first is this one:
            // ***NMI
            //$00 / 0100 5C A8 C0 00 JMP $00C0A8[$00:C0A8]   A: 0001 X: 0030 Y: FFFE P:envMxdIzC
            //$00 / C0A8 C2 30       REP #$30                A:0001 X:0030 Y:FFFE P:envMxdIzC
            //$00 / C0AA 48          PHA A:0001 X: 0030 Y: FFFE P:envmxdIzC
            //$00 / C0AB DA PHX                     A: 0001 X: 0030 Y: FFFE P:envmxdIzC
            //$00 / C0AC 5A PHY                     A: 0001 X: 0030 Y: FFFE P:envmxdIzC
            //$00 / C0AD 0B          PHD A:0001 X: 0030 Y: FFFE P:envmxdIzC
            //$00 / C0AE 8B PHB                     A: 0001 X: 0030 Y: FFFE P:envmxdIzC
            //$00 / C0AF A9 00 00    LDA #$0000              A:0001 X:0030 Y:FFFE P:envmxdIzC
            //$00 / C0B2 5B TCD                     A: 0000 X: 0030 Y: FFFE P:envmxdIZC

            int incrementTimerSubrOffset = c->working_offset;
            P(0xC0AF, 0x22);
            P(0xC0B0, (uint8_t)incrementTimerSubrOffset);
            P(0xC0B1, (uint8_t)(incrementTimerSubrOffset >> 8));
            P(0xC0B2, (uint8_t)((incrementTimerSubrOffset >> 16) + 0xC0));
            l3_generate_increment_code(c, BYTES(0xC2, 0x20, 0xA9, 0x00, 0x00, 0x5B), 0x7ecf18, 0x7ecf19, 0x7ecf1a, 0x7ecf1b);

            // and another here:
            // $C0/20B2 AF 10 42 00 LDA $004210[$00:4210]   A:007E X:0006 Y:0004 P:envMxdIzc
            int incrementTimerSubrOffsetAltVBlank = c->working_offset;
            P(0x20B2, 0x22);
            P(0x20B3, (uint8_t)incrementTimerSubrOffsetAltVBlank);
            P(0x20B4, (uint8_t)(incrementTimerSubrOffsetAltVBlank >> 8));
            P(0x20B5, (uint8_t)((incrementTimerSubrOffsetAltVBlank >> 16) + 0xC0));

            l3_generate_increment_code(c, BYTES(0xAF, 0x10, 0x42, 0x00), 0x7ecf18, 0x7ecf19, 0x7ecf1a, 0x7ecf1b);


            // to render every frame, we replace this block:
            // $00/C144 C2 20       REP #$20                A:0001 X:0220 Y:FFFE P:envMxdIzC
            // $00/C146 E6 F4       INC $F4    [$00:00F4]   A:0001 X:0220 Y:FFFE P:envmxdIzC

            int showTimerSubrOffset = c->working_offset;
            P(0xC144, 0x22);
            P(0xC145, (uint8_t)showTimerSubrOffset);
            P(0xC146, (uint8_t)(showTimerSubrOffset >> 8));
            P(0xC147, (uint8_t)((showTimerSubrOffset >> 16) + 0xC0));

            // LDA 7ECF1B - the frames portion of our counter, which runs 00->3B
            E(0xAF);
            E(0x1B);
            E(0xCF);
            E(0x7E);

            // AND #03 - only render when the result is 0, so we aren't running all of this every frame
            E(0x29);
            E(0x03);
            // BEQ over
            E(0xF0);
            E(0x03);
            // (replaced code) INC $F4
            E(0xE6);
            E(0xF4);
            // RTL
            E(0x6B);

            // over:
            // LDA $7E00DC - load mapnum
            E(0xAF);
            E(0xDC);
            E(0x00);
            E(0x7E);
            // CMP #B4 - credits map
            E(0xC9);
            E(0xB4);
            // BNE over
            E(0xD0);
            E(0x03);
            // (replaced code) INC $F4
            E(0xE6);
            E(0xF4);
            // RTL
            E(0x6B);

            // over:
            // LDX #5AC1
            E(0xA2);
            E(0xF4);
            E(0x5A);
            // STX $2116
            E(0x8E);
            E(0x16);
            E(0x21);

            // (hours)
            // LDA $7ECF18 - hours value
            E(0xAF);
            E(0x18);
            E(0xCF);
            E(0x7E);
            // TAY
            E(0xA8);
            // A = Y % 10
            E(0x22);
            E((uint8_t)div10SubrOffset);
            E((uint8_t)(div10SubrOffset >> 8));
            E((uint8_t)((div10SubrOffset >> 16) + 0xC0));
            // REP 20
            E(0xC2);
            E(0x20);
            // AND #00FF
            E(0x29);
            E(0xFF);
            E(0x00);
            // CLC
            E(0x18);
            // ADC #2008 - for tile id values
            E(0x69);
            E(0x08);
            E(0x20);
            // PHA - hold onto it for now
            E(0x48);
            // LDA $4214 - the 10s digit of the calculation
            E(0xAD);
            E(0x14);
            E(0x42);
            // CLC
            E(0x18);
            // ADC #2008 - for tile id values
            E(0x69);
            E(0x08);
            E(0x20);
            // STA $2118 - write to vram
            E(0x8D);
            E(0x18);
            E(0x21);
            // PLA - grab the ones digit back out
            E(0x68);
            // STA $2118 - write to vram
            E(0x8D);
            E(0x18);
            E(0x21);
            // LDA #2005 - ":"
            E(0xA9);
            E(0x05);
            E(0x20);
            // STA $2118 - write to vram
            E(0x8D);
            E(0x18);
            E(0x21);

            // (minutes)
            // AND #00FF
            E(0x29);
            E(0xFF);
            E(0x00);
            // SEP 20
            E(0xE2);
            E(0x20);
            // LDA $7ECF19 - minutes value
            E(0xAF);
            E(0x19);
            E(0xCF);
            E(0x7E);
            // TAY
            E(0xA8);
            // A = Y % 10
            E(0x22);
            E((uint8_t)div10SubrOffset);
            E((uint8_t)(div10SubrOffset >> 8));
            E((uint8_t)((div10SubrOffset >> 16) + 0xC0));
            // REP 20
            E(0xC2);
            E(0x20);
            // CLC
            E(0x18);
            // ADC #2008 - for tile id values
            E(0x69);
            E(0x08);
            E(0x20);
            // PHA - hold onto it for now
            E(0x48);
            // LDA $4214 - the 10s digit of the calculation
            E(0xAD);
            E(0x14);
            E(0x42);
            // CLC
            E(0x18);
            // ADC #2008 - for tile id values
            E(0x69);
            E(0x08);
            E(0x20);
            // STA $2118 - write to vram
            E(0x8D);
            E(0x18);
            E(0x21);
            // PLA - grab the ones digit back out
            E(0x68);
            // STA $2118 - write to vram
            E(0x8D);
            E(0x18);
            E(0x21);
            // LDA #2005 - ":"
            E(0xA9);
            E(0x05);
            E(0x20);
            // STA $2118 - write to vram
            E(0x8D);
            E(0x18);
            E(0x21);


            // (seconds)
            // AND #00FF
            E(0x29);
            E(0xFF);
            E(0x00);
            // SEP 20
            E(0xE2);
            E(0x20);
            // LDA $7ECF1A - seconds value
            E(0xAF);
            E(0x1A);
            E(0xCF);
            E(0x7E);
            // TAY
            E(0xA8);
            // A = Y % 10
            E(0x22);
            E((uint8_t)div10SubrOffset);
            E((uint8_t)(div10SubrOffset >> 8));
            E((uint8_t)((div10SubrOffset >> 16) + 0xC0));
            // REP 20
            E(0xC2);
            E(0x20);
            // CLC
            E(0x18);
            // ADC #2008 - for tile id values
            E(0x69);
            E(0x08);
            E(0x20);
            // PHA - hold onto it for now
            E(0x48);
            // LDA $4214 - the 10s digit of the calculation
            E(0xAD);
            E(0x14);
            E(0x42);
            // CLC
            E(0x18);
            // ADC #2008 - for tile id values
            E(0x69);
            E(0x08);
            E(0x20);
            // STA $2118 - write to vram
            E(0x8D);
            E(0x18);
            E(0x21);
            // PLA - grab the ones digit back out
            E(0x68);
            // STA $2118 - write to vram
            E(0x8D);
            E(0x18);
            E(0x21);
            // LDA #2006 - "."
            E(0xA9);
            E(0x06);
            E(0x20);
            // STA $2118 - write to vram
            E(0x8D);
            E(0x18);
            E(0x21);


            // (frames)
            // AND #00FF
            E(0x29);
            E(0xFF);
            E(0x00);
            // SEP 20
            E(0xE2);
            E(0x20);
            // LDA $7ECF1B - frames value
            E(0xAF);
            E(0x1B);
            E(0xCF);
            E(0x7E);

            // ---
            // quick conversion from 0-60 to 0-100 using bitwise ops
            // shift right once, add (90)
            // shift right twice more, add (97.5ish)
            // close enough.
            // PHA
            // LSR
            // 8F 1C CF 7E    STA 7ECF1C
            // LSR
            // LSR
            // 8F 1D CF 7E    STA 7ECF1D
            // PLA
            // ADC 7ECF1D
            // ADC 7ECF1C

            // PHA
            E(0x48);
            // LSR
            E(0x4A);
            // STA $7ECF1C
            E(0x8F);
            E(0x1C);
            E(0xCF);
            E(0x7E);
            // LSR, LSR
            E(0x4A);
            E(0x4A);
            // STA $7ECF1D
            E(0x8F);
            E(0x1D);
            E(0xCF);
            E(0x7E);
            // PLA
            E(0x68);
            // ADC $7ECF1D
            E(0x6F);
            E(0x1C);
            E(0xCF);
            E(0x7E);
            // ADC $7ECF1C
            E(0x6F);
            E(0x1D);
            E(0xCF);
            E(0x7E);
            // ---
            // now more of the same.
            // TAY
            E(0xA8);
            // A = Y % 10
            E(0x22);
            E((uint8_t)div10SubrOffset);
            E((uint8_t)(div10SubrOffset >> 8));
            E((uint8_t)((div10SubrOffset >> 16) + 0xC0));
            // REP 20
            E(0xC2);
            E(0x20);
            // CLC
            E(0x18);
            // ADC #2008 - for tile id values
            E(0x69);
            E(0x08);
            E(0x20);
            // PHA - hold onto it for now
            E(0x48);
            // LDA $4214 - the 10s digit of the calculation
            E(0xAD);
            E(0x14);
            E(0x42);
            // CLC
            E(0x18);
            // ADC #2008 - for tile id values
            E(0x69);
            E(0x08);
            E(0x20);
            // STA $2118 - write to vram
            E(0x8D);
            E(0x18);
            E(0x21);
            // PLA - grab the ones digit back out
            E(0x68);
            // STA $2118 - write to vram
            E(0x8D);
            E(0x18);
            E(0x21);
            // LDA #2000 - " "
            E(0xA9);
            E(0x00);
            E(0x20);
            // STA $2118 - write to vram
            E(0x8D);
            E(0x18);
            E(0x21);
            // (replaced code) INC $F4
            E(0xE6);
            E(0xF4);
            // RTL
            E(0x6B);


            // ///////////////////////////////////////////////////////////
            // Modify number chars to look okay when not above a player bar
            // ///////////////////////////////////////////////////////////

            // take the line out of the number graphics
            // 127F00, 16 bytes each
            static const uint8_t numberCharsChange[] = {
                0x3C, 0x00, 0x42, 0x3C, 0xFF, 0x66, 0xFF, 0x66, 0xFF, 0x66, 0xFF, 0x66, 0x42, 0x3C, 0x3C, 0x00,
                0x18, 0x00, 0x24, 0x18, 0x7C, 0x38, 0x3C, 0x18, 0x3C, 0x18, 0x3C, 0x18, 0x42, 0x3C, 0x3C, 0x00,
                0x3C, 0x00, 0x42, 0x3C, 0xFF, 0x46, 0xFF, 0x06, 0x3E, 0x18, 0x7E, 0x20, 0x81, 0x7E, 0x7E, 0x00,
                0x3C, 0x00, 0x43, 0x3C, 0xFF, 0x66, 0x7E, 0x0C, 0x7F, 0x06, 0xFF, 0x66, 0x42, 0x3C, 0x3C, 0x00,
                0x0C, 0x00, 0x12, 0x0C, 0x3E, 0x1C, 0x7E, 0x2C, 0xFE, 0x4C, 0xFF, 0x7E, 0x72, 0x0C, 0x0C, 0x00,
                0x7E, 0x00, 0x81, 0x7E, 0xFE, 0x60, 0xFE, 0x7C, 0x7F, 0x06, 0xFF, 0x66, 0x42, 0x3C, 0x3C, 0x00,
                0x3C, 0x00, 0x42, 0x3C, 0xFC, 0x60, 0xFE, 0x7C, 0xFF, 0x66, 0xFF, 0x66, 0x42, 0x3C, 0x3C, 0x00,
                0x7E, 0x00, 0x81, 0x7E, 0xFF, 0x66, 0x7E, 0x04, 0x1C, 0x08, 0x3C, 0x18, 0x24, 0x18, 0x18, 0x00,
                0x3C, 0x00, 0x42, 0x3C, 0xFF, 0x66, 0x7E, 0x3C, 0xFF, 0x66, 0xFF, 0x66, 0x42, 0x3C, 0x3C, 0x00,
                0x3C, 0x00, 0x42, 0x3C, 0xFF, 0x66, 0xFF, 0x66, 0x7F, 0x3E, 0x3F, 0x06, 0x42, 0x3C, 0x3C, 0x00,
            };

            int fontOffset = 0x127F00;
            for (size_t b_i = 0; b_i < sizeof numberCharsChange; b_i++) { const uint8_t b = numberCharsChange[b_i];
                P(fontOffset++, b);
            }

            // a little extra code to handle display of timer when in the ring menu
            // replace:
            // $C0 / 6D1E A2 C0 18    LDX #$18C0            A:0800 X:0038 Y:0004 P:envmxdIZc
            // $C0 / 6D21 AD 0B 18    LDA $180B[$00:180B]   A:0800 X:18C0 Y:0004 P:envmxdIzc
            P(0x6D1E, 0x22);
            P(0x6D1F, (uint8_t)c->working_offset);
            P(0x6D20, (uint8_t)(c->working_offset >> 8));
            P(0x6D21, (uint8_t)((c->working_offset >> 16) + 0xC0));
            P(0x6D22, 0xEA);
            P(0x6D23, 0xEA);

            // PHA
            E(0x48);
            // LDA #0000
            E(0xA9);
            E(0x00);
            E(0x00);
            // SEP 20
            E(0xE2);
            E(0x20);
            // JSL showTimerSubrOffset
            E(0x22);
            E((uint8_t)showTimerSubrOffset);
            E((uint8_t)(showTimerSubrOffset >> 8));
            E((uint8_t)((showTimerSubrOffset >> 16) + 0xC0));
            // REP 20
            E(0xC2);
            E(0x20);
            // PLA
            E(0x68);
            // (replaced code) LDX #18C0
            E(0xA2);
            E(0xC0);
            E(0x18);
            // (replaced code) LDA $180B
            E(0xAD);
            E(0x0B);
            E(0x18);
            // RTL
            E(0x6B);

            return 1;
}



/* upstream hacks/common/procgen/TimerDialogue.cs */

static void td_print16_bit_value_to_dialogue(struct sr_ctx *c, int div10SubrOffset) {
            // pull two event byte params
            // use these to index 7E to get the value to print
            // 10000 -> 7EA22F
            //  1000 -> 7EA230
            //   100 -> 7EA231
            //    10 -> 7EA232
            //     1 -> 7EA233
            // shift back if zero in any high digit

            // LDA #00
            E(0xA9);
            E(0x00);

            // STA 7EA234
            E(0x8F);
            E(0x34);
            E(0xA2);
            E(0x7E);

            // LDA #80
            E(0xA9);
            E(0x80);
            // STA 7EA22F
            E(0x8F);
            E(0x2F);
            E(0xA2);
            E(0x7E);
            // STA 7EA230
            E(0x8F);
            E(0x30);
            E(0xA2);
            E(0x7E);
            // STA 7EA231
            E(0x8F);
            E(0x31);
            E(0xA2);
            E(0x7E);
            // STA 7EA232
            E(0x8F);
            E(0x32);
            E(0xA2);
            E(0x7E);
            // STA 7EA233
            E(0x8F);
            E(0x33);
            E(0xA2);
            E(0x7E);


            // basically just divide by 10 until we get zero, then stop.

            // PHX
            E(0xDA);

            // 16 bit a
            E(0xC2);
            E(0x20);

            // LDA 7EA1D1
            E(0xAF);
            E(0xD1);
            E(0xA1);
            E(0x7E);
            // INC A
            E(0x1A);
            // TAX
            E(0xAA);
            // INC A
            E(0x1A);
            // STA 7EA1D1
            E(0x8F);
            E(0xD1);
            E(0xA1);
            E(0x7E);
            // LDA C90000,X
            E(0xBF);
            E(0x00);
            E(0x00);
            E(0xC9);

            // TAX
            E(0xAA);

            // BF 00 00 7E LDA 7E0000,x ; this is the value we print
            E(0xBF);
            E(0x00);
            E(0x00);
            E(0x7E);

            // PHA ; store it off for now
            E(0x48);

            // LDX #A233 ; target position
            E(0xA2);
            E(0x33);
            E(0xA2);

            // loop start:
            int loopOffset = c->working_offset;

            // 8 bit a
            E(0xE2);
            E(0x20);

            // LDA #00
            E(0xA9);
            E(0x00);
            // PHA
            E(0x48);
            // PLB
            E(0xAB);
            // stz 4207 (why?)
            E(0x9C);
            E(0x07);
            E(0x42);
            // 16 bit a
            E(0xC2);
            E(0x20);
            // lda #0000
            E(0xA9);
            E(0x00);
            E(0x00);
            // tay
            E(0xA8);
            // pla with the value from earlier that we want to print
            E(0x68);
            // tay (for subroutine)
            E(0xA8);

            // 8 bit a
            E(0xE2);
            E(0x20);

            // call div10 subroutine
            E(0x22);
            E((uint8_t)div10SubrOffset);
            E((uint8_t)(div10SubrOffset >> 8));
            E((uint8_t)((div10SubrOffset >> 16) + 0xC0));
            // mod result in a; divide result in 4214
            // if it's 0, we're done
            // otherwise, dex and go again

            // clc
            E(0x18);
            // adc #B5
            E(0x69);
            E(0xB5);
            // STA 7E0000,x
            E(0x9F);
            E(0x00);
            E(0x00);
            E(0x7E);

            // dex
            E(0xCA);

            // cpx #A22E
            E(0xE0);
            E(0x2E);
            E(0xA2);

            // beq out
            E(0xF0);
            E(0x0A);

            // 16 bit a
            E(0xC2);
            E(0x20);

            // LDA 4214 - divide result
            E(0xAD);
            E(0x14);
            E(0x42);

            // beq out
            E(0xF0);
            E(0x03);

            // PHA to push the new, div10'ed value and use that next time
            E(0x48);

            int loopSrcOffset = c->working_offset + 2;
            // bra back (or jmp)
            E(0x80);
            E((uint8_t)(loopOffset- loopSrcOffset));

            // out:

            // 8 bit a
            E(0xE2);
            E(0x20);

            // restore stuff at the end
            // PLX
            E(0xFA);

            E(0x6B);

}

static void td_print32_bit_value_to_dialogue(struct sr_ctx *c, int div10SubrOffset) {
            // pull two event byte params
            // use these to index 7E to get the value to print
            // 10000000 -> 7EA22F
            //  1000000 -> 7EA230
            //   100000 -> 7EA231
            //    10000 -> 7EA232
            //     1000 -> 7EA233
            //      100 -> 7EA234
            //       10 -> 7EA235
            //        1 -> 7EA236
            // shift back if zero in any high digit

            // LDA #00
            E(0xA9);
            E(0x00);

            // STA 7EA234
            E(0x8F);
            E(0x37);
            E(0xA2);
            E(0x7E);

            // LDA #80
            E(0xA9);
            E(0x80);
            // STA 7EA22F
            E(0x8F);
            E(0x2F);
            E(0xA2);
            E(0x7E);
            // STA 7EA230
            E(0x8F);
            E(0x30);
            E(0xA2);
            E(0x7E);
            // STA 7EA231
            E(0x8F);
            E(0x31);
            E(0xA2);
            E(0x7E);
            // STA 7EA232
            E(0x8F);
            E(0x32);
            E(0xA2);
            E(0x7E);
            // STA 7EA233
            E(0x8F);
            E(0x33);
            E(0xA2);
            E(0x7E);
            // STA 7EA234
            E(0x8F);
            E(0x34);
            E(0xA2);
            E(0x7E);
            // STA 7EA235
            E(0x8F);
            E(0x35);
            E(0xA2);
            E(0x7E);
            // STA 7EA236
            E(0x8F);
            E(0x36);
            E(0xA2);
            E(0x7E);

            // basically just divide by 10 until we get zero, then stop.

            // PHX
            E(0xDA);

            // 16 bit a
            E(0xC2);
            E(0x20);

            // LDA 7EA1D1
            E(0xAF);
            E(0xD1);
            E(0xA1);
            E(0x7E);
            // INC A
            E(0x1A);
            // TAX
            E(0xAA);
            // INC A
            E(0x1A);
            // STA 7EA1D1
            E(0x8F);
            E(0xD1);
            E(0xA1);
            E(0x7E);
            // LDA C90000,X
            E(0xBF);
            E(0x00);
            E(0x00);
            E(0xC9);

            // PHA ; save the address off
            E(0x48);

            // TAX
            E(0xAA);

            // BF 00 00 7E LDA 7E0000,x ; this is the value we print
            E(0xBF);
            E(0x00);
            E(0x00);
            E(0x7E);

            // PHA ; store it off for now
            E(0x48);

            // LDX #A236 ; target position
            E(0xA2);
            E(0x36);
            E(0xA2);

            // loop start:
            int loopOffset = c->working_offset;

            // 8 bit a
            E(0xE2);
            E(0x20);

            // LDA #00
            E(0xA9);
            E(0x00);
            // PHA
            E(0x48);
            // PLB
            E(0xAB);
            // stz 4207 (why?)
            E(0x9C);
            E(0x07);
            E(0x42);
            // 16 bit a
            E(0xC2);
            E(0x20);
            // lda #0000
            E(0xA9);
            E(0x00);
            E(0x00);
            // tay
            E(0xA8);
            // pla with the value from earlier that we want to print
            E(0x68);
            // tay (for subroutine)
            E(0xA8);

            // 8 bit a
            E(0xE2);
            E(0x20);

            // call div10 subroutine
            E(0x22);
            E((uint8_t)div10SubrOffset);
            E((uint8_t)(div10SubrOffset >> 8));
            E((uint8_t)((div10SubrOffset >> 16) + 0xC0));
            // mod result in a; divide result in 4214
            // if it's 0, we're done
            // otherwise, dex and go again

            // clc
            E(0x18);
            // adc #B5
            E(0x69);
            E(0xB5);
            // STA 7E0000,x
            E(0x9F);
            E(0x00);
            E(0x00);
            E(0x7E);

            // 16 bit a
            E(0xC2);
            E(0x20);

            // dex
            E(0xCA);

            // cpx #A232
            E(0xE0);
            E(0x32);
            E(0xA2);

            // beq nextByte
            E(0xF0);
            E(0x0D);

            // LDA 4214 - divide result
            E(0xAD);
            E(0x14);
            E(0x42);

            // bne in
            E(0xD0);
            E(0x05);

            // PLA - take address off the stack
            E(0x68);
            // 8 bit a
            E(0xE2);
            E(0x20);
            // PLX
            E(0xFA);
            // RTL
            E(0x6B);
            // in:

            // PHA to push the new, div10'ed value and use that next time
            E(0x48);

            int loopSrcOffset = c->working_offset + 2;
            // bra back (or jmp)
            E(0x80);
            E((uint8_t)(loopOffset - loopSrcOffset));

            // nextByte:

            // PLA - address saved off from beginning
            E(0x68);
            // INC A ; move on
            E(0x1A);
            // INC A
            E(0x1A);
            // TAX
            E(0xAA);

            // basically the same shit from above

            // BF 00 00 7E LDA 7E0000,x ; this is the value we print
            E(0xBF);
            E(0x00);
            E(0x00);
            E(0x7E);
            // zero in whole second value? skip this
            // BNE over
            E(0xD0);
            E(0x04);
            // 8bit a
            E(0xE2);
            E(0x20);
            // PLX
            E(0xFA);
            // RTL
            E(0x6B);
            // over:

            // PHA ; store it off for now
            E(0x48);

            // LDX #A232 ; target position
            E(0xA2);
            E(0x32);
            E(0xA2);

            // loop start:
            loopOffset = c->working_offset;

            // 8 bit a
            E(0xE2);
            E(0x20);

            // LDA #00
            E(0xA9);
            E(0x00);
            // PHA
            E(0x48);
            // PLB
            E(0xAB);
            // stz 4207 (why?)
            E(0x9C);
            E(0x07);
            E(0x42);
            // 16 bit a
            E(0xC2);
            E(0x20);
            // lda #0000
            E(0xA9);
            E(0x00);
            E(0x00);
            // tay
            E(0xA8);
            // pla with the value from earlier that we want to print
            E(0x68);
            // tay (for subroutine)
            E(0xA8);

            // 8 bit a
            E(0xE2);
            E(0x20);

            // call div10 subroutine
            E(0x22);
            E((uint8_t)div10SubrOffset);
            E((uint8_t)(div10SubrOffset >> 8));
            E((uint8_t)((div10SubrOffset >> 16) + 0xC0));
            // mod result in a; divide result in 4214
            // if it's 0, we're done
            // otherwise, dex and go again

            // clc
            E(0x18);
            // adc #B5
            E(0x69);
            E(0xB5);
            // STA 7E0000,x
            E(0x9F);
            E(0x00);
            E(0x00);
            E(0x7E);

            // 16 bit a
            E(0xC2);
            E(0x20);

            // LDA 4214 - divide result
            E(0xAD);
            E(0x14);
            E(0x42);

            // bne in
            E(0xD0);
            E(0x04);

            // 8 bit a
            E(0xE2);
            E(0x20);
            // PLX
            E(0xFA);
            E(0x6B);

            // in:

            // dex
            E(0xCA);

            // cpx #A22E
            E(0xE0);
            E(0x2E);
            E(0xA2);

            // beq out
            E(0xF0);
            E(0x03);


            // PHA to push the new, div10'ed value and use that next time
            E(0x48);

            loopSrcOffset = c->working_offset + 2;
            // bra back (or jmp)
            E(0x80);
            E((uint8_t)(loopOffset - loopSrcOffset));

            // out:

            // 8 bit a
            E(0xE2);
            E(0x20);

            // restore stuff at the end
            // PLX
            E(0xFA);
            // RTL
            E(0x6B);
}

int sr_w8_hack_timer_dialogue(struct sr_ctx *c) {
            // note this hack currently has a dependency on Layer3Changes because it uses this subroutine
            int div10SubrOffset = w8_wget_int(c, W8_KEY_DIV10_SUBR_OFFSET);
            int print16BitSubrOffset = c->working_offset;
            td_print16_bit_value_to_dialogue(c, div10SubrOffset);
            int print32BitSubrOffset = c->working_offset;
            td_print32_bit_value_to_dialogue(c, div10SubrOffset);

            int printTimerSubrOffset = c->working_offset;
            // print timer value event command
            w8_register_event(c, "print timer", printTimerSubrOffset);
            // print 16 bit value event command
            w8_register_event(c, "print 16 bit value", print16BitSubrOffset);
            // print 32 bit value event command
            w8_register_event(c, "print 32 bit value", print32BitSubrOffset);

            // LDA #00
            E(0xA9);
            E(0x00);
            // PHA
            E(0x48);
            // PLB
            E(0xAB);
            // stz 4207 (why?)
            E(0x9C);
            E(0x07);
            E(0x42);
            // 16 bit a
            E(0xC2);
            E(0x20);
            // lda #0000
            E(0xA9);
            E(0x00);
            E(0x00);
            // tay
            E(0xA8);
            // 8 bit a
            E(0xE2);
            E(0x20);
            // lda hours
            E(0xAF);
            E(0x18);
            E(0xCF);
            E(0x7E);
            // tay (for subroutine)
            E(0xA8);
            // call div10 subroutine
            E(0x22);
            E((uint8_t)div10SubrOffset);
            E((uint8_t)(div10SubrOffset >> 8));
            E((uint8_t)((div10SubrOffset >> 16) + 0xC0));
            // result = mod result - low digit
            // clc
            E(0x18);
            // adc #B5
            E(0x69);
            E(0xB5);
            // pha
            E(0x48);
            // LDA 4214 - divide result - high digit
            E(0xAD);
            E(0x14);
            E(0x42);
            // CLC
            E(0x18);
            // ADC #B5
            E(0x69);
            E(0xB5);
            // STA 7EA22F - this is the dialogue thing i guess - first digit
            E(0x8F);
            E(0x2F);
            E(0xA2);
            E(0x7E);
            // PLA - second digit
            E(0x68);
            // STA 7EA230 - second digit
            E(0x8F);
            E(0x30);
            E(0xA2);
            E(0x7E);
            // LDA #C5 - this is a colon character i think
            E(0xA9);
            E(0xC5);
            // STA 7EA231 - third character
            E(0x8F);
            E(0x31);
            E(0xA2);
            E(0x7E);

            // ////////////////////////////////////////////////////////
            // AF 19 CF 7E  LDA $7ECF19 minutes
            // .. code essentially the same as above for hours
            // ////////////////////////////////////////////////////////
            E(0xAF);
            E(0x19);
            E(0xCF);
            E(0x7E);

            E(0xA8);

            E(0x22);
            E((uint8_t)div10SubrOffset);
            E((uint8_t)(div10SubrOffset >> 8));
            E((uint8_t)((div10SubrOffset >> 16) + 0xC0));

            E(0x18);

            E(0x69);
            E(0xB5);

            E(0x48);

            E(0xAD);
            E(0x14);
            E(0x42);

            E(0x18);

            E(0x69);
            E(0xB5);

            E(0x8F);
            E(0x32);
            E(0xA2);
            E(0x7E);

            E(0x68);

            E(0x8F);
            E(0x33);
            E(0xA2);
            E(0x7E);

            E(0xA9);
            E(0xC5);

            E(0x8F);
            E(0x34);
            E(0xA2);
            E(0x7E);

            // ////////////////////////////////////////////////////////
            // AF 1A CF 7E  LDA $7ECF1A seconds
            // .. code essentially the same as above for hours
            // ////////////////////////////////////////////////////////
            E(0xAF);
            E(0x1A);
            E(0xCF);
            E(0x7E);

            E(0xA8);

            E(0x22);
            E((uint8_t)div10SubrOffset);
            E((uint8_t)(div10SubrOffset >> 8));
            E((uint8_t)((div10SubrOffset >> 16) + 0xC0));

            E(0x18);

            E(0x69);
            E(0xB5);

            E(0x48);

            E(0xAD);
            E(0x14);
            E(0x42);

            E(0x18);

            E(0x69);
            E(0xB5);

            E(0x8F);
            E(0x35);
            E(0xA2);
            E(0x7E);

            E(0x68);

            E(0x8F);
            E(0x36);
            E(0xA2);
            E(0x7E);

            E(0xA9);
            E(0xBF);

            E(0x8F);
            E(0x37);
            E(0xA2);
            E(0x7E);

            // ////////////////////////////////////////////////////////
            // AF 1B CF 7E  LDA $7ECF1B frames
            // .. roughly the same as above except we convert to 0-99, similarly to in Layer3Changes
            // ////////////////////////////////////////////////////////
            E(0xAF);
            E(0x1B);
            E(0xCF);
            E(0x7E);

            // ---
            // conversion from 0-60 to 0-100
            // shift right once, add (90)
            // shift right twice more, add (97.5ish)
            // close enough.
            // PHA
            E(0x48);

            // LSR
            E(0x4A);

            // 8F 1C CF 7E    STA 7ECF1C
            E(0x8F);
            E(0x1C);
            E(0xCF);
            E(0x7E);

            // LSR
            // LSR
            E(0x4A);
            E(0x4A);

            // 8F 1D CF 7E    STA 7ECF1D
            E(0x8F);
            E(0x1D);
            E(0xCF);
            E(0x7E);

            // PLA
            E(0x68);

            // ADC 7ECF1D
            E(0x6F);
            E(0x1C);
            E(0xCF);
            E(0x7E);

            // ADC 7ECF1C
            E(0x6F);
            E(0x1D);
            E(0xCF);
            E(0x7E);

            // ---

            // now the same code as above for printing to dialogue.
            E(0xA8);

            E(0x22);
            E((uint8_t)div10SubrOffset);
            E((uint8_t)(div10SubrOffset >> 8));
            E((uint8_t)((div10SubrOffset >> 16) + 0xC0));

            E(0x18);

            E(0x69);
            E(0xB5);

            E(0x48);

            E(0xAD);
            E(0x14);
            E(0x42);

            E(0x18);

            E(0x69);
            E(0xB5);

            E(0x8F);
            E(0x38);
            E(0xA2);
            E(0x7E);

            E(0x68);

            E(0x8F);
            E(0x39);
            E(0xA2);
            E(0x7E);

            E(0xA9);
            E(0x00);

            // zero at 7EA23A to end the string
            E(0x8F);
            E(0x3A);
            E(0xA2);
            E(0x7E);

            // now just restore all the shit and return

            // LDA #7E
            E(0xA9);
            E(0x7E);
            // PHA
            E(0x48);
            // PLB
            E(0xAB);

            E(0x6B);

            return 1;
}



/* upstream hacks/common/procgen/Statistics.cs */

static void st_inject_magic_damage_tracker(struct sr_ctx *c, const uint8_t *replaced_code, int replaced_code_len) {
            // replaced code A
            // 9D F1 E1    STA $E1F1,x
            E(0x9D);
            E(0xF1);
            E(0xE1);
            // E0 00 06    CPX #$0600
            E(0xE0);
            E(0x00);
            E(0x06);
            // 90 xx       BCC/BLT player
            E(0x90);
            E(0x3F);
            // (enemy)
            // ; total damage dealt
            // 48          PHA
            E(0x48);
            // 18          CLC
            E(0x18);
            // 6F xx xx xx ADC TOTAL_DAMAGE_DEALT_24BIT
            E(0x6F);
            E((uint8_t)W8_RAM_TOTAL_DAMAGE_DEALT_32BIT);
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_DEALT_32BIT >> 8));
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_DEALT_32BIT >> 16));
            // CMP #2710
            E(0xC9);
            E(0x10);
            E(0x27);
            // BCC/BLT noOverflow
            E(0x90);
            E(0x0F);
            // 48          PHA
            E(0x48);
            // AF xx xx xx LDA TOTAL_DAMAGE_DEALT_24BIT+2
            E(0xAF);
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_DEALT_32BIT + 2));
            E((uint8_t)((W8_RAM_TOTAL_DAMAGE_DEALT_32BIT + 2) >> 8));
            E((uint8_t)((W8_RAM_TOTAL_DAMAGE_DEALT_32BIT + 2) >> 16));
            // 1A          INC A
            E(0x1A);
            // 8F xx xx xx STA TOTAL_DAMAGE_DEALT_24BIT+2
            E(0x8F);
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_DEALT_32BIT + 2));
            E((uint8_t)((W8_RAM_TOTAL_DAMAGE_DEALT_32BIT + 2) >> 8));
            E((uint8_t)((W8_RAM_TOTAL_DAMAGE_DEALT_32BIT + 2) >> 16));
            // 68          PLA
            E(0x68);
            // SEC
            E(0x38);
            // SBC #2710
            E(0xE9);
            E(0x10);
            E(0x27);

            // noOverflow:
            // 8F xx xx xx STA TOTAL_DAMAGE_DEALT_24BIT
            E(0x8F);
            E((uint8_t)W8_RAM_TOTAL_DAMAGE_DEALT_32BIT);
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_DEALT_32BIT >> 8));
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_DEALT_32BIT >> 16));
            // ; now total physical damage dealt too
            // 68          PLA
            E(0x68);
            // 18          CLC
            E(0x18);
            // 6F xx xx xx ADC TOTAL_P_DAMAGE_DEALT_24BIT
            E(0x6F);
            E((uint8_t)W8_RAM_TOTAL_M_DAMAGE_DEALT_32BIT);
            E((uint8_t)(W8_RAM_TOTAL_M_DAMAGE_DEALT_32BIT >> 8));
            E((uint8_t)(W8_RAM_TOTAL_M_DAMAGE_DEALT_32BIT >> 16));
            // CMP #2710
            E(0xC9);
            E(0x10);
            E(0x27);
            // BCC/BLT noPOverflow
            E(0x90);
            E(0x0F);
            // 48          PHA
            E(0x48);
            // AF xx xx xx LDA TOTAL_P_DAMAGE_DEALT_24BIT+2
            E(0xAF);
            E((uint8_t)(W8_RAM_TOTAL_M_DAMAGE_DEALT_32BIT + 2));
            E((uint8_t)((W8_RAM_TOTAL_M_DAMAGE_DEALT_32BIT + 2) >> 8));
            E((uint8_t)((W8_RAM_TOTAL_M_DAMAGE_DEALT_32BIT + 2) >> 16));
            // 1A          INC A
            E(0x1A);
            // 8F xx xx xx STA TOTAL_P_DAMAGE_DEALT_24BIT+2
            E(0x8F);
            E((uint8_t)(W8_RAM_TOTAL_M_DAMAGE_DEALT_32BIT + 2));
            E((uint8_t)((W8_RAM_TOTAL_M_DAMAGE_DEALT_32BIT + 2) >> 8));
            E((uint8_t)((W8_RAM_TOTAL_M_DAMAGE_DEALT_32BIT + 2) >> 16));
            // 68          PLA
            E(0x68);
            // SEC
            E(0x38);
            // SBC #2710
            E(0xE9);
            E(0x10);
            E(0x27);
            // noPOverflow:
            // 8F xx xx xx STA TOTAL_DAMAGE_DEALT_24BIT
            E(0x8F);
            E((uint8_t)W8_RAM_TOTAL_M_DAMAGE_DEALT_32BIT);
            E((uint8_t)(W8_RAM_TOTAL_M_DAMAGE_DEALT_32BIT >> 8));
            E((uint8_t)(W8_RAM_TOTAL_M_DAMAGE_DEALT_32BIT >> 16));
            // ; end (replaced code)
            for (int b_i = 0; b_i < replaced_code_len; b_i++) { const uint8_t b = replaced_code[b_i];
                E(b);
            }
            // 6B          RTL
            E(0x6B);

            // player:
            // ; total damage taken
            // 48          PHA
            E(0x48);
            // 18          CLC
            E(0x18);
            // 6F xx xx xx ADC TOTAL_DAMAGE_TAKEN_24BIT
            E(0x6F);
            E((uint8_t)W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT);
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT >> 8));
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT >> 16));
            // CMP #2710
            E(0xC9);
            E(0x10);
            E(0x27);
            // BCC/BLT noOverflow
            E(0x90);
            E(0x0F);
            // 48          PHA
            E(0x48);
            // AF xx xx xx LDA TOTAL_DAMAGE_TAKEN_24BIT+2
            E(0xAF);
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT + 2));
            E((uint8_t)((W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT + 2) >> 8));
            E((uint8_t)((W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT + 2) >> 16));
            // 1A          INC A
            E(0x1A);
            // 8F xx xx xx STA TOTAL_DAMAGE_TAKEN_24BIT+2
            E(0x8F);
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT + 2));
            E((uint8_t)((W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT + 2) >> 8));
            E((uint8_t)((W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT + 2) >> 16));
            // 68          PLA
            E(0x68);
            // SEC
            E(0x38);
            // SBC #2710
            E(0xE9);
            E(0x10);
            E(0x27);
            // noOverflow:
            // 8F xx xx xx STA TOTAL_DAMAGE_TAKEN_24BIT
            E(0x8F);
            E((uint8_t)W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT);
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT >> 8));
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT >> 16));
            // ; now total physical damage taken too
            // 68          PLA
            E(0x68);
            // 18          CLC
            E(0x18);
            // 6F xx xx xx ADC TOTAL_P_DAMAGE_TAKEN_24BIT
            E(0x6F);
            E((uint8_t)W8_RAM_TOTAL_M_DAMAGE_TAKEN_32BIT);
            E((uint8_t)(W8_RAM_TOTAL_M_DAMAGE_TAKEN_32BIT >> 8));
            E((uint8_t)(W8_RAM_TOTAL_M_DAMAGE_TAKEN_32BIT >> 16));
            // CMP #2710
            E(0xC9);
            E(0x10);
            E(0x27);
            // BCC/BLT noPOverflow
            E(0x90);
            E(0x0F);
            // 48          PHA
            E(0x48);
            // AF xx xx xx LDA TOTAL_P_DAMAGE_TAKEN_24BIT+2
            E(0xAF);
            E((uint8_t)(W8_RAM_TOTAL_M_DAMAGE_TAKEN_32BIT + 2));
            E((uint8_t)((W8_RAM_TOTAL_M_DAMAGE_TAKEN_32BIT + 2) >> 8));
            E((uint8_t)((W8_RAM_TOTAL_M_DAMAGE_TAKEN_32BIT + 2) >> 16));
            // 1A          INC A
            E(0x1A);
            // 8F xx xx xx STA TOTAL_P_DAMAGE_TAKEN_24BIT+2
            E(0x8F);
            E((uint8_t)(W8_RAM_TOTAL_M_DAMAGE_TAKEN_32BIT + 2));
            E((uint8_t)((W8_RAM_TOTAL_M_DAMAGE_TAKEN_32BIT + 2) >> 8));
            E((uint8_t)((W8_RAM_TOTAL_M_DAMAGE_TAKEN_32BIT + 2) >> 16));
            // 68          PLA
            E(0x68);
            // SEC
            E(0x38);
            // SBC #2710
            E(0xE9);
            E(0x10);
            E(0x27);
            // noPOverflow:
            // 8F xx xx xx STA TOTAL_DAMAGE_TAKEN_24BIT
            E(0x8F);
            E((uint8_t)W8_RAM_TOTAL_M_DAMAGE_TAKEN_32BIT);
            E((uint8_t)(W8_RAM_TOTAL_M_DAMAGE_TAKEN_32BIT >> 8));
            E((uint8_t)(W8_RAM_TOTAL_M_DAMAGE_TAKEN_32BIT >> 16));
            // ; end
            for (int b_i = 0; b_i < replaced_code_len; b_i++) { const uint8_t b = replaced_code[b_i];
                E(b);
            }
            // 6B          RTL
            E(0x6B);
}

int sr_w8_hack_statistics(struct sr_ctx *c) {
            // MOPPLE: these are pretty busted, seems like maybe something is overwriting them in RAM? should be fixed someday
            // they are currently only used for procgen modes, and only ancient cave really puts anything in it


            // ///////////////////////////////////////////////////////
            // init all values to zero
            // ///////////////////////////////////////////////////////

            int NUM_STATISTICS_BYTES = 128;
            // this is when player characters change, should only execute at the beginning of the ac run
            // $01/ECAE 9D 00 E0    STA $E000,x[$7E:E400]   A:0001 X:0400 Y:0000 P:envmxdIzc
            // $01/ECB1 A4 3A       LDY $3A    [$00:003A]   A:0001 X:0200 Y:0000 P:envmxdIzc
            P(0x1ECAE, 0x22);
            P(0x1ECAF, (uint8_t)(c->working_offset));
            P(0x1ECB0, (uint8_t)(c->working_offset >> 8));
            P(0x1ECB1, (uint8_t)((c->working_offset >> 16) + 0xC0));
            P(0x1ECB2, 0xEA);

            // replace with:
            // PHA
            E(0x48);
            // PHX
            E(0xDA);
            // PHY
            E(0x5A);
            // SEP 20
            E(0xE2);
            E(0x20);
            // LDA #00
            E(0xA9);
            E(0x00);
            // STA $STATISTICS_BASE_OFFSET
            E(0x8F);
            E((uint8_t)W8_RAM_STATISTICS_BASE_OFFSET);
            E((uint8_t)(W8_RAM_STATISTICS_BASE_OFFSET >> 8));
            E((uint8_t)(W8_RAM_STATISTICS_BASE_OFFSET >> 16));
            // REP 20
            E(0xC2);
            E(0x20);
            // LDX #STATISTICS_BASE_OFFSET
            E(0xA2);
            E((uint8_t)W8_RAM_STATISTICS_BASE_OFFSET);
            E((uint8_t)(W8_RAM_STATISTICS_BASE_OFFSET >> 8));
            // LDY #STATISTICS_BASE_OFFSET+1
            E(0xA0);
            E((uint8_t)(W8_RAM_STATISTICS_BASE_OFFSET + 1));
            E((uint8_t)((W8_RAM_STATISTICS_BASE_OFFSET + 1) >> 8));
            // LDA #NUM_STATISTICS_BYTES-2
            E(0xA9);
            E((uint8_t)(NUM_STATISTICS_BYTES - 2));
            E((uint8_t)((NUM_STATISTICS_BYTES - 2) >> 8));
            // MVN 7E 7E
            E(0x54);
            E(0x7E);
            E(0x7E);
            // use 7ea208 as rng for the moon phase screen; normally it depends on save files, but procgen modes have no
            // saving, so it turns out the same every time
            // AF 0B A2 7E    LDA $7EA20B
            E(0xAF);
            E(0x0B);
            E(0xA2);
            E(0x7E);
            // 8F 01 60 30    STA $306001 - write to saveram checksum, which is used for the moon phase/stars
            E(0x8F);
            E(0x01);
            E(0x60);
            E(0x30);
            // PLY
            E(0x7A);
            // PLX
            E(0xFA);
            // PLA
            E(0x68);
            // 9D 00 E0    STA $E000,x ; removed code
            E(0x9D);
            E(0x00);
            E(0xE0);
            // A4 3A       LDY $3A     ; removed code
            E(0xA4);
            E(0x3A);
            // 6B          RTL
            E(0x6B);


            // ///////////////////////////////////////////////////////
            // Kills/deaths
            // ///////////////////////////////////////////////////////

            // $C0/4205 9E B1 E1    STZ $E1B1,x[$7E:E9B1]   A:0000 X:0800 Y:0011 P:envMxdIZc
            // $C0/4208 9E B0 E1    STZ $E1B0,x[$7E:E9B0]   A:0000 X:0800 Y:0011 P:envMxdIZc
            // gets called when something dies.  if x 0000, 0200, or 0400, it's a player

            P(0x4205, 0x22);
            P(0x4206, (uint8_t)(c->working_offset));
            P(0x4207, (uint8_t)(c->working_offset >> 8));
            P(0x4208, (uint8_t)((c->working_offset >> 16) + 0xC0));
            P(0x4209, 0xEA);
            P(0x420A, 0xEA);

            // so replace this with:
            // 9E B1 E1    STZ $E1B1,x    ; original A
            E(0x9E);
            E(0xB1);
            E(0xE1);
            // 9E B0 E1    STZ $E1B0,x    ; original B
            E(0x9E);
            E(0xB0);
            E(0xE1);
            // E0 00 06    CPX #0600
            E(0xE0);
            E(0x00);
            E(0x06);
            // B0 xx       BCS/BGE enemy
            E(0xB0);
            E(0x0E);
            // C2 20       REP #20
            E(0xC2);
            E(0x20);
            // AF xx xx xx LDA $PLAYER_KILLS_OFFSET_16BIT
            E(0xAF);
            E((uint8_t)W8_RAM_PLAYER_KILLS_OFFSET_16BIT);
            E((uint8_t)(W8_RAM_PLAYER_KILLS_OFFSET_16BIT >> 8));
            E((uint8_t)(W8_RAM_PLAYER_KILLS_OFFSET_16BIT >> 16));
            // 1A          INC A
            E(0x1A);
            // 8F xx xx xx STA $PLAYER_KILLS_OFFSET_16BIT
            E(0x8F);
            E((uint8_t)W8_RAM_PLAYER_KILLS_OFFSET_16BIT);
            E((uint8_t)(W8_RAM_PLAYER_KILLS_OFFSET_16BIT >> 8));
            E((uint8_t)(W8_RAM_PLAYER_KILLS_OFFSET_16BIT >> 16));
            // E2 20       SEP #20
            E(0xE2);
            E(0x20);
            // 6B          RTL
            E(0x6B);
            // enemy:
            // C2 20       REP #20
            E(0xC2);
            E(0x20);
            // AF xx xx xx LDA $ENEMY_KILLS_OFFSET_16BIT
            E(0xAF);
            E((uint8_t)W8_RAM_ENEMY_KILLS_OFFSET_16BIT);
            E((uint8_t)(W8_RAM_ENEMY_KILLS_OFFSET_16BIT >> 8));
            E((uint8_t)(W8_RAM_ENEMY_KILLS_OFFSET_16BIT >> 16));
            // 1A          INC A
            E(0x1A);
            // 8F xx xx xx STA $ENEMY_KILLS_OFFSET_16BIT
            E(0x8F);
            E((uint8_t)W8_RAM_ENEMY_KILLS_OFFSET_16BIT);
            E((uint8_t)(W8_RAM_ENEMY_KILLS_OFFSET_16BIT >> 8));
            E((uint8_t)(W8_RAM_ENEMY_KILLS_OFFSET_16BIT >> 16));
            // E2 20       SEP #20
            E(0xE2);
            E(0x20);
            // 6B          RTL
            E(0x6B);


            // chest drop
            // $C0 / 42A8 DE 82 E1    DEC $E182,x[$7E:E982]   A:0F84 X:0800 Y:007F P:eNvMxdIzC
            // $C0 / 42AB A9 80       LDA #$80                A:0F84 X:0800 Y:007F P:eNvMxdIzC
            P(0x42A6, 0xEA);
            P(0x42A7, 0xEA);
            P(0x42A8, 0x22);
            P(0x42A9, (uint8_t)(c->working_offset));
            P(0x42AA, (uint8_t)(c->working_offset >> 8));
            P(0x42AB, (uint8_t)((c->working_offset >> 16) + 0xC0));
            P(0x42AC, 0xEA);

            // replace with:
            // REP 20
            E(0xC2);
            E(0x20);
            // AF xx xx xx LDA $NUM_CHESTS_OFFSET_16BIT
            E(0xAF);
            E((uint8_t)W8_RAM_NUM_CHESTS_OFFSET_16BIT);
            E((uint8_t)(W8_RAM_NUM_CHESTS_OFFSET_16BIT >> 8));
            E((uint8_t)(W8_RAM_NUM_CHESTS_OFFSET_16BIT >> 16));
            // 1A          INC A
            E(0x1A);
            // 8F xx xx xx STA $NUM_CHESTS_OFFSET_16BIT
            E(0x8F);
            E((uint8_t)W8_RAM_NUM_CHESTS_OFFSET_16BIT);
            E((uint8_t)(W8_RAM_NUM_CHESTS_OFFSET_16BIT >> 8));
            E((uint8_t)(W8_RAM_NUM_CHESTS_OFFSET_16BIT >> 16));
            // E2 20       SEP #20
            E(0xE2);
            E(0x20);
            // A9 80       LDA #$80    ; replaced code
            E(0xA9);
            E(0x80);
            // 6B          RTL
            E(0x6B);

            // ///////////////////////////////////////////////////////
            // Walking distance
            // ///////////////////////////////////////////////////////

            // (x axis movement)
            //$00 / D8EF 9D 02 E0    STA $E002,x[$7E:EA02]   A:0263 X:0A00 Y:0232 P:eNvmxdIzc
            //$00 / D8F2 E0 00 06    CPX #$0600              A:0142 X:0800 Y:0202 P:envmxdIzC
            P(0xD8EF, 0x22);
            P(0xD8F0, (uint8_t)(c->working_offset));
            P(0xD8F1, (uint8_t)(c->working_offset >> 8));
            P(0xD8F2, (uint8_t)((c->working_offset >> 16) + 0xC0));
            P(0xD8F3, 0xEA);
            P(0xD8F4, 0xEA);
            // E0 00 06    CPX #0600
            E(0xE0);
            E(0x00);
            E(0x06);
            // 90 xx       BCC/BLT player
            E(0x90);
            E(0x07);
            // (replaced code)
            // 9D 02 E0    STA $E002,x
            E(0x9D);
            E(0x02);
            E(0xE0);
            // E0 00 06    CPX #$0600
            E(0xE0);
            E(0x00);
            E(0x06);
            // 6B          RTL
            E(0x6B);
            // player:
            // 48          PHA
            E(0x48);
            // E2 20       SEP 20
            E(0xE2);
            E(0x20);
            // BD 2C E0    LDA $E02C,x
            E(0xBD);
            E(0x2C);
            E(0xE0);
            // F0          BEQ npc
            E(0xF0);
            E(0x33);
            // C2 20       REP 20
            E(0xC2);
            E(0x20);
            // 68          PLA
            E(0x68);
            // 48          PHA
            E(0x48);
            // SEP 20
            E(0xE2);
            E(0x20);
            // 38          SEC
            E(0x38);
            // FD 02 E0    SBC $E002,x
            E(0xFD);
            E(0x02);
            E(0xE0);
            // 10 xx       BPL skip
            E(0x10);
            E(0x05);
            // 49 FF FF    EOR #FFFF
            E(0x49);
            E(0xFF);
            // 18          CLC
            E(0x18);
            // 69 01       ADC #01
            E(0x69);
            E(0x01);
            // REP 20
            E(0xC2);
            E(0x20);
            // AND 000F
            E(0x29);
            E(0x0F);
            E(0x00);
            // skip:
            // 18          CLC
            E(0x18);
            // 6F xx xx xx ADC $MOVE_DIST_24BIT
            E(0x6F);
            E((uint8_t)W8_RAM_MOVE_DIST_32BIT);
            E((uint8_t)(W8_RAM_MOVE_DIST_32BIT >> 8));
            E((uint8_t)(W8_RAM_MOVE_DIST_32BIT >> 16));
            // CMP #2710
            E(0xC9);
            E(0x10);
            E(0x27);
            // BCC/BLT noOverflow
            E(0x90);
            E(0x0F);
            // 48          PHA
            E(0x48);
            // AF xx xx xx LDA $MOVE_DIST_24BIT+2
            E(0xAF);
            E((uint8_t)(W8_RAM_MOVE_DIST_32BIT +2));
            E((uint8_t)((W8_RAM_MOVE_DIST_32BIT +2) >> 8));
            E((uint8_t)((W8_RAM_MOVE_DIST_32BIT +2) >> 16));
            // 1A          INA
            E(0x1A);
            // 8F xx xx xx STA $MOVE_DIST_24BIT+2
            E(0x8F);
            E((uint8_t)(W8_RAM_MOVE_DIST_32BIT + 2));
            E((uint8_t)((W8_RAM_MOVE_DIST_32BIT + 2) >> 8));
            E((uint8_t)((W8_RAM_MOVE_DIST_32BIT + 2) >> 16));
            // 68          PLA
            E(0x68);
            // SEC
            E(0x38);
            // SBC #2710
            E(0xE9);
            E(0x10);
            E(0x27);
            // noOverflow:
            // 8F xx xx xx STA $MOVE_DIST_24BIT
            E(0x8F);
            E((uint8_t)W8_RAM_MOVE_DIST_32BIT);
            E((uint8_t)(W8_RAM_MOVE_DIST_32BIT >> 8));
            E((uint8_t)(W8_RAM_MOVE_DIST_32BIT >> 16));
            // npc:
            // C2 20       REP 20
            E(0xC2);
            E(0x20);
            // 68          PLA
            E(0x68);
            // (replaced code)
            // 9D 02 E0    STA $E002,x
            E(0x9D);
            E(0x02);
            E(0xE0);
            // E0 00 06    CPX #$0600
            E(0xE0);
            E(0x00);
            E(0x06);
            // 6B          RTL
            E(0x6B);

            // (y axis movement)
            // $00 / D93E 9D 04 E0    STA $E004,x[$7E:E804]   A:01C9 X:0800 Y:01C2 P:eNvmxdIzc
            // $00 / D941 E0 00 06    CPX #$0600              A:01D3 X:0800 Y:0202 P:eNvmxdIzc
            P(0xD93E, 0x22);
            P(0xD93F, (uint8_t)(c->working_offset));
            P(0xD940, (uint8_t)(c->working_offset >> 8));
            P(0xD941, (uint8_t)((c->working_offset >> 16) + 0xC0));
            P(0xD942, 0xEA);
            P(0xD943, 0xEA);
            // for y
            // E0 00 06    CPX #0600
            E(0xE0);
            E(0x00);
            E(0x06);
            // 90 xx       BCC/BLT player
            E(0x90);
            E(0x07);
            // (replaced code)
            // 9D 02 E0    STA $E004,x
            E(0x9D);
            E(0x04);
            E(0xE0);
            // E0 00 06    CPX #$0600
            E(0xE0);
            E(0x00);
            E(0x06);
            // 6B          RTL
            E(0x6B);
            //
            // player:
            // 48          PHA
            E(0x48);
            // E2 20       SEP 20
            E(0xE2);
            E(0x20);
            // BD 2C E0    LDA $E02C,x
            E(0xBD);
            E(0x2C);
            E(0xE0);
            // F0          BEQ npc
            E(0xF0);
            E(0x33);
            // C2 20       REP 20
            E(0xC2);
            E(0x20);
            // 68          PLA
            E(0x68);
            // 48          PHA
            E(0x48);
            // *****
            E(0xE2);
            E(0x20);
            // *****
            // 38          SEC
            E(0x38);
            // FD 02 E0    SBC $E004,x
            E(0xFD);
            E(0x04);
            E(0xE0);
            // 10 xx       BPL skip
            E(0x10);
            E(0x05);
            // 49 FF FF    EOR #FFFF
            E(0x49);
            E(0xFF);
            // 18          CLC
            E(0x18);
            // 69 01       ADC #01
            E(0x69);
            E(0x01);
            // REP 20
            E(0xC2);
            E(0x20);
            // AND 000F
            E(0x29);
            E(0x0F);
            E(0x00);
            // skip:
            // 18          CLC
            E(0x18);
            // 6F xx xx xx ADC $MOVE_DIST_24BIT
            E(0x6F);
            E((uint8_t)W8_RAM_MOVE_DIST_32BIT);
            E((uint8_t)(W8_RAM_MOVE_DIST_32BIT >> 8));
            E((uint8_t)(W8_RAM_MOVE_DIST_32BIT >> 16));
            // CMP #2710
            E(0xC9);
            E(0x10);
            E(0x27);
            // BCC/BLT noOverflow
            E(0x90);
            E(0x0F);

            // 48          PHA
            E(0x48);
            // AF xx xx xx LDA $MOVE_DIST_24BIT+2
            E(0xAF);
            E((uint8_t)(W8_RAM_MOVE_DIST_32BIT + 2));
            E((uint8_t)((W8_RAM_MOVE_DIST_32BIT + 2) >> 8));
            E((uint8_t)((W8_RAM_MOVE_DIST_32BIT + 2) >> 16));
            // 1A          INA
            E(0x1A);
            // 8F xx xx xx STA $MOVE_DIST_24BIT+2
            E(0x8F);
            E((uint8_t)(W8_RAM_MOVE_DIST_32BIT + 2));
            E((uint8_t)((W8_RAM_MOVE_DIST_32BIT + 2) >> 8));
            E((uint8_t)((W8_RAM_MOVE_DIST_32BIT + 2) >> 16));
            // 68          PLA
            E(0x68);
            // SEC
            E(0x38);
            // SBC #2710
            E(0xE9);
            E(0x10);
            E(0x27);
            // noOverflow:
            // 8F xx xx xx STA $MOVE_DIST_24BIT
            E(0x8F);
            E((uint8_t)W8_RAM_MOVE_DIST_32BIT);
            E((uint8_t)(W8_RAM_MOVE_DIST_32BIT >> 8));
            E((uint8_t)(W8_RAM_MOVE_DIST_32BIT >> 16));
            // npc:
            // C2 20       REP 20
            E(0xC2);
            E(0x20);
            // 68          PLA
            E(0x68);
            // (replaced code)
            // 9D 02 E0    STA $E004,x
            E(0x9D);
            E(0x04);
            E(0xE0);
            // E0 00 06    CPX #$0600
            E(0xE0);
            E(0x00);
            E(0x06);
            // 6B          RTL
            E(0x6B);


            // ///////////////////////////////////////////////////////
            // Physical damage dealt/taken
            // ///////////////////////////////////////////////////////

            // $C0/51E1 9D F1 E1    STA $E1F1,x[$7E:E1F1]   A:000A X:0000 Y:0600 P:envmxdIzc (physical damage)
            // $C0/51E4 E2 20       SEP #$20                A:000A X:0000 Y:0600 P:envmxdIzc
            P(0x51E1, 0x22);
            P(0x51E2, (uint8_t)(c->working_offset));
            P(0x51E3, (uint8_t)(c->working_offset >> 8));
            P(0x51E4, (uint8_t)((c->working_offset >> 16) + 0xC0));
            P(0x51E5, 0xEA);

            // replaced code A
            // 9D F1 E1    STA $E1F1,x
            E(0x9D);
            E(0xF1);
            E(0xE1);
            // E0 00 06    CPX #$0600
            E(0xE0);
            E(0x00);
            E(0x06);
            // 90 xx       BCC/BLT player
            E(0x90);
            E(0x3F);
            // (enemy)
            // ; total damage dealt
            // 48          PHA
            E(0x48);
            // 18          CLC
            E(0x18);
            // 6F xx xx xx ADC TOTAL_DAMAGE_DEALT_24BIT
            E(0x6F);
            E((uint8_t)W8_RAM_TOTAL_DAMAGE_DEALT_32BIT);
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_DEALT_32BIT >> 8));
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_DEALT_32BIT >> 16));
            // CMP #2710
            E(0xC9);
            E(0x10);
            E(0x27);
            // BCC/BLT noOverflow
            E(0x90);
            E(0x0F);
            // 48          PHA
            E(0x48);
            // AF xx xx xx LDA TOTAL_DAMAGE_DEALT_24BIT+2
            E(0xAF);
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_DEALT_32BIT + 2));
            E((uint8_t)((W8_RAM_TOTAL_DAMAGE_DEALT_32BIT + 2) >> 8));
            E((uint8_t)((W8_RAM_TOTAL_DAMAGE_DEALT_32BIT + 2) >> 16));
            // 1A          INC A
            E(0x1A);
            // 8F xx xx xx STA TOTAL_DAMAGE_DEALT_24BIT+2
            E(0x8F);
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_DEALT_32BIT + 2));
            E((uint8_t)((W8_RAM_TOTAL_DAMAGE_DEALT_32BIT + 2) >> 8));
            E((uint8_t)((W8_RAM_TOTAL_DAMAGE_DEALT_32BIT + 2) >> 16));
            // 68          PLA
            E(0x68);
            // SEC
            E(0x38);
            // SBC #2710
            E(0xE9);
            E(0x10);
            E(0x27);
            // noOverflow:
            // 8F xx xx xx STA TOTAL_DAMAGE_DEALT_24BIT
            E(0x8F);
            E((uint8_t)W8_RAM_TOTAL_DAMAGE_DEALT_32BIT);
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_DEALT_32BIT >> 8));
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_DEALT_32BIT >> 16));
            // ; now total physical damage dealt too
            // 68          PLA
            E(0x68);
            // 18          CLC
            E(0x18);
            // 6F xx xx xx ADC TOTAL_P_DAMAGE_DEALT_24BIT
            E(0x6F);
            E((uint8_t)W8_RAM_TOTAL_P_DAMAGE_DEALT_32BIT);
            E((uint8_t)(W8_RAM_TOTAL_P_DAMAGE_DEALT_32BIT >> 8));
            E((uint8_t)(W8_RAM_TOTAL_P_DAMAGE_DEALT_32BIT >> 16));
            // CMP #2710
            E(0xC9);
            E(0x10);
            E(0x27);
            // BCC/BLT noPOverflow
            E(0x90);
            E(0x0F);
            // 48          PHA
            E(0x48);
            // AF xx xx xx LDA TOTAL_P_DAMAGE_DEALT_24BIT+2
            E(0xAF);
            E((uint8_t)(W8_RAM_TOTAL_P_DAMAGE_DEALT_32BIT + 2));
            E((uint8_t)((W8_RAM_TOTAL_P_DAMAGE_DEALT_32BIT + 2) >> 8));
            E((uint8_t)((W8_RAM_TOTAL_P_DAMAGE_DEALT_32BIT + 2) >> 16));
            // 1A          INC A
            E(0x1A);
            // 8F xx xx xx STA TOTAL_P_DAMAGE_DEALT_24BIT+2
            E(0x8F);
            E((uint8_t)(W8_RAM_TOTAL_P_DAMAGE_DEALT_32BIT + 2));
            E((uint8_t)((W8_RAM_TOTAL_P_DAMAGE_DEALT_32BIT + 2) >> 8));
            E((uint8_t)((W8_RAM_TOTAL_P_DAMAGE_DEALT_32BIT + 2) >> 16));
            // 68          PLA
            E(0x68);
            // SEC
            E(0x38);
            // SBC #2710
            E(0xE9);
            E(0x10);
            E(0x27);
            // noPOverflow:
            // 8F xx xx xx STA TOTAL_DAMAGE_DEALT_24BIT
            E(0x8F);
            E((uint8_t)W8_RAM_TOTAL_P_DAMAGE_DEALT_32BIT);
            E((uint8_t)(W8_RAM_TOTAL_P_DAMAGE_DEALT_32BIT >> 8));
            E((uint8_t)(W8_RAM_TOTAL_P_DAMAGE_DEALT_32BIT >> 16));
            // ; end
            // E2 20       SEP #$20
            E(0xE2);
            E(0x20);
            // 6B          RTL
            E(0x6B);

            // player:
            // ; total damage taken
            // 48          PHA
            E(0x48);
            // 18          CLC
            E(0x18);
            // 6F xx xx xx ADC TOTAL_DAMAGE_TAKEN_24BIT
            E(0x6F);
            E((uint8_t)W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT);
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT >> 8));
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT >> 16));
            // CMP #2710
            E(0xC9);
            E(0x10);
            E(0x27);
            // BCC/BLT noOverflow
            E(0x90);
            E(0x0F);
            // 48          PHA
            E(0x48);
            // AF xx xx xx LDA TOTAL_DAMAGE_TAKEN_24BIT+2
            E(0xAF);
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT + 2));
            E((uint8_t)((W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT + 2) >> 8));
            E((uint8_t)((W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT + 2) >> 16));
            // 1A          INC A
            E(0x1A);
            // 8F xx xx xx STA TOTAL_DAMAGE_TAKEN_24BIT+2
            E(0x8F);
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT + 2));
            E((uint8_t)((W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT + 2) >> 8));
            E((uint8_t)((W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT + 2) >> 16));
            // 68          PLA
            E(0x68);
            // SEC
            E(0x38);
            // SBC #2710
            E(0xE9);
            E(0x10);
            E(0x27);
            // noOverflow:
            // 8F xx xx xx STA TOTAL_DAMAGE_TAKEN_24BIT
            E(0x8F);
            E((uint8_t)W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT);
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT >> 8));
            E((uint8_t)(W8_RAM_TOTAL_DAMAGE_TAKEN_32BIT >> 16));
            // ; now total physical damage taken too
            // 68          PLA
            E(0x68);
            // 18          CLC
            E(0x18);
            // 6F xx xx xx ADC TOTAL_P_DAMAGE_TAKEN_24BIT
            E(0x6F);
            E((uint8_t)W8_RAM_TOTAL_P_DAMAGE_TAKEN_32BIT);
            E((uint8_t)(W8_RAM_TOTAL_P_DAMAGE_TAKEN_32BIT >> 8));
            E((uint8_t)(W8_RAM_TOTAL_P_DAMAGE_TAKEN_32BIT >> 16));
            // CMP #2710
            E(0xC9);
            E(0x10);
            E(0x27);
            // BCC/BLT noPOverflow
            E(0x90);
            E(0x0F);
            // 48          PHA
            E(0x48);
            // AF xx xx xx LDA TOTAL_P_DAMAGE_TAKEN_24BIT+2
            E(0xAF);
            E((uint8_t)(W8_RAM_TOTAL_P_DAMAGE_TAKEN_32BIT + 2));
            E((uint8_t)((W8_RAM_TOTAL_P_DAMAGE_TAKEN_32BIT + 2) >> 8));
            E((uint8_t)((W8_RAM_TOTAL_P_DAMAGE_TAKEN_32BIT + 2) >> 16));
            // 1A          INC A
            E(0x1A);
            // 8F xx xx xx STA TOTAL_P_DAMAGE_TAKEN_24BIT+2
            E(0x8F);
            E((uint8_t)(W8_RAM_TOTAL_P_DAMAGE_TAKEN_32BIT + 2));
            E((uint8_t)((W8_RAM_TOTAL_P_DAMAGE_TAKEN_32BIT + 2) >> 8));
            E((uint8_t)((W8_RAM_TOTAL_P_DAMAGE_TAKEN_32BIT + 2) >> 16));
            // 68          PLA
            E(0x68);
            // SEC
            E(0x38);
            // SBC #2710
            E(0xE9);
            E(0x10);
            E(0x27);
            // noPOverflow:
            // 8F xx xx xx STA TOTAL_DAMAGE_TAKEN_24BIT
            E(0x8F);
            E((uint8_t)W8_RAM_TOTAL_P_DAMAGE_TAKEN_32BIT);
            E((uint8_t)(W8_RAM_TOTAL_P_DAMAGE_TAKEN_32BIT >> 8));
            E((uint8_t)(W8_RAM_TOTAL_P_DAMAGE_TAKEN_32BIT >> 16));
            // ; end
            // E2 20       SEP #$20
            E(0xE2);
            E(0x20);
            // 6B          RTL
            E(0x6B);


            // ///////////////////////////////////////////////////////
            // Magical damage dealt/taken
            // ///////////////////////////////////////////////////////

            // $C8/EB90 9D F1 E1    STA $E1F1,x[$7E:E1F1]   A:0028 X:0000 Y:0800 P:eNvmxdIzc (magical damage)
            // $C8/EB93 A5 A4       LDA $A4    [$00:03A4]   A:0029 X:0000 Y:0800 P:eNvmxdIzc

            // $C8/E9F6 9D F1 E1    STA $E1F1,x[$7E:E1F1]   A:000B X:0000 Y:0A00 P:envmxdIzc also for burst
            // $C8/E9F9 E2 20       SEP #$20                A:0013 X:0000 Y:0A00 P:envmxdIzc

            // energy absorb, etc
            P(0x8EB90, 0x22);
            P(0x8EB91, (uint8_t)(c->working_offset));
            P(0x8EB92, (uint8_t)(c->working_offset >> 8));
            P(0x8EB93, (uint8_t)((c->working_offset >> 16) + 0xC0));
            P(0x8EB94, 0xEA);

            st_inject_magic_damage_tracker(c, BYTES(0xA5, 0xA4));

            // burst, exploder
            P(0x8E9F6, 0x22);
            P(0x8E9F7, (uint8_t)(c->working_offset));
            P(0x8E9F8, (uint8_t)(c->working_offset >> 8));
            P(0x8E9F9, (uint8_t)((c->working_offset >> 16) + 0xC0));
            P(0x8E9FA, 0xEA);

            st_inject_magic_damage_tracker(c, BYTES(0xE2, 0x20));

            return 1;
}



/* upstream hacks/bossrush/InstantWeaponLevels.cs */

int sr_w8_hack_instant_weapon_levels(struct sr_ctx *c) {
            // initial charge level should be 1? do in char init somewhere - add to StartingLevel hack

            // 7EE1Cx for boy weapon levels .. 0-3, one nibble each
            // 7EE1Dx is the partial amount .. 0-7
            // 7EE1E8 is boy player's current weapon
            // 7ECC55 sword level
            // here's watts leveling up the boomerang in 5A:
            // $C0/7BF4 9F 00 CC 7E STA $7ECC00,x[$7E:CC5A] A:00C1 X:005A Y:00FF P:eNvMxdIzC
            P(0x7BF4, 0x22);
            P(0x7BF5, (uint8_t)(c->working_offset));
            P(0x7BF6, (uint8_t)(c->working_offset >> 8));
            P(0x7BF7, (uint8_t)((c->working_offset >> 16) + 0xC0));


            // replaced bit to write weapon level
            // 9F 00 CC 7E STA $7ECC00,x
            E(0x9F);
            E(0x00);
            E(0xCC);
            E(0x7E);

            // - preserve these; they seem to be used again except maybe y
            // PHA
            E(0x48);
            // PHX
            E(0xDA);
            // PHY
            E(0x5A);

            // TXA
            E(0x8A);
            // SEC
            E(0x38);
            // SBC #54 - get weapon type index
            E(0xE9);
            E(0x54);
            // LDY #0000
            E(0xA0);
            E(0x00);
            E(0x00);
            // loop:
            // CMP #02
            E(0xC9);
            E(0x02);
            // BLT out
            E(0x90);
            E(0x06);
            // INY
            E(0xC8);
            // SEC
            E(0x38);
            // SBC #02
            E(0xE9);
            E(0x02);
            // BRA loop
            E(0x80);
            E(0xF6);
            // out:
            // - now y is the byte index, and A is the nibble index
            // CMP #00
            E(0xC9);
            E(0x00);
            // BNE oddNibble
            E(0xD0);
            E(0x4F);

            // evenNibble:
            // PHX [1]
            E(0xDA);
            // TYX [1]
            E(0xBB);
            // - remove the existing data
            // LDA 7EE1C0,X [4]
            E(0xBF);
            E(0xC0);
            E(0xE1);
            E(0x7E);
            // AND #0F      [2]
            E(0x29);
            E(0x0F);
            // STA 7EE1C0,X [4]
            E(0x9F);
            E(0xC0);
            E(0xE1);
            E(0x7E);
            // LDA 7EE3C0,X [4]
            E(0xBF);
            E(0xC0);
            E(0xE3);
            E(0x7E);
            // AND #0F      [2]
            E(0x29);
            E(0x0F);
            // STA 7EE3C0,X [4]
            E(0x9F);
            E(0xC0);
            E(0xE3);
            E(0x7E);
            // LDA 7EE5C0,X [4]
            E(0xBF);
            E(0xC0);
            E(0xE5);
            E(0x7E);
            // AND #0F      [2]
            E(0x29);
            E(0x0F);
            // STA 7EE5C0,X [4]
            E(0x9F);
            E(0xC0);
            E(0xE5);
            E(0x7E);
            // PLX          [1]
            E(0xFA);
            // - now we need to load the actual weapon level
            // LDA $7ECC00,x[4]
            E(0xBF);
            E(0x00);
            E(0xCC);
            E(0x7E);
            // INC A        [1]
            E(0x1A);
            // AND #0F      [2]
            E(0x29);
            E(0x0F);

            // ***
            // CMP #08
            E(0xC9);
            E(0x08);
            // BLT over (2)
            E(0x90);
            E(0x02);
            // LDA #08
            E(0xA9);
            E(0x08);
            // ***

            // - store in upper nibble
            // ASL          [1]
            E(0x0A);
            // ASL          [1]
            E(0x0A);
            // ASL          [1]
            E(0x0A);
            // ASL          [1]
            E(0x0A);
            // PHX          [1]
            E(0xDA);
            // TYX          [1]
            E(0xBB);
            // ORA 7EE1C0,X [4]
            E(0x1F);
            E(0xC0);
            E(0xE1);
            E(0x7E);
            // STA 7EE1C0,X [4]
            E(0x9F);
            E(0xC0);
            E(0xE1);
            E(0x7E);
            // ORA 7EE3C0,X [4]
            E(0x1F);
            E(0xC0);
            E(0xE3);
            E(0x7E);
            // STA 7EE3C0,X [4]
            E(0x9F);
            E(0xC0);
            E(0xE3);
            E(0x7E);
            // ORA 7EE5C0,X [4]
            E(0x1F);
            E(0xC0);
            E(0xE5);
            E(0x7E);
            // STA 7EE5C0,X [4]
            E(0x9F);
            E(0xC0);
            E(0xE5);
            E(0x7E);
            // PLX          [1]
            E(0xFA);
            // BRA end      [2]
            E(0x80);
            E(0x49);
            //              [73 dec, 49+6 hex]

            // oddNibble:
            // PHX
            E(0xDA);
            // TYX
            E(0xBB);
            // - remove the existing data
            // LDA 7EE1C0,X
            E(0xBF);
            E(0xC0);
            E(0xE1);
            E(0x7E);
            // AND #F0
            E(0x29);
            E(0xF0);
            // STA 7EE1C0,X
            E(0x9F);
            E(0xC0);
            E(0xE1);
            E(0x7E);
            // LDA 7EE3C0,X
            E(0xBF);
            E(0xC0);
            E(0xE3);
            E(0x7E);
            // AND #F0
            E(0x29);
            E(0xF0);
            // STA 7EE3C0,X
            E(0x9F);
            E(0xC0);
            E(0xE3);
            E(0x7E);
            // LDA 7EE5C0,X
            E(0xBF);
            E(0xC0);
            E(0xE5);
            E(0x7E);
            // AND #F0
            E(0x29);
            E(0xF0);
            // STA 7EE5C0,X
            E(0x9F);
            E(0xC0);
            E(0xE5);
            E(0x7E);
            // PLX
            E(0xFA);
            // - now we need to load the actual weapon level
            // LDA $7ECC00,x
            E(0xBF);
            E(0x00);
            E(0xCC);
            E(0x7E);
            // INC A
            E(0x1A);
            // AND #0F
            E(0x29);
            E(0x0F);

            // ***
            // CMP #08
            E(0xC9);
            E(0x08);
            // BLT over (2)
            E(0x90);
            E(0x02);
            // LDA #08
            E(0xA9);
            E(0x08);
            // ***

            // - store in lower nibble
            // PHX
            E(0xDA);
            // TYX
            E(0xBB);
            // ORA 7EE1C0,X
            E(0x1F);
            E(0xC0);
            E(0xE1);
            E(0x7E);
            // STA 7EE1C0,X
            E(0x9F);
            E(0xC0);
            E(0xE1);
            E(0x7E);
            // ORA 7EE3C0,X
            E(0x1F);
            E(0xC0);
            E(0xE3);
            E(0x7E);
            // STA 7EE3C0,X
            E(0x9F);
            E(0xC0);
            E(0xE3);
            E(0x7E);
            // ORA 7EE5C0,X
            E(0x1F);
            E(0xC0);
            E(0xE5);
            E(0x7E);
            // STA 7EE5C0,X
            E(0x9F);
            E(0xC0);
            E(0xE5);
            E(0x7E);
            // PLX
            E(0xFA);

            //
            // end:
            // PLY
            E(0x7A);
            // PLX
            E(0xFA);
            // PLA
            E(0x68);
            // RTL
            E(0x6B);

            return 1;
}



/* upstream hacks/bossrush/MpAbsorbLimiter.cs */

int sr_w8_hack_mp_absorb_limiter(struct sr_ctx *c) {
            if (!sr_settings_get_bool(c->settings, "brLimitMpAbsorb")) return 0;

            const int limit = 3;

            // $C8/EC8A 65 A4       ADC $A4    [$00:03A4]   A:0000 X:0600 Y:0400 P:envMxdIZc
            // $C8 / EC8C 99 F6 E1    STA $E1F6,y[$7E:E5F6]   A: 000F X: 0600 Y: 0400 P: envMxdIzc
            // ^ writing the amount of "damage" that mp steal does.
            P(0x8EC8A, 0x22);
            P(0x8EC8B, (uint8_t)(c->working_offset));
            P(0x8EC8C, (uint8_t)(c->working_offset >> 8));
            P(0x8EC8D, (uint8_t)((c->working_offset >> 16) + 0xC0));
            P(0x8EC8E, 0xEA);

            // code we took out
            E(0x65);
            E(0xA4);

            // cmp #limit
            E(0xC9);
            E(limit);

            // bcc skip
            E(0x90);
            E(0x02);

            // lda #limit
            E(0xA9);
            E(limit);

            // more code we took out
            E(0x99);
            E(0xF6);
            E(0xE1);

            // rtl
            E(0x6B);

            return 1;
}



/* upstream hacks/bossrush/StartingLevel.cs */

int sr_w8_hack_starting_level(struct sr_ctx *c) {
            // $C0/4E3A 9D 70 E1    STA $E170,x[$7E:E180]   A:0000 X:0010 Y:0001 P:envmxdIzc
            // replace this bit:
            // $C0/4E5A A9 0F       LDA #$0F                A:0000 X:0200 Y:0000 P:envMxdIzc
            // $C0 / 4E5C 9D EC E1    STA $E1EC,x[$7E:E3EC]   A: 000F X: 0200 Y: 0000 P: envMxdIzc
            // with:
            // same shit
            // lda #xx
            // sta e181,x
            // rtl

            int playerLevel = w8_diff_int(c, "startingLevel");

            P(0x4E5A, 0x22);
            P(0x4E5B, (uint8_t)(c->working_offset));
            P(0x4E5C, (uint8_t)(c->working_offset >> 8));
            P(0x4E5D, (uint8_t)((c->working_offset >> 16) + 0xC0));
            P(0x4E5E, 0xEA);

            // (removed code)
            // LDA #0F
            E(0xA9);
            E(0x0F);
            // STA $E1EC,x
            E(0x9D);
            E(0xEC);
            E(0xE1);

            // CPX #0600
            E(0xE0);
            E(0x00);
            E(0x06);
            // BCC/BLT over [90 01]
            E(0x90);
            E(0x01);
            // RTL [6B]
            E(0x6B);
            // LDA #level
            E(0xA9);
            E((uint8_t)(playerLevel - 1));
            // STA $E181,x (character level)
            E(0x9D);
            E(0x81);
            E(0xE1);

            // now weapon levels .. 7EE1C0-3 for boy
            // LDA #11
            E(0xA9);
            E(0x11);

            // STA E1C0,x
            E(0x9D);
            E(0xC0);
            E(0xE1);
            // STA E1C1,x
            E(0x9D);
            E(0xC1);
            E(0xE1);
            // STA E1C2,x
            E(0x9D);
            E(0xC2);
            E(0xE1);
            // STA E1C3,x
            E(0x9D);
            E(0xC3);
            E(0xE1);
            // RTL
            E(0x6B);

            return 1;
}



/* upstream hacks/common/procgen/MagicRope.cs */

int sr_w8_hack_magic_rope(struct sr_ctx *c) {
            // replace a small amount of code associated with using consumables, to check for magic rope
            // C1 / DC48: AD F3 19   LDA $19F3
            // C1 / DC4B: 29 03 0A   AND #$0A03
            // change it to execute door 0xFFFF, which the procgen DoorExpansion hack defines as procgen floor entry door

            P(0x1DC48, 0x22);
            P(0x1DC49, (uint8_t)(c->working_offset));
            P(0x1DC4A, (uint8_t)(c->working_offset >> 8));
            P(0x1DC4B, (uint8_t)((c->working_offset >> 16) + 0xC0));
            P(0x1DC4C, 0xEA);
            P(0x1DC4D, 0xEA);
            // (8bit a, 16bit x/y)

            // CMP #06 - magic rope id
            E(0xC9);
            E(0x06);
            // BEQ 07
            E(0xF0);
            E(0x07);
            // (replaced code) LDA $19F3
            E(0xAD);
            E(0xF3);
            E(0x19);
            // (replaced code) AND #$0A03
            E(0x29);
            E(0x03);
            E(0x0A);
            // RTL
            E(0x6B);

            ////////////////////////
            // REP 20
            E(0xC2);
            E(0x20);
            // PHX
            E(0xDA);
            // LDA #FFFF - continue door ID
            E(0xA9);
            E(0xFF);
            E(0xFF);
            // JSL C1E76D - run a door
            E(0x22);
            E(0x6D);
            E(0xE7);
            E(0xC1);
            // PLX
            E(0xFA);
            // SEP 20
            E(0xE2);
            E(0x20);
            // RTL
            E(0x6B);

            // name of magic rope
            struct sr_bytes name_bytes = {0};
            sr_event_text_bytes(c, &name_bytes, "Stuck?");
            for (int i = 0xaa12e; i <= 0xaa137; i++)
            {
                P(i, 0);
            }
            int off = 0xAA12E;
            for (size_t b_i = 0; b_i < name_bytes.len; b_i++) { const uint8_t b = name_bytes.p[b_i];
                P(off++, b);
            }
            return 1;
}


