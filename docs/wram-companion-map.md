# WRAM map for the dual-screen companion (stage 1 of board item T4)

Date: 2026-10-17. Machine-readable twin: `data/wram_companion_map.json` (address, size, meaning, evidence and a
verification status per ROM for every field). Code that consumes it: `tools/companion/som_companion.c`.

The companion reads WRAM through the libretro memory API (snes9x; bsnes exposes none) and injects button presses.
It never writes WRAM or patches the ROM. Addresses are offsets in the 128 KiB WRAM image (`$7E:xxxx` = `xxxx`).

## Method

Every field below was checked by a mutation: change game state through real inputs (or the game's own event opcodes
and pending-damage fields), run frames, read the field back, and compare with what the game shows (HUD, ring title
bar, status page screenshots). Identical-ROM comparisons were not used. Base state: the TASVideos-derived input route
of `tools/fidelity_d_inputs/hhs_glitch_bk2_s9x14.txt` to frame 23,000 (Potos village, hero with the Rusty Sword,
map 131), then a fixture (`tools/companion/build_fixture.py`): girl and sprite activated by event opcode `0x2B`,
HP refilled by `0x2F`, dead flag cleared, and items / weapons / spirit masks poked (listed in the prototype document).
The poked values are test inputs; the checks read the game's reaction to them.

Verification legend: V verified by mutation, P partial (one data point or documentation only), U not exercised,
`-` not obtainable. Columns: vanilla US, Magno Spanish translation, Ancient Cave.

## Party (hero record `$E000 + 0x200 * slot`, slot 0 boy, 1 girl, 2 sprite)

| Field | Address | Size | Meaning | US | ES | AC |
| --- | --- | ---: | --- | :-: | :-: | :-: |
| active | `+0x000` | 1 | 1 = in the party; 0x80 after event `0x2C` removal | V | V | U |
| object id | `+0x1E7` | 1 | 0x80 / 0x81 / 0x82 | V | V | U |
| level | `+0x181` | 1 | internal, display = +1 | P | U | U |
| HP, max HP | `+0x182`, `+0x184` | 2+2 | pending damage 7 took 50 to 43; Candy macro 20 to 50 with Royal Jam; HUD shows the same numbers | V | V | U |
| MP, max MP | `+0x186`, `+0x187` | 1+1 | cast macro 8 to 4; pending MP damage 2 | V (max P) | V (max P) | U |
| status word | `+0x190` | 2 | bit 15 = dead / uninitialised (hero skipped by the owner cycle); 0x2000 = 1 HP per timer drop, timer `+0x1B6` | V | V | U |
| weapon recharge | `+0x1ED` | 1 | 0 = ready; about 0x55 on a swing; HUD % = 100 - v*100/85 (0x22 = 60 %, 0x17 = 73 %, 0x0F = 82 %, 0x07 = 91 %) | V | U | U |
| power-up charge level | `+0x19B`, `$E01B`, `$E085` | 1 | documented; could not be provoked in the synthetic state | U | U | U |
| weapon type, row | `+0x1E4`, `+0x1E8` | 1+1 | type 0..7 (sword 1, axe 2, spear 3, javelin 7); row = type*9 + tier | V | V | U |
| weapon levels | `+0x1C0` | 4 | eight nibbles, even type = high nibble | U | U | U |
| spirit levels | `+0x1C4` | 4 | eight nibbles; the spell level of a cast equals the nibble for bytes 0..5 | P | U | U |
| spirit masks | `+0x1C8` | 8 | non-zero byte lists the spirit; 0x3F gives 3 spells | V | V | U |

Spirit ring order is byte order: Gnome, Undine, Salamando, Sylphid, Luna, Dryad, Lumina, Shade (names read off ring
title bars; the Lumina byte 6 is never listed for the girl and Shade byte 7 never for the sprite; the boy has no
magic ring). Spell id of a girl cast is `6 * ring_index + 3 + spell_slot`.

## Inventory

| Field | Address | Meaning | US | ES | AC |
| --- | --- | --- | :-: | :-: | :-: |
| weapons owned | `$CC54 + type` (8) | 0xFF = not owned, low bits tier, bit 6 = equipped by the boy (axe 0x02 to 0x42 on equip) | V | V | U |
| consumables | `$CC48..$CC53` (12 records) | 0xFF empty, else `(qty << 5) \| id`; ids 0 Candy, 1 Chocolate, 2 Royal Jam, 3 Faerie Walnut, 4 Medical Herb, 5 Cup of Wishes, 6 Magic Rope, 7 Flammie Drum, 8 Moogle Belt, 9 Midge Mallet, 10 Barrel; the item ring lists the non-empty records in record order | V | V | U |
| gold | `$CC6A` (3) | 24 bit; status page shows 100GP for 0x64 | P | U | U |
| names | `$CC00 + 12 * slot` | glyph codes, not decoded | U | U | U |

## Ring menu, modes, map

| Field | Address | Meaning | US | ES | AC |
| --- | --- | --- | :-: | :-: | :-: |
| ring open | `$007D` | 1 while the ring, status page, spell sub-ring or target selection is open | V | V | U |
| ring owner | `$1808` | hero slot shown; X cycles to the next living hero not bound to a pad | V | V | U |
| category | `$1809` | 0 item, 1 magic, 2 config, 3 weapon; UP: weapon, item, magic (skipped when empty), config; 0x55 before the first ring | V | V | U |
| cursor | `$180A` | LEFT = +1, RIGHT = -1 (mod count), reflected about 9 frames after the press | V | V | U |
| entry count | `$180B` | 7 spirits, 3 spells, item records, owned weapon types, 7 config entries | V | V | U |
| busy | `$1812` | non-zero (frames 8..43 after Y) while the ring opens; input is ignored | V | V | U |
| ring enable | `$CF00` | story flag, ring menus enabled | P | P | U |
| event state | `$00D0` | non-zero while the event VM runs (dialogue, cutscene, warp) | V | V | U |
| fade | `$00E4`, `$00E6` | E4 = 1 and E6 = 0x0F when fully visible | V | V | U |
| map id | `$00DC` | 131 Potos village, 128 falls, 256 opening room, 257 house, 179 prologue (matches `docs/maps.md`) | V | V | U |
| pad binding | `$00D9` | bit n = hero n bound to pad 1 | P | P | U |

