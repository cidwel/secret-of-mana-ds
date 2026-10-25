# Thor feedback, round 3 (board item T4)

Scope: the 19 items from the Thor test plus item 20 (tap a character square to take control). Nothing here was run on the Thor.
Status vocabulary: **fixed** (code + tests), **verified-in-render** (PNG evidence from the desktop renderer or the headless core),
**needs-device** (cannot be judged without the Thor in hand).

Evidence root: `/home/cidwel/.local/state/som-decomp/companion-20261018/shots/ux/` (not committed; regenerate with
`SOM_UX_MOCKS=<dir> python3 tools/companion/ux/render_mocks.py`). `base/` = renders before the round, `r3/` = after,
`r3flow/` = live headless-core flow (`tools/companion/ux/test_round3_flow.py`).

## Mechanisms shared by several items

* **Story flag `$CF00` (WRAM `0x7E:CF00`)**. Non-zero from the moment the boy takes the first sword (frame ~9180 of the TAS route, the
  moment the ring becomes usable). The UX layer latches it (`som_ux_is_armed`) and clears the latch in BOOT/FRONTEND. While not armed the
  bottom screen is plain black; the only exception is the name-entry keyboard (item 3). The flag is read only; nothing writes it.
* **Per-game identity**. FNV-1a of the 36 name bytes at `$CC00` (`som_ux_game_key`). A new game is detected as "the name entry was seen
  during this frontend visit" (`som_ux_game_event` NEW), a loaded save as LOAD. Java `Bindings` keys its stores by variant + that hash.
  Limitation: two saves with identical hero names share one entry (no save-slot number was found in WRAM).
* **Discovered entries** (`seen_w/s/i`): weapon/spell/item ownership is OR-ed over time, persisted by `Bindings` per game identity,
  wiped on a NEW event. Limitation: items that are used up disappear from the item grid because the game compacts the record.

## The 19 items

