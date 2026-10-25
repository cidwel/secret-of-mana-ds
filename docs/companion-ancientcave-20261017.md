# Ancient Cave compatibility of the companion (board item T5)

Date: 2026-10-17. Code: `tools/companion/ancientcave/`. Data: `data/companion_ancientcave_20261017.json` (full report),
`data/wram_companion_map_ancientcave.json` (sibling of `data/wram_companion_map.json` with the Ancient Cave column filled; the
original is untouched). Background: `docs/wram-companion-map.md`, `docs/companion-prototype-20261017.md`.

The ROM is generated outside the repository with the randomizer's own code (`tools/companion/ancientcave_gen`, Moppu/SecretOfManaRandomizer
v1.49, LGPL-2.1) and read from `$SOM_COMPANION_STATE/ancientcave/out/`. No ROM bytes, states or screenshots are stored in the repository.

## Result in numbers

| Item | Measured |
| --- | --- |
| Boot to free play | reached headless on seed 1 and seed 777, 3,044 frames from reset |
| Map fields (34) verified by mutation or exact observation | 29 verified, 2 partial, 3 not testable (85.29 % verified) |
| Fields that moved to another WRAM address | 0 |
| Same verdicts on a second generated ROM (seed 777) | 34 of 34 |
| Macro and status tests (`test_ancientcave.py`) | 16 of 16 pass on seed 1; 15 pass and 1 skipped (equip matrix, run only on seed 1) on seed 777 |
| Use item / cast magic | work unchanged (Candy 242 frames, Royal Jam 292, girl cast 479, sprite cast 637) |
| Equip weapon, repository host | 7 of 64 (held weapon, wanted weapon) pairs correct: two latent defects (below) |
| Equip weapon, patched copy of the host | 64 of 64 pairs correct |

## 1. Getting past the name entry

The earlier stall was not a rendering fault of the hack. The name entry ignores START when the typed name equals another hero's name, and
the old script typed "A" for all three heroes, so the sprite screen never accepted. With distinct names the game proceeds directly:

1. frame 300 tap START (title); 410 frames later tap B (NEW GAME); 300 frames later the boy name screen.
2. Boy: B once ("A"), START. Girl: RIGHT once, B ("B"), START. Sprite: RIGHT twice, B ("C"), START. About 400 frames after each START.
3. Floor 1 appears with a four-box message ("Ancient Cave mode still needs a lot of work..."), event state `$D0` = 129..131. Tap B every
   60 frames until `$D0` = 0 three times in a row. The hero is then free on floor 1 (HUD "FLOOR 01", a clock, HP 50/50, 45/45, 40/40).

`tools/companion/ancientcave/build_ac_fixture.py` does this and saves `ac_base.state` and `ac_party.state` (base plus spirit masks 0x3F and
levels for girl and sprite, the only pokes). The fixture needs no WRAM pokes for the party, items or weapons: Ancient Cave starts with
three active heroes, MP 8 for girl and sprite, all eight weapon types, one of each consumable and 100 gold. Girl and sprite start with no
spirits (masks 0). Names typed are stored as glyphs 0x9B, 0x9C, 0x9D at `$CC00 + 12 * slot`.

## 2. Field-by-field result (`verify_fields.py`, `fields_results.json`)

Verdicts: V verified, P partial, N not testable here. All addresses are the same as in `docs/wram-companion-map.md`.

