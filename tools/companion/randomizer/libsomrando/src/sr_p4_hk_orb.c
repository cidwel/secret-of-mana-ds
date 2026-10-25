/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Derived from processing/hacks/openworld/CrystalOrbColors.cs (Author: Moppleton), the comments are those of the C#.  Translated to C
 * 2026-10-18.  Shows element orbs in the colour of their element and names them after it; adds the enemy level to enemy names when enabled.
 */
#include <stdlib.h>
#include <string.h>

#include "sr_p4.h"
#include "sr_p4_sel.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))

/* ElementSwaps.getCrystalOrbElementMap: the workingData entries "orbElement<mapNum>" in insertion order */
static void orb_element_map(struct sr_ctx *c, int *n, int **maps, int **vals) {
  int cnt = 0;
  for (int i = 0; i < c->n_work; i++)
    if (!strncmp(c->work[i].key, "orbElement", 10)) cnt++;
  *maps = (int *)sr_alloc(c, (size_t)(cnt + 1) * sizeof(int));
  *vals = (int *)sr_alloc(c, (size_t)(cnt + 1) * sizeof(int));
  *n = 0;
  for (int i = 0; i < c->n_work; i++) {
    if (strncmp(c->work[i].key, "orbElement", 10)) continue;
    char *end;
    const char *rest = c->work[i].key + 10;
    long mapNum = strtol(rest, &end, 10);
    if (!rest[0] || *end) sr_fail(c, SOMRANDO_ERR_FAILED, "Input string was not in a correct format.");
    int value = sr_p4_sel_work_get_int(c, c->work[i].key) & 0xFF;
    int at = -1;
    for (int k = 0; k < *n; k++)
      if ((*maps)[k] == (int)mapNum) at = k;
    if (at < 0) at = (*n)++;
    (*maps)[at] = (int)mapNum;
    (*vals)[at] = value;
  }
}

/* SomVanillaValues.elementOrbByteToName(element, true) */
static const char *orb_name(int element) {
  switch (element) {
    case 0x81: return "Gnome";
    case 0x82: return "Undine";
    case 0x83: return "Salamando";
    case 0x85: return "Sylphid";
    case 0x84: return "Lumina";
    case 0x86: return "Shade";
    case 0x87: return "Luna";
    case 0x88: return "Dryad";
  }
  return "";
}

