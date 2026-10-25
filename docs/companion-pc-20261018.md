# PC version: 1080p widescreen with L1 R1 L2 R2 slots (board item T10)

Date: 2026-10-18. Code: `tools/companion/pc/` (launcher `som-pc`). A self-contained SDL2 frontend that hosts the patched Snes9x widescreen libretro core
(`companion-widescreen-core-20261017.md`) and puts the dual-screen quick slots on the PC as four small slots bound to L1, R1, L2 and R2.
No ROM is shipped, patched or copied; ROM-derived assets (the UI pack) are read from `~/.local/state/som-decomp/`, never from the repository.
Related: `companion-direct-20261017.md` (somd_* direct control, sections 11-14), `companion-ux-20261017.md` (section 3l, shoulder slots),
`companion-startmenu-20261018.md` (T9 menu, reused as the start screen).

## What it is

```
tools/companion/pc/som-pc --rom "Secret of Mana (USA).smc"          # 1920x1080 window, wide 46, FIT
tools/companion/pc/som-pc --menu --rom "Secret of Mana (USA).smc"   # start menu: SoM Original / Create Randomized
tools/companion/pc/som-pc --help
```

`som-pc` is a bash script: on first use (and whenever a source is newer) it compiles `som_pc.c`, `pc_logic.c` and the shared companion C files
with `gcc` and `pkg-config sdl2` into `~/.local/state/som-decomp/pc/build/som-pc`, then runs it. Needs `gcc`, `libsdl2-dev` and the widescreen core
(default `~/.local/state/som-decomp/companion-20261017/widescreen/pc-x86_64/snes9x_widescreen_libretro.so`, built by `widescreen/build_core.py`; `--core` or
`config.json` override it). Without the optional UI pack (default `.../companion-20261017/assets/som_ui_pack.bin`) the overlay and the picker use the built-in art.

### Why this design

- **SDL2 + C, one process.** The ux renderer (`som_ui_draw.c`), the direct layer (`som_direct.c`) and the start menu are plain C; a C host links them directly, so
  the overlay is drawn by the same code and the same pack as the Thor and the cast, queue and target logic are the same calls (`somd_queue_cast`,
  `somd_can_cast`, `som_ux_shoulder_press`). SDL2 2.32 and its dev headers were already installed (Linux, KDE Wayland, XWayland); pygame was not.
  A RetroArch flatpak cannot give direct WRAM access (`RETRO_MEMORY_SYSTEM_RAM`) or an overlay driven by the game state.
- **The picker is the Thor's quick-slot editor.** It is not re-implemented: the panel is `som_ux_draw()` on the quick-slot editor page (MENU / WEAPON / ITEM / SPIRIT /
  SPELL tabs, spirit first then its spells, the SELECT / NEAR / LESS HP / ALL chooser, CLEAR SLOT), centred as a modal over the dimmed, paused game. Mouse taps go to
  `som_ux_touch`; keyboard and gamepad move a focus box with `pc_nav_pick` over `som_ux_hit_list` (the tap targets of the layout). Only the four shoulder slots show
  (`som_ux_set_four_slots`); slots 4..7 of the Thor do not exist on the PC.
- **Overlay = `som_ux_draw_shoulders`** (new, in `ux/som_ui_draw.c`): the four slots in a row on a transparent canvas, drawn with the same `draw_slot` as the Thor (icon,
  permanent button label, orange rim, flash animation, queued clock, target-policy letter, dimmed when `somd_can_cast` says it cannot be used), plus an entry-name row and
  the queue / refusal chip above. SDL blends it over the game at an integer scale (2 at 1080p for "small", 3 for "medium").
- **Default 46 px, FIT, HUD visible.** 46 px gives 348 x 224, aspect 1.8125, wider than 16:9, so FIT fills the width of a 1920x1080 window with 10 px bars above and below and nothing
  is cut. The original HUD stays (there is no second screen); `--hud hide` uses the core's `hide_except_ring`.
- **Fire on release, hold to assign.** A slot button fires when it is released; held for 0.7 s it opens the picker for that slot instead (a progress bar fills under
  the slot from 0.18 s). `--fire press` fires on press and leaves F2 / the right stick click as the only way to open the picker. L and R never reach the game
  (the game never reads them in any mode, `companion-bindings-20261017.md`), so the soft-reset chord L+R+Select+Start cannot happen.