| Field | AC | What was measured |
| --- | :-: | --- |
| record base `$E000 + 0x200*slot`, active, object id | V | actives 1,1,1, ids 0x80..0x82 in HUD order; girl record set to 0 makes the ring owner cycle skip her |
| level `+0x181` | V | 6 written, status page shows LEVEL 7 (read by eye) |
| HP `+0x182` | V | pending damage 7 took 50 to 43; Candy and Royal Jam macros healed |
| max HP `+0x184` | V | derived: level 6 gives 108 and the status page shows 50/108; a direct write is not kept |
| MP `+0x186`, max MP `+0x187` | V | pending MP damage 2: 8 to 6; girl level 7 gives max 10 and the spell caption shows "08/10" (by eye); like max HP it is refreshed when the ring opens |
| status `+0x190`, timer `+0x1B6` | V, P | 0x2000 with timer 0x14: HP 50 to 48 in 120 frames, timer 0x08; bit 15 on the girl skips her in the owner cycle. Other timers `+0x1B2..` not exercised (P) |
| weapon recharge `+0x1ED` | V | one swing: 88, 78, 68 ... 0 every 10 frames; HUD percentage matches |
| weapon type / row `+0x1E4`, `+0x1E8` | V | all eight ring entries equip types 1..7, 0; row = type * 9 (tier 0) |
| weapon slots `$CC54 + type` | V | see section 4: 0x00 = owned tier 0, bits 7..6 = holder + 1 |
| spirit masks `+0x1C8` | V | girl magic ring lists 7 spirits with eight 0x3F masks, 6 with byte 2 zeroed |
| last spell `+0x170` | V | girl casts: 0x103, 0x209, 0x415 (low byte = 6 * ring index + 3 + slot, high byte = ring index + 1); sprite 2/5/2 gives 0x620 (low byte 0x20) |
| spirit levels `+0x1C4` | P | written, no visible effect isolated |
| weapon levels `+0x1C0`, charge level, armor slots | N | no effect found (skill page lists names only; B held 400 frames does not move `+0x19B`, `$E01B`, `$E085`; armor page not driven) |
| items `$CC48` | V | native inventory 0x20 0x21 0x22 0x23 0x24 0x25 0x27 0x26 (qty 1); ring entries follow record order |
| gold `$CC6A` | V | 100 at start; 1234 written shows MONEY 1234GP |
| names `$CC00` | V | A, B, C typed: 0x9B, 0x9C, 0x9D |
| ring open `$7D`, busy `$1812` | V | open two frames after Y; busy 5 from frame 7 to 41, then 0 (vanilla 8..43) |
| owner `$1808`, category `$1809`, cursor `$180A`, count `$180B` | V | owner cycle 0,1,2,1; UP order weapon, item, config (magic skipped for the boy); LEFT = +1, RIGHT = -1; counts 8, 8, 7, girl magic 7, spell ring 3 |
| ring enable `$CF00` | V | 1 at start; with 0 written Y does not open the ring |
| event `$D0`, fade `$E4/$E6`, map `$DC` | V | event 0 in free play, 129..131 in the start message; fade ramps through a door warp; map 0 on floor 1, 1 on other doors |
| pad binding `$D9` | V | 1, 2, 4 make the ring open on hero 0, 1, 2 and X skip the bound hero (vanilla was only partial) |

Evidence JSON and PNGs: `$SOM_COMPANION_STATE/evidence/ancientcave/` (`fields_results.json`, `macro_results.json`, `equip_matrix.json`,
`status_level6_gold1234.png`, `spell_caption_mp.png`, `hud_party.png`).

## 3. Macro tests (`test_ancientcave.py`)

| Test | Result |
| --- | --- |
| Use Candy (record 0, qty 1) | done, 242 frames, HP 20 to 50; the qty-1 record is removed and every later record moves up one place |
| Use Royal Jam (record 2) | done, 292 frames, HP 20 to 50 |
| Cup of Wishes on a healthy hero | fail-timeout, ring closed by the recovery, item kept (same as vanilla) |
| Record 6 ("Random Music") | done-unverified: no WRAM effect |
| Record 7 ("Stuck?") | fail-unsafe "transition": the item starts a fade, is not consumed |
| Cast, girl byte 0 slot 0 | done, 479 frames, MP 8 to 4 (owner switched with X) |
| Cast, sprite byte 5 slot 2 | done, 637 frames, MP 8 to 7 |
| Sprite byte 0 slot 0 | fail-timeout: an attack spell with no enemy (same limit as vanilla) |
| Refusals, abort, event during a macro, boot | all as in vanilla (fail-unsafe, fail-bad-argument, aborted, recovery closes the ring) |
| Equip, repository host, boy, sword held, wanted axe | equips the spear, result done-unverified |
| Equip matrix, 8 held x 8 wanted | repository host 7 of 64, patched host 64 of 64 |

### Two defects of the repository macro that Ancient Cave exposes (also present on vanilla)

Ancient Cave owns all eight weapon types, which vanilla fixtures never did, so the weapon ring is fully populated:

