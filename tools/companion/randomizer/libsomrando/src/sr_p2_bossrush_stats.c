/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Boss rush mode (group w8): EnemyChanges, DropChanges and ShopChanges.  Derived from (Author: Moppleton) the processing/hacks/bossrush
 * classes of the same names.
 * Translated to C 2026-10-18.
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "sr_core.h"
#include "sr_p2_bossrush_int.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))
#define false 0
#define true 1

enum { S_LEVEL, S_STR, S_AGI, S_INT, S_WIS, S_EVADE, S_MEVADE, S_HP, S_MP, S_DEF, S_MDEF, S_EXP, S_GOLD, S_WEAPON_LEV, S_MAGIC_LEV, S_WEAPON_DAMAGE, S_COUNT };

static const char *const stat_key[S_COUNT] = {"enemyLevel", "enemyStr", "enemyAgi", "enemyInt", "enemyWis", "enemyEvade", "enemyMevade", "enemyHp",
                                              "enemyMp", "enemyDef", "enemyMdef", "enemyExp", "enemyGold", "enemyWeaponLev", "enemyMagicLev", "enemyWeaponDamage"};

static void ec_write_code_for_ram_injection(struct sr_ctx *c, int romOffset, int ramOffset);
static void ec_write_code_for_ram_injection_mp(struct sr_ctx *c, int romOffset, int ramOffset, int tonpoleMp);

static int w8_in_list(const int *l, int n, int v) {
  for (int i = 0; i < n; i++)
    if (l[i] == v) return 1;
  return 0;
}

/* ---------------------------------------------------------------------------------------------------- EnemyChanges */
static void ec_write_stat_injection_subroutine(struct sr_ctx *c, int numMaps, const int *statOffsets, const uint8_t *statRamOffsets, int includeHpMp, int tonpoleMp, int floorTableLocation) {
            if (!includeHpMp)
            {
                // LDY $BD
                E(0xA4);
                E(0xBD);
            }

            // load map num
            // LDA $7E00DC
            E(0xAF);
            E(0xDC);
            E(0x00);
            E(0x7E);

            if (floorTableLocation < 0)
            {
                // AND #00FF
                E(0x29);
                E(0xFF);
                E(0x00);
            }

            // check for mana beast
            // CMP #00FD
            E(0xC9);
            E(0xFD);
            E(0x00);

            // not mana beast? skip
            // BNE skip
            E(0xD0);
            E(0x03);

            // special case mana beast
            // LDA #(numMaps - 1) * 2
            E(0xA9);
            E((uint8_t)((numMaps - 1) * 2));
            E(0x00);

            // skip:
            if (floorTableLocation < 0)
            {
                // shift right since two maps per floor
                // LSR
                E(0x4A);
            }
            else
            {
                // PHX
                E(0xDA);
                // look up floorTableLocation first
                // TAX
                E(0xAA);
                // LDA floorTableLocation,X
                E(0xBF);
                E((uint8_t)(floorTableLocation));
                E((uint8_t)(floorTableLocation >> 8));
                E((uint8_t)((floorTableLocation >> 16) + 0xC0));

                // AND #00FF
                E(0x29);
                E(0xFF);
                E(0x00);
                // PLX
                E(0xFA);
            }

            // CPX #09DB - check for boss
            E(0xE0);
            E(0xDB);
            E(0x09);

            // BCC skip if not boss
            E(0x90);
            E(0x04);

            // CLC
            E(0x18);

            // ADC #numMaps for boss
            E(0x69);
            E((uint8_t)numMaps);
            E((uint8_t)(numMaps >> 8));

            // skip:
            // PHX
            E(0xDA);

            // TAX
            E(0xAA);

            // 8bit A mode
            // SEP 20
            E(0xE2);
            E(0x20);

            // 8bit stats
            ec_write_code_for_ram_injection(c, statOffsets[S_LEVEL], statRamOffsets[S_LEVEL]);
            ec_write_code_for_ram_injection(c, statOffsets[S_STR], statRamOffsets[S_STR]);
            ec_write_code_for_ram_injection(c, statOffsets[S_AGI], statRamOffsets[S_AGI]);
            ec_write_code_for_ram_injection(c, statOffsets[S_INT], statRamOffsets[S_INT]);
            ec_write_code_for_ram_injection(c, statOffsets[S_WIS], statRamOffsets[S_WIS]);
            ec_write_code_for_ram_injection(c, statOffsets[S_EVADE], statRamOffsets[S_EVADE]);
            ec_write_code_for_ram_injection(c, statOffsets[S_MEVADE], statRamOffsets[S_MEVADE]);
            if (includeHpMp)
            {
                ec_write_code_for_ram_injection_mp(c, statOffsets[S_MP], statRamOffsets[S_MP], tonpoleMp);
                ec_write_code_for_ram_injection_mp(c, statOffsets[S_MP], (uint8_t)(statRamOffsets[S_MP] + 1), tonpoleMp);
            }
            ec_write_code_for_ram_injection(c, statOffsets[S_WEAPON_LEV], statRamOffsets[S_WEAPON_LEV]);
            ec_write_code_for_ram_injection(c, statOffsets[S_MAGIC_LEV], statRamOffsets[S_MAGIC_LEV]);

            // turn off 8bit A mode
            // REP 20
            E(0xC2);
            E(0x20);

            // now double x for the 16 bit stats

            // TXA
            E(0x8A);

            // ASL
            E(0x0A);

            // TAX
            E(0xAA);

            // 16bit stats
            ec_write_code_for_ram_injection(c, statOffsets[S_DEF], statRamOffsets[S_DEF]);
            ec_write_code_for_ram_injection(c, statOffsets[S_MDEF], statRamOffsets[S_MDEF]);
            ec_write_code_for_ram_injection(c, statOffsets[S_EXP], statRamOffsets[S_EXP]);
            if (includeHpMp)
            {
                ec_write_code_for_ram_injection(c, statOffsets[S_HP], statRamOffsets[S_HP]);
                ec_write_code_for_ram_injection(c, statOffsets[S_HP], (uint8_t)(statRamOffsets[S_HP] + 2));
            }
            ec_write_code_for_ram_injection(c, statOffsets[S_GOLD], statRamOffsets[S_GOLD]);

            // PLX
            E(0xFA);

            // RTL
            E(0x6B);
}

