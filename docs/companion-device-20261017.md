# Companion on the real AYN Thor and on an emulated 3DS (board item T4, stages 3 and 4)

Date: 2026-10-17. Code: `tools/companion/android/`, `tools/companion/n3ds/`, test-state builder
`tools/companion/android/advstate/`. Earlier design and host-side results: `docs/companion-platforms-20261017.md`,
`docs/companion-prototype-20261017.md`. Screenshots (both displays, `adb exec-out screencap -d <id>`) are in
`/home/cidwel/.local/state/som-decomp/companion-20261017/device/` (not in the repository: they contain game frames).

Device: AYN Thor, serial `5d1ee37e` (the only device used; a second, unauthorised phone on the bus was never touched).
Display ids: top `4630946441858561667` (1920x1080 landscape, Android display 0), bottom `4630946482288158084`
(1240x1080 landscape, display 4 "Screen-2", `FLAG_PRESENTATION`).

The 3DS part ran on **Azahar 2126.2 (an emulator on the Thor's Snapdragon 8 Gen 2)**. It is neither real 3DS hardware nor
old-3DS speed; see section 5 for what that does and does not tell us.

## 1. Summary

| Item | Result |
| --- | --- |
| APK installs and starts | yes (`org.somdecomp.companion`, debug build, 2.1 MB with the arm64 + x86-64 snes9x core) |
| Bottom screen shows the Presentation | yes, **only after switching the Thor's vendor "Dual-screen display" mode** (section 2.1) |
| Touch on the bottom screen | yes (`input -d 4 tap`): tabs, party rows, item, weapon and magic cells all land correctly |
| Core loads, ROM runs, aspect | yes; 256x224 (or 512x224 hi-res) drawn at 8:7 pixels, 1410x1080 px, constant in every mode |
| Speed | 60.0 fps emulation (120 frames per 2.0 s window, 25,000+ frames in one run), core 5 to 9 ms per frame, no drops |
| Audio | AAudio stream `started` on the speaker, 0 underruns in steady state; **audibility needs a human** |
| Input | injected key events reach the core in 3 to 13 ms on average (max 16.5 ms); physical pad layout: see 2.6 |
| Macros end to end | use item, equip weapon and cast magic all finish with `DONE` on the real game, values read from WRAM before and after (section 3) |
| 3DS build in Azahar | boots, runs the game and shows the status screen; speed 50 ms per frame **in the emulator's own timing** (section 5) |
| Advanced test state | level-50 party, all weapons, all magic, full items, in the Pure Land (map 215), one tap to load (section 4) |

READY FOR USER TEST: see section 6 for the launch recipe.

## 2. Thor findings

### 2.1 The bottom screen stays black until the vendor mode is changed

First launch: the Presentation window existed on display 4 (`dumpsys window`: `mDisplayId=4`, a 1080x1240 buffer on layer
stack 4) but `screencap -d <bottom>` was black. SurfaceFlinger showed a second layer above ours on display 4:
`primaryScreenTopLayout`, a full-screen `TYPE_BOOT_PROGRESS` (2021) window owned by the vendor service
`com.odin.dualscreen.assistant`, covering Screen-2 whenever `Settings.System dual_screen_display_mode` is `1`
("Only top screen" in the vendor app's strings). Setting the value to `0` ("Dual screen") removed the cover and our
Presentation became visible immediately.

I changed that one system setting for the test (`adb -s 5d1ee37e shell settings put system dual_screen_display_mode 0`);
the original value was `1` and is recorded in `device/settings_to_restore.txt`. **The device is currently left at `0`**
so the app can be tried; restore with `settings put system dual_screen_display_mode 1` or through the Odin settings. A user
of the app will therefore have to select "Dual screen" in the vendor panel once; the app cannot do it itself (it would need
`WRITE_SETTINGS`). Worth putting on the first-run screen.

### 2.2 Bugs found on the device and fixed

| Bug | Cause | Fix |
| --- | --- | --- |
| Every title, name-entry, menu and dialogue screen was black or frozen (the user's report) | snes9x switches to **512x224 hi-res frames** on those screens and `video()` dropped every frame wider than 256 (the stats line showed `core frame 512x224` with 100+ rejected frames per 2 s) | frames are kept at the core's width (256 or 512), interlaced 448-high frames are averaged to 224, drawn as 256 pixels of 8:7 |
| Camera "zoom" when a dialogue opened | the top view cropped rows 199+ (HUD) in all modes, so geometry depended on the mode | geometry is now constant: always the whole frame at 8:7. The HUD is hidden only in free field play with the core's own `snes9x_layer_3 = disabled`, entered after 30 stable frames of WRAM mode FIELD and released immediately on any other mode (`GET_VARIABLE` / `GET_VARIABLE_UPDATE` in the JNI host) |
| Pixel aspect was 1:1 | `GameView` fitted 256x199 uniformly | width scaled by 8/7 (1410x1080 on the Thor top display) |
| Fast taps lost | `adb shell input keyevent` presses and releases inside one frame, so the core never saw the button | a press is latched for two frames |
| Navigation bar on the top display, text overlay stealing a row | no immersive mode, info text in the layout | immersive (`WindowInsetsController`), keep-screen-on flag, overlay on top of the game |
| No way to measure | no counters | a stats line every 120 frames in logcat (`somcompanion`) and on screen: core ms per frame, input to core latency, audio underruns, mask state, mode, map, HP/MP; macro progress lines |

Added for testing: a debuggable-build broadcast `org.somdecomp.companion.DEBUG` (`--es poke "ADDR HEX"` writes WRAM,
`--ei pad MASK --ei ms N` holds a pad mask) and a save-state loader (`Native.loadState`, `state.sav`, section 4).

### 2.3 Hiding the HUD: what snes9x allows

With BG3 off the HUD frames, HP numbers and gauges disappear and the world below shows normally (screenshot
`g_field_top.png`). **Limitation: the three party portraits are sprites in the same rows and remain** (bottom left of
`party_top.png`). The libretro core has no option for sprite ranges, and repainting those rows from a world-only render
was ruled out. The app flag `--ez hide_hud false` keeps the whole HUD instead (`adb shell am start -n
org.somdecomp.companion/.MainActivity --ez hide_hud false`). snes9x2005 on the 3DS has no layer option at all, so the 3DS
build always shows the HUD.

Geometry check (`geo_field_top.png`, `geo_ring_top.png`, `geo_closed_top.png`): the non-black rectangle is 255..1665 by 0..1080
in field (HUD masked), with the weapon ring open (HUD back, dimmed world, title bar) and after closing it; a background
patch of field and closed is pixel-identical (difference bbox none), so the world neither moves nor rescales. The same
rectangle was measured on the title, name entry (`nameentry_top.png`, hi-res), the opening cutscene dialogue and a field map.
I could not make an in-field NPC dialogue appear in the time available; the opening cutscene dialogue and the ring (both
are non-field modes, `$D0 != 0` and `$7D == 1`) cover the same code path. A shop or sign box that opens without raising
`$D0` or `$7D` would still be seen with the HUD mask on for one frame at most before the next mode read; not observed.

### 2.4 Measurements

| Quantity | Value | How |
| --- | --- | --- |
| Emulation rate | 60.0 fps: 120 frames per 2.0 s window; 25,080 frames over about 7 minutes in one run | in-app counter (`somcompanion` logcat) |
| Core time per frame | 5 to 9 ms average, max 9 to 13 ms (budget 16.6 ms) on the title, in the opening scenes and in the Pure Land | in-app, around `retro_run` |
| UI drawing | `dumpsys gfxinfo`: 1207 frames in 10 s (120 Hz panel), p50 6 ms, p90 13 ms, p99 15 ms, 1 janky frame (0.08 %), 0 missed vsync | `gfxinfo ... reset` then 10 s |
| Audio | `dumpsys audio`: `AudioPlaybackConfiguration ... type:AAudio u/pid:10147/... state:started usage=USAGE_MEDIA deviceId:2`; AudioFlinger lists an active track for the app; underruns 0 per 2 s window in steady state (9 and 2 in the first window after a load) | not audible from here |
| Input to core | average 3 to 13 ms, max 16.5 ms from `dispatchKeyEvent` (or debug hook) to the frame that consumes it | in-app timestamps; adb injection overhead and panel latency are not included |
| Stability | one run of 25,000+ frames, title to field to Pure Land, ring, three macros, state loads, no crash, no `FATAL` in logcat | |

### 2.5 Controls exercised

`adb shell input keyevent KEYCODE_BUTTON_START / A / X / B` (the keys the Thor pad produces) were used to start a new game,
name the heroes and skip the opening: START and A (SNES B) work in the menus and cutscenes. In free field, the ring opens
with `KEYCODE_BUTTON_X` (SNES Y) and `KEYCODE_BUTTON_B` (SNES A) cancels (`geo_ring_top.png`: weapon ring with "MANA SWORD
59 -> 188" in the Pure Land state).

### 2.6 Not verifiable from the host

The Thor pad is the input device "Odin Controller" (`Vendor_2020_Product_0111.kl`: standard Android names; D-pad as both
`DPAD_*` keys and `HAT_X/Y`; both are handled). Physical buttons cannot be pressed from here. One thing to check by hand: the
kernel codes `0x133`/`0x134` are mapped by Android to `BUTTON_X`/`BUTTON_Y`; if the Thor prints them in Nintendo positions
(north = X), then the app's `BUTTON_X -> SNES Y` and `BUTTON_Y -> SNES X` would be swapped relative to the SNES layout
(Y west, X north). A one-line change in `MainActivity.bit()` fixes it once the user reports which one feels wrong.

## 3. Macros on the real device (WRAM before and after, read through the in-app log and the status panel)

| Macro | Start state | Result |
| --- | --- | --- |
| Use item (Candy) | Potos, boy HP 20/50 (WRAM written by the debug hook), Candy x3 | 258 frames, result `done`, HP 20 to 50, Candy x3 to x2 (`item_before_bottom.png`, `item_after_bottom.png`, header `DONE`) |
| Cast magic (girl, Gnome, first spell) | Pure Land, girl MP 47/47 | 490 frames, `done`, MP 47 to 43 (`magic3_bottom.png`) |
| Equip weapon (axe, boy) | Pure Land, boy with the sword | 225 frames, `done`, boy weapon SWORD to AXE on the status panel (`equip2_bottom.png`) |
| Header | `RUNNING` while pressing, `DONE` after; `LOCKED` and `EVENT` appear while a cutscene runs (`item_run_bottom.png`) | |

The status panel shows real WRAM values (HP, MP, level, weapon, item quantities, spirit availability): for example
`BOY L50 HP 523/524`, `GIRL L50 HP 416/417 MP 47/47` in `adv_bottom.png`.

One unexplained event: the very first item tap, issued after the fixture state had been idling in Potos for about a minute,
was followed by a scripted scene (the village events trigger by position and time). The identical tap on a fresh load
finished normally, so it is not the macro; not reproduced.

## 4. Advanced test state (vanilla US, level-50 party in the Pure Land)

Delivered as a libretro save state, loaded automatically at launch when `state.sav` exists next to `rom.sfc` in the app's
external files directory, and by the **LOAD TEST STATE** button (top right) at any time. Built by
`tools/companion/android/advstate/build_advanced_state.py` from the PC fixture `work/party3_vanilla.state`
(`tools/companion/build_fixture.py`); the state format of the PC snes9x and of the app's core is identical (the app logs
`state loaded (823407 bytes, core wants 823407)`).

| Content | Real or injected |
| --- | --- |
| Route to Potos, the girl and sprite joining | real: the TAS-derived input route and the game's own join opcode |
| Level 50 (internal 49) for the three heroes | injected level byte; **max HP, max MP, the five stats and experience are then rebuilt by the game's own map-load code** (girl 417 HP / 47 MP, sprite 412 / 47, stats 44/48/44/39/56 and 28/44/31/56/28). The game rebuilds only slots 1 and 2, so for the boy the same rebuild was run in slot 1 with his object id and the fields (`+0x17D..`, `+0x184..`, `+0x187..+0x18C`, `+0x197`, `+0x1A4..`) copied to slot 0: 524 HP, stats 55/57/57/28/40 |
| Weapon levels, spirit levels | injected: eight nibbles `8` each |
| Magic | injected: spirit masks `0x3F` for every spirit (all three spells), girl and sprite |
| Weapons | injected: all eight types owned at tier 8 (Mana Sword shows 59 -> 188 in the ring), boy keeps the sword equipped |
| Items | injected: all eleven consumables, quantity 7 |
| Gold | injected: 65,535 (the game's cap) |
| Current HP | max minus 1 through the game's pending-damage field `+0x1F1` so its HUD counters refresh (a plain write leaves the HUD showing the old numbers) |
| Location | the game's own warp event (opcode 0x18) to map 215 |

Verified on the PC harness for 4,087 frames, then on the Thor for 25,000+: walking, equip spear, cast Gnome as the girl and
a sprite spell, use an item, ring menu, no crash. Status panel: levels 50, all 8 weapon cells enabled, all spirits except Lumina
for the girl enabled (that one is never listed for the girl, matching the game), 11 items.

**Pure Land identification is a judgement, not a ROM fact.** No map name table exists in the repository. I warped to all 397
reachable maps and looked at the thumbnails (`adv/sheet*.png`). Maps 198 to 235 are the floating green islands and the
elemental sub-lands (snow, lava, forest) that the Pure Land consists of; I chose 215, the first green island, because it is a
free field with room to walk. Other islands (222 crystal cave, 228 snow tower, 232 forest castle with a boss) can be built with
`--map N`. No story flags beyond the ring flag `$CF00 = 1` were needed: the map loads and runs normally. A boss sits on 232,
so 215 is the safe start.

Backup: `/sdcard/Android/data/org.somdecomp.companion/files/` held only `rom.sfc` and the earlier `state.sav` (party fixture,
Potos); the latter was pulled to `device/backup/state_party3_before.sav` before being replaced. No other saves exist.

## 5. 3DS build in Azahar (emulated, not real hardware)

Azahar 2126.2 (`org.azahar_emu.azahar`, Vulkan, JIT, "New 3DS" mode on) was the only 3DS emulator installed. It runs `.3dsx`
directly. Procedure: `som_companion.3dsx` pushed to `/sdcard/som-companion-test/`, started with
`am start -a android.intent.action.VIEW -d file:///sdcard/som-companion-test/som_companion.3dsx -n
org.azahar_emu.azahar/org.citra.citra_emu.activities.EmulationActivity`; the ROM was copied to Azahar's `sdmc/som/rom.sfc`
(`/sdcard/Azahar/sdmc/som/`) for the run and **removed afterwards** (the file and the directory; nothing else in that tree
was touched).

| Check | Result |
| --- | --- |
| Boots | yes, "CORE RUNNING" on the bottom screen status line, snes9x2005 plays the title and credit screens on the top screen (`n3ds_top2.png`, `n3ds_top5.png`) |
| Bottom screen | the shared UI renders (`n3ds_bottom4.png`); a touch held about 400 ms on the ITEM tab switches the tab (`n3ds_touch.png`); a plain 20 ms injected tap is shorter than one emulated frame and is missed |
| Speed | 50 ms per frame as measured by the app itself with `osGetTime` (title screen: core 25 ms, status UI 4 ms, top-screen scaling 3 ms after the fix below). **This is the emulator's timing model on a fast phone SoC at 268 MHz ARM11 equivalent (`ConfigureNew3DSCPU` is unimplemented in Azahar, so the 804 MHz mode is not engaged). It says nothing reliable about a real old or new 3DS.** |
| Audio | Azahar has no DSP firmware dump configured here, so `ndspInit` fails and the app runs silently (see bug 1) |

Bugs found and fixed in `tools/companion/n3ds`:

1. **Crash**: `audio_batch` wrote through a NULL buffer when `ndspInit` failed (no `dspfirm.cdc`), flooding unmapped writes at
   PC `audio_batch` (`addr2line` on the ELF). Audio is now skipped unless ndsp and all four linear buffers initialised, and
   `ndspExit` is only called when it was initialised. This also protects a real 3DS without a DSP dump.
2. The top-screen scaler did a division per pixel and cost 17 ms per frame; replaced by per-size index tables and contiguous
   column writes: 3 ms.
3. 8:7 aspect, hi-res 512-wide frames box-filtered to 256, no HUD crop (constant 256x224 geometry), same as Android.
4. A frame-time breakdown (`MS/F Rnn Unn Tnn`: core, status UI, top scaling) now shows on the bottom screen.

Not changed (outside my files): the shared renderer `tools/companion/som_status_draw.c` shows garbage item names while the
game is not running (`n3ds_touch.png`, WRAM is uninitialised before the title) and the third item row overlaps the 3DS
status line.

## 6. Launch recipe and device state: READY FOR USER TEST

Installed on the Thor: `org.somdecomp.companion` (debug APK built with the arm64 core), the user's ROM as
`/sdcard/Android/data/org.somdecomp.companion/files/rom.sfc` and the advanced state as `state.sav` in the same directory.
Start the app (launcher "SoM Companion" or `adb -s 5d1ee37e shell am start -n org.somdecomp.companion/.MainActivity`):
the ROM and the level-50 Pure Land state load by themselves; the bottom screen needs the vendor mode "Dual screen" (the
device is currently set to it). LOAD TEST STATE reloads the save, the text at the top left is the live stats line (tap it to
pick another ROM).

## 7. Still needs real 3DS hardware (updated checklist)

1. Speed on an old 3DS and a New 3DS, title and busy field map, from the on-screen `MS/F Rnn Unn Tnn` line (the target is 16.7
   ms; if the old 3DS is under 50 fps try snes9x2002, then `snes9x_3ds`). The emulator figure above is not a substitute.
2. Real audio through ndsp with a dumped `dspfirm.cdc`; crackle versus `ACHUNK`.
3. `osSetSpeedupEnable(true)` actually engaging the 804 MHz mode on a New 3DS.
4. Physical buttons, L/R, Select+Start exit, touch sensitivity of the status UI on the real resistive/capacitive panel.
5. Macros on hardware (they run in the same shared module; they passed on the Thor and on the PC harness, not yet on ARM11).
6. Save states are not portable to snes9x2005 (different serialisation), so the advanced state is Android-only; a 3DS test
   profile needs either an SRAM save or the same fixture driven through the game.
7. HUD stays visible on 3DS (no layer option in snes9x2005); decide whether a second core option build is wanted.

Thor items still for a person: audible sound and sync, physical pad layout (2.6), the `120 Hz` bottom-panel tearing report in
the earlier research, fold/hotplug of the second display, 10-minute battery draw.

## Launch from the app drawer (fix)

Symptom: opening the app from a launcher icon left the top display on the
launcher (ES-DE) while the panel appeared on the bottom display. Reproduced with
`am start --display 4 -n org.somdecomp.companion/.MainActivity`: the activity and
its Presentation both landed on display 4.

Fix (`tools/companion/android`):
- `LaunchActivity` (invisible, `Theme.NoDisplay`, `taskAffinity=""`, `noHistory`,
  `excludeFromRecents`) now owns the MAIN/LAUNCHER filter. It starts `MainActivity`
  with `ActivityOptions.setLaunchDisplayId(DEFAULT_DISPLAY)`, so the game always opens
  on the top display whichever display the icon was tapped on.
- `MainActivity` has no intent filter; if it is ever started directly on a secondary
  display it re-launches itself on display 0 (`NEW_TASK | MULTIPLE_TASK`) and
  finishes without touching the native core.
- The status Presentation stays on display 4.

Verified on the Thor (`dumpsys activity activities`, `screencap -d 0/4`), each after
force-stop of our package and Home: monkey LAUNCHER; `am start --display 4` of
LaunchActivity; `am start --display 4` of MainActivity; `--display 0`. All end with
MainActivity on Display #0 and the launcher untouched on Display #4 with the panel
on top of it. Home then monkey again resumes the same single MainActivity, no
crash. Screenshots: `device/t_{monkey,launcher_d4,main_d4,launcher_d0,relaunch}_{top,bottom}.png`.
Not done: a physical tap on the drawer icon (only monkey/am equivalents).

## Widescreen core in the Android app (Thor, verified)

- Packaging: `-PsomWsCorePath=<snes9x_widescreen_libretro_android.so>` stages it as `libsnes9x_ws_core.so` (arm64 only); the
  plain core stays as `libsnes9x_core.so` and is selected with the bottom-screen button "Core: wide/plain (restart)" (also used
  when the ws core is absent, e.g. x86_64).
- Defaults answered through `GET_VARIABLE`: `snes9x_som_widescreen=27`, `snes9x_som_widescreen_fallback=normal`, `snes9x_som_hud=hide_in_field`
  (follows the HUD toggle); BG3 masking unchanged; `SET_GEOMETRY` answered true. `--es ws 46 --es fb black --ez hide_hud false`
  override the saved options for one run.
- Bottom screen: `SettingsBar` under the panel (Wide off/17/27/46, Fallback normal/black, HUD hide on/off, Core), saved in prefs `som`.
- `GameView`: one scale for every width (`min(W / (348 * 8/7), H / 224)`, 8:7), frame column x drawn at `cx + (x - w/2) * pxw`, 512-wide
  frames counted as 256. The 256 px centre is therefore at the same pixels (255..1665 of 1920) for 256, 290, 310 and 348; only the side bands change.
- Verified (`device/w_*`): field map 215 at 310 (portraits gone, HUD numbers gone, valid left side), maps 253 and 102 fall back to 256 with
  the centre unmoved, map 131 stays 310, ring open (HUD and portraits visible, 310), title and name entry (310, black sides), taps on the
  buttons cycle 348 / 256 / 290, plain core gives 256. The advanced state loads in the ws core (823407 bytes, same format).
- Timing (device log, every 120 frames): 120 frames per 1.997 s = 60.1 fps; core run avg 6-12 ms at 310, up to 10.5 avg and 14.6 max at 348;
  budget 16.6 ms, no underruns after warm-up.

## Direct control (no ring) in the Android app (Thor, verified)

- Build: `som_jni.c` links `tools/companion/direct/som_direct.c`; `somd_init` runs at ROM load (and again after a state load),
  `somd_pre_frame` / `somd_post_frame` wrap every `retro_run` with the core's WRAM pointer. The glue of
  `som_companion_direct.patch` is kept inside `som_jni.c` (`cmd_direct`), so the shared `som_companion.c` (also built for the 3DS) is untouched.
- Bottom screen: weapon tap equips at once; items and spells with a target open a card screen drawn into the same 320x240 buffer
  (mini-map of the screen with numbered enemy markers, one card per party member or enemy, WHOLE PARTY / ALL ENEMIES, CANCEL); a card
  or a marker tap runs the command immediately (`somd_pick`, radius 16 map px). No-target and boy-only classes run on the first tap.
  Errors appear as a toast strip (NO_MP is checked before the target screen, BAD_TARGET, UNSAFE_STATE, BUSY, ...); grid cells that cannot
  be used (locked state, empty, not enough MP, spirit without spells) are dimmed. NEEDS_RING (Magic Rope, Flammie Drum) falls back to the ring macro.
- Device log (`direct done ... latency`): equip 2-3 frames, 21-42 ms from tap to result; item 2 frames, 21 ms; spells 2-3 frames, 23-42 ms.
- Verified on map 344 (three bats) from the advanced state: Axe equip, Candy on the boy (HP 50 -> 150, x7 -> x6), girl spell on a tapped
  ally (HP 150 -> 524, MP 44 -> 42), sprite Gnome on ALL ENEMIES (cast pose, bats gone) and on enemy 3 (target 1/5), NO_MP toast, empty-target
  screen, Magic Rope falling back to the macro. During the direct commands the stats line never showed the ring mode; the ring appears
  only in the NEEDS_RING fallback. Screenshots `device/d_*`.
- Not verified: damage numbers of the single-target cast on the device (only the cast request and MP), Flammie Drum, items on a dead ally,
  Magno and Ancient Cave on the device, minimap marker taps (cards were used), audio of the casts.

## SoM-style bottom screen (UX layer) in the Android app (Thor, verified)

- Build: `som_jni.c` uses `tools/companion/ux/android_patch/som_ux_glue.h` (`ux_glue_init` after ROM load, `ux_glue_frame` after every `retro_run`,
  `Native.renderUx` / `touchUx`); CMake adds `ux/som_ui_draw.c`, `ux/som_ui_state.c` and `-lm`. `StatusView` draws a bitmap of its own pixel size (about
  1240x930 above the settings strip) at 20 fps. The old 320x240 renderer, its target screen and greying are gone (the UX layer owns them); the toast strip is
  kept (red band, scaled text) and shows the somd errors; NO_MP is checked before the target panel opens.
- The graphics pack is NOT in the repo: `-PsomUiPack=<state>/assets/som_ui_pack.bin` stages it into the local APK assets (`build/staged-assets`); without it the
  renderer uses its built-in art. Do not commit or share that APK.
- Actions: ACT_CMD -> `run_direct` (target kinds 1/2/3 map to ONE / ALL_ALLIES / ALL_ENEMIES; boy-only spells target the boy); ACT_PAD latches the pad for 5 frames;
  BACK in the ring sends A, then Y if the ring is still open (A alone did not close the weapon ring); name entry taps a letter and a loop steps the cursor
  every 28 frames, then presses B.
- Verified on the Thor (screenshots `device/u_*`): field (party cards, weapon grid, radar, enemy list), magic tab and spell grid, target panel for ally and enemy spells,
  direct execution from the panel (Axe equip 26 ms, ally spell 21 ms, enemy 48 ms, ALL ENEMIES 23 ms), ring mirror (arrows moved the entry, BACK closed it, grid taps
  ignored while it is open), boot pad, name-entry keyboard (typed "M" and "O" into the game's name box), event/dialogue (locked, NEXT) and the WAIT transition (locked,
  grid tap started no command).
- Mock or not reached on the device: FLAMMIE was reached by poking `$E4/$FA/$FC/$F6` (the map is decoded from the ROM, palette approximate), not by flying; SHOP could not be
  forced by pokes (`$7D`, `$1847` are rewritten by the game) so it is only verified in the PC mocks; boss bar, Magno and Ancient Cave not run on the device.
- Draw cost: no extra core time was visible (core run avg 6-10 ms, no sustained underruns). The font overlap in the pack art matches the PC mocks (design, not a bug); the hero-card
  PSN/name overlap and truncated weapon labels were not reproduced on the Thor layout (wider cards), so no change was needed.

## Button bindings

See `docs/companion-bindings-20261017.md` (hotkeys for spells, weapon cycle, items and spirit focus, per-ROM prefs, editor by long press, defaults L/R weapon and ZL/ZR spirit).

## Bindings v2, UX quality check, options, hotlinks and sounds (Thor, verified on the device)

Evidence lives in `<state>/companion-20261017/device/` (`final/` = fresh screencaps of this build, `ico/` = icon sheets).

### Squashed bottom-panel font: cause and fix
The old panel renderer scaled its glyph atlas by a non-integer factor and filtered it, which turned "WEAPON" into "HEAPON".  The renderer now draws at
the native panel resolution (the bitmap has the view's size, one pixel per device pixel), uses an integer scale and no filtering.
`final/field_bottom.png`, `field_item.png`, `field_magic.png`, `field_map.png`, `ring_bottom.png`, `opt_main.png`, `bind_screen.png` are fresh screencaps.

### Name truncation
With the real names of the vanilla ROM the weapon and item cells use two lines: AURA GLOVE, MANA SWORD, DAEDALUS LANCE, GIGAS FLAIL, GARUDA BUSTER,
NINJA'S TRUMP, FAERIE WALNUT, MEDICAL HERB, CUP OF WISHES, MAGIC ROPE, FLAMMIE DRUM, MOOGLE BELT, MIDGE MALLET are complete (`ico/icons_items_game_vs_panel.png`,
`ico/icons_weapons_game_vs_panel.png`).  `tools/companion/ux/test_text_fit.py` found no truncation on four ROMs.  Names that still use the tighter font:
VALKYRIAN and DOOM AXE (they fill their cell).  Magno accents other than the two digraphs print as `?`.

### Icon verification (game ring on the top screen versus the bottom panel)
* Weapons `ico/icons_weapons_game_vs_panel.png`: all eight match.  Found and fixed earlier: the weapon-to-cell mapping was shifted (`ICON_WEAPON`).
* Items `ico/icons_items_game_vs_panel.png`: all eleven match.
* Spells `ico/icons_spells_girl_game_vs_panel.png` and `ico/icons_spells_sprite_game_vs_panel.png` (42 spells, each name written under the game icon; the game ring
  lists the slots in the order 1, 3, 2, the sheets are reordered to slot order).  Mismatches found and fixed in `ux/build_assets.py` (`SPELL_CELL`):
  Earth Slide (was a sphere, now the dots), Exploder, the girl's Fire Bouquet / Blaze Wall (swapped), the girl's Luna and Lumina spells (the two blocks were
  swapped), the sprite's Sleep Flower and Burst, the girl's Revivifier, Mana Magic and Dispel Magic frame choice.  After the fix every one of the 42 matches by eye.
* Spirits: ICON_SPIRIT had Luna and Shade swapped and Lumina missing.  Now raw byte 6 = Shade (the sprite's), raw byte 7 = Lumina (the girl's), as in the game
  ring headers (`ico/spells_hero*_part*.png`, `ico/m_all.png`).  Observation: the game's own ring header calls the girl's seventh spirit "SHADE" while drawing the
  Lumina icon and listing Lumina's spells (Light Saber, Lucent Beam, Lucid Barrier); the companion labels it LUMINA.
* Not verifiable: none left; the weapon sheet shows the javelin only partly in the game crop, it was confirmed on the other ring frames.

### Options, hotlinks, avatars, sounds (integration of the UX layer)
* The gear (OPT) opens the UX layer's options page (WIDE, FALLBACK, HUD HIDE, CORE, UI SOUNDS, HAPTICS, LANGUAGE, BINDINGS, QUICK SLOTS); the Java side only persists
  (`Settings.java`, prefs) and applies (`Native.pollEvent`).  The old config strip and gear overlay were removed.  WIDE is a toggle (off, or on with the saved margin).
* Hotlink slots persist per ROM variant (prefs `quick_<variant>`; 3DS `sdmc:/som/quick_<variant>.txt`).  Verified: slot set from the ring (BIND MENU), reload,
  tap in the field opened the Weapon ring; menu hotlink for the Magic ring (girl focus) opened it by X + UP taps; submenu hotlink for the Levels screen opened
  the weapon-level screen (log: `macro ... result=done note=screen opened`).  These use the macro engine (`SOM_CMD_OPEN_MENU`, tested on the PC by a ring model for every
  start state, hero, category and config entry).
* Ring mode: tapping the Sprite avatar moved the ring owner to the sprite (`ringq owner=2`).  A hotlink tapped inside the ring closes the ring and then runs.
* UI sounds: the log shows `sfx 29/2a requested (mailbox 00 29 0f 00)` for taps; the toggle works.  Whether they sound right needs ears; no audio capture was made.
* Bindings v2 on the device by injected key events (`adb shell input -d 0 keyevent`): R1/L1 weapon cycle, C bound through the long-press bind screen (badge C,
  "BOUND C"), C then cast Cure Water (`direct start kind=3 hero=1 ... target=1/1`).  The bind screen shows the data-driven list of free inputs and game buttons.
  Physical Thor buttons, trigger axes and stick directions were not exercised by hand (injected events cannot produce axes).
* Bug note: `adb shell input keyevent` without `-d 0` goes to the display that was last touched; use `-d 0`.  The old "Wide: 46" setting was my own tap on the old strip.

### 3DS
Built (`tools/companion/n3ds`, devkitARM) with the shared engine, macro engine and UX layer, including hotlinks and avatars.  Verified by PC and ARM (qemu-arm) unit tests only;
it was not run in Azahar in this round.

### Device state
`dual_screen_display_mode` is left at 0 (test value); the original value was 1 (`device/settings_to_restore.txt`).

### UX fixes round (rebuild with the latest ux and direct layers)
* Rebuilt with the card-label fixes and the fixed `som_direct`, reinstalled on the Thor only.  Captures in `device/final2/` (field weapon, item, magic, map, ring, options).
  Against `ux/mocks/field_device_thor_nav.png` (1240x970) the field capture differs in 4% of the pixels: live HP values, the quick-slot sword and status icon.
* Black strip: 23 top-display captures (3 before, 12 during and 8 around a second equip, `device/final2/seq/`) have no fully black row of 3 rows or more in the game area.
* Translucent gear at the bottom right of the old `field_bottom.png`: it was ours (the old `OptionsPanel` gear button, 56 dp, over the lower-right cell, and it did take touches).
  It was removed with the options move into the UX layer; the new captures show the plain VALKYRIAN cell.  Not the Thor system nor the screencap tool.
* Touch mapping: the bottom window is 1240x970 (the system navigation bar "NavigationBar4" takes the last 110 px of display 4 and cannot be hidden by the app); taps are in view
  pixels of that bitmap.  Verified by taps on OPT, avatars, BIND MENU, the bottom hotlink row and tabs, all hitting their targets.

## Top screen scaling (round 5)
- Top display: 1920x1080 (native 1080x1920 panel, landscape through the activity); wide core option `snes9x_som_widescreen` 27 gives 310x224 frames (N=27), 46 gives 348x224.
- What changed: the widescreen contract (docs/companion-widescreen-core-20261017.md) sizes N=27 for 1920x1080 only with the HUD rows cropped (rows 0..198, 6.20x5.43 scale). When the crop was removed (the core hides the HUD itself) `GameView` kept a fit of the whole 224 rows: 1708x1080, 106 px bars on each side, no longer filling the screen. Nothing regressed in the core.
- Now: option TOP FIT in the options page (saved in prefs `fit`): FILL (default, scale = width / (256 + 2N) with 8:7 pixels, rows cut at the bottom: 199 of 224 rows at N=27, all 224 at N=46, no bars and no distortion, identical to the old crop-based fill), FIT (whole frame, bars), INTEGER (4x high, 5x wide, centred), STRETCH (whole frame over the whole screen). WIDE now cycles OFF / 17 / 27 / 46 (`GameView` uses the same margin to pick the scale). adb: `am start ... --es ws 27 --es fit FILL`, broadcast extra `fit`.

- Update (round 7): the default is now wide 46 + TOP FIT = FIT (whole frame, ~3 px side bars); FILL is labelled FILL CROP.

## Top screen font and hi-res frames (round 8)

Symptom: the game's own text (title, name entry, intro pages) looked smeared on the top screen with the wide core. Cause: with the widescreen option on, the core averaged every 512 px hi-res frame down to 256 px, which destroys the 1 px strokes of the font; the 4.82x vertical scale with nearest-neighbour then made the strokes uneven.
Fix: `snes9x_som_hires=native` (Android sets it; the core default stays `average`) passes the stock 512x224 frame through; `GameView` draws every frame with a sharp-bilinear shader when the scale is fractional and plain nearest in INTEGER mode. The 348 px field frames and the 512 px text frames share the same centre scale, so nothing jumps. Captures: `device/matrix/r8_font_before_*.png` and `r8_font_after_*.png`.
