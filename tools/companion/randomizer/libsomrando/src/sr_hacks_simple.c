/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Straight-line hacks translated mechanically from the C# by tools/transpile_simple.py (then reviewed).  Author of every upstream hack:
 * Moppleton.  Each function names its upstream file; comments are those of the C#.  Translated to C 2026-10-18.
 */
#include <string.h>

#include "sr_core.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))
#define SB(key) sr_settings_get_bool(c->settings, (key))

/* Remove energy cost for running - upstream processing/hacks/common/other/NoEnergyToRun.cs */
int sr_hack_no_energy_to_run(struct sr_ctx *c) {
  if (!SB("noStaminaRun")) return 0;
  // this is from the old secret of mana editor
  // MOPPLE: this removes the spending of stamina, but not the requirement to have 100 first
  // STA $E1ED,X -> remove
  P(0xB78A, 0xEA);
  P(0xB78B, 0xEA);
  P(0xB78C, 0xEA);
  return 1;
}

/* Increase max consumables to 7 - upstream processing/hacks/common/qol/MaxItemsIncrease.cs */
int sr_hack_max_items_increase(struct sr_ctx *c) {
  if (!SB("moreItems")) return 0;
  // $C0/6559 C9 9F       CMP #$9F                A:00A3 X:004B Y:0003 P:eNvMxdIzc - remove check, allowing higher values
  // $C0/655B B0 EA BCS $EA[$6547]                A:00A3 X:004B Y:0003 P:envMxdIzC
  P(0x6559, 0xEA);
  P(0x655A, 0xEA);
  return 1;
}

/* Attacking doesn't cost stamina - upstream processing/hacks/common/other/NoEnergyToAttack.cs */
int sr_hack_no_energy_to_attack(struct sr_ctx *c) {
  if (!SB("attacksForever")) return 0;
  // store zero into stamina remaining instead of the usual amount
  // STA $E1ED,X -> STZ $E1ED,X
  P(0x2C24A, 0x9E);
  // STA $E1ED,X -> STZ $E1ED,X
  // note that FasterEnemies overwrites this later if it's enabled
  P(0xF944, 0x9E);
  return 1;
}

/* Beak disable for Spring/Axe Beak - upstream processing/hacks/common/qol/BeakShieldDisable.cs */
int sr_hack_beak_shield_disable(struct sr_ctx *c) {
  if (!SB("hittableBeaks")) return 0;
  // this is some sort of hitbox data, i currently don't have docs for the format of it
  // 07 -> 00
  P(0x7F7F0, 0);
  // 0B -> 00
  P(0x7F7F1, 0);
  return 1;
}

/* Faster top of screen dialogue - upstream processing/hacks/common/other/FasterTopOfScreenDialogue.cs */
int sr_hack_faster_top_of_screen_dialogue(struct sr_ctx *c) {
  if (!SB("fasterMessages")) return 0;
  /*
  $C0/5CF3 A9 60       LDA #$60                A:0000 X:61AE Y:005A P:envMxdIzc
  $C0/5CF5 99 00 FE    STA $FE00,y[$7E:FE5A]   A:0060 X:61AE Y:005A P:envMxdIzc
  */
  // faster top-screen dialogues (was 0x60)
  P(0x5CF4, 0x30);
  // MOPPLE: there may be more spots? this doesn't seem to speed up everything
  return 1;
}

/* Whip coordinate fix - upstream processing/hacks/common/fix/WhipFix.cs */
int sr_hack_whip_fix(struct sr_ctx *c) {
  if (!SB("whipFix")) return 0;
  // limit a loop writing palettes that overwrites some other data
  P(0x586A, 0x1D);
  // 14D8C7 - length of whip? downward only.  normally 3C, change to 4C
  P(0x14D8C7, 0x4C);
  return 1;
}

/* Use candy and chocolate on 0 HP characters - upstream processing/hacks/common/qol/CandyHealouts.cs */
int sr_hack_candy_healouts(struct sr_ctx *c) {
  if (!SB("candyHealouts")) return 0;
  /*
  C0/5539:   B982E1  LDA $E182,Y - load target character hp
  C0/553C:   F00E    BEQ $554C - branch if zero to code disallowing candy to be used - remove
  */
  P(0x553C, 0xEA);
  P(0x553D, 0xEA);
  return 1;
}

/* Accurate damage percentages - upstream processing/hacks/common/qol/DamagePercentFix.cs */
int sr_hack_damage_percent_fix(struct sr_ctx *c) {
  if (!SB("accuratePercent")) return 0;
  // this is from the old secret of mana editor
  // x5383, put EA EA
  /*
  C0/5383:   4689   LSR $89 - divide damage by two
  */
  P(0x5383, 0xEA);
  P(0x5384, 0xEA);
  return 1;
}

/* Faster door transitions - upstream processing/hacks/common/other/FastTransitions.cs */
int sr_hack_fast_transitions(struct sr_ctx *c) {
  if (!SB("fastTransition")) return 0;
  // $00/8B75 A5 E6       LDA $E6    [$00:00E6]   A:0001 X:0020 Y:00FE P:envMXdIzC
  // $00 / 8B77 10 02       BPL $02[$8B7B]      A: 0005 X: 0020 Y: 00FE P:envMXdIzC
  // $00 / 8B7B C5 E7 CMP $E7[$00:00E7]   A: 0005 X: 0020 Y: 00FE P:envMXdIzC
  // screen transition fades
  P(0x8B76, 0xE7);
  return 1;
}

/* Chests open without shaking - upstream processing/hacks/common/qol/ChestOpeningSpeedup.cs */
int sr_hack_chest_opening_speedup(struct sr_ctx *c) {
  if (!SB("fasterChests")) return 0;
  // from zhadeth's research:
  // "Ok got chests to open faster, it indeed is waiting for animations to finish, both the character and the chest. To skip it, put FF (anim code for 'End of animation') at these offsets:
  // D15FB8(PCs: Breaking Lock)
  // D1ED01(Chest: Lock Beign Broken(Left / Right))
  // D1EC84(Chest: Lock Beign Broken(Up / Down)) "
  P(0x115FB8, 0xFF);
  P(0x11ED01, 0xFF);
  P(0x11EC84, 0xFF);
  return 1;
}