Config ring entries (cursor index to function, boy ring): 0 Equip Armor, 1 Targeting, 2 Status, 3 Weapon/Magic Level,
4 Action Grid, 5 Controller Edit (button remap, not hero switching), 6 Window Edit.

### Game mode for injection safety

`NO_GAME` if hero 0 active != 1; `TRANSITION` if `$E6 != 0x0F` or `$E4 != 1`; `EVENT` if `$D0 != 0`; `RING` if
`$7D == 1`; otherwise `FIELD`. Injection is safe only in `FIELD` with a living hero bound to pad 1. Observed:
dialogue and cutscenes have `$D0` = 0x81..0x86 (events 0x300, 0x600, 0xDB), warps set `$D0` = 1 and fade `$E6` down to 2,
the status page keeps `$7D` = 1. There is no flag for the cast animation after the ring closes (about 120 to 150 frames
until MP is paid); the macro verifies by the MP drop. There is no separate battle mode: monsters live in the field.

### Ring navigation model (measured frames at 60 fps)

1. Open: tap Y (3 frames). `$7D` = 1 two frames later; ready when `$1812` = 0 and `$1809 != 0x55`, about frame 43.
2. Owner: tap X until `$1808` = hero (about 80 frames per step; up to 2 taps for the sprite).
3. Category: tap UP until `$1809` matches (about 40 frames per step).
4. Cursor: tap LEFT or RIGHT along the shorter way, 22 frames per tap, until `$180A` = index. Index of a weapon = rank
   among owned types; of an item = rank among non-empty records; of a spirit = rank among non-zero mask bytes.
5. Confirm with B. Weapon: one B equips and closes. Item: B selects, a second B (target = bound hero) uses it.
   Magic: B on the spirit opens the 3-entry spell ring (`$180B` 7 to 3), cursor to the spell, B selects, B on the
   target, ring closes, MP is paid later.
6. Totals measured by the macros: equip 240 frames, use item 255 to 305, cast as the girl 492, as the sprite 650.
   Recovery: the macro taps A, A, Y repeatedly until `$7D` = 0 (the ring closed in every tested case; which of the
   two buttons does it at each depth was not separated).
Unusable choices (an item the game rejects such as Cup of Wishes on a healthy hero, or an attack spell with no enemy in
range) leave the ring open; the macro times out and closes it.

## HUD and rings: what draws them

Vanilla US, 256x224 frame, measured by toggling the snes9x core layers and diffing frames
(`snes9x_layer_1..5`): the HUD occupies rows 199..216, x 8..247. Its frames, HP numbers and gauge are BG3 (layer 3); the
portraits are sprites in the same rows. BG3 also carries the dialogue windows and the ring title bar. Ring icons are
sprites (layer 5). BG1 and BG2 are the map, BG4 is unused. Cropping rows >= 199 removes the HUD with no core support
(used by the host). Layer toggles are all-or-nothing and would also hide dialogue or every actor. Not found: the OAM
index ranges of the HUD sprites, shadow copies of `$212C/$212D` (TM/TS), and a HUD-visible flag (libretro exposes no
OAM or PPU registers). In the Magno early state the HUD is not drawn yet, so its rows could not be compared there.

## Ancient Cave (T5)

Ancient Cave is a mode of Mop's Secret of Mana Randomizer (`github.com/Moppu/SecretOfManaRandomizer`, LGPL-2.1, version
1.49 of 2026-02-13; the project was named "Secret of Mana Ancient Cave" before). There is no IPS/BPS patch: the Windows
.NET application writes a 4 MiB ROM from the user's ROM. Documentation on WRAM or maps is not published (the README
only lists the modes and says Ancient Cave "needs work"). What was done:

- Downloaded the release (a single .exe) into the state directory and built a console harness
  (`tools/companion/ancientcave_gen`, calls the app's own `RomGenerator` with the Ancient Cave settings; the app is
  not redistributed). It generated a ROM from the vanilla image (4,194,304 bytes, header unchanged: HiROM, size byte 0x0B).
- Static comparison with vanilla: 159 differing regions in the first 2 MiB. Bank C0 has 45 hook sites that replace
  instructions by `JSL $E0xxxx` (new code in the expansion at `$20B8F3+`), among them the ring poll (`C0:6559`,
  `C0:657E`), the menu loop request (`C0:692F`), ring phase (`C0:6D1E`), inventory (`C0:7AC3`), prices (`C0:7C8C`)
  and a six-byte table at `C0:57B7` (`03 03 03 0F 0F 0F` zeroed, purpose not determined).
- Runtime: in the headless harness the new-game flow did not get past the name entry (rendering of that screen is
  garbled and START cycles back), so no field could be verified. Every Ancient Cave cell above is therefore U.
  Compatibility is not claimed. Next step: boot the generated ROM interactively (or find the right input script),
  save a state in the first floor and run `SOM_TEST_ROM` style checks on it.

## Open items

Power-up charge level; meaning of each mask bit (which spell); weapon and spirit level ranges; equip marks for the
girl and sprite in `$CC54+`; HUD OAM ranges and TM/TS shadows; Ancient Cave runtime verification; Magno HUD rows.