static void ec_write_code_for_ram_injection(struct sr_ctx *c, int romOffset, int ramOffset) {
            // LDA romOffset, x
            E(0xBF);
            E((uint8_t)romOffset);
            E((uint8_t)(romOffset >> 8));
            E((uint8_t)(romOffset >> 16));
            // STA $EBxx, y
            E(0x99);
            E(ramOffset);
            E(0xE1);
}

static void ec_write_code_for_ram_injection_mp(struct sr_ctx *c, int romOffset, int ramOffset, int tonpoleMp) {
            // LDA romOffset, x
            E(0xBF);
            E((uint8_t)romOffset);
            E((uint8_t)(romOffset >> 8));
            E((uint8_t)(romOffset >> 16));

            // need to restore x in here temporarily .. god what a clusterfuck to do this though
            // PHX - 2 bytes
            E(0xDA);
            // PHA - 1 byte
            E(0x48);
            // REP 20
            E(0xC2);
            E(0x20);
            // LDA 4,s - seek back 5 bytes to the X we pushed earlier
            E(0xA3);
            E(0x04);
            // TAX
            E(0xAA);
            // SEP 20
            E(0xE2);
            E(0x20);
            // PLA
            E(0x68);

            // see if we should overwrite A with tonpole mp
            static const uint8_t lowerMpEnemies[] = { 95, 113, 122 };
            for (size_t b_i = 0; b_i < sizeof lowerMpEnemies; b_i++) { const uint8_t b = lowerMpEnemies[b_i];
                int val = b * 0x1D;
                // cpx #enemyIndex
                E(0xE0);
                E((uint8_t)val);
                E((uint8_t)(val >> 8));

                // bne to skip / run next iter
                E(0xD0);
                E(0x02);

                // LDA #tonpoleMp
                E(0xA9);
                E((uint8_t)tonpoleMp);
            }

            // 99 xx EB    STA $EBxx, y
            E(0x99);
            E(ramOffset);
            E(0xE1);

            // PLX - even out the stack; restore table read position
            E(0xFA);
}

static void ec_write_stat_table(struct sr_ctx *c, int numMaps, int statName, int *statOffsets, int maxValue, double bossMultiplier, int sixteenBit, double scalingRate) {
            int agi = statName == S_AGI;
            char dkey[64];
            statOffsets[statName] = c->working_offset + 0xC00000;
            snprintf(dkey, sizeof dkey, "%s_base", stat_key[statName]);
            double baseValue = w8_diff(c, dkey);
            snprintf(dkey, sizeof dkey, "%s_growth", stat_key[statName]);
            double growthValue = w8_diff(c, dkey) * scalingRate;
            snprintf(dkey, sizeof dkey, "%s_exp", stat_key[statName]);
            double expValue = w8_diff(c, dkey);
            for (int i = 0; i < numMaps; i++)
            {
                int statThisFloor = (int)(baseValue + pow((double)i, expValue) * growthValue);
                int bossStatThisFloor = (int)(statThisFloor * bossMultiplier);

                if (statThisFloor > maxValue)
                {
                    statThisFloor = maxValue;
                }
                if (bossStatThisFloor > maxValue)
                {
                    bossStatThisFloor = maxValue;
                }
                // 0 agi for aggressive bosses, otherwise you basically don't get to play
                if (agi && sr_settings_get_bool(c->settings, "aggBosses"))
                {
                    bossStatThisFloor = 0;
                }
                if (sixteenBit)
                {
                    P(c->working_offset, (uint8_t)statThisFloor);
                    P(c->working_offset + 1, (uint8_t)(statThisFloor>>8));
                    P(c->working_offset + numMaps * 2, (uint8_t)bossStatThisFloor);
                    P(c->working_offset + numMaps * 2 + 1, (uint8_t)(bossStatThisFloor >> 8));
                    c->working_offset += 2;
                }
                else
                {
                    P(c->working_offset, (uint8_t)statThisFloor);
                    P(c->working_offset + numMaps, (uint8_t)bossStatThisFloor);
                    c->working_offset++;
                }
            }

            // now skip the pointer past the boss stuff
            if (sixteenBit) c->working_offset += numMaps * 2;
            else c->working_offset += numMaps;
}

