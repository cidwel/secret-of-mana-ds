# Companion on the Nintendo 3DS, final-scope round (board item T4, 3DS part)

Date: 2026-10-20. Code: `tools/companion/n3ds/` (app), `tools/companion/gameplay_c/` (C gameplay patcher). Earlier rounds: `docs/companion-3ds-20261018.md`
(the .3dsx, direct control, launcher), `docs/companion-platforms-20261017.md` sec. 5.

Scope stated by the user on 2026-10-20: the 3DS version must save normally, have save states, a start menu (original or Ancient Cave run), the
QOL/fix toggles of `tools/gameplay/gameplay_menu.json`, a few simple filters, and a Credits entry. Real 3DS hardware is user-only: everything below
was run in **Azahar on the PC** (emulator timings are not console timings) and on the host with sanitizers; nothing ran on a console.

## 1. What the app does now

| Feature | How | State |
| --- | --- | --- |
| Battery RAM per game | `sdmc:/som/saves/<game>.srm` (`original`, or the 16 hex digit key of a generated ROM). Loaded after the core loads the game, written when it changes (checked every 5 s), on the pause menu, on exit and when the app is closed from HOME | Azahar: a pattern written to the file is loaded by the core and written back unchanged. Not played through an in-game save |
| Save states | 8 slots per game, `sdmc:/som/states/<game>.<n>.sta`: 28-byte header (ROM CRC-32, size, time, state CRC-32), 128x112 RGB565 thumbnail, the core state (529,140 B). A state loads only into the same ROM bytes (patches included); the slot shows "other ROM or options" otherwise. Written to a temporary file and renamed | Azahar: save, walk, load back; thumbnail and date shown in the menu; state file 557,840 B |
| Pause menu | Select+Start. Resume, State slot, Save state (asks twice to overwrite), Load state, Filter, Credits, Exit game. Top screen list, bottom screen only a grid background (user rule: nothing on the bottom unless touch is needed) | Azahar |
| Start menu | the shared launcher (`launcher/`): SoM Original, Create Randomized, Saved Randomized. On the main screen **Y** (or **Select**, or a tap on the small line at the bottom) opens Settings. No "ROM found" lines: only a missing `rom.sfc` is reported | Azahar |
| Ancient Cave and the other modes on the console | libsomrando linked into the .3dsx (sec. 3): "Create Randomized" generates on the console into `sdmc:/som/roms/gen/<key>.sfc` with a progress screen (B cancels) and a `<key>.json` sidecar, then plays it. Presets the generator does not reproduce byte for byte show "PC only" as on Android | Azahar: Ancient Cave "short" generated and booted; 17-preset batch in sec. 3 |
| Gameplay and QOL toggles | Settings > Gameplay: a preset row (Vanilla, Quick Combat, QOL, Arcade) and every option of `gameplay_menu.json` that the C patcher knows, grouped as in the JSON; advanced numeric options behind a toggle. Saved in `sdmc:/som/settings.cfg`. Applied when a game starts, in memory, to the ROM (original or generated); the patched copy is also written to `sdmc:/som/roms/patched/<game>.sfc`. An option whose bytes were changed by a randomizer hack is dropped and reported ("OPTION SKIPPED ...", the shared UX notice chip) | on-device patched ROM is byte-identical to the Python tool (sec. 2) |
| Video filters | Settings or pause menu: None (nearest neighbour), Smooth (bilinear), Scanlines (every other output line at 70 percent). Saved in `settings.cfg` | Azahar screenshots; cost in sec. 5 |
| Credits | `data/credits.json` embedded at build time (`gen_data.py`), shown scrolling, wrapped at 31 columns: from the start menu (Y, Settings, Credits) and from the pause menu. The start menu also carries the generator's own credit line (LGPL-2.1, Moppleton) | Azahar |
| Dual screen | unchanged: top = whole frame at 8:7 into 400x240, bottom = the shared UX panels | Azahar (round 1) |

Pad navigation everywhere (D-pad, A, B, L/R page); no simulated cursor, no touch needed for any new screen.

The dev fixture (`state.sav`, a libretro state loaded into the original ROM at start for tests) moved to `sdmc:/som/dev/state.sav` so it can never replace a real
game on a console that has an old copy of it in `som/`.