/* Extend status lengths - upstream processing/hacks/common/other/StatusLengths.cs */
int sr_hack_status_lengths(struct sr_ctx *c) {
  if (!SB("longStatus")) return 0;
  // vanilla loading the timer for statuses and &ing it with 0x03 to determine when to tick down individual
  // status timers.  modify the constant we "and" by to extend status timers
  //    $C0/3C38 BD B9 E1    LDA $E1B9,x[$7E:E3B9]   A:0000 X:0200 Y:0056 P:envMxdIzC
  //    $C0/3C3B 29 03       AND #$03                A:0074 X:0200 Y:0056 P:envMxdIzC
  //    $C0/3C3D D0 48       BNE $48    [$3C87]
  // quadruple status tick length
  P(0x3C3C, 0x0F);
  // half weapon-initiated status lengths (which were quadrupled)
  P(0x8E3C1, 0x0A);
  return 1;
}

/* Sound effects channel reduction - upstream processing/hacks/common/other/SoundEffectsReduction.cs */
int sr_hack_sound_effects_reduction(struct sr_ctx *c) {
  if (!SB("seReduction")) return 0;
  // note this is a patch to SPC700 code, which is an entirely different instruction set from normal SNES code
  // change
  // 0E7D D0 ED     BNE $0E6C
  // to
  // 0E7D 2F 01     BRA $0E80
  // i don't remember exactly why this works, but it limits the number of channels that SPC code will choose
  // to allocate sound effects to.
  // this is part of an audio block in the ROM that gets loaded into SPC memory on game startup.
  P(0x313C7, 0x2F);
  P(0x313C8, 0x01);
  return 1;
}

/* Starter gear in each slot - upstream processing/hacks/common/other/StarterGear.cs */
int sr_hack_starter_gear(struct sr_ctx *c) {
  if (!SB("starterGear")) return 0;
  // starting gear - note that weapons are also in here, we aren't setting them (here) though
  P(0x57BD, 0x01);  // bandana
  P(0x57BE, 0x16);  // overalls
  P(0x57BF, 0x3E);  // wristband
  P(0x57C1, 0x02);  // hair ribbon
  P(0x57C2, 0x17);  // kungfu suit
  P(0x57C3, 0x3E);  // wristband
  P(0x57C5, 0x03);  // rabite cap
  P(0x57C6, 0x18);  // midge robe
  P(0x57C7, 0x2C);  // elbow pad - wristband fucks up for sprite and overlaps with trash can value (hence not being equippable)
  return 1;
}

/* Obscure own gold amount - upstream processing/hacks/common/other/ObscureGoldAmount.cs */
int sr_hack_obscure_gold_amount(struct sr_ctx *c) {
  if (!SB("obscureGold")) return 0;
  // status menu:
  // $C7/69A7 BD 6A CC    LDA $CC6A,x[$7E:CC6A]   A:0400 X:0000 Y:0000 P:envMxdIZc
  // LDA #00 instead
  P(0x769A7, 0xA9);
  P(0x769A8, 0x00);
  P(0x769A9, 0xEA);
  // events:
  // $C0/19A2 BD 6A CC    LDA $CC6A,x[$7E:CC6A]   A:04CC X:0000 Y:0000 P:eNvMxdIzc
  // LDA #00 instead
  P(0x19A2, 0xA9);
  P(0x19A3, 0x00);
  P(0x19A4, 0xEA);
  return 1;
}

/* Mode 7 minor graphical fix - upstream processing/hacks/common/other/Mode7EdgesFix.cs */
int sr_hack_mode7_edges_fix(struct sr_ctx *c) {
  if (!SB("mode7Fix")) return 0;
  // minor adjustments to mode7 calculations
  // this is a really old hack and i don't remember exactly what it did
  // but it fixed some sort of graphical issue at the bottom corners of flammie flight
  /*
  $00/90A3 A9 0F       LDA #$0F                A:0000 X:00A0 Y:0000 P:envMXdiZc **
  $00/90A5 8D 05 42    STA $4205  [$00:4205]   A:000F X:00A0 Y:0000 P:envMXdizc
  ..
  $00/90B3 38          SEC                     A:00D0 X:00A0 Y:0000 P:eNvMXdizc
  $00/90B4 E9 28       SBC #$28                A:00D0 X:00A0 Y:0000 P:eNvMXdizC **
  $00/90B6 85 07       STA $07    [$00:0007]   A:00A8 X:00A0 Y:0000 P:eNvMXdizC
  */
  P(0x90A4, 0x08);
  P(0x90B5, 0x00);
  return 1;
}

/* Vampire hittable when cloaked - upstream processing/hacks/common/qol/HittableVampire.cs */
int sr_hack_hittable_vampire(struct sr_ctx *c) {
  if (!SB("noBsVampires")) return 0;
  // the vampire closed-cape frame w/o hitbox can be found at 0xACAFC, as referenced at 0x17358 and other spots
  // i don't remember how this data works, but changing the last line of it to 01 01 01 01 00 00 00 00 gave him a hitbox
  // in this frame:
  // CAFC - CB46:
  // 10 00
  // F0 C0 06 1A 00 C0 06 5A
  // E0 D0 2A 1A 10 D0 2A 5A
  // F0 D0 2C 1A 00 D0 2C 5A
  // E0 E0 40 1A 10 E0 40 5A
  // F0 E0 42 1A 00 E0 42 5A
  // E0 F0 44 1A 10 F0 44 5A
  // F0 F0 46 1A 00 F0 46 5A
  // F0 00 2E 1A 00 00 2E 5A
  // 00 00 00 00 11 17 00 DD
  // ^^^ change to:
  // 01 01 01 01 00 00 00 00
  // for hittable vampires. buffy shares the same data.
  P(0xACB3E, 0x01);
  P(0xACB3F, 0x01);
  P(0xACB40, 0x01);
  P(0xACB41, 0x01);
  P(0xACB42, 0x00);
  P(0xACB43, 0x00);
  P(0xACB44, 0x00);
  P(0xACB45, 0x00);
  return 1;
}

/* One-hit KO - upstream processing/hacks/common/other/OneHitKo.cs */
int sr_hack_one_hit_ko(struct sr_ctx *c) {
  if (!SB("ohko")) return 0;
  // player hp = 1, def, mdef=0
  for (int i = 0; i < 99; i++)
  {
  // boy
  P(0x104210 + i * 8, 1);  // hp lsb
  P(0x104210 + i * 8 + 1, 0);  // hp msb
  P(0x104210 + i * 8 + 5, 0);  // con
  // girl
  P(0x104528 + i * 8, 1);  // hp lsb
  P(0x104528 + i * 8 + 1, 0);  // hp msb
  P(0x104528 + i * 8 + 5, 0);  // con
  // sprite
  P(0x104840 + i * 8, 1);  // hp lsb
  P(0x104840 + i * 8 + 1, 0);  // hp msb
  P(0x104840 + i * 8 + 5, 0);  // con
  }
  // armor def,mdef=0
  for (int i = 0; i <= 0x3E; i++)
  {
  P(0x103ED0 + i * 10 + 1, 0);  // def
  P(0x103ED0 + i * 10 + 3, 0);  // mdef
  }
  return 1;
}