int sr_w8_hack_enemy_changes(struct sr_ctx *c) {
            const int numMaps = w9_num_floors(c, W8_NUM_BOSS_ENTRIES);
            const double scalingRate = w9_is_chaos(c) ? 24.0 / numMaps : sr_p3_is_ac(c) ? ceil(24.0 / numMaps) : 1.0;
            const int floorTableLocation = w9_is_chaos(c) ? w8_wget_int(c, "floorTableLocation") : -1;

            // write stat tables.. N normal-enemy values followed by N boss-values
            // if we're looking at a boss, add N to the index value to load the second set of values
            // X = mapnum; if enemy id >= boss threshold; x+=numMaps
            // LDA strRomAddr,x .. etc

            int statOffsets[S_COUNT];
            memset(statOffsets, 0, sizeof statOffsets);
            double bossStatMul = w8_diff(c, "bossStatMultiplier");
            double bossExpMul = w8_diff(c, "bossExpMultiplier");
            double bossGoldMul = w8_diff(c, "bossGoldMultiplier");
            double bossHpMul = w8_diff(c, "bossHpMultiplier");
            // level
            ec_write_stat_table(c, numMaps, S_LEVEL, statOffsets, 255, bossStatMul, false, scalingRate);
            // str
            ec_write_stat_table(c, numMaps, S_STR, statOffsets, 255, bossStatMul, false, scalingRate);
            // agi
            ec_write_stat_table(c, numMaps, S_AGI, statOffsets, 255, bossStatMul, false, scalingRate);
            // int
            ec_write_stat_table(c, numMaps, S_INT, statOffsets, 255, bossStatMul, false, scalingRate);
            // wis
            ec_write_stat_table(c, numMaps, S_WIS, statOffsets, 255, bossStatMul, false, scalingRate);
            // eva
            ec_write_stat_table(c, numMaps, S_EVADE, statOffsets, 255, bossStatMul, false, scalingRate);
            // meva
            ec_write_stat_table(c, numMaps, S_MEVADE, statOffsets, 255, bossStatMul, false, scalingRate);
            // mp
            ec_write_stat_table(c, numMaps, S_MP, statOffsets, 255, bossStatMul, false, scalingRate);
            // weapon level
            ec_write_stat_table(c, numMaps, S_WEAPON_LEV, statOffsets, 8, bossStatMul, false, scalingRate);
            // magic level
            ec_write_stat_table(c, numMaps, S_MAGIC_LEV, statOffsets, 8, bossStatMul, false, scalingRate);

            // weapon dmg
            ec_write_stat_table(c, numMaps, S_WEAPON_DAMAGE, statOffsets, 255, bossStatMul, false, scalingRate);

            // def
            ec_write_stat_table(c, numMaps, S_DEF, statOffsets, 65535, bossStatMul, true, scalingRate);
            // mdef
            ec_write_stat_table(c, numMaps, S_MDEF, statOffsets, 65535, bossStatMul, true, scalingRate);
            // exp
            ec_write_stat_table(c, numMaps, S_EXP, statOffsets, 65535, bossExpMul, true, scalingRate);
            // hp
            ec_write_stat_table(c, numMaps, S_HP, statOffsets, 65535, bossHpMul, true, scalingRate);
            // gold
            ec_write_stat_table(c, numMaps, S_GOLD, statOffsets, 65535, bossGoldMul, true, scalingRate);


            // MOPPLE: i don't remember exactly what these changes were for but some changes to logic in event types
            // 0x49 and 0x4a .. probably not the best fix for whatever this was a workaround for?

            // $01/F244 B0 26       BCS $26    [$F26C]      A:0700 X:0781 Y:0002 P:eNvMxdIzc
            P(0x1F244, 0xEA);
            P(0x1F245, 0xEA);
            // $01/F24C F0 1E       BEQ $1E    [$F26C]      A:0701 X:0781 Y:0002 P:envMxdIZC
            P(0x1F24C, 0xEA);
            P(0x1F24D, 0xEA);

            uint8_t statRamOffsets[S_COUNT];
            memset(statRamOffsets, 0, sizeof statRamOffsets);

            statRamOffsets[S_LEVEL] = 0x81;
            statRamOffsets[S_STR] = 0x88;
            statRamOffsets[S_AGI] = 0x89;
            statRamOffsets[S_INT] = 0x8B;
            statRamOffsets[S_WIS] = 0x8C;
            statRamOffsets[S_EVADE] = 0xA4;
            statRamOffsets[S_MEVADE] = 0xA7;
            statRamOffsets[S_MP] = 0x86; // current, then max (8 bit)
            statRamOffsets[S_DEF] = 0xA5;
            statRamOffsets[S_MDEF] = 0xA8;
            statRamOffsets[S_EXP] = 0x8D;
            statRamOffsets[S_HP] = 0x82; // current, then max (16 bit)
            statRamOffsets[S_GOLD] = 0xC8;
            statRamOffsets[S_WEAPON_LEV] = 0xC0;
            statRamOffsets[S_MAGIC_LEV] = 0xC4;

            // there are two different stat loaders here - one of these is used when the object is loaded (second one, i think?) and the
            // other is called now and then to refresh things like status

            // remove a few vanilla stat loaders first
            for (int i = 0x5625; i <= 0x562e; i++)
            {
                P(i, 0xEA);
            }
            for (int i = 0x5636; i <= 0x5643; i++)
            {
                P(i, 0xEA);
            }
            for (int i = 0x5652; i <= 0x5666; i++)
            {
                P(i, 0xEA);
            }
            for (int i = 0x567f; i <= 0x5688; i++)
            {
                P(i, 0xEA);
            }

            P(0x5625, 0x22);
            P(0x5626, (uint8_t)(c->working_offset));
            P(0x5627, (uint8_t)(c->working_offset >> 8));
            P(0x5628, (uint8_t)((c->working_offset >> 16) + 0xC0));

            int tonpoleMp = w8_diff_int(c, "tonpoleMp");
            // replace with subroutine for hp, def, mdef, str, agi, int, wis, exp
            ec_write_stat_injection_subroutine(c, numMaps, statOffsets, statRamOffsets, true, tonpoleMp, floorTableLocation);


            // remove a couple vanilla stat loaders in the second block, too
            for (int i = 0x477c; i <= 0x47A0; i++)
            {
                P(i, 0xEA);
            }
            for (int i = 0x47a8; i <= 0x47b5; i++)
            {
                P(i, 0xEA);
            }

            P(0x477c, 0x22);
            P(0x477d, (uint8_t)(c->working_offset));
            P(0x477e, (uint8_t)(c->working_offset >> 8));
            P(0x477f, (uint8_t)((c->working_offset >> 16) + 0xC0));
            // replace with subroutine for str, agi, int, wis, evade, def, mevade, mdef, [species, element]
            // same as above except without hp/mp.
            ec_write_stat_injection_subroutine(c, numMaps, statOffsets, statRamOffsets, false, tonpoleMp, floorTableLocation);

            // weapon changes
            P(0x49C5, 0x22);
            P(0x49C6, (uint8_t)(c->working_offset));
            P(0x49C7, (uint8_t)(c->working_offset >> 8));
            P(0x49C8, (uint8_t)((c->working_offset >> 16) + 0xC0));
            P(0x49C9, 0xEA);

            // CPX #0600 - check object id
            E(0xE0);
            E(0x00);
            E(0x06);

            // BCS enemy (06)
            E(0xB0);
            E(0x06);

            // (replaced code, for non-enemies, then return)
            // LDY #$1008
            E(0xA0);
            E(0x08);
            E(0x10);

            // LDA [$C0], y
            E(0xB7);
            E(0xC0);

            // RTL
            E(0x6B);

            // enemy:
            // AF DC 00 7E LDA 7E00DC
            // 18          CLC
            // 69 02       ADC #$08
            // 6B          RTL
            // ^ old
            // new:
            // PHX
            E(0xDA);

            // REP 20
            E(0xC2);
            E(0x20);

            // LDA 7E00DC - load map number, to determine floor number
            E(0xAF);
            E(0xDC);
            E(0x00);
            E(0x7E);

            if (floorTableLocation < 0)
            {
                // AND #00FF
                E(0x29);
                E(0xFF);
                E(0x00);
            }

            // check for mana beast (not necessary here, since it's normal enemies, but just to be safe)
            // CMP #00FD
            E(0xC9);
            E(0xFD);
            E(0x00);

            // not mana beast? skip
            // BNE skip
            E(0xD0);
            E(0x03);

            // special case mana beast
            // LDA #(numMaps - 1) * 2
            E(0xA9);
            E((uint8_t)((numMaps - 1) * 2));
            E(0x00);

            // skip:
            if (floorTableLocation < 0)
            {
                // shift right since two maps per floor
                // LSR
                E(0x4A);
            }
            else
            {
                // look up floorTableLocation first
                // TAX
                E(0xAA);
                // LDA floorTableLocation,X
                E(0xBF);
                E((uint8_t)(floorTableLocation));
                E((uint8_t)(floorTableLocation >> 8));
                E((uint8_t)((floorTableLocation >> 16) + 0xC0));

                // AND #00FF
                E(0x29);
                E(0xFF);
                E(0x00);
            }

            // TAX
            E(0xAA);

            int weaponRomOffset = statOffsets[S_WEAPON_DAMAGE];

            // SEP 20
            E(0xE2);
            E(0x20);

            // LDA romOffset, x
            E(0xBF);
            E((uint8_t)weaponRomOffset);
            E((uint8_t)(weaponRomOffset >> 8));
            E((uint8_t)(weaponRomOffset >> 16));

            // PLX
            E(0xFA);

            // RTL
            E(0x6B);


            // boss weapon changes
            // essentially the same as above but pull from rom loc + numMaps for the second set of values.
            P(0x462C, 0x22);
            P(0x462D, (uint8_t)(c->working_offset));
            P(0x462E, (uint8_t)(c->working_offset >> 8));
            P(0x462F, (uint8_t)((c->working_offset >> 16) + 0xC0));

            // PHX
            E(0xDA);

            // REP 20
            E(0xC2);
            E(0x20);

            // LDA 7E00DC
            E(0xAF);
            E(0xDC);
            E(0x00);
            E(0x7E);

            if (floorTableLocation < 0)
            {
                // AND #00FF
                E(0x29);
                E(0xFF);
                E(0x00);
            }

            // check for mana beast
            // CMP #00FD
            E(0xC9);
            E(0xFD);
            E(0x00);

            // not mana beast? skip
            // BNE skip
            E(0xD0);
            E(0x03);

            // special case mana beast
            // LDA #(numMaps - 1) * 2
            E(0xA9);
            E((uint8_t)((numMaps - 1) * 2));
            E(0x00);

            // skip:
            if (floorTableLocation < 0)
            {
                // shift right since two maps per floor
                // LSR
                E(0x4A);
            }
            else
            {
                // look up floorTableLocation first
                // TAX
                E(0xAA);
                // LDA floorTableLocation,X
                E(0xBF);
                E((uint8_t)(floorTableLocation));
                E((uint8_t)(floorTableLocation >> 8));
                E((uint8_t)((floorTableLocation >> 16) + 0xC0));

                // AND #00FF
                E(0x29);
                E(0xFF);
                E(0x00);
            }

            // TAX
            E(0xAA);

            int bossWeaponRomOffset = statOffsets[S_WEAPON_DAMAGE] + numMaps;

            // SEP 20
            E(0xE2);
            E(0x20);

            // LDA romOffset, x
            E(0xBF);
            E((uint8_t)bossWeaponRomOffset);
            E((uint8_t)(bossWeaponRomOffset >> 8));
            E((uint8_t)(bossWeaponRomOffset >> 16));

            // PLX
            E(0xFA);

            // RTL
            E(0x6B);

            return 1;
}

