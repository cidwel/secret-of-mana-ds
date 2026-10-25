# Thor companion, round 4 (2026-10-20)

APK: `~/.local/state/som-decomp/som-companion-thor-round4-20261020.apk` (debug build, with libsomrando, arm64 and x86_64). Not installed on the Thor (waiting for "liberado").
Build: `bash ~/.local/state/som-decomp/thor-r4-20261020/build_apk.sh`. Tests: `tools/companion/android/tests/run_thor_tests.sh`.

## What is new
| Area | Behaviour | Code |
|---|---|---|
| SRAM | Cartridge RAM is saved per game (`save_<key>.srm`, key = original ROM hash or generated config), flushed on pause, stop, destroy and by state chords. Gameplay tweaks do not fork the save. The old 32-bit file name is migrated. | `som_jni.c` (srm_name, flushSave), `MainActivity` |
| Save states | 8 slots per ROM and per gameplay signature (`files/states/<key[-sig]>/slotN.sst`), atomic write, 128x72 thumbnail and time, states from another core build are refused. Chord: hold R3, then R1 save, L1 load, R2 next slot, L2 previous slot; the chord buttons never reach the game. No on-screen message on success (haptic only); a failure shows a short error. | `som_states.c`, `MainActivity.stateChord` |
| Bottom screen | Options has SHADER, SAVE STATES (slot grid with thumbnails, SAVE/LOAD) and CREDITS pages. The bottom screen stays empty unless touch is needed. Classic options page (3DS, PC) is unchanged. | `ux/som_ui_draw.c` |
| Shaders | NONE (automatic sharp/crisp as in round 3), SHARP, SCANLINES, LCD GRID, CRT (AGSL, API 33+; older devices keep nearest). Uses screen-pixels-per-game-pixel, so it works with the fractional Thor scale, FIT/FILL/INTEGER/STRETCH and every wide width. Effects fade out below 2x. Persisted (`shader`). A compile failure disables only that shader. | `GameView.java`, `Settings.java` |
| Gameplay / QOL | Start menu page (X on the main screen, or the footer) reads `tools/gameplay/gameplay_menu.json`, pad and touch navigable. Applied to a patched copy (`files/roms/gameplay/<base>-<sig>.sfc`) by a C port of `som_gameplay.py`; the user's ROM is never written. Options that conflict with a ROM are skipped and named in one toast. | `som_gameplay_apply.c`, `som_start_pages.c`, `StartMenuActivity` |
| Credits | Start menu (Y, or the footer) and Options render `data/credits.json` (special thanks: Moppleton and the QOL patch authors). Scrollable with pad and touch. | `som_start_pages.c`, `som_ui_draw.c` |
| Ancient Cave / Create a run | Unchanged path through libsomrando (17 presets, device SHA-256 only in the log and json). | round 3 |
| Round 3 fixes | Kept (no change to the input, slot, wide-core or HUD code). | |

## Tests
- `test_states.c`: 25 checks (isolation, atomicity, truncation, core mismatch), ASan/UBSan.
- `test_start_pages.c`: 26 checks, rendered at 1240x969, 1080x1160 and 800x480.
- `gameplay_parity.py`: 225 comparisons C vs Python (US, Magno, 3 generated ROMs, 46 option sets), 0 differences. Fails when the menu has an option missing from the C file.
- `RunKeysTest` (JVM): 11 checks.
- `ux/test_som_ui_draw.c`: 1183 checks (12 new golden cases, no existing hash changed). `launcher/run_launcher_tests.sh`, `run_bind_tests.sh`: pass.
- Emulator (Pixel_8, API 37, x86_64): start menu with footer, credits and gameplay pages; all five shaders render (screenshots here); R3+R1 / R2 / L1 create `slot0`/`slot1` under `states/orig-<hash>`; a `gameplay.json` yields `roms/gameplay/orig-<hash>-<sig>.sfc` and a separate state directory. Screenshots: `docs/companion-thor-20261020/`.

## Parity note
`som_gameplay.py` is edited by T11 at the same time. `qol_top_text_fast` and `qol_skip_sword_pull` were removed there and `qol_unready_damage` added; the C port follows. A new option in the menu without a C port makes the parity test fail by design, and the page hides options the C file does not know.

## Not verified (needs the real device)
Shader look and speed at the real 1240x970 scale, chord feel with the real R3, the pad navigation of the new pages on the physical pad, in-game SRAM round trip (the emulator run never made an in-game save), and the 12-preset hash comparison against the PC.
