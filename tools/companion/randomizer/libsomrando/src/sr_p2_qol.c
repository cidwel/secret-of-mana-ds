/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Phase 2: FreeWalking, Speedup, FreeTargetting, BuyMultipleItems.  Author of every upstream hack: Moppleton.  Translated to C 2026-10-18.
 */
#include <math.h>
#include <string.h>

#include "sr_core.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))
#define S(key) sr_settings_get(c->settings, (key))
#define SB(key) sr_settings_get_bool(c->settings, (key))

/* CRAM_STATISTICS_BASE_OFFSET + n */
#define CRAM_STATISTICS_BASE 0x7E0400
#define CRAM_FREE_MOVE_PLAYER_SELECT_BYTE (CRAM_STATISTICS_BASE + 34)
#define CRAM_FREE_MOVE_PLAYER1_COUNTER_16BIT (CRAM_STATISTICS_BASE + 35)
#define CRAM_X_SCREEN_TEMP_LOCATION (CRAM_STATISTICS_BASE + 58)
#define CRAM_Y_SCREEN_TEMP_LOCATION (CRAM_STATISTICS_BASE + 59)
#define CRAM_CONSUMABLE_BUY_MAX (CRAM_STATISTICS_BASE + 60)
#define CRAM_CONSUMABLE_BUY_CURRENT (CRAM_STATISTICS_BASE + 61)
#define CRAM_CONSUMABLE_INPUT_TIMER (CRAM_STATISTICS_BASE + 62)
#define CRAM_BUYING_MULTIPLE_ITEMS (CRAM_STATISTICS_BASE + 63)

/* context.initialValues[addr] = value: ordered (addr, value) pairs kept in the work table */
static void sr_w3_initial_value_set(struct sr_ctx *c, int addr, int value) {
  int a[128];
  int n = sr_work_get_int_array(c, "w3.initialValues", a, 128);
  if (n < 0) n = 0;
  for (int i = 0; i + 1 < n; i += 2)
    if (a[i] == addr) {
      a[i + 1] = value & 0xFF;
      sr_work_set_int_array(c, "w3.initialValues", a, n);
      return;
    }
  if (n + 2 > 128) sr_fail(c, SOMRANDO_ERR_FAILED, "too many initial values");
  a[n++] = addr;
  a[n++] = value & 0xFF;
  sr_work_set_int_array(c, "w3.initialValues", a, n);
}

/* InitialValues (upstream processing/hacks/common/util/InitialValues.cs), called from sr_hack_initial_values */
int sr_w3_initial_values_hack(struct sr_ctx *c) {
  int a[128];
  int n = sr_work_get_int_array(c, "w3.initialValues", a, 128);
  if (n <= 0) return 0;
  /* compare y to 0x7566 like autosave to recognize the starting menu */
  P(0x245A, 0x22);
  P(0x245B, (uint8_t)(c->working_offset));
  P(0x245C, (uint8_t)(c->working_offset >> 8));
  P(0x245D, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x245E, 0xEA);
  P(0x245F, 0xEA);
  static const uint8_t replaced[] = {0xA9, 0x00, 0xEB, 0xB9, 0x00, 0x00};
  E(0xC0); E(0x66); E(0x75);
  E(0xF0); E(0x07);
  for (int i = 0; i < 6; i++) E(replaced[i]);
  E(0x6B);
  for (int i = 0; i + 1 < n; i += 2) {
    E(0xA9); E(a[i + 1]);
    E(0x8F); E((uint8_t)a[i]); E((uint8_t)(a[i] >> 8)); E((uint8_t)(a[i] >> 16));
  }
  for (int i = 0; i < 6; i++) E(replaced[i]);
  E(0x6B);
  return 1;
}

/* vanillaSubrToLongSubr */
static void vanilla_subr_to_long_subr(struct sr_ctx *c, struct sr_bytes *out, int start_addr, int return_addr) {
  for (int i = start_addr; i < return_addr; i++) sr_bytes_push(c, out, c->orig[i]);
  sr_bytes_push(c, out, 0x6B);
}

static void write_walk_speed_check(struct sr_ctx *c, const uint8_t *orig_code, int n_orig, const uint8_t *new_code, int n_new) {
  // PHA
  E(0x48);
  // LDA $7ee02c - boy player number
  E(0xAF);
  E(0x2C);
  E(0xE0);
  E(0x7E);
  // CMP #01
  E(0xC9);
  E(0x01);
  // BNE overp1
  E(0xD0);
  E(0x06);
  // LDA $7ee1b0 - boy status byte including speedup flags
  E(0xAF);
  E(0xB0);
  E(0xE1);
  E(0x7E);
  // BRA end
  E(0x80);
  E(0x12);
  // overp1:
  // LDA $7ee22c - girl player number
  E(0xAF);
  E(0x2C);
  E(0xE2);
  E(0x7E);
  // CMP #01
  E(0xC9);
  E(0x01);
  // BNE overp2
  E(0xD0);
  E(0x06);
  // LDA $7ee3b0 - girl status byte including speedup flags
  E(0xAF);
  E(0xB0);
  E(0xE3);
  E(0x7E);
  // BRA end
  E(0x80);
  E(0x04);
  // LDA $7ee5b0 - sprite status byte including speedup flags
  E(0xAF);
  E(0xB0);
  E(0xE5);
  E(0x7E);
  // end:
  // AND 0x14 to check for speedup buffs
  E(0x29);
  E(0x14);
  // BEQ noChange
  E(0xF0);
  E((uint8_t)(n_new + 2));
  // PLA
  E(0x68);
  // haste is active - use running speed code
  for (int i = 0; i < n_new; i++)
  {
    E(new_code[i]);
  }
  // RTL
  E(0x6B);
  // PLA
  E(0x68);
  // haste is not active - use walking speed code
  for (int i = 0; i < n_orig; i++)
  {
    E(orig_code[i]);
  }
  // RTL
  E(0x6B);
}

