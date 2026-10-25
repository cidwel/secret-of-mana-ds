# Companion widescreen libretro core (stage 5 of board item T4, follow-up of `companion-widescreen-20261017.md`)

Date: 2026-10-17. Code: `tools/companion/widescreen/` (patch `ws_snes9x.patch`, `build_core.py`, tests `core_*.py`,
`geo_*.py`, `hud_*.py`, host wrapper `wscore.py`). Binaries and raw results are under
`/home/cidwel/.local/state/som-decomp/companion-20261017/widescreen/` (called `$WS`): `android-arm64/`,
`pc-x86_64/`, `core_tests/`. No ROM bytes are stored, the ROM is never patched, nothing was installed on a device
and no `adb` was run. Snes9x is non-commercially licensed: the cores are local tools, not distributable.

## What exists

The Level 2 three-pass renderer of the prototype is now a normal libretro core: Snes9x 1.63 (commit 1bcc369e) plus one
patch (about 650 diff lines in `gfx.cpp`, `libretro/libretro.cpp`, `libretro/libretro_core_options.h`).

| File | What |
| --- | --- |
| `$WS/android-arm64/snes9x_widescreen_libretro_android.so` (+ `README.md`) | Android arm64-v8a, NDK r28c (`~/Android/Sdk/ndk/28.2.13676358`), API 30, `ndk-build`, 2.57 MB stripped. **Built, not run** (no device access; the x86_64 build of the same sources is what was tested). |
| `$WS/pc-x86_64/snes9x_widescreen_libretro.so` | same sources, the Makefile's default g++ flags (LTO), used by every test |
| `$WS/pc-x86_64/snes9x_plain_libretro.so` | unmodified Snes9x 1.63 at the same commit (reference for the identity tests) |

Rebuild: `python3 tools/companion/widescreen/build_core.py PREFIX` (needs network; `--plain` skips the patch). The Android
build is the same tree with `ndk-build -j8 APP_ABI=arm64-v8a APP_PLATFORM=android-30` run in `libretro/` after setting
`CORE_DIR` in `libretro/jni/Android.mk` to the absolute source path (the stock makefile builds a doubled `jni/jni/..`
path with this NDK).

## Core options

| Key | Values (default first) | Effect |
| --- | --- | --- |
| `snes9x_som_widescreen` | `disabled`, `17`, `27`, `46` | pixels of extra world per side; frame width 256, 290, 310, 348 |
| `snes9x_som_widescreen_fallback` | `normal`, `black` | what to do on an unsupported map: `normal` = 256 px frame, `black` = keep the wide frame with black (or partly valid) sides |
| `snes9x_som_hires` | `average` (default), `native` | with widescreen on: `average` pair-averages hi-res frames to 256 px (old contract), `native` sends the stock 512x224 frame for title, name entry and text pages (sharp font; the frame width is 512 for those frames, the frontend must read the width per frame) |
| `snes9x_som_hud` | `off`, `hide_except_ring`, `hide_in_field` (legacy alias of `hide_except_ring`) | remove the HUD at render time in every state except while the ring is open (see HUD section) |

Debug environment only: `SOM_WS_MARGIN=N` (1..128, overrides the option, used by the old measuring scripts through
`wslib.py`), `SOM_WS_FIX/RAW/BG3/OBJ`. Widescreen is ignored while `snes9x_blargg` is on (the core then emits the NTSC
filter frame as before).

## Geometry contract (what the frontend can rely on)