/* Mech rider death fix - upstream processing/hacks/common/fix/MechRiderDeathFix.cs */
int sr_hack_mech_rider_death_fix(struct sr_ctx *c) {
  if (!SB("mechRiderDeathFix")) return 0;
  // this removes the entry in his table defining the animation to run when he dies
  // from the mana banks docs:
  /*
  [EB49~ECFC Mech Rider I/II/III]
  [07: Mech Rider I/II/III]
  C2/EB49:	019C
  C2/EB4B:	0000
  C2/EB4D:	B370
  C2/EB4F:	C570
  C2/EB51:	1071
  C2/EB53:	0000
  C2/EB55:	7BEB		[??]
  C2/EB57:	6BEB		[??]
  C2/EB59:	9DEB		[AI Pointers]
  C2/EB5B:	0400
  C2/EB5D:	7272
  C2/EB5F:	D0FF
  C2/EB61:	63EB		[Extra]
  [Extra]
  C2/EB63:	7D72
  C2/EB65:	C172
  C2/EB67:	FF72
  C2/EB69:	FF72
  */
  P(0x2EB61, 0x00);
  P(0x2EB62, 0x00);
  return 1;
}

/* Display zero damage for evades - upstream processing/hacks/common/other/DisplayEvadesAsZero.cs */
int sr_hack_display_evades_as_zero(struct sr_ctx *c) {
  if (!SB("showEvades")) return 0;
  // MOPPLE: this doesn't show misses/evades as zero in all cases.  needs more work!
  // relevant block of code here.  set carry to indicate display on return.
  /*
  $C0/50AB 8F 02 42 00 STA $004202[$00:4202]   A:0FF5 X:0600 Y:0200 P:envMxdIzC
  $C0/50AF A5 8E       LDA $8E    [$00:038E]   A:0FF5 X:0600 Y:0200 P:envMxdIzC
  $C0/50B1 4A          LSR A                   A:0F32 X:0600 Y:0200 P:envMxdIzC
  $C0/50B2 8F 03 42 00 STA $004203[$00:4203]   A:0F19 X:0600 Y:0200 P:envMxdIzc
  $C0/50B6 85 A4       STA $A4    [$00:03A4]   A:0F19 X:0600 Y:0200 P:envMxdIzc
  $C0/50B8 EA          NOP                     A:0F19 X:0600 Y:0200 P:envMxdIzc
  $C0/50B9 EA          NOP                     A:0F19 X:0600 Y:0200 P:envMxdIzc
  $C0/50BA EA          NOP                     A:0F19 X:0600 Y:0200 P:envMxdIzc
  $C0/50BB EA          NOP                     A:0F19 X:0600 Y:0200 P:envMxdIzc
  $C0/50BC AF 17 42 00 LDA $004217[$00:4217]   A:0F19 X:0600 Y:0200 P:envMxdIzc
  $C0/50C0 18          CLC                     A:0F17 X:0600 Y:0200 P:envMxdIzc
  $C0/50C1 65 A4       ADC $A4    [$00:03A4]   A:0F17 X:0600 Y:0200 P:envMxdIzc
  $C0/50C3 85 A4       STA $A4    [$00:03A4]   A:0F30 X:0600 Y:0200 P:envMxdIzc
  $C0/50C5 A5 8D       LDA $8D    [$00:038D]   A:0F30 X:0600 Y:0200 P:envMxdIzc
  $C0/50C7 C5 A4       CMP $A4    [$00:03A4]   A:0F2B X:0600 Y:0200 P:envMxdIzc
  $C0/50C9 60          RTS                     A:0F2B X:0600 Y:0200 P:eNvMxdIzc
  */
  // - 0 damage display for boss evades? or dodge sound? 50c7 -> 38 (sec), 50c8 -> ea (nop)
  P(0x50c7, 0x38);
  P(0x50c8, 0xEA);
  return 1;
}

/* Gigases never split into those sparkly things - upstream processing/hacks/common/other/GigasesNeverSplit.cs */
int sr_hack_gigases_never_split(struct sr_ctx *c) {
  if (!SB("noGigasSplit")) return 0;
  // this is a small change to the table of gigas actions.  see below for the original bank doc:
  /*
  [03: Frost/Fire/Thunder Gigas]
  C2/DF9E:	119C
  C2/DFA0:	0000
  C2/DFA2:	B862
  C2/DFA4:	C262
  C2/DFA6:	0F63
  C2/DFA8:	0000
  C2/DFAA:	B8DF		[??]
  C2/DFAC:	BADF		[??]
  C2/DFAE:	E7DF		[AI Pointer]
  C2/DFB0:	0400
  C2/DFB2:	AB63
  C2/DFB4:	E0FF
  C2/DFB6:	0000
  [??]
  C2/DFB8:	00FF
  [??]
  C2/DFBA:	10FF 00FF 02FF 01FF 0412 0605 13FF 14FF   ** 04 -> FF
  C2/DFCA:	07FF 08FF 0BFF 0CFF 16FF 17FF 1918 FF19
  C2/DFDA:	0DFF 190E FF1A FF1B FF1C FF1D FF
  */
  P(0x2DFC2, 0xFF);
  return 1;
}

/* Faster magic levels - upstream processing/hacks/common/qol/FasterMagicLevels.cs */
int sr_hack_faster_magic_levels(struct sr_ctx *c) {
  if (!SB("fasterMagicLevel")) return 0;
  // increment magic level in vanilla..
  // subtract level from 9 to determine increment amount
  // $D0/4DEC A9 09       LDA #$09                A:0800 X:0000 Y:0200 P:eNvMxdIzc
  // $D0/4DEE 38          SEC                     A:0809 X:0000 Y:0200 P:envMxdIzc
  // $D0/4DEF E5 9D       SBC $9D[$00:039D]       A:0809 X:0000 Y:0200 P:envMxdIzC
  // $D0/4DF1 85 AC       STA $AC[$00:03AC]       A:0809 X:0000 Y:0200 P:envMxdIzC
  P(0x104DEC, 0x22);
  P(0x104DED, (uint8_t)(c->working_offset));
  P(0x104DEE, (uint8_t)(c->working_offset >> 8));
  P(0x104DEF, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x104DF0, 0xEA);
  P(0x104DF1, 0xEA);
  P(0x104DF2, 0xEA);
  // LDA #09 (vanilla)
  E(0xA9);
  E(0x09);
  // SEC A (vanilla)
  E(0x38);
  // SBC $9D (vanilla)
  E(0xE5);
  E(0x9D);
  // ASL twice to multiply by 4
  E(0x0A);
  E(0x0A);
  // STA $AC (vanilla)
  E(0x85);
  E(0xAC);
  // RTL
  E(0x6B);
  return 1;
}