/* FreeWalking - upstream processing/hacks/common/qol/FreeWalking.cs */
int sr_hack_free_walking(struct sr_ctx *c) {
  if (!SB("leavePlayersBehind")) return 0;
  // //////////////////////////////////////////////////////////
  // Removal of scrolling limitation
  // //////////////////////////////////////////////////////////
  int teleportTimerInFrames = 60;
  // this STZ stops you from moving horizontally if separated from the other characters - remove it
  //$00 / D9E5 64 1E       STZ $1E[$00:001E]   A: 00C8 X:0400 Y: 0067 P: envMxdIZC
  P(0xD9E5, 0xEA);
  P(0xD9E6, 0xEA);
  // same for moving vertically
  P(0xDA6B, 0xEA);
  P(0xDA6C, 0xEA);
  // //////////////////////////////////////////////////////////
  // Limitation for selecting a character based on screen x/y
  // //////////////////////////////////////////////////////////
  /*
  * replace:
  $00 / BD72 A6 04       LDX $04[$00:0004]       A: 0000 X: 0400 Y: 0000 P: envMxdIZc
  $00 / BD74 9E 2C E0    STZ $E02C,x[$7E:E22C]   A: 0000 X: 0200 Y: 0000 P: envMxdIzc
  
  note that 7EEx2C for a player indicates which player number they are, and is 0 for npc control
  */
  P(0xBD72, 0xEA);
  P(0xBD73, 0x22);
  P(0xBD74, (uint8_t)(c->working_offset));
  P(0xBD75, (uint8_t)(c->working_offset >> 8));
  P(0xBD76, (uint8_t)((c->working_offset >> 16) + 0xC0));
  // REP 20
  E(0xC2);
  E(0x20);
  // LDA 7EE020,X - this is the screen x position of the character
  E(0xBF);
  E(0x20);
  E(0xE0);
  E(0x7E);
  // BMI fixWindowPosition
  E(0x30);
  E(0x18);
  // CMP #00E0
  E(0xC9);
  E(0xE0);
  E(0x00);
  // BCS/BGE fixWindowPosition
  E(0xB0);
  E(0x13);
  // LDA 7EE020,Y - this is the screen y position of the character
  E(0xBF);
  E(0x22);
  E(0xE0);
  E(0x7E);
  // BMI fixWindowPosition
  E(0x30);
  E(0x0D);
  // CMP #00E0
  E(0xC9);
  E(0xE0);
  E(0x00);
  // BCS/BGE fixWindowPosition
  E(0xB0);
  E(0x08);
  // (removed code) LDX $04
  E(0xA6);
  E(0x04);
  // (removed code) STZ $E02C,x
  E(0x9E);
  E(0x2C);
  E(0xE0);
  // SEP 20, since we REP 20'ed earlier
  E(0xE2);
  E(0x20);
  // RTL - vanilla behavior if screen position isn't out of bounds
  E(0x6B);
  // fixWindowPosition:
  // just mark the camera as moving, and every frame we shift toward the target (whichever of 7eex2c is == 1) if it's == 1
  // SEP 20
  E(0xE2);
  E(0x20);
  // LDA #01
  E(0xA9);
  E(0x01);
  // STA FREE_MOVE_PLAYER_SELECT_BYTE
  E(0x8F);
  E((uint8_t)(CRAM_FREE_MOVE_PLAYER_SELECT_BYTE));
  E((uint8_t)(CRAM_FREE_MOVE_PLAYER_SELECT_BYTE >> 8));
  E((uint8_t)(CRAM_FREE_MOVE_PLAYER_SELECT_BYTE >> 16));
  // REP 20
  E(0xC2);
  E(0x20);
  // (removed code) LDX $04
  E(0xA6);
  E(0x04);
  // (removed code) STZ $E02C,x
  E(0x9E);
  E(0x2C);
  E(0xE0);
  // SEP 20
  E(0xE2);
  E(0x20);
  // RTL
  E(0x6B);
  /*
  * replace:
  $00/BD3D BD 00 E0    LDA $E000,x[$7E:E400]   A:7002 X:0400 Y:0000 P:envMxdIzC
  $00/BD40 F0 4D       BEQ $4D[$BD8F]          A:7001 X:0400 Y:0000 P:envMxdIzC
  $00/BD42 30 4B       BMI $4B[$BD8F]          A:7001 X:0400 Y:0000 P:envMxdIzC
  */
  P(0xBD3D, 0x22);
  P(0xBD3E, (uint8_t)(c->working_offset));
  P(0xBD3F, (uint8_t)(c->working_offset >> 8));
  P(0xBD40, (uint8_t)((c->working_offset >> 16) + 0xC0));
  // BNE $BD8F
  P(0xBD41, 0xD0);
  P(0xBD42, 0x4C);
  // NOP
  P(0xBD43, 0xEA);
  // LDA #00
  E(0xA9);
  E(0x00);
  // STA $7ECF1E
  E(0x8F);
  E((uint8_t)(CRAM_FREE_MOVE_PLAYER_SELECT_BYTE));
  E((uint8_t)(CRAM_FREE_MOVE_PLAYER_SELECT_BYTE >> 8));
  E((uint8_t)(CRAM_FREE_MOVE_PLAYER_SELECT_BYTE >> 16));
  // LDA $E000,x[$7E:E000]
  E(0xBD);
  E(0x00);
  E(0xE0);
  // BEQ error
  E(0xF0);
  E(0x1A);
  // BMI error
  E(0x30);
  E(0x18);
  // REP 20
  E(0xC2);
  E(0x20);
  // LDA $E020,x[$7E:E020]
  E(0xBD);
  E(0x20);
  E(0xE0);
  // BMI error
  E(0x30);
  E(0x11);
  // CMP #00E0
  E(0xC9);
  E(0xE0);
  E(0x00);
  // BCS error
  E(0xB0);
  E(0x0C);
  // LDA $E022,x[$7E:E022]
  E(0xBD);
  E(0x22);
  E(0xE0);
  // BMI error
  E(0x30);
  E(0x07);
  // CMP #00D0
  E(0xC9);
  E(0xD0);
  E(0x00);
  // BCS error
  E(0xB0);
  E(0x02);  // error
  // BRA end
  E(0x80);
  E(0x08);  // end
  // error:
  // SEP 20
  E(0xE2);
  E(0x20);
  // LDA #01
  E(0xA9);
  E(0x01);
  // STA FREE_MOVE_PLAYER_SELECT_BYTE
  E(0x8F);
  E((uint8_t)(CRAM_FREE_MOVE_PLAYER_SELECT_BYTE));
  E((uint8_t)(CRAM_FREE_MOVE_PLAYER_SELECT_BYTE >> 8));
  E((uint8_t)(CRAM_FREE_MOVE_PLAYER_SELECT_BYTE >> 16));
  // end:
  // SEP 20
  E(0xE2);
  E(0x20);
  // LDA FREE_MOVE_PLAYER_SELECT_BYTE
  E(0xAF);
  E((uint8_t)(CRAM_FREE_MOVE_PLAYER_SELECT_BYTE));
  E((uint8_t)(CRAM_FREE_MOVE_PLAYER_SELECT_BYTE >> 8));
  E((uint8_t)(CRAM_FREE_MOVE_PLAYER_SELECT_BYTE >> 16));
  // RTL
  E(0x6B);
  // //////////////////////////////////////////////////////////
  // Teleport characters who have been off screen for a while
  // //////////////////////////////////////////////////////////
  /*
  * replace:
  $00/E4C3 BD 84 E0    LDA $E084,x[$7E:E684]   A:0200 X:0600 Y:0006 P:envMxdIZC
  $00/E4C6 29 0F       AND #$0F                A:0200 X:0600 Y:0006 P:envMxdIZC
  $00/E4C8 85 02       STA $02[$00:0002]       A:0200 X:0600 Y:0006 P:envMxdIZC
  this goes off for enemies too, so we have to check object index
  */
  P(0xE4C3, 0x22);
  P(0xE4C4, (uint8_t)(c->working_offset));
  P(0xE4C5, (uint8_t)(c->working_offset >> 8));
  P(0xE4C6, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0xE4C7, 0xEA);
  P(0xE4C8, 0xEA);
  P(0xE4C9, 0xEA);
  // (A 8bit, XY 16bit, X is object index and should be preserved, A/Y can be corrupted)
  // CPX #0600
  E(0xE0);
  E(0x00);
  E(0x06);
  // BCC over - player character
  E(0x90);
  E(0x08);
  // (removed code) LDA $E084,x
  E(0xBD);
  E(0x84);
  E(0xE0);
  // (removed code) AND #$0F
  E(0x29);
  E(0x0F);
  // (removed code) STA $02
  E(0x85);
  E(0x02);
  // RTL
  E(0x6B);
  // over:
  // REP 20
  E(0xC2);
  E(0x20);
  // TXA
  E(0x8A);
  // XBA -- A should be 0000, 0002, or 0004 for boy/girl/sprite
  E(0xEB);
  // TAY -- now it's in Y.
  E(0xA8);
  // LDA 7EE020, X -- load screen x
  E(0xBF);
  E(0x20);
  E(0xE0);
  E(0x7E);
  // BMI error
  E(0x30);
  E(0x20);
  // CMP #00E0
  E(0xC9);
  E(0xE0);
  E(0x00);
  // BCS error
  E(0xB0);
  E(0x1B);
  // LDA 7EE022, X -- load screen y
  E(0xBF);
  E(0x22);
  E(0xE0);
  E(0x7E);
  // BMI error
  E(0x30);
  E(0x15);
  // CMP #00D0
  E(0xC9);
  E(0xD0);
  E(0x00);
  // BCS error
  E(0xB0);
  E(0x10);
  // if we made it here, the player is on screen, so don't teleport them, and if the timer is active, cancel it
  // LDA #0000
  E(0xA9);
  E(0x00);
  E(0x00);
  // 99 20 CF   STA FREE_MOVE_PLAYER1_COUNTER_16BIT,Y
  E(0x99);
  E((uint8_t)(CRAM_FREE_MOVE_PLAYER1_COUNTER_16BIT));
  E((uint8_t)(CRAM_FREE_MOVE_PLAYER1_COUNTER_16BIT >> 8));
  // SEP 20
  E(0xE2);
  E(0x20);
  // (removed code) LDA $E084,x
  E(0xBD);
  E(0x84);
  E(0xE0);
  // (removed code) AND #$0F
  E(0x29);
  E(0x0F);
  // (removed code) STA $02
  E(0x85);
  E(0x02);
  // RTL
  E(0x6B);
  // error:
  // if we made it here, the character is off screen.
  // increment the offscreen counter; if it's been long enough, find p1 and warp them to it (and reset the counter)
  // B9 20 CF    LDA CF20, Y
  E(0xB9);
  E((uint8_t)(CRAM_FREE_MOVE_PLAYER1_COUNTER_16BIT));
  E((uint8_t)(CRAM_FREE_MOVE_PLAYER1_COUNTER_16BIT >> 8));
  // INC A
  E(0x1A);
  // CMP #teleportTimerInFrames
  E(0xC9);
  E((uint8_t)teleportTimerInFrames);
  E((uint8_t)(teleportTimerInFrames>>8));
  // BCS warpPlayer -- branch if greater/equal
  E(0xB0);
  E(0x0D);
  // if we got here, the character is off screen, but it's not time to teleport them yet.  store the counter back and wait.
  // STA FREE_MOVE_PLAYER1_COUNTER_16BIT,Y
  E(0x99);
  E((uint8_t)(CRAM_FREE_MOVE_PLAYER1_COUNTER_16BIT));
  E((uint8_t)(CRAM_FREE_MOVE_PLAYER1_COUNTER_16BIT >> 8));
  // SEP 20
  E(0xE2);
  E(0x20);
  // (removed code) LDA $E084,x
  E(0xBD);
  E(0x84);
  E(0xE0);
  // (removed code) AND #$0F
  E(0x29);
  E(0x0F);
  // (removed code) STA $02
  E(0x85);
  E(0x02);
  // RTL
  E(0x6B);
  // warpPlayer:
  // if we got here, the character's been off screen for a while; teleport them back to p1 and reset the counter
  // LDA #0000
  E(0xA9);
  E(0x00);
  E(0x00);
  // STA FREE_MOVE_PLAYER1_COUNTER_16BIT,Y
  E(0x99);
  E((uint8_t)(CRAM_FREE_MOVE_PLAYER1_COUNTER_16BIT));
  E((uint8_t)(CRAM_FREE_MOVE_PLAYER1_COUNTER_16BIT >> 8));
  // LDA 7ee02c - player control number
  E(0xAF);
  E(0x2C);
  E(0xE0);
  E(0x7E);
  // AND 00FF
  E(0x29);
  E(0xFF);
  E(0x00);
  // CMP #0001 - is the boy player 1?
  E(0xC9);
  E(0x01);
  E(0x00);
  // BNE tryGirlChar - nope, check the girl
  E(0xD0);
  E(0x22);
  // if we got here, warp to boy character since he's p1, then return
  // LDA 7ee002 - x position
  E(0xAF);
  E(0x02);
  E(0xE0);
  E(0x7E);
  // STA 7ee002,x - x position
  E(0x9F);
  E(0x02);
  E(0xE0);
  E(0x7E);
  // LDA 7ee004 - y position
  E(0xAF);
  E(0x04);
  E(0xE0);
  E(0x7E);
  // STA 7ee004,x - y position
  E(0x9F);
  E(0x04);
  E(0xE0);
  E(0x7E);
  // SEP 20
  E(0xE2);
  E(0x20);
  // LDA 7ee00B - z position
  E(0xAF);
  E(0x0B);
  E(0xE0);
  E(0x7E);
  // STA 7ee00B,x - z position
  E(0x9F);
  E(0x0B);
  E(0xE0);
  E(0x7E);
  // (removed code) LDA $E084,x
  E(0xBD);
  E(0x84);
  E(0xE0);
  // (removed code) AND #$0F
  E(0x29);
  E(0x0F);
  // (removed code) STA $02
  E(0x85);
  E(0x02);
  // RTL
  E(0x6B);
  // tryGirlChar:
  // basically the same as the above but we're checking whether the girl is p1, and teleporting to her if so.
  // LDA 7ee22c - girl player num
  E(0xAF);
  E(0x2C);
  E(0xE2);
  E(0x7E);
  // AND 00FF
  E(0x29);
  E(0xFF);
  E(0x00);
  // CMP #0001
  E(0xC9);
  E(0x01);
  E(0x00);
  // BNE trySpriteChar - nope, check the sprite
  E(0xD0);
  E(0x22);
  // if we got here, warp to girl character since she's p1, then return
  // LDA 7ee202 - x position
  E(0xAF);
  E(0x02);
  E(0xE2);
  E(0x7E);
  // STA 7ee002,x - x position
  E(0x9F);
  E(0x02);
  E(0xE0);
  E(0x7E);
  // LDA 7ee204 - y position
  E(0xAF);
  E(0x04);
  E(0xE2);
  E(0x7E);
  // STA 7ee004, x - y position
  E(0x9F);
  E(0x04);
  E(0xE0);
  E(0x7E);
  // SEP 20
  E(0xE2);
  E(0x20);
  // LDA 7ee20B - z position
  E(0xAF);
  E(0x0B);
  E(0xE2);
  E(0x7E);
  // STA 7ee00B,x - z position
  E(0x9F);
  E(0x0B);
  E(0xE0);
  E(0x7E);
  // (replaced code) LDA $E084,x
  E(0xBD);
  E(0x84);
  E(0xE0);
  // (replaced code) AND #$0F
  E(0x29);
  E(0x0F);
  // (replaced code) STA $02
  E(0x85);
  E(0x02);
  // RTL
  E(0x6B);
  // trySpriteChar:
  // basically the same as the above but at this point we have to assume the sprite is p1 since no one else is, so we don't bother to check
  // LDA 7ee402 - x position
  E(0xAF);
  E(0x02);
  E(0xE4);
  E(0x7E);
  // STA 7ee002,x - x position
  E(0x9F);
  E(0x02);
  E(0xE0);
  E(0x7E);
  // LDA 7ee404 - y position
  E(0xAF);
  E(0x04);
  E(0xE4);
  E(0x7E);
  // STA 7ee004,x - y position
  E(0x9F);
  E(0x04);
  E(0xE0);
  E(0x7E);
  // SEP 20
  E(0xE2);
  E(0x20);
  // LDA 7ee40B - z position
  E(0xAF);
  E(0x0B);
  E(0xE4);
  E(0x7E);
  // STA 7ee00B,x - z position
  E(0x9F);
  E(0x0B);
  E(0xE0);
  E(0x7E);
  // (replaced code) LDA $E084,x
  E(0xBD);
  E(0x84);
  E(0xE0);
  // (replaced code) AND #$0F
  E(0x29);
  E(0x0F);
  // (replaced code) STA $02
  E(0x85);
  E(0x02);
  // (replaced code) RTL
  E(0x6B);
  // ///////////////////////////////////
  // fix menu positioning for offscreen characters
  // ///////////////////////////////////
  // note that if menus show up close enough to the edge of the screen, sometimes they will spin forever
  // and softlock the game.  definitely don't want this!
  /*
  * replace:
  $C0/713C 8D 1A 18    STA $181A  [$00:181A]   A:0078 X:0200 Y:FFEB P:envmxdIzC
  $C0/713F 5A          PHY                     A:0078 X:0200 Y:FFEB P:envmxdIzC
  $C0/7140 BF 22 E0 7E LDA $7EE022,x[$7E:E222] A:0078 X:0200 Y:FFEB P:envmxdIzC
  $C0/7144 18          CLC                     A:0064 X:0200 Y:FFEB P:envmxdIzC
  $C0/7145 63 01       ADC $01,s[$00:01E4]     A:0064 X:0200 Y:FFEB P:envmxdIzc
  $C0/7147 8D 1C 18    STA $181C  [$00:181C]   A:004F X:0200 Y:FFEB P:envmxdIzC
  $C0/714A 7A PLY                              A:004F X:0200 Y:FFEB P:envmxdIzC
  */
  P(0x713C, 0x22);
  P(0x713D, (uint8_t)(c->working_offset));
  P(0x713E, (uint8_t)(c->working_offset >> 8));
  P(0x713F, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x7140, 0xEA);
  P(0x7141, 0xEA);
  P(0x7142, 0xEA);
  P(0x7143, 0xEA);
  P(0x7144, 0xEA);
  P(0x7145, 0xEA);
  P(0x7146, 0xEA);
  P(0x7147, 0xEA);
  P(0x7148, 0xEA);
  P(0x7149, 0xEA);
  P(0x714A, 0xEA);
  uint8_t xMin = 0x30;
  uint8_t xMax = 0xC0;
  uint8_t yMin = 0x30;
  uint8_t yMax = 0xA8;
  // BPL 05
  E(0x10);
  E(0x05);
  // LDA #0020 - negative replacement
  E(0xA9);
  E(xMin);
  E(0x00);
  // BRA yCheck
  E(0x80);
  E(0x12);
  // CMP #00C0 - right screen bounds
  E(0xC9);
  E(xMax);
  E(0x00);
  // BCC/BLT skip1
  E(0x90);
  E(0x05);
  // LDA #00C0 - set position
  E(0xA9);
  E(xMax);
  E(0x00);
  // BRA yCheck
  E(0x80);
  E(0x08);
  // skip1:
  // CMP #0020 - left screen bounds
  E(0xC9);
  E(xMin);
  E(0x00);
  // BCS/BGE skip2
  E(0xB0);
  E(0x03);
  // LDA #0020
  E(0xA9);
  E(xMin);
  E(0x00);
  // skip2:
  // yCheck:
  // STA $181A - write x
  E(0x8D);
  E(0x1A);
  E(0x18);
  // PHY
  E(0x5A);
  // LDA $7EE022,x
  E(0xBF);
  E(0x22);
  E(0xE0);
  E(0x7E);
  // CLC
  E(0x18);
  // ADC $01,s
  E(0x63);
  E(0x01);
  // BPL 05
  E(0x10);
  E(0x05);
  // LDA #0020 - negative replacement
  E(0xA9);
  E(yMin);
  E(0x00);
  // BRA yCheck
  E(0x80);
  E(0x12);
  // CMP #00C0 - bottom screen bounds
  E(0xC9);
  E(yMax);
  E(0x00);
  // BCC/BLT skip1
  E(0x90);
  E(0x05);
  // LDA #00C0 - set position
  E(0xA9);
  E(yMax);
  E(0x00);
  // BRA end
  E(0x80);
  E(0x08);
  // skip1:
  // CMP #0020 - top screen bounds
  E(0xC9);
  E(yMin);
  E(0x00);
  // BCS/BGE skip2
  E(0xB0);
  E(0x03);
  // LDA #0020
  E(0xA9);
  E(yMin);
  E(0x00);
  // end:
  // STA $181C
  E(0x8D);
  E(0x1C);
  E(0x18);
  // PLY
  E(0x7A);
  // RTL
  E(0x6B);
  return 1;
}

