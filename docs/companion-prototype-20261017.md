# Companion prototype on PC (stage 2 of board item T4)

Date: 2026-10-17. Location: `tools/companion/`. WRAM map and evidence: `docs/wram-companion-map.md`,
`data/wram_companion_map.json`.

## What exists

| File | Role |
| --- | --- |
| `som_companion.h/.c` | Shared C99 module: `som_read_state()` decodes the party, items, weapons, ring and game mode from a WRAM pointer; `som_macro_*` is the button-macro state machine (equip weapon, use item, cast magic) with refusal, timeout and recovery. No libretro or OS dependency, never writes memory. |
| `som_companion_host.c` | Headless PC harness: dlopen a libretro core, hand it the ROM from disk, run frames, give the module the WRAM pointer (`retro_get_memory_data(2)`), press the pad mask it returns; script commands for states, macros, JSON state output, PPM screenshots, core options. |
| `render_bottom.py` | Draws the 320x240 bottom screen (party HP/MP/charge, weapon, items, owned weapons, ring status) from the host's JSON and stacks it under the top screen as one PNG. Image only, no window. |
| `build_fixture.py`, `common.py` | Build the base and party save states by driving the real game (nothing from the ROM is stored). |
| `test_companion.py`, `test_som_companion.c` | 13 end-to-end tests through the host and C unit tests on synthetic WRAM. |
| `retro.py` | ctypes libretro host used for exploration (probes), also usable for tests. |
| `ancientcave_gen/` | Console harness to generate an Ancient Cave ROM with the randomizer's own code (the app is not in the repo). |

Run: `python3 tools/companion/test_companion.py` (about 7 s after the fixtures exist; the first run builds them,
about 1 minute) and, for the Spanish translation, `SOM_TEST_ROM=magno python3 tools/companion/test_companion.py`.
Evidence (JSON lines and PPM/PNG) goes to `$SOM_COMPANION_STATE/evidence/<rom>/`
(`/home/cidwel/.local/state/som-decomp/companion-20261017`). The ROMs are read from `rommancer/roms/snes`.

## Measured results (13 of 13 tests pass on vanilla US and on the Magno translation)

| Check | Result |
| --- | --- |
| Live status reading | After the fixture the module reports 3 present heroes, map 131, mode `field`, `safe`. A pending damage of 7 written to the game's own field lowers HP 50 to 43; pending MP damage 2 lowers the girl's MP 8 to 6; status word 0x2000 with timer 20 makes HP fall 2 per 40 frames and the timer 0x14 to below 0x14. |
| Use item (Candy, record 0) | Hero at HP 20: Candy x3 becomes x2, HP rises above 20; 255 frames; mode back to `field`, ring closed. |
| Use item (Royal Jam, record 4, ring index 4) | x2 to x1, HP 20 to 50, other records untouched. Confirms ring order = record order. |
| Equip weapon (axe) | weapon type 1 to 2, row 9 to 20, slot bytes sword 0x40 to 0x00 and axe 0x02 to 0x42; 240 frames. |
| Cast magic (girl, Gnome, first spell) | the macro first cycles the ring owner to the girl with X, selects the magic category, spirit, spell and target; MP 8 to 4; 492 frames. Sprite (spirit byte 5, spell 2): MP drops; 650 frames. |
| Refusals | Refused with `fail-unsafe` while an event runs and from the title/boot (`no-game`), and with `fail-bad-argument` for an unowned weapon, an empty record, an unlisted spirit, the boy casting, the girl using byte 6 and the sprite byte 7. |
| Unusable choice | Cup of Wishes on a healthy hero: 703 frames, `fail-timeout`, ring closed by the recovery, item untouched. |
| Abort | Abort at frame 150 with the magic ring open: recovery closes it in 139 frames, nothing cast, mode `field`. |
| Event during a macro | A warp script started with the item ring open: result `fail-unsafe`, ring closed, the warp completes (map 23). |
| Top screen | `TOP` writes the frame without rows 199..223 (256x199 pixels); with BG3 disabled through the core option the HUD rows change, which shows the HUD is BG3 plus sprites. |
| Dual image | `evidence/vanilla/dual_after_use_item.png`: top = game without HUD, bottom = party status after the use-item macro (Candy x2). Evidence JSON before/after for every macro sits next to it. |

## Design notes

- The macro reads WRAM every frame and only advances on observed transitions (`$7D`, `$1808`, `$1809`, `$180A`,
  `$180B`, `$1812`), with fixed settle times; it does not trust frame counts alone. A tap holds a button 3 frames.
- Safety: a macro starts only when `safe_to_inject` (mode `FIELD`, a living hero bound to pad 1). If the mode becomes
  `EVENT`, `TRANSITION` or `NO_GAME` mid-way it stops pressing, runs the recovery (taps A, A, Y until `$7D` = 0, at
  most 12 taps) and reports `fail-unsafe`. `som_macro_abort()` does the same on request.
- Results: `done` (effect seen in WRAM), `done-unverified`, `fail-unsafe`, `fail-bad-argument`, `fail-timeout`,
  `fail-unavailable`, `aborted`. A frontend should show the result and never retry blindly.