int sr_p4_hack_crystal_orb_colors(struct sr_ctx *c) {
  int showEnemyLevels = sr_settings_get_bool(c->settings, "opShowLevels");
  const char *enemyLevelType = sr_p4_sel_work_str(c, SR_P4_ENEMY_LEVEL_TYPE);

  // pull the element values for each orb off the working data, as set by ElementSwaps
  int nmap = 0, *map_nums = NULL, *map_vals = NULL;
  orb_element_map(c, &nmap, &map_nums, &map_vals);
  int *subr_addrs = (int *)sr_alloc(c, (size_t)(nmap + 1) * sizeof(int));
  sr_ensure_space_in_bank(&c->working_offset, nmap * 40 + 10);
  int mapNumSubrCheckOffset = c->working_offset;
  // LDA $7E00DC - current map number
  E(0xAF);
  E(0xDC);
  E(0x00);
  E(0x7E);
  // load orb elements based on map number we expect them on
  for (int k = 0; k < nmap; k++)
  {
    int mapNum = map_nums[k];
    int elementValue = map_vals[k];
    // CMP #mapnum
    E(0xC9);
    E((uint8_t)mapNum);
    E((uint8_t)(mapNum >> 8));
    // BNE over
    E(0xD0);
    E(0x00);
    int mapNumBranchSrc = c->working_offset;
    // SEP 20 for 8bit A
    E(0xE2);
    E(0x20);
    // LDA #40
    E(0xA9);
    E(0x40);
    // STA $E01C, y - values set by analyzer to change the orb color
    E(0x99);
    E(0x1C);
    E(0xE0);
    // LDA #FF
    E(0xA9);
    E(0xFF);
    // STA $E030, y - values set by analyzer to change the orb color
    E(0x99);
    E(0x30);
    E(0xE0);
    // LDA #01
    E(0xA9);
    E(0x01);
    // STA $E042, y - values set by analyzer to change the orb color
    E(0x99);
    E(0x42);
    E(0xE0);
    // LDA #element
    E(0xA9);
    E(elementValue);
    // STA $E011, y - values set by analyzer to change the orb color
    E(0x99);
    E(0x11);
    E(0xE0);
    // REP 20 for 16bit A
    E(0xC2);
    E(0x20);
    // RTL
    E(0x6B);
    // set branch dist
    P(mapNumBranchSrc - 1, (uint8_t)(c->working_offset - mapNumBranchSrc));
  }
  // RTL at the end
  E(0x6B);
  sr_ensure_space_in_bank(&c->working_offset, 20);
  P(0x5625, 0x22);
  P(0x5626, (uint8_t)(c->working_offset));
  P(0x5627, (uint8_t)(c->working_offset >> 8));
  P(0x5628, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x5629, 0xEA);
  P(0x562A, 0xEA);
  P(0x562B, 0xEA);
  // CPX #09A1 - crystal orb is id 85 (decimal) and there's 29 (decimal) bytes per thingy
  E(0xE0);
  E(0xA1);
  E(0x09);
  // BNE over
  E(0xD0);
  E(0x00);
  int objTypeBranchSrc = c->working_offset;
  // call map check subr if we're loading a crystal orb
  E(0x22);
  E((uint8_t)(mapNumSubrCheckOffset));
  E((uint8_t)(mapNumSubrCheckOffset >> 8));
  E((uint8_t)((mapNumSubrCheckOffset >> 16) + 0xC0));
  // set jump distance (4)
  P(objTypeBranchSrc - 1, (uint8_t)(c->working_offset - objTypeBranchSrc));
  // over:
  // replaced code - load hp from rom
  // LDA $D01C01,x
  E(0xBF);
  E(0x01);
  E(0x1C);
  E(0xD0);
  // STA $E182,y
  E(0x99);
  E(0x82);
  E(0xE1);
  // RTL
  E(0x6B);
  sr_ensure_space_in_bank(&c->working_offset, 20);
  P(0x1DAEF, 0x22);
  P(0x1DAF0, (uint8_t)(c->working_offset));
  P(0x1DAF1, (uint8_t)(c->working_offset >> 8));
  P(0x1DAF2, (uint8_t)((c->working_offset >> 16) + 0xC0));
  P(0x1DAF3, 0xEA);
  P(0x1DAF4, 0xEA);
  // LDA $E1E7,x - species id
  E(0xBD);
  E(0xE7);
  E(0xE1);
  // CMP #55 - element orb
  E(0xC9);
  E(0x55);
  // BEQ over - skip replaced code
  E(0xF0);
  E(0x06);
  // (replaced code)
  // LDA $E142,x
  E(0xBD);
  E(0x42);
  E(0xE1);
  // STA $E011,x
  E(0x9D);
  E(0x11);
  E(0xE0);
  // RTL
  E(0x6B);


  // change names as they're loaded
  sr_names_set(c, 0x124, "Orb");
  /* copy of the 0x17C7..0x1803 text routine with its final RTS turned into RTL */
  int letterLen = 0x1803 - 0x17c7 + 1;
  sr_ensure_space_in_bank(&c->working_offset, letterLen);
  int writeEventLetterSubroutineAddr = c->working_offset;
  for (int i = 0x17c7; i <= 0x1803; i++) E(i == 0x1803 ? 0x6B : sr_peek(c, i));

  // generate subroutine for each orb to load its associated element name, using the character loader above
  for (int k = 0; k < nmap; k++)
  {
    int eleValue = map_vals[k];
    struct sr_bytes nameString = {0};
    char nm[16];
    snprintf(nm, sizeof nm, "%s ", orb_name(eleValue));
    sr_event_text_bytes(c, &nameString, nm);
    sr_ensure_space_in_bank(&c->working_offset, (int)nameString.len * 6 + 10);
    subr_addrs[k] = c->working_offset;
    // SEP 20
    E(0xE2);
    E(0x20);
    // write name characters
    for (size_t bi = 0; bi < nameString.len; bi++)
    {
      // LDA #letter
      E(0xA9);
      E(nameString.p[bi]);
      // JSL writeEventLetterSubroutineAddr
      E(0x22);
      E((uint8_t)(writeEventLetterSubroutineAddr));
      E((uint8_t)(writeEventLetterSubroutineAddr >> 8));
      E((uint8_t)((writeEventLetterSubroutineAddr >> 16) + 0xC0));
    }
    // REP 20
    E(0xC2);
    E(0x20);
    // RTL
    E(0x6B);
  }


  sr_ensure_space_in_bank(&c->working_offset, 100);
  int showEnemyLevelSubroutine = c->working_offset;
  if (showEnemyLevels)
  {
    // CPY #19d3
    E(0xC0);
    E(0xD3);
    E(0x19);
    // BEQ over [01]
    E(0xF0);
    E(0x01);
    // RTL
    E(0x6B);
    // CPX #99e (rabite)
    E(0xE0);
    E(0x9E);
    E(0x09);
    // BGE over [01]
    E(0xB0);
    E(0x01);
    // RTL
    E(0x6B);
    // CPX #a9d (mana beast + 1)
    E(0xE0);
    E(0x9D);
    E(0x0A);
    // BLT over [01]
    E(0x90);
    E(0x01);
    // RTL
    E(0x6B);
    // 7ea173 is current text position, 7ea1a4 is the array
    // PHA
    E(0x48);
    // SEP 20
    E(0xE2);
    E(0x20);
    // LDA #07
    E(0xA9);
    E(0x07);
    // STA 7ea173 - position
    E(0x8F);
    E(0x73);
    E(0xA1);
    E(0x7E);
    // STA 7ea1ce - length
    E(0x8F);
    E(0xCE);
    E(0xA1);
    E(0x7E);
    // LDA #A6 "L"
    E(0xA9);
    E(0xA6);
    // STA 7ea1a4
    E(0x8F);
    E(0xA4);
    E(0xA1);
    E(0x7E);
    // LDA #96 "v"
    E(0xA9);
    E(0x96);
    // STA 7ea1a5
    E(0x8F);
    E(0xA5);
    E(0xA1);
    E(0x7E);
    // LDA #BF "."
    E(0xA9);
    E(0xBF);
    // STA 7ea1a6
    E(0x8F);
    E(0xA6);
    E(0xA1);
    E(0x7E);
    // LDA #80 " "
    E(0xA9);
    E(0x80);
    // STA 7ea1a7
    E(0x8F);
    E(0xA7);
    E(0xA1);
    E(0x7E);
    // 7e1822 - 16 bit index into 7e1800 for the x value to use for the target data (E600, E800, etc)
    // PHX
    E(0xDA);
    // REP 20
    E(0xC2);
    E(0x20);
    // LDA 7e1822
    E(0xAF);
    E(0x22);
    E(0x18);
    E(0x7E);
    // TAX
    E(0xAA);
    // LDA 7e0000,x
    E(0xBF);
    E(0x00);
    E(0x00);
    E(0x7E);
    // TAX
    E(0xAA);
    // SEP 20
    E(0xE2);
    E(0x20);
    // LDA 7E0181,x - these x values are E600, etc - alternate species byte
    E(0xBF);
    E(0x81);
    E(0x01);
    E(0x7E);
    // PLX
    E(0xFA);
    // INC A - don't do this anymore now that we're pulling from object space
    if (strcmp(enemyLevelType, "none"))
    {
      // account for memory levels being off by one from what we want to display
      E(0x1A);
    }
    // divide by 10
    // REP 20
    E(0xC2);
    E(0x20);
    // AND #00FF
    E(0x29);
    E(0xFF);
    E(0x00);
    // STA 004204
    E(0x8F);
    E(0x04);
    E(0x42);
    E(0x00);
    // SEP 20
    E(0xE2);
    E(0x20);
    // LDA #0A
    E(0xA9);
    E(0x0A);
    // STA 004206
    E(0x8F);
    E(0x06);
    E(0x42);
    E(0x00);
    // eight nops to wait
    for (int i = 0; i < 8; i++)
    {
      E(0xEA);
    }
    // LDA 004214 - divisor
    E(0xAF);
    E(0x14);
    E(0x42);
    E(0x00);
    // CLC
    E(0x18);
    // ADC #B5 - numeric font character value
    E(0x69);
    E(0xB5);
    // STA 7ea1a8
    E(0x8F);
    E(0xA8);
    E(0xA1);
    E(0x7E);
    // LDA 004216 - remainder
    E(0xAF);
    E(0x16);
    E(0x42);
    E(0x00);
    // CLC
    E(0x18);
    // ADC #B5
    E(0x69);
    E(0xB5);
    // STA 7ea1a9
    E(0x8F);
    E(0xA9);
    E(0xA1);
    E(0x7E);
    // LDA #80 " "
    E(0xA9);
    E(0x80);
    // STA 7ea1aa
    E(0x8F);
    E(0xAA);
    E(0xA1);
    E(0x7E);
    // REP 20
    E(0xC2);
    E(0x20);
    // PLA
    E(0x68);
    // RTL
    E(0x6B);
  }


  // this is name loader for any top of screen text, it seems
  sr_ensure_space_in_bank(&c->working_offset, nmap * 20 + 50);
  P(0x192A, 0x22);
  P(0x192B, (uint8_t)(c->working_offset));
  P(0x192C, (uint8_t)(c->working_offset >> 8));
  P(0x192D, (uint8_t)((c->working_offset >> 16) + 0xC0));
  // (removed code) STA $001D01
  E(0x8F);
  E(0x01);
  E(0x1D);
  E(0x00);
  // CPX #0A48 - compare to A48 for crystal orb, which is event 924 .. * 2 - 0x800 = A48
  E(0xE0);
  E(0x48);
  E(0x0A);
  if (showEnemyLevels)
  {
    // BEQ 05 - skip printing levels if it's a crystal orb
    E(0xF0);
    E(0x05);
    // JSL showEnemyLevelSubroutine - show enemy levels for other object types
    E(0x22);
    E((uint8_t)(showEnemyLevelSubroutine));
    E((uint8_t)(showEnemyLevelSubroutine >> 8));
    E((uint8_t)((showEnemyLevelSubroutine >> 16) + 0xC0));
    // RTL
    E(0x6B);
  }
  else
  {
    // BEQ 01 - for non-orbs, do nothing special
    E(0xF0);
    E(0x01);
    // RTL
    E(0x6B);
  }
  // PHA
  E(0x48);
  // PHX
  E(0xDA);
  // LDX #0000
  E(0xA2);
  E(0x00);
  E(0x00);
  // LDA 7E00DC - map number
  E(0xAF);
  E(0xDC);
  E(0x00);
  E(0x7E);
  // compare to each map, use generated subroutines from above to name it
  for (int k = 0; k < nmap; k++)
  {
    int mapNum = map_nums[k];
    // next:
    // CMP #mapnum
    E(0xC9);
    E((uint8_t)mapNum);
    E((uint8_t)(mapNum>>8));
    // BNE next
    E(0xD0);
    E(0x0C);
    // JSL subrAddr
    int subrAddr = subr_addrs[k];
    E(0x22);
    E((uint8_t)(subrAddr));
    E((uint8_t)(subrAddr >> 8));
    E((uint8_t)((subrAddr >> 16) + 0xC0));
    // TXA
    E(0x8A);
    // STA 7EA173 - start printing at the position we left off
    E(0x8F);
    E(0x73);
    E(0xA1);
    E(0x7E);
    // PLX
    E(0xFA);
    // PLA
    E(0x68);
    // RTL
    E(0x6B);
  }
  // next:
  // PLX
  E(0xFA);
  // PLA
  E(0x68);
  // RTL
  E(0x6B);
  return 1;
}