/* Speedup - upstream processing/hacks/common/enhancement/Speedup.cs */
int sr_hack_speedup(struct sr_ctx *c) {
  if (!SB("funSpeedup")) return 0;
  // ////////////////////////////////////////
  // make speedup spell give + movement speed
  // ////////////////////////////////////////
  // B717 -> B730 - rewrite to handle movement speed
  // $00/B717 F0 08       BEQ $08 - not moving horizontally
  // $00/B719 A2 02       LDX #$02 ; right speed
  // $00/B71B 29 01       AND #$01
  // $00/B71D D0 02       BNE $02
  // $00/B71F A2 82       LDX #$82 ; left speed
  //
  // $00/B721 A5 10       LDA $10
  // $00/B723 29 0C       AND #$0C
  // $00/B725 F0 08       BEQ $08 - not moving vertically
  // $00/B727 A0 02       LDY #$02 ; down speed
  // $00/B729 29 04       AND #$04
  // $00/B72B D0 02       BNE $02
  // $00/B72D A0 82       LDY #$82 ; up speed
  //
  // $00/B72F 24 10       BIT $10
  // B719 - load right-moving speed
  // $00 / B719 A2 02       LDX #$02                A:0001 X:0000 Y:0000 P:envMXdIzc
  // $00 / B71B 29 01       AND #$01                A:0001 X:0002 Y:0000 P:envMXdIzc
  int rightOffset = c->working_offset;
  { static const uint8_t orig_code[] = {0xA2, 0x02}, new_code[] = {0xA2, 0x03}; write_walk_speed_check(c, orig_code, 2, new_code, 2); }
  // up
  // $00/B72D A0 82       LDY #$82                A:0000 X:0000 Y:0002 P:envMXdIZc
  // $00 / B72F 24 10       BIT $10[$00:0010]   A: 0000 X: 0000 Y: 0082 P: eNvMXdIzc
  int upOffset = c->working_offset;
  { static const uint8_t orig_code[] = {0xA0, 0x82}, new_code[] = {0xA0, 0x83}; write_walk_speed_check(c, orig_code, 2, new_code, 2); }
  // down
  // $00/B727 A0 02       LDY #$02                A:0004 X:0000 Y:0000 P:envMXdIzc
  // $00 / B729 29 04       AND #$04                A:0004 X:0000 Y:0002 P:envMXdIzc
  int downOffset = c->working_offset;
  { static const uint8_t orig_code[] = {0xA0, 0x02}, new_code[] = {0xA0, 0x03}; write_walk_speed_check(c, orig_code, 2, new_code, 2); }
  // left
  // $00/B71F A2 82       LDX #$82                A:0000 X:0003 Y:0000 P:envMXdIZc
  // $00 / B721 A5 10       LDA $10[$00:0010]   A: 0000 X: 0082 Y: 0000 P: eNvMXdIzc
  int leftOffset = c->working_offset;
  { static const uint8_t orig_code[] = {0xA2, 0x82}, new_code[] = {0xA2, 0x83}; write_walk_speed_check(c, orig_code, 2, new_code, 2); }
  // E02C,x on player char is whether they're p1
  for (int i = 0xB717; i <= 0xB730; i++)
  {
  P(i, 0xEA);
  }
  P(0xB717, 0x22);
  P(0xB718, (uint8_t)(c->working_offset));
  P(0xB719, (uint8_t)(c->working_offset >> 8));
  P(0xB71A, (uint8_t)((c->working_offset >> 16) + 0xC0));
  // recreate original sequence above, but with generated code that checks for haste bits and moves faster if they're enabled
  E(0xF0);
  E(0x0C);
  E(0x22);
  E((uint8_t)(rightOffset));
  E((uint8_t)(rightOffset >> 8));
  E((uint8_t)((rightOffset >> 16) + 0xC0));
  E(0x29);
  E(0x01);
  E(0xD0);
  E(0x04);
  E(0x22);
  E((uint8_t)(leftOffset));
  E((uint8_t)(leftOffset >> 8));
  E((uint8_t)((leftOffset >> 16) + 0xC0));
  E(0xA5);
  E(0x10);
  E(0x29);
  E(0x0C);
  E(0xF0);
  E(0x0C);
  E(0x22);
  E((uint8_t)(downOffset));
  E((uint8_t)(downOffset >> 8));
  E((uint8_t)((downOffset >> 16) + 0xC0));
  E(0x29);
  E(0x04);
  E(0xD0);
  E(0x04);
  E(0x22);
  E((uint8_t)(upOffset));
  E((uint8_t)(upOffset >> 8));
  E((uint8_t)((upOffset >> 16) + 0xC0));
  E(0x24);
  E(0x10);
  E(0x6B);
  // ////////////////////////////////////////
  // faster stamina recharge
  // ////////////////////////////////////////
  // $00/EB5F 3A          DEC A                   A:0043 X:0000 Y:0000 P:envMxdIzc
  // $00/EB60 9D ED E1    STA $E1ED,x[$7E:E1ED]   A:0053 X:0000 Y:0000 P:envMxdIzC
  P(0xEB5F, 0x22);
  P(0xEB60, (uint8_t)(c->working_offset));
  P(0xEB61, (uint8_t)(c->working_offset >> 8));
  P(0xEB62, (uint8_t)((c->working_offset >> 16) + 0xC0));
  // replaced code
  E(0x3A);
  E(0x9D);
  E(0xED);
  E(0xE1);
  // pha
  E(0x48);
  // lda $E1B0,x - check if haste is up
  E(0xBD);
  E(0xB0);
  E(0xE1);
  // and #14 - check if haste is up
  E(0x29);
  E(0x14);
  // beq over - it's not
  E(0xF0);
  E(0x0B);
  // lda $E1ED,x - load stamina remaining until max
  E(0xBD);
  E(0xED);
  E(0xE1);
  // CMP #02 - do nothing if near minimum
  E(0xC9);
  E(0x02);
  // blt over
  E(0x90);
  E(0x04);
  // DEC A - hasten stamina recharge
  E(0x3A);
  // STA $E1ED,x - write it back
  E(0x9D);
  E(0xED);
  E(0xE1);
  // over:
  // pla - even out the stack
  E(0x68);
  // RTL
  E(0x6B);
  // ////////////////////////////////////////
  // faster weapon charging
  // ////////////////////////////////////////
  // $00/B3DF C2 20       REP #$20                A:000E X:0000 Y:0000 P:eNvMxdIzc
  // $00 / B3E1 9F 1A E0 7E STA $7EE01A,x[$7E:E01A] A: 000E X: 0000 Y: 0000 P: eNvmxdIzc
  // $00 / B3E5 E2 20       SEP #$20                A:000E X:0000 Y:0000 P:eNvmxdIzc
  P(0xB3DF, 0x22);
  P(0xB3E0, (uint8_t)(c->working_offset));
  P(0xB3E1, (uint8_t)(c->working_offset >> 8));
  P(0xB3E2, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0xB3E3, 0xEA);
  P(0xB3E4, 0xEA);
  P(0xB3E5, 0xEA);
  P(0xB3E6, 0xEA);
  // (replaced code)
  // REP 20
  E(0xC2);
  E(0x20);
  // STA $7ee01a,x
  E(0x9F);
  E(0x1A);
  E(0xE0);
  E(0x7E);
  // SEP 20
  E(0xE2);
  E(0x20);
  // pha
  E(0x48);
  // lda 7ee1b0,x - check if hasted
  E(0xBF);
  E(0xB0);
  E(0xE1);
  E(0x7E);
  // and #14 - check if hasted
  E(0x29);
  E(0x14);
  // beq over
  E(0xF0);
  E(0x0D);
  // lda 7ee01a,x - load weapon charge
  E(0xBF);
  E(0x1A);
  E(0xE0);
  E(0x7E);
  // cmp #2B - at max
  E(0xC9);
  E(0x2A);
  // bge over
  E(0xB0);
  E(0x05);
  // inc A
  E(0x1A);
  // sta 7ee01a,x
  E(0x9F);
  E(0x1A);
  E(0xE0);
  E(0x7E);
  // over:
  // pla
  E(0x68);
  // rtl
  E(0x6B);
  return 1;
}