## 2. The C gameplay patcher (`tools/companion/gameplay_c/`)

`som_gameplay.c` is a line-by-line C port of the planner in `tools/gameplay/som_gameplay.py` (T8 and T11 options): same signature search, same
65816 routines, same records, same conflict rule (an option is all-or-nothing, dropped when any of its records does not match), same checksum
rule. Pure C99, no allocation, no OS calls: the Thor and PC frontends can use it too.

* `test_gameplay_c.py` builds `sgp_cli` (ASan/UBSan) and compares its output with `som_gameplay.py apply --on-conflict skip` on random option combinations,
  on the US v1.0 ROM, a copy with a 512-byte header and two Ancient Cave ROMs (where randomizer hacks make some options conflict): **726 of 726
  outputs byte-identical** with 120 random cases per ROM variant (QOL options included: chest, unready damage, fades, transitions, item limit, MP at level-up,
  dead experience, spell level rate, scroll window). The direct-control module still accepts a ROM patched with every option (`test_direct_ok.c`).
* End to end on the (emulated) console: the 3DS app patched the original ROM with the Arcade preset and the copy it wrote
  (`roms/patched/original.sfc`) is byte-identical to the Python tool's output (23 records, 219 bytes + checksum).
* **Sync rule.** The python tool and `gameplay_menu.json` are still being changed by the QOL stream (during this round `qol_skip_sword_pull` was
  removed and `qol_unready_damage` added). The app embeds the JSON at build time and shows only the options whose key the C patcher knows
  (`n3_gp_sgp()` returns -1 for the others), so a new option never crashes the app, it is just hidden until ported. After any change of
  `som_gameplay.py`: port it in `som_gameplay.c` (enum in the header, routine, record), rebuild, and run
  `SOM_ROM=... tools/companion/n3ds/test/run_n3ds_tests.sh` - the comparison fails on any drift. Status at the end of this round: all 22 options of the JSON are ported and the comparison is green (726 and 244 outputs in two runs).

## 3. libsomrando on the 3DS (newlib, ARM11)

`emu/build_libsomrando.sh <out>` compiles the 96 sources plus the generated resources (not stored in the repository) with devkitARM r68
(`-march=armv6k -mfloat-abi=hard -ffp-contract=off`, std gnu99) into `libsomrando.a` in about 4 seconds with 4 processes; **no source change was needed**
(newlib has everything the generator uses). The Makefile takes `RANDO_A=<libsomrando.a>`; without it the stub is linked ("no generator").

Two findings:

1. **(Superseded, see `randomizer-c-port-progress-20261018.md`, T9: libsomrando no longer needs any linker flag; `--whole-archive` stays in the Makefile and is harmless.)** The generator registered its hacks through weak stubs; in a normal static link the strong definitions live in archive
   members nothing else references, so the linker keeps the stubs and Open World answers "sr_p4_hack_character_selection is not ported yet" at generation
   time. The Makefile and `build_rando_batch.sh` use `-Wl,--whole-archive`.
2. **Same bytes as the host.** `test/rando_batch.c` generates every preset of the launcher with seed 12345. Built for the 3DS and run in Azahar, then compared with
   the same source built for the host (`test/compare_rando_batch.py`): **17 of 17 presets produce the same ROM SHA-256** (5 Ancient Cave, 5 Open World, 3 Randomizer, 2 Chaos, 2 Boss Rush; evidence `rando_batch_3ds_*.txt` and `rando_batch_host.txt` in the state folder). The first Ancient Cave ROM made in the app through the menu (seed 59882898, 14 options) was also compared byte for byte with the host generator. The 12-hex digit sidecar of the PC tool for "short", seed 12345 has the same SHA-256 (871ac2dc...) as the one the 3DS build made, i.e. also the C# oracle.