/* Faster weapon levels - upstream processing/hacks/common/qol/FasterWeaponLevels.cs */
int sr_hack_faster_weapon_levels(struct sr_ctx *c) {
  if (!SB("fasterWeaponLevel")) return 0;
  // increment weapon level in vanilla..
  // subtract level from 9 to determine increment amount
  // $C0/4358 A9 09       LDA #$09                A:0002 X:0800 Y:0200 P:envMxdIzc
  // $C0/435A 38          SEC                     A:0009 X:0800 Y:0200 P:envMxdIzc
  // $C0/435B F9 9C E1    SBC $E19C,y[$7E:E39C]   A:0009 X:0800 Y:0200 P:envMxdIzC
  // $C0/435E 85 A6 STA $A6[$00:03A6]             A:0009 X:0800 Y:0200 P:envMxdIzC
  P(0x4358, 0x22);
  P(0x4359, (uint8_t)(c->working_offset));
  P(0x435A, (uint8_t)(c->working_offset >> 8));
  P(0x435B, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x435C, 0xEA);
  P(0x435D, 0xEA);
  P(0x435E, 0xEA);
  P(0x435F, 0xEA);
  // LDA #09 (vanilla)
  E(0xA9);
  E(0x09);
  // SEC A (vanilla)
  E(0x38);
  // SBC $E19C,y (vanilla)
  E(0xF9);
  E(0x9C);
  E(0xE1);
  // ASL twice to multiply by 4
  E(0x0A);
  E(0x0A);
  // STA $A6 (vanilla)
  E(0x85);
  E(0xA6);
  // RTL
  E(0x6B);
  return 1;
}

/* Faster boss AI - upstream processing/hacks/common/other/FastBosses.cs */
int sr_hack_fast_bosses(struct sr_ctx *c) {
  if (!SB("aggBosses")) return 0;
  // $C2/3445 A0 00 00    LDY #$0000              A:D080 X:0000 Y:0000 P:eNvmxdIzc
  // $C2/3448 B7 0C LDA[$0C], y[$C2:D080]   A: D080 X:0000 Y: 0000 P: envmxdIZc
  P(0x23445, 0x22);
  P(0x23446, (uint8_t)(c->working_offset));
  P(0x23447, (uint8_t)(c->working_offset >> 8));
  P(0x23448, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x23449, 0xEA);
  // (replaced code)
  // LDY #0000
  E(0xA0);
  E(0x00);
  E(0x00);
  // LDA [$0C],y
  E(0xB7);
  E(0x0C);
  // ora #4000 - enable the boss wacky speedup bit
  E(0x09);
  E(0x00);
  E(0x40);
  // rtl
  E(0x6B);
  // this flag goes in 7ee698 for 7ee6xx enemy - this can be changed in battle to enable this
  // $C2/0DB7 AD 56 00    LDA $0056  [$7E:0056]   A:0200 X:0800 Y:0200 P:envMxdizc ?
  // $C2/0DF6 D0 3E       BNE $3E    [$0E36]      A:0001 X:E600 Y:0200 P:envmxdizc
  // this one's for the slimes and mana beast - remove the BNE to process boss AI every frame instead
  // of only when the 5-step frame counter 7E0056 is 0
  P(0x20DF6, 0xEA);
  P(0x20DF7, 0xEA);
  return 1;
}

/* Levelups restore MP - upstream processing/hacks/common/qol/LevelupMpRestore.cs */
int sr_hack_levelup_mp_restore(struct sr_ctx *c) {
  if (!SB("mpAtLevel")) return 0;
  // hp restore at levelup
  // $C0/44D3 BD 84 E1    LDA $E184,x[$7E:E184]   A:0000 X:0000 Y:004D P:envmxdIzc
  // $C0/44D6 9D 82 E1    STA $E182,x[$7E:E182]   A:003B X:0000 Y:004D P:envmxdIzc
  // 86,87 are mp, and 8bit
  // 22 to thingy
  P(0x44D3, 0x22);
  P(0x44D4, (uint8_t)(c->working_offset));
  P(0x44D5, (uint8_t)(c->working_offset >> 8));
  P(0x44D6, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x44D7, 0xEA);
  P(0x44D8, 0xEA);
  // original - hp
  // LDA $E184,x
  E(0xBD);
  E(0x84);
  E(0xE1);
  // STA $E182,x
  E(0x9D);
  E(0x82);
  E(0xE1);
  // SEP 20
  E(0xE2);
  E(0x20);
  // new - mp
  // LDA $E187,x
  E(0xBD);
  E(0x87);
  E(0xE1);
  // STA $E186,x
  E(0x9D);
  E(0x86);
  E(0xE1);
  // REP 20
  E(0xC2);
  E(0x20);
  // RTL
  E(0x6B);
  return 1;
}

/* Obscure player HP - upstream processing/hacks/common/other/ObscureOwnHp.cs */
int sr_hack_obscure_own_hp(struct sr_ctx *c) {
  if (!SB("obscureHp")) return 0;
  // hide display on bottom of main screen
  // subr to display boy hp on screen
  // C1/E34F:   2069E3   JSR $E369
  P(0x1E34F, 0xEA);
  P(0x1E350, 0xEA);
  P(0x1E351, 0xEA);
  // subr to display girl hp on screen
  // C1/E35A:   2069E3   JSR $E369
  P(0x1E35A, 0xEA);
  P(0x1E35B, 0xEA);
  P(0x1E35C, 0xEA);
  // subr to display sprite hp on screen
  // C1/E365:   2069E3   JSR $E369
  P(0x1E365, 0xEA);
  P(0x1E366, 0xEA);
  P(0x1E367, 0xEA);
  // hide display in stats menu
  // current HP
  // $C7/69D3 B9 82 E1    LDA $E182,y[$7E:E382]   A:0200 X:0018 Y:0200 P:envmxdIzc
  // $C7/69D9 20 17 5D    JSR $5D17  [$C7:5D17]   A:003B X:0018 Y:003B P:envMxdIzc **
  // $C7/69DC 20 1E 5A    JSR $5A1E  [$C7:5A1E]   A:03B5 X:0018 Y:0000 P:envMxdIzC **
  P(0x769D9, 0xEA);
  P(0x769DA, 0xEA);
  P(0x769DB, 0xEA);
  P(0x769DC, 0xEA);
  P(0x769DD, 0xEA);
  P(0x769DE, 0xEA);
  // max HP
  // $C7/69E8 B9 84 E1    LDA $E184,y[$7E:E384]   A:03C1 X:001C Y:0200 P:envmxdIzC
  // $C7/69EE 20 17 5D    JSR $5D17  [$C7:5D17]   A:003B X:001C Y:003B P:envMxdIzC **
  // $C7/69F1 20 1E 5A    JSR $5A1E  [$C7:5A1E]   A:03B5 X:001C Y:0000 P:envMxdIzC **
  P(0x769EE, 0xEA);
  P(0x769EF, 0xEA);
  P(0x769F0, 0xEA);
  P(0x769F1, 0xEA);
  P(0x769F2, 0xEA);
  P(0x769F3, 0xEA);
  return 1;
}