1. Height is the core's usual 224 (239 with the overscan option). Width is 256 or `256 + 2 N`. The stock 256 px picture
   is always at `x = N .. N+255` and is pixel-identical to the unmodified core. Hi-res frames (title, name entry,
   text screens, 512 px in the stock core) are averaged to 256 px (the same pair average Snes9x's "merge" option uses), so the width
   does not jump to 512 either; this is the only case where the centre is not bit-identical to the stock frame.
2. The width changes only:
   - when the option changes (including to or from `disabled`);
   - at a map transition: the core watches the map id (WRAM `$DC`); after a change it waits for the screen to go dark
     (forced blank or brightness 0, which every transition does) and decides on the first visible frame after
     it, so the switch happens on a nearly black fade-in frame: widen if every enabled BG1/BG2 layer rebuilt from the WRAM map
     equals the game's own tilemap in view, fall back to 256 (option `normal`) if the map is Mode 7 or a layer fails the
     validation. Single-screen maps (width <= 256, e.g. map 8 used by the new-game and name-entry flow) and non-field
     layouts never trigger a fallback (nothing to show on their sides), the width simply stays;
   - after a state load, once, if a valid field frame is seen (positive only; it never falls back by itself).
3. Dialogs, ring menu, status screens, name entry, events, fades and battles do not change it (the decision is tied to the map id
   and to the dark frame, never to the game mode). While wide, BG3 and the sprites over the centre are untouched, the sides
   are drawn from the valid layers or black when no layer is valid.
4. Every change is announced with `RETRO_ENVIRONMENT_SET_GEOMETRY` from inside `retro_run`, before the `video_refresh` call
   that already carries the new width. `retro_get_system_av_info` always returns the current width; `max_width` is 604;
   `aspect_ratio` is the stock aspect times `width / 256` (pixels keep their shape).
5. Initial state with the option on: wide from the first frame (title and name entry have black or valid sides), so the
   normal new-game path has no jump before the first map.

## Measured results (all headless on the PC)

Machine load average was about 115 during the whole session; times are CPU times where it matters.

| Test | Result |
| --- | --- |
| Centre identity, US ROM, 27 px, **397 maps**, every frame of the warp (fade-out, load, fade-in, settle, walk): 290 frames per map, `core_identity.py` | **115,130 frames, 0 differing** versus the unmodified core (lockstep, raw RGB565). 285 maps end wide, 112 fall back to 256 |
| Same, 17 px, 50 maps | 14,500 frames, 0 differing (38 wide) |
| Same, **Magno translation**, 46 px, 45 maps | 13,050 frames, 0 differing (34 wide) |
| Option values (`core_options_test.py`, one process each) | av_info base width 256 / 290 / 310 / 348, height 224, max 604, aspect 1.3333 / 1.5104 / 1.6146 / 1.8125; first frame has the same size |
| Fallback on unsupported maps | maps 253, 488, 102, 305 with `normal` end at 256, with `black` stay at 310; map 8 and map 131 stay wide in both. Blargg filter on: option ignored (frame 602, av width 256) |
| New game, whole flow, 8,400 frames at 27 px (`geo_newgame.py`) | width 310 on **every** frame (title, new game, name entry, intro dialogues, first fields), 1 SET_GEOMETRY (frame 0), 9 map-id changes, name entry shows the forest/sea layer on the sides |
| Random mash, US, 9,000 frames, 27 px, with START/Y/A/B, warps every 900 frames and the option toggled `disabled` -> 46 -> 27 (`geo_mash.py`) | 4 width changes: the 3 option toggles and 1 fallback 86 frames after a map change (a warp). **0 violations** (a violation = a change that is not a toggle and not within 700 frames of a map change, or while ring/event states 0x81.. were active). 5 SET_GEOMETRY |
| Same, Ancient Cave fixture, 6,000 frames | 3 changes (the toggles), 0 violations; no map transition occurred in this fixture |
| Speed (`core_bench.py`, CPU time, field walk, 5 rounds, best of) | plain 2505 fps, widescreen core with option off 2623 (+5 %, noise), 17 px 1706 (68 %), 27 px 1732 (69 %), 46 px 1694 (68 %), 27 px + HUD mask 1664 (66 %), HUD mask alone 2580. So widescreen costs about one third of the frame rate of a plain core (three renders per frame instead of one); on this x86 it is 28x real time. An arm64 phone is not measured; the 3DS would not cope without limiting the side passes to the extension columns |

Sprites popping at the centre edge and Mode 7: unchanged from `companion-widescreen-20261017.md`. Actors that are outside the
256 px picture are not in OAM and are not drawn, so they appear when they touch the centre (about 10 % of actors would
be inside a 46 px zone at spawn); sprites that touch the picture are drawn whole across the border. The three
Mode 7 field maps were not reached by this test (their first frame at spawn is mode 1 in `survey.json` and they end up
in the validation fallback); the Mode 7 branch (`BGMode == 7` in a decision frame -> fallback, or black sides with the `black` option)
is implemented but only covered by the Mode 7 handling of the prototype, not by a dedicated run. Layer validation is
strict for the fallback decision (28 % of the 397 maps fall back in the test state, many because the loader state of
a warped map is artificial); the sides themselves are drawn leniently from whichever layer validates, so a momentary
validation miss does not flicker in a map that was accepted. `snes9x_som_widescreen_fallback=black` avoids all fallback
jumps at the price of black sides on those maps.

## HUD (option `snes9x_som_hud=hide_except_ring`, inside the core, no frontend toggling)

Policy: the HUD is invisible in every state, including free field play, fades, map transitions, events, dialogue and cutscenes,
**except while the ring is open** (`$7D == 1`, which per `wram-companion-map.md` also covers the status, equipment, spell and config
screens). The decision is read from WRAM by the core once per frame (latched on the first scanline batch), so there is no
latency and no flicker, and the previous mask that only applied to "free field play" (and therefore leaked during fades and
transitions) is replaced. `hide_in_field` is accepted as an alias.

What the HUD is (found by dumping BG3 tilemap and OAM over field play, 20 event maps, the new-game flow and a scan of all 397 maps
with events/dialogue on the US ROM; identical cells in US, Magno and Ancient Cave):

| Part | Where |
| --- | --- |
| BG3 panels | three panels of 10 x 3 cells at **BG3 rows 25..27** (pixels y 199..223), columns 1..10, 11..20, 21..30 (one panel per party slot; a single hero has only the middle one: columns 11..20): frame, name, HP/MP numbers, gauge. Static cells used to recognise a present panel: tile `$016` at (row 25, first column) and (row 26, first column), tile `$013` at (row 27, first column + 2) |
| Ancient Cave line | BG3 **row 23** (pixels y 184..191): "FLOOR nn" and the timer, tiles `$001..$009`. Cleared whenever a panel is present |
| Portrait sprites | OAM slots **0, 1, 2** (16x16, y 199, x 8/88/168, slot 0 at x 88 for a single hero; tiles `$E8/$EA/$EC`, attr `$33/$35/$37`: priority 3, palettes 1..3) |
| Dialogue boxes | BG3 rows 1..8 (top of the screen) in all 397 maps with events (event frames use rows 1..8 and 25..27 only), so they do not share cells with the HUD |

Implementation (`gfx.cpp`, `som_hud_bg3/som_hud_begin/som_hud_end`): per scanline batch, when the ring is not open, in BG mode 1 with a 32x32
BG3 map, each present panel's 30 cells (and row 23) are written as tile 0 (blank) in VRAM for the render and restored afterwards; the
portrait signature entries in slots 0..2 are moved to y 240 for the render and restored. Only panel cells are cleared, so a dialogue
window keeps its own cells even in the same rows; screens that are not Mode 1 with these panels (title, hi-res name entry, intro text,
Mode 7) are untouched; the OAM, VRAM contents and game state are never changed. The HUD is also removed in the side passes (BG3 is off there anyway)
and does not depend on the width option.

### Validation (`hud_ring_check.py`, `hud_newgame.py`; raw results in `$WS/core_tests/hr_*.json`, `hud_newgame.json`)

Three cores in lockstep: **A** the unmodified core, **O** an oracle (unmodified core with the BG3 layer off through its own option and the portrait
entries removed from the game's OAM staging buffer `$7E:0800` each frame, i.e. "no HUD at all"), **C** the widescreen core with the option on.
Per frame (ring state before and after the step must agree): ring frames need C == A; other frames are scored for **leak** = pixels in rows >= 184
where C still differs from O although A does (HUD pixels visible) and **removed** = pixels where C differs from A although A equals O (a non-HUD pixel was taken away).
Sanity: with the option `off` every frame leaks (630 of 630), so the detector works.

| Run | Frames | Ring frames (C == A) | Other frames | HUD leak | Non-HUD removed |
| --- | ---: | ---: | ---: | ---: | ---: |
| US, 54 warped maps (every 9th + dialogue, boss and dungeon maps), 420 frames each: fade-out, load, fade-in, walk | 22,680 | 0 | 22,680 | **0** | **0** |
| Magno, same 54 maps | 22,680 | 0 | 22,680 | **0** | **0** |
| US, 20 maps with entry events, A pressed to advance text (92 frames with dialogue text on BG3) | 8,400 | 0 | 8,400 | **0** | **0** |
| Magno, same (258 frames with dialogue text) | 8,400 | 0 | 8,400 | **0** | **0** |
| Ancient Cave, 5 warps (maps 0, 8, 57, 100, 131 of its table; its state keeps a text box on BG3, 2,005 frames) | 2,100 | 0 | 2,100 | **0** | **0** |
| US long walk with ring open/close, 27 px widescreen on (centre compared) | 8,006 | 4,655, 0 differ | 3,351 | **0** | **0** |
| Magno long walk with ring | 8,023 | 5,263, 0 differ | 2,760 (328 in events) | **0** | **0** |
| Ancient Cave long walk with ring (row 23 line included) | 8,011 | 3,639, 0 differ | 4,372 | **0** | **0** |
| New game 8,400 frames (title, new game, name entry, intro text, first fields; modes 0, 1, 3, 5, 7) | 8,400 | - | - | no frame differs at all (the HUD does not exist yet there) | 0 |

Frames where the ring flag changes during the step (boundary) are excluded: 1 to 13 per walk run. Dialogue text, title and name entry are never altered.
Open points: the ring-open flag flips on the frame the ring starts, so the HUD may appear one frame early or late at the boundary; dialogue boxes at the
bottom rows (not seen in any of the 397 maps' event frames) would be preserved cell by cell but were not exercised; shops, save and game-over screens
were not reached by these runs (they are not mode-1 BG3 panel screens in the cases seen, and the rule never touches cells that are not panel cells).

## Integration steps for the Android agent (not done here: `tools/companion/android` is not touched)

1. Packaging: load `snes9x_widescreen_libretro_android.so` instead of the stock core (same ABI/exports). Keep the stock `.so` as the
   fallback when the option is off if desired; with the option `disabled` the new core behaves like the stock one (hi-res
   frames are untouched then).
2. Core variables answered in the environment callback (`GET_VARIABLE`, and `GET_VARIABLE_UPDATE` = true after a change):
   `snes9x_som_widescreen` = `disabled|17|27|46` (Thor 1920x1080 with the HUD rows cropped needs 27; 3DS 17),
   `snes9x_som_widescreen_fallback` = `normal` (default) or `black` (never change width at maps), `snes9x_som_hud` = `hide_except_ring`.
   The Android code hides BG3 itself with a field-run counter (`mask_on`, `MASK_ENTER_FRAMES` in `som_jni.c`);
   **stop toggling BG3 (`snes9x_layer_3`) and the field-run counter from the frontend**: the core now hides the BG3 HUD panels, the Ancient Cave line and the portrait sprites
   itself, every frame, from WRAM, in all states except ring-open, so there is no async option latency and no flicker; set `snes9x_som_hud=hide_except_ring` once. Answer
   `SET_GEOMETRY` (cmd 37) with `true`.
3. Read the geometry **every frame** from the `video_refresh` arguments (`w`, `h`, `pitch`), as `som_jni.c` already does (`v_w`, `frame_w`),
   and treat `w` as 256 + 2N: `N = (w - 256) / 2`. The frame buffer is already 512 wide, which is enough for 348.
4. Draw contract (zoom): choose the scale once from the **centre 256 px** (rows 0..198 after the HUD crop, 8:7 pixels), then draw the full
   `w` at that same scale centred on the 256 px picture. When `w` falls back to 256, keep the scale and the centre: the extra pixels
   are simply not there (black bars), the picture must not be re-fitted. N = 27 on the 1920x1080 target fills the width, 17 px for 3DS 400x240.
   The width changes only at option changes and map transitions (fade frames), so a re-layout there is invisible; never re-layout on
   dialogs or menus, the core does not change the width there.
5. HUD: nothing to do in the frontend. With `snes9x_som_hud=hide_except_ring` the HUD rows (y >= 184) contain world graphics outside the ring; the old crop of rows >= 199 is no longer
   needed for the HUD (keep any crop only for aesthetics). In the ring menu (`$7D == 1`) the HUD is shown by the core, so the ring layout is unchanged. Contract in the core: `ws_snes9x.patch`, functions `som_hud_bg3`, `som_hud_begin`, `som_hud_end` (gfx.cpp).
6. Save states: a state load re-evaluates the map once; the frame width may change after `retro_unserialize`; no action needed beyond reading `w` per frame.
7. Test on the Thor: the arm64 build was compiled but never executed; check that `retro_get_system_av_info` returns 310 (27 px), that the
   first frame is 310 x 224 and watch `retro_run` time (expect about 1.5x the stock core).

## Limits

- Sprites are not extended (pop-in at the 256 px border), Window/mosaic/colour-math inside the centre only, extension valid to 128 px by
  tile data, offered values 17, 27 and 46 px.
- The Mode 7 fallback and the left-side walk comparison are not covered by a dedicated run; speed on arm64 and the 3DS is unmeasured.
- Hi-res frames are pair-averaged when the option is on (not bit-identical to the 512 px stock frame, identical to the stock "merge" blend).
- Savestates made with the widescreen core are the stock Snes9x format (nothing extra is serialised).
- The fixture for Ancient Cave is a party state at a field start; there is no map-transition coverage on it.

## Round 3 fixes (2026-10-18)

Reported on the AYN Thor build (46 px, hi-res option, HUD hidden): a horizontal line on the first map while scrolling, blurry background
tilesets in the first scene, black stripes while the ring menu opens, tile flicker after the Mantis Ant earthquake. Code: `ws_snes9x.patch`
(now about 810 diff lines), tests `tools/companion/widescreen/core_round3_test.py`. Binaries and results are under `$WS`
(`/home/cidwel/.local/state/som-decomp/companion-20261017/widescreen/`); frame dumps in
`/home/cidwel/.local/state/som-decomp/companion-20261018/shots/core/` (`proof_*.png` = before / after, `before/`, `after/` = raw test frames).

### Root causes

One mechanism explains (11), (18) and the intermittent part of (5). `som_ws_tilemaps` rebuilds the 64 x 32 BG1/BG2 tilemaps for the side passes and
checks the rebuild against the tilemap the game really has in view (limit 2 % of the cells). The cell offset between the game's tilemap and the map array
(`colK`/`rowK`) was computed per frame as `(scroll register - WRAM camera + 4) >> 3`, and the window of 32 rows was centred on the WRAM camera. But the
scroll registers (copied from the shadows `$B0/$B2/$B4/$B6` in NMI) and the camera (`$A8/$AA`, `$AC/$AE`, written by the main loop) are not updated
together. Whenever they disagree by more than 4 px the computed offset is off by one cell, the check fails, both layers are hidden and the side columns
are drawn black for that frame:

* (11) ring menu: on map 84 the camera glides 14 px per frame while the ring opens (frames 2 and 3 after the button, `$AA` 200 -> 186 -> 172 while the
  register is still 9 and then 507); on map 347 one frame. The sides were black for those frames (`proof_ring_map84.png`, right band black in frames 2..5).
* (18) earthquake: the shake adds a few px to the scroll shadows while the camera stays put. With +-4 px 25 % of the frames, with +-6 px 60 % of the
  frames lost both layers in the sides (`proof_shake_amp6_worst.png`): the tiles in the sides flicker on and off. A scan of every map that widens (397 warped maps, 400 frames of random walking each, previous-frame shift oracle) found 8 frames with a whole side wrong before the fix and 1 after (a held, 2 px stale frame; measured before the last hold refinement, the single-layer case). The fixture is the closest
  reproduction (no Mantis Ant state was reachable): the real shake changes the same registers (`$B0..$B6` drive the PPU, verified by writing them), amplitude
  is not known, the tests use 4, 6 and 14 px.
* (5) horizontal line while scrolling: the same desync, plus a second defect found on the way: the 32-row window was centred on the camera, so with the camera
  a tile row behind the register the last displayed tile row of the sides came from the other end of the 32-row tilemap (a band of wrong tiles at the bottom edge)
  and with a difference over 4 px the layer was hidden. Also a validation that sits exactly at the limit (map 266: 16 of 792 cells = 2.02 %) hid the sides for one
  frame (flash). On the first map after the intro (map 128) the desync does not happen when walking (6,000 random-walk frames and a 1,760 frame walk across
  the map: always 0 differing cells, one batch per frame), so on that map the remaining candidate is the frontend (see below).
* The Ancient Cave finding (`snes9x_som_hires=native`, `ancientcave-short` floor 1 fell back to 256 px): not the hi-res option itself. The generated floor 1 map ends
  16 px inside the view, the game fills the 2 columns beyond the map edge with wall tiles (48 of 792 compared cells, 6 %) where the core expected the blank
  character. In native mode the decision frame is the first visible one, so it fell back; in average mode the decision was taken on a later frame.
* (6) blur: **the core never blends**. The side passes are the stock renderer on a rebuilt tilemap, no filter. The only averaging in the core is the pair average
  of 512 px frames (`snes9x_som_hires=average`), which in the new-game flow touches exactly frames 421..1068 (title, game select, name entry; 648 frames of the first 1300)
  and never a field frame: from the first field on the stock frame is 256 px. In average mode the picture of those screens is softer (`after/hires_frame700_plain512_avg_native.png`:
  stock 512 / average / native); `native` (what Android sets) sends the 512 px frame unchanged. Sides on the first field map are pixel exact: what is on the right side
  reappears unchanged in the centre later in 94 to 97 % of the pixels (the rest is sprites and animated water), a blur would not.

### Fixes (all in `gfx.cpp` and `libretro/libretro.cpp` of the patch)

1. The cell offset is derived as before, but when the derived value does not match the game's tilemap the last offset that matched (the offset is constant for a
   map load) and then its neighbours (+-2 cells in both axes) are tried and the best one is used. The 32 x 64 window is anchored to the cells that are really
   displayed (scroll register), not to the camera, so a lagging camera cannot put a wrong row at the edge.
2. Cells outside the map are not compared (the game may show anything there) and, where they are in view, the game's own cell is kept; beyond the view they stay blank.
3. Lenient drawing: a layer that matched within the last 600 evaluations is still drawn while up to 10 % of the compared cells differ; the strict 2 % result still
   drives the widen / fall back decision (`som_ws_layer_ok` strict, `som_ws_layer_draw` lenient).
4. Hold: when no layer can be drawn in a batch, when one layer that matched within the last 60 evaluations is missing (drawing only the other would make it blink), or when a
   hi-res / interlace bit shows up in a field mode, the side columns keep what was drawn last for 6 consecutive frames, and only then go black (nothing valid) or show the valid
   layer (Mode 5/6/7 screens go black at once as before). A held frame is up to 2 px stale while the camera moves.
5. Decision grace: a map whose first visible frame does not validate gets up to 30 visible frames or half brightness before it counts as unsupported (a Mode 7 map falls back
   at once). Widening is still decided on the first valid frame.
6. Test hooks: `retro_som_ws_stat()` (cumulative counters), `wscore.WS.stat()`, `wscore.copy_core()`.

### Tests (`python3 tools/companion/widescreen/core_round3_test.py [shake|lead|ring|hold|hires|all] [--core PATH] [--shots DIR]`, 20 s)

| Test | What it checks | Before (Oct 17 binary) | After |
| --- | --- | --- | --- |
| `shake` +-4 / +-6 px | two lockstep cores, one with the scroll shadows shaken; its frame equals the unshaken frame shifted, sides included | 25 / 120 and 75 / 120 frames with a hidden side | 0 / 120 both |
| `lead` +-14 px | same with a camera a whole tile row away from the registers | 112 / 120 | 0 / 120 |
| `ring` maps 84, 347 | ring opening: no skipped side pass, no side that turns black | 4 frames black on map 84 | 0 and 0 |
| `hold` | 3 frames of wiped tilemaps keep the previous sides; 5 % wrong cells still drawn | both fail (side fully black) | both pass |
| `hires` | new-game frames 0..1299 centre = stock / pair average / native 512; first field map sides pixel exact | (not applicable) | 648 hi-res frames, 0 wrong; side exactness 0.94 0.95 0.97 |

Centre identity (`core_identity.py`, 46 px, lockstep against the unmodified core, final binary): US 80 maps, 23,200 frames, **0 differing**, 78 of 80 wide at the end; Magno translation 30 maps, 8,700 frames, 0 differing, 27 wide. HUD ring check (`hud_ring_check.py walk us 4000 46`): 4,003 frames, ring frames equal to stock (0 differ), 0 leak, 0 removed.
Fallback behaviour (`core_options_test.py`): the Mode 7 maps 253 and 488 still go to 256 px with `normal` and stay wide with `black`; maps 102 and 305, which used to fall back, now widen (the sides look right, `scratchpad` frames checked by eye); map 8 and map 131 unchanged. A map that does not validate within the grace window still falls back, up to about 30 frames after the first visible frame instead of on it.
HUD check on the Ancient Cave (`python3 tools/companion/ancientcave/t5_hud.py ancientcave-short 8000 46`): 8,003 frames, ring frames equal to stock (0 differ), 0 HUD leak, 0 wrongly
removed, widths `[[348, 224]]` (stays wide). From reset with native hi-res (`t5_core_soak` rig, 3,036 boot frames + 2,000 floor 1 frames): width 348 on floor 1 (decision: widened),
0 centre differences; before: 256.

### What only the device can show

* Whether the line of (5) on the first map is the core at all: on the PC the core output has no line in 8,000 frames of that map. The Android frontend draws the frame with a
  sharp-bilinear AGSL shader (`GameView.java`, API 33+) at a fractional scale (about 5.5 x 4.8 on 1080p): each texel edge is one blended screen pixel, so single-pixel dither
  patterns (the game's translucent water, waterfalls, dialogue boxes, forest tiles) look soft, and a line that moves with scrolling can come from the texel seams or from the
  translucent edge rows when `dst.top`/`dst.left` are fractional. `som_jni.c` copies the frame under the same mutex as `retro_run`, so there is no tearing. `Settings.fit == 2` (integer scale) uses
  plain nearest neighbour and is the check: if the line and the blur disappear there, they come from the scaler.
* The real Mantis Ant earthquake (amplitude and which registers it uses) and arm64 timing.

### Reproducible build

The committed patch plus `build_core.py` produce the shipped binaries (verified: built from a clean clone, same functional result; the bytes depend on the build directory because of LTO
path strings, so the sha256 below is of the builds in `$WS/core_build` and `$WS/core_android_build`):

```
cd $WS
python3 tools/companion/widescreen/build_core.py core_build --git $WS/core_plain/src          # PC x86_64, any Snes9x clone that has commit 1bcc369e works for --git (omit for GitHub)
python3 tools/companion/widescreen/build_core.py core_android_build --git $WS/core_plain/src --android   # arm64-v8a, NDK r28c, API 30
```

| File | sha256 |
| --- | --- |
| `$WS/pc-x86_64/snes9x_widescreen_libretro.so` (= `$WS/core_build/snes9x/libretro/snes9x_libretro.so`, `$WS/core/src` is a link to its tree, used by `wscore.CORE_WS`) | `5c1831f41449e2bcb0b95fcc45017d280fad3c47fbe5129c64a7b9e02873b623` |
| `$WS/android-arm64/snes9x_widescreen_libretro_android.so` (= `$WS/core_android_build/snes9x/libretro/libs/arm64-v8a/libretro.so`) | `cb4b56a7a0ea547813343859b85534fe443a619cc75761d88b3a391acf494f81` |

The older binaries stay beside them as `*.before-round3` (PC: the Oct 17 core without the `snes9x_som_hires` option; Android: the previous build).
