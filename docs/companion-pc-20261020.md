# Companion PC build, T12: saves, savestates, filters, display, launcher, credits (2026-10-20)

Extends `docs/companion-pc-20261018.md`. All code is in `tools/companion/pc/` and `tools/companion/launcher/`.

## What was added

| Feature | Files | Notes |
| --- | --- | --- |
| Battery saves | `som_pc.c` (`srm_*`) | SRAM per ROM, flushed every 2 s when it changed and on exit (SDL turns SIGTERM/SIGINT into a quit). The previous session is kept as `.srm.bak`. Written through `pc_spit` (temp file + rename). |
| Savestates | `pc_states.[ch]`, `test_states.c` | 10 slots, thumbnail + timestamp, per-ROM directory, versioned and checksummed, corruption-safe. |
| Retro filters | `pc_shader.[ch]`, `pc_sdl.[ch]` (`pc_fx_*`), `test_shader.c` | none / sharp-bilinear / scanlines / CRT-lite / LCD grid. SDL renderer only (no GL shaders), integer arithmetic only, so golden hashes are stable. |
| Display settings | `pc_logic.[ch]`, `pc_sdl.[ch]`, `pc_menu.[ch]` | 720p/900p/1080p/1440p/2160p windowed presets, integer scaling, aspect (wide 46/27/17, 4:3), fullscreen desktop/exclusive, vsync. All persisted in the config. |
| In-game menu | `pc_menu.[ch]`, `test_pcmenu.c` | Pad- and keyboard-navigable. Pages: root, save, load, video, credits. |
| Runs list | `pc_runs.[ch]`, `test_runs.c` | Generated Ancient Cave / randomizer ROMs with their own saves and states. |
| Credits | `launcher/som_credits.[ch]`, `data/credits.json` | Start menu and Options entry, scrollable, 40 columns. Built-in fallback if the JSON is missing. |
| Launcher extras | `launcher/som_launcher.[ch]`, `som_launcher_draw.c` | Optional entries Tweaks, Display, Credits via `sl_set_extras()`. Without extras the menu is pixel-identical to before. |

## Controls

Keyboard: `Esc` or `F10` opens the menu (Esc used to quit; quit is now in the menu), `F7` quick-save, `F8` quick-load, `1`..`0` pick slot 1..10, `[` and `]` previous/next slot, `Shift+F3` cycles the filter.

Pad (hold Select): `R1` quick-save, `L1` quick-load, D-pad left/right change slot. While the chord is active, Select and L1/R1 are masked from the game.

Menu: D-pad/stick navigate (with repeat), confirm/back as in the launcher, mouse wheel scrolls credits. Saving over a used slot asks for confirmation.

## Files and keys

Savestate file `states/<rom key>/slotNN.state` (magic `SOMPCST1`, version 1, 72-byte header): header/thumbnail/payload CRC32, ROM key and core signature (`retro_serialize_size`). Written to `.part`, fsync'd, renamed; the previous good file is kept as `.bak`. A bad or foreign file is reported and never loaded.

Keys: the state key is the SHA-256 prefix (12 hex) of the ROM actually played. The battery save key is the hash of the unpatched source ROM, so toggling a gameplay (T8/T11) tweak keeps the game save but uses separate states. Each generated run is a different ROM and so has its own saves and states.

Config additions: `shader`, `state_slot`, `excl_w`, `excl_h` (plus the existing fit/aspect/vsync/size keys). CLI: `--shader`, `--integer`, `--exclusive-size`.

Runs list `runs.json`: the host imports `<rom>.sfc.json` sidecars from the generation dir (`SOM_PC_GEN_DIR`), shows the newest 4 in "Saved Randomized" with an "N older not shown" count, counts plays, and deletion moves files to `state/trash/` (nothing is unlinked). The main loop runs in rounds: launcher, game, back to launcher.

Script mode additions: `SSAVE SLOAD SINFO QUICKSAVE QUICKLOAD SLOTSET FRAMEHASH SRAMPOKE SRAMPEEK SRAMFLUSH MENU MKEY MSTATE`, `SET shader|slot|vsync|size`, autopilot kind `sram`.

## Tests