## Files

| File | Role |
| --- | --- |
| `tools/companion/pc/som-pc` | launcher / builder |
| `tools/companion/pc/som_pc.c` | the SDL2 host: libretro glue, window, renderer, audio, keyboard and controllers, direct-layer dispatch, picker, start menu, scripted and autopilot test modes |
| `tools/companion/pc/pc_logic.[ch]` | pure logic: config and profile JSON, picture fitting, shoulder press / hold machine, gamepad navigation, pixel hash |
| `tools/companion/pc/pc_sdl.[ch]` | SDL side: controller hot-plug and mapping (`pc_pads_*`), audio device with underrun / overflow / device-loss handling (`pc_audio_*`), window mode with fallbacks (`pc_window_set_mode`) |
| `tools/companion/pc/pc_store.[ch]` | atomic file writes with a `.bak` of the last good file and a `.bad` quarantine, used for `config.json`, the slot profiles and `gameplay.json` |
| `tools/companion/pc/pc_gameplay.[ch]` | the gameplay tweaks page (T8 options): reads `tools/gameplay/gameplay_menu.json`, keeps the values, writes the options file, draws the page |
| `tools/companion/pc/test_pc.c`, `test_sdl.c`, `run_pc_tests.sh`, `golden_hashes.txt` | `test_pc` (170,139 checks, no SDL: config / profile / store / gameplay page / fit / press-hold machine / overlay goldens) and `test_sdl` (194 checks on the SDL dummy video and audio drivers with virtual joysticks and synthetic events); both ASan + UBSan |
| `tools/companion/pc/test_gameplay.sh` | the gameplay page in the real menu under Xvfb, then the patched ROM starts |
| `tools/companion/pc/check_all.py` | end-to-end checks on vanilla, Magno and a generated Ancient Cave ROM (headless) |
| `tools/companion/pc/scripts/*.txt`, `run_script.sh`, `make_fixture.sh`, `test_roms.sh`, `test_window.sh`, `test_keys.sh`, `test_menu.sh` | scenarios and the Xvfb window tests (below) |
| `tools/companion/ux/som_ui_draw.[ch]` | added `som_ux_draw_shoulders`, `som_ux_shoulders_size`, `som_ux_hit_list`, `som_ux_set_four_slots`, field `qe_four` (nothing existing changed) |

## Options

| Option | Values | Notes |
| --- | --- | --- |
| `--rom PATH` | US v1.0, Magno, generated 4 MiB ROM | 2 or 4 MiB (a 512 byte copier header is skipped). Remembered in `config.json` |
| `--menu` | | start menu first (also when no ROM is known) |
| `--wide` | `off 17 27 46` | core option `snes9x_som_widescreen`; the scale is fixed by the configured width so a map the core falls back to 256 px keeps its size with side bars |
| `--fit` | `fill fit integer stretch` | FILL covers the window (rows are cut), FIT whole picture, INTEGER 4x high / 5x wide at 1080p, STRETCH whole frame over the window |
| `--hud` | `show hide` | `snes9x_som_hud` off / hide_except_ring |
| `--windowed`, `--fullscreen`, `--exclusive` | | borderless desktop fullscreen, or exclusive (closest 1920x1080 mode); F11 toggles window / borderless, Shift+F11 exclusive |
| `--size WxH`, `--no-vsync`, `--volume N`, `--no-audio` | | defaults 1920x1080, vsync on, 100 |
| `--overlay` | `off small medium` | |
| `--corner` | `tr br bl bc` | default top right: the HUD occupies the bottom of the centre picture |
| `--no-names`, `--fire press|release`, `--profile NAME` | | |
| `--state FILE` | | load a core save state at start |
| `--save-config` | | write the options to `config.json` (also written on a normal exit) |
| `--script FILE --headless WxH`, `--autopilot FILE` | | test modes, see Verification |

## Controls

