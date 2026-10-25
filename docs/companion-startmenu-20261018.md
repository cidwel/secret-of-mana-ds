# Start menu: SoM original or Create randomized (board item T9)

Date: 2026-10-18. The start menu is a platform-neutral C model plus renderer (`tools/companion/launcher/`), a preset catalogue taken from the randomizer
source (`tools/companion/randomizer/presets.json`), a PC pipeline (`som-dualscreen generate`) and an Android screen (`StartMenuActivity`).
Generating on the device waits for libsomrando (plan: `docs/randomizer-c-port-plan-20261018.md`); until then the app plays ROMs the PC pushed.
Related: `docs/companion-cli-20261017.md` (CLI, variants), `docs/companion-cli-audit-20261017.md` (source and licence audit).

## Stages and progress

| Stage | Weight | State |
| --- | ---: | --- |
| 1. Preset and option catalogue from the upstream source, checked against the generator | 5 | done |
| 2. Menu model, renderer and tests in C (buttons, touch, seed, serialisation, drawing) | 10 | done |
| 3. PC pipeline: config JSON to generated, header-checked, boot-tested ROM; reproducibility | 10 | done |
| 4. Android screen, cache lookup, `push-random`, generator seam (stub) | 10 | 10 of 10: runs on the Thor (top screen and mirrored on the bottom panel), a pushed Ancient Cave ROM plays; the in-menu worker path stays untested until libsomrando |
| 5. 3DS screen | 5 | not started |
| 6. PC frontend (T10) uses the same model | 5 | done in T10 (`docs/companion-pc-20261018.md`) |
| 7. libsomrando: generation on the device | 55 | not started (plan written) |

Done: 35 of 100.

## What the player sees

MAIN: two entries, `SoM Original` and `Create Randomized`, each with a status line (`ROM found` / `Choose your ROM after start`; `Generator ready` /
`N pre-generated ROMs` / `Needs a pre-generated ROM`) and the credit line `Randomizer by Moppleton, LGPL-2.1`.

PRESETS: `Surprise me` first (a random preset, random settings, random seed), then the presets grouped by mode, Ancient Cave first.
TWEAK: rows `Generate and play`, `Seed`, `Randomize all`, `Reset to preset`, `Difficulty` (modes that have it), then the options by group, and
`More options` that reveals the advanced ones (the generator's shared quality-of-life and challenge switches among them). MULTI: a checklist for a
multiple-choice option (Ancient Cave biomes; at least one stays on). SEED: eight digit boxes, a keypad, `DEL`, `OK`, `RANDOM SEED`.
GENERATE: preset, seed, cache key, progress bar with the step name and a percentage, `CANCEL`; or the messages below. PLAY: `Starting the game`.

| Input | Buttons | Touch |
| --- | --- | --- |
| Move | D-pad up/down, L/R page | tap a row, drag to scroll, up/down boxes in the footer |
| Change a value | left/right, A (next) | the `<` and `>` boxes, or a tap on the row (next) |
| Back | B | `BACK` |
| Randomize all, reset, go | X, Y, Start | rows, `GO` |
| Seed | left/right digit, up/down change it, X random, Y delete, A or Start accept | keypad |

Targets: every tappable box is at least 16 virtual pixels (about 3 mm on the 3DS panel); the layout scales by an integer from 320x240 (3DS) to 1240x1080 and
1920x1080 (Thor screens) and is centred in a 360 virtual pixel column. The window frame, palette and the game's own 8x11 face (and the anti-aliased faces of the
graphics pack at 2x or more) are those of `som_ui_draw.c`; its drawing primitives are file-private, so the few needed are repeated in `som_launcher_draw.c`.
The pack is optional; without it a built-in 5x7 face is used. Long option values have short display forms (for example `Find both at cur. L`); the detail panel
shows the generator's full text. On a 3DS-sized screen with the pack, 52 of 1,998 option row states (2.6 percent) shorten a label or value with `..`; no text is cut
without the marker (the walk test in `test_launcher`, run with `SOM_UI_PACK` set, counts them).

## Presets (data: `tools/companion/randomizer/presets.json`)