/* Walk through walls - upstream processing/hacks/common/other/NoCollision.cs */
int sr_hack_no_collision(struct sr_ctx *c) {
  if (!SB("noclip")) return 0;
  /*
  $02/A052 BF 00 00 CB LDA $CB0000,x[$CB:0068] A:0068 X:0068 Y:0000 P:envmxdIzc
  $02/A056 91 14       STA ($14),y[$7F:B800]   A:0010 X:0068 Y:0000 P:envmxdIzc
  $02/A058 C8          INY                     A:0010 X:0068 Y:0000 P:envmxdIzc
  $02/A059 C8          INY                     A:0010 X:0068 Y:0001 P:envmxdIzc
  $02/A05A BF 02 00 CB LDA $CB0002,x[$CB:006A] A:0010 X:0068 Y:0002 P:envmxdIzc
  $02/A05E 91 14       STA ($14),y[$7F:B802]   A:0000 X:0068 Y:0002 P:envmxdIZc
  $02/A060 C8          INY                     A:0000 X:0068 Y:0002 P:envmxdIZc
  $02/A061 C8          INY                     A:0000 X:0068 Y:0003 P:envmxdIzc
  */
  // b1 0x01 = wall
  // b3 0x07 = corners?
  P(0x2A052, 0x22);
  P(0x2A053, (uint8_t)c->working_offset);
  P(0x2A054, (uint8_t)(c->working_offset >> 8));
  P(0x2A055, (uint8_t)((c->working_offset >> 16) + 0xC0));
  // (removed code - load collision)
  E(0xBF);
  E(0x00);
  E(0x00);
  E(0xCB);
  // AND #FFFE - strip off bits for collision
  E(0x29);
  E(0xFE);
  E(0xFF);
  // RTL
  E(0x6B);
  P(0x2A05A, 0x22);
  P(0x2A05B, (uint8_t)c->working_offset);
  P(0x2A05C, (uint8_t)(c->working_offset >> 8));
  P(0x2A05D, (uint8_t)((c->working_offset >> 16) + 0xC0));
  // (removed code - load collision)
  E(0xBF);
  E(0x02);
  E(0x00);
  E(0xCB);
  // AND #FF00 - strip off bits for collision
  E(0x29);
  E(0x00);
  E(0xFF);
  // RTL
  E(0x6B);
  return 1;
}

/* Players are always poisoned - upstream processing/hacks/common/other/AlwaysPoisoned.cs */
int sr_hack_always_poisoned(struct sr_ctx *c) {
  if (!SB("deathByPoison")) return 0;
  /*
  $C0/3B21 22 01 E3 C8 JSL $C8E301[$C8:E301]   A:0000 X:0600 Y:0200 P:envMxdIZc
  */
  P(0x3B21, 0x22);
  P(0x3B22, (uint8_t)c->working_offset);
  P(0x3B23, (uint8_t)(c->working_offset >> 8));
  P(0x3B24, (uint8_t)((c->working_offset >> 16) + 0xC0));
  // here: load 7ecf00, if zero, skip
  // LDA 7ecf00
  E(0xAF);
  E(0x00);
  E(0xCF);
  E(0x7E);
  // BEQ skip (8+5)
  E(0xF0);
  E(0x08 + 0x05);
  // CPX #600
  E(0xE0);
  E(0x00);
  E(0x06);
  // BGE skip
  E(0xB0);
  E(0x08);
  // LDA #20 - poison bit
  E(0xA9);
  E(0x20);
  // ORA E191,x - or with existing status
  E(0x1D);
  E(0x91);
  E(0xE1);
  // STA E191,x - write back with poison
  E(0x9D);
  E(0x91);
  E(0xE1);
  // skip:
  // removed code to process statuses (see also StatusGlow)
  E(0x22);
  E(0x01);
  E(0xE3);
  E(0xC8);
  // RTL
  E(0x6B);
  return 1;
}

/* Allow cup of wishes on any 0 HP characters - upstream processing/hacks/common/qol/CupWishesOnZeroHp.cs */
int sr_hack_cup_wishes_on_zero_hp(struct sr_ctx *c) {
  if (!SB("cupAtZeroHp")) return 0;
  /*
  * replace:
  $D0/DB2A BF 91 01 7E LDA $7E0191,x[$7E:E391] A:0005 X:E200 Y:1814 P:envMxdIZC ** (change to new subr)
  $D0/DB2E 10 1D       BPL $1D    [$DB4D]      A:0080 X:E200 Y:1814 P:eNvMxdIzC ** (change to BCS/BCC on whatever we determine in ^); note 8bit A here
  */
  sr_ensure_space_in_bank(&c->working_offset, 30);  // idk a bunch
  P(0x10db2a, 0x22);
  P(0x10db2b, (uint8_t)(c->working_offset));
  P(0x10db2c, (uint8_t)(c->working_offset >> 8));
  P(0x10db2d, (uint8_t)((c->working_offset >> 16) + 0xC0));
  // BCS for error case (can't revive) to skip
  P(0x10db2e, 0xB0);
  // removed code; check dead status
  E(0xBF);
  E(0x91);
  E(0x01);
  E(0x7E);
  // BPL over (2)
  E(0x10);
  E(0x02);
  // CLC - can revive since dead
  E(0x18);
  // RTL - blorf
  E(0x6B);
  // REP 20
  E(0xC2);
  E(0x20);
  // LDA 7e0182,x - additionally check hp
  E(0xBF);
  E(0x82);
  E(0x01);
  E(0x7E);
  // BNE over (4)
  E(0xD0);
  E(0x04);
  // SEP 20
  E(0xE2);
  E(0x20);
  // CLC - can revive - not dead yet, but at 0 hp
  E(0x18);
  // RTL - blerf
  E(0x6B);
  // default - can't use
  // SEP 20
  E(0xE2);
  E(0x20);
  // SEC - indicate inability to use
  E(0x38);
  // RTL
  E(0x6B);
  return 1;
}