| Input | Keyboard | Gamepad (SDL game controller) |
| --- | --- | --- |
| Move | arrows | D-pad, left stick |
| SNES B / A / Y / X | Z / X / A / S | south / east / west / north (the position of the SNES buttons) |
| Start / Select | Enter / Backspace or Right Shift | Start / Back |
| Slot L1 R1 L2 R2 | Q W E R | L1, R1, left trigger, right trigger (0.6 on / 0.4 off) |
| Assign a slot | hold the slot key 0.7 s, or F2 | hold the slot button 0.7 s, or right stick click |
| Options page | F1 | left stick click, Guide |
| Picker: move, confirm, back, tab | arrows, Enter or Z, Esc or X or Backspace, PgUp / PgDn | D-pad, A, B, L1 / R1 |
| Picker with the mouse | left click = tap, right click outside the panel = back | |
| Overlay with the mouse | left click = use the slot, right click = assign | |
| F3 fit, F4 wide, F5 overlay size, F6 HUD | | |
| F7 / F8 | save / load a state (one per ROM, `~/.local/state/som-decomp/pc/states/`) | |
| F9 mute, P pause, F11 fullscreen, F12 screenshot (BMP in `.../pc/shots/`), Esc quit | | |

Game saves (battery SRAM) are loaded at start and written on exit and every 20 s when changed: `~/.local/state/som-decomp/pc/saves/<sha256 prefix of the ROM>.srm`, so each
generated seed has its own file. There is no remapping UI for the game buttons; the SDL game controller database is the mapping layer (below).

### Controllers: hot-plug and mapping

- Up to four controllers, tracked by SDL instance id (`pc_pads_event`): a duplicate ADDED (SDL repeats them for pads open at start-up), an event for a device that is gone and a fifth pad are
  ignored; an unplug closes only that pad, clears its trigger state and cancels any slot button that was held (`pc_sh_cancel`), so nothing fires or stays stuck.
- Mappings: SDL's built-in database, then `SDL_GAMECONTROLLERCONFIG`, `$SOM_PC_GAMECONTROLLERDB` (a file) and `~/.config/som-decomp-pc/gamecontrollerdb.txt` (`SOM_PC_CONFIG` relocates it), standard
  SDL db lines (`GUID,name,a:b0,...`; comments and bad lines are skipped). A joystick with no entry that looks like a pad (2 axes, 8 buttons, 1 hat) gets a generic xinput-like layout
  so it still plays, and its GUID is printed with a hint to add a proper line; anything smaller is ignored and says so.
- Triggers use 0.6 on / 0.4 off hysteresis, the left stick a 16000 dead zone (`pc_trigger_step`, `pc_stick_dir`).

### Window, audio and bindings

- F11 toggles window <-> borderless desktop fullscreen (`SDL_WINDOW_FULLSCREEN_DESKTOP`, no video mode change). Shift+F11 / `--exclusive` ask for the exclusive 1920x1080 mode; when SDL refuses it the
  window falls back to borderless, then windowed (`pc_screen_fallback`) instead of failing. The windowed size is remembered on exit and comes back after fullscreen.
- Audio: the queue is the clock. An empty queue while the game runs is counted as an underrun and refilled with a 4-frame burst; a queue deeper than 9 frames (stall, suspended device) is dropped so the
  sound does not lag. A device that stops playing is reopened (immediately, then after 1, 3, 8 s) and after that the game paces by timer without sound. F9 and volume 0 now queue silence, so
  pacing does not change (before, a muted queue emptied and the loop ran three frames per iteration).
- Slot bindings (`profiles/<variant>.json`), `config.json` and `gameplay.json` are written through `pc_store`: temp file, fsync, rename, previous file kept as `.bak`. A file that is truncated or
  not a complete JSON object is moved to `.bad` and the `.bak` is used (stderr says which); bindings are saved as soon as the picker assigns a slot and on exit.

### Gameplay tweaks (T8 options)