/* FreeTargetting - upstream processing/hacks/common/enhancement/FreeTargetting.cs */
int sr_hack_free_targetting(struct sr_ctx *c) {
  if (!SB("offscreenTarget")) return 0;
  // screen x/y checks when processing targets
  /*
  $D0/DAB8 BF 22 00 7E LDA $7E0022,x[$7E:E622] A:0045 X:E600 Y:1812 P:eNvmxdIzc
  $D0/DABC C9 D8 00    CMP #$00D8              A:00C7 X:E600 Y:1812 P:envmxdIzc
  $D0/DABF B0 28       BCS $28    [$DAE9]      A:00C7 X:E600 Y:1812 P:eNvmxdIzc **
  $
  
  $D0/DAAE C9 08 00    CMP #$0008              A:0045 X:E600 Y:1812 P:envmxdIzC
  $D0/DAB1 90 36       BCC $36    [$DAE9]      A:0045 X:E600 Y:1812 P:envmxdIzC **
  $D0/DAB3 C9 F8 00    CMP #$00F8              A:0045 X:E600 Y:1812 P:envmxdIzC
  $D0/DAB6 B0 31       BCS $31    [$DAE9]      A:0045 X:E600 Y:1812 P:eNvmxdIzc **
  */
  P(0x10DABF, 0xEA);
  P(0x10DAC0, 0xEA);
  P(0x10DAB1, 0xEA);
  P(0x10DAB2, 0xEA);
  P(0x10DAB6, 0xEA);
  P(0x10DAB7, 0xEA);
  P(0x10DC9F, 0x22);
  P(0x10DCA0, (uint8_t)(c->working_offset));
  P(0x10DCA1, (uint8_t)(c->working_offset >> 8));
  P(0x10DCA2, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x10DCA3, 0xEA);
  int XMIN = 0x01;
  int XMAX = 0xE4;
  int YMIN = 0x01;
  int YMAX = 0xBC;
  // PHA
  E(0x48);
  // AND #00FF
  E(0x29);
  E(0xFF);
  E(0x00);
  // SEP 20
  E(0xE2);
  E(0x20);
  // STA RamOffsets.X_SCREEN_TEMP_LOCATION
  E(0x8F);
  E((uint8_t)CRAM_X_SCREEN_TEMP_LOCATION);
  E((uint8_t)(CRAM_X_SCREEN_TEMP_LOCATION >> 8));
  E((uint8_t)((CRAM_X_SCREEN_TEMP_LOCATION >> 16)));
  // REP 20
  E(0xC2);
  E(0x20);
  // PLA
  E(0x68);
  // XBA
  E(0xEB);
  // SEP 20
  E(0xE2);
  E(0x20);
  // STA RamOffsets.Y_SCREEN_TEMP_LOCATION
  E(0x8F);
  E((uint8_t)CRAM_Y_SCREEN_TEMP_LOCATION);
  E((uint8_t)(CRAM_Y_SCREEN_TEMP_LOCATION >> 8));
  E((uint8_t)((CRAM_Y_SCREEN_TEMP_LOCATION >> 16)));
  //
  // -- check if msbs set and we're way out of bounds
  // BF 21 00 7E   LDA $7E0021,x[$7E:E621] - screen x msb
  E(0xBF);
  E(0x21);
  E(0x00);
  E(0x7E);
  // BMI negative
  E(0x30);
  E(0x04);
  // BNE positive
  E(0xD0);
  E(0x0A);
  // BRA overx
  E(0x80);
  E(0x0E);
  // negative:
  // LDA #XMIN
  E(0xA9);
  E((uint8_t)XMIN);
  // STA RamOffsets.X_SCREEN_TEMP_LOCATION
  E(0x8F);
  E((uint8_t)CRAM_X_SCREEN_TEMP_LOCATION);
  E((uint8_t)(CRAM_X_SCREEN_TEMP_LOCATION >> 8));
  E((uint8_t)((CRAM_X_SCREEN_TEMP_LOCATION >> 16)));
  // BRA overx
  E(0x80);
  E(0x06);
  // positive:
  // LDA #XMAX
  E(0xA9);
  E((uint8_t)XMAX);
  // STA RamOffsets.X_SCREEN_TEMP_LOCATION
  E(0x8F);
  E((uint8_t)CRAM_X_SCREEN_TEMP_LOCATION);
  E((uint8_t)(CRAM_X_SCREEN_TEMP_LOCATION >> 8));
  E((uint8_t)((CRAM_X_SCREEN_TEMP_LOCATION >> 16)));
  // overx:
  //
  // BF 23 00 7E   LDA $7E0023,x[$7E:E623] - screen y msb
  E(0xBF);
  E(0x23);
  E(0x00);
  E(0x7E);
  // BMI negative
  E(0x30);
  E(0x04);
  // BNE positive
  E(0xD0);
  E(0x0A);
  // BRA overy
  E(0x80);
  E(0x0E);
  // negative:
  // LDA #YMIN
  E(0xA9);
  E((uint8_t)YMIN);
  // STA RamOffsets.Y_SCREEN_TEMP_LOCATION
  E(0x8F);
  E((uint8_t)CRAM_Y_SCREEN_TEMP_LOCATION);
  E((uint8_t)(CRAM_Y_SCREEN_TEMP_LOCATION >> 8));
  E((uint8_t)((CRAM_Y_SCREEN_TEMP_LOCATION >> 16)));
  // BRA overy
  E(0x80);
  E(0x06);
  // positive:
  // LDA #YMAX
  E(0xA9);
  E((uint8_t)YMAX);
  // STA RamOffsets.Y_SCREEN_TEMP_LOCATION
  E(0x8F);
  E((uint8_t)CRAM_Y_SCREEN_TEMP_LOCATION);
  E((uint8_t)(CRAM_Y_SCREEN_TEMP_LOCATION >> 8));
  E((uint8_t)((CRAM_Y_SCREEN_TEMP_LOCATION >> 16)));
  // overy:
  //
  // - bounds check the x/y values
  // LDA RamOffsets.X_SCREEN_TEMP_LOCATION
  E(0xAF);
  E((uint8_t)CRAM_X_SCREEN_TEMP_LOCATION);
  E((uint8_t)(CRAM_X_SCREEN_TEMP_LOCATION >> 8));
  E((uint8_t)((CRAM_X_SCREEN_TEMP_LOCATION >> 16)));
  // CMP #XMIN
  E(0xC9);
  E((uint8_t)XMIN);
  // BGE overA
  E(0xB0);
  E(0x06);
  // LDA #XMIN
  E(0xA9);
  E((uint8_t)XMIN);
  // STA RamOffsets.X_SCREEN_TEMP_LOCATION
  E(0x8F);
  E((uint8_t)CRAM_X_SCREEN_TEMP_LOCATION);
  E((uint8_t)(CRAM_X_SCREEN_TEMP_LOCATION >> 8));
  E((uint8_t)((CRAM_X_SCREEN_TEMP_LOCATION >> 16)));
  // overA:
  //
  // CMP #XMAX
  E(0xC9);
  E((uint8_t)XMAX);
  // BLT overB
  E(0x90);
  E(0x06);
  // LDA #XMAX
  E(0xA9);
  E((uint8_t)XMAX);
  // STA RamOffsets.X_SCREEN_TEMP_LOCATION
  E(0x8F);
  E((uint8_t)CRAM_X_SCREEN_TEMP_LOCATION);
  E((uint8_t)(CRAM_X_SCREEN_TEMP_LOCATION >> 8));
  E((uint8_t)((CRAM_X_SCREEN_TEMP_LOCATION >> 16)));
  // overB:
  //
  // LDA RamOffsets.Y_SCREEN_TEMP_LOCATION
  E(0xAF);
  E((uint8_t)CRAM_Y_SCREEN_TEMP_LOCATION);
  E((uint8_t)(CRAM_Y_SCREEN_TEMP_LOCATION >> 8));
  E((uint8_t)((CRAM_Y_SCREEN_TEMP_LOCATION >> 16)));
  // CMP #YMIN
  E(0xC9);
  E((uint8_t)YMIN);
  // BGE overC
  E(0xB0);
  E(0x06);
  // LDA #YMIN
  E(0xA9);
  E((uint8_t)YMIN);
  // STA RamOffsets.Y_SCREEN_TEMP_LOCATION
  E(0x8F);
  E((uint8_t)CRAM_Y_SCREEN_TEMP_LOCATION);
  E((uint8_t)(CRAM_Y_SCREEN_TEMP_LOCATION >> 8));
  E((uint8_t)((CRAM_Y_SCREEN_TEMP_LOCATION >> 16)));
  // overC:
  //
  // CMP #YMAX
  E(0xC9);
  E((uint8_t)YMAX);
  // BLT overD
  E(0x90);
  E(0x06);
  // LDA #YMAX
  E(0xA9);
  E((uint8_t)YMAX);
  // STA RamOffsets.Y_SCREEN_TEMP_LOCATION
  E(0x8F);
  E((uint8_t)CRAM_Y_SCREEN_TEMP_LOCATION);
  E((uint8_t)(CRAM_Y_SCREEN_TEMP_LOCATION >> 8));
  E((uint8_t)((CRAM_Y_SCREEN_TEMP_LOCATION >> 16)));
  // overD:
  //
  // REP 20
  E(0xC2);
  E(0x20);
  // XBA
  E(0xEB);
  // SEP 20
  E(0xE2);
  E(0x20);
  // LDA RamOffsets.X_SCREEN_TEMP_LOCATION
  E(0xAF);
  E((uint8_t)CRAM_X_SCREEN_TEMP_LOCATION);
  E((uint8_t)(CRAM_X_SCREEN_TEMP_LOCATION >> 8));
  E((uint8_t)((CRAM_X_SCREEN_TEMP_LOCATION >> 16)));
  // REP 20
  E(0xC2);
  E(0x20);
  //
  // removed code:
  // AE 32 18    LDX $1832  [$00:1832]   A:0A6B X:E600 Y:0120 P:envmxdIzc
  E(0xAE);
  E(0x32);
  E(0x18);
  // 95 00       STA $00,x  [$00:082C]    A:0A6B X:082C Y:0120 P:envmxdIzc
  E(0x95);
  E(0x00);
  // 6B RTL
  E(0x6B);
  // fix: 4 pixels of overflow
  // $D0/DC8C BF 20 00 7E LDA $7E0020,x[$7E:EA20] A:6EEA X:EA00 Y:0120 P:envMxdIzC
  // $D0 / DC90 18          CLC A:6EFD X:EA00 Y:0120 P: eNvMxdIzC
  // $D0/DC91 69 04       ADC #$04                A:6EFD X:EA00 Y:0120 P:eNvMxdIzc
  P(0x10DC8C, 0x22);
  P(0x10DC8D, (uint8_t)(c->working_offset));
  P(0x10DC8E, (uint8_t)(c->working_offset >> 8));
  P(0x10DC8F, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x10DC90, 0xEA);
  P(0x10DC91, 0xEA);
  P(0x10DC92, 0xEA);
  E(0xBF);
  E(0x20);
  E(0x00);
  E(0x7E);
  E(0x18);
  E(0x69);
  E(0x04);
  // BCC over
  E(0x90);
  E(0x02);
  E(0xA9);
  E((uint8_t)XMAX);
  // RTL
  E(0x6B);
  // also on y axis
  // $D0/DC7C   BF 22 00 7E LDA $7E0022,x[$7E:EA22] A:EA00 X:EA00 Y:0020 P:eNvMxdIzc
  // $D0 / DC80 38          SEC A:EA84 X:EA00 Y:0020 P: eNvMxdIzc
  // $D0 / DC81 E9 16       SBC #$16                A:EA84 X:EA00 Y:0020 P:eNvMxdIzC
  P(0x10DC7C, 0x22);
  P(0x10DC7D, (uint8_t)(c->working_offset));
  P(0x10DC7E, (uint8_t)(c->working_offset >> 8));
  P(0x10DC7F, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x10DC80, 0xEA);
  P(0x10DC81, 0xEA);
  P(0x10DC82, 0xEA);
  P(0x10DC83, 0xEA);
  P(0x10DC84, 0xEA);
  P(0x10DC85, 0xEA);
  P(0x10DC86, 0xEA);
  P(0x10DC87, 0xEA);
  // (replaced code)
  // LDA $7E0022,x
  E(0xBF);
  E(0x22);
  E(0x00);
  E(0x7E);
  // SEC
  E(0x38);
  // SBC #16
  E(0xE9);
  E(0x16);
  // BPL over -> BCS over
  E(0xB0);
  E(0x02);
  E(0xA9);
  E((uint8_t)YMIN);
  // $D0/DC83 38          SEC                     A:EA01 X:EA00 Y:0020 P:envMxdIzc
  E(0x38);
  // $D0/DC84 FF 45 00 7E SBC $7E0045,x[$7E:EA45] A:EA01 X:EA00 Y:0020 P:envMxdIzC
  E(0xFF);
  E(0x45);
  E(0x00);
  E(0x7E);
  // BPL over -> BCS over
  E(0xB0);
  E(0x02);
  E(0xA9);
  E((uint8_t)YMIN);
  // RTL
  E(0x6B);
  return 1;
}