Time and memory (Azahar, emulated ARM11 268 MHz, **not calibrated against a console**): Ancient Cave 12.6 to 29.6 s (marathon is the longest), Open World 3.1 s, Randomizer 5.5 s, Chaos 5.5 s, Boss Rush 5.6 s; peak malloc heap during the run: Ancient Cave 12.9 to 22.0 MB (marathon 22.0), Open World 9.1, Randomizer 9.0, Chaos 9.7, Boss Rush 11.1 MB. The app frees the 9.4 MB art pack while it generates and reads it again afterwards, so the menu never holds the pack and the generator together. On the host the same generations take 0.03 to 0.35 s. The
generator allocates through `calloc` arena blocks (peak heap above), so memory is not the limit; the limit is the wait: the progress screen is redrawn at every
generator step (about 80 steps) so the console never looks frozen. LGPL-2.1+: the library is linked statically into the .3dsx (a user can relink with
`emu/build_libsomrando.sh` and the Makefile, all sources are in the repository); the credits screen and the menu footer name the author.

## 4. Tests

| Test | What | Result |
| --- | --- | --- |
| `tools/companion/n3ds/test/run_n3ds_tests.sh` | `test_sui` (list navigation, wrapping, text pages, rendering), `test_savefile` (CRC-32, battery RAM round trip and partial file, state header, thumbnail, wrong ROM, corrupt byte, truncated file, overwrite), `test_settings` (every option of the JSON steps through its values and back, presets, persistence, bad values), the gameplay comparison and the direct-control check; ASan + UBSan | all pass |
| `emu/azahar_smoke.sh` | scenarios `menu`, `state`, `srm`, `preset`, `generate` in headless Azahar with screenshots | run by hand today, results above |
| `test/rando_batch.c` + `compare_rando_batch.py` | generator on the 3DS build against the host | sec. 3 |
| Build | `make CORE_A=... RANDO_A=...` with `-Wall -Wextra`: no warning in the new code | ok |

## 5. Speed and memory (Azahar, indicative only)

* Frame time with the original ROM on a field map, old-3DS model, top-screen scaling `T` of `perf.txt`: None 3 ms (previous round), Scanlines 6 ms, **Smooth 19 ms**
  (bilinear, four reads and three blends per output pixel, 313x240). On a real old 3DS (268 MHz) Smooth is probably too slow next to the core; it is offered
  because the New 3DS (804 MHz) has the room. If it is too slow on the console the filter is one line to remove.
* Heap with the original ROM, field map, art pack and fixture: `heap_used` about 24.6 MB (it was 25.8 MB before this round; the pack and the ROM copy dominate).
  The patched ROM copy adds nothing (the buffer is patched in place). Save states allocate 529 KB for the duration of a save or a load.
* `.3dsx` 1,866,528 B (core 0.78 MB, libsomrando 0.9 MB, the rest the UI), bss 3.5 MB.

## 6. Decisions that need the user

* **Select+Start now opens the pause menu instead of quitting.** Exit is a menu row.
* **Loading a state also restores the battery RAM stored in it** (the core serialises it), like every emulator: loading an old state rolls back an in-game save made after it. The normal `.srm` is untouched until the next flush.
* States are tied to the exact ROM bytes: changing a gameplay option makes the existing slots unusable (they stay on the card and work again if the options are put back).
* The Settings entry is a hint line (Y) on the start menu because `launcher/` is shared with Android and the PC build; if the launcher model gets a native Credits
  row (the PC and Thor streams may add one), replace the hint with it.

## 7. What is left for real hardware (user only)

1. Run the .3dsx on an old and a new 3DS; read `sdmc:/som/perf.txt` (`R` core, `U` bottom UI, `T` top scaling, `heap_used`, `linear_free`): is 60 fps held with None and Scanlines, is Smooth usable.
2. Generation time on the console for each Ancient Cave preset (the emulator number above says nothing reliable) and whether memory is enough with the art pack loaded.
3. A real in-game save at a save point, power off, resume (battery RAM) and a save state round trip with audio on (needs the DSP dump).
4. SD write speed of a state (557 KB) and of the patched ROM copy (2 to 4 MB) at each game start with options on.
5. File times: `stat` may give no time on the console, the Saved list then shows no date (already handled).

Files: new `tools/companion/n3ds/source/{sui,screens,savefile,settings}.{c,h}`, `gen_data.py`, `test/`, `emu/{build_libsomrando,build_rando_batch,azahar_smoke}.sh`,
`tools/companion/gameplay_c/`; changed `source/main.c`, `source/menu.c`, `source/menu.h`, `Makefile`, `emu/azahar_env.sh`. Nothing outside `tools/companion/n3ds/`, `tools/companion/gameplay_c/` and the docs was touched.