In the start menu press G (keyboard) or Back / Select (pad): a page lists the options of `tools/gameplay/gameplay_menu.json` by group (Weapon, Enemies, Heroes), advanced ones behind a toggle,
with the help text of the selected option. Left / Right (or D-pad) change a value, PgUp / PgDn step numbers by 10, Delete (X on the pad) restores the default, A = reset all, Esc = back. The
values are saved in `~/.config/som-decomp-pc/gameplay.json` (the options file `som_gameplay.py apply --options` reads) and the menu shows `GAMEPLAY TWEAKS (ACTIVE)` when any differs from vanilla.
When the game starts from the menu (original or generated ROM) and some value is not the default, `som_gameplay.py apply --on-conflict skip` writes a patched copy to
`~/.local/state/som-decomp/pc/gameplay/` (cached per ROM and option set; the ROM on disk is untouched) and that copy is played with the slot bindings of the original ROM. If the tool fails,
the ROM is played unchanged and stderr says so. `--gameplay FILE` applies an options file on a direct `--rom` start. Nothing in `tools/companion/randomizer/` is read or changed.

## The four slots

- Visible in free field play, the ring, shops and Flammie flight; hidden in cutscenes, fades, the title and name entry. Toggled and resized with F5 / `--overlay`.
- Each slot shows the icon of its entry, the button name, the entry name below, the target-policy letter (N near, A all, L less HP), a flash for 0.3 s when it fires
  (check for equip / item / menu, star for a cast, star with a clock for a queued cast that finally fires, cross when refused), a clock while a cast waits in the queue and
  the dimmed look when it cannot be used now (no MP, KO, spirit not owned, no item). The queue / refusal chip (QUEUED, CANCELLED, DROPPED: NO MP, BAD_TARGET ...) appears above.
- Entries (same as the Thor): weapon (equip on the controlled hero), item (first record holding it, with a target policy), spell (`somd_queue_cast`: fires at once, or waits for
  a transient reason such as the caster swinging; pressing again while it waits cancels it), spirit (opens that spirit's spell list in the game's own ring), ring and config screen
  hotlinks (`somd_open_menu`), with the video of the menu commands hidden as on the Thor. A slot used while the ring is open closes the ring first.
- Target policy is asked when assigning: SELECT shows the target cards of the UX layer as a modal when the slot is used (plain enemy cards, generic silhouette), NEAR, ALL (enemies or
  party) and LESS HP act at once; a policy that does not fit the entry is not offered.
- Persistence: `~/.config/som-decomp-pc/profiles/<variant>.json` (`variant` = size and header checksum of the ROM, as the Android bindings; `--profile NAME` selects another file),
  `~/.config/som-decomp-pc/config.json` for the options. `SOM_PC_CONFIG` and `SOM_PC_STATE` relocate them.

## Start menu (T9)

`--menu`, or no ROM known: the T9 menu (`launcher/som_launcher*.c`, drawn at the window size) with keyboard (arrows, Enter or Z, Esc or X, S random, A reset, Space start, PgUp/PgDn),
mouse (tap, wheel) and gamepad. **SoM Original** starts the configured ROM. **Create Randomized** picks a preset, tweaks, seed, then GENERATE looks up
`~/.local/state/som-decomp/companion-cli-20261017/gen/<key>.sfc` (`SOM_PC_GEN_DIR` overrides) and otherwise runs `som-dualscreen generate --config ... --rom <configured US ROM> --json`
as a child process (about 1.5 s; the menu keeps drawing, the bar is time based), then starts the generated ROM. The configured ROM stays the original in `config.json`.
The generator needs `dotnet` and the headless build (`som-dualscreen setup-randomizer`); the model's seam to libsomrando is unchanged.

## Verification (2026-10-18, this machine: Linux, KDE Wayland session, 2560x1440 display, 16 cores)