/* BuyMultipleItems - upstream processing/hacks/common/qol/BuyMultipleItems.cs */
int sr_hack_buy_multiple(struct sr_ctx *c) {
  if (!SB("buyMultiple")) return 0;
  uint8_t maxItems = 4;
  if (SB("moreItems")) maxItems = 7;
  sr_w3_initial_value_set(c, CRAM_CONSUMABLE_BUY_MAX, 0);
  sr_w3_initial_value_set(c, CRAM_CONSUMABLE_BUY_CURRENT, 0);
  sr_w3_initial_value_set(c, CRAM_CONSUMABLE_INPUT_TIMER, 0);
  sr_w3_initial_value_set(c, CRAM_BUYING_MULTIPLE_ITEMS, 0);
  // ///////////////////////////////////////////////////////////////////
  // logic for initialization handling
  // first, the replaced code:
  // 0A ASL
  // AA TAX
  // C2 30 REP 30
  // now, if x == 0 we're in the buy menu, so do:
  //     CONSUMABLE_BUY_MAX = 0
  //     CONSUMABLE_BUY_CURRENT = 0
  //     CONSUMABLE_INPUT_TIMER = 0
  //     look up the item value at D8[$183C] + $180A
  //     if it's >= 0xBA, then it's a consumable, and we do:
  //        check how many we currently have of this; CONSUMABLE_BUY_MAX (new 8bit thing in RamOffsets) = 7 - that
  //            CONSUMABLE_BUY_CURRENT = CONSUMABLE_BUY_MAX
  //        check how many we can afford given our current gold.  if it's < CONSUMABLE_BUY_MAX, then CONSUMABLE_BUY_MAX = that
  //            CONSUMABLE_BUY_CURRENT = CONSUMABLE_BUY_MAX
  // ///////////////////////////////////////////////////////////////////
  /*
  * replace code run when you select something in a menu:
  $C0/7AC3 3A          DEC A                   A:1802 X:0070 Y:0004 P:envMXdIzC              **
  $C0/7AC4 0A          ASL A                   A:1801 X:0070 Y:0004 P:envMXdIzC              **
  $C0/7AC5 AA          TAX                     A:1802 X:0070 Y:0004 P:envMXdIzc              **
  $C0/7AC6 C2 30       REP #$30                A:1802 X:0002 Y:0004 P:envMXdIzc              **
  $C0/7AC8 7C CB 7A    JMP ($7ACB,x)[$C0:7B17] A:1802 X:0002 Y:0004 P:envmxdIzc (vanilla handler; keep)
  */
  P(0x7AC3, 0x22);
  P(0x7AC4, (uint8_t)(c->working_offset));
  P(0x7AC5, (uint8_t)(c->working_offset >> 8));
  P(0x7AC6, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x7AC7, 0xEA);
  E(0x3A);  // replaced code
  E(0x0A);  // replaced code
  E(0xAA);  // replaced code
  E(0xC2);  // replaced code
  E(0x30);  // replaced code
  // CPX #0000 - this will be 0 for buy, 2 for sell
  E(0xE0);
  E(0x00);
  E(0x00);
  // BEQ over
  E(0xF0);
  E(0x01);
  // RTL if not buy menu
  E(0x6B);
  // PHX
  E(0xDA);
  // PHY
  E(0x5A);
  // PHA
  E(0x48);
  // SEP #20
  E(0xE2);
  E(0x20);
  // LDA #00
  E(0xA9);
  E(0x00);
  // STA CONSUMABLE_BUY_MAX
  E(0x8F);
  E((uint8_t)CRAM_CONSUMABLE_BUY_MAX);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_MAX >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_MAX >> 16));
  // STA CONSUMABLE_BUY_CURRENT
  E(0x8F);
  E((uint8_t)CRAM_CONSUMABLE_BUY_CURRENT);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 16));
  // STA CONSUMABLE_INPUT_TIMER
  E(0x8F);
  E((uint8_t)CRAM_CONSUMABLE_INPUT_TIMER);
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 16));
  // REP 20
  E(0xC2);
  E(0x20);
  // LDA 7E183C
  E(0xAF);
  E(0x3C);
  E(0x18);
  E(0x7E);
  // CLC
  E(0x18);
  // SEP 20
  E(0xE2);
  E(0x20);
  // ADC 7E180A
  E(0x6F);
  E(0x0A);
  E(0x18);
  E(0x7E);
  // REP 20
  E(0xC2);
  E(0x20);
  // TAX
  E(0xAA);
  // SEP #20
  E(0xE2);
  E(0x20);
  // LDA $D80000,x
  E(0xBF);
  E(0x00);
  E(0x00);
  E(0xD8);
  // CMP #BA (candy ID - first consumable)
  E(0xC9);
  E(0xBA);
  // BCS/BGE over - keep processing if we selected a consumable
  E(0xB0);
  E(0x06);
  // otherwise, selected an armor or something - even out the stack and return
  // REP 20
  E(0xC2);
  E(0x20);
  // PLA
  E(0x68);
  // PLY
  E(0x7A);
  // PLX
  E(0xFA);
  // RTL
  E(0x6B);
  // here: we continue because it's a consumable
  // so .. these are at 7ECC48 .. the amount is the 3 MSBs, the id is the lower 5; 0xFF means end list
  // SEC
  E(0x38);
  // SBC #BA - candy id; first consumable
  E(0xE9);
  E(0xBA);
  // PHA - store the item id we're trying to buy
  E(0x48);
  // LDA #07 (or 4)
  E(0xA9);
  E(maxItems);
  // STA CONSUMABLE_BUY_MAX
  E(0x8F);
  E((uint8_t)CRAM_CONSUMABLE_BUY_MAX);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_MAX >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_MAX >> 16));
  // LDX #FFFF
  E(0xA2);
  E(0xFF);
  E(0xFF);
  // backToCheck:
  // INX - to zero initially, +1 further iters
  E(0xE8);
  // LDA 7ECC48,x - check inventory
  E(0xBF);
  E(0x48);
  E(0xCC);
  E(0x7E);
  // CMP #FF - if it's FF, we're at the end of the inventory
  E(0xC9);
  E(0xFF);
  // BEQ doneCheckingAmounts
  E(0xF0);
  E(0x13 + 7);
  // AND #1F - extract item id from full value, which contains the count also
  E(0x29);
  E(0x1F);
  // CMP $01,s - compare against the item id we pushed above
  E(0xC3);
  E(0x01);
  // BNE backToCheck - not equal; branch back and try the next item
  E(0xD0);
  E(0xF1);
  // LDA 7ECC48,x - reload the item with count
  E(0xBF);
  E(0x48);
  E(0xCC);
  E(0x7E);
  // LSR - extract count by shifting the id off
  E(0x4A);
  // LSR
  E(0x4A);
  // LSR
  E(0x4A);
  // LSR
  E(0x4A);
  // LSR
  E(0x4A);
  // subtract from 7 (or 4)
  // PHA
  E(0x48);
  // LDA #07 (or 4)
  E(0xA9);
  E(maxItems);
  // SEC
  E(0x38);
  // SBC $01,s
  E(0xE3);
  E(0x01);
  // STA CONSUMABLE_BUY_MAX - the most of this item we can buy, given the number we currently have
  E(0x8F);
  E((uint8_t)CRAM_CONSUMABLE_BUY_MAX);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_MAX >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_MAX >> 16));
  // PLA - even out the stack
  E(0x68);
  // doneCheckingAmounts:
  // PLA - even out the stack for the comparison value we pushed earlier
  E(0x68);
  // now.. check gold (7ecc6a, 24 bit)
  // D8:FB9C 16 bit value item costs
  // 00: Candy = 000A
  // 01: Chocolate = 001E
  // 02: Royal Jam = 0064
  // 03: Faerie Walnut = 01F4
  // 04: Medical Herb = 000A
  // 05: Cup of Wishes = 0096
  // * 06: Magic Rope = FFFF
  // * 07: Flammie Drum = FFFF
  // * 08: Moogle Belt = FFFF
  // * 09: Midge Mallet = FFFF
  // 0A: Barrel = 01C2
  // * 0B: ? (Dummied Out) = FFFF
  // so..
  // ASL - shift item id << 1
  E(0x0A);
  // REP 20
  E(0xC2);
  E(0x20);
  // AND #00FF
  E(0x29);
  E(0xFF);
  E(0x00);
  // TAX
  E(0xAA);
  // LDA D8:FB9C,x - load the item's price
  E(0xBF);
  E(0x9C);
  E(0xFB);
  E(0xD8);
  // PHA - push cost of what we're buying
  E(0x48);
  // -------------------------
  // check for neko
  // -------------------------
  // SEP 20
  E(0xE2);
  E(0x20);
  // LDA #99 - neko sprite id
  E(0xA9);
  E(0x99);
  // CMP $185B - sprite we talked to
  E(0xCD);
  E(0x5B);
  E(0x18);
  // BNE skipNekoMultiply
  E(0xD0);
  E(0x05);
  // REP 20
  E(0xC2);
  E(0x20);
  // PLA - pull cost
  E(0x68);
  // ASL - multiply cost by 2 since we're at a neko
  E(0x0A);
  // PHA - push back
  E(0x48);
  // skipNekoMultiply:
  // REP 20
  E(0xC2);
  E(0x20);
  // LDY #0000 - this ends up being how many we can afford
  E(0xA0);
  E(0x00);
  E(0x00);
  // LDA 7ecc6a - 16-bit gold LSBs (it's 24 bits total)
  E(0xAF);
  E(0x6A);
  E(0xCC);
  E(0x7E);
  // back:
  // CMP $01,s - compare gold to item cost
  E(0xC3);
  E(0x01);
  // BLT/BCC out - don't have enough - branch out
  E(0x90);
  E(0x0B);
  // INY - can afford - increment y
  E(0xC8);
  // CPY #0007 - if we can afford the max we could buy, break out
  E(0xC0);
  E(maxItems);
  E(0x00);
  // BEQ out
  E(0xF0);
  E(0x05);
  // SEC - subtract the cost and check again with what's remaining
  E(0x38);
  // SBC $01,s
  E(0xE3);
  E(0x01);
  // BRA back - loop back to see if we can afford another
  E(0x80);
  E(0xF1);
  // out:
  // SEP 20
  E(0xE2);
  E(0x20);
  // LDA 7ecc6c - check the MSB of gold
  E(0xAF);
  E(0x6C);
  E(0xCC);
  E(0x7E);
  // BEQ notRich - no MSB; go with the calculation from above
  E(0xF0);
  E(0x03);
  // LDY #0007 - assume if we have > 65k gold, we can afford 7 of anything
  E(0xA0);
  E(maxItems);
  E(0x00);
  // notRich:
  // TYA - put number we can afford back into A
  E(0x98);
  // CMP CONSUMABLE_BUY_MAX
  E(0xCF);
  E((uint8_t)CRAM_CONSUMABLE_BUY_MAX);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_MAX >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_MAX >> 16));
  // BGE over - can afford more than we can hold?  don't care
  E(0xB0);
  E(0x04);
  // STA CONSUMABLE_BUY_MAX
  E(0x8F);
  E((uint8_t)CRAM_CONSUMABLE_BUY_MAX);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_MAX >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_MAX >> 16));
  // over:
  // LDA CONSUMABLE_BUY_MAX
  E(0xAF);
  E((uint8_t)CRAM_CONSUMABLE_BUY_MAX);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_MAX >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_MAX >> 16));
  // STA CONSUMABLE_BUY_CURRENT - default to the max we can buy
  E(0x8F);
  E((uint8_t)CRAM_CONSUMABLE_BUY_CURRENT);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 16));
  // REP 20 - for PLA plus we want to leave it in that state
  E(0xC2);
  E(0x20);
  // PLA - remove the comparison value we pushed earlier
  E(0x68);
  // that's it! restore the shit we saved at the beginning; i think the only one they care about is X but not sure
  // PLA
  E(0x68);
  // PLY
  E(0x7A);
  // PLX
  E(0xFA);
  // RTL
  E(0x6B);
  // from 0x1664 - takes text buffer from $A1A4,x and flushes it to screen
  // should be 8bit a and 16bit x
  // original "Okay?" from 0x19FEEC: 7F 52 A9 8B 81 99 CA 00
  struct sr_bytes how_many_bytes = {0};
  sr_event_text_bytes(c, &how_many_bytes, "How many? (");
  int stringLocation = c->working_offset;
  for (size_t i = 0; i < how_many_bytes.len; i++) E(how_many_bytes.p[i]);
  // copy a few vanilla subroutines and move them for easier calling outside of their original banks
  struct sr_bytes refreshScreenSubrCopy = {0};
  vanilla_subr_to_long_subr(c, &refreshScreenSubrCopy, 0x1664, 0x16B7);
  struct sr_bytes refreshScreenSubr2Copy = {0};
  vanilla_subr_to_long_subr(c, &refreshScreenSubr2Copy, 0x15B9, 0x162B);
  struct sr_bytes refreshScreenSubr2ACopy = {0};
  vanilla_subr_to_long_subr(c, &refreshScreenSubr2ACopy, 0x1539, 0x159D);
  struct sr_bytes refreshScreenSubr2BCopy = {0};
  vanilla_subr_to_long_subr(c, &refreshScreenSubr2BCopy, 0x14C6, 0x14DA);
  struct sr_bytes refreshScreenSubr3Copy = {0};
  vanilla_subr_to_long_subr(c, &refreshScreenSubr3Copy, 0x1D24, 0x1D51);
  int refreshScreenSubr1Location = c->working_offset;
  for (size_t i = 0; i < refreshScreenSubrCopy.len; i++) E(refreshScreenSubrCopy.p[i]);
  int refreshScreenSubr2Location = c->working_offset;
  for (size_t i = 0; i < refreshScreenSubr2Copy.len; i++) E(refreshScreenSubr2Copy.p[i]);
  int refreshScreenSubr2ALocation = c->working_offset;
  for (size_t i = 0; i < refreshScreenSubr2ACopy.len; i++) E(refreshScreenSubr2ACopy.p[i]);
  int refreshScreenSubr2BLocation = c->working_offset;
  for (size_t i = 0; i < refreshScreenSubr2BCopy.len; i++) E(refreshScreenSubr2BCopy.p[i]);
  int refreshScreenSubr3Location = c->working_offset;
  (void)refreshScreenSubr3Location;
  for (size_t i = 0; i < refreshScreenSubr3Copy.len; i++) E(refreshScreenSubr3Copy.p[i]);
  int redrawTextLocation = c->working_offset;
  // PHB
  E(0x8B);
  // LDA #7E
  E(0xA9);
  E(0x7E);
  // PHA
  E(0x48);
  // PLB
  E(0xAB);
  // LDA #80
  E(0xA9);
  E(0x80);
  // LDX #0018
  E(0xA2);
  E(0x18);
  E(0x00);
  // back:
  // STA $A1A4,x
  E(0x9D);
  E(0xA4);
  E(0xA1);
  // DEX
  E(0xCA);
  // BNE back
  E(0xD0);
  E(0xFA);
  // LDY (int)how_many_bytes.len (0x0B)
  E(0xA2);
  E((uint8_t)(int)how_many_bytes.len);
  E(0x00);
  // back2:
  // LDA stringLocation,x
  E(0xBF);
  E((uint8_t)(stringLocation));
  E((uint8_t)(stringLocation >> 8));
  E((uint8_t)((stringLocation >> 16) + 0xC0));
  // STA $A1A4,x
  E(0x9D);
  E(0xA4);
  E(0xA1);
  // DEX
  E(0xCA);
  // BPL instead to include zero
  E(0x10);
  E(0xF6);
  // LDA CONSUMABLE_BUY_CURRENT
  E(0xAF);
  E((uint8_t)CRAM_CONSUMABLE_BUY_CURRENT);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 16));
  // CLC
  E(0x18);
  // ADC #B5
  E(0x69);
  E(0xB5);
  // STA 7EA1A4 + (int)how_many_bytes.len
  E(0x8F);
  E((uint8_t)(0xA4 + (int)how_many_bytes.len));
  E(0xA1);
  E(0x7E);
  // LDA EventUtil.getByte(')')
  E(0xA9);
  E(0xCC);
  // CLC
  E(0x18);
  // STA 7EA1A4 + (int)how_many_bytes.len + 1
  E(0x8F);
  E((uint8_t)(0xA4 + (int)how_many_bytes.len + 1));
  E(0xA1);
  E(0x7E);
  // JSL refreshScreenSubr1Location, refreshScreenSubr2Location
  E(0x22);
  E((uint8_t)(refreshScreenSubr1Location));
  E((uint8_t)(refreshScreenSubr1Location >> 8));
  E((uint8_t)((refreshScreenSubr1Location >> 16) + 0xC0));
  E(0x22);
  E((uint8_t)(refreshScreenSubr2Location));
  E((uint8_t)(refreshScreenSubr2Location >> 8));
  E((uint8_t)((refreshScreenSubr2Location >> 16) + 0xC0));
  // LDY #0000
  E(0xA0);
  E(0x00);
  E(0x00);
  // code from $C0/151D
  // LDA #0E
  E(0xA9);
  E(0x0E);
  // PHA
  E(0x48);
  // JSL refreshScreenSubr2ALocation
  E(0x22);
  E((uint8_t)(refreshScreenSubr2ALocation));
  E((uint8_t)(refreshScreenSubr2ALocation >> 8));
  E((uint8_t)((refreshScreenSubr2ALocation >> 16) + 0xC0));
  // from $C0/152A: loop and call the above
  // REP #20
  E(0xC2);
  E(0x20);
  // TYA
  E(0x98);
  // CLC
  E(0x18);
  // ADC #0010
  E(0x69);
  E(0x10);
  E(0x00);
  // TAY
  E(0xA8);
  // SEP #20
  E(0xE2);
  E(0x20);
  // PLA
  E(0x68);
  // DEC A
  E(0x3A);
  // BNE to the PHA .. same dist +1 because JSL - ED
  E(0xD0);
  E(0xED);
  // LDY #0010
  E(0xA0);
  E(0x10);
  E(0x00);
  // code from $C0/151D
  // LDA #0E
  E(0xA9);
  E(0x0E);
  // PHA
  E(0x48);
  // JSL refreshScreenSubr2ALocation
  E(0x22);
  E((uint8_t)(refreshScreenSubr2ALocation));
  E((uint8_t)(refreshScreenSubr2ALocation >> 8));
  E((uint8_t)((refreshScreenSubr2ALocation >> 16) + 0xC0));
  // from $C0/152A: loop and call the above
  // REP #20
  E(0xC2);
  E(0x20);
  // TYA
  E(0x98);
  // CLC
  E(0x18);
  // ADC #0010
  E(0x69);
  E(0x10);
  E(0x00);
  // TAY
  E(0xA8);
  // SEP #20
  E(0xE2);
  E(0x20);
  // PLA
  E(0x68);
  // DEC A
  E(0x3A);
  // BNE to the PHA .. same dist +1 because JSL - ED
  E(0xD0);
  E(0xED);
  // prepare for next call
  // LDA #38
  E(0xA9);
  E(0x38);
  // STA $A171
  E(0x8D);
  E(0x71);
  E(0xA1);
  // LDY #9000
  E(0xA0);
  E(0x00);
  E(0x90);
  // LDX #9400
  E(0xA2);
  E(0x00);
  E(0x94);
  // JSL refreshScreenSubr2BLocation
  E(0x22);
  E((uint8_t)(refreshScreenSubr2BLocation));
  E((uint8_t)(refreshScreenSubr2BLocation >> 8));
  E((uint8_t)((refreshScreenSubr2BLocation >> 16) + 0xC0));
  // PLB
  E(0xAB);
  // LDA #x67
  E(0xA9);
  E(0x67);
  // STA 7E1D04 - trigger VRAM upload next frame
  E(0x8F);
  E(0x04);
  E(0x1D);
  E(0x7E);
  // RTL
  E(0x6B);
  int handleRightLocation = c->working_offset;
  // LDA CONSUMABLE_INPUT_TIMER
  E(0xAF);
  E((uint8_t)CRAM_CONSUMABLE_INPUT_TIMER);
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 16));
  // CMP #00
  E(0xC9);
  E(0x00);
  // BEQ over (1)
  E(0xF0);
  E(0x01);
  // RTL
  E(0x6B);
  // LDA CONSUMABLE_BUY_CURRENT
  E(0xAF);
  E((uint8_t)CRAM_CONSUMABLE_BUY_CURRENT);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 16));
  // CMP CONSUMABLE_BUY_MAX
  E(0xCF);
  E((uint8_t)CRAM_CONSUMABLE_BUY_MAX);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_MAX >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_MAX >> 16));
  // BCC/BLT handleInput (1)
  E(0x90);
  E(0x01);
  // RTL
  E(0x6B);
  // INC A
  E(0x1A);
  // STA CONSUMABLE_BUY_CURRENT
  E(0x8F);
  E((uint8_t)CRAM_CONSUMABLE_BUY_CURRENT);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 16));
  // LDA #1E
  E(0xA9);
  E(0x1E);
  // STA CONSUMABLE_INPUT_TIMER
  E(0x8F);
  E((uint8_t)CRAM_CONSUMABLE_INPUT_TIMER);
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 16));
  // JSL redrawText
  E(0x22);
  E((uint8_t)(redrawTextLocation));
  E((uint8_t)(redrawTextLocation >> 8));
  E((uint8_t)((redrawTextLocation >> 16) + 0xC0));
  // JSL $D0CF10 - play menu beep sound
  E(0x22);
  E(0x00);
  E(0xCF);
  E(0xD0);
  // RTL
  E(0x6B);
  int handleLeftLocation = c->working_offset;
  // LDA CONSUMABLE_INPUT_TIMER
  E(0xAF);
  E((uint8_t)CRAM_CONSUMABLE_INPUT_TIMER);
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 16));
  // CMP #00
  E(0xC9);
  E(0x00);
  // BEQ over (1)
  E(0xF0);
  E(0x01);
  // RTL
  E(0x6B);
  // LDA CONSUMABLE_BUY_CURRENT
  E(0xAF);
  E((uint8_t)CRAM_CONSUMABLE_BUY_CURRENT);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 16));
  // CMP #02
  E(0xC9);
  E(0x02);
  // BCS/BGE handleInput (1)
  E(0xB0);
  E(0x01);
  // RTL
  E(0x6B);
  // DEC A
  E(0x3A);
  // STA CONSUMABLE_BUY_CURRENT
  E(0x8F);
  E((uint8_t)CRAM_CONSUMABLE_BUY_CURRENT);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 16));
  // LDA #1E
  E(0xA9);
  E(0x1E);
  // STA CONSUMABLE_INPUT_TIMER
  E(0x8F);
  E((uint8_t)CRAM_CONSUMABLE_INPUT_TIMER);
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 16));
  // JSL redrawText
  E(0x22);
  E((uint8_t)(redrawTextLocation));
  E((uint8_t)(redrawTextLocation >> 8));
  E((uint8_t)((redrawTextLocation >> 16) + 0xC0));
  // JSL $D0CF10 - play menu beep sound
  E(0x22);
  E(0x00);
  E(0xCF);
  E(0xD0);
  // RTL
  E(0x6B);
  int handleUpLocation = c->working_offset;
  // LDA CONSUMABLE_INPUT_TIMER
  E(0xAF);
  E((uint8_t)CRAM_CONSUMABLE_INPUT_TIMER);
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 16));
  // CMP #00
  E(0xC9);
  E(0x00);
  // BEQ over (1)
  E(0xF0);
  E(0x01);
  // RTL
  E(0x6B);
  // LDA CONSUMABLE_BUY_MAX
  E(0xAF);
  E((uint8_t)CRAM_CONSUMABLE_BUY_MAX);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_MAX >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_MAX >> 16));
  // STA CONSUMABLE_BUY_CURRENT
  E(0x8F);
  E((uint8_t)CRAM_CONSUMABLE_BUY_CURRENT);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 16));
  // LDA #1E
  E(0xA9);
  E(0x1E);
  // STA CONSUMABLE_INPUT_TIMER
  E(0x8F);
  E((uint8_t)CRAM_CONSUMABLE_INPUT_TIMER);
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 16));
  // JSL redrawText
  E(0x22);
  E((uint8_t)(redrawTextLocation));
  E((uint8_t)(redrawTextLocation >> 8));
  E((uint8_t)((redrawTextLocation >> 16) + 0xC0));
  // JSL $D0CF10 - play menu beep sound
  E(0x22);
  E(0x00);
  E(0xCF);
  E(0xD0);
  // RTL
  E(0x6B);
  int handleDownLocation = c->working_offset;
  // LDA CONSUMABLE_INPUT_TIMER
  E(0xAF);
  E((uint8_t)CRAM_CONSUMABLE_INPUT_TIMER);
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 16));
  // CMP #00
  E(0xC9);
  E(0x00);
  // BEQ over (1)
  E(0xF0);
  E(0x01);
  // RTL
  E(0x6B);
  // LDA #01
  E(0xA9);
  E(0x01);
  // STA CONSUMABLE_BUY_CURRENT
  E(0x8F);
  E((uint8_t)CRAM_CONSUMABLE_BUY_CURRENT);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 16));
  // LDA #1E
  E(0xA9);
  E(0x1E);
  // STA CONSUMABLE_INPUT_TIMER
  E(0x8F);
  E((uint8_t)CRAM_CONSUMABLE_INPUT_TIMER);
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 16));
  // JSL redrawText
  E(0x22);
  E((uint8_t)(redrawTextLocation));
  E((uint8_t)(redrawTextLocation >> 8));
  E((uint8_t)((redrawTextLocation >> 16) + 0xC0));
  // JSL $D0CF10 - play menu beep sound
  E(0x22);
  E(0x00);
  E(0xCF);
  E(0xD0);
  // RTL
  E(0x6B);
  /*
  * replace:
  $C0/7E83 E2 20       SEP #$20                A:0000 X:0000 Y:002C P:envmxdIZc
  $C0/7E85 A9 40       LDA #$40                A:0000 X:0000 Y:002C P:envMxdIZc
  */
  P(0x7E83, 0x22);
  P(0x7E84, (uint8_t)(c->working_offset));
  P(0x7E85, (uint8_t)(c->working_offset >> 8));
  P(0x7E86, (uint8_t)((c->working_offset >> 16) + 0xC0));
  // main sub:
  // 8 bit A, 16 bit x/y
  E(0xE2);
  E(0x20);
  // LDA CONSUMABLE_BUY_MAX
  E(0xAF);
  E((uint8_t)CRAM_CONSUMABLE_BUY_MAX);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_MAX >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_MAX >> 16));
  // CMP #00
  E(0xC9);
  E(0x00);
  // BNE over (6)
  E(0xD0);
  E(0x05);
  // replaced code
  E(0xE2);
  E(0x20);
  E(0xA9);
  E(0x40);
  // RTL
  E(0x6B);
  // over:
  // LDA CONSUMABLE_INPUT_TIMER
  E(0xAF);
  E((uint8_t)CRAM_CONSUMABLE_INPUT_TIMER);
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 16));
  // CMP #00
  E(0xC9);
  E(0x00);
  // BEQ over2 (7)
  E(0xF0);
  E(0x07);
  // SEC
  E(0x38);
  // SBC #01
  E(0xE9);
  E(0x01);
  // STA CONSUMABLE_INPUT_TIMER
  E(0x8F);
  E((uint8_t)CRAM_CONSUMABLE_INPUT_TIMER);
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 16));
  // over2:
  // LDA 7E1801
  E(0xAF);
  E(0x01);
  E(0x18);
  E(0x7E);
  // AND #01
  E(0x29);
  E(0x01);
  // BEQ 04
  E(0xF0);
  E(0x04);
  // JSL handleRight
  E(0x22);
  E((uint8_t)(handleRightLocation));
  E((uint8_t)(handleRightLocation >> 8));
  E((uint8_t)((handleRightLocation >> 16) + 0xC0));
  // LDA 7E1801
  E(0xAF);
  E(0x01);
  E(0x18);
  E(0x7E);
  // AND #02
  E(0x29);
  E(0x02);
  // BEQ 04
  E(0xF0);
  E(0x04);
  // JSL handleLeft
  E(0x22);
  E((uint8_t)(handleLeftLocation));
  E((uint8_t)(handleLeftLocation >> 8));
  E((uint8_t)((handleLeftLocation >> 16) + 0xC0));
  // LDA 7E1801
  E(0xAF);
  E(0x01);
  E(0x18);
  E(0x7E);
  // AND #04
  E(0x29);
  E(0x04);
  // BEQ 04
  E(0xF0);
  E(0x04);
  // JSL handleDown
  E(0x22);
  E((uint8_t)(handleDownLocation));
  E((uint8_t)(handleDownLocation >> 8));
  E((uint8_t)((handleDownLocation >> 16) + 0xC0));
  // LDA 7E1801
  E(0xAF);
  E(0x01);
  E(0x18);
  E(0x7E);
  // AND #08
  E(0x29);
  E(0x08);
  // BEQ 04
  E(0xF0);
  E(0x04);
  // JSL handleUp
  E(0x22);
  E((uint8_t)(handleUpLocation));
  E((uint8_t)(handleUpLocation >> 8));
  E((uint8_t)((handleUpLocation >> 16) + 0xC0));
  // LDA 7E1801
  E(0xAF);
  E(0x01);
  E(0x18);
  E(0x7E);
  // AND #0F
  E(0x29);
  E(0x0F);
  // BNE xx (6)
  E(0xD0);
  E(0x06);
  // LDA #00
  E(0xA9);
  E(0x00);
  // STA CONSUMABLE_INPUT_TIMER
  E(0x8F);
  E((uint8_t)CRAM_CONSUMABLE_INPUT_TIMER);
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 16));
  // xx:
  // replaced code
  E(0xE2);
  E(0x20);
  E(0xA9);
  E(0x40);
  // RTL
  E(0x6B);
  // ///////////////////////////////////////////////////////////////////
  // logic for confirmation handling - gold subtraction
  // if CONSUMABLE_BUY_CURRENT > 0
  //   gold -= CONSUMABLE_BUY_CURRENT * price (use LDA $01,s then multiply stuff)
  //   BUYING_MULTIPLE_ITEMS = 1 - mark this here so the item addition below knows the difference between a buy and a drop
  // else
  //   do original subtraction - 7 bytes from above
  // ///////////////////////////////////////////////////////////////////
  // when sbc, carry is cleared if we over(under)flow
  /*
  *  replace:
  $C0/7C8C AF 6A CC 7E LDA $7ECC6A[$7E:CC6A]   A:000A X:0000 Y:000A P:envmxdIZc
  $C0/7C90 38          SEC                     A:000A X:0000 Y:000A P:envmxdIzc
  $C0/7C91 E3 01       SBC $01,s  [$00:01E3]   A:000A X:0000 Y:000A P:envmxdIzC
  */
  P(0x7C8C, 0x22);
  P(0x7C8D, (uint8_t)(c->working_offset));
  P(0x7C8E, (uint8_t)(c->working_offset >> 8));
  P(0x7C8F, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x7C90, 0xEA);
  P(0x7C91, 0xEA);
  P(0x7C92, 0xEA);
  // SEP 20
  E(0xE2);
  E(0x20);
  // LDA 7E1847 - buy menu type
  E(0xAF);
  E(0x47);
  E(0x18);
  E(0x7E);
  // CMP #01
  E(0xC9);
  E(0x01);
  // BEQ over
  E(0xF0);
  E(0x0A);
  // REP 20
  E(0xC2);
  E(0x20);
  // replaced code [7 bytes]
  E(0xAF);
  E(0x6A);
  E(0xCC);
  E(0x7E);
  E(0x38);
  E(0xE3);
  E(0x04);  // modified due to stack thing
  // RTL
  E(0x6B);
  // LDA CONSUMABLE_BUY_CURRENT
  E(0xAF);
  E((uint8_t)CRAM_CONSUMABLE_BUY_CURRENT);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 16));
  // CMP #02
  E(0xC9);
  E(0x02);
  // BGE over
  E(0xB0);
  E(0x0A);
  // REP 20
  E(0xC2);
  E(0x20);
  // replaced code [7 bytes]
  E(0xAF);
  E(0x6A);
  E(0xCC);
  E(0x7E);
  E(0x38);
  E(0xE3);
  E(0x04);  // modified due to stack thing
  // RTL
  E(0x6B);
  // over:
  // LDA #01
  E(0xA9);
  E(0x01);
  // STA BUYING_MULTIPLE_ITEMS
  E(0x8F);
  E((uint8_t)CRAM_BUYING_MULTIPLE_ITEMS);
  E((uint8_t)(CRAM_BUYING_MULTIPLE_ITEMS >> 8));
  E((uint8_t)(CRAM_BUYING_MULTIPLE_ITEMS >> 16));
  // PHX - save old x
  E(0xDA);
  // REP 20
  E(0xC2);
  E(0x20);
  // LDA CONSUMABLE_BUY_CURRENT
  E(0xAF);
  E((uint8_t)CRAM_CONSUMABLE_BUY_CURRENT);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 16));
  // AND #00FF
  E(0x29);
  E(0xFF);
  E(0x00);
  // TAX
  E(0xAA);
  // LDA 7ecc6a - current gold LSBs
  E(0xAF);
  E(0x6A);
  E(0xCC);
  E(0x7E);
  // loop:
  // SEC
  E(0x38);
  // SBC $06,s - price already pushed outside the modified code
  E(0xE3);
  E(0x06);
  // BCS skipUnderflow
  E(0xB0);
  E(0x13 + 3);
  // PHA
  E(0x48);
  // SEP 20
  E(0xE2);
  E(0x20);
  // LDA 7ecc6c - handle underflow; subtract one from gold msb
  E(0xAF);
  E(0x6C);
  E(0xCC);
  E(0x7E);
  // DEC A
  E(0x3A);
  // BPL noError
  E(0x10);
  E(0x05);
  // REP 20
  E(0xC2);
  E(0x20);
  // PLA
  E(0x68);
  // PLX - underflowed gold; return
  E(0xFA);
  // RTL -
  E(0x6B);
  // noError:
  // STA 7ecc6c - store gold msb back
  E(0x8F);
  E(0x6C);
  E(0xCC);
  E(0x7E);
  // REP 20
  E(0xC2);
  E(0x20);
  // PLA
  E(0x68);
  // skipUnderflow:
  // DEX
  E(0xCA);
  // CPX #0001 - to fix off by one
  E(0xE0);
  E(0x01);
  E(0x00);
  // BEQ done (2)
  E(0xF0);
  E(0x02);
  // BRA loop
  E(0x80);
  E(0xE0 - 3);
  // done:
  // STA 7ECC6A - save current gold
  E(0x8F);
  E(0x6A);
  E(0xCC);
  E(0x7E);
  // SEC - so the next code doesn't handle underflow
  E(0x38);
  // PLX - value we pushed earlier
  E(0xFA);
  // RTL
  E(0x6B);
  // ///////////////////////////////////////////////////////////////////
  // logic for confirmation handling - item addition
  // if CONSUMABLE_BUY_CURRENT > 0 and BUYING_MULTIPLE_ITEMS == 1
  //   LDA $7ECC00,x[$7E:CC48] to pull current value as in replaced code
  //   while CONSUMABLE_BUY_CURRENT > 0
  //     ADC #20
  //     CONSUMABLE_BUY_CURRENT--
  //   CONSUMABLE_BUY_MAX = 0
  //   CONSUMABLE_INPUT_TIMER = 0
  //   BUYING_MULTIPLE_ITEMS = 0
  // else
  //   do original code - 7 bytes
  // ///////////////////////////////////////////////////////////////////
  /*
  replace:
  $C0/657E 0A          ASL A                   A:0000 X:0048 Y:0000 P:envMxdIZc
  $C0/657F 0A          ASL A                   A:0000 X:0048 Y:0000 P:envMxdIZc
  $C0/6580 0A          ASL A                   A:0000 X:0048 Y:0000 P:envMxdIZc
  $C0/6581 0A          ASL A                   A:0000 X:0048 Y:0000 P:envMxdIZc
  */
  P(0x657E, 0x22);
  P(0x657F, (uint8_t)(c->working_offset));
  P(0x6580, (uint8_t)(c->working_offset >> 8));
  P(0x6581, (uint8_t)((c->working_offset >> 16) + 0xC0));
  // PHA
  E(0x48);
  // PHX
  E(0xDA);
  // SEP 20
  E(0xE2);
  E(0x20);
  // LDA BUYING_MULTIPLE_ITEMS
  E(0xAF);
  E((uint8_t)CRAM_BUYING_MULTIPLE_ITEMS);
  E((uint8_t)(CRAM_BUYING_MULTIPLE_ITEMS >> 8));
  E((uint8_t)(CRAM_BUYING_MULTIPLE_ITEMS >> 16));
  // CMP #01
  E(0xC9);
  E(0x01);
  // BNE to the BRA
  E(0xD0);
  E(0x08);
  // LDA CONSUMABLE_BUY_CURRENT
  E(0xAF);
  E((uint8_t)CRAM_CONSUMABLE_BUY_CURRENT);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 16));
  // CMP #02
  E(0xC9);
  E(0x02);
  // BNE over [C] -> BGE
  E(0xB0);
  E(0x02);
  // BRA end
  E(0x80);
  E(0x15);
  // over:
  // REP 20
  E(0xC2);
  E(0x20);
  // AND #00FF
  E(0x29);
  E(0xFF);
  E(0x00);
  // TAX
  E(0xAA);
  // SEP 20
  E(0xE2);
  E(0x20);
  // LDA $xx,s
  E(0xA3);
  E(0x07);
  // back:
  // CLC
  E(0x18);
  // ADC #20
  E(0x69);
  E(0x20);
  // DEX
  E(0xCA);
  // CPX #0001 - to fix off by one
  E(0xE0);
  E(0x01);
  E(0x00);
  // BNE back
  E(0xD0);
  E(0xF7);
  // STA $xx,s
  E(0x83);
  E(0x07);
  // LDA #00
  E(0xA9);
  E(0x00);
  // STA CONSUMABLE_BUY_MAX
  E(0x8F);
  E((uint8_t)CRAM_CONSUMABLE_BUY_MAX);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_MAX >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_MAX >> 16));
  // STA CONSUMABLE_BUY_CURRENT
  E(0x8F);
  E((uint8_t)CRAM_CONSUMABLE_BUY_CURRENT);
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_BUY_CURRENT >> 16));
  // STA CONSUMABLE_INPUT_TIMER
  E(0x8F);
  E((uint8_t)CRAM_CONSUMABLE_INPUT_TIMER);
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 8));
  E((uint8_t)(CRAM_CONSUMABLE_INPUT_TIMER >> 16));
  // STA BUYING_MULTIPLE_ITEMS
  E(0x8F);
  E((uint8_t)CRAM_BUYING_MULTIPLE_ITEMS);
  E((uint8_t)(CRAM_BUYING_MULTIPLE_ITEMS >> 8));
  E((uint8_t)(CRAM_BUYING_MULTIPLE_ITEMS >> 16));
  // PLX
  E(0xFA);
  // PLA
  E(0x68);
  // replaced code
  E(0x0A);
  E(0x0A);
  E(0x0A);
  E(0x0A);
  // RTL
  E(0x6B);
  // buying confirmation text
  /*
  *  replace:
  $C0/17C1 E2 20       SEP #$20                A:FEF3 X:0005 Y:FEF3 P:eNvmxdIzc
  $C0/17C3 9C D0 A1    STZ $A1D0  [$7E:A1D0]   A:FEF3 X:0005 Y:FEF3 P:eNvMxdIzc
  */
  P(0x17C1, 0x22);
  P(0x17C2, (uint8_t)(c->working_offset));
  P(0x17C3, (uint8_t)(c->working_offset >> 8));
  P(0x17C4, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x17C5, 0xEA);
  // SEP 20
  E(0xE2);
  E(0x20);
  // STZ $A1D0
  E(0x9C);
  E(0xD0);
  E(0xA1);
  // CPY #FEF3 - i don't remember what value this represents - some state being checked to make sure we're buying items
  E(0xC0);
  E(0xF3);
  E(0xFE);
  // BEQ over
  E(0xF0);
  E(0x01);
  // RTL
  E(0x6B);
  // LDA #09
  E(0xA9);
  E(0x09);
  // STA $A1D0
  E(0x8D);
  E(0xD0);
  E(0xA1);
  // LDA $1847
  E(0xAD);
  E(0x47);
  E(0x18);
  // CMP #01 - buy menu id
  E(0xC9);
  E(0x01);
  // BEQ 01
  E(0xF0);
  E(0x01);
  // RTL - not in buy menu
  E(0x6B);
  // also have to check that we're buying a consumable
  // REP 20
  E(0xC2);
  E(0x20);
  // LDA 7E183C - shop offset
  E(0xAF);
  E(0x3C);
  E(0x18);
  E(0x7E);
  // CLC
  E(0x18);
  // SEP 20
  E(0xE2);
  E(0x20);
  // ADC 7E180A - ring menu selection index
  E(0x6F);
  E(0x0A);
  E(0x18);
  E(0x7E);
  // REP 20
  E(0xC2);
  E(0x20);
  // TAX - shop offset + ring menu selection index to x
  E(0xAA);
  // SEP #20
  E(0xE2);
  E(0x20);
  // LDA $D80000,x - load from shop data
  E(0xBF);
  E(0x00);
  E(0x00);
  E(0xD8);
  // CMP #BA - check for consumable id
  E(0xC9);
  E(0xBA);
  // BCS/BGE over
  E(0xB0);
  E(0x01);
  // RTL if not a consumable
  E(0x6B);
  // JSL redrawText - render the new text
  E(0x22);
  E((uint8_t)(redrawTextLocation));
  E((uint8_t)(redrawTextLocation >> 8));
  E((uint8_t)((redrawTextLocation >> 16) + 0xC0));
  // RTL
  E(0x6B);
  return 1;
}

