/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Small hacks ported by hand because they use a list, a loop over a table or a second option (the straight-line ones are in
 * sr_hacks_simple.c).  Author of every upstream hack: Moppleton.  Translated to C 2026-10-18.  Upstream files:
 *   NoFootstepSound      processing/hacks/common/other/NoFootstepSound.cs
 *   SummonDespawnFix     processing/hacks/openworld/SummonDespawnFix.cs
 *   ConfuseFix           processing/hacks/common/fix/ConfuseFix.cs
 *   PercentagePoison     processing/hacks/common/other/PercentagePoison.cs
 *   MagicRopeDeathFix    processing/hacks/common/fix/MagicRopeDeathFix.cs
 *   BossDeathFix         processing/hacks/common/fix/BossDeathFix.cs
 */
#include <string.h>

#include "sr_core.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))
#define SB(key) sr_settings_get_bool(c->settings, (key))

/* JSL target patch: P(at) = 0x22 and the 24-bit address of the next free byte */
static void patch_jsl(struct sr_ctx *c, int at) {
  P(at, 0x22);
  P(at + 1, (uint8_t)c->working_offset);
  P(at + 2, (uint8_t)(c->working_offset >> 8));
  P(at + 3, (uint8_t)((c->working_offset >> 16) + 0xC0));
}

/* Sound effects reduction */
int sr_hack_no_footstep_sound(struct sr_ctx *c) {
  if (!SB("seReduction")) return 0;
  patch_jsl(c, 0x1D4FD);
  patch_jsl(c, 0x4172);
  patch_jsl(c, 0x230F8);
  E(0x08);                         /* PHP */
  E(0xC2); E(0x20);                /* REP #20 */
  E(0xAF); E(0x01); E(0x1E); E(0x7E);   /* LDA $7E1E01 */
  static const int filters[3] = {0x0F8B, 0x004F, 0x0F50};
  for (int i = 0; i < 3; i++) {
    E(0xC9); E((uint8_t)filters[i]); E((uint8_t)(filters[i] >> 8));   /* CMP #filter */
    int items_left = 3 - i - 1;
    E(0xF0); E((uint8_t)(0x04 + items_left * 5));                      /* BEQ skip */
  }
  E(0x22); E(0x04); E(0x00); E(0xC3);   /* JSL $C30004 */
  E(0x28);                         /* PLP */
  E(0x6B);                         /* RTL */
  return 1;
}

/* Fix summoning enemies despawning certain NPCs */
int sr_hack_summon_despawn_fix(struct sr_ctx *c) {
  if (!SB("noSummonDespawn")) return 0;
  patch_jsl(c, 0x1DA54);
  E(0x22); E(0x60); E(0xA3); E(0x02);   /* original call to check x/y to see if this object can be erased */
  E(0xB0); E(0x01);                     /* BCS over */
  E(0x6B);                              /* RTL */
  E(0xE2); E(0x20);                     /* SEP #20 */
  E(0xB9); E(0x80); E(0xE1);            /* LDA $E180,y */
  static const uint8_t no_despawn[4] = {0x99, 0xEB, 0x55, 0xD3};
  for (int i = 0; i < 4; i++) {
    E(0xC9); E(no_despawn[i]);          /* CMP #type */
    E(0xD0); E(0x04);                   /* BNE next */
    E(0xC2); E(0x20);                   /* REP #20 */
    E(0x18);                            /* CLC */
    E(0x6B);                            /* RTL */
  }
  E(0xC2); E(0x20);
  E(0x38);
  E(0x6B);

  /* the two neko npcs must always spawn: object 0 of map 62, object 5 of map 195 (read from the original ROM) */
  static const int maps[2] = {62, 195}, idx[2] = {0, 5};
  for (int k = 0; k < 2; k++) {
    int start = sr_vanilla_object_offset(c->orig, maps[k]) + 8;
    int end = sr_vanilla_object_offset(c->orig, maps[k] + 1);
    int at = start + idx[k] * 8;
    if (at + 8 > end) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range (map %d objects).", maps[k]);
    struct sr_map_object o;
    memcpy(o.d, c->orig + at, 8);
    sr_obj_set_spawn_priority(&o, 1);
    int dest = sr_vanilla_object_offset(c->out, maps[k]) + 8 + idx[k] * 8;
    for (int i = 0; i < 8; i++) P(dest + i, o.d[i]);
  }
  return 1;
}

/* Confusion status inversion fix */
int sr_hack_confuse_fix(struct sr_ctx *c) {
  if (!SB("confusionFix")) return 0;
  P(0x7573f, 0x22);
  P(0x75740, (uint8_t)c->working_offset);
  P(0x75741, (uint8_t)(c->working_offset >> 8));
  P(0x75742, (uint8_t)((c->working_offset >> 16) + 0xC0));
  E(0xE0); E(0x95); E(0x0E);      /* CPX #0E95 */
  E(0xD0); E(0x02);               /* BNE over */
  E(0xA9); E(0x00);               /* LDA #00 */
  E(0x9F); E(0x00); E(0x60); E(0x30);   /* STA $306000,x */
  E(0x6B);                        /* RTL */

  /* copy of the inversion subroutine at 9476..951b with an RTL at the end */
  int inversion_subr_address = c->working_offset;
  for (int i = 0x9476; i <= 0x951b; i++) {
    int v = sr_peek(c, i);
    if (i == 0x951b) v = 0x6b;
    E(v);
  }

  P(0x9440, 0x22);
  P(0x9441, (uint8_t)c->working_offset);
  P(0x9442, (uint8_t)(c->working_offset >> 8));
  P(0x9443, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x9444, 0xEA); P(0x9445, 0xEA); P(0x9446, 0xEA);
  E(0xAF); E(0xEC); E(0x00); E(0x7E);   /* LDA $7E00EC */
  E(0xF0); E(0x05);                     /* BEQ over */
  E(0xA5); E(0x43);                     /* LDA $43 */
  E(0xA4); E(0x42);                     /* LDY $42 */
  E(0x6B);                              /* RTL */
  E(0xA5); E(0x43);
  E(0xA4); E(0x42);
  E(0x22); E((uint8_t)inversion_subr_address); E((uint8_t)(inversion_subr_address >> 8)); E((uint8_t)((inversion_subr_address >> 16) + 0xC0));
  E(0x6B);
  return 1;
}