| Check | Result |
| --- | --- |
| `run_pc_tests.sh` (ASan + UBSan) | 20,105 checks, 0 failures: config / profile round trips and invalid input, fit rectangles for FIT / FILL / INTEGER / STRETCH at 1080p and 720p (46 px FIT = 1920x1059, 27 px FIT = 1744x1080, fallback frame keeps size and centre), the press / hold machine (including a 20,000 step random run), gamepad navigation, 15 golden hashes of the overlay (empty, filled, flash, refused, queued, unavailable, notices, scales 1-3, with and without names) and no cut text |
| `check_all.py` (headless, 13 s) | 19 PASS: vanilla session (hold L1 opens the picker; Fireball with NEAR assigned through the picker tabs; cast spends MP; second press while the caster is busy is queued (`queue = [0, 1]`) and fires later, MP 47 -> 43; item with LESS HP on R2 through the picker, 7 -> 6), and on **vanilla, Magno and a generated Ancient Cave ROM** (`som-dualscreen generate`, `ancientcave-short`, seed 12345): state loads with a 348 px wide frame, picker assignment, slot cast spends MP, item slot uses the item |
| Advanced state `adv/advanced_equipped.state` | loads in the widescreen core (frame 348 x 224 at once, 310 after `SET wide 27`) |
| Screenshots at 1920x1080 | wide 27 (310 x 224 in a 1744 px picture) and wide 46 (348 px, 1920 px wide): `~/.local/state/som-decomp/pc/shots/s3_*`, `s4_*`; picker pages, queued chip, dimmed slots |
| Real window under a private X server (`test_window.sh`, `test_keys.sh`, xvfb-run, SDL x11, OpenGL via llvmpipe) | virtual SDL game controller: hold L1 -> hold bar -> picker -> D-pad / A navigation to SPELL > Salamando > Fireball > NEAR -> bound; L1 tap casts (MP 47 -> 45). Trigger axes and R1 on the controller: R2 and L2 use their items (7 -> 6 each), R1 equips its weapon. Keyboard through xdotool: F3 / F4 / F5 cycle fit, wide and overlay size, a 1.1 s hold of Q opens the picker, Esc closes it, F12 saves screenshots, Esc quits |
| Start menu in the real window (`test_menu.sh`) | SoM Original starts the game; Create Randomized > Ancient Cave > Generate and play runs `som-dualscreen generate` for real (new seed each run) and the generated ROM boots ("ALSO MOPPLETON" credit) |
| `run_pc_tests.sh` (2026-10-18, after the T10 close-out) | test_pc 170,139 checks, 0 failures; test_sdl 194 checks, 0 failures (virtual joysticks: plug / unplug storm of 60 rounds, duplicate and bogus events, five pads, db-file mapping, generic layout, trigger hysteresis on real axes; audio underrun / overflow / suspended device / lost device and give-up; window windowed -> borderless -> exclusive fallback -> windowed, 200 random toggles) |
| `test_gameplay.sh` (Xvfb, real menu, real vanilla ROM) | the page opens with G and draws, a value changes and is saved to `gameplay.json` (`.bak` kept), Return starts the game from a ROM patched by `som_gameplay.py` (26 bytes differ from the original), profile `200000-cff0f8e2` is the original's |
| Audio | SDL disk driver (raw PCM file, nothing played): 18.8 s of audio for an 18 s run, music present, silence while the picker pauses the game. The real audio device (PipeWire / Pulse backend) was opened once for 3 s at volume 3 % with a steady 60.4 emulated frames per second |
| One short visible run on `:0` | 1920x1080 window then borderless fullscreen (2560x1440 picture, FIT) then window again, OpenGL, vsync: overlay and HUD correct |

Timings (`stats` line printed on exit; emulation = core + direct layer + UX update per frame, render = texture upload and composition; the budget is 16.6 ms):

| Run | Emulation avg / max | Render avg / max | Frames per second |
| --- | --- | --- | --- |
| Headless, software renderer, full 1080p pipeline, wide off / 27 / 46 | 0.57 / 0.95 / 0.96 ms (max 1.5-2.3) | 2.9 / 3.6 / 3.7 ms (software scaling) | 286 / 222 / 216 (3.6x real time) |
| Xvfb window, llvmpipe OpenGL, wide 46, vsync | 1.3 ms (max 4.2) | 0.2 ms (max 3.3) | 60 paced by the audio queue |
| Real display `:0`, OpenGL, vsync, wide 46 | 1.3 ms (max 5.3) | 0.26 ms (max 2.0) | 62.8 over 4.5 s including two window mode changes |

Pacing: the audio queue is the clock (3 frames of latency target), a loop iteration runs 0..3 core frames; without an audio device a timer is used. The core runs at 60.0988 fps.

### Not verified

