# Secret of Mana (USA) - combat and stat formulas extracted from the ROM

Tags: **[V]** verified = the arithmetic was reproduced by a Python model that was checked against the REAL 65816 routine of the ROM running in `tools/cpu65816.py` with randomised inputs (counts given per section), or the value was cross-checked against live game objects stored in ZSNES save states. **[C]** read from the disassembly, read, not executed. Anything not determined is listed in section 18 and stated there as unknown.

The repo contains only code, notes, and the gameplay-number tables in `data/*.json` (raw table bytes and values computed by the routines; no graphics, no text).
ROM: 2 MiB HiROM, `$Cx:xxxx` (banks C0-DF) = file offset `((bank-0xC0)<<16)|addr`. All file offsets below are file offsets (e.g. `0x101C00`). Save states used: six ZSNES save states of a late-game session (see README).

## 0. Summary

All values are integers; `rnd(n) = (R * n) >> 8` where `R` is the next byte of the game's RNG (0..255). "cap999" = `min(v, 999)`.

```
# --- per-hit physical damage, attacker A, defender D (all [V], §4)
A.atk  = (str + weapon.power) & 255          A.acc = min(99, agi//4 + weapon.hit)      # heroes; monsters: rec stats + their weapon row
D.eva  = min(99, agi//4 + sum(armor.evade))  D.def = min(999, con + sum(armor.def))    # heroes; monsters: record bytes 8, 9-10
ev  = D.eva  +-25% if D has Speed-Up/Down buff (E1B0 bit2/bit3), cap 99
df  = D.def  +-25% if Defender/Acid buff (E1B0 bit0/bit1);  df //= 4 if D is Pygmized;  cap 999
acc = A.acc, -25%/+25% only when A has bit E190&4 (quirk, §4.2), cap 99; acc //= 2 if A's weapon gauge E1ED != 0
HIT iff acc >= ev//2 + rnd(ev//2)                                                      # no other miss roll
m   = 2 normally (1 if resisted, 4 if weak; see §6 for the quirk)                      # i.e. x1, x0.5, x2
v   = A.atk * (2*stage + 4) // 4              # stage = charge stage of the swing (0..weapon level)  == atk*(1+stage/2)
v  += or -= 25% for ATK-up/down buff (E1B0 bit6/bit7);   v += base // (10 - n)  if saber level n=1..9 active
v   = cap999(v * m // 2);  if A is Pygmized: v = 1
roll = rnd(101); if roll >= acc:  v = cap999(acc * v // 100)         # "glancing": reduced by accuracy
                 else:            v = cap999(v + (E1E6)^2 + rnd(v//16))   # E1E6 = $CFFC for heroes: 0 at new game, 8 in all six saves (+64); $CFFB for monsters (0). See §11.6
roll = rnd(100); if roll >= ev:   D' = cap999(ev * df // 100)        # evade says how often defense is applied in full
                 else:            D' = cap999(df + rnd(df//16))
crit (not if E1ED != 0): chance = (E196 [*2 if D.E190&0x10] [+5 for hero attackers, +10 when the attacker is hero slot 1]) / 256   -> v *= 2 (after the caps)
v  -= D.E1BF if D has the flat-guard flag (E1B1 bit4) and then D'=0
damage = max(0, v - D')                        # NO minimum of 1 for weapons; applied damage is capped to 999 per hit
```
Magic (§5): `power = (p + stat) & 255` (p = spell row byte 10 for heroes, monster record byte 18/19), `v = cap999(power * (level+2) // 2)`, accuracy roll `rnd(101) >= acc` (acc = min(99, stat//4 + 75)) -> `acc*v//100` else `v + E1E6^2 + rnd(v//16)`, `* m // 2`, `// divisor`, cap 999, then `damage = max(1, v - mdef')` (magic has a minimum of 1), `mdef'` as the defense roll with MEv. Heal spells ignore the target defense (`m = 4`).

Weapon and magic levels, drops, statuses, boss rows, items (new, all executed unless tagged): weapon progress `+= 9 - level` per kill (halved vs lower-level monsters, others get half), cap `grade + 1`, max 8 (§11.2); one charge stage = 90 frames = 1.5 s (§11.3); spells level only while `level < $CFFC` and `$CFFC` also adds `+CFFC^2` (64 in late saves) to every non-glancing hit (§11.5-11.6); chest chance 4/64 for 77 monsters, content bytes decoded (§13); statuses timers drop every 4th tick (12 ticks/s -> d = 20 lasts 6.4 s), poison/engulf 1 HP per drop (§12); spell divisor = number of valid targets 1..3 (§14); 115 boss attack rows and the boss -> row map (§15); item effects (§16).

Level-up (§8): NO random growth. Every hero stat per level is a fixed ROM table (`data/hero_levels.json`); every living hero receives the full monster EXP; EXP/GP cap 9,999,999; level cap 99 (internal index 98); HP is fully restored on level-up.

## 1. Conventions

### 1.1 Objects
All actors are 0x200-byte records at `$7E:E000 + slot*0x200` ([V] harness runs on save states). Slots 0,1,2 = boy, girl, sprite (object id `E1E7` = 0x80,0x81,0x82); slots 3,4,5 (`$E600,$E800,$EA00`) = monsters/bosses (the Lich sits in slot 3). Offsets (relative to the record) used by the combat code:

| off | meaning | tag |
|---|---|---|
| 0x181 | level, 0-based internal (display = +1) | [V] row 0 of the hero table is the starting stats |
| 0x182 / 0x184 | HP / max HP (word) | [V] |
| 0x186 / 0x187 | MP / max MP | [V] |
| 0x188 0x189 0x18A 0x18B 0x18C | Str, Agi, Con, Int, Wis (monsters: Con = 0) | [V] |
| 0x17D | EXP needed for the next level (24-bit) | [V] |
| 0x18D | EXP: heroes = total, monsters = reward (24-bit) | [V] |
| 0x190, 0x191 | status words/flags (see §7, §12) | [V] |
| 0x192 / 0x193 | monster type / monster element byte (resist) | [V] |
| 0x194 / 0x195 | weapon byte-5 mask / weapon flags (bit 6 of 0x195 = drain, §4.7; Saber spells OR an element bit into 0x195, §4.6) | [V] code |
| 0x196 | crit base | [V] |
| 0x197 | accuracy | [V] |
| 0x198 | attack power | [V] |
| 0x199 | status word inflicted by hits | [V] |
| 0x19B | charge stage of the current swing | [V] (sim of `$C0:B330`) |
| 0x19C | weapon level of the equipped weapon type (0..8) | [V] |
| 0x19D | remaining hits of an active Saber enchant | [V] |
| 0x1A0 / 0x1A2 | weak-element / resist-element words (the useful bytes are 0x1A1 and 0x1A3) | [V] |
| 0x1A4 | evade (0..99) | [V] |
| 0x1A5 | defense (word, <=999) | [V] |
| 0x1A7 | magic evade | [V] |
| 0x1A8 | magic defense (word) | [V] |
| 0x1AA | status immunity word | [V] |
| 0x1AE | low nibble = Saber level n, high nibble = counter that forces a crit and is decremented per hit (§4.3) | [V] code |
| 0x1B0 | buff bits: 01 DEF+25%, 02 DEF-25%, 04 EVA+25%, 08 EVA-25%, 10 ACC+25%, 20 ACC-25%, 40 ATK+25%, 80 ATK-25% | [V] |
| 0x1B1 | flags: bit4 = flat damage reduction active (E1BF); bit1 not determined | [V] |
| 0x1B2-0x1B6 | status timers (see §7) | [V] |
| 0x1C0-0x1C3 | weapon levels, 8 nibbles (even weapon type = high nibble, odd = low) | [V] |
| 0x1C4-0x1C7 | magic levels, 8 nibbles by element index (same nibble rule) | [V] |
| 0x1C8 | monster: gold reward | [V] |
| 0x1D0-0x1D7 / 0x1D8-0x1DF | weapon-level progress / magic-level progress per type/element (0..99) | [V] |
| 0x1E0 0x1E1 0x1E2 | equipped head / body / accessory armor ids | [V] |
| 0x1E4 / 0x1E8 0x1E9 | weapon type (hero) / weapon row ids (short, long range) | [V] |
| 0x1E6 | `$CFFB` (monsters) / `$CFFC` (heroes). New game: 0 (`$C0:5787`); the six save states all have `$CFFC = 8`, `$CFFB = 0`. Added as `E1E6^2` to every non-glancing hit and is also the spell-level cap (§11.5, §11.6) | [V] |
| 0x1ED | weapon-recharge gauge | [V] |
| 0x1F1 0x1F3 0x1F5 0x1F6 | pending: HP damage, HP heal, MP damage, MP heal | [V] |
| 0x1F7 | status chance of the weapon (%) | [V] |
| 0x1FB | bit7 hero, bit6 boss (`id >= 0x57`), bit4 set for object ids `0x55-0x56`; bit5 not determined | [V] |
| 0x1FD 0x1FE | monster spell power (Int-class / Wis-class spells) | [V] |
| 0x170 0x171 0x176 0x177 | caster object: spell id, spell level, damage divisor, element index | [V] |

### 1.2 RNG [V]
`$C0:38A0`: 15 bytes at `$03F1-$03FF` with index `$03F0` (0..14): `t[i-1] += t[i]` (index wraps 0 -> 14), returns the new byte, index increments. `$C0:3884` with `A=n` returns `(R*n)>>8`. Model: `tools/rng.py` (bit exact, used by all validators).

### 1.3 Timing [C]
The main loop (`$C0:B070-B08A`) runs once per NMI (60.0988 Hz NTSC) and keeps `$56` = 0..4. Per-frame work (input handler `$C0:B69C`, charge gauge `$C0:B330`, weapon recharge gauge `E1ED` at `$C0:EB4F`) runs every frame. The per-actor combat tick `$C0:3A79` (hit resolution, status timers, damage over time, pending HP) is called from the object step `$C0:F4CB`, which `$C0:B0D3` dispatches by `$56`: monster slots 3, 4, 5 at `$56` = 0, 1, 2 and the three heroes at `$56` = 3 (when `$ED` bit 7 is set, as in all save states). So **every ordinary actor ticks once per 5 frames = 12.02 Hz**. The C2 boss engine (`$C2:0003`, single call site `$C0:FD12`, reached from the same three dispatch entries) is called only for `$56` = 0, 1, 2 and runs an object when `(flags_hi ^ $56) & 3 == 0` (or flag 0x4000): objects with flags high byte `&3 == 0` (Dark Lich `0x9C`, Mana Beast `0x18`) tick at `$56` = 0 only, **12.02 ticks/s as well**; high byte `&3 == 1` and `2` tick at `$56` = 1 and 2; `&3 == 3` never ticks unless flag 0x4000 is set (`docs/mana-beast.md` section 1, re-verified by re-running the Lich with the full frame `$C0:B08C`). "Object tick" below means this 12 Hz tick unless a section says "frame". Section 17 lists all durations in seconds.

## 2. Data tables (all dumped to `data/`, see `tools/dump_tables.py`)

| table | file offset | layout | JSON |
|---|---|---|---|
| Monster stat records | `0x101C00 + id*29`, ids 0-0x7F | §2.1 | `monsters.json` |
| Monster drop rows | `0x103A50 + id*5` | §2.1 | in `monsters.json` |
| Weapon rows | `0x101000 + id*12` (ids 0-71 player weapons, 72+ monster attacks) | §2.2 | `weapons.json` |
| Armor rows | `0x103ED0 + id*10` (head 0-20, body 21-41, accessory 42-62) | §2.3 | `armors.json` |
| Spell rows | `0x102AD0 + id*64`, ids 0-0x29 | §2.4 | `spells.json` |
| Hero level tables | `0x104210 + hero*0x318 + level*8` | §2.5 | `hero_levels.json` |
| EXP table | `0x104B58 + level*3` (24-bit) | §2.5 | `hero_levels.json` |
| Boss/special attacks | `0x10BDC1 + id*7` | §2.6 | `boss_attacks.json` |
| Consumables | effect byte `0x104152 + id*16` | §9 | `items.json` |

### 2.1 Monster record, 29 bytes [V] (copy code `$C0:55EC`, `$C0:474F`; checked against the Lich and by running the init on 11 ids)
```
0 level            1-2 max HP         3 MP (also starting MP)
4 Str  5 Agi  6 Int  7 Wis            (Con is not stored: 0)
8 evade            9-10 defense       11 magic evade      12-13 magic defense
14 type byte  (obj+0x192; bit 0x20 reverses drain weapons, see §4.7)
15 element byte (obj+0x193 = resist mask, weakness = opposite element, see §6)
16-17 EXP reward   18 spell power for Int-class spells (spell id % 6 < 3)   19 same for Wis-class
20-21 status immunity word            22 not determined (0 for all records seen)
23 weapon row id, short range         24 weapon row id, long range      (rows of §2.2, monster rows start at id 72)
25 flag byte (bit0 -> E1B1 bit0, bit7 -> E1B1 bit1, bit6 -> E1B1 bit3, whole byte -> E1CA) [C]
26 high nibble = weapon level, low nibble = magic level
27-28 gold reward (obj+0x1C8)
```
Drop row (5 bytes, D0:3A50 + id*5): fully decoded and validated in §13 (`data/drops.json`). `0xFF` rows = no drop.

Boss records also carry the Lich-style fields; bosses (`id >= 0x57`) read their ATTACK numbers from the boss attack table (§2.6) instead of the weapon table.

### 2.2 Weapon row, 12 bytes [V] (`$C0:49AA`, `$C0:4A0C`, `$C0:4AFF`, `$C0:48B4`)
```
0 weapon type 0-7 (ids 0-71 = 8 types x 9)
1 stat bits: Agi = tbl[b>>6], Con = tbl[(b>>4)&3], Int = tbl[(b>>2)&3], Wis = tbl[b&3]       tbl = [0,+1,+5,-5]  (at $C0:4B79)
2 bits0-1: Str = tbl[b&3];  other bits not determined (0xC0/0xC1/0xC2 in all rows)
3,4 graphics (not dumped)
5 mask copied to obj+0x194, 6 crit base, 7 hit %, 8 power
9-10 status word inflicted, 11 status chance %
```
Examples [V from the dump]: every player weapon has hit 75 (0x4B) except id 17 with 99; powers run 2..127 (id 17: 127); only 13 player weapons have a status word (all with chance 80%); the byte-5 masks are single bits.

### 2.3 Armor row, 10 bytes [V] (`$C0:4860`, `$C0:4B7D`, `$C0:4A2D`, `$C0:4A75`)
```
0 flags: bits 4,3,2,1 = +5 Str / Agi / Con / Int   (0x40 is set on every row; its "-2" use is a quirk, see §3)
1 defense  2 evade  3 magic defense  4 magic evade
5 equip mask (0x80 boy, 0x40 girl, 0x20 sprite)   6 element resist mask   7-8 status immunity word   9 unknown
```
The evade/magic-evade columns are constants per slot (head 23, body 37, accessory 15 -> sum 75; magic evade the same), so a fully dressed hero has `eva = agi//4 + 75`.

### 2.4 Spell row, 64 bytes [V for fields used by the validated handlers]
`10` power (hero casters), `11` accuracy base (75 for all), `12` element mask, `13-14` status word, `15` MP cost, plus pointers/graphics (not dumped). `$C8:E801 + id*2` = handler address (listed in `spells.json`). Table in §5.5.

### 2.5 Hero tables [V against 16 hero objects]
`HP(word), MP, Str, Agi, Con, Int, Wis` per internal level 0..98 and hero (boy/girl/sprite), Row 0 = starting stats (boy 50 HP, 15/15/13/5/5). Max level row: boy HP 999 / 99 Str, girl and sprite 800 HP. EXP to next level: `ex[level]` = 16, 47, 105, 204, ... 9,999,999 (the table entry used is `ex[E181]`, with E181 the 0-based level).

### 2.6 Boss / special attack rows, 7 bytes [V] (`$C0:45D6`)
`0-1` word -> obj+0x194/0x195, `2` accuracy (99), `3` power (atk = Str + power), `4-5` status word, `6` status chance. The Dark Lich uses id 0: power 35 (-> attack 74+35 = 109, as in `docs/dark-lich.md`), status 0x4000, chance 99%. The table has 115 rows (ids 0-0x72, `data/boss_attacks.json`); all 115 were loaded through the real routine and compared [V, `tools/validate_boss_attacks.py`]. Which boss uses which row: §15.

## 3. Hero stat build ([V]: `tools/hero_stats.py` reproduces Str/Agi/Con/Int/Wis, max HP/MP, accuracy, attack, evade, defense, magic evade, magic defense and the immunity word of all 16 hero objects found in six save states)
Order (`$C0:4530`):
1. base = hero table row `[hero][level]` (`$C0:464F`).
2. If the hero is NOT driven by a human controller (`$D9-$DB` low nibbles do not point at it): add a per-hero tweak byte `tbl[$CC7A+hero]` from the 16-byte table at `$C0:473F` (`35 04 04 17 32 00 00 23 32 00 00 23 35 04 04 17`): 2-bit fields add `[0,+1,+2,-1]` to Str, Agi, Con. With the usual `$CC7A=0` that is **+1 Str, +1 Agi, -1 Con for computer-controlled party members** [V]; the meaning of `$CC7A` is not determined.
3. Armor flags (`$C0:4B7D`): each of the three pieces: bit4 -> Str +5, bit3 -> Agi +5, bit2 -> Con +5, bit1 -> Int +5 [V]. (The routine tests `BIT $0040`, i.e. WRAM `$7E:0040`, for a "-2" variant; it was zero in every state; its meaning is not determined.)
4. Weapon stat codes (§2.2) added to Str/Agi/Con/Int/Wis.
5. If status bit `E190 & 8`: Agi -= Agi/16. Then Str..Wis clamped to 1..99.
6. Derived: `acc = min(99, agi//4 + weapon.hit)`, `atk = (str + weapon.power) & 255`, `crit base = 3*weaponLevel + weapon.crit` (halved when the weapon type is > 4: bow, boomerang, javelin), `eva = min(99, agi//4 + armor evade sum)`, `def = min(999, con + armor def sum)`, `mev = min(99, wis//4 + armor mev sum)`, `mdef = min(999, wis + armor mdef sum)`, immunity = OR of the three armor words | 0x0800, resist mask = OR of the three armor `[6]` bytes.
Equipment bonuses therefore combine **additively before the 99 clamp**; defense/evade values are plain sums.

## 4. Physical damage (`$C0:4F7B` dispatcher -> `$C0:4FED`, `505B`, `50CA`, `514B` [51E7, 52E7, 532C, 541B], `5119`)

Validation [V]: `tools/validate_phys.py` runs the real routines and `tools/phys_model.py` on 1,600 random attacker/defender pairs (monster ids 0-0x79, both directions, random buffs/statuses/gauge/stage/RNG state) on three save states: 0 mismatches. The ROM objects are real (heroes from the saves, monsters created by the game's own `$C0:55EC`).

### 4.1 Roles
`X` = defender (owner of the hit mask `E059`, bit i = attacker slot), `Y` = attacker. Hero defenders are hit by slots 3-5, monster defenders by slots 0-2 (`E1FB` bit 7 selects). Pending result goes to the defender's `E1F1`.

### 4.2 Hit test (`4FED`, `505B`) [V]
```
ev = D.E1A4;  ev += ev>>2 if D.E1B0&4; ev -= ev>>2 if D.E1B0&8;  ev = min(ev, 99)
acc = A.E197
if A.E190 & 4: acc -= acc>>4;  q = acc>>2        else:  q = A.E190 >> 2     # ROM quirk: A still holds E190 at $C0:5076
if A.E1B0 & 0x10: acc += q  elif A.E1B0 & 0x20: acc -= q ;   acc = min(acc, 99)
if A.E1ED != 0: acc >>= 1
hit  iff  acc >= (ev>>1) + ((R * (ev>>1)) >> 8)        # R = one RNG byte
```
So the "buff/debuff 25%" on accuracy only bites when the attacker has E190 bit 2 set, otherwise it adds `E190>>2` (normally 0). Hit probability for `ev` and `acc`: `T = ev//2 + floor(R*(ev//2)/256)`, e.g. acc 92 vs ev 99 -> T in [49,97], P(hit) = 230/256 = 89.8%.

### 4.3 Damage (`51E7`, `52E7`, `532C`, `541B`, `514B`) [V]
```
stage = A.E19B (+ $CC76 once per hit when the attacker is hero slot 0; $CC76 is the Mana Sword bonus, section 4.3a)
v = (A.E198 * (2*stage + 4)) >> 2            # integer, 16 bit
base = v;  v -= v>>2 if A.E1B0w & 0x80  elif  v += v>>2 if A.E1B0w & 0x40
n = A.E1AE & 15;  if n: v += base // (10 - n)           # n = Saber spell level (note: uses the pre-+-25% value)
v = ((v * m) & 0xFFFF) >> 1;  v = min(v, 999)            # m from §6: 1,2 or 4
if A.E191 & 2: v = 1                                     # Pygmized attackers always do 1
r = rnd(101)
if r >= acc:  v = min(999, acc * v // 100)               # acc is the value after 4.2 (incl. gauge halving)
else:         v = min(999, (E1E6*E1E6 + v + rnd((v + E1E6^2)>>4 & 255)) & 0xFFFF)
# defense (52E7)
df = D.E1A5 (+-25% from D.E1B0w bit0/bit1, //4 if D.E191&2, min 999)
r = rnd(100)
if r >= ev:   D' = min(999, ev * df // 100)
else:         D' = min(999, df + rnd((df>>4) & 255))
# crit / gauge (532C)
if A.E1AE & 0xF0:  A.E1AE -= 0x10; crit
elif A.E1ED == 0:  c = A.E196; if D.E190&0x10: c = 2c; if A is hero: c += 10 if attacker slot==1 else 5;  crit iff R < c & 255
else:  v = (v>>1) * (AC - A.E1ED) // AC,  AC = (99-Agi)//2 + 80       # no crit at all while the gauge is not empty
if crit: v *= 2
# flat guard (541B)
if D.E1B1 & 0x10: v -= (D.E1BF & 255); D' = 0
damage = v - D'  if v > D' else 0
```
- Maximum: every intermediate is capped at 999, but the final doubling for crits is applied after the cap; when the pending value is applied (`$C0:4004`) it is capped again to **999 per hit** [V, executed].
- Weapon damage has **no minimum**: `v <= D'` gives 0 damage (a "0" number is shown for monsters, see `docs/damage-counter.md`).
- The result is *not* divided by anything for several enemies; each contact is resolved separately.
- `E19B` (stage): `$C0:B330` raises it while the attack button is held: a counter 0..44 advances every second call (one call per frame), then stage++ up to the weapon level `E19C`; stage 0 = uncharged swing [V by execution]. Timing in seconds: §11.3 and §17. `E1ED` is loaded with `(100-Agi)/2 + 50` when a swing ENDS (`$C0:F937-F944`, `$C1:CACD`; `docs/glove-attacks.md` 2.4, not at its start; also when a run is released, `$C0:B77B-B78A`, `docs/hero-movement.md` 4.1) and decremented once per FRAME (`$C0:EB4F`, called from the per-frame `$C0:EB06`; Agi 50 -> 75 frames = 1.25 s, Agi 99 -> 50 frames = 0.83 s): a new swing is never blocked by the gauge (it only blocks a run and the start of a charge); a swing made before it reaches 0 deals `(AC-E1ED)/AC` of half damage and cannot crit, and accuracy is halved [V].

### 4.4 Worked examples (model, 20,000 trials, `tools/example_damage.py`) [V model]
- Lv66 boy, atk 135, acc 92 vs Dark Lich (eva 99, def 200), `E1E6 = 0`: stage 0 -> 90% hit, mean 9 damage; stage 8 -> 90% hit, mean ~577, median 492, max 1232 (capped to 999 when applied). With `E1E6 = 8` (the value in the late-game saves, §11.6): stage 0 mean 27, stage 8 mean 645, median 558, max 1368. With the gauge at 30 the hit chance is 0% (acc halves to 46 < 49).
- Lv1-like atk 17 vs eva 0/def 0: always hits, mean 17 (`E1E6 = 0`, the new-game value); with `E1E6 = 8` the same swing averages 79.

### 4.5 Status on hit (`5119`) [V]
After a damaging hit: if `A.E199 & D.E1AA == 0` (not immune), `rnd(100) < A.E1F7` and damage > 0, then `A.E199` is queued as the status to apply (`$9A`). Weapon rows carry the status word and chance (0x50 = 80% is the default chance byte, only rows with a status word matter).

### 4.6 Saber buffs [V]
Handlers `$C8:EAA3-EABC` (ids 3, 9, 0xF, 0x15, 0x1B, 0x27): `E195 |= element bit`, `E199 |= spell status word`, `E19D = (level+1)*4`, `E1AE low nibble = level`. The remaining-hit counter `E19D` is decremented once per hit by the attacker (`$C0:50EF`), the buff ends at 0. The *numerical* effect of a Saber is the `+ base/(10-n)` term (n = 1..9: +11% .. +100%).

### 4.7 Drain weapons [C]
Attacker `E195 & 0x40`: the defender takes the damage and the attacker gets the same amount as HP heal (`E1F3`), unless the defender has type bit `0x20`, in which case the roles are swapped.

### 4.3a The Mana Sword bonus `$CC76` [V 2026-10-06, C]
The stat build of hero 0 (`$C0:491F-4999`, run by `$C0:4530`) clears `$CC76`, then, only while both Mana Magic timers `$CC78` and `$CC79` are non-zero (`$C0:499A`) and hero 0 holds a sword (`$E1E4` = 1): `E195 = 0x80` and

```
$CC76 = (sum of the 8 weapon-level nibbles E1C0-E1C3 of hero 0) / 8 + (sum of the 8 bytes E1D0-E1D7 of hero 0) / 100
```

`$C0:51EB` adds `$CC76` to `E19B` (the charge stage) of every weapon hit of hero slot 0, so with the Mana Sword an uncharged swing hits like a stage-`$CC76` one. Measured with the real spells 0x23 and 0x20 cast on the boy (`.zs3`, weapon levels 6 5 8 5 7 7 6 6, sum 50): the weapon row became 17 and `$CC76` = 6 in the frame the second spell took effect; against the Mana Beast every hit then did 610-635 at every stage (mana-beast.md section 10).

## 5. Magic (`$C8:E6D4-E7A6` hit entry, `E7A7`, `E855`, `E8A0`, `E8C0`, `E990`, `E9DF`, handlers in `$C8:EA8B-EF..`)

Validation [V]: `tools/validate_magic.py` - damage handlers `EA8B/EB39/EBCA/EBE5` ids {0,1,6,7,0xC,0xD,0xE,0x10,0x11,0x12,0x13,0x1F,0x25,0x28} and the heal handler `EBB3` (id 0x0B), caster hero or monster, random levels/divisors/elements/defenses/RNG: 800 random cases on three states, 0 mismatches.

### 5.1 Casting
- MP cost = spell row byte 15, subtracted at cast time (`$C0:3F6C` compares `MP - cost >= 0`, the monster AI uses the same table at `$C1:1E41`) [C].
- Spell level `L` (`$D0:4E6E`, `$D0:4EAC`) [C, plus sim]: heroes: nibble of `E1C4..E1C7` for the spell's element index (`E177`), even index = high nibble; monsters: `E1C4` (record low nibble). The level actually stored in the cast (`E171`, used as `$9D` in the damage formula) is `L`, except for heroes and bosses (`E1FB & 0xC0`) with `L == 8`: `E171 = 8` only if `(R>>1) + 1 < progress`, else 7, where `progress = E1D8[element]` (0..99). Bosses/monsters have progress 0, so **boss level-8 spells run at level 7**: confirmed in the boss-engine simulation (the Lich's Dispel Magic cast wrote `E171 = 7`) [V sim].
- Progress: every spell (first hit of a cast, `$D0:4DCD`, heroes only) adds `9 - L` to `E1D8[elem]` (halved unless `$ED` bit7), at 100 the level goes up (nibble++), at level 8 the progress sticks at 99 [C].

### 5.2 Damage (E8C0 + E9DF) [V]
```
stat  = caster Int (spell id % 6 < 3) or Wis (>=3)                       # E18B / E18C
p     = spell.power (hero casters)  |  record byte 18 / 19 (monster casters)
acc   = min(99, ((stat>>2) + spell.accuracy) & 255)                        # 8 bit add
v     = min(999, ((p + stat) & 255) * (L + 2) >> 1)                       # (p+stat) wraps at 256 (power 250 spells)
r = rnd(101);  v = acc*v//100 if r >= acc  else  (E1E6^2 + v) + rnd(...)  # same as physical
v = ((v * m) & 0xFFFF) >> 1;  v //= divisor (obj E176, 1 for single hits);  v = min(v, 999)
D' (E990): r = rnd(101); if r < target.mev:  D' = min(999, mdef + rnd(mdef>>4))
                         else:               D' = mev * (mdef + (mdef>>2)) // 100
damage = max(1, v - D')                                                    # magic always does at least 1
```
Spells never "miss": `acc` only scales damage. Element multiplier `m` (§6) uses the spell's element mask against the target's `E1A3` (resist -> 1) and `E1A1` (weak -> 4).

### 5.3 Healing [V]
Cure Water (id 0x0B, handler `EBB3`): same computation with `m = 4`, target defense unused, result added to `E1F3` (HP heal), applied capped at 999 and at max HP. At `L=0`: `Wis 74, p 61 -> (135*2/2)*4/2 = 270`; at L=8: 999 (cap). Other restorative handlers (`EB9F`, `ED04`, `ECD1`) are not modelled.

### 5.4 Item/regeneration application (`$C0:4004`) [V executed]
Pending heal is applied before pending damage (one of them per tick): heal: `E1F3` capped 999, `HP = min(maxHP, HP + heal)`; damage: `E1F1` capped 999, `HP = max(0, HP - dmg)`; MP heal `E1F6` capped 99 and to max MP; MP damage `E1F5` capped 99 floors at 0.

### 5.5 Spell table (power/accuracy/MP from the ROM; "class" from the handler code, [C] unless in the validated list; handlers that were not read in detail are marked "not modelled")
| id | name | element mask | power | MP | status word | handler | class |
|---|---|---|---|---|---|---|---|
| 00 | Earth Slide | 0x01 | 61 | 3 | - | EA8B | damage [V] |
| 01 | Gem Missile | 0x01 | 43 | 2 | - | EA8B | damage [V] |
| 02 | Speed Down | 0x01 | 61 | 1 | 0004 | EA97 | damage + status |
| 03 | Stone Saber | 0x01 | 61 | 4 | 0040 | EAA3 | saber |
| 04 | Speed Up | 0x01 | 61 | 3 | - | EB04 | buff: EVA+25% & ACC+25% |
| 05 | Defender | 0x01 | 61 | 2 | - | EB20 | buff: DEF+25% |
| 06 | Freeze | 0x04 | 61 | 2 | - | EA8B | damage [V] |
| 07 | Acid Storm | 0x04 | 43 | 3 | 2000 | EB39 | damage [V] + DEF-25% |
| 08 | Energy Absorb | 0x04 | 43 | 2 | - | EB54 | not modelled |
| 09 | Ice Saber | 0x04 | 61 | 2 | 0020 | EAA8 | saber |
| 0A | Remedy | 0x04 | 61 | 1 | - | EB9F | not modelled |
| 0B | Cure Water | 0x04 | 61 | 2 | - | EBB3 | heal [V] |
| 0C | Fireball | 0x08 | 52 | 2 | - | EA8B | damage [V] |
| 0D | Exploder | 0x08 | 61 | 4 | - | EA8B | damage [V] |
| 0E | Lava Wave | 0x08 | 43 | 3 | - | EA8B | damage [V] |
| 0F | Flame Saber | 0x08 | 61 | 2 | 4000 | EAAD | saber |
| 10 | Fire Bouquet | 0x08 | 43 | 3 | - | EBCA | damage [V] + ATK-25% |
| 11 | Blaze Wall | 0x08 | 32 | 4 | 4000 | EBE5 | damage [V] + status |
| 12 | Air Blast | 0x02 | 43 | 2 | - | EA8B | damage [V] |
| 13 | Thunderbolt | 0x02 | 61 | 4 | - | EA8B | damage [V] |
| 14 | Silence | 0x02 | 61 | 2 | 0080 | EBF4 | damage + status |
| 15 | Thunder Saber | 0x02 | 61 | 3 | - | EAB2 | saber |
| 16 | Balloon | 0x02 | 61 | 2 | 0100 | EBF4 | damage + status |
| 17 | Analyzer | 0x02 | 61 | 1 | - | EC00 | not modelled |
| 18 | Change Form | 0x40 | 61 | 5 | 0800 | EBF4 | damage + status |
| 19 | Magic Absorb | 0x40 | 43 | 1 | - | EC3B | not modelled |
| 1A | Lunar Magic | 0x40 | 61 | 8 | - | EEF5 | not modelled |
| 1B | Moon Saber | 0x40 | 61 | 3 | - | EABC | saber |
| 1C | Lunar Boost | 0x40 | 61 | 2 | - | ECB5 | not modelled |
| 1D | Moon Energy | 0x40 | 61 | 2 | - | ECD1 | not modelled |
| 1E | Sleep Flower | 0x80 | 61 | 2 | 0010 | EBF4 | damage + status |
| 1F | Burst | 0x80 | 100 | 4 | - | EA8B | damage [V] |
| 20 | (see `names.py`) | 0x80 | 250 | 1 | - | ECEA | not modelled |
| 21 | Revivifier | 0x80 | 61 | 10 | 8000 | ED04 | not modelled |
| 22 | Wall | 0x80 | 61 | 6 | - | ED53 | not modelled |
| 23 | (see `names.py`) | 0x80 | 250 | 1 | - | ED68 | not modelled |
| 24 | Evil Gate | 0x10 | 61 | 8 | - | ED82 | not modelled |
| 25 | Dark Force | 0x10 | 61 | 2 | - | EA8B | damage [V] |
| 26 | Dispel Magic | 0x10 | 61 | 4 | - | EDCB | not modelled |
| 27 | Light Saber | 0x20 | 61 | 5 | - | EAB7 | saber |
| 28 | Lucent Beam | 0x20 | 61 | 8 | - | EA8B | damage [V] |
| 29 | Lucid Barrier | 0x20 | 61 | 4 | - | EEB7 | not modelled |

Accuracy base is 75 for every spell. Names are as decoded from the ROM text block at run time by `tools/names.py`.

## 6. Elements [V for the spell path, see quirk for weapons]
Bits (spell rows, monster element byte, armor resist byte): `0x01 .. 0x80`. Only the six low bits have opposites in the weak-mask formula below: 0x01<->0x02, 0x04<->0x08, 0x10<->0x20 (0x40 and 0x80 have none).
- Resist mask `E1A3` = monster record byte 15 (or the OR of the armor resist bytes); weak mask `E1A1 = ((m<<1)&0x2A) | ((m>>1)&0x15)` masked to 6 bits: **a creature with element bit X is weak to the opposite bit** (Dark Lich: element byte 0x10 -> resists 0x10, weak to 0x20).
- Multiplier for spells: `m = 1` if `spell.element & resist`, `m = 4` if `& weak`, else 2; the damage formula uses `v*m/2`, so it is **x0.5 / x1 / x2**. The check order is resist first.
- Quirk [V, `$C0:50CA`]: for physical hits the routine tests the word `E194/E195` of the SAME object as `E1A2/E1A0` (the defender's own, since `X` is the defender), the only live byte pair being `E195 & E1A3` / `E1A1`. No code path was found that copies the attacker's weapon element or its Saber bits into the defender before the test (searched all writers of `E194/E195`). Practical consequence in vanilla code: physical hits against monsters always have `m = 2`; the weapon "enemy type" byte and the Saber element bits have no effect on weapon damage by themselves [V as code]. Related finding [V as code]: `$C0:4A53` loads the monster type byte (`LDA E192,X`) and then stores ZERO into `E1A2` (`STZ`), so the low byte of the `E1A2` mask - the one the weapon enemy-type byte `E194` would be tested against - is always 0. As a result the byte-5 test cannot trigger in the code paths read [V as code]. Whether another code path handles it was not found.

## 7. Status effects
- Application (`$C8:E301`, via `E88E` after a spell, or `5119` for weapons) [C+V partly]: `new = status & ~target.E1AA` (immunity word: monster record bytes 20-21, hero armor words | 0x0800); no random roll for spells (spells always inflict unless immune); for weapons the roll is §4.5.
- Spell status words by spell id: 02 -> 0004, 1E -> 0010, 09 -> 0020, 03 -> 0040, 14 -> 0080, 16 -> 0100, 18 -> 0800, 07 -> 2000, 0F and 11 -> 4000, 21 -> 8000 (bit 0x8000 is cleared by that handler). Boss attacks: `data/boss_attacks.json` (statuses 0004, 0010, 0020, 0040, 0100, 0200, 1000, 2000, 4000 with chances 33/50/80/99%).
- Duration (`$C8:E3A9`): `d = max(20, ($96>>2) - (target.mdef>>1))` where `$96` is the damage value *before* defense of the hit; stored in the timer of the status group: bits `0x007C -> E1B2`, `0x0180 -> E1B3`, `0x1C00 -> E1B5`, `0x6000 -> E1B6` (group bits all end together when the timer reaches 0). Executed and validated in §12 (timers drop every 4th combat tick = 12 ticks/s / 4, so d = 20 lasts 6.4 s). Bit names, cascade, immunity, damage over time: §12.
- Effects found in the code [V/C]: `E190&4` accuracy -1/16; `E190&8` Agi -1/16; `E191&2`: own damage = 1 and Def/4; `E191&4`: no hits resolved; `E190&0x10` (target) doubles the target's crit exposure; bits `E191 & 0x60` deal 1 HP per tick (never kills: stops at 1 HP) [C].
- Item id 4 clears `E190` and keeps only bit 7 of `E191`; item id 5 also clears bit 7 [V executed, §9].
- The sources (spells, items, attack rows) of every status bit: §12.1.

## 8. Rewards, level-up and growth
- Kill reward (`$C0:4459`, `$C0:44F0`) [V executed, `tools/validate_exp.py`, 60 cases]: for every hero that is alive (HP>0): `exp += monster.exp` (24-bit, cap 9,999,999). **Full EXP to each living hero** (no splitting). While `level < 98` and `exp >= ex[level]`: `level++`, stats rebuilt from the hero table (§3), HP set to max HP. Level cap 99 (index 98). Gold: `+= monster.gold`, cap 9,999,999 (`$C0:39A4`).
- Stat growth per level: none computed; the whole curve is the table in `data/hero_levels.json` (HP, MP, Str, Agi, Con, Int, Wis per level and hero). Equipment adds on top (§3).
- Weapon level: fully specified and validated in §11.2 (progress `+= 9 - level of the killer's weapon`, halved against lower-level monsters, the other living heroes get half, threshold 99, cap `CFB0[type] + 1` and maximum 8; kills by spells give none).
- Magic level: §11.5 (validated); the old text in §5.1 about the progress remains correct.

## 9. Consumables [V executed on a save state, `$C0:54E7`] (full table with dead-hero behaviour: §16)
Item ids 0-11: id 0 adds 100 HP (`0x64`), id 1 adds 250 HP, id 2 sets HP to max (`E1F3 = max`), id 3 adds 50 MP (via `E1F6`), id 4 clears the status words (timers reset to 1, `E190 = 0`), id 5 sets HP to max and clears the KO bit, id 8 toggles `E191` bit 4, id 9 toggles bit 1, id 10 toggles bit 2; ids 6 and 7 have no combat effect in this routine. Prices: the price tables are in `data/prices.json` (consumable price word table at `0x18FB9C`, `0xFFFF` = not sold); which item id each price entry belongs to was not verified (see §18).

## 10. Scope
This document describes only what the ROM code and tables do; every formula was executed or read in the ROM as tagged.

## 11. Weapon level, weapon grade (orbs), charge gauge, magic level

Validation: `tools/weapon_level.py` (3,000 random kills incl. dead heroes, caps and the `$A6` quirk: 0 mismatches), `tools/validate_magic_level.py` (1,500 random casts: 0 mismatches), `tools/charge_timing.py` (real `$C0:B330`).

### 11.1 Storage [V]
| where | meaning |
|---|---|
| `E1C0-E1C3` (hero object) | weapon level of the 8 weapon types, 4 bits each: type `t` = byte `t>>1`, HIGH nibble for even `t`, low nibble for odd `t`. Range 0..8 |
| `E1D0 + t` | weapon-level progress of type `t`, 0..98 (99 is only reachable at level 8) |
| `E1E4` | weapon type of the equipped weapon; `E1E8` = equipped weapon row id = `type*9 + grade` (checked in 3 saves) |
| `E19C` | level of the equipped type, rebuilt from the nibble by `$C0:48F0` after every level-up and equipment change |
| `E19B` | current charge stage of the swing (0..`E19C`) |
| `E1C4-E1C7`, `E1D8 + e` | magic level nibbles (same packing) and progress 0..99 per element index `e` = `E177` of the cast, 0..7 (`E177` = 1 for spell ids 06 and 0B in a save); the mapping of every index to an element mask is not determined |
| `$CFB0 + t` | **weapon grade** of type `t` (0..8) = index of the owned weapon inside its 9-row group; level cap is derived from it (11.4) |
| `$CFB8 + t` | orbs found for type `t` (0..8). The upgrade shop needs `CFB0 < CFB8` |
| `$CFC0 + t` | 9 for every type except sword (8) in all saves; an orb drop of type `t` is only kept while `CFB8[t] + 1 < CFC0[t]` (11.4). Initial value and writer not found |
| `$CC54 + t` | shop/inventory slot of the weapon of type `t`: bits 0-5 = grade (mirrors `CFB0`), bits 6-7 = equipped-by flags |
| `$CC7D + hero` | player-chosen charge level for the AI-controlled hero (11.3); set from `E19C` by `$C0:7A4A` and clamped to the weapon level in the menu (`$C7:6C18`) |

### 11.2 Weapon level-up [V] (`$C0:4358`, per-hero part `$C0:43B7`)
Triggered when a monster dies (`$C0:4247-4255`, right before the EXP and gold routines) and its `E1F0` is 0..2. `E1F0` = index of the hero whose WEAPON hit landed last (`$C0:4FD2`); every spell hit overwrites it with 0xFF (`$C8:E785`), so **a kill by a spell gives EXP/gold but no weapon progress**. Boss kills run the same code.
```
a6 = 9 - E19C(killer)                       # killer's CURRENT weapon level, not the other heroes' levels
if monster.level < killer.level: a6 >>= 1   # lower-level monsters give half
killer gets a6; then a6 >>= 1; the other two heroes get that (each on its OWN equipped weapon type)
for each hero that exists and is not dead (E191 bit 7 clear):
    t = E1E4;  wl = nibble(E1C0.., t);  cap = CFB0[t] + 1
    if wl == cap:             progress[t] = 0                  # frozen at the cap, progress is wiped
    else:                     p = progress[t] + a6             # byte add
         if p < 99:           progress[t] = p
         elif wl == 8:        progress[t] = 99                 # maximum level: sticks at 99
         else:                progress[t] = 0; nibble += 1     # LEVEL UP
```
The threshold is 99 for all nine levels (`$C0:43AC`, `data/weapon_levels.json`). ROM quirk reproduced by the model: the level-up message code leaves the NEW level (0..8, one digit) in the scratch byte `$A6`, so every hero processed after a level-up within the same kill gets `new_level` (already halved once if the level-up was the killer's) instead of `9 - wl`.
Kills needed per level: `ceil(99 / (9 - wl))` (x2 against lower-level monsters): level 0 -> 11, level 4 -> 20, level 7 -> 50, level 8 -> none.
A level-up rebuilds the stats (`$C0:4530`) and plays the sound/message `$CE`.

### 11.3 Charge gauge and stage [V executed]
`$C0:B330` is called once per frame for every hero whose attack button is held (heroes with `E02C != 0`, which was 1 for the controlled hero in the saves; stage cap = `E19C`) and for AI-controlled heroes (`E02C == 0`, cap = `$CC7D + hero`, 0 = never charge). It does nothing while the recharge gauge `E1ED`, `E01C`, `E061` or the pygmy bit (`E191 & 2`) are non-zero.
The gauge word `E01A` = (stage << 8) | counter: it acts on every 2nd call (`E01F`), counter 0..44 (0x2C); the act after counter 44 sets stage+1 and counter 0, until stage == cap (then counter parks at 44). So **one stage = 45 acts = 90 frames = 1.50 s** (90 / 60.0988 = 1.4976 s), and stage `s` is reached on call `90*s - 1`:

| stage | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
|---|---|---|---|---|---|---|---|---|
| frames | 89 | 179 | 269 | 359 | 449 | 539 | 629 | 719 |
| seconds @ 60.0988 Hz | 1.48 | 2.98 | 4.47 | 5.97 | 7.47 | 8.97 | 10.47 | 11.96 |

Damage uses `v = atk * (2*stage + 4) / 4` (§4.3).

### 11.4 Grade, orbs, level cap [C + checked against saves]
- **Level cap (0-based nibble) = `CFB0[type] + 1`, absolute maximum 8.** With grade 0 (first weapon of the type) the level can reach 1, with grade 7 it can reach 8; grade 8 changes nothing more. Seen in saves: `CFB0 = 7` and a hero with level nibble 8 on that type.
- The grade is raised by the upgrade shop (`$C0:7B9B-7C49`): allowed when `CFB0[t] < 8` and `CFB0[t] < CFB8[t]`; price = `orb_price[CFB0[t]]` = 200, 400, 800, 1600, 3000, 6500, 12500, 25000 gold (word table `D8:FCFB`, `data/weapon_levels.json` / `data/prices.json`; 65535 at grade 8). On purchase: `CFB0[t]++`, the slot byte `CC54+t` is incremented and the equipped weapon row of the heroes flagged in bits 6-7 is recomputed (`$C0:6481`). The purchase routine was read, not executed [C]. The smith's menu is buy mode 3: event command 0x0F (`JSL $C0:004E` -> `$C0:6653`, `$1847 = 3`) and the purchase dispatch `$C0:7AC0` (`JMP ($7ACB,X)`: mode 1 buy `$7AD1`, mode 2 sell `$7B17`, mode 3 upgrade `$7B95`). The weapon a hero swings is row `t * 9 + CFB0[t]`, so each grade has its own row (power, hit, crit, stat codes, status) and its own name.
- Orbs are found as monster drops (drop byte `0x80 + t`, §13) and chest/event items; `CFB8[t]` is the count found. Where `CFB8` is incremented on pickup (class-8 chest object, `$C8:E2F6` -> object action) was **not** traced.
- Orb drop gate (`$C8:E199`, executed in §13): a drop of type `t` becomes a plain consumable id 0 when `CFB8[t] + 1 >= CFC0[t]`.

### 11.5 Magic level [V] (`$D0:4DCD`, called on the first hit of every hero spell)
```
L = level nibble of the spell's element, prog = E1D8[e]            # e = E177
cap = $CFFC; if cap == 8: cap = 9
if L >= cap:       nothing
else:              gain = 9 - L;  if $ED bit 7 clear: gain >>= 1   # $ED bit 7 is set in play (all saves), so 9 - L
                   p = prog + gain
                   if p < 100:      prog = p
                   elif L == 8:     prog = 99
                   else:            prog = 0; L += 1                # level up (message + sound)
```
So spells only level while `L < CFFC` (CFFC = 8 means no cap); with `CFFC = 0` (new game) magic never levels. Casts needed (first hits): `ceil(100 / (9 - L))` per level: 12 at level 0, 20 at level 4, 50 at level 7; level 8 never advances. The cast itself uses `L` (and 7 instead of 8 for heroes/bosses with the 50% roll of §5.1).

### 11.6 `$CFFC` / `E1E6` [V]
`E1E6` of every hero is loaded from `$CFFC` at object init (`$C0:4850`), of every monster from `$CFFB` (`$C0:4819`). New game sets `$CFFC = 0` (`$C0:5787`); all six save states have `$CFFC = 8`, `$CFFB = 0`. `E1E6` enters the damage of every non-glancing physical or magic hit as `+ E1E6^2` (and in the `rnd` bound): **+64 for heroes with `$CFFC = 8`**, 0 for monsters. This replaces the earlier statement that it is 0 in normal play; the model and validators already take `E1E6` from the objects. What raises `$CFFC` during the story is not determined (no routine examined writes it after the new-game init).

### 11.7 Display rules [C]
The level-up message prints the nibble itself (weapon level 0..8, spell level 0..8; no +1): the scratch digit left in `$A6` equals the new nibble. The upgrade screen (`$C7:6461`) prints `CFB0+1` and `CFB8+1`. No routine that draws progress from `E1D0`/`E1D8` was found; progress is 0..98/99 as stored.

## 12. Status effects

Validation: `tools/validate_status_rules.py` runs the real apply routine `$C8:E301` on 1,500 random (current status, new status, immunity, damage, magic defense) tuples and compares the resulting status word and the four timers with the model below: 0 mismatches. `tools/status_timing.py` runs the real combat tick `$C0:3A79` until each status clears. Data: `data/status_effects.json`.

### 12.1 The status word and its bits
`word = E191 << 8 | E190` (16 bit). Bits are identified by mask only; the sources are rows of the spell, weapon, attack and item tables.

| mask | evidence / effect | timer |
|---|---|---|
| 0x0001, 0x0002 | `E190 & 3`: dying/removal countdown `E1B4`; a chest object sets bit 0 when opened. Not part of the status cascade; item id 4 clears the whole low byte, so it clears them too | `E1B4` |
| 0x0004 | set by spell 02, player weapons 37/42/56, boss rows 23/75/76 [V]; effect found: attacker accuracy -1/16 in the hit test (§4.2) | `E1B2` |
| 0x0008 | no spell/attack sets it; the stat build subtracts `Agi/16` while it is set | `E1B2` |
| 0x0010 | set by spell 1E [V]; cleared by any damaging hit (`$C0:5119`); doubles the crit chance against the target (`$C0:533D`) | `E1B2` |
| 0x0020 | set by spell 09, boss rows 1/14/55/86/107; cannot act (mask `C160`) | `E1B2` |
| 0x0040 | set by spell 03, boss rows 7/15/58; cannot act; applying it halves HP (`HP = HP/2 + 1`, `$C8:E491`) | `E1B2` |
| 0x0080 | set by spell 14, player weapons 31/48/51, boss row 71 [V]; effect not determined | `E1B3` |
| 0x0100 | set by spell 16, boss rows 18/73 [V]; cannot act; sprite size by spell level (<4, <8, 8) | `E1B3` |
| 0x0200 | toggled by item id 9, set by monster/boss attacks [V]; own damage forced to 1, defense /4 | none (until cleared) |
| 0x0400 | toggled by item id 10 [V]; no hit is resolved on the actor (`$C0:4F7B`) | none |
| 0x0800 | set by spell 18 [V]; monsters become one of 8 random sprites (`$C8:E5F3` table); heroes are always immune (their immunity word always includes 0x0800) | `E1B5` |
| 0x1000 | toggled by item id 8; set by monsters and boss rows 16/21/24/47-49; sprite offset +0x70 | `E1B5` |
| 0x2000 | set by spell 07, monster attacks (25-80%), boss rows 4/9/30/31/64/77/84/98 [V]; 1 HP per timer drop | `E1B6` |
| 0x4000 | set by spells 0F and 11, monster attacks, boss rows 0/11/53/54/85/108 [V]; same drain as 0x2000; cannot act (`$C0:BD46` mask `C160`); removes 0x2000 | `E1B6` |
| 0x8000 | KO flag; clears every other bit; removed by spell 21 and item id 5 | - |

### 12.2 Applying a status (`$C8:E301`, model in `validate_status_rules.py`)
```
new  = add & ~immunity                      # immunity = E1AA: monsters = record bytes 20-21, heroes = OR of the 3 armor words | 0x0800
w    = (current | new) & ~immunity          # the target also LOSES any current bit it is immune to
if w & 0x8000: w &= 0x8000                  # dead overrides everything
cascade (each rule only looks at the bits left by the previous one):
  0x0040 -> w &= 0x9AC0 ; HP = HP/2+1      0x0020 -> w &= 0x9AE0     0x0010 -> w &= 0xFBF0 (+E1B1 bit0)
  0x0008 -> w &= 0xFBF0                     0x0004 -> w &= 0xFFFC
  0x1000 -> w &= 0x92FF
  0x0800 -> w &= 0xF7FF if w & 0x0260 else w &= 0x9A13
  0x0400 -> w &= 0xFBFF if w & 0x617B
  0x4000 -> w &= 0xDAFF                     0x2000 -> w &= 0xDFFF if w & 0x1860
```
So the low-byte bits are mutually exclusive in the order 0x0040 > 0x0020 > 0x0010 > 0x0008 > 0x0004; bit 0x1000 wipes `0x6D00`; bit 0x4000 wipes `0x2500`; bits 0x0040, 0x0020, 0x0800 and 0x1000 remove 0x2000 (`0x1860`).
A timer is (re)loaded only for bits that were not already set: `E1B2` for `0x007C`, `E1B3` for `0x0180`, `E1B5` when `0x1C00` is new (only `0x1800` is counted down), `E1B6` for `0x6000`. The same cast re-applied does not refresh a running timer.

**Duration** `d = max(20, low byte of ((V >> 2) - (magic_defense >> 1)))`, 20 when the difference is negative, where `V` is the damage value before defense of the hit (`$96`; weapons: the capped pre-defense damage, hero spells: the spell power byte, monster spells: record byte 18/19). Spells (power 61): always 20. Weapon hits: `V >> 2` is up to 249 (damage 999), so strong hits give longer statuses (e.g. damage 300 and mdef 0: 75 timer units).
Timer drop: once every 4th combat tick (first drop on tick 1): lifetime `4d - 3` ticks (77 ticks for d = 20) [V executed]. At 12.02 ticks/s that is **6.4 s** for d = 20 (ordinary actors; bosses of the C2 engine with flags high byte `&3 == 0` tick at the same 12.02 Hz, other flag classes at `$56` = 1 or 2: §1.3).
Buff/debuff bytes `E1B0` (DEF/EVA/ACC/ATK +-25%, §1.1) use the same cadence: DEF `E1BC`, EVA `E1BB`, ACC `E1BA`, ATK `E1B7`, loaded with the same `d` by the buff handlers (`$C8:EB04`, EB20, EB39, EBCA).

### 12.3 Damage over time [V executed]
Status bits 0x2000 and 0x4000 are the same code (`E191 & 0x60`, `$C0:3D5F`): on every timer drop the actor loses **1 HP** (sound `0x39`) and the status is removed instead when the HP would reach 0 (so it never kills; stops at 1 HP). 3 HP/s at 12 ticks/s (1 HP per 4 ticks); at most `d` HP per application (19 for d = 20). Heroes and monsters alike.

### 12.4 Which hits inflict a status
- Weapon hit (`$C0:5119`): if `attacker.E199 & defender.E1AA != 0` nothing happens (ONE shared bit blocks the whole word); otherwise with `rnd(100) < E1F7` and pending damage > 0 the word `E199` is queued. Hero weapons: 80% for all 72 rows (99% for row 17); a Saber spell ORs its status word into `E199` for its hit counter (`E19D = (level+1)*4` hits), so every Saber hit has the weapon's chance (80%, 99% for row 17). Monster attack rows: 25/50/60/80/99% per row (`data/weapons.json` ids 72+), boss rows: `data/boss_attacks.json`.
- Spell hit: no roll; the status word of the spell row is applied bitwise through the immunity mask on the first hit of the cast (§5). Statuses by spell: `data/spells.json` / §7.
- Immunity words: monsters `data/monsters.json` `status_immunity` (41 records are immune to everything but bits 0-1: 0x7FFC), heroes `data/armors.json` `status_immunity` (accessory rows 43, 46, 59, 60...).

## 13. Monster drops

Everything here was executed: `tools/validate_drops.py` runs the real death routine `$C0:4203` on 3,360 (monster id, RNG state) pairs, the real chest-content routine `$C8:E12C` on 840 pairs (with random orb counters) and compares with the model: 0 mismatches. Data: `data/drops.json` (one entry per monster id 0-0x56 with a drop row: raw bytes, decoded fields and the outcome probabilities).

### 13.1 Row format (5 bytes at `D0:3A50 + monster_id*5`, `0xFF` row = never drops)
| byte | meaning |
|---|---|
| 0 | bits 0-5 `chance`: a chest appears when `chance >= (rnd_byte & 63) + 1`, i.e. probability `chance / 64`; bit 6: always (skip the roll); bit 7: the chest object gets 255 HP instead of 0 (`$C0:42A8`), purpose not determined (set for 32 monsters, levels 6-69) |
| 1 | high nibble `n`: trap threshold `t = n * 8` (`(b & 0xF0) >> 1`); low nibble: not read by any routine examined |
| 2 | bits 3-5 `a` (0..7): content roll threshold; bits 6-7 `mode` (below); bits 0-2 unused |
| 3, 4 | the two possible contents, byte 3 when the content roll `r = rnd_byte & 63` satisfies `r >= a` (prob. `(64-a)/64`), byte 4 otherwise |

`mode` decides what each byte is: 0 = gold, gold; 1 = item, item; 2 = byte 3 gold, byte 4 item; 3 = byte 3 item, byte 4 gold.
- **Gold**: the byte is the amount added to the party gold, 1..255 (it is about twice the monster's kill gold, e.g. 14 vs 7).
- **Item byte** (same encoding as the inventory-add routine `$C0:6420`): `0x00-0x14` head armor row, `0x15-0x29` body, `0x2A-0x3E` accessory (so the byte is the armor row id of `data/armors.json`), `0x40-0x4B` consumable id (byte - 0x40, see §16), `0x80-0x87` weapon orb of type byte-0x80. An orb drop is replaced by consumable byte 0x40 (id 0) when `CFB8[t] + 1 >= CFC0[t]` (§11.4), otherwise the chest object becomes an orb pickup (class 8).

### 13.2 What happens on death (`$C0:4203`)
1. Only ordinary monsters roll (not heroes `E1FB & 0x80`, not bosses `& 0x40`, not object ids 0x20/0x2E/0x3F marked `& 0x01`). EXP, gold (`E1C8` = record bytes 27-28, added immediately) and weapon progress are paid first (§8, §11.2).
2. Chest roll: `chance >= (rnd & 63) + 1`, or bit 6. No chest -> nothing.
3. Chest class (the trap kind, `E1C8` of the chest object) from the monster level `L`: `L < 10`: index = rnd(3) (0..2); `L >= 10`: 2; `L >= 15`: 3; `L >= 25`: 4; `L >= 30`: 5; `L >= 36`: 6, and 1/8 (`rnd < 0x20`) 7; the class is `[2,3,4,1,5,7,6,0][index]` (`data/misc.json` `kill_item_class_table`). So L < 10: classes 2, 3 or 4; 10-14: 4; 15-24: 1; 25-29: 5; 30-35: 7; >= 36: 6 (0 with probability 1/8).
4. The monster object becomes the chest: level 0, max HP 0, `E1B1 = 0x80`.

### 13.3 Opening the chest (`$C8:E0B0`, contents `$C8:E12C`) [V for contents, C for the rest]
- A chest dropped by a monster (max HP 0) may be a **trap**: when `t >= opener.Agi` (t from byte 1) the opening hero triggers it with probability 1/2 (RNG bit 0 clear) and receives no loot; otherwise the loot is paid (no check when Agi > t).
- Trap effect by class (`$C8:E259`): 0 and 7 none; 1 one status chosen at random from {0x0200, 0x1000, 0x0010}; 2 and 4 HP damage `((rnd & 7) + hero_level_index + 1) & 255`; 3 sets bit 0x2000 (no timer set); 5 sets bit 0x0040; 6 with 1/8 HP damage equal to the hero's current HP, otherwise HP set to 1; 8 spawns the orb pickup. [C]
- Loot: roll `r = rnd & 63`; pick byte 3 or byte 4 per 13.1; gold is added at once, items go through `$C0:6420` (inventory-add; with a message). A full inventory leaves `CF0F` non-zero and shows the other message variant.

### 13.4 Summary of the drop table
84 monster ids have a row (0-0x53); ids 0x54-0x56 and the bosses have `FF` rows (bosses give no chest). Chest probability is 4/64 = 6.25% for 77 monsters, 8/64 = 12.5% for id 0, and 0 for ids 5, 32, 44, 46, 63 and 79 (no row uses the "always" bit). Row 0, for example: 12.5% chest, content 89% consumable id 0 / 11% 4 gold. All rows: `data/drops.json`.

## 14. Area / multi-target spells: the damage divisor (`E176`)

**There is no per-spell constant.** `E176` of the caster is the number of valid targets of THIS cast (1..3) and is the final divisor of the damage and heal formulas (§5.2, `v //= E176` at `$C8:E969`; the handler is skipped altogether when `E176 == 0`, `$C8:E7DD`). It is written when the cast starts, together with the target list `E178-E17A` (target slot + 1, 0 = empty) and the "already hit" list `E172-E174` (a target is hit at most once per cast, `$C8:E739`). Cross-checked on a save state: Cure Water with `E176 = 3`, `E178..E17A = 1,2,3`, hit list `0,1,2`; Freeze with `E176 = 1`, one target.

| caster | how the count is made |
|---|---|
| hero, hostile spell (spell row byte 8 = `0x81`, 24 spells) | target list is built automatically by `$D0:DA60` from the 3 monster slots: valid = object present, on screen (`8 <= x < 0xF8`, `y < 0xD8`), alive (`E191` bit 7 clear) or object id 0x54/0x55; the count of valid targets (1..3) is stored in `E006` by `$C0:7364` and becomes `E176` |
| hero, ally spell (byte 8 = `0x01`, 14 spells) | same list built over the 3 heroes; the player's target choice (`1818`: one or all) gives 1 or the number of valid allies; `0x40` (2 spells, ids 20 and 23), `0xA0` (Analyzer, hostile + object test), `0x00` (Moon Energy, self) |
| monster, AI bytecode (`$C1:1E47` single / `$C1:1E98` area) | single: `E176 = 1`, target = the one stored in `E1AD`; area: `E176` = number of valid opposing actors found by the scan (1..3), 0 = cast fails |
| boss state script op `0x0E` (`$C2:396D`) | parameter bit 8 = area: `E176` = number of alive actors on the target side (1..3); clear: `E176 = 1` at the stored target; bit 9 = target side is the monsters (own side buffs/heals). Per-boss lists: `data/boss_records.json` |

So an area spell does `damage / n_targets` per target (rounded down after the element multiplier, before the defense roll): with 3 valid targets each takes one third; with one target the full damage. The same holds for the party-wide healing of Cure Water (`/3` with three allies). Spell row byte 8 per spell: `data/spells.json` `target_flags`.
Spells whose damage uses this divisor: every row with `0x81` (ids 00 01 02 06 07 08 0C 0D 0E 10 11 12 13 14 16 18 19 1A 1E 1F 24 25 26 28) and spells 0A and 0B when several allies are targeted.
Not determined: the exact in-engine hero code that copies the list count into `E176` (observed in a save and in the targeting code, not executed end to end); whether the 3-target cap leaves a 4th monster untouched (only 3 monster slots exist, so it cannot occur).

## 15. Boss attack rows, boss records, boss id 0x7F

Tools: `tools/boss_records.py` (static decode of the 15 native boss records and their state scripts), `tools/boss_sim.py` (spawns a boss with the game's own spawner `$C2:0000` on the map-246 save state and steps the real engine; the attack-row loader call `JSL $C0:006C` is intercepted and logged; no hero is hit), `tools/validate_boss_attacks.py`. Data: `data/boss_records.json`, `data/boss_attacks.json` (115 rows).

### 15.1 How a boss picks the attack numbers [V]
- A boss never uses the weapon table. `JSL $C0:006C` (= `$C0:45D6`) takes a row id, stores it in `E1E3`, and copies row `D0:BDC1 + id*7` into the object: `E194` = word 0, `E197` = accuracy, `E198` = Str + power (8 bit), `E199` = status word, `E1F7` = status chance. All 115 rows were loaded through the real routine and compared with `data/boss_attacks.json` (0 mismatches).
- The row is set by state-script op `0x13` (named `C0_006C` in `tools/statescript.py`; op `0x0C` (`SET_E1E3` in `statescript.py`) only stores the row id in `E1E3` without loading it and is unused by the 15 records) and by native handlers (`LDA #id / JSL $C0:006C`; 17 call sites in bank C2 besides the op handler `$C2:3947`).
- A boss record whose weapon bytes (23, 24) are 0 starts with row 0 (power 35, status 0x4000 at 99%) until a state script changes it; all 41 boss records have 0 there.
- Which rows each of the 15 native boss records uses (static, from the state scripts; native handlers add the rows marked in the sim table):

| record | C2 address | flags | attack rows in its state scripts / handlers (static) | spells (a = area, o = own side) |
|---|---|---|---|---|
| 0 | C2:D080 | 1C11 | 3(12) 10(11) 11(11,4000,99%) 68(7) 99(10,0010,99%) | 01 01a 1Co |
| 1 | C2:D658 | 9C01 | 7(65,0040,99%) 39(70) 40(84) 41(94,0010,33%) 43(98,0010,99%) 44(94) 45(103,0010,33%) | 00 00a 05o |
| 2 | C2:D20A | 1C11 | 47(48,1000,33%) 48(43,1000,33%) 49(52,1000,33%) | 1Co |
| 3 | C2:DF9E | 9C11 | 1(34,0020,99%) 2(125) 53(40,4000,50%) 72(34) | 12 13 14 15 06a 07a 09a 0Ca 0Da 0Ea |
| 4 | C2:E5AD | 9C02 | 19(81,0010,99%) 75(76,0004,99%) 77(76,2000,99%) 78(85,0010,33%) 81(81) 82(95) | 06 08 25 26 |
| 5 | C2:DD35 | 9C11 | 36(35) | 0Bo 0Bao |
| 6 | C2:D451 | 1C11 | 13(124) 50(106,0010,33%) 52(111,0010,50%) | 1Co 29o |
| 7 | C2:EB49 | 9C01 | 25(93) 26(69) 63(44,0010,33%) 65(46,0010,33%) 66(36,0010,33%) 70(60) | 04o 22o |
| 8 | C2:E82E | 9C01 | 14(93,0020,99%) 15(89,0040,99%) 59(89,0010,99%) 64(89,2000,99%) 71(89,0080,99%) 73(89,0100,99%) 74(89,0200,99%) 76(89,0004,99%) 110(127,0010,99%) | 00 06 08 13 24 25 26 |
| 9 | C2:E120 | 1C11 | 22(71,0200,99%) 100(71) | 00 01 06 07 0C 0E 12 14 26 |
| 10 | C2:ECFD | 1811 | 18(67,0100,80%) 58(68,0040,99%) 60(40) 61(59,0200,99%) 62(50,1200,50%) | 00 01 |
| 11 | C2:F00C | 1801 | 2(125) 16(0,1000,50%) 18(67,0100,80%) 20(0,0010,50%) 54(120,4000,99%) 55(120,0020,99%) 103(123) 109(125) | 06 0D 13 |
| 12 | C2:DAEA | 9C11 | 0(35,4000,99%) 5(37) 21(22,1000,99%) 33(34,0010,50%) 34(36,0010,33%) 35(33) | 0C 10 |
| 13 | C2:E2F5 | 1801 | 8(33,0010,99%) 17(37,0010,99%) 56(20) 57(48) | 0C 0E 10 16 12a 13a 14a |
| 14 | C2:D8D8 | 1C19 | 79(100) 92(120) 93(110) 95(120) | 1E 1F 22o |

### 15.2 Boss id -> record -> rows (runtime, 2 seeds x 3,000 frames each, no heroes on the arena)
`record` = index into the 15 records (matched through the init handler of the spawned object); `-` = no native record (object created with flags `0x1811`, `0x9C11` or `0x180D` and no handler pointers; these bosses are driven by the AI bytecode engine or by code reached from other tables).

| boss id | record | attack rows seen in the real engine (power, status, chance) | spells seen |
|---|---|---|---|
| 0x57 | 0 | 68(7) 99(10,0010,99%) | 01 |
| 0x58 | - | 10(11) 23(22,0004,50%) | 06 08 0B |
| 0x59 | - | 28(30) 30(25,2000,33%) | - |
| 0x5A | 1 | 39(70) 40(84) | 05 |
| 0x5B | 12 | 34(36,0010,33%) | - |
| 0x5C | - | 96(33) 97(42,0010,50%) | - |
| 0x5D | 13 | 56(20) | 13 |
| 0x5E | 3 | 1(34,0020,99%) 72(34) | 06 |
| 0x5F | 5 | - | - |
| 0x60 | 7 | 63(44,0010,33%) | 04 |
| 0x61 | - | 16(0,1000,50%) 23(22,0004,50%) | 08 0B 13 |
| 0x62 | 4 | - | - |
| 0x63 | 0 | 68(7) 99(10,0010,99%) | 01 |
| 0x64 | 7 | 63(44,0010,33%) 65(46,0010,33%) 66(36,0010,33%) | 04 |
| 0x65 | 2 | 47(48,1000,33%) 48(43,1000,33%) 49(52,1000,33%) | 1C |
| 0x66 | 1 | 39(70) 40(84) | 05 |
| 0x67 | - | 28(30) 30(25,2000,33%) | - |
| 0x68 | - | 29(89) 30(25,2000,33%) | 1E 1F |
| 0x69 | 10 | 60(40) | 01 |
| 0x6A | - | 83(88) 84(88,2000,99%) 85(88,4000,66%) 86(88,0020,66%) | 24 25 26 90 |
| 0x6B | 12 | 34(36,0010,33%) | - |
| 0x6C | - | - | - |
| 0x6D | - | 96(33) 97(42,0010,50%) | - |
| 0x6E | 14 | 95(120) | 1F 22 |
| 0x6F | 9 | 22(71,0200,99%) 100(71) | 26 |
| 0x70 | 6 | 50(106,0010,33%) 52(111,0010,50%) | 1C 29 |
| 0x71 | 5 | 36(35) | - |
| 0x72 | 7 | 63(44,0010,33%) 65(46,0010,33%) 66(36,0010,33%) | 22 |
| 0x73 | 11 | - | - |
| 0x74 | 3 | 53(40,4000,50%) 72(34) | 0D |
| 0x75 | 11 | - | - |
| 0x76 | 13 | 56(20) 57(48) | 0E |
| 0x77 | 11 | - | - |
| 0x78 | 4 | - | - |
| 0x79 | 8 | 14(93,0020,99%) 15(89,0040,99%) 59(89,0010,99%) 64(89,2000,99%) 76(89,0004,99%) 110(127,0010,99%) | 24 |
| 0x7A | 5 | 36(35) | - |
| 0x7B | 10 | 18(67,0100,80%) 58(68,0040,99%) 60(40) | - |
| 0x7C | - | - | 07 |
| 0x7D | 3 | 2(125) 72(34) | 13 |
| 0x7E | - | - | - |
| 0x7F | - | 113(92) 114(120) | 22 28 |

Caveat: this table was produced with the earlier harness that called the boss engine on every frame (twice the real tick rate, see §1.3); the rows and spells seen per boss are valid, but a boss that fires rarely may have been seen in too few ticks of a 3,000-frame run under the real schedule. Per-boss runs have not been repeated.

Notes: the same record serves several bosses (e.g. record 7 = ids 0x60, 0x64, 0x72; record 3 = 0x5E, 0x74, 0x7D; record 12 = 0x5B, 0x6B); the rows listed for a boss in the first table may be a subset of its record (only what the unattended boss actually fired). Bosses 0x6C and 0x7E created no object in this state (spawn gate) and 0x5F/0x62/0x73/0x75/0x77/0x78 never fired an attack row in 3,000 frames without heroes; why is not determined.
The Dark Lich (record 8) in full, state by state (state ids of `docs/dark-lich.md`): projectile states 0F-17 use rows 64 (power 89, status 0x2000 99%), 59 (89, 0x0010 99%), 76 (89, 0x0004 99%), 15 (89, 0x0040 99%), 74 (89, 0x0200 99%), 71 (89, 0x0080 99%), 73 (89, 0x0100 99%), 14 (93, 0x0020 99%), 15; every hands state (1E-27) uses row 110 (power 127, 0x0010 99%). Attack = `Str + power & 255` = 74 + power (so 163 and 201).

### 15.3 Boss id 0x7F [full description in `docs/mana-beast.md`]
- Record: level 73, HP 9990, MP 99, Str 99, spell power 45/45, magic level 8, weapon level 4 (`data/monsters.json` id 127). Spawned object: flags `0x180D`; logic = the 21-phase machine in bank C2 (table `C2:8F51`), which loops until the dead bit is seen in phase 0xB.
- Attack rows set by that code: 114 (power 120) at the end of phase 4, 113 (power 92) at the end of phase 5, 111 (power 89) in phase 7; each is a scripted hit on all three heroes, never a contact attack. Spells: Wall (0x22), Dispel Magic (0x26) and Lucent Beam (0x28) from phase 0xB (`C2:93CB`, choice by the Wall status of the boss and of the nearest hero).
- Earlier versions of this section said the boss idled in its intro phases; that came from calling the engine without the game's frame loop. With the full frame (`$C0:B08C`) it cycles through all phases. Timings, hit windows and the end of the fight are in `docs/mana-beast.md`.

## 16. Consumable items (ids 0-11)

Executed: `tools/dump_gap_tables.py` runs `$C0:54E7` on a hero (500 max HP, 100 HP, 50 max MP, status words 0x0010, 0x0100 and 0x2000 set) and on a dead hero, for every id; results in `data/item_effects.json`.

| id | effect on a living hero (pending value, applied by `$C0:4004` next tick) | on a dead hero |
|---|---|---|
| 0 | HP +100 (`E1F3`) | nothing (heal is skipped for dead actors) |
| 1 | HP +250 | nothing |
| 2 | HP set to max (`E1F3` = max HP) | nothing |
| 3 | MP +50 (`E1F6`, cap 99 and max MP) | nothing |
| 4 | clears `E190` and every `E191` bit except bit 7; timers `E1B2-E1B6` set to 1; HP unchanged | nothing |
| 5 | HP to max AND clears the dead flag (`E191` bit 7) | **revives** at max HP |
| 6, 7 | no effect in this routine | - |
| 8 | toggles `E191` bit 4 (status 0x1000), `E1B5 = 0xFF` | toggles |
| 9 | toggles `E191` bit 1 (status 0x0200), `E1B5 = 0xFF` | toggles |
| 10 | toggles `E191` bit 2 (status 0x0400) | toggles |
| 11 | none | - |

`item_effects.json` also stores the effect byte of each item row (`D0:4152 + id*16`: 100, 250, 1, 50, 1, 250, 1...): it is used only by ids 0, 1 and 3 (HP/MP amount); id 2/5 use the max-HP shortcut and 4 ignores it. Heal values are subject to the application rules of §5.4 (cap 999 HP / 99 MP, max HP). The toggles are an XOR of the bit, so a second use removes the bit; the 0xFF timer of 8 and 9 is irrelevant for 0x0200 (no timer) and counts down only for 0x1000 (1020 ticks). Which routine decides when an item can be used from the menu was not determined.
Prices: `data/prices.json` holds the raw price word tables (range table `D8:FB84`, read at `$C0:7CA5`), in table order: consumables, armor rows (head 0-20, body 21-41, accessory 42-62) and the orb price table. The consumable entries are listed in table order only; the pairing of each entry with an item id above was not verified.

## 17. Everything in seconds (60.0988 Hz frames; ordinary actors tick every 5th frame)

| quantity | frames / ticks | seconds | status |
|---|---|---|---|
| charge, one stage | 90 frames | 1.50 | [V executed] |
| charge, stages 1..8 | 89, 179, ..., 719 frames | 1.48 ... 11.96 | [V executed] |
| weapon recharge gauge `E1ED` after a swing | `(100-Agi)/2 + 50` frames (Agi 50: 75, Agi 99: 50) | 1.25 / 0.83 | [V] |
| status timer unit | 4 ticks = 20 frames | 0.333 | [V executed] |
| status/buff duration, d = 20 (all hero spells) | 77 ticks | 6.4 | [V executed ticks; seconds = ticks / 12.02] |
| status duration, damage 300, mdef 0 | 4*75-3 = 297 ticks | 24.7 | [V formula] |
| poison / engulf damage | 1 HP per 4 ticks | 3.0 HP/s | [V executed] |
| Dark Lich / C2 bosses with flags 0x9C or 0x18 | tick on 1 of 5 frames (`$56` = 0) | 12.02 ticks/s | [V, `docs/dark-lich.md`, `docs/mana-beast.md`] |
| weapon level, kills per level | `ceil(99/(9-wl))` kills, x2 on lower-level monsters | - | [V] |
| spell level, casts per level | `ceil(100/(9-L))` first-hits | - | [V] |

## 18. Open questions (not determined)
These are unknown; no values or formulas are claimed for them.
- What writes `$CFFC` during the story after the new-game init; where `$CFB8` is incremented on pickup; where the initial `$CFC0` table (9,8,9,9,9,9,9,9 in every save) comes from; the class-8 chest object (orb pickup).
- Which in-game ailment each status bit is (only the sources are listed, §12.1); the sources of bit 0x0008; the effect of bit 0x0080.
- Whether the tick rate of ordinary monsters (12 Hz, §1.3) holds in a running frame loop; it comes from reading the dispatcher.
- Boss id 0x7F: see `docs/mana-beast.md` section 12. Other bosses: attack rows are those fired unattended in 3,000 frames plus the static rows of their record; bosses 0x58/0x59/0x5C/0x61/0x67/0x68/0x6A/0x6D/0x7C use no native record and 0x6C/0x7E created no object in the test state.
- Drop row byte 1 low nibble; the purpose of byte 0 bit 7; the trap-spring code path (`$C8:E1ED`, read, not executed); the hero code that copies the target count to `E176`.
- The weapon byte-5 and element bytes in physical damage (§6), the `BIT $0040` armor branch, `$CC7A`, monster record byte 22, weapon-row byte 2 upper bits, `E1B1` bit 1, `E1FB` bit 5.
- How monster projectile/ranged attacks reach `$C0:4F7B`; all contact attacks follow §4, spell hits §5, both executed on real routines with synthetic objects, not in a live battle (the save states contain no live monsters).
- Menu-level item restrictions; the upgrade-shop purchase routine was read, not executed.
- The pairing of price table entries with item ids (§16) and of the `E177` element index with element masks (§11.1).
- The harness never runs the PPU/sound; `$C3:0000/0004` are stubbed. All save states are late-game (levels 66-71, `$CFFC = 8`); anything that depends on story variables was checked only at those values.

## 19. Reproduce
Set `R` to your ROM, `S` to a ZSNES save state of a late-game session with the party on a map, `A` to a save state taken in the map-246 arena, and `D` to a directory with save states.
```
R=path/to/rom.sfc; S=path/to/state.zs2; A=path/to/arena.zs3; D=path/to/states
python3 tools/validate_phys.py  "$R" "$S" 800      # physical model vs ROM
python3 tools/validate_magic.py "$R" "$S" 400      # magic/heal model vs ROM
python3 tools/validate_exp.py   "$R" "$S"          # EXP / level-up
python3 tools/hero_stats.py     "$R" "$D"/*.z1? "$D"/*.zs?      # stat build vs live hero objects
python3 tools/weapon_level.py   "$R" "$S" 2000     # weapon level-up rule vs $C0:4358
python3 tools/validate_magic_level.py "$R" "$S"    # spell level rule vs $D0:4DCD
python3 tools/charge_timing.py  "$R" "$S" 8        # charge stage timing from $C0:B330
python3 tools/validate_drops.py "$R" "$S"          # death + chest routines vs drop model
python3 tools/validate_status_rules.py "$R" "$S"   # $C8:E301 cascade, immunity, timers
python3 tools/status_timing.py  "$R" "$S" 20       # real combat tick: lifetime and damage over time
python3 tools/validate_boss_attacks.py "$R" "$A"   # all 115 boss attack rows through $C0:45D6
python3 tools/boss_sim.py "$R" "$A" 3000 1,2 79,7f > sim.jsonl ; python3 tools/boss_records.py "$R" data/boss_records.json sim.jsonl
python3 tools/dump_tables.py    "$R" data          # regenerate data/*.json (monsters, weapons, armors, spells, boss attacks, ...)
python3 tools/dump_gap_tables.py "$R" "$S" data    # drops, item effects, weapon levels, status effects, prices
python3 tools/flowdis.py        "$R" out.txt --auto C04004:10 C0514B:10    # recursive disassembly (ADDR:MX seeds)
```
Tools: `combat_env.py` (harness: run any routine on a save state), `rng.py`, `phys_model.py`, `magic_model.py`, `hero_stats.py`, `example_damage.py`, `names.py`, `flowdis.py`, `dump_tables.py`, `weapon_level.py`, `validate_magic_level.py`, `charge_timing.py`, `validate_drops.py`, `validate_status_rules.py`, `status_timing.py`, `validate_boss_attacks.py`, `boss_sim.py`, `boss_records.py`, `dump_gap_tables.py`.

## 20. Spells without direct damage (details in `docs/spells-non-damage.md`)
The class column of the spell table in section 5.5 is superseded for the non-damage spells by `docs/spells-non-damage.md` (every handler was executed on random caster and target objects and compared with a model, `tools/spell_effects.py`, tables in `data/spell_effects.json`). Short version [V]:
- Common value `V` = the heal-style spell value of 5.2/5.3 (`m = 4` for Speed Up, Defender, Lunar Boost, Cure Water, Remedy, Revivifier, sabers, Moon Energy, Wall, spells 20 and 23, Lucid Barrier; the element test for the others), divided by the number of targets. Buff timers are `V>>2`; status durations are `max(20, (V>>2) - (magic defence>>1))` timer units of 4 ticks.
- 02 Speed Down, 14 Silence, 16 Balloon, 18 Change Form and 1E Sleep Flower do no damage, they only apply a status word (no accuracy roll, only the immunity word). 07 Acid Storm and 10 Fire Bouquet deal damage and write a DEF-25% / ATK-25% buff byte (the 0x2000 word of row 07 is not applied); 11 Blaze Wall deals damage and applies 0x4000.
- Sabers: `E19D = 4(L'+1)` weapon hits, `E1AE` low nibble = `L'`, damage bonus `base/(10-L')` for `L' >= 1` (up to +50%), element bit into `E195` (Moon Saber = the drain flag 0x40), status word into `E199`.
- Remedy / Revivifier cure by shortening all status timers by `V>>2` units, or completely at `L' == 8` (or `L' >= 4` on one target); Revivifier revives a hero or boss and heals `V/(9-L')`. Wall blocks the next `L'+2` spell hits (spells 20, 23 and 26 pass); Lucid Barrier absorbs weapon damage (not spell damage) into a pool of `min(V, max HP)` with a flat guard of `defence/2/n` per hit.
- Corrections to earlier sections: 5.5 calls 02, 14, 16, 18, 1E "damage + status" (they deal no damage); 12.2 says spell durations are always 20 and buff timers use "the same d" (they follow V as above); 4.6 gives the Saber level as 1..9 with up to +100% (the level is 0..8, level 0 adds nothing, level 8 adds 1/2).
- Reproduce: `python3 tools/spell_effects.py "$R" "$S" 300 data/spell_effects.json`.

## 21. Hero movement (details in `docs/hero-movement.md`)
Movement is per frame (60.0988 Hz), not on the 12.02 Hz combat tick, in whole pixels (`E002/E004`); the velocity word `E006` is set by `$C0:B710` from the pad and applied by `$C0:D5C0` [V, `tools/hero_movement.py`, numbers in `data/hero_movement.json`]:
- walking 2 px per frame per pressed axis (120.20 px/s, no acceleration, diagonal not normalised: 169.99 px/s); running on a new press of the A button 3 px per frame (180.30 px/s, direction locked until A is released, then a recharge of `(100 - Agi)/2 + 50` frames in `E1ED`); charging a weapon (`E01B != 0` or `E01A >= 2`) and the statuses 0x0004 and 0x0400 make it 1 px per frame; 0x0020, 0x0040, 0x0100, 0x4000, 0x8000 stop it; equipment and level do not matter.
- wall collision is per axis on 16 px tiles with a probe box of x +6/-6 and y +4/-4; a refused diagonal keeps the X component, then the Y component; a corner contact nudges the hero sideways by 2 px per frame; facing on a diagonal is the horizontal direction.
- knockback when hit: opposite to the facing, 55 px in 40 frames (3, 2, 1 px per frame for 5, 5, 30 frames), whatever the damage or the attacker position.
- Reproduce: `python3 tools/hero_movement.py "$R" "$S" data/hero_movement.json`.

## 22. Hit reactions, hits during the hurt state, statuses (details in `docs/hit-reactions.md`)
- A hero hit is applied on the next combat tick; the hurt state lasts 110 frames (1.83 s), knockback 55 px / 40 frames, dying 160 frames. Hits that land during the hurt state are resolved at once but only added to `E1F1` and applied together when the state ends (a new reaction follows) [V].
- Frozen, petrified, balloon and engulfed targets take weapon damage without a reaction; a hit wakes a sleeper (status 0x0010 cleared); only status 0x0400 blocks hits [V].
- The forced crit nibble `E1AE & 0xF0` is checked before the weapon gauge, flat guard follows the defense roll (re-validated with random high nibbles, 300 cases) [V]. `tools/godot_vectors.py` exports model results for physical hits, spells, hero stats, EXP, weapon progress, status durations and drops.

## 23. Which hero casts which spell, when MP is paid, status 0x1000
- **Spells per hero** [C, `$D0:CE5D-CEBC`, checked against the save states]: the ring menu builds the spell id from the spirit index `e` (0 Gnome, 1 Undine, 2 Salamando, 3 Sylphid, 4 Luna, 5 Dryad, 6 Shade, 7 Lumina) and the position `k` in the spirit's ring. When the caster slot `$1808` is odd (the girl) the id is `6e + 3 + k` (the Wis half, ids with `id % 6 >= 3`; Lumina `6e = 42` is folded back by 6 to 0x27-0x29), otherwise `6e + k` (the Int half, the sprite; Shade 0x24-0x26). The spirits and spells offered come from a per-hero bit table at `+0x1C8..+0x1CF` of the hero record, one byte per spirit: the girl's bits 3-5 (`$D0:D0B0`, record `$E3C8`), the sprite's bits 0-2 (`$D0:D0DA`, record `$E5C8`), one bit per learnt spell. In the late-game saves the girl has `38` for every spirit except Shade (`00`) and Dryad (`18` or `38`), the sprite `07` except Lumina (`00`) and Dryad (`03`/`07`); the boy has none. So the girl casts only the support half of each spirit (and Lumina), the sprite only the offensive half (and Shade); the Mana Magic pair (0x20 / 0x23) is the last bit of Dryad for each.
- **MP payment** [V, `docs/summon-cast.md` section 5, `data/summon_cast.json`]: the cost is taken when the first target is hit (`$C8:E6D4`), which is the spell-start frame for most spells, 6 frames later for the sabers 03 09 0F 15 1B 27 and Moon Energy 1D, and for Fire Ball 0C and Air Blast 12 when the first homing object reaches its target (section 24).
- **Status 0x1000** [V, `tools/hurt_loop.py`-style runs with the status set on the hero]: the hero still walks and swings, but **its weapon hits do no damage** (a passive monster in front kept 9999 HP while the same swing without the status took 152) and **a spell cast is refused** (no MP paid, no damage). Source: item 8 (toggles it with the timer `E1B5 = 0xFF`, 1,017 combat ticks = 84.6 s) and the monster / boss attack rows that carry it (§12.1). This is the Moogle Belt status (the hero sprite set is moved by 0x70 to the moogle pictures).
- `tools/godot_vectors.py` also exports `monster_spell`: spells cast by every ordinary monster with its record Int/Wis, spell powers (bytes 18/19) and magic level, `E1E6 = $CFFB = 0`.

## 24. Homing objects of Fire Ball and Air Blast [V, `tools/spell_flight.py ROM check`: 2784 of 2784 MP-paid frames of `data/summon_release.json` `grid`]
At the spell start (0C: frame 121, 12: frame 131) the launch `$C1:8B82` makes three objects in the caster record, groups `+0x110`, `+0x120`, `+0x130` (16 bytes each: `+0` hits left (table `$D0:2AA1` by level), `+2` angle, `+3` speed, `+4` start delay, `+5` top speed, `+6/7` target object, `+8/+A` x/y from the caster's foot point, `+C/+D` x/y fractions). The step `$C1:8DDE` runs every frame for each group in order:
- group start: up (0, -24), down (0, 24), right (24, 0), left (-24, 0); objects at start + (0, -8), (8, 5), (-8, 8); start angles (0 = +x, 0x40 = +y, 0x80 = -x, 0xC0 = -y): up C0/00/80, down 40/00/80, right C0/00/40, left C0/40/80; delays 5, 15, 25 frames; speed 0, +2 per frame up to 0x70;
- target object `n` = the n-th entry of the target list (`+0x178..17A`; an empty entry takes the first). The aim point is the target's foot point minus its body height (`+0x74 & 0x7F`: 14 for the heroes, `$D0:0000 + id*16 + 4` for the monsters), the object's point is the caster's foot point minus 14 plus the offset;
- steering `$C1:8E93`: with `d = aim - object`, the larger of `|dx|`, `|dy|` (low byte) below 24 is a **hit** (`$C1:8D12`, which runs the spell handler `$C0:0054` → the MP of the first hit is paid). Otherwise the octant angle `atan[min*16/max]` (17-entry table `$01:9006`, 0..32) is turned toward by up to `max(3, (255 - max) >> 5)` per frame (a gap below 5 is closed at once), only once the speed is 0x20 or more; the move is `(speed * sin/2) >> 8` plus a 4-bit fraction, shifted by 3, per axis (sine table `$00:FD22`, 65 words 0..256, every 8 bytes).
The MP-paid frame (and with it the damage of the first hit) therefore depends on the facing and on where the target stands: 16-71 frames after the start over the measured grid. Remake: `engine/combat/spell_flight.gd` runs this model to delay the payment; `tests/combat/rom_spell_flight.json` (`spell_flight.py ROM vectors`) holds the measured grid. Not modelled: the later hits and the release of the caster (the hero-free frame stays the measured grid of `docs/summon-cast.md` section 7).

## 25. Which monsters a hero's hostile spell reaches [V, `tools/hostile_window.py ROM STATE check`: 2000 of 2000 random layouts]
`$D0:DA60` (called with `$1842` bit 7 set for a hero's spell) walks the monster slots 3, 4, 5 (`$7E:E600`, `E800`, `EA00`) and stores each valid slot at `$1812 + 2n` (0 for an invalid one), the count in `$1834`. A slot is valid when its screen foot point (`+0x20/+0x22`, map position minus the camera `$A8/$AA`, unsigned words) has **8 <= x < 248 and y < 216**, `+0x00` is exactly 1 (active), the id `+0x180` is below 0x80 and the monster is alive (`+0x191` bit 7 clear) or has id 0x54 / 0x55. Ids 0x80 and up count only with `$1842` bit 5 and id 0xEA. So the targets are every live monster of the three slots whose feet are on the 256 x 224 screen less an 8-pixel band at the left, right and bottom.
Remake mapping (`engine/combat/rom_targets.gd`): the SNES screen is a 256 x 224 rectangle centred on the Godot camera's screen centre, and the window above is applied inside it, whatever the wider Godot view shows. Approximations, stated: (1) the ROM never has more than three monsters in slots 3-5 (which monsters a map puts there is the map's spawn order); the remake takes the three nearest to the caster when more stand in the window; (2) the ROM camera rule (`$A8/$AA`) is not decoded, so the window follows the Godot camera centre (the ROM keeps the heroes inside screen x 0x38-0xC7, y 0x48-0xAF, `docs/hero-movement.md` section 9, which a centred camera satisfies).

## 26. When each spell becomes available [C: event command decode; V: `tools/spell_unlocks.py ROM check` on six save states, 96 of 96 spell bytes are values the events write]
The spell bytes `+0x1C8 + spirit` of the girl (bits 3-5) and the sprite (bits 0-2) are written only by event command 39 (SET_FIELD_BYTE: actor, field, value; actor 2 = girl, 3 = sprite), in nine events (`tools/spell_unlocks.py ROM list`):

| event | spirit | girl byte | sprite byte | spells added | reached from |
|---|---|---|---|---|---|
| 0x580 | Gnome | 0x38 | 0x07 | girl 03-05, sprite 00-02 | GOTO in 0x22E |
| 0x581 | Undine | 0x38 | 0x07 | 09-0B, 06-08 | not found by the static scan |
| 0x582 | Salamando | 0x38 | 0x07 | 0F-11, 0C-0E | CALL in 0x38D |
| 0x583 | Sylphid | 0x38 | 0x07 | 15-17, 12-14 | CALL in 0x4E2 |
| 0x584 | Luna | 0x38 | 0x07 | 1B-1D, 18-1A | not found |
| 0x585 | Dryad | 0x18 | 0x03 | 21-22, 1E-1F | CALL in 0x4B6 |
| 0x586 | Shade | 0x00 | 0x07 | sprite 24-26 | not found |
| 0x587 | Lumina | 0x38 | 0x00 | girl 27-29 | not found |
| 0x42D | Dryad | 0x38 | 0x07 | Mana Magic 23 / 20 | not found |

So a spirit teaches all of its spells at once (no per-level unlocking); only Dryad comes in two steps, the Mana Magic bit being set by the story event 0x42D (the same event that changes map with transition 47 and raises flag 0x4E). The four events without a caller in the event scripts are started from outside the scripts (map object or trigger words, which the static scan does not read). The ring menu then offers a spirit when its byte has a bit for the caster and a spell per set bit (`$D0:D0B0` girl, `$D0:D0DA` sprite, section 23).
Remake: `engine/combat/spell_unlocks.gd` (`apply_rom_field(actor, field, value)` is what an event runner calls for command 39; `learn_spirit(n)`, `learn_mana_magic()`), saved with the progress state; the ring menu hides spells not learnt. Without any story event yet run the remake keeps every spell available (free play). Test: `tests/combat/rom_spell_unlocks.json` (`spell_unlocks.py ROM vectors`).

## 27. Approximations still open in the remake's combat (and why)
- **Monsters per hostile spell** (section 25): the ROM has three monster slots; when a Godot room has more monsters inside the window the three nearest are taken, because the slot assignment belongs to the map spawn order, which the remake does not reproduce.
- **ROM camera** (section 25): `$A8/$AA` is not decoded; the SNES screen is placed around the Godot camera centre.
- **Later homing hits and caster release of Fire Ball / Air Blast** (section 24): only the first hit (the MP) is modelled; the hero-free frame is the measured grid.
- **Events 0x581 / 0x584 / 0x586 / 0x587 / 0x42D** (section 26): what starts them was not found by the event-script scan; the remake exposes `SpellUnlocks.apply_rom_field` for the map event runner.
- **New-game flag values** (section 28): the orb, allowance and mana-cap flags are assumed 0 at a new game; the remake uses the late-game values until an event writes them.
- **Physical element quirk** (`ROM_PHYSICAL_ELEMENT_QUIRK`): kept as instructed.
- **Boss hit sounds**: the per-monster hit sound table covers ids 0-0x56 only; bosses keep the default.

## 28. Orbs, orb allowance and the mana cap are event flags [C: event decode; vectors from the decoded commands, `tools/orb_events.py`]
`$CFB8 + t` (orbs found), `$CFC0 + t` (orb allowance, the drop gate of section 11.4) and `$CFFC` (mana cap, section 11.5-11.6) are event flags `0xB8-0xBF`, `0xC0-0xC7` and `0xFC` (`$7E:CF00 + n`), written only by event commands (`orb_events.py ROM list`); no routine writes them directly (an absolute-address scan of the ROM finds only the new-game clear of `$CFFC` at `$C0:5787`):
- **Orb pickup, events 0x500 + t** (called by 0x670 + t and by story events, e.g. 0x233, 0x527 for the glove): if the orb count is 9-15 (sword 8-15) go to 0x5E4 (nothing more); else orbs + 1, then the allowance + 1 if it is 0-8 (sword 0-7), a message, and on to 0x5E5. So every orb picked up also raises the allowance, and the allowance is otherwise raised by single story events: 0x040 (glove, when 0), 0x103 (sword), 0x1B6 (axe), 0x13A (spear), 0x689 (whip), 0x1D4 (bow), 0x041 / 0x042 (boomerang, when 0), 0x581 (javelin; the Undine event). A monster orb drop is kept only while `orbs + 1 < allowance` (section 11.4): those story events are what lets monsters drop orbs.
- **Mana cap, event 0x594**: `$CFFC` + 1 while it is 0-7, called by the Mana Seed events 0x204 and 0x589-0x592 (each also sets one of the flags 0x90-0x97). So the spell-level cap and the `+CFFC^2` damage term grow by one per Mana Seed, up to 8.
- Event 0x5F9 sets the late-game values at once (allowance 9, sword 8, cap 8, flags 0x90-0x97 = 1); 0x5F8 sets the sword allowance 8 and its orbs 7.
Remake: `engine/combat/rom_progress.gd` (`orb_found(t)`, `mana_seed()`, `on_flag(index, value)` for an event runner, the allowance saved with the progress state; the drop gate of `drop_roller.gd` reads it). Until an event has written the allowance the late-game values (9, sword 8) apply. Test: `tests/combat/rom_orb_events.json` (`orb_events.py ROM vectors`, events run on every nibble value). Not verified by execution: the new-game value of these flags (assumed 0; event 0x040 increments the glove allowance only when it is 0).

## Matching physical-combat source reconstruction

The ROM routines4FED/505B/50CA/514B/51E7/52E7/532C/541B and arithmetic543B/54B6 now have independently assemblable ca65 sources, alongside their two long-call wrappers and RNG3884/3888/389C/38A0. All16 files match1,298 original ROM bytes exactly. Three contiguous components (300-byte defence/accuracy/element,924-byte damage/arithmetic and74-byte RNG) also assemble and match; their bytes are already counted in the source blocks. Named object fields refer to the established offsets in this document.

The existing physical/RNG models validate numeric hit/damage results in their documented domain, not every object/animation/status side effect. `tools/physical_branch_probe.py` executes96 additional cases to resolve instruction widths at Saber expiration, healing/drain routing, forced criticals, gauge/barrier and capped defence. Original game routines run with supplied object/RNG states and the existing sound hooks. These are execution samples, explicitly not an independent model comparison. Full semantic recovery of those ancillary effects, physical hardware timing and the full combat dispatcher remain open.

## Matching spell-hit source reconstruction

Eight sources reconstruct E7A7's90-byte dispatcher body, E855's57-byte stat selector, E8A0/E8C0/E990/E9DF element/attack/defence/damage stages, EA8B's standard damage handler and EBB3's Cure Water handler. All530 instruction bytes match the original ROM. The84-byte42-entry dispatch table atE801 is rebuilt separately from the audited numeric spell table, preventing data from being mistaken for instructions. Named entries that are not yet reconstructed retain an `unrecovered_spell_handler_...` name. Two integrated components (174-byte dispatch/table and348-byte element/attack/defence/damage) also match; reused bytes are not counted twice.

`tools/magic_boundaries.py` compares192 damage results,64 healing results and four exact-defence-equality cases against the established readable models. It exercises hero/monster casters for each selected spell, level0/7/8, target divisor1/2/3, stat/power byte wrap, attack/defence caps and resistance-first precedence. Equality produces minimum damage1 at E9DF. Original spell/RNG/arithmetic code executes with the existing sound hooks. Ancillary object/status/animation writes and physical timing remain outside these numeric comparisons.


### Reconstructed native timer groups and controller-position guard

`tools/combat_status_timers.py:StatusTimers` adds231 matching instruction bytes in five complete routines. Three timer groups form the67-byte C03D1C..C03D5E component; position recording/rectangle checking form164 bytes atC03DDC..C03E7F. This is reconstructed executable behavior, not just a catalogue of addresses.

C03D1C tests low status with7E and decrements byte1B2 only when that test is nonzero. On expiry it masks the status with83, preserving bit1 despite bit1 having participated in the activation test. Timer zero wraps toFF rather than expiring immediately. C03D31 tests the combined word status with0180 and decrements1B3; expiry masks the word withFE7F. Its accumulator retains the initially read status across the byte-timer decrement and returnsM8. C03D4D similarly tests high status18, decrements1B5 and clears those bits on expiry. Unrelated actor fields and CPU carry/overflow are retained.

C03E35 scans controller bytesD9/DA/DB in order and accepts the first negative byte. Its low nibble minus1 determines the party binding, with binding3 decremented to2. It marks actor1B1 bit20 and snapshots actor coordinates2/4 into FF86/FF88 plus four times that binding. No negative controller leaves actor memory unchanged. The wrapper restores P before restoringDB/D, so final N/Z reflect the restored direct-page register, not the original P bits; incoming byte indexes also truncate the resulting X/Y when P is restored.

C03DDC checks that mark and constructs an unsigned rectangle using the saved center plus/minus12. Lower bounds are inclusive and upper bounds exclusive; word overflow is preserved. Outside the rectangle it clears CF03 and actor1B1 bit20. An active marked path reloads X fromDP85, which can differ from the incoming actor. An unmarked path returns immediately with the original X and its real accumulator/flags.

`data/combat_status_timers.json` verifies5120 complete original no-hook calls (1024 per routine). Full randomized128KiB WRAM outside129 caller-stack bytes, static peripherals and CPU/S/D/DB match. Timers and rectangle use D0300/DB7E/M8/X16/binary,12 aligned actors, all256 low-status bytes with sampled high bytes, timers0/1/2/FF and rectangle boundary offsets-13/-12/-11/0/10/11/12/13. The record wrapper checks all eight controller-active masks, bindings1..4, first-negative priority, D0/0300/1234 and both incoming accumulator/index widths with DB7E. Only valid native party bindings are claimed.

The main combat tick remains open. Other timer groups, poison/message effects, buff expiry/stat restoration, status-transition dispatch and damage application still need complete reconstruction and composition. Existing Claude gameplay/status models above remain preserved; this new proof does not imply that all their callees or whole-game timing are closed. No payload is committed.


### Reconstructed timed HP loss, sound and 24-bit cap

`tools/combat_status_damage.py:StatusDamage` adds116 matching instruction bytes: C03D5F50, C0398D23 andC039A443. C03D5F tests high-status bits60, decrements timer1B6 and clears those bits when the timer reacheszero. Zero onentry wraps toFF. A surviving timer setsDP80 bit40 before examining HP. HP zero orone clears the status; neither path stores zero toHP. Higher HP decrements once and requests sound39. Thus this native path is nonlethal, with a floor ofone for initially living actors. This does not establish all poison/status application paths.

C0398D writes a real nonmusic sound packet: command2, the incoming byte asparameter1, parameter2=0F andparameter3=0. It calls C30004 andthe original sender instructions execute. The APU acknowledgement is supplied externally with delay0..3; SPC execution/audio timing is outside the proof. This call is a sound request, not a text-message formatter.

C039A4 clamps the unsigned value stored inDPD0..D2 to98967F (9,999,999). A highbyte below98 returns without inspecting the lowword; highbyte98 compares the lowword to967F; larger highbytes replace both components. Actual accumulator highbyte, carry and returnM8 are preserved, including the early path.

The proof contains3968 complete original no-hook calls:1280 direct cases pereach routine plus16 persistent eight-step status sequences. It verifies1896 sound requests. Full randomized128KiB WRAM outside129 caller-stack bytes,static peripherals,CPU/S/D/DB andall APU packet writes agree. Domain D0300/DB7E/M8/X16/binary,12 actors,all256 status/high-cap/sound bytes in five contexts; timer0/1/2/FF/8,HP0/1/2/FFFF/random andlowcap0/967E/967F/9680/FFFF. Persistent sequences retain timer/HP/status acrosssteps andallclear the tested group bytheir endpoint.

The four recovered status groups now form a117-byte C03D1C..C03D90 component. Sound/cap form66 bytes C0398D..C039CE. Main combat tick, buff expiry/stat restoration, status transition/event callbacks andfull damage application remainopen. See `data/combat_status_damage.json`;no ROM/audio/text payload isstored.


### Reconstructed buff expiry and real notice queue

`tools/combat_buff_expiry.py:BuffExpiry` adds90 instruction bytes: C03D91's four-pair buff expiration loop75 andC058B9's actor-name notice15. C058B9 is a message enqueue wrapper, not an attribute rebuild. The existing matching queue/name/text/clear sources are reused without duplicate source-byte credit, with their flags and hardware effects included in this composition.

C03D91 reads actor1B0 andsaves P afterthat load. It tests pair masks03/0C/30/C0 in order, decrementing timers1BC/1BB/1BA/1B7 only for active pairs. A timer reachingzero clears its pair fromthe retained accumulator andstores the result back to1B0; timerzero onentry wraps toFF. After allfour groups, PLP restores the saved flags. A originally zero buff byte returns directly. Otherwise the routine rereads1B0 andqueues a notice only when the final result iszero. That final-pair path reloads X fromDP85, which can differ fromthe actor whose timers were updated.

C058B9 preservesX whileallocating a30-byte line, appending the hero/monster name token andcopying the original zero-terminated text atC0:62F3. Allocation clears15 words, starts the line with60 andincrements queuecount9F/tailA1. Tail exactly12 wraps tozero; count12 hasits literal transient Y write beforethe computed multiplier result replacesY. The proof preserves that original behavior rather than imposing a new queue cap. A hero token57 subtracts80 fromclass1E7 withnative byte flags; a monster token54 copies that class unchanged.

The report verifies5312 complete original no-hook calls:1024 each forbuff expiry, notice, queue allocator, name token andtext helper, plus24 persistent eight-step buff sequences. Every persistent sequence clears allfour pairs andqueues exactlyone notice onthe final expiry. Full randomized128KiB WRAM outside129 caller-stack bytes,static peripherals including multiplication/quotient shared latch andCPU/S/D/DB agree. Domain D0300/DB7E/M8/X16/binary,12 aligned actors,all256 buff/name bytes,four timercontexts0/1/2/FF,queuecount0..12/tail0..11 anddistinct selected actors. Text isread fromthe supplied ROM only atruntime; no text payload isstored.

The status/buff timer component nowspans192 bytes C03D1C..C03DDB. MainC03A79, death handling/status change/stat rebuild andfull damage composition remainopen. This proof establishes expiration andits notice, not a full stat-restoration cycle or visible message publication. See `data/combat_buff_expiry.json`.


### Reconstructed death references and gold ledger

`tools/combat_death_bookkeeping.py:DeathBookkeeping` adds229 matching instruction bytes acrossfour complete routines. These are dependencies ofdeath/reward handling; the fullC04203 death routine andits level-up/status dependencies remainopen.

C04315 scans six512-byte actor records, comparing dead-actor identityDP81 against fields1AC/1AD/1EC/1F0. Matches at1AC/1AD writeFF; a match at1EC writes0F. The final comparison at1F0 writesFF to1EC, leaving1F0 unchanged. That asymmetry is present in the original instruction stream andis preserved. The loop mutates scratchA6/A7/B2, keepsX andleavesY at the last record, rather than reconstructing a cleaner generalized reference remover.

C0444A reloads actor fromDPCC, reads its weapon byte1E4 andstores onlythe lowbyte ofDPD4 before reloading wordDPD4 asan index. The highbyte remains fromthe caller andcan carry writes into the next WRAM bank. It writesDPD0's lowbyte toE1D0 plus that word index. The proof includes this partial scratch-store behavior.

C044F0 reloads the dead actor fromDP85, adds word1C8 to the party gold lowwordCC6A, propagates carry into highbyteCC6C, applies the recovered98967F cap andstores the result. Native24-bit overflow ispreserved before the cap. C039CF provides the saved-context gold transaction: X37 subtracts the full accumulator word; otherX values add it withthe same cap. An insufficient subtraction writesCF0F=1 andreturns a lowbyteFF whileleaving allthree gold bytes unchanged. Successful transactions do not introduce an extraCF0F reset. The wrapper restoresP beforeDB/D, so finalN/Z reflect restoredD; byte-index restoration truncates the resulting indexes.

4096 complete original no-hook calls agree (1024 per routine). Full randomized128KiB WRAM except129 caller-stack bytes, static peripherals andCPU/S/D/DB match. NativeD0300/DB7E/M8/X16/binary,12 incoming/selected actors,six reference records/all256 identity bytes; all256 lowweapon indexes withhighword0/1/80/FF cover bank carry. Gold lowwords0/1/967E/967F/FFFF,highbytes0/1/97/98/FF andtransaction amounts0/1/967F/FFFF covercap/add/spend/failure paths. Publictransaction D0/0300/1234,DB0/7E/7F andboth incoming widths areincluded. See `data/combat_death_bookkeeping.json`.

The sound/cap/gold transaction component nowspans173 bytes C0398D..C03A39; reference cleanup67 bytes anddead-gold reward40 bytes areindependently built. No ROM/save-state payload isstored. Full death rewards, weapon/actor level-up, status rebuilding andmaincombat composition remainopen.


## Complete weapon and hero experience progression (2026-10-09)

`tools/combat_weapon_experience.py:WeaponExperience` adds465 matching instruction bytes: C04358(84), C043B7(147), C0445B(149), C05B20(32) andC05B40(53). The existing C05C38 decimal source(52) is reused. `data/combat_weapon_experience.json` proves4224 complete original calls, with every original stat, notice, arithmetic andsound callee executing,1999 sound requests and247 weapon-promotion calls. All128KiB WRAM outside129 stack bytes, CPU/S/D/DB, static peripherals andordered sound packet writes agree. Port acknowledgements are controlled inputs; physical SPC playback/timing is not established.

C04358 forms an unsigned byte `9 - killer_weapon_level`, halves it for a lower-level enemy, rejects negative last-weapon-attacker identities andprocesses the incoming hero before the other two party slots. C043B7 skips inactive/dead heroes, enforces the equipped weapon grade+1 equality cap, preserves byte overflow inprogress addition, caps level8 progress at the actual threshold andotherwise increments the packed high/low level nibble, zeros progress, rebuilds attributes andqueues the two-line weapon notice followed by soundCE. The prior documented A6 quirk is now retained in a complete CPU/peripheral comparison: decimal formatting overwrites the gain scratch, which the dispatcher subsequently halves/reuses.

C0445B scans hero2→hero1→hero0 through overlapping scratch83/84. Active heroes with nonzeroHP receive the dead actor's **lowword** experience reward; the dead reward's highbyte is not added by this routine. Carry enters the hero experience highbyte, with native24-bit wrapping before the existing98967F cap. A threshold match advances the level; a strictly larger threshold does not. Each advancement rebuilds attributes, queues the name/decimal/text notice, sendsAF, fills currentHP from newmaxHP andsetsDP80 bit40, then reevaluates the new threshold until level98 or insufficientXP. Existing highbyte residue, scratch side effects andordered writes are retained.

C05B20 queues the actor name anddisplays actor level+1. C05B40 creates two records, selects the weapon-name pointer at C05DFB anddisplays the rebuilt weapon level directly; it jumps past the hero notice's increment. C05C38 formats an8-bit input by signed subtraction loops, rather than a generalized decimal conversion; the proof covers every input byte, including out-of-range signed behavior. Text is read from the supplied ROM at runtime andno dialogue payload is saved.

`tools/combat_stat_rebuild.py:StatRebuild` supplies the missing complete CPU/peripheral contract for the prior `actor_stats_native.rebuild` model. Its4096 calls cover allfour early/ordinary/special/hero paths, twelve aligned slots, both incoming accumulator widths andD0/0300/1234 withDB0/7E/7F. The early path retains the identity-derived accumulator highbyte; ordinary/special returnsA0363,Yactoroffset; hero returnsA03xx,Y3ED7. P is restored beforeDB/D, so finalN/Z reflect restoredD while C/V andwidths come from savedP. Multiplication quotient/product latches andthe ManaSword division dividend/quotient/remainder are preserved. No additional source bytes are credited for these existing routines.

Weapon direct/distribution/notice cases use768 calls per entry; hero XP uses384 calls with levels0/10/95/98, real threshold initialXP, lowword rewards0/1/1000/FFFF andzero/nonzeroHP. NativeD0300/DB7E/M8/X16/binary, valid hero weapon selectors0..7/levels0..8 andqueue tails0..11 are the composition domain. Repeated level-up calls can advance queue count beyond12, preserving original byte arithmetic. Whole death/status/combat composition, physical publication, arbitrary frames/index8/unaligned actors andexhaustive crossproducts remainopen.


## Complete actor death and low-status/death timer (2026-10-09)

`tools/combat_actor_death.py:ActorDeath` reconstructs C04203(266 bytes) andC03CA5(119),adding385 independently assembled matching instruction bytes. `data/combat_actor_death.json` records3072 complete original calls:768 each death,timer,existing C05D46 partycleanup andC8E301 in the stated death context. All original callees run; full128KiB WRAM outside129 stack bytes, CPU/S/D/DB, static peripherals andordered APU packets agree. There are154 drop conversions and122 soundrequests. XP/stat/queue/text/RNG/cleanup sources already counted are reused.

C04203 first clears actorB1/B0 andunlinks six actor references. A special actor (1FB bit40) marksDP80 bit80, retains only8200 status bits in the three heroes, sets its own deadstatus8000 andcalls real partyeffect cleanup. Other actors with last-hit field1EC below3 run weaponXP,heroXP andgold rewards,then reload the dead actor fromDP85. FlagsC1 prohibit ordinary drop conversion. Otherwise the corpse identity is F9 plus the four-bit extraction from1CA; it clears attackrow,marks refresh07 andruns original status application before drawing the drop probability.

Drop probability is `(random_byte & 3F)+1`,so the inclusive comparison uses1..64. Flag40 forces the drop; otherwise its lower six bits must meet the draw. Flag80 decrements **only the low HP byte** with byte-width DEC, preserving the highbyte. B1 becomes80/B0zero. A nonzero forced-item byte1CF maps to item1CF-1. Otherwise a bounded RNG draw selects the low-level tier; levels10/15/25/30/36 successively select tiers2..6,with a further random bytebelow20 adding the final tier. C0430D's numeric permutation is read at runtime. The selected class is stored in1C8,the original sprite in1CA,andonly lowbytes181/184 are cleared.

C03CA5 preserves zero-timer underflow. Low statusbits03 decrement1B4; when it reacheszero,a nonzero175 delays expiration by restoring1B4=1. Otherwise it clears the lowstatusbits andchecksHP. ZeroHP heroes get action2F andpendingupdate40; other low-status expirations markdead without invoking rewards. Without lowstatusbits,already-dead actors return. A newly zeroHP nonhero clears cachedstatus1EE/19E,marks refresh02,retains status8003 andcalls the complete death routine.

The reused C05D46 adapter preserves the shared multiplication quotient latch during program restarts,including malformed status-stack depths. A status-stack write can change an actor's program byte before restart; the latch therefore follows the program actually read after the write rather than a snapshot taken before cleanup. Existing cleanup memory andregister reconstruction remainsunchanged.

Ordinary C8E301 integration is explicitly the death context: currentstatus8000,cachedstatus0,addedstatusDP9A=0,immunitywithout8000 andoriginalsprite0..1F. The adapter reuses the existing full-WRAM status model andadds exact final CPU/arithmetic effects for this context,including no-surviving-hero andsurviving-hero branches. C03CA5 demonstrably clears cachedstatus before death; DP9A=0 is an explicit comparison input andhas not been proved for every caller. This is not a general C8E301 CPU/peripheral reconstruction. Broader status anddamage application remainnext work for fullC03A79.

Proof domains: D0300/DB7E/M8/X16/binary,aligned actors,valid15-byte RNG indices,party queue tails0..11,recipient0/1/2/0F,deathflags0/1/40/80,level boundaries0/9/10/14/15/24/25/29/30/35/36/99,dropflags0/1/3F/40/80/C0/FF andforceditem0/1/FF. Threeheroes use existing maxlevel98 fixtures here;prior complete experience proof covers repeated levels. Group cleanup covers all256 statusdepth bytes andboth controller branches. Physical audio/timing,wholecombat,statusoutside the stated context,unaligned/index8 andwhole-ROM rebuild remainopen.


## Complete combat timer coordination and status outputs (2026-10-09)

`tools/combat_status_flow.py:StatusFlow` adds150 matching instruction bytes: C03C19 coordinator111,C03C88 ManaSword-expiration consumer29 andC058AF genericnotice10. The existing C8E5FF body/C8E5FB long wrapper,C05A87 messages andC03F28 long wrapper/C03F2C controller consumer are reused with new full CPU/peripheral models. `data/combat_status_flow.json` records8192 complete original calls,1024 per entry,with4269 queuedpackets and61 actual soundrequests. All originalcallees execute; complete randomized128KiB WRAM outside129 stackbytes,CPU/S/D/DB,staticperipherals andordered soundwrites agree. Supplied APU acknowledgements do not establish physical playback/timing.

C03C19 decrements1B9 each call. When it reacheszero on a deadhero,it selectsattackF1 andmarksrefresh04. The lowstatus/death helper always runs. Further status/buff timers run onlywhen the resulting1B9 lowtwo bits arezero. Their effects can change the actor before the rest ofthis same call. ActoridentityDP81 zero enables the two independent timersCC79 thenCC78; each transition tonativezero calls C03C88,so simultaneousexpiration executes both calls. The finalB1 bit04 is cleared before testing the fullstatusword; a nonzero word queues the genericnotice andthenreloads X fromDP85. Timerzero underflows ratherthanexpiring immediately.

C03C88 copiesCC77 tohero0's primary weaponrowE1E8 butpasses the currentactorX toattack-row selection,marks itsrefresh04,rebuilds itsattributes,queues the actor-name expirationnotice andreloads X fromDP85. Thus the globalweapon store andthe currentactor operations must not be rewritten as a single generalized hero0 update. C058AF queues a record andappends actualROM text from62E2; it returns the textcursorX andrecordcursorY,which the coordinator handles explicitly.

The attack-row reconstruction preserves classification40's immediate return andpriority order: death,pygmy/engulfed,quarterstep paralysis,other statusgroups andselected equippedweapon. It preserves the accumulator highbyte created by the intermediate16-bit statusload. Death with a newhero deathstatus selectsFF,sets1B9=30 andclears175; previouslymarkedhero death selectsF1. A changedattack row stores the selection andrefresh04; an equalrow returns the selected accumulator withthe comparison's flags. This is independently compared through both direct andlong entries over all256 classification/sprite byte contexts.

C05A87 scans statusbits15→0,removing nonhero death notices,while saving incomingX/Y. Each setbit uses its pointer fromC05DBB,queues aC0 record andwrites the DP85 targetoffset/fullstatus atrecord+16/+18,then appends the targetname andROM text. The comparison deliberately includes anincomingactor different fromthe DP85 target. Fullqueues still advance the originalbyte counters andtail; multiple bits preserve repeated multiplier latch effects. FinalA follows the lastmask load ormessage terminator;CCbecomeszero,D0FFFF,andthe finalC/V plusrestored X/Y flags arepreserved. No textpayload isstored.

The controller helper copiesCC6D..CC6F toEE..F0,scans hero2→hero1→hero0 andtoggles03 for active controllerbindings on lowstatusbit80. Binding4 maps toindex2 afterthe two decrements. It retains byte-XBA accumulator residue,finalY fromthe lastselectedbinding/statusscan,comparisonC andthe finalB5=FE loopflags. Direct andlong entry contracts arecompared for all256 lowstatusbytes andbindings0..4.

Nativecomparison domains areD0300/DB7E/M8/X16/binary,twelve aligned actors,queuecount0..12/tail0..11,timer0/1/2/4/FF,CC78/79 zero/one/two andHP0/1/2/FFFF. Timer/ManaSword compositions usevalidhero/ordinary classifications,cachedstatus0,addedstatus0,immunityexcludingdeath andordinarysprites0..1F;they retain the explicit ActorDeath inputboundary. General C8E301 CPU/recovery/appearance contracts,arbitraryframes/index8/live timing andwholecombat remainopen.


## General status application and status consumers (2026-10-09)

`tools/combat_status_application.py:StatusApplication` completes C8E301's CPU/peripheral contract,including C8E441 recovery,C8E52F appearance andC10024/C12698 statusprogram dispatch. It reuses prior matching sources andthe existing independent full-WRAM status/filter models without duplicatebyte credit. New C03E80(136) andC03F08(31) add167 matching instructionbytes,assembled together as C03E80..C03F26. `data/combat_status_application.json` proves11264 completeoriginalcalls:4096statuscore plus1024 each recovery,appearance,statusflags,pendingprogram,statusprogramAPI,death andtimercoordinator. Everyoriginalcallee executes without hooks;full128KiBWRAM outside129stackbytes,staticperipherals,CPU/S/D/DB andordered audio packets agree.

C8E301 consumes addedstatus9A,filters through inverseimmunity andpreviousstatus1EE,andretains exact earlyreturn accumulator/flags. Death removal calls recovery;death addition clears otherstatusgroups andmarks combat-animation reset. The three exclusivityfilters preserve petrification's HP shift/increment,secondarysleep flag andwordwrites. Duration follows16-bit defense/damage subtraction,then tests the retained16-bit negativeflag after switching to8-bit accumulator mode,before byte comparison/clamp20. Each timerfield is updatedonly for its correspondingnewstatus mask. Controller status recovery,attribute rebuild,appearance selection,attackselection andstatusmessages retain their order andscratch sideeffects.

C8E441 preserves the unusual literal masked1/2/3 binding comparisons. Hero2's binding becomes4 andtherefore takes the program initialization path. Recovery clears selectedstatus/effect words butdoes not invent a reset ofall actorfields. C10020/C125B6 writes the actor program pointer/zeroes programlocals,uses the realhardware multiplier andconditionally rebuilds attributes. The complete recovery contract retains multiplier/stat arithmetic latches,accumulatorhighbyte andrestored contextflags.

C8E52F includes allhero/nonhero priorities andthe realrandom transformation path. Transformation uses C0389C's additivegenerator,selects oneofeight actual C8E5F3 entries,reloadsDP85,marksappearance refresh,rebuilds attributes andassigns the chosen primary/secondaryattack row. Random index/product latches,byteADC overflow andthe resultingA/X/Y/C/V are compared. Coreintegration includes arbitraryaddedstatus andimmunity contexts ratherthan the earlier death-only status adapter.

C03E80 rebuilds stats whenrefreshbits05 arepresent,clearing19D first. Refreshbit02 on a nonspecialactor then comparescurrentstatus190 withcached19E. A negative difference combinedwith the wordrebuild-flags sign can setsecondaryB1 bit08;new4160 statuses setclassificationbit02. Controller low-nibble presence selects whether4160 or5760 sets pendingprogrambit08. It stores fullcurrentstatus in19E andreturns the difference inY;early paths returnYzero. The wordsign reads adjacentclassification/highbytes literally,andfinal N/Z canreflect the16-bit statusload evenafter SEP20.

C03F08 clears pendingclassificationbit08,retains the classification's sign throughPHP/PLP andselects statusprogram1 forheroes or2 forothers. C10024 writes the returnPC at150+(2*depth modulo256),reads the requested D04ED3 pointer,andadvances depth. At a newdepth>=16,it instead restarts fromthe actoridentity after the returnPCwrite;that write can alias the identity. Depth255 wraps tozero withoutrestart. A/Y/X andmultiplicationresidue follow the actual path,then the consumer reloadsX fromDP85. This updates programstate;later VM execution isnot claimed here.

The newclass's death_status usesgeneralapplication,and1024 complete death plus1024 complete timercalls verify zero/same/randomaddedstatus,zero/0800/randomimmunity andzero/same cachedstatus. The earlier death-only adapter andits report remainhistorical evidence;the generalcomposition adds coverage withoutrecounting thoseinstruction bytes.

Proof domains: D0300/DB7E/X16/binary,apply/appearance/consumersM8,recoveryM16,twelvealignedactor slots,RNGindices0..14,queuecount0..12/tail0..11 andcontrollerbindings0..4. Corehas sixclassification/eight spritecontexts,15statusmasks,all256levelbytes andsampled actual equipment. Statusconsumers cover all256refresh/classification/depthbytes,current0/4160/5760/8000/FFFF andprevious0/8000/FFFF;statusprogramAPI covers all256 commandlowbytes withhigh00/AB. Death/timer usevalidhero/ordinary classifications andweaponlevels0..8,heroXPlevel98,HP0/1/2/FFFF andtimer0/1/2/4/FF. Arbitraryframes/index8/unalignedactors,hardware timing andwholecombat remainopen. NoROM/dialogue/asset payload isstored.

## Pending HP/MP damage and impact reactions — 2026-10-09

`tools/combat_pending_damage.py` reconstructs nine original routines: C04004 pending HP/MP application, C04157 impact sound, and C04E6B..C04F7A miss/death/damage/recovery motion helpers. All 5,120 complete original calls agree on full WRAM outside 129 stack bytes, CPU, static peripherals and ordered audio requests, including actual death, general status and RNG callees. A directed MP recovery overflow case exercises the native clamp to 99 before max-MP comparison.

Nine new independently matching sources add **643 instruction bytes**, taking instructions from **74,664 to 75,307** and all matching sources from77,214 to77,857 (numeric data remains2,550). Two contiguous components cover371 and272 bytes. Preserve HP-healing priority, 999/99 caps, wrapping HP addition, barrier bypass for negative last-hit flags, byte-only MP operations, and busy-motion gates. Main combat dispatch, physical hit resolution and counter publication remain open. No complete-game percentage is established.

## Complete physical accuracy, element and weapon status contracts — 2026-10-09

`tools/combat_physical_accuracy.py:PhysicalAccuracy` now reconstructs the complete CPU/memory/arithmetic-port contracts of C04FED defence, C0505B accuracy, C050CA element/Saber expiry, and C05119 weapon status-on-hit. `data/combat_physical_accuracy.json` verifies8,192 complete original calls, including real RNG, stat rebuilding and expiration notices; no bypass hooks. Full randomized WRAM outside129 stack bytes, CPU/S/D/DB and static peripherals agree. D0300/DB7E/M8/X16/binary,12 aligned slots including attacker=defender, RNG indices0..14 and valid message queue boundaries. Defence boundaries, all256 evasion/accuracy/status/chance/Saber count values, gauge0/30, both hit outcomes and6 stat classifications are included.

C05119 adds **50 new matching instruction bytes**. The other300 instruction bytes were already matching sources and receive no new byte credit. Instructions increase **75,307→75,357**, total matching sources77,857→77,907; numeric data stays2,550. The complete physical damage calculation, C04F7B dispatcher and C03A79 combat frame remain open. Whole-game code percentage is unknown.

The status-on-hit routine removes low-status bit10 before testing weapon status against immunity. A single shared bit rejects the entire word, without a random draw. Otherwise `floor(random_byte*100/256) < attacker.E1F7` and nonzero defender.E1F1 permit replacing DP9A with attacker.E199; rejected paths preserve existing DP9A. BIT memory preserves its operand-derived N/V through paths that perform no arithmetic. This producer does not itself apply queued status; the later general status call consumes it.

Element selection tests the defender's own E194 word against its resistance, then weakness mask, yielding1/4/2. Saber count is read from attacker.Y; zero returns unchanged, counts above1 decrement, and1→0 clears the low nibble of E1AE before complete stat rebuilding and the actual buff-expiration message. The rebuild/notice effects survive restoration of incoming X/Y; attacker refresh bit04 is then set. No rendered sound/video or full-frame claim is made.

## Complete physical-hit dispatch and damage contracts — 2026-10-09

`tools/combat_physical_damage.py:PhysicalDamage` closes C04F7B pending-hit dispatch and the complete CPU/memory/hardware contracts of C0514B damage queueing, C051E7 attack roll, C052E7 defence roll, C0532C critical/gauge processing, C0541B barrier absorption, C0543B byte-word percentage arithmetic, C054B6 byte-word multiplication and C05990 critical notice. `data/combat_physical_damage.json` verifies10,240 complete original calls (2048 dispatcher plus8×1024 helpers), with real accuracy/element/Saber/status/RNG/message/miss/audio callees and no bypass hooks. Full randomized WRAM outside129 stack bytes, CPU/S/D/DB and static peripherals agree.

New matching instruction sources C04F7B(114) and C05990(13) add **127 bytes**, taking instructions **75,357→75,484** and all matching sources77,907→78,034. The916 existing arithmetic/damage source bytes receive no duplicate credit. Numeric data remains2,550. Both new components match independently; whole-ROM rebuild and whole-game code percentage remain incomplete/unknown.

Domain: D0300/DB7E/M8/X16/binary,12 aligned slots including attacker=defender, valid appearance/level inputs, RNG0..14, message count0..12 and forward/reverse cursors0..11. Dispatch covers all256 pending-hit masks, HP0/1/999, current status0/4/16/8000, immunity0/8000, initial cached/added status0 and actual weapon-produced status. Helpers cover boundary defence, all256 attack/stage/agility/bonus values, gauges0/1/30/255, elemental multipliers1/2/4, barrier, drain and inverted-drain paths. Zero multiplier's65536-iteration loop is implemented but not included in this proof. Continuous hit-to-HP application, C03A79 combat tick, alternate status contexts and live timing remain open.

Dispatch consumes only the first three bits of E059, then clears that byte. Dead HP, secondary-condition bit04 and an initially empty mask return before the status call. Otherwise target class determines attacker slots: ordinary targets use0/1/2; negative-class targets use3/4/5. Zero-HP attackers are skipped. An accurate hit stores its attacker index in E1F0 before element selection, damage queueing and status production. General C8E301 status processing occurs after the three-bit scan.

Critical notices allocate a reverse message slot via DP A2 rather than the ordinary tail. Critical processing reloads X/Y from DP85/83 after this notice. Forced high-nibble weapon bonuses decrement by10(hex) and guarantee doubling; ordinary critical probability wraps as a byte. Gauge processing builds a24-bit product and performs two native division steps, preserving overlapping scratch bytes and arithmetic-port quotient/remainder state. Division by zero follows the hardware result (quotientFFFF/remainder dividend).

Damage subtraction branches on the wrapped signed result. A nonpositive result selects miss motion and can install a zero-damage D0 counter for a nonhero only when pending damage and the counter are both zero. Positive damage wraps into16-bit pending totals. Drain bit40 at attacker.E195 adds target damage and attacker healing; target type bit20 reverses those recipients and writes the last-attacker field on the attacker. This stage queues damage; C04004 applies HP/MP changes later.

## Complete actor combat tick and public entry — 2026-10-09

`tools/combat_actor_tick.py:ActorTick` reconstructs the complete C03A79 actor tick, its A-lowFF initialization continuation C03A3A, and the public/internal jumps C0002D/C03815. `data/combat_actor_tick.json` passes4,096 complete original calls (1366 direct,1365 public,1365 internal), without bypass hooks. Real status, pending physical hits, death/status/buff timers, HP/MP damage/healing, stat rebuild, status-program setup, controller messages and audio callees execute. Full randomized WRAM outside129 stack bytes, CPU/S/D/DB/static peripherals and ordered sound requests agree. Both accumulator widths, X16/DB7E/binary,12 aligned slots through E000 aliases, incoming D0/0300/1234 and defined valid appearance/level/RNG/message-queue domains are checked.

Four new matching source blocks add **485 instruction bytes**:416 tick,63 initialization continuation and6 jump bytes. Instructions increase **75,484→75,969**; total matching sources78,034→78,519, still including2,550 numeric data bytes. The479-byte combined initialization/tick and both3-byte jump components match independently. Documented ROM coverage is a separate38.6302% metric and does not represent complete-code decompilation.

All5 low-byte action codes are observed. Twelve fixture families include held-action andEA last-attacker gates, incoming controller setup, cooldown expiry, opacity delay, pending lethal HP damage, status/phase timers, motion/counter ordering, refresh and hero notification. Directed normal/dead recovery and hero/nonhero sleep cases preserve equal cached status and disable immunity for the dead branch. General tests also include immunity stripping death; those take a different return path. Whole actor scheduling and consumption/publication of returned requests, continuous multi-frame simulation, alternate actor/index/bank domains and live timing remain open.

The tick establishes DP0300 and X=`incomingX & 1FFF`, derives DP81 from the actor index, then sequences general status → physical hits → timers → general status → pending HP/MP application → status-program dispatch. Normal entries clear DP96 before this work. The FF initialization continuation keeps DP96 and binds controller1 to hero0 only when all three input-selection bytes D9/DA/DB are zero. C03A3A requires the already saved tick prologue and is not a standalone-call ABI.

Return low-byte actions:0 means no new request;1 returns counter type in X and displayed amount in Y;2 returns motion in A high byte (some held-action releases return0002);3 reports the hero change flag;4 returns refresh flags in X. The high accumulator byte and scratch residues follow original execution, rather than being normalized. The return restores incoming C/V/width/D/DB, while final N/Z reflect restored16-bit D because PLD runs after PLP.

E060==20 increments the opacity delay; exactly40 clears both state and delay. Other E060 states clear the delay immediately. E1F8 cooldown decrements, clears the pending-hit mask only on expiry and returns before combat calls. Held class bits36 similarly bypass normal combat. In the normal path, motion wins over counter publication unless status restrictions discard the motion; motion consumption sets E1E5 toFF. Pending counter publication clears E175 and returns its value/amount. Death/status transition masks and recovery/sleep requests precede refresh and the final hero notification. The caller still has to consume these returned actions; this proof does not claim actor pool scheduling or rendering.

## Boss combat request consumption and native nine-slot pass — 2026-10-09

`tools/boss_combat_requests.py:CombatRequests` reconstructs C20F27 through actual secondary/primary contact marking, complete C0002D combat, motion reaction and018015 floating-number creation. `data/boss_combat_requests.json` verifies1,024 complete original calls with full randomized WRAM outside129 stack bytes, CPU/S/D/DB, static video/DMA/hardware and ordered sound requests. All5 action codes, motions8/9/12/13, four counter types, number states0/40, F1-negative guard and displays0/9/49/50/199/200/999/FFFF are included. Nine boss slots skip the hero-number HUD/NMI wait, rather than bypassing it. Actual number graphics/palette/decimal/tile callees execute;128 sound requests are observed.

`tools/boss_actor_pool.py:ActorPool` reconstructs C20DB2, the actual native nine-slot update loop. `data/boss_actor_pool.json` verifies512 complete original passes, one for every active-slot mask, through cache markers, relative geometry, contact/combat/counter processing, original definition7456 states0..2 and animation/snapshot helpers. Phase0..3, flags2000/4000, both freeze bits, terminal/nonterminal definitions, party active/inactive and a pending counter are covered. Full memory/peripheral/CPU/audio comparisons agree. This closes traversal with this recovered definition; other definition targets and dynamic callback inventory remain incomplete.

Two new independently matching sources add **194 instruction bytes** (50 request consumer +144 pool), taking instructions **75,969→76,163** and all matching sources78,519→78,713. Numeric data stays2,550;1,276 blocks/238 components match. Existing contact/combat/graphics/animation source bytes are reused without duplicate credit. Whole-game code percentage is unknown. Ordinary actor request consumption C18089, all definition targets, continuous game scheduling and live rendering/timing remain open.

The boss consumer stores the complete combat result in DP0256 before masking the low byte. Action1 saves counter type in DP0246 and amount in DP024E, restores the boss actor offset and calls018015. Action2 passes the saved high-byte motion to C20F59, which reacts only to8 or9; actions0/3/4 return without additional publication. A negative global F1 suppresses number creation inside the real public wrapper. Static number records and graphic planes are checked, not later screen presentation.

The pool resets0E69 and DP02CA, captures low-byte0056 as phase02C9 and reloads graphics cache markers only at phase0. It scans active bytes0E60..0E68 in order, deriving actor E600+512*slot and frame cache F800+128*slot. Flag4000 bypasses the two-bit phase match; flag2000 bypasses combat and its freeze tests but still executes the definition. For other actors, nonzero definition state0094 enables the CFFF-bit1/1D04-bit40 freeze tests; initial state0 ignores them. Successful definitions proceed to animation/snapshot. Carry-set termination clears the actor's word0000 and008C, then the slot's active byte. Inactive/skipped slots retain their state. The pass ends with X9, M16 and the final comparison's flags.

## Actor motion and effect cleanup — 2026-10-09

`tools/actor_motion_cleanup.py:MotionCleanup` reconstructs the public motion jump C1800C/body C18052, interrupt-charge helper C1CA58, movement/charge/effect cleanup C1CA6C, palette restoration C1CAAC, gauge reset C1CACD and pre-refresh graphic-state clearing C194EA. `data/actor_motion_cleanup.json` verifies7,168 complete original calls (1024 each), including real cached/weapon palette callees and no hooks. Full randomized WRAM outside129 stack bytes, CPU/S/D/DB/static peripherals and all affected software palette words agree.

Seven independently matching sources add **210 instruction bytes**; instructions increase **76,163→76,373**, total matching sources78,713→78,923, numeric data unchanged2,550. Four components (3-byte jump,29-byte public body,143-byte cleanup group,35-byte graphic cleanup) match. The87 existing palette dependency bytes receive no duplicate credit.

Domain: D0/DB7E/X16/binary,12 aligned actors,internal M8 and both A widths for the public motion API. All256 global5C/agility/motion-state/effect flags/palette ids, zero/nonzero charge pairs and16 valid palette cursors are included. The full ordinary consumerC18089 still requires graphics/status refreshC1939C and its controller/party dependencies. Physical palette publication, arbitrary index/bank domains and continuous live frames remain open; global code-decompilation percentage is unknown.

Charge cancellation clears byte fieldsE01A/E01B/E19B/E063 and sets E1ED to `(((100 - agility) & FF) >> 1) + 50`. The subtraction is unsigned byte arithmetic, so agility above100 wraps instead of clamping. C1CA58 cancels only a negative E01C, unless global5C is negative and actorX>=0600. C1CA6C uses that same deferred-boss gate: it clears only low bytesE104/E100 and returns, preserving adjacent bytes and movement fields. Other actors cancel an active charge, restore effects and clear movement/transient flags, ending with E030=FF.

C1CAAC restores fifteen cached actor palette words when E100 is negative, except for deferred boss slots. Its independent negative-E104 test still restores the five weapon palette words even for a deferred slot. Runtime ROM reads and multiplier residue are preserved; no palette payload is saved. C18052 stores the requested motion byte, performs cleanup, sets E01C=40/E064=80 and restores original Y/P. C194EA performs cleanup then clears motion/graphic-state bytes, retaining the accumulator and flags of its E060 load. These are the actual dependencies used by the ordinary combat consumer and graphics refresh, rather than substituting a new movement system.

## Actor graphics resource refresh — 2026-10-09

`actor_graphics_resources.py` reconstructs C0E9F4 (JSR EA19/RTL) and C0E9F8 (saved-bank three-party refresh), adding37 matching instruction bytes. Its5,888 complete original calls also finish full CPU/memory/peripheral contracts for389 existing descriptor/palette/resource/refresh bytes. C1DFD3 loads eight D00000 words at low identity*16, copies the literal unaligned E077 word to E065 and builds E080/E082 with ASL6 and LSR2 bank selection. C1E022 uses special transformed hero palette E7+Xhigh/2, otherwise low identity and a low-byte E100 clear; it copies fifteen colors to the working and cached palettes. C0EA41 preserves native software arithmetic flags and pointer scratch; C0EA19 executes real plane conversion and five-color weapon palette loading. C0E9F8 refreshes slots0000/0200/0400 under DB7E and restores the original bank.

Models compare full randomized memory outside caller stack, final CPU/S/D/DB and static peripherals without bypass hooks. Both A widths, twelve aligned actor slots, all low identity/weapon values, high weapon byte00/80/FF, transformation flags and valid palette cursors are sampled; saved party DB00/7E/7F/42 is covered. Full C1939C graphics/status coordination and its program/controller/gameover dependencies remain open. No live rendering or whole-game decompilation percentage is claimed.

## Program initialization and hero charge gauge — 2026-10-09

`actor_program_cpu.py:ProgramInitialize` completes original C10020/C125B6/C12599 CPU/memory/hardware behaviour, reusing167 earlier instruction bytes. C125B6 initializes PC/wait/counter/flags/depth and command state, inserts status program1 only for full-word ids128..130 with statusmask5760, then C12599 rebuilds stats only for lowbyte128..130. High program bytes therefore change continuation without changing the rebuild predicate. Registers follow actual status/rebuild branches; the stack-local frame returns the helper's A, restores C/V/M/X and ends with NZ from incoming D.

New C2B01E (53 instructions) clears085/01A/01B/19B byte fields and, only for hero offsets below0600 with zero1ED, sets gauge2817 and clears the literal overlapping wordsA50/A52/A53 at eight-byte hero offsets. Its8,192-call combined proof compares full randomized WRAM outside caller stack, CPU/S/D/DB and peripherals, without hooks and with real status/stat callees. Initializer D0/300/1234,DB7E/42,both A widths andprogramhigh00/01/AB/FF are covered; charge helper M8/D0/DB7E,12actors,zero/nonzero delay. Full C1939C coordinator still needs portrait/controller/gameover composition; no live frame or full-game percentage claim.

## Party portrait refresh boundary — 2026-10-09

`actor_party_portraits.py:PartyPortrait` closes C1E6A8 public wrapper/C1E6AC EC2 wait and the complete party icon path. The supplied NMI runs the original handler throughRTI at the first wait BIT; the readable model composes the established NMI and three-icon/tile models. WithDB00 the twelve packed tiles reachVRAM; withDB7E the unbanked stores instead modify WRAM offsets2115/2116/4370/4372/4374/4375/420B. C1939C saves its bank and setsDB00 for the portrait call. This distinction is preserved rather than assuming every absolute store reaches the graphics ports.

Two new sources add14 matching instructions,157 existing icon bytes reused without credit. Proof2048 completecalls compares fullWRAM outside129callerstackbytes,CPU/S/D/DB andstaticperipherals/video/DMA; all256identitybytes inthree slots,DB00/7E,bothAwidths,D0/X16/binary. NMIforcedblankE6=80,EC2/menu/restoredisabled; live timing/vector delivery/allframeupload modes remainopen. Controller reassignment andgameovercomposition stillneeded forfullC1939C.

## Controller reassignment and HUD — 2026-10-09

`actor_controller_assignment.py:ControllerAssignment` reconstructs C0BDBB saved-bank entry,C0BDC5 complete reassignment/HUD and C0BDD0 choice/release, with hero release entriesC0BD91/BD98/BD9F/BDA4 and eligibilityC2B59D/B5C9. Negative controller flags suppress reassignment; charge/motion protects an existing assignment and the sole controller cannot be released. Empty controllers scan unbound active/unrestricted heroes first, then unbound living heroes; restrictive status41/60 assigns the deferred E08C binding rather than E02C. Release clears both and calls realC10020. Native binding4 charge guard uses literal0800 actor offset, whereas release chooses0400 via the binding bit scan.

C0BF38 waits onEC2 through the realnativeNMI whenCF00 is enabled; C0BF73 writes VRAM lowbyte symbol21/22/23 for controllers or16 whenunbound. Eleven sources461 new instructions match. Proof5632completecalls/672nativeNMIs compares full randomizedWRAM outside129 stackbytes,CPU/S/D/DB andallstaticvideo/peripherals; actual initializer/status/statcallees run,nohooks. DomainD0/M8/binary,controllerX8 ids0..2,heroX16 offsets0/200/400,HUDDB00/eligibilityDB7E,publicsavedDB00/7E/7F/42,forcedblankNMIEC2/menu/restoredisabled. Everyhero selected under directed unrestricted/restricted fixtures; otherstatus/guard/HUD combinations sampled. FullC1939C/gameovercomposition,arbitraryinvalidbinding/index/bankdomains andlive interrupttiming remainopen.

## Complete actor graphic/status refresh — 2026-10-09

`actor_graphics_status_refresh.py:ActorRefresh` reconstructs C1939C..C194E9 (334instructions). Lowmask1 resets state/program andreloads hero portrait/palette/descriptor/resources;4 updates weapon graphics/palette;2 interprets the savedY statusdifference. Death clears motion/charge, releases/reassigns the controller andrequests numeric7FF onlywhenallthree hero active/death tests fail. Other status changes refresh palettes, defer currentbindings under41/60, restart actorprograms and restore eligible bindings/controller bytes. Calls retain the native bank switches, savedY/A bytes, lowbyte clears andfinalCPU semantics.

C1E76D numericcommands0..7FF nowhave fullCPU/peripheral/memory reconstruction forbusy/deferred/started paths, including the realnew-event actorreset. Commands800+ andsubsequentgameover eventexecution remainopen inthisclass;219existingnumeric wrapper/request/scratch/reset bytes receive nofreshcredit. Proof4096completecalls/168nativeNMIs compares fullWRAM outside129callerstackbytes,CPU/S/D/DB/staticvideo/peripherals,nohooks,actual helper callees. CoordinatorD0/DB7E/M8/X16/binary,12alignedactors,all8lowmasks,status/death/restrict/binding contexts;numericentrybothM/DB00/7E/42. NativeNMIforcedblankEC2/menu/restoredisabled. Nextfullordinaryactor consumerC18089 andbroader scheduling/liveframes.

## Complete ordinary combat request consumption — 2026-10-09

`actor_combat_consumer.py:ActorConsumer` closesC18089(133instructions),includingscene/eventguards andrealC0002Dtick. Return0 restoresactor;1 publishescountertypeX/amountY andexecutesC1810E fullnumbercreation;2 setsAhigh|80 motion,interruptscharge andresetsreactionfields;3 runsC1E326 HUDwait;4 convertsreturnedX intoC1939C graphic/status/controller refreshflags. Alloriginalcalleesexecute,includingrealnumbergraphics/palettes/decimaltiles,nonmusicAPUpackets andnativeNMIhandler throughRTI.

Proof2048completecalls coversallfiveactualactioncodes,342nativeNMIs and204soundpackets. FullrandomizedWRAM outside129stackbytes,CPU/S/D/DB/allstaticvideo/arithmetic/orderedaudio match,nohooks. D0/DB7E/X16/binary,bothincomingAwidths,12alignedactors,counterstates0/40,fourcountertypes/display0..FFFF,validtilecache/palette destinations,RNG0..14/queues0..12/cursors0..11 andsampled52/FF/F1/E8/event/EDguards. NMIforcedblankEC2/menu/restoredisabled; APUnonmusicackdelay0..3. Ordinaryactor poolscheduling,continuousgameframes andliveaudio/interrupttiming remainopen.