| # | Item | Status | How | On-device check |
|---|------|--------|-----|-----------------|
| 1 | Start-menu wallpaper = original SoM image | fixed | `LauncherView.drawWallpaper()` reads `assets/som_bg.jpg` (centre-crop, filtered). The jpg is the user's own file, staged by gradle `-PsomWallpaper=`; it is **not** in the repo (`companion-20261018/assets/som_bg.jpg`). Preview `som_bg_preview.png` | look at the top screen of the start menu |
| 2 | Bottom screen empty when the game opens | verified-in-render | `quiet_level()` = black for BOOT and for FRONTEND without name entry; `boot_thor.png`, `startup_first_frame_thor.png`; live `r3flow/intro_blank.png` | none |
| 3 | Name keyboard: no cursor simulation | fixed + verified | `somd_name_letter` / `somd_name_delete` write the name buffer (grid `0x9246`, buffer `0x9000`, count `0xA1CC`) and honour the ROM length limit. JNI `NAME_LETTER`/`NAME_DELETE` call them, the closed-loop queue and the hidden 160-frame fast-forward are gone. `test_direct_round3.py` (21 checks) and `test_round3_flow.py new` (typed ADAM, DEL, name bytes verified in WRAM: `r3flow/name_entry_*.png`) | type a name on the Thor; START still confirms |
| 4 | Nothing on the bottom screen until the first sword | verified-in-render | `$CF00` latch (above). `test_round3_flow.py route`: all sampled frames blank from the title to frame 9180, panel (1,202,800 lit px) 900 frames after. `r3flow/route_before_sword.png`, `route_after_sword.png` | play the intro |
| 5 | Horizontal line glitches in Mode 7 scenes | core verified exact; app mitigation, needs-device | Reproduced the Mode 7 intro flyover (new game, frames 4200-5100) and the Mode 7 maps 253 and 488 (400 frames of walking each) on the wide core (46 px, hires native) against the stock core: the centre is identical in every frame (0 differing pixels), sides stay black, the geometry never changes inside the flyover, and the Mode 7 maps fall back to 256 px (stock rendering). So the core neither drops nor duplicates scanlines in Mode 7. The remaining suspect is the app scaler: `GameView` now snaps the destination rectangle to whole pixels and uses nearest-neighbour from 4x up (see 6). A real Flammie flight was not reachable without a save, only these maps | fly Flammie on the Thor |
| 6 | Blurry mountain tiles in the log/trunk scene | cause found (app scaler), fixed, needs-device | Scene = new game frames ~4980-7000 (map 128 side tiles in `shots/core/r3b/cmp_5300.png`, `w5800.png`). The wide core (average and native hires, 46 px) renders the 256 px centre byte-identical to the stock core in that scene, and the scene has no hi-res, mosaic or colour-math tiles, so the core is not blurring it. The blur is the app's sharp-bilinear shader at the fractional Thor scale (5.5 x 4.8): 23.8 % of the output pixels of the dithered mountain texture are blended seams (nearest 0 %, plain bilinear 93 %), measured by `widescreen/r3b_scale.py` (PNG crops `scale_*.png`). Fix: from 4x up on both axes `GameView` uses nearest-neighbour (rows of 5 or 4 px, uniform per axis) instead of the shader; the shader remains for smaller scales; the destination is a whole-pixel rect. INTEGER fit stays the exactly uniform choice. Side note: beside the log the sides are black where the authored map (384 px wide) ends, that is the map edge, not a defect | judge crispness vs 1 px jitter |
| 7 | No dialogues on the bottom screen | fixed | EVENT with no dialogue line -> black, with one -> faint centred "LOOK UP" (`event_thor.png`) | none |
| 8 | Hide never-owned weapons/spells/items | fixed | `cell_visible()`, packed grids, `qe_shown()` in the quick editor; persisted via `seen*` + `Bindings`, reset on NEW. `field_weapon_*`, `slots_armed_*`, `quick_editor_*`; unit tests in `test_som_ui_draw.c` | load a save, open the ring |
| 9 | Bindings leak between games | fixed | per-game key (above); legacy `quick_<variant>` deliberately not read | new game after a played one: slots empty |
| 10 | Hide wait/event/ready indicators | fixed | header chips only for editing/ring/shop/flight | none |
| 11 | Black stripes while the ring opens (wide) | fixed (core) + verified-in-render | side tilemap offset was derived from scroll registers vs camera that are not updated together; now tolerant (neighbour offsets, window anchored to displayed scroll, hold last picture 6 frames). `core_round3_test.py ring`: 4 black-side frames on map 84 -> 0. `shots/core/proof_ring_map84.png` | feel on the Thor |
| 12 | Weapon level as `0:00` + damage | fixed | badge `%d:%02d` (level:progress, nibble at `+0x1C0`, progress `+0x1D0+type`) and `ATK n` from the ROM weapon table (`weapon_attack()`); `field_weapon_*`, `ring_weapon_filled_*` | compare ATK with the in-game stat screen |
| 13 | No "BOY"; real names, avatars instead of names | fixed | default heroes RANDI/PURIM/POPOI, "<name> ONLY", cards/ring/revive show portraits only; `names_custom_*` | none |
| 14 | Do not skip frames when opening the ring; MENU > SLOT | fixed | `somd_hide_video` is a no-op unless `somd_set_fast_menu` (tests only); the ring opens in real time. `BIND MENU` ("SPIRITS > SLOT") exists only in the magic ring, quick-editor MENU tab = spirit ring + the 7 config screens | feel of the ring opening |
| 15 | Zoomed map follows the active character | fixed | zone map centres on the pad hero (`st.pad_hero`); radar highlight still uses `ux->hero` (minor) | switch character on a big map |
| 16 | Thicker HP/MP bars | fixed | `BARH_T` 8 (9 on wide/slim, 6 compact); `field_hp4_thor_nav.png` | none |
| 17 | No "NO MP" | fixed | `nomp()` is a no-op | none |
| 18 | Tile flicker after the first-boss earthquake | fixed (core), shake simulated | same root cause as 11; simulated by shaking `$B0..$B6` (+-4: 25 % bad frames -> 0, +-6: 62 % -> 0). The real Mantis Ant amplitude is unknown. `shots/core/proof_shake_amp6_worst.png` | fight the boss |
| 19 | Status backgrounds on the player squares | fixed | `status_bg()`: strong tint (alpha 150) for sleep/frozen/stone/balloon/moogle/barrel/pygmy/KO, faint for poison/silence/stat-down; unit tests per status; `field_danger_*` | trigger a status in game |
| 20 | Tap a square to switch control | fixed + verified | `SOM_UX_ACT_SWITCH_HERO` from `HK_PARTY` taps (alive, not already the pad hero, nav off) -> JNI -> `somd_switch_hero` (the game's own swap, `A_PAD1_EDGE` bit 1, no skipped frames). Refused during events/open ring: `test_direct_round3.py` (hero 1/2/0, no-op, event refusal, ring refusal) | tap the squares in a field |

## Core (items 5, 6, 11, 18) and hires=native on Ancient Cave floor 1

Details, root causes and exact commands are in the "Round 3 fixes" section of `docs/companion-widescreen-core-20261017.md`.
* Core test: `python3 tools/companion/widescreen/core_round3_test.py all --shots DIR` (about 20 s): ALL PASS. Centre identity vs the stock core: US 80 maps / 23,200 frames, Magno 30 maps / 8,700 frames, 0 differing.
* Ancient Cave: `python3 tools/companion/ancientcave/t5_hud.py ancientcave-short 8000 46` -> 8,003 frames, 0 ring differences, 0 HUD leak, widths `[[348, 224]]` (stays wide on floor 1; it was 256). Cause: the map ends 16 px inside the view and the game fills the outside columns with wall tiles (6 % vs the 2 % limit); outside-map cells are no longer compared, plus a grace period before a map counts as unsupported. Mode 7 maps (253, 488) still fall back.
* Reproducible core build: `build_core.py` (`--git SRC`, `--android`, `--no-make`) against the regenerated `ws_snes9x.patch`.
* Android core in the APK: sha256 `cb4b56a7...4f81`.

## Round 3b (items 5 and 6) tests and evidence

* `python3 tools/companion/widescreen/r3b_mode7.py OUT 4200 5100` (intro flyover, PASS) and `r3b_m7scene.py OUT 253,488 400` (Mode 7 maps, PASS) are the regression tests; `r3b_dump.py` (new-game frames on stock, wide average and wide native cores) and `r3b_scale.py` (scaler emulation) produce the comparison data. All write PNGs to `shots/core/r3b/` (`cmp_*.png` stock over wide, `mode7_worst_4978.png`, `m7_map*_last.png`, `scale_*.png`).
* APK: `/home/cidwel/.local/state/som-decomp/som-companion-thor-round3b-20261018.apk` (sha256 `f09ce5e4c456accd2e37bac7fc1a8f989a6bd15dc5e7ebb49fc41ebf994d6bba`).

## libsomrando in the APK

APK: `/home/cidwel/.local/state/som-decomp/som-companion-thor-round3-20261018.apk` (not installed). `/home/cidwel/.local/state/som-decomp/companion-20261018/build_apk.sh [ws-core.so]` builds the debug APK with
`-PsomRandoSrc=<upstream SoMRandomizer folder>`; `libsomrando.so` and `libsomjni.so` link for arm64-v8a (checked with `file`: ELF aarch64, NDK r28c).
`presetSupported` / `generatorWhy` already call the real `somrando_supports()`, so with the library linked no supported preset shows "PC only".
After generating, `StartMenuActivity` computes the SHA-256 of the produced ROM, logs `generated key=... sha256=...`, shows it in a toast and
stores it in the sidecar json (`"sha256"`), to compare with the PC output of the same seed.

## Tests added or changed

* `tools/companion/ux/test_som_ui_draw.c`: 1099 checks, 0 failures under ASan/UBSan (quiet screens, armed latch, name keys, game events, seen persistence, switch hero, zone follow, status tint, `0:00`, BIND MENU).
* `tools/companion/direct/test_direct_round3.py`: ALL PASS (21 checks); `test_direct_menu.py vanilla`: 39 cases, 0 failed.
* `tools/companion/ux/test_round3_flow.py` (`new`, `route`): live headless core, both ALL PASS.
* `test_device_path.py`: goldens refreshed (`glue_hashes.txt`) and the fixture now sets `$CF00`; the device-capture comparison is informational.
* `run_bind_tests.sh` and `launcher/run_launcher_tests.sh` pass (108 configurations, 17 of 17 presets).
* `test_text_fit.py`: 86 problems per ROM ("target card text: GRIFFIN HAND"), pre-existing (reproduced with the old `som_ui_draw.c`).

## Known limitations

Identical hero names share one binding entry; used-up items leave the item grid; item 15 radar highlight; everything marked needs-device.

## Round 3c (Thor findings after the round3b APK)

APK: `/home/cidwel/.local/state/som-decomp/som-companion-thor-round3c-20261018.apk` (sha256 `687f63e78ab44e4e2c8f093981b00b5f2238d48d7e03a2376aded2453b75d8ed`, core sha256 `00ba75d0...41f6`, built from the regenerated `ws_snes9x.patch` on a clean clone with `build_core.py --android`). Evidence PNGs: `shots/core/r3c/`, `shots/core/r3b/`, `shots/ux/r3c/`.

| Item | Status | How |
|------|--------|-----|
| A. Wall / dialogue box checkerboard | fixed, needs-device | Reproduced on the new-game log scene (frame 5500 of `r3b_dump.py`): the window background is drawn by the game as one navy pixel alternating with the scene (colour math by pixel), 256 px based, so it is in the stock render too (not hi-res, not the core). The Thor shows it unblended; a CRT blends it. `som_dedither.h` (used by `copyFrame` in `som_jni.c`, field frames below 512 px only): where three luma steps in a row alternate in sign (>= 14) the pixel gets the 1-2-1 horizontal mix; pixels next to near-white (font glyphs) are skipped. `widescreen/r3c_dedither.py`: dialogue-box energy 203 -> 62 (frame 5500) and 212 -> 6 (5800), rock wall 30 -> 23, glyph pixels changed 0 of 2362; PNG `dedither_*.png`. HUD / text frames (512 wide) are untouched, so the earlier sharp-font fix is kept |
| B. Keyboard before New Game | fixed | The title and game-select menus also put their cursor in OAM sprite 0. Name entry now also requires `$A20A` bit 4 and no transition (`$E4`, `$E184`); live run: keyboard hit boxes exist only in frames 740-1055 (the name entry), none on the title or the select menu (`test_som_ui_draw.c` + `somd` `name_entry_up`). Later name entries in game flow (the other heroes) are not covered by the FRONTEND gate |
| C. Wallpaper on quiet bottom screens | superseded by round 3d item 2 (the grid, not the key art) | `som_ux_quiet()` (new) tells the JNI layer; quiet screens are written transparent and `StatusView` draws the `som_bg.jpg` key art (centre-cropped) under them; "LOOK UP" stays on top |
| D. Sword effect repeated in the sides | fixed (core), verified-in-render | The effect is a window ellipse + colour math; windows are screen coordinates, so the side renders repeated the ellipse. In the side passes the windows are now rebuilt per side: a window touching that screen edge covers the whole side, otherwise the side is outside it. `widescreen/r3c_sword.py`: centre identical to stock in all frames, near-white side pixels 824k -> 393k over frames 9240-9330; PNGs `sword_old_vs_new.png`, `sword_stock_old_new.png` |
| E. Weapon level / damage | fixed | Correction of item 12: it is shown on the ring card of the bottom screen: `LEVEL 0:00` and `ATTACK n` (ROM weapon table + strength), plus the equipped hero by name instead of BOY. Nothing damage-related is drawn on the top screen (the selection lines are not used by the Java overlay). `ring_weapon_filled_thor.png` |
| F. Mode 7 widescreen | fixed (core), verified-in-render, needs-device | The Mode 7 renderer takes a column shift (`som_m7_dx`): the side passes evaluate the same per-line affine matrix at screen columns -256..-1 and 256..511 (HFlip handled; Mode 7 repeat / transparent / tile 0 come from the same code). The intro flight starts each frame in Mode 1 (sky) and switches to Mode 7 mid-frame: that sky batch has no field tilemap, so it uses the plain scroll shift and black pixels (the empty half of the sky tilemap) take the nearest centre column colour. No fallback is needed for Mode 7 any more; DoubleWidth/interlace Mode 7 still falls back. Tests: `r3b_mode7.py` (intro flight 4200-5100, centre identical in 900 frames, sides lit in 628), `r3b_m7scene.py` (maps 253 and 488: now 348 wide, centre identical); regressions: `core_round3_test.py all` ALL PASS, `core_identity.py` 397 maps / 115130 frames / 0 differing, `t5_hud.py ancientcave-short 8000 46` 0 leaks, widths `[[348, 224]]`. PNGs `m7wide_sheet.png`, `m7_map*_last.png`. Known cosmetic: the sky clouds smear horizontally in the sides |

Core files changed in the patch: `gfx.cpp` (Mode 7 passes, sky fix, per-side windows), `tileimpl.h` (column shift), `libretro/libretro.cpp` (sky fix at composition). Previous binaries: `*.before-r3c` next to the new ones in `$WS`, previous patch in `companion-20261018/ws_snes9x.patch.before-r3c`.

## Round 3d

APK: `/home/cidwel/.local/state/som-decomp/som-companion-thor-round3d-20261018.apk` (sha256 `dd4bb7e916d8dcd61e391dc13a029f224927ad1ea16177f21a04e96e2d5937ff`). Evidence: `shots/core/r3d/`, `shots/core/r3b/`, `shots/ux/r3d/`.

1. **Affirmative messages removed.** Start menu: "ROM found" and "Makes N of M presets here" are no longer shown (only "Choose your ROM after start" / the generator limitation, which need action); the generate screen title "READY" is "NEW GAME" and its "ready" tag is gone; the SHA-256 toast is gone (the digest is still logged and written to the sidecar json). Launcher goldens refreshed; `run_launcher_tests.sh` passes (108 configurations, 17 of 17 presets).
2. **Empty bottom screen** shows the panel's usual grid (`background()`), nothing else ("LOOK UP" stays faint on top during dialogue). `som_bg.jpg` is used only by the start menu again (the StatusView wallpaper and the transparency path are removed). `ux/r3d/event_thor.png`, `boot_thor.png`; tests: `test_ux` 1104 checks, 0 failures (the "nothing drawn" checks now ignore the two grid colours).
3. **Intro flight sky.** Data (debug `SOM_WS_DBG=78`): the sky batch is Mode 1 with BG1 = 21+37 cloud tiles and BG2 = a 64x32 map whose picture only covers the 256 px view; wrapping the tilemaps shows black or half-clouds in the sides, and the nearest-column fill of round 3c smeared clouds, so a true wrap extension does not exist for these layers. The sky rows of the sides now mirror the centre columns (continuous at the seam, stable frame to frame, no black, no smear). The split line is clean: `r3d_horizon.py OUT 4200 4970 50` zooms the horizon of both sides every 50 frames (`shots/core/r3d/horizon_*.png`), centre identical to stock, 0 black pixels in the sky rows. Other Mode 7 tests unchanged and PASS.
4. **Waterfall / rock wall blur.** Measured with `widescreen/r3d_scale.py` on frame 5800 at the Thor's 1920x1080 panel (FILL geometry 5.52 x 4.83), wall region, nearest = reference 1.0:

   | scaler | blended pixels | Laplacian variance vs nearest |
   |---|---|---|
   | nearest (fractional) | 0 % | 1.00 |
   | integer 5x4 nearest | 0 % | 1.00 |
   | sharp-bilinear shader (round 3b) | 24.8 % | 0.56 |
   | area / supersample | 36.9 % | 0.75 |
   | bilinear | 93.4 % | 0.03 |

   hires native vs average makes no difference in this scene (identical numbers, no hi-res frames); the wide core centre is byte-identical to stock. Findings: (a) the round 3b shader softened the wall (that blur is gone since round 3c, which uses nearest from 4x up); (b) the round 3c de-dither threshold (14 luma) also smoothed 13.6 % of the wall's own texture pixels: it is now 48, which changes 0.1 % of the wall and still removes the window checker (62 % of the box pixels changed, glyphs 0 of 2362; `r3c_dedither.py` also asserts the wall energy stays within 3 %). Scaling choices already exposed in Options: FIT (default, nearest from 4x up), FILL CROP, INTEGER (exact 5x4 blocks with bars, sharpest and jitter-free) and STRETCH; no new setting was added. If the wall still looks soft on the Thor with INTEGER, the softness is in the game's texture, as the reference render shows.