- Frontends keep their own thin glue: call `som_macro_step()` before `retro_run()` each frame and OR the mask into the
  pad state; call `som_read_state()` after each frame for the bottom screen. The bottom screen drawing is per
  platform (the PIL renderer is the reference layout).

## Honest limits (what does not work yet)

- Everything was measured on snes9x 1.63 in the flatpak RetroArch cores; bsnes exposes no WRAM through libretro.
- The party, items, weapons and magic come from a fixture: girl and sprite were activated with the game's own event
  opcode, but items, weapons, spirit masks, MP and the ring-enable flag were poked. A real playthrough with all three
  heroes was not available (the public 3-player movie desyncs on this core). Effects were read back from the game,
  so the macros are validated, but real save files may hold states the fixture does not (equipped weapons of girl and
  sprite, armor, higher weapon levels).
- Attack spells need an enemy: casts for several spirit and spell slots (for example the girl's Salamando slots 1
  and 2) timed out because nothing was in range; no enemy was provided. Only support spells were verified end to end.
- The magic macro pays MP 120 to 150 frames after the ring closes; the verification window is 330 frames.
- Macros are slow (4 to 11 seconds) because waits are conservative; they can be tuned after measuring on a device.
- Charge (power-up) level was not found; the HUD gauge value is the recharge counter only.
- HUD hiding is a crop of the bottom 25 rows: the top screen loses that strip and the ring/dialogue are untouched.
  True hiding needs OAM/BG3 control that libretro does not offer; HUD sprite ranges were not found.
- The Magno state is before the HUD is enabled, so the HUD crop was not compared there.
- Ancient Cave: the ROM was generated but not booted past the name entry; no field is verified (see the map).
- Switching which hero the pad controls is done with X in the ring (owner cycle); the pad itself stays bound to
  the boy. The module treats the ring owner as the caster, which matches what was observed.
- Names are not decoded; the bottom screen uses fixed role names and item names from `docs/items-shops.md`.
- No window mode: output is files. A second window needs SDL on the frontend side and was not built.

## Equip-weapon fix and shared drawing module (2026-10-17)

`tools/companion/som_companion.c` now carries the two ring fixes measured on Ancient Cave (see `docs/companion-ancientcave-20261017.md`):

- `som_weapon_ring_index` ranks owned weapon types in ring order 1..7 then 0 (the glove is last).
- The equip macro syncs with one LEFT tap, then aims at `(rank(wanted) - rank(held)) mod count`, because the ring is relative to the held
  weapon (cursor byte starts at the held rank, the first press resets it, ring position = cursor + held rank). A held type that is not in
  the ring (a hero with no weapon, girl and sprite in the early vanilla fixture) counts as rank 0; measured: after the sync tap the cursor
  reads 1 and the ring position is 1.
- Wanting the weapon that is already held returns `done` at once (note `already held`) without opening the ring.
- The C API is unchanged (same functions, struct layouts and enums).

Tests added: `test_som_companion.c` models the ring and runs the real macro on all 8 x 8 (held, wanted) pairs (plus ring order and gap
checks); `equip_matrix.py` drives the real core on every owned (held, wanted) pair for one hero (held states are built with the macro and
confirmed in WRAM); `test_companion.py` `test_04b` runs it for the boy on the vanilla and Magno fixtures.

Measured results (all with the repository host, no patched copy):

| Run | Result |
| --- | --- |
| C unit tests (`test_som_companion.c`, includes 64-pair ring model; `test_status_draw.c`) | pass |
| `test_companion.py` vanilla US / Magno | 14 of 14 / 14 of 14 (boy matrix over owned types 1, 2, 3, 7: 16 of 16 on both) |
| Ancient Cave seed 1, boy / girl / sprite, 8 x 8 | 64 of 64 / 64 of 64 / 64 of 64 |
| Ancient Cave seed 777, boy / girl / sprite, 8 x 8 | 64 of 64 / 64 of 64 / 64 of 64 |
| Vanilla fixture, girl / sprite | 12 of 16 each; the 4 pairs with held type 1 are not verified (the boy holds the sword, so that held state cannot be built); wanted type 1 works (weapon swap with the boy) |
| Magno fixture, girl / sprite | not run (same ring code; fixture weapon set equals vanilla) |

`tools/companion/ancientcave/test_ancientcave.py` still expects the old behaviour in `test_04_equip_stock_host_wrong_weapon` and builds a
patched copy in `patched_host.py` that asserts on the old source text; both now fail by design (the fix is in the repository). That
directory belongs to another agent and was not edited; the stock-versus-patched comparison there should be retired.

Shared module: `som_status_draw.{c,h}` and `test_status_draw.c` moved from `tools/companion/android/native/` to `tools/companion/`
(no other board claim covered `tools/companion/android`). `android/app/src/main/cpp/CMakeLists.txt` and `n3ds/Makefile` now point at the new
path and no longer add `android/native` to the include path (`make -n` of the 3DS Makefile resolves). Android was not rebuilt.