```
tools/companion/pc/run_pc_tests.sh          # test_pc, test_states, test_shader, test_pcmenu, test_runs, test_credits, test_sdl (dummy SDL drivers)
tools/companion/launcher/run_launcher_tests.sh   # launcher model + test_launcher_extras
tools/companion/pc/test_t12.sh              # Xvfb end-to-end: A keyboard, B SRAM kill/reload, C pad combos, D isolation/patched copy, E launcher credits/display/runs
tools/companion/pc/test_keys.sh, test_gameplay.sh, test_menu.sh random   # regressions
```

Results at the last full run: test_pc about 170k checks 0 failures; test_states 94; test_shader 1770; test_pcmenu 117; test_runs 91; test_credits 10; test_sdl 221; launcher 153690 checks over 108 configurations, test_launcher_extras 127; test_t12.sh 31 passed, 0 failed (about 1.5 min). A scripted SSAVE/SLOAD round trip reproduces identical frame hashes.

Test quirks: a real controller may be attached, so SDL tests set `SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT`; Xvfb has no window manager, so fullscreen is not verifiable there.

## Limitations

- (Fixed by T9, 2026-10-20: all 17 presets generate on the PC, see `randomizer-c-port-progress-20261018.md`.) Open World presets: libsomrando reported `open world hack sr_p4_hack_character_selection is not ported yet` (5 of the 17 presets); the launcher shows NOT AVAILABLE for them. Nothing to change on the PC side.
- The launcher list shows the newest 4 runs (`SL_SAVED_MAX`).
- Desktop and exclusive fullscreen, and the visual look of the filters on a real display, need the user's eyes (screenshots under Xvfb were reviewed and look correct).
- Credit URLs wrap at 40 columns.
- Other launchers (Thor, 3DS) can enable Credits/Tweaks/Display with `sl_set_extras()` and feed `sc_format()`; not done here.

## Part 2: native generation, C gameplay patcher, portable release

### In-process generation (libsomrando)

`pc/build_rando.sh` builds `libsomrando.a` (4 compiler processes at most, incremental, `CC`/`AR` for cross builds) from `tools/companion/randomizer/libsomrando` without touching its sources; it needs the upstream checkout (`SOM_RANDO_SRC`) for the build-time resources and exits 3 without it, in which case `pc/som-pc` links `launcher/somrando_stub.c` (the menu then says "No generator in this build"). `SOM_PC_NO_RANDO=1` forces the stub. The source lists live in `pc/sources.sh`.

`pc/pc_gen.[ch]` (no SDL) calls `somrando_generate` with the launcher's configuration: output `<key>.sfc.part`, checked (4 MiB, game title), SHA-256, renamed. `<key>.sfc.json` is written beside it (config, key, `output_sha256`, seconds; the shape `pc_runs` imports). The SHA-256 appears only in that file and in the `--verbose` log, never on screen. Inside the launcher (`som_pc.c` `gen_run`) generation runs in the main thread; the progress callback repaints the launcher (about every 40 ms) and watches Esc/X/Backspace/pad B to cancel (back to the tweaks screen). A generator refusal (`SOMRANDO_ERR_UNSUPPORTED`) shows NOT AVAILABLE without Retry. The preset list and the GO button follow `somrando_menu_*` support like on the 3DS. Runs go to `$SOM_PC_GEN_DIR` or `<state>/gen/` and into the runs list with their own SRAM and states (keyed by ROM hash). Generation takes 0.06 to 0.4 s.

Batch mode without a window: `som-pc --rom ROM --generate-all DIR [--gen-seed N] [--gen-preset ID]` prints one JSON line per preset (`supported`, `rc`, `sha256`, `seconds`, `path`).

### Byte identity