1. Ring order is types 1..7 and then 0 (glove last). `som_weapon_ring_index` ranks owned types in type order, so with the glove owned
   every rank is off by one. Measured on vanilla as well (fixture with all slots owned: cursor 0..7 equips 1, 2, 3, 4, 5, 6, 7, 0).
2. The cursor is relative to the held weapon. At open `$180A` holds the held weapon's rank, the first directional press resets it to 0
   (it does not move), and the ring position is `(cursor + held rank) mod n`. Measured on vanilla too (held spear at rank 2: reads 2 at open, then 0, 1, 2, 3 after successive LEFT presses).
   Equipping the held weapon leaves the ring open until the macro times out.

Proposed change to `tools/companion/som_companion.c` (not applied; the file is not owned by T5). `patched_host.py` builds a patched copy under
the state directory with exactly this and reproduces the 64 of 64 result:

- `som_weapon_ring_index`: iterate `order[] = {1, 2, 3, 4, 5, 6, 7, 0}` and count owned types before `type`.
- Equip, `P_CURSOR`: first tap LEFT once and wait about 22 frames (sync), then target cursor = `(rank(wanted) - rank(held)) mod count` using the
  hero's weapon type at macro start.
- Equip start: if the hero already holds the wanted type return `done` at once.

Not checked: the same rule for the girl and sprite rings (only the boy was driven), and whether the magic and item rings have the same
first-press behaviour (their tests passed from the initial cursor only).

## 4. What the hack changes that matters to the companion

Static comparison with the vanilla image (`make_report.py`): 323 regions (merged when closer than 16 bytes), 30,909 bytes differ in the first
2 MiB (452 in bank C0, 202 C1, 20 C2, 41 C3, 4,086 C6, 10,092 C8, 9,950 CC, others small); the second 2 MiB holds the new code and data
(320,088 non-zero bytes up to file offset 0x2532A4). Header bytes 0xFFC0..0xFFFF are identical to vanilla, including the size byte and the
unchanged checksum. The earlier "159 regions" figure used another merge rule and cannot be reproduced; the 45 hook sites are confirmed.

45 hook sites in bank C0 replace four bytes of vanilla code by `JSL $E0xxxx` (new code in the expansion). By function (names from
`data/functions.json`), grouped by what the companion reads:

| Area | Sites | Effect observed |
| --- | --- | --- |
| Menu ring | `C0:692F` request_event_menu_loop_and_initialize_input_state, `C0:6D1E` project_and_publish_all_native_menu_ring_rows, `C0:713C` set ring centre, `C0:1E08`, `C0:20B2`, `C0:245A` menu coordinator, scroll tick, page commands | ring state variables unchanged (`$7D`, `$1808..$180B`, `$1812`); item ring entries carry new text ("Stuck?", "Random Music", attack values after a weapon name) |
| Inventory | `C0:657E` insert_or_increment_inventory_record | records, ids and quantities as in vanilla layout; qty 1 consumed records compact the list |
| Shops | `C0:7AC3`, `C0:7C8C`, `C0:7E83` | prices and buy/sell dispatch; not exercised |
| Pad and charge | `C0:9440` button mappings, `C0:BD3D`, `C0:BD73` controller transfer, `C0:B717` pose from controller, `C0:B3DF`, `C0:EB5F` charge gauge | `$D9` binding behaves as documented; charge level still not found |
| HUD and sprites | `C0:9BF7`, `C0:9FA8`, `C0:E52D`, `C0:E543`, `C0:EA67` (UI sprite grids, actor sprite lists, weapon graphics) | HUD has a floor/clock row; BG3 HUD rows are 184..215 |
| Dialogue | `C0:17C1`, `C0:185F`, `C0:1931` | text decoding |
| Combat, actors, maps | the other 20 sites (area `combat_actor_map` in the JSON) | damage rolls, experience, drops, actor identity; not needed by the companion |

The six-byte table at `C0:57B7` (`03 03 03 0F 0F 0F` in vanilla) is zeroed; its purpose is not determined and no companion field depends on it.

Differences a companion has to honour (none moves a WRAM address):

- Map ids are generator numbering (floor 1 outside map = 0, other generated maps 1.., boss maps 256 + 2 * floor from the generator source);
  vanilla ids such as 131 never occur.
- Items: one of each consumable, qty 1, record order 0 1 2 3 4 5 7 6; id 7 shows as "Random Music" and id 6 as "Stuck?". The bottom screen
  must not use the vanilla names for these two ids. Using a qty-1 item shifts later records up.