Mode data (names, values, defaults, numeric ranges, the generator's own help text) comes from `config/settings/*Settings.cs` of v1.49. `presets_tool.py build`
parses it, `presets_tool.py check` fails when a key or default differs from the C# or from `somrando --list-options`, when a preset names an unknown option
or an illegal value, or when the file is out of date (verified: OK for 5 modes, 17 presets, 59 common options). Labels, grouping, which options are advanced,
the short value texts and the preset values are decisions of this repository; the Open World presets `owl%` and the pools of "randomize all" come from
the upstream GUI presets (`forms/MainForm_OpenWorldPresets.cs`, Mystery seeds).

| Mode | Presets | Own options (basic / advanced) | Difficulty |
| --- | --- | --- | --- |
| Ancient Cave | Ancient Cave, short, marathon, gauntlet, boy only | length, boss frequency, characters, music, profanity filter / dialogue source, biomes | casual, hard, really hard |
| Open World | Open World, Mana Tree Revival, vanilla long, casual, owls only | goal, enemies, bosses, stat growth, difficulty, characters, logic, rewards / many more (chests, start, levels, roles, Mana seeds, ...) | none (the stat growth option is the difficulty) |
| Vanilla Randomizer | Vanilla Randomizer, classic plus, hard | shuffles, experience, gold, enemy strength / autosave, holiday, status ailments | none |
| Chaos | Chaos, short | characters, floors, bosses, healing first, safer early floors / map colours | casual, hard, really hard |
| Boss Rush | Boss Rush, hard | characters, Luna MP limit | casual, hard, really hard |

Excluded on purpose: the `custom` difficulty (needs a per-field editor), `plandoSettings`, `spoilerLog`, debug and bookkeeping keys, custom character colours.
`acDifficulty`, `brDifficulty` and `chDifficulty` are not options of the menu: the generator takes the difficulty as its own argument.
"Randomize all" leaves the seed and every option marked `randomize: false` (cheat and challenge switches, holiday themes, start gold, roles, plando) alone,
draws from the upstream pools where they exist, otherwise uniformly, honours requirements (`oopsAllThis` only with `opEnemies=oops`, the Mana seed options only with
`opGoal=mtr`) and the constraints (one character at least; minimum seeds not above the maximum).

## Configuration, key and generator command

The menu serialises the choice as compact JSON with sorted keys (`sl_config_json`), only options that differ from the generator default and whose requirement is met:

```
{"difficulty":"casual","generator":"1.49","key":"2c845ac55030ff15","mode":"ancientcave","options":{"acLength":"short"},"preset":"ancientcave-short","schema":1,"seed":"12345"}
```

Seed: 1 to 8 digits, no leading zeros, passed to the generator as the same string the Windows app takes (the generator hashes the string, so `0042` and `42` would
be different seeds; the menu never produces the first). `difficulty` is `null` for Open World and the Vanilla Randomizer.
Key: the first 16 hex digits of SHA-256 over `somrando/<generator version>|<mode>|<seed>|<difficulty or ->|<key=value;...>` (the preset id is not part of it:
two routes to the same settings share a ROM). The source ROM is not in the key because the generator accepts only the US v1.0 image.
Other outputs of the model: `sl_config_cli` (`--mode ... --seed ... --difficulty ... --option k=v`, the arguments of `som-dualscreen randomize`) and
`sl_config_lines` (the `key=value` lines `somrando_generate` takes). `tools/companion/cli/launcher_config.py` is the Python reference (validate, key string, key, command line,
`from_preset`); `test_launcher_config.py` checks 108 configurations written by the C code (all presets at 4 seeds and 40 randomized ones) for validity against the catalogue, canonical bytes,
key string, key and command line. The key computed by the C code inside the Android app (x86_64 emulator) matched the Python value for the same configuration (`9dbc187d1ec5e8d9`).

## PC pipeline

```
som-dualscreen generate --config cfg.json --rom <US v1.0 ROM> [--out-dir D] [--frames 1800] [--force] [--json]
som-dualscreen push-random --serial <adb serial> --rom <generated ROM> [--dir <app files dir>] [--dry-run]
```

`generate`: 1. validates the JSON against the catalogue (exit 2 with the list of problems); 2. accepts only the US v1.0 ROM (exit 2 otherwise, message names the variant);
3. reuses `<out-dir>/<key>.sfc` when its sidecar hash and key match (not with `--force`), otherwise runs the headless generator into a `.part` file and renames it;
4. checks size 4 MiB, header title `Secret of MANA` and the variant by hook sites and mode string; 5. runs the libretro core headless for `--frames` frames (default 1,800) and requires a non-blank
picture that changes between the two halves; 6. writes `<key>.sfc.json` (config, sha256, source ROM SHA-1, boot result) and prints the ROM path (or one JSON line with `--json`).
The default output directory is `~/.local/state/som-decomp/companion-cli-20261017/gen`; no path inside the repository is accepted. `push-random` copies the ROM and its sidecar to `<dir>/roms/gen/<key>.sfc|json`
(default directory: the release package in `variants.json`); there is no default serial, on purpose.

Verified 2026-10-18 on lumina with the v1.49 headless build and the US v1.0 ROM (SHA-1 `8133041a`):

| Check | Result |
| --- | --- |
| Presets generated, header and 1,800-frame boot checked | `ancientcave-short` seeds 12345 and 777; `open-mtr` seeds 12345 and 777; `chaos-short` 4242; `rando-hard`, `bossrush-hard`, `open-owl` 99: all pass (variant names `randomizer_ancientcave`, `_open`, `_chaos`, `_rando`, `_bossrush`) |
| Menu "randomize all" configurations produced by the C model | four of them (Boss Rush, two Ancient Cave, Open World) pass the same checks |
| Same configuration and seed twice, different output directories, `--force` | identical SHA-256 for `ancientcave-short`/12345 (`871ac2dc...`) and `open-mtr`/777 (`56733dd6...`) |
| Different seeds | different ROMs (`871ac2dc`, `4340e97b` for the two Ancient Cave seeds) |
| Cache | a second run without `--force` reports `cached` |
| Refusals | a configuration with an option that does not exist exits 2 and lists it; the Magno Spanish ROM exits 2 with the reason |
| Generation time | 0.3 s (Open World) to 1.0 s (Ancient Cave) plus 0.5 s for the boot test |

Limits of the boot check: 1,800 frames reach the logo and the title (12 colours, animating), not the first map; the companion's fixtures play past floor 1 for Ancient Cave only (`som-dualscreen verify`).
The hook-site detector needs at least half of the 45 sites: randomized Open World and Ancient Cave ROMs with many toggled options showed 23 to 37 of 45, close to the threshold of 22 for the lowest, so
detection by hook count alone is fragile when a toggled option removes a hook; the mode string check is the second leg. Raise it before shipping a randomize-all that can hit fewer sites.

## Android

Files: `StartMenuActivity.java` (flow), `LauncherView.java` (draw and touch), `LauncherNative.java` and `cpp/som_launcher_jni.c` (JNI). The C sources of the menu are added to `libsomjni`
(`CMakeLists.txt`). Shared files touched, minimally: `LaunchActivity.java` (forwards to the menu instead of `MainActivity`), `AndroidManifest.xml` (the new activity),
`MainActivity.java` (an optional `rom_path` extra loads that ROM; with it the test save state is not auto-loaded), `CMakeLists.txt`.

Storage (app files directory, never the APK or the repository): `rom.sfc` the user's ROM (as before), `roms/gen/<key>.sfc` plus `<key>.json` generated ROMs.
Flow: `SoM Original` starts `MainActivity` exactly as before. `Create Randomized`, at GENERATE: a valid `roms/gen/<key>.sfc` (4 MiB, title `Secret of MANA`) goes straight to play; otherwise, if
`LauncherNative.generatorAvailable()` the generator runs on a worker thread through the seam and reports progress; otherwise the screen says
`Generator not available on this device yet (libsomrando pending). On the PC run: som-dualscreen push-random`.

Seam: `launcher/somrando_api.h` (`somrando_available`, `somrando_version`, `somrando_generate(request, progress, user, err)`), `somrando_stub.c` now, libsomrando later: replace the stub in
`CMakeLists.txt`; the menu, the JNI file and the Java code do not change. The worker-thread path (`nativeGenerate`, progress, cancel, rename after the header check) is written but could not run,
because the only implementation is the stub.

Verified on an x86_64 Android emulator (a 1080x2400 phone image forced to landscape, package `org.somdecomp.companion.menu` built from a copy of the tree so the user's app was not touched; no libretro core, so the
game itself runs in demo mode): menu renders through JNI; buttons navigate MAIN to PRESETS to TWEAK; START shows `NOT AVAILABLE` with the key; after `som-dualscreen push-random --serial emulator-5580 --dir ...`
B then START goes to PLAY and `MainActivity` starts with `rom_path` (log line `No core: demo mode only` comes from the missing core, not from the ROM path); `SoM Original` starts `MainActivity`.
Not verified: the Thor (the device was running the user's app when checked, and later disconnected; nothing was installed on it), gamepad keys on real hardware, touch on the Thor screens, playing a randomized ROM on the arm64 core, the second display.

## Tests

`tools/companion/launcher/run_launcher_tests.sh` (ASan and UBSan): `test_launcher` 158,597 checks (catalogue and every preset applies; delta rules; hidden options; value validation; SHA-256 vector; 60 random seeds per preset
for validity, determinism and round trip through `sl_set_option`; button navigation from MAIN to PLAY including seed entry, constraints, multiple choice, generate, cancel, retry, cache hit; touch on five screen sizes; touch target size;
drawing of ten scenes at four sizes with no cut text and golden hashes `golden_hashes.txt` of the built-in face; every row and every enumeration value drawn at three sizes, with the pack too when `SOM_UI_PACK` is set), then
`test_launcher_config.py` (108 configurations). `presets_tool.py check` and `gen_catalog.py --check` keep the data and the generated C table in step; `som_launcher_catalog.c` is generated, edit `presets_tool.py` and rerun both.

## Licence

The catalogue and the option names are facts about Moppu/SecretOfManaRandomizer v1.49 (LGPL-2.1); `presets_tool.py` reads the upstream source at build time and nothing from the upstream tree is copied. The menu shows
`Randomizer by Moppleton, LGPL-2.1`. The ROM and generated ROMs are never in the repository or the APK; the app and the CLI write them to app or state directories only.

## Open items

- Run the menu on the Thor (install, gamepad and touch on both screens, play a generated ROM on the arm64 core, check `state.sav` is untouched); decide whether the bottom screen should mirror the menu for touch.
- 3DS screen (devkitARM build of the three C files, no libretro dependency; the font pack path is optional).
- PC frontend (T10) uses the same model and `som-dualscreen generate`.
- libsomrando (plan in `docs/randomizer-c-port-plan-20261018.md`); then the in-menu generation path gets its first real test.
- Detector robustness for heavily toggled ROMs (above); per-ROM save files (the app keeps no per-ROM SRAM today).
- Third-party quote tables in Ancient Cave before any public build.

## Thor verification (2026-10-18)

Installed and driven on the AYN Thor (arm64, wide core). Changes made then: a `Presentation` mirrors the menu on the second screen (touch works on both; the model follows the smaller screen's row count), `dispatchGenericMotionEvent` turns the hat and left stick into menu keys (they never arrive as DPAD key codes), `MenuSound` plays synthesised cursor/accept/back/error blips (the game's own effects need the emulator core), test extras are forwarded to `MainActivity`, and the sound-mailbox debug log in `som_jni.c` no longer overflows its buffer (it aborted the app on the first randomized ROM boot). Results: SoM Original unchanged; `som-dualscreen generate` + `push-random` for `ancientcave` seed 12345 (key `e0343466b771b0c1`) plays: title, new game, floor 1 (HP 50/45/40), equip, cast, quick slots, its own `save_<hash>.srm`; a configuration with no pushed ROM shows the libsomrando-pending message; BACK chain GENERATE, TWEAK, PRESETS, MAIN; going back to Android and starting the other mode in the same process works with no stale state (`state.sav` is not loaded into a randomized ROM). The FILL top-screen mode crops the lowest 25 rows, so the last lines of the Ancient Cave title screen are cut; use TOP FIT = FIT to see them.
Needs the system setting `dual_screen_display_mode` = 0 for the bottom panel to show app content.

Follow-up (2026-10-18): PRESETS keeps the last choice (`preset_row`, tested in `test_launcher`). The menu has no ROM picker for other ROMs (Magno): the app only starts rom.sfc or roms/gen/<key>.sfc; for tests, `am start -n org.somdecomp.companion/.LaunchActivity --es rom_path <file>` then SoM Original plays that file. Chaos: short seed 4242 (key `a705b603eb8f2253`) boots and reaches name entry on map 16. Top screen defaults are now wide 46 + FIT.

## Round 2 (user feedback after testing on the Thor)

Menu (all verified on the device with real hat/button events through `/dev/input/event9`, captures of both screens):

* **Two-screen layout.** With a second display the main screen (top) no longer mirrors the panel: on the main menu it shows a procedural Secret of
  Mana style wallpaper (twilight sky, Mana Tree, Mana Sword, hills; drawn in `som_launcher_draw.c`, no game asset, nothing ROM-derived); while a
  game is being set up it shows a summary read from the configuration model: preset, mode, difficulty, seed, key, the three characters (in the
  party or not, who starts, joins at level 1 / party level, plays-as roles, starting level / weapon / gear / gold where the mode has those
  options), the goal and the mode description, every visible option grouped (advanced ones only when changed; yellow = differs from the mode
  default), the help text of the highlighted option and a pad legend. What the configuration cannot say (levels, HP, MP, equipment outside the
  options) is stated as "not part of the settings", not guessed. In PRESETS it previews the highlighted preset. The bottom screen carries the
  menu (touch) and is the only interactive view.
* **Saved Randomized** (third main entry): up to four pre-generated games from `roms/gen/<key>.sfc` with their sidecar (`<key>.json`: preset, seed,
  date); A plays, DELETE (or RIGHT then A, or X) asks twice within 3 s, then moves the `.sfc` and `.json` to `roms/trash/<stamp>-<key>.*` (rename,
  never rm; moving them back restores the game). A newly generated game replaces the oldest when four exist (also by moving it to the trash).
  The top screen shows the summary of the highlighted slot.
* **Pad everywhere.** List screens form a ring: rows, then the footer buttons (BACK, GO, page up, page down), then the first row; LEFT/RIGHT walk the
  footer, A presses, wrapping both ways. L1/R1 page, START = GO (also on a saved slot = play), SELECT = reset (also on SEED = random), X = randomize
  all, Y = reset, B = back. The focused footer button is drawn highlighted; GO is greyed when the generator cannot make the current setup.
* **Pad panel mode in the game.** A click of the left stick (L3) hands the pad to the bottom panel: D-pad / left stick move a cyan focus ring over the
  buttons (nearest in that direction, wrapping), A presses the focused one, B is back / cancel (closes OPTIONS); L3 again returns the pad to the
  game. Library: `som_ux_nav_*` (tested: every enabled target reachable on every tab and size).
* **Panel.** MAP is the first of the four tabs and no longer takes the whole panel; the enemy list is up to three big cards (sprite, name on two
  lines when long, HP gauge) sharing the whole area; the target selector also uses up to three full-height cards.

Generator: the app now links `libsomrando.so` (`-PsomRandoSrc=<upstream SoMRandomizer folder>` in gradle, arm64 and x86_64). The literal text
"libsomrando not found" never existed in the code: the old stub message said "(libsomrando pending)". The message is now
"This device cannot create new randomized games yet..." and never names a library; a failed native call is caught and reported the same way.
`presetSupported` / `generatorWhy` drive the list ("PC only" on the ten Ancient Cave / Open World presets) and the GO button.

On-device generation (Thor, 7 supported presets): rando, rando-plus, rando-hard, chaos, chaos-short, bossrush, bossrush-hard. For each, the
SHA-256 of the ROM made on the device equals the PC oracle (`som-dualscreen randomize`, same preset, seed, difficulty, options). Time: 0.17 to 0.21 s per
ROM on the Thor (progress and cancel exist but the run ends before a person can press cancel). A generated rando-hard ROM boots to the
RANDOMIZER title and the new-game name entry. Ancient Cave and Open World presets stay "PC only" until libsomrando supports them.