/* ----------------------------------------------------------------------------------------------------- DropChanges */
int sr_w8_hack_drop_changes(struct sr_ctx *c) {
            const int granularity = sr_p3_is_ac(c) ? (int)ceil(24.0 / sr_p3_ac_num_floors(c->settings)) : 1;
            const int dropTableLocation = w9_is_chaos(c) ? w8_wget_int(c, "dropTableLocation") : -1;
            struct sr_random *r = &c->rf;
            int itemTableOffset = c->working_offset;

            // drop tables
            // 64 possible drops per floor
            // [14] x01-x14 hats
            // [14] x16-x29 armor
            // [14] x2b-x3e accesories
            // [14] x40-x4b usables
            // [08] x80-x87 orbs

            // new code:
            // load floor num
            // >> 1
            // << 6
            // index into new table with that
            // add rng % 64
            // use that item

            // base id for each floor
            uint8_t hatIds[64], armorIds[64], accessoryIds[64];

            // in order
            for (int i = 0; i < 64; i++)
            {
                hatIds[i] = (uint8_t)(0x01 + ((0x14 - 0x01 + 1) * i / 64.0));
                armorIds[i] = (uint8_t)(0x16 + ((0x29 - 0x16 + 1) * i / 64.0));
                accessoryIds[i] = (uint8_t)(0x2b + ((0x3e - 0x2b + 1) * i / 64.0));
                // faerie ring and wrist band wtf
                if (accessoryIds[i] == 0x3E)
                {
                    accessoryIds[i] = 0x2B;
                }
                else if (accessoryIds[i] == 0x2B)
                {
                    accessoryIds[i] = 0x3E;
                }
                // overalls, kung fu suit, midge robe .. never drop since we start with them
                if(armorIds[i] == 0x16 || armorIds[i] == 0x17 || armorIds[i] == 0x18)
                {
                    // chain vest instead
                    armorIds[i] = 0x19;
                }
            }

            // should add up to 64.
            int numHatsInTable = 9;
            int numArmorsInTable = 9;
            int numAccInTable = 9;
            int numUsablesInTable = 21; // should be a multiple of 7
            int numOrbsInTable = 16; // should be a multiple of 8

            static const uint8_t usefulUsables[] = { 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x4A };
            uint8_t usableIds[21];
            // 00: Candy
            // 01: Chocolate
            // 02: Royal Jam
            // 03: Faerie Walnut
            // 04: Medical Herb
            // 05: Cup of Wishes
            // * 06: Magic Rope -> Stuck
            // * 07: Flammie Drum -> Change Music
            // * 08: Moogle Belt
            // * 09: Midge Mallet
            // 0A: Barrel
            // * 0B: ? (Dummied Out)
            for (int i = 0; i < numUsablesInTable; i++)
            {
                usableIds[i] = usefulUsables[i % (int)sizeof usefulUsables];
            }

            // chance of any on any floor
            uint8_t orbIds[16];
            for (int i = 0; i < numOrbsInTable; i++)
            {
                orbIds[i] = (uint8_t)(0x80 + (i % 8));
            }

            // 64 floors
            for (int i = 0; i < 32; i++)
            {
                int itemShifts[512];
                int nShifts = 0;
                // easy: -3 -> 12
                int spread = 12;
                int lowLimit = 3;
                for (int id = 0; id < spread; id++)
                {
                    for (int j = id; j <= spread; j++)
                    {
                        int val1 = i * 2 * granularity + j;
                        if (val1 < 64)
                        {
                            itemShifts[nShifts++] = val1;
                        }
                        int val2 = i * 2 * granularity - j;
                        if (val2 >= 0 && j <= lowLimit)
                        {
                            itemShifts[nShifts++] = val2;
                        }
                    }
                }

                // hats
                for (int item = 0; item < numHatsInTable; item++)
                {
                    int hatIndex = itemShifts[sr_random_next(r) % nShifts];
                    if (hatIndex < 0)
                    {
                        hatIndex = 0;
                    }
                    if (hatIndex > 63)
                    {
                        hatIndex = 63;
                    }
                    P(itemTableOffset + i * 64 + item, hatIds[hatIndex]);
                }

                // armors
                for (int item = 0; item < numArmorsInTable; item++)
                {
                    int armorIndex = itemShifts[sr_random_next(r) % nShifts];
                    if (armorIndex < 0)
                    {
                        armorIndex = 0;
                    }
                    if (armorIndex > 63)
                    {
                        armorIndex = 63;
                    }
                    P(itemTableOffset + i * 64 + item + numHatsInTable, armorIds[armorIndex]);
                }

                for (int item = 0; item < numAccInTable; item++)
                {
                    int accIndex = itemShifts[sr_random_next(r) % nShifts];
                    if (accIndex < 0)
                    {
                        accIndex = 0;
                    }
                    if (accIndex > 63)
                    {
                        accIndex = 63;
                    }
                    P(itemTableOffset + i * 64 + item + numHatsInTable + numArmorsInTable, accessoryIds[accIndex]);
                }

                for (int item = 0; item < numUsablesInTable; item++)
                {
                    P(itemTableOffset + i * 64 + item + numHatsInTable + numArmorsInTable + numAccInTable, usableIds[item]);
                }
                for (int item = 0; item < numOrbsInTable; item++)
                {
                    P(itemTableOffset + i * 64 + item + numHatsInTable + numArmorsInTable + numAccInTable + numUsablesInTable, orbIds[item]);
                }
            }

            // item table done
            c->working_offset += 32 * 64;

            // remove a block of vanilla code that reads drop tables to determine the item dropped
            // ...
            // $C8 / E156 BF 53 3A D0 LDA $D03A53,x[$D0:3A53] A:0FC0 X:0000 Y:0000 P:envMxdIzC - table read
            // $C8 / E15A 80 22       BRA $22[$E17E]          A:0F40 X:0000 Y:0000 P:envMxdIzC
            // ...
            // $C8 / E17E 85 A4       STA $A4[$00:03A4]       A:0F40 X:0000 Y:0000 P:envMxdIzC (keep - this ends up being the id of the drop)
            // $C8 / E180 C9 00       CMP #$00                A:0F40 X:0000 Y:0000 P:envMxdIzC

            for (int i = 0x8E12C; i < 0x8E17E; i++)
            {
                P(i, 0xEA);
            }

            // 22 to thingy
            P(0x8E12C, 0x22);
            P(0x8E12D, (uint8_t)(c->working_offset));
            P(0x8E12E, (uint8_t)(c->working_offset >> 8));
            P(0x8E12F, (uint8_t)((c->working_offset >> 16) + 0xC0));

            // this was to fix a weird loading bug that sometimes triggered non-drop events/text when opening a chest
            // $C8/E11E 89 01       BIT #$01                A:0FAA X:00D7 Y:0000 P:envMxdIzC
            // $C8/E120 D0 E7       BNE $E7[$E109]          A:0FAA X:00D7 Y:0000 P:envMxdIZC -> change to 0x80 to branch always
            // $C8/E122 A6 85       LDX $85[$00:0385]       A:0FAA X:00D7 Y:0000 P:envMxdIZC

            P(0x8E120, 0x80);

            // REP 20
            E(0xC2);
            E(0x20);
            // LDA 7E00DC - map number
            E(0xAF);
            E(0xDC);
            E(0x00);
            E(0x7E);

            if (dropTableLocation < 0)
            {
                // AND #00FF
                E(0x29);
                E(0xFF);
                E(0x00);
            }

            if (dropTableLocation < 0)
            {
                // LSR - shift right since two maps per floor
                E(0x4A);
            }
            else
            {
                // look up floorTableLocation first
                // TAX
                E(0xAA);
                // LDA floorTableLocation,X
                E(0xBF);
                E((uint8_t)(dropTableLocation));
                E((uint8_t)(dropTableLocation >> 8));
                E((uint8_t)((dropTableLocation >> 16) + 0xC0));
                // AND #00FF
                E(0x29);
                E(0xFF);
                E(0x00);
            }

            // ASL 6x for * 64, the size of the drop table per floor
            E(0x0A);
            E(0x0A);
            E(0x0A);
            E(0x0A);
            E(0x0A);
            E(0x0A);
            // PHA
            E(0x48);
            // SEP 20
            E(0xE2);
            E(0x20);
            // JSL C0389C - vanilla rng
            E(0x22);
            E(0x9C);
            E(0x38);
            E(0xC0);
            // REP 20
            E(0xC2);
            E(0x20);
            // AND #003F - rng value 0->3F (size of drop table)
            E(0x29);
            E(0x3F);
            E(0x00);
            // STA $A4 - write the drop index temporarily
            E(0x85);
            E(0xA4);
            // PLA
            E(0x68);
            // CLC
            E(0x18);
            // ADC $A4
            E(0x65);
            E(0xA4);
            // TAX
            E(0xAA);
            // LDA tableOffset,x - load the value in the drop table
            E(0xBF);
            E((uint8_t)itemTableOffset);
            E((uint8_t)(itemTableOffset>>8));
            E((uint8_t)(0xC0 + (itemTableOffset >> 16)));
            // 29 FF 00       AND 00FF
            E(0x29);
            E(0xFF);
            E(0x00);
            // SEP 20
            E(0xE2);
            E(0x20);
            // RTL (then write to $A4 once we return, with vanilla code, to set the drop)
            E(0x6B);

            // drop rates up for now
            for (int i = 0; i < 128; i++)
            {
                P(0x103A50 + i * 5 + 2, 0xF8);
            }

            // change drops, more often
            P(0x428E, 0x0F);

            // $C0/4292 BD CE E1    LDA $E1CE,x[$7E:E7CE]   A:0F06 X:0600 Y:0058 P:envMxdIzC
            // $C0 / 4295 85 A4 STA $A4[$00:03A4]   A: 0F84 X: 0600 Y: 0058 P: eNvMxdIzC
            // LDA #??   84 for fish, this did not work well

            // this is part of a check for current orb level to decide whether weapon orbs should be allowed to drop
            // change to lda #08 to just allow them to always drop.
            // $C8 / E1A9: B9C0CF   LDA $CFC0,Y

            // LDA #08
            P(0x8e1a9, 0xa9);
            P(0x8e1aa, 0x08);
            // NOP
            P(0x8e1ab, 0xea);

            return 1;
}