- Weapons: slot byte 0x00 = owned tier 0 (0xFF "not owned" was not seen); bits 7..6 = holder + 1 (1 boy, 2 girl, 3 sprite), which also decodes
  the girl and sprite marks that were open in the vanilla map. Equipping a weapon held by another hero swaps weapons between them
  (boy takes the sprite's type 6, the sprite gets the sword; slots `0x40`/`0xC0` exchange).
- Max HP and max MP are derived by the game from the level and refreshed when a menu opens; the companion should only read them.
- HUD: the crop row for the top screen must be 184 on Ancient Cave (vanilla 199), otherwise the floor/clock line and the top of the party
  bars stay visible. The floor number was not located; `$CF1A` counts seconds (+1 per 60 frames), minutes and hours were not decoded.
- Name entry rejects duplicate names (matters only to automated new games).

## 5. Detecting the ROM variant and choosing address tables

No WRAM address differs, so the companion needs one address table plus a small per-variant behaviour record. Detection proposal:

1. Size: 2,097,152 bytes (or +512 for a copier header) is vanilla or a translation; 4,194,304 bytes is an Ancient Cave candidate.
2. Header: the title "Secret of MANA" at 0xFFC0 is the same on every variant, so it only confirms the game, it does not discriminate.
3. For a 4 MiB ROM check the structure, not a hash: opcode `0x22` (JSL) with an operand bank of `0xE0` or higher at file offsets `0x692F` and
   `0x6D1E` (and ideally a majority of the 45 sites; list in the JSON). Seeds 1 and 777 have identical hook sites but different JSL operands,
   expansion layout and map data (18,345 bytes differ in the first 2 MiB), so whole-ROM hashes and operand values are useless.
4. Confirm with the ASCII strings `ancient`cave` and `moppleton` in the expansion (E0:272B and E0:27B4 in seed 1, E0:26D3 and E0:275C in
   seed 777; the positions move with the seed).
5. Variant record for Ancient Cave: same address table; map id space 0.., boss maps from 256; item ids 6 and 7 renamed; weapon rank rule
   (ring order 1..7, 0 plus relative cursor); weapon slot holder bits; HUD crop row 184; no vanilla map names; macros may run only from `FIELD`
   as before. Unknown 4 MiB or hooked ROMs: use the same table in read-only mode and refuse macros.
6. A runtime check complements the ROM check: after boot the state `hero0 active = 1` with three active heroes at once, HP 50/45/40 and
   weapon slots all owned identifies Ancient Cave floor 1; it cannot replace the ROM test.

Detection and the per-variant record are a proposal; no frontend code was changed.

## 6. Honest limits

- One generator configuration (default Ancient Cave settings; the harness sets every difficulty field to "1" and turns the music randomizer
  off), two seeds. Other randomizer modes were not generated.
- snes9x 1.63 core only. Only floor 1 and door warps were exercised: no enemy fights, boss, chest, orb that grants magic, shop or later floor.
- Level, max HP, max MP, gold and the clock were confirmed by reading screenshots by eye; the images are in the state directory.
- `weapon_levels`, `weapon_charge_level` and `armor_slots` are not testable with what was tried; `spirit_levels` and the timers below `+0x1B6`
  are single data points. Attack spells need an enemy, so only support spells were cast to completion.
- The equip fix was validated on the boy only, in a patched copy, not in the repository source.
- The 29 of 34 figure counts fields of the existing map; it is not a statement about everything the companion could need on Ancient Cave
  (floor number, clock, armor, shops).

## Reproduce

```
python3 tools/companion/ancientcave/build_ac_fixture.py          # states (1 minute)
python3 tools/companion/ancientcave/verify_fields.py             # 34 field checks (about 2 minutes)
python3 tools/companion/ancientcave/test_ancientcave.py          # 16 tests (about 10 s)
python3 tools/companion/ancientcave/equip_matrix.py              # 64 + 64 equip runs (about 5 minutes)
python3 tools/companion/ancientcave/make_report.py               # writes the two data files
```

Another seed: generate it with `ancientcave_gen`, then set `SOM_ROM_AC=<path>` and `SOM_AC_TAG=_name` (separate states and evidence).