```
tools/companion/pc/test_rando_parity.py [ROM]    # som-pc --generate-all vs sr_cli vs the C# oracle (NativeAOT somrando), 17 presets x seeds 12345 and 777, also the config key
```
Result (after T9): 34 of 34 cases identical (17 presets x 2 seeds; before T9: 24 of 24 comparable, 12 presets; som-pc = sr_cli = C# oracle, same key as `launcher_config.py`). (The Open World presets used to be PENDING because of a static-link problem, fixed by T9.) `run_pc_tests.sh` runs it at the end (SKIP without ROM or oracle).

### Gameplay toggles through the C patcher

`pc/pc_gpc.[ch]` maps the tweaks page (`pc_gameplay`) to `gameplay_c/som_gameplay.c` (`sgp_apply`), so there is no Python or dotnet at run time. Menu entries the patcher does not know are ignored while at default and refused when changed. The page gained the presets of `gameplay_menu.json` (Vanilla, Quick Combat, Arcade, Quality of life): key `P` (pad Y) applies the next one, the title shows the matched preset or CUSTOM; a flat options object is accepted like in `som_gameplay.py`. The patched copy is cached under `<state>/gameplay/` and the battery save stays the one of the unpatched ROM.

```
tools/companion/pc/test_gpc_parity.py [ROM] [--cases N]   # C path vs tools/gameplay/som_gameplay.py: presets, every option value, random combinations; vanilla + Ancient Cave, Randomizer, Chaos, Boss Rush ROMs
```
Result: 286 identical, 0 different (170 cases x 5 ROMs; ASan/UBSan build of the C side).

### Portable release

```
tools/companion/pc/package_release.sh [--platform linux|windows] [--out DIR] [--version V] [--core FILE] [--bundle-sdl] [--no-generator]
tools/companion/pc/test_release.sh [ROM]
```
Folder `som-pc-V-linux-x86_64/`: `som-pc`, `portable.txt` (user data in `userdata/`; always on Windows), `README.txt`, `NOTICE.txt`, `data/credits.json`, `data/gameplay_menu.json`, `licenses/` (GPL-3, LGPL-2.1, libsomrando notice, SDL2 zlib when bundled), `core/README.txt`, `source/som-pc-source.tar.gz` (LGPL source offer, rebuilt by `test_release.sh` as proof). Data is looked up in `$SOM_PC_DATA`, `<exe>/data/`, `<exe>/`, then the source tree. There are no shader files: the filters are CPU code in the binary. The Snes9x core is not shipped (non-commercial licence): `core/README.txt` tells how to build it, `--core FILE` copies a local build in for personal use.

`test_release.sh`: 12 checks (layout, notices, no ROM/save/state/image files, relinkable source archive, a packaged run generating a known hash, data found next to the executable, portable user data, C patcher, nothing written to `$HOME`).

Known finding (fixed by T9, 2026-10-20: the tables are rebuilt from the user's ROM at run time and `libsomrando/tests/scan_rom_bytes.py` finds no ROM window in `som-pc`), ROM-derived bytes: libsomrando's build-time resources (upstream's Vineshroom and event tile data generated from the upstream checkout) are linked into every binary that contains the library (PC, 3DS, Android): about 3.7 KB of game tile data was matched against the owner's ROM inside `som-pc`; no other file of the release matches. It needs a change in libsomrando (read those tiles from the user's ROM at run time), which was out of this item's scope.

### Windows

`pc/pc_compat.h` isolates the OS calls (LoadLibrary, `_mkdir`, `_commit`, `MoveFileEx` for atomic replace, `localtime_s`, a QPC clock). Fork/exec and `/proc` use are gone. `pc/ci/crosscheck_windows.sh` cross compiles libsomrando and every source with mingw-w64 and links: only `SDL_*` and `WinMain` stay unresolved (SDL2 mingw package import library), 253 references. A real `.exe` was not produced or run here (no SDL2 mingw development package on this machine), so the Windows build is compile-checked, not run-tested. Recipes: `package_release.sh --platform windows` (MSYS2, or Linux with `SDL2_PREFIX=<SDL2 mingw dir>`) and the GitHub Actions workflow `pc/ci/som-pc-release.yml` (Linux and Windows jobs; copy it to `.github/workflows/`).

### Tests, robustness

`pc/test_t12.sh` now polls log lines and files instead of fixed sleeps, scales key delays by the load average (`T12_SLOW=N` forces a factor), and re-runs a failing section up to three times with a larger factor. Its Ancient Cave run is made by the in-process generator (no dotnet state), and a new section G drives the launcher end to end (preset, generate, play, quick save). 37 checks pass in about 19 s, also with 14 busy loops running. `run_pc_tests.sh`: test_pc 170162, test_states 94, test_shader 1770, test_pcmenu 117, test_runs 91, test_credits 10, test_sdl 221, plus the two parity scripts. Launcher tests unchanged (108 configurations, 127 extras).