- A physical gamepad (a virtual SDL controller drives the same `SDL_GameController` code path) and the real speakers (content and pacing checked, audibility not).
- Exclusive fullscreen (`--exclusive`) on the real display: only its refusal and fallback path is tested (SDL dummy driver refuses it); under Xvfb without a window manager F11 changes nothing.
  Borderless fullscreen was run on `:0` through XWayland; native Wayland (`SDL_VIDEODRIVER=wayland`) and variable refresh rate were not tested.
- Long play sessions: no soak run; the scripts cover about 750 emulated frames each, the real window runs 10-20 s.
- The in-game ring, shops and Flammie flight with a slot pressed (the deferred close-ring path follows the Android code but was only exercised through `somd_close_menu` unit paths of the direct layer, not by a PC scenario).
- Generation without `dotnet` or without the headless generator build: the menu shows the generator's error text; not exercised.

## Limits and known issues

- Spell names of the Magno ROM show as `SPELL n` (the UX name block lookup has no Magno spell table); weapons and items print their Spanish names.
- The options page (F1) is the Thor's: WIDE, FALLBACK, HUD HIDE and TOP FIT act; CORE, UI SOUNDS, HAPTICS and LANGUAGE are inert on the PC. The menu click sounds (`somd_sound`) are not wired.
- The picker has no long-press arming of list entries (assign always goes through the slot editor). Item targets of SELECT use the generic silhouette (no enemy sprite pack).
- The default keyboard layout is fixed; the game's buttons cannot be remapped, and L / R are never sent to the game.
- Fire on release adds one button release (about 80 ms) of latency; use `--fire press` to remove it.
- Hi-res screens (title, name entry) are pair-averaged by the core when widescreen is on; sprites are not extended (pop at the 256 px border) - see the core document.
- The overlay at the top right covers about 100 px of the centre picture at 46 px wide on a 16:9 window; the corner is configurable.
- `tools/companion/ux/test_som_ui_draw.c` reports 12 golden-hash and editor-page failures that are independent of this work (the same 12 fail with the PC additions removed; the renderer is under work).
- A generated ROM's progress bar is time based; cancelling the menu generation lets the child finish on its own.

## Reproducing

```
tools/companion/pc/run_pc_tests.sh            # unit tests + overlay goldens
python3 tools/companion/pc/check_all.py       # three ROMs headless (builds the Ancient Cave fixture when missing)
tools/companion/pc/test_window.sh ROM STATE tools/companion/pc/scripts/ap_gamepad.txt   # real window, virtual controller
tools/companion/pc/test_keys.sh ROM STATE     # real window, xdotool keyboard
tools/companion/pc/test_menu.sh original|random ROM
tools/companion/pc/run_script.sh ROM tools/companion/pc/scripts/s7_bench.txt            # frame times
```

The state files are local fixtures: `companion-20261017/adv/advanced_equipped.state` (vanilla), `.../work/party3_magno.state` (Magno), `pc/ac12345_party.state` (built by `make_fixture.sh`
by playing the new-game flow through `som-pc --script`, on the ROM that `som-dualscreen generate` wrote). The scripted mode accepts LOAD, RUN, PAD, SLOT, BIND, PICKER, UITAP, UINAV, UIOK, UIBACK,
WAIT, SET, SHOT, STATE, BENCH, POKE, SAVESTATE, CLEARBIND (see `som_pc.c`).

## Stages (T10)

| Stage | Weight | State |
| --- | ---: | --- |
| 1. Host: libretro core, window / fullscreen, fit modes, wide option, audio, keyboard and controller | 25 | done |
| 2. Four-slot overlay over the game with the direct layer (cast, queue, item, weapon, menus) | 25 | done |
| 3. Picker with target policy, keyboard / pad / mouse navigation, persistence per ROM | 25 | done |
| 4. Start menu (T9) as the front screen, generation through `som-dualscreen` | 10 | done |
| 5. Verification on three ROMs, tests, documentation | 15 | done, minus the items above that need the user's hands (physical pad, exclusive fullscreen, speakers) |
| 6. Close-out: hot-plug and mapping robustness, desktop fullscreen with fallbacks, audio underrun handling, atomic persistent bindings, gameplay tweaks page, headless SDL tests | - | done (2026-10-18) |

Board: 96 %. What is left needs the user's hands: a physical pad (and its db line if SDL does not know it), the speakers, exclusive fullscreen and native Wayland on the real display.