/* Poison deals percentage damage */
int sr_hack_percentage_poison(struct sr_ctx *c) {
  if (!SB("percentPoison")) return 0;
  const int poison_kills_you = SB("deathByPoison");
  patch_jsl(c, 0x3D6D);
  E(0xC2); E(0x20);               /* REP #20 */
  E(0x48);                        /* PHA */
  E(0xBD); E(0x84); E(0xE1);      /* LDA $E184,x - max hp */
  E(0x4A); E(0x4A); E(0x4A); E(0x4A); E(0x4A);   /* LSR x5 */
  E(0x48);                        /* PHA */
  E(0xBD); E(0x82); E(0xE1);      /* LDA $E182,x - current hp */
  E(0x38);                        /* SEC */
  E(0xE3); E(0x01);               /* SBC $01,s */
  E(0xB0); E(0x03);               /* BCS over */
  if (poison_kills_you) { E(0xA9); E(0x00); E(0x00); }   /* LDA #0000 */
  else { E(0xA9); E(0x01); E(0x00); }                    /* LDA #0001 */
  E(0x9D); E(0x82); E(0xE1);      /* STA $E182,x - write current HP back */
  E(0x68); E(0x68);               /* PLA, PLA - even out the stack */
  E(0xE2); E(0x20);               /* SEP #20 */
  E(0x09); E(0x40);               /* ORA #40 (replaced code) */
  E(0x85); E(0x80);               /* STA $80 (replaced code) */
  E(0x6B);                        /* RTL */

  /* determine whether poison should be turned off based on your current HP: SEC for yes, CLC for no */
  patch_jsl(c, 0x3D73);
  E(0xBD); E(0x82); E(0xE1);      /* LDA $E182,x - current hp */
  if (!poison_kills_you) {
    E(0xC9); E(0x01); E(0x00);    /* CMP #0001 */
    E(0xD0); E(0x02);             /* BNE over */
    E(0x38);                      /* SEC */
    E(0x6B);                      /* RTL */
  }
  E(0xC9); E(0x00); E(0x00);      /* CMP #0000 */
  E(0xD0); E(0x02);               /* BNE over */
  E(0x38);                        /* SEC */
  E(0x6B);                        /* RTL */
  E(0x18);                        /* CLC */
  E(0x6B);                        /* RTL */
  P(0x3D77, 0xB0);
  P(0x3D78, 0x0D);
  P(0x3D79, 0xEA);
  P(0x3D7A, 0xEA);
  return 1;
}

/* Magic rope animation/death fix for girl/sprite */
int sr_hack_magic_rope_death_fix(struct sr_ctx *c) {
  if (!SB("magicRopeDeathFix")) return 0;
  if (!strcmp(sr_settings_get(c->settings, "mode"), "ancientcave")) return 0;   /* conflicts with MagicRope hack for ancient cave */
  P(0x1DC48, 0x22);
  P(0x1DC49, (uint8_t)c->working_offset);
  P(0x1DC4A, (uint8_t)(c->working_offset >> 8));
  P(0x1DC4B, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x1DC4C, 0xEA);
  P(0x1DC4D, 0xEA);
  E(0xC9); E(0x06);               /* CMP #06 */
  E(0xF0); E(0x07);               /* BEQ xx */
  E(0xAD); E(0xF3); E(0x19);      /* LDA $19F3 (replaced code) */
  E(0x29); E(0x03);               /* AND #03 */
  E(0x0A);                        /* ASL */
  E(0x6B);                        /* RTL */
  E(0xAD); E(0x08); E(0x18);
  E(0x29); E(0x03);
  E(0x0A);
  E(0x6B);
  return 1;
}

/* Boss death softlock fix */
int sr_hack_boss_death_fix(struct sr_ctx *c) {
  if (!SB("bossDeathFix")) return 0;
  /* VanillaEventUtil.getVanillaEvent(origRom, 0x7f8) */
  int start = sr_event_start_offset(c, c->orig, 0x7f8);
  int next = sr_event_start_offset(c, c->orig, 0x7f8 + 1);
  struct sr_bytes *ev = sr_event_new(c, 0x7f8);
  for (int i = start; i < next; i++) sr_bytes_push(c, ev, sr_peek_orig(c, i));
  if (ev->len < 2) sr_fail(c, SOMRANDO_ERR_FAILED, "Index was out of range (event 0x7f8).");
  ev->p[ev->len - 2] = 0x00;
  return 1;
}