/* Enemy type/species damage - upstream processing/hacks/common/enhancement/EnemyTypeDamage.cs */
int sr_hack_enemy_type_damage(struct sr_ctx *c) {
  if (!SB("speciesDamage")) return 0;
  // read atk
  // $C0/51F7 B9 98 E1    LDA $E198,y[$7E:E198]   A:0000 X:0A00 Y:0000 P:envMxdIZC
  // $C0/51FA 85 D4       STA $D4    [$00:03D4]   A:0012 X:0A00 Y:0000 P:envMxdIzC
  P(0x51F7, 0x22);
  P(0x51F8, (uint8_t)(c->working_offset));
  P(0x51F9, (uint8_t)(c->working_offset >> 8));
  P(0x51FA, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x51FB, 0xEA);
  // [194] is atk type
  // x is target, y is source
  // where is target species type? [192]?
  // replace above with:
  // -- B9 98 E1    LDA $E198,y[$7E:E198]
  // -- PHA
  // B9 94 E1    LDA $E194,y[$7E:E194]
  E(0xB9);
  E(0x94);
  E(0xE1);
  // 3D 92 E1    AND $E192,x[$7E:EA92]
  E(0x3D);
  E(0x92);
  E(0xE1);
  // F0 12       BEQ over
  E(0xF0);
  E(0x12);
  // B9 98 E1    LDA $E198,y[$7E:E198]
  E(0xB9);
  E(0x98);
  E(0xE1);
  // 4A          LSR
  E(0x4A);
  // 18          CLC
  E(0x18);
  // 79 98 E1    ADC $E198,y[$7E:E198]
  E(0x79);
  E(0x98);
  E(0xE1);
  // D9 98 E1    CMP $E198,y[$7E:E198]
  E(0xD9);
  E(0x98);
  E(0xE1);
  // B0 02       BGE skipSet
  E(0xB0);
  E(0x02);
  // A9 FF       LDA #FF
  E(0xA9);
  E(0xFF);
  // 85 D4       STA $D4    [$00:03D4]
  E(0x85);
  E(0xD4);
  // 6B          RTL
  E(0x6B);
  // over:
  // B9 98 E1    LDA $E198,y[$7E:E198]
  E(0xB9);
  E(0x98);
  E(0xE1);
  // 85 D4       STA $D4    [$00:03D4]
  E(0x85);
  E(0xD4);
  // 6B          RTL
  E(0x6B);
  return 1;
}

/* Obscure damage numbers - upstream processing/hacks/common/other/ObscureDamage.cs */
int sr_hack_obscure_damage(struct sr_ctx *c) {
  if (!SB("obscureDamage")) return 0;
  // skip this entire sequence that generates the sprite numbers and replaces with zero, but maintain
  // the logic that gives damage the largest font
  /*
  C1/843C:    B002        BCS $8440     -> BRA to branch always
  C1/843E:    8053        BRA $8493     ---
  C1/8440:    C9C800      CMP #$00C8
  C1/8443:    B008        BCS $844D     -> BRA to branch always
  C1/8445:    A508        LDA $08       ---
  C1/8447:    18          CLC           ---
  C1/8448:    698002      ADC #$0280    ---
  C1/844B:    8508        STA $08       ---
  C1/844D:    A504        LDA $04
  C1/844F:    3013        BMI $8464     -> BRA to branch always
  C1/8451:    0A          ASL A         ---
  C1/8452:    0A          ASL A         ---
  C1/8453:    0A          ASL A         ---
  C1/8454:    0A          ASL A         ---
  C1/8455:    0A          ASL A         ---
  C1/8456:    0A          ASL A         ---
  C1/8457:    18          CLC           ---
  C1/8458:    6508        ADC $08       ---
  C1/845A:    990000      STA $0000,Y   ---
  C1/845D:    18          CLC           ---
  C1/845E:    692000      ADC #$0020    ---
  C1/8461:    990400      STA $0004,Y   ---
  C1/8464:    A502        LDA $02
  C1/8466:    3013        BMI $847B     -> BRA to branch always
  C1/8468:    0A          ASL A         ---
  C1/8469:    0A          ASL A         ---
  C1/846A:    0A          ASL A         ---
  C1/846B:    0A          ASL A         ---
  C1/846C:    0A          ASL A         ---
  C1/846D:    0A          ASL A         ---
  C1/846E:    18          CLC           ---
  C1/846F:    6508        ADC $08       ---
  C1/8471:    990800      STA $0008,Y   ---
  C1/8474:    18          CLC           ---
  C1/8475:    692000      ADC #$0020    ---
  C1/8478:    990C00      STA $000C,Y   ---
  C1/847B:    A500        LDA $00
  C1/847D:    3013        BMI $8492    - remove
  C1/847F:    0A          ASL A
  C1/8480:    0A          ASL A
  C1/8481:    0A          ASL A
  C1/8482:    0A          ASL A        -
  C1/8483:    0A          ASL A        -
  C1/8484:    0A          ASL A        - replace with LDA #0000
  C1/8485:    18          CLC
  */
  P(0x1843C, 0x80);
  P(0x18443, 0x80);
  P(0x18444, 0x08);
  P(0x1844F, 0x80);
  P(0x18466, 0x80);
  P(0x1847D, 0xEA);
  P(0x1847E, 0xEA);
  // LDA #0000
  P(0x18482, 0xA9);
  P(0x18483, 0x00);
  P(0x18484, 0x00);
  return 1;
}

/* Mana magic fixed charge level - upstream processing/hacks/openworld/ManaMagicFix.cs */
int sr_hack_mana_magic_fix(struct sr_ctx *c) {
  if (!SB("manaMagicFix")) return 0;
  // allow all the bullshit above to run, but then do nothing with it and just load 6 at the end.
  // LDA #06 at 4994
  P(0x4994, 0xA9);
  P(0x4995, 0x06);
  return 1;
}