/* ----------------------------------------------------------------------------------------------------- ShopChanges */
int sr_w8_hack_shop_changes(struct sr_ctx *c) {
            struct sr_random *r = &c->rf;
            // take out neko price doubling
            P(0x7CEE, 0xEA);
            P(0x7CEF, 0xEA);
            P(0x7CF0, 0xEA);

            // shop data
            // BA is candy (0x40 for drops .. 7A diff)
            // BE is med. herb
            // BF is cup of wishes
            // bandana is 7c (0x01 in drops .. 7B diff)
            // overalls is 91 (0x16 in drops .. 7B diff)
            // faerie ring, which for silly reasons is the first ring, is a6

            // base id for each shop
            uint8_t hatIds[64], armorIds[64], accessoryIds[64];

            static const uint8_t usefulUsables[] = { 0xBA, 0xBB, 0xBC, 0xBD, 0xBE, 0xBF, 0xC4 };

            // in order
            // basically the same thing we do in DropChanges but with different item IDs
            for (int i = 0; i < 64; i++)
            {
                hatIds[i] = (uint8_t)(0x7C + ((0x14 - 0x01 + 1) * i / 64.0));
                armorIds[i] = (uint8_t)(0x91 + ((0x29 - 0x16 + 1) * i / 64.0));
                accessoryIds[i] = (uint8_t)(0xA6 + ((0x3e - 0x2b + 1) * i / 64.0));
                // swap faerie ring and wrist band, which are swapped in vanilla
                if (accessoryIds[i] == 0xB9)
                {
                    accessoryIds[i] = 0xA6;
                }
                else if (accessoryIds[i] == 0xA6)
                {
                    accessoryIds[i] = 0xB9;
                }
                // overalls, kung fu suit, midge robe .. never drop since we start with them
                if (armorIds[i] == 0x91 || armorIds[i] == 0x92 || armorIds[i] == 0x93)
                {
                    // chain vest instead
                    armorIds[i] = 0x94;
                }
            }

            int numHatsInTable = 2;
            int numArmorsInTable = 2;
            int numAccInTable = 2;
            int numUsablesInTable = 3;

            // generate shops
            for (int i = 0; i < 16; i++)
            {
                int shopItems[64];
                int nShop = 0;
                int itemShifts[512];
                int nShifts = 0;
                // easy: -3 -> 12
                int spread = 12;
                int lowLimit = 3;
                for (int id = 0; id < spread; id++)
                {
                    for (int j = id; j <= spread; j++)
                    {
                        int val1 = i * 2 + j;
                        if (val1 < 64)
                        {
                            itemShifts[nShifts++] = (val1);
                        }
                        int val2 = i * 2 - j;
                        if (val2 >= 0 && j <= lowLimit)
                        {
                            itemShifts[nShifts++] = (val2);
                        }
                    }
                }

                // hats
                for (int item = 0; item < numHatsInTable; item++)
                {
                    int hatIndex = 0;
                    int ok = 0;
                    int iter = 0;
                    while (!ok && iter < 10)
                    {
                        hatIndex = itemShifts[(sr_random_next(r) % nShifts)];
                        if (hatIndex < 0)
                        {
                            hatIndex = 0;
                        }
                        if (hatIndex > 63)
                        {
                            hatIndex = 63;
                        }
                        ok = !w8_in_list(shopItems, nShop, hatIds[hatIndex]);
                        iter++;
                    }
                    if (ok)
                    {
                        shopItems[nShop++] = hatIds[hatIndex];
                    }
                }

                // armors
                for (int item = 0; item < numArmorsInTable; item++)
                {
                    int ok = 0;
                    int iter = 0;
                    int armorIndex = 0;
                    while (!ok && iter < 10)
                    {
                        armorIndex = itemShifts[(sr_random_next(r) % nShifts)];
                        if (armorIndex < 0)
                        {
                            armorIndex = 0;
                        }
                        if (armorIndex > 63)
                        {
                            armorIndex = 63;
                        }
                        ok = !w8_in_list(shopItems, nShop, armorIds[armorIndex]);
                        iter++;
                    }
                    if (ok)
                    {
                        shopItems[nShop++] = armorIds[armorIndex];
                    }
                }

                for (int item = 0; item < numAccInTable; item++)
                {
                    int ok = 0;
                    int iter = 0;
                    int accIndex = 0;
                    while (!ok && iter < 10)
                    {
                        accIndex = itemShifts[(sr_random_next(r) % nShifts)];
                        if (accIndex < 0)
                        {
                            accIndex = 0;
                        }
                        if (accIndex > 63)
                        {
                            accIndex = 63;
                        }
                        ok = !w8_in_list(shopItems, nShop, accessoryIds[accIndex]);
                        iter++;
                    }
                    if (ok)
                    {
                        shopItems[nShop++] = accessoryIds[accIndex];
                    }
                }

                for (int item = 0; item < numUsablesInTable; item++)
                {
                    int itemIndex = 0;
                    int ok = 0;
                    int iter = 0;
                    while(!ok && iter < 10)
                    {
                        itemIndex = usefulUsables[sr_random_next(r) % (int)sizeof usefulUsables];
                        ok = !w8_in_list(shopItems, nShop, itemIndex);
                        iter++;
                    }
                    if (ok)
                    {
                        shopItems[nShop++] = itemIndex;
                    }
                }

                // 11 bytes per; terminate with FF, 10 items
                int offset = 0xFC52 + i * 10;
                P(0x18FC32 + i * 2, (uint8_t)offset);
                P(0x18FC32 + i * 2 + 1, (uint8_t)(offset>>8));
                for(int j=0; j < 9; j++)
                {
                    if (j < nShop)
                    {
                        P(0x180000 + offset++, (uint8_t)shopItems[j]);
                    }
                    else
                    {
                        P(0x180000 + offset++, 0xFF);
                    }
                }
                P(0x180000 + offset++, 0xFF);
            }

            return 1;
}