/* Enemies have infinite MP - upstream processing/hacks/common/other/EnemyInfiniteMp.cs */
int sr_hack_enemy_infinite_mp(struct sr_ctx *c) {
  if (!SB("enemyInfiniteMp")) return 0;
  sr_ensure_space_in_bank(&c->working_offset, 40);  // idk a bunch
  // spell casting - for enemies (x >= 0x600), override to succeed regardless of current mp
  /*
  $C0/3F80 BD 86 E1    LDA $E186,x[$7E:E786]   A:0004 X:0600 Y:0600 P:envMxdIzc * [replace]
  $C0/3F83 38          SEC                     A:0007 X:0600 Y:0600 P:envMxdIzc * [replace]
  $C0/3F84 E5 A4       SBC $A4    [$00:03A4]   A:0007 X:0600 Y:0600 P:envMxdIzC * [replace]
  $C0/3F86 10 04       BPL $04    [$3F8C]      A:0003 X:0600 Y:0600 P:envMxdIzC - branch if have enough
  $C0/3F88 A9 FF       LDA #$FF                A:00FC X:0600 Y:0600 P:eNvMxdIzc - don't have enough
  $C0/3F8A 80 74       BRA $74    [$4000]      A:00FF X:0600 Y:0600 P:eNvMxdIzc
  $C0/3F8C 22 FD 5A C0 JSL $C05AFD[$C0:5AFD]   A:0003 X:0600 Y:0600 P:envMxdIzC
  */
  P(0x3f80, 0x22);
  P(0x3f81, (uint8_t)(c->working_offset));
  P(0x3f82, (uint8_t)(c->working_offset >> 8));
  P(0x3f83, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x3f84, 0xEA);
  P(0x3f85, 0xEA);
  // CPX #0600
  E(0xE0);
  E(0x00);
  E(0x06);
  // BGE processEnemy (7)
  E(0xB0);
  E(0x07);
  // (removed code)
  // LDA $E186,x
  E(0xBD);
  E(0x86);
  E(0xE1);
  // SEC
  E(0x38);
  // SBC $A4
  E(0xE5);
  E(0xA4);
  // RTL
  E(0x6B);
  // processEnemy:
  // LDA #01 - for the BPL; always pass and allow casts
  E(0xA9);
  E(0x01);
  // RTL
  E(0x6B);
  // mp steal - change to ignore enemy target's mp remaining
  // C8/E833:	3BEC		[19: MP Absorb]
  /*
  before this block: calculate damage for MP absorb via some math ending in a divide, and in this spot vanilla loads the divide result,
  then compares to the target's current MP, to max at that amount.  here, we remove that check and just take the fully calculated amount.
  ...
  C8/EC6B:	AF144200	LDA $004214 - divide result
  C8/EC6F:	D002    	BNE $EC73
  C8/EC71:	A901    	LDA #$01    - minimum of 1 damage
  C8/EC73:	DD86E1  	CMP $E186,X - compare to current mp                   *
  C8/EC76:	9003    	BCC $EC7B   - jump over if there's enough             *   - change to BRA to branch always? we only want to do this is x >= 600 though
  C8/EC78:	BD86E1  	LDA $E186,X - otherwise load current mp (steal all)   *
  C8/EC7B:	85A4    	STA $A4     - stick in a4, damage amount              *
  C8/EC7D:	BDF5E1  	LDA $E1F5,X - mp damage on target
  C8/EC80:	18      	CLC
  C8/EC81:	65A4    	ADC $A4     - add the steal amount
  C8/EC83:	9DF5E1  	STA $E1F5,X - write back
  C8/EC86:	B9F6E1  	LDA $E1F6,Y - mp heal on caster
  C8/EC89:	18      	CLC
  C8/EC8A:	65A4    	ADC $A4
  C8/EC8C:	99F6E1  	STA $E1F6,Y - mp heal on caster
  C8/EC8F:	60      	RTS
  */
  P(0x8ec73, 0x22);
  P(0x8ec74, (uint8_t)(c->working_offset));
  P(0x8ec75, (uint8_t)(c->working_offset >> 8));
  P(0x8ec76, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x8ec77, 0xEA);
  P(0x8ec78, 0xEA);
  P(0x8ec79, 0xEA);
  P(0x8ec7A, 0xEA);
  P(0x8ec7B, 0xEA);
  P(0x8ec7C, 0xEA);
  // check for enemy target
  // CPX #0600
  E(0xE0);
  E(0x00);
  E(0x06);
  // BGE infiniteMpLogic (0x0B)
  E(0xB0);
  E(0x0B);  // lol boob
  // player target: vanilla logic + rtl
  // CMP $E186,x - compare to current mp
  E(0xDD);
  E(0x86);
  E(0xE1);
  // BCC 03 - branch if enough mp
  E(0x90);
  E(0x03);
  // LDA $E186,x - otherwise load all of current MP
  E(0xBD);
  E(0x86);
  E(0xE1);
  // STA $A4
  E(0x85);
  E(0xA4);
  // RTL
  E(0x6B);
  // infiniteMpLogic:
  // enemy target: no logic; take the calculated amount always
  // STA $84
  E(0x85);
  E(0xA4);
  // RTL
  E(0x6B);
  return 1;
}

/* 12-letter name entry - upstream processing/hacks/common/enhancement/NameEntryChanges.cs */
int sr_hack_name_entry_changes(struct sr_ctx *c) {
  if (!SB("nameEntryFix")) return 0;
  /*
  * this is the counter for input repeats on left/right, it counts up to 04 from like F0ish
  $C0/1E08 AD 01 A2    LDA $A201  [$7E:A201]   A:0003 X:0220 Y:0004 P:eNvMxdIzc **
  $C0/1E0B 1A          INC A                   A:00F2 X:0220 Y:0004 P:eNvMxdIzc **
  $C0/1E0C 8D 01 A2    STA $A201  [$7E:A201]   A:00F3 X:0220 Y:0004 P:eNvMxdIzc **
  $C0/1E0F C9 04       CMP #$04                A:00F3 X:0220 Y:0004 P:eNvMxdIzc
  $C0/1E11 D0 09       BNE $09    [$1E1C]      A:00F3 X:0220 Y:0004 P:eNvMxdIzC
  but doing LDA #04 here instead makes it zoom super fast
  // replace with:
  // JSL $CFFE00[$CF:FE00]  for inputs?
  // LDA $42
  // BNE over
  // LDA $44
  // BNE over
  // LDA #04
  // RTL
  // over:
  // (same code)
  */
  // fix the weird delay when you tap right/left by resetting the counter when you've let off the controls
  P(0x1E08, 0x22);
  P(0x1E09, (uint8_t)c->working_offset);
  P(0x1E0A, (uint8_t)(c->working_offset >> 8));
  P(0x1E0B, (uint8_t)((c->working_offset >> 16) + 0xC0));
  // JSL $CFFE00
  E(0x22);
  E(0x00);
  E(0xFE);
  E(0xCF);
  // REP 20
  E(0xC2);
  E(0x20);
  // LDA $42
  E(0xA5);
  E(0x42);
  // BNE over
  E(0xD0);
  E(0x0A);
  // LDA $44
  E(0xA5);
  E(0x44);
  // BNE over
  E(0xD0);
  E(0x06);
  // LDA #0004
  E(0xA9);
  E(0x04);
  E(0x00);
  // SEP 20
  E(0xE2);
  E(0x20);
  // RTL
  E(0x6B);
  // over:
  // (removed code)
  // SEP 20
  E(0xE2);
  E(0x20);
  // LDA $A201
  E(0xAD);
  E(0x01);
  E(0xA2);
  // INC A
  E(0x1A);
  // RTL
  E(0x6B);
  // initial pos
  // $C7/5029 A9 12       LDA #$12                A:0000 X:0220 Y:3021 P:envMxdIZC - 06 instead to start far enough back for 12 characters
  P(0x7502A, 0x06);
  // $C7/502B 8D 57 A1    STA $A157  [$7E:A157]   A:0012 X:0220 Y:3021 P:envMxdIzC
  // $C0/2496 B9 00 00    LDA $0000,y[$C7:75A8]   A:0000 X:0090 Y:75A8 P:envmxdIzc - the 07 here is the width - change to D
  P(0x775A9, 0x0D);
  // $C0/248F BE 00 00    LDX $0000,y[$C7:75A6]   A:0000 X:0000 Y:75A6 P:envMxdIZc - 0x90 here is the ram position - move back to x84
  P(0x775A6, 0x84);
  // $C0/255C 9D 00 90    STA $9000,x[$7E:9090]  writing the name border
  /*
  $C0/3198 AD CC A1    LDA $A1CC  [$7E:A1CC]   A:0000 X:0000 Y:0004 P:envMxdIZc
  $C0/319B C9 06       CMP #$06                A:0006 X:0000 Y:0004 P:envMxdIzc
  */
  // entry length check - change to C
  P(0x319C, 0x0C);
  // $C7/50A5 A2 D2 00    LDX #$00D2              A:0000 X:000C Y:0000 P:envmxdIZc - change to C6 to look earlier in RAM for letters
  P(0x750A6, 0xC6);
  // 35ea, text saying you can enter 6 letters
  P(0x35EA, 0xB6);
  P(0x35EB, 0xB7);
  P(0x35EC, 0x80);
  P(0x35ED, 0xA6);
  P(0x35EE, 0x9F);
  P(0x35EF, 0xAE);
  P(0x35F0, 0xAE);
  P(0x35F1, 0x9F);
  P(0x35F2, 0xAC);
  P(0x35F3, 0xAD);
  P(0x35F4, 0x80);
  P(0x35F5, 0xA6);
  P(0x35F6, 0xA9);
  P(0x35F7, 0xA8);
  P(0x35F8, 0xA1);
  return 1;
}

/* Overcharge fix - upstream processing/hacks/common/fix/OverchargeFix.cs */
int sr_hack_overcharge_fix(struct sr_ctx *c) {
  if (!SB("ovrchrgFix")) return 0;
  // replace the following that vanilla uses in the AI menu to set the requested max charge level:
  // $C7/6CD0 AD 11 A2    LDA $A211  [$7E:A211]   A:000F X:000F Y:0001 P:envMxdIzC
  // $C7/6CD3 99 7D CC STA $CC7D,y[$7E:CC7E] A:0001 X:000F Y:0001 P:envMxdIzC
  P(0x76CD0, 0xEA);
  P(0x76CD1, 0xEA);
  P(0x76CD2, 0x22);
  P(0x76CD3, (uint8_t)(c->working_offset));
  P(0x76CD4, (uint8_t)(c->working_offset >> 8));
  P(0x76CD5, (uint8_t)((c->working_offset >> 16) + 0xC0));
  // orig code - LDA $A211 - load AI charge max setting
  E(0xAD);
  E(0x11);
  E(0xA2);
  // orig code - STA $CC7D, y - write AI charge max setting
  E(0x99);
  E(0x7D);
  E(0xCC);
  // REP 20
  E(0xC2);
  E(0x20);
  // PHA
  E(0x48);
  // TYA
  E(0x98);
  // XBA
  E(0xEB);
  // ASL
  E(0x0A);
  // TAX -- 000, 200, 400 in x
  E(0xAA);
  // SEP 20
  E(0xE2);
  E(0x20);
  // LDA #00
  E(0xA9);
  E(0x00);
  // STA 7EE01A, X - zero out the charge level of the character (percent)
  E(0x9F);
  E(0x1A);
  E(0xE0);
  E(0x7E);
  // STA 7EE01B, X - zero out the charge level of the character (full level)
  E(0x9F);
  E(0x1B);
  E(0xE0);
  E(0x7E);
  // REP 20
  E(0xC2);
  E(0x20);
  // PLA
  E(0x68);
  // SEP 20
  E(0xE2);
  E(0x20);
  // RTL
  E(0x6B);
  // replace code used to set weapon value for character, to fix overcharge when swapping character weapons
  // $00 / EA67 99 68 E0    STA $E068,y[$7E:E068]   A:0206 X:0288 Y:0000 P:envMxdIzC
  // $00 / EA6A EB XBA
  P(0xEA67, 0x22);
  P(0xEA68, (uint8_t)(c->working_offset));
  P(0xEA69, (uint8_t)(c->working_offset >> 8));
  P(0xEA6A, (uint8_t)((c->working_offset >> 16) + 0xC0));
  // PHX
  E(0xDA);
  // TYX
  E(0xBB);
  // when setting player weapon, if the weapon id is changing, reset charge levels.
  // i believe this check was needed because there are occasions when this code runs
  // for existing weapons (dispel? weapon effect changes?), and we don't want to reset
  // the charge level for those
  // CMP E068,y - compare to existing weapon
  E(0xD9);
  E(0x68);
  E(0xE0);
  // BNE over
  E(0xD0);
  E(0x06);
  // here: run vanilla code and return, since weapon did not change
  // PLX
  E(0xFA);
  // STA $E068,y replaced code
  E(0x99);
  E(0x68);
  E(0xE0);
  // XBA replaced code
  E(0xEB);
  // RTL
  E(0x6B);
  // over:
  // STZ E01A,x - zero out weapon charge level (percent)
  E(0x9E);
  E(0x1A);
  E(0xE0);
  // STZ E01B,x - zero out weapon charge level (full charge)
  E(0x9E);
  E(0x1B);
  E(0xE0);
  // PLX
  E(0xFA);
  // replaced code - set weapon type number, then xba
  // STA $E068,y
  E(0x99);
  E(0x68);
  E(0xE0);
  // XBA
  E(0xEB);
  // RTL
  E(0x6B);
  return 1;
}
