# Companion widescreen (stage 5 of board item T4)

Date: 2026-10-17. Code: `tools/companion/widescreen/`. Images and raw measurements:
`/home/cidwel/.local/state/som-decomp/companion-20261017/widescreen/` (called `$WS` below). No ROM bytes are stored
and the ROM is never patched; everything runs on an unmodified vanilla US ROM (and was spot-checked on the Magno
translation). Emulator: Snes9x 1.63 (commit 1bcc369) libretro core, built from source with one patch.

Goal: the top screen shows only the game, wide, without HUD. Targets: AYN Thor top (1920x1080, 16:9; bottom
1240x1080; per retail spec pages, not measured here), 3DS top 400x240 (5:3) and 3DS 2D wide mode 800x240 (same
panel, pixels half as wide as tall).

## Summary

| Question | Answer |
| --- | --- |
| Level 1 (stretch/crop/letterbox) | Works today, no emulator changes. Best per target in the table below. |
| Level 2 (true extra world on the sides) | **Feasible for field maps** at up to 128 px per side, 27 px (Thor) or 17 px (3DS) are all that 8:7 pixels need once the HUD rows are cropped. The 256 px centre is pixel-identical to the stock core (11,910 frames, 397 maps, 0 differences). Tile content of the extension is rebuilt from the game's own map in WRAM and validated against the game's tilemap in view. |
| What does not work | Sprites: the game never emits OAM entries for actors outside the screen, so actors in the extension are invisible until they touch the 256 px view (about 10 % of actors would be in a 46 px zone). 3 of 397 maps render Mode 7 in the field and fall back to black sides. About 11 % of maps fail layer validation and show black sides (see below). |
| Recommendation | Ship Level 1 first (HUD rows cropped, fit with 8:7 pixels), then add Level 2 as an opt-in on the Thor and 3DS wide mode with a 27 px / 17 px extension and per-map automatic fallback to Level 1. Details at the end. |

## Level 1: aspect handling options (measured)

Source: 256x224 frame (8:7 pixels, 4:3 on a TV). HUD = BG3 frame/numbers/gauge plus the portrait sprite, rows
199..223 (re-checked in `$WS/l1/hud_layers.png`: with BG3 off only the portrait remains; with sprites off too the
rows show plain world, so BG1/BG2 continue under the HUD). `tools/companion/widescreen/level1.py` computes the
scale, bars and distortion for every option and renders each one at target size on real frames of 5 scenes
(`$WS/l1/<target>_<scene>.png`, option letters in the corner; `$WS/l1/metrics.json`).
"Aspect error" is relative to the correct 8:7 pixel shape; "pixel jitter" is the worst-case width difference of
adjacent source pixels with nearest-neighbour scaling (0 % = integer scale; use a sharp-bilinear filter for the others).

| Option | Thor 1920x1080 | 3DS 400x240 | 3DS wide 800x240 |
| --- | --- | --- | --- |
| A integer, HUD kept | 4x, 1024x896, bars 55.8 %, aspect -12.5 % | 1x, 256x224, bars 40.3 %, -12.5 % | 2x1, 512x224, bars 40.3 %, -12.5 % |
| B integer, HUD cropped (199 rows) | 5x, 1280x995, bars 38.6 %, -12.5 % | 1x, 256x199, bars 46.9 % | 2x1, 512x199, bars 46.9 % |
| C fit 8:7, HUD kept | 5.51x4.82, 1411x1080, bars 26.5 %, 0 %, jitter 21 % | 1.22x1.07, 313x240, bars 21.6 %, jitter 93 % | 2.45x1.07, 627x240, bars 21.6 % |
| D fit 8:7, HUD cropped | 6.20x5.43, 1588x1080, bars 17.3 %, 0 %, jitter 18 % | 1.38x1.21, 353x240, bars 11.8 %, jitter 83 % | 2.76x1.21, 706x240, bars 11.8 % |
| E stretch, HUD kept | 7.5x4.82, aspect error +36 % | 1.56x1.07, +27.6 % | 3.13x1.07, +27.6 % |
| F stretch, HUD cropped | 7.5x5.43, +20.9 % | 1.56x1.21, +13.4 % | 3.13x1.21, +13.4 % |
| G/H cover (crop rows to fill, correct aspect) | 7.5x6.56, 165 of 224 rows (73.5 % of the game area), jitter 15 % | 1.56x1.37, 176 rows (78.4 %) | 3.13x1.37, 176 rows (78.4 %) |
| I true wide, 8:7, HUD cropped | 6.20x5.43, **256+2x26.8 px**, 0 % bars, 0 % aspect error | 1.38x1.21, 256+2x17.1 px | 2.76x1.21, 256+2x17.1 px |

Cover crops: the hero (feet y 76..152 in 95 % of the 396 maps measured at spawn, 114 in most) stays fully inside the
window in 96.5 % of the maps, but 25 to 33 % of the world rows are lost, so enemies attacking from above or below are
cut. Not recommended.

Per target:

- **AYN Thor top (1920x1080)**: today D (fit with 8:7 pixels, HUD rows cropped, 1588x1080, 17 % bars) or B for pure
  integer 5x with 39 % bars if sharpness matters more than size. With Level 2 option I fills the whole screen with
  correct pixel shape and no bars. Stretching (E/F) is not recommended (+21 to +36 % distortion).
- **3DS top 400x240**: no integer scale beyond 1x, so D with a sharp-bilinear filter (1.38x1.21, 353x240) is the
  practical choice; Level 2 option I fills 400x240 with a 17 px extension per side.
- **3DS wide 800x240**: sx must be about 2.3 times sy for correct shape; D (2.76x1.21) or I. The panel is the same
  size, only the horizontal resolution doubles, so the gain is sharper horizontal edges, not extra view.

Caveat: dialogue boxes and the ring menu are BG3 plus sprites over the map, so cropping rows >= 199 does not touch
them when they sit above the HUD, but anything the game draws at the very bottom is lost in B/D/F/H/I.

## Level 2: true widescreen without patching the ROM

### Known mechanisms (not SoM specific)

bsnes-hd (README, fetched 2026-10-17) offers widescreen: default only for Mode 7, an "all" setting for other modes
that the author says gives "many ... mostly artifacts"; per-layer settings; sprites: default "safe" renders a
sprite only if at least partially in the classic area, "unsafe" renders sprites fully in the widescreen area and
"causes artifacts in many" games; "Objects/Sprites will not be visible correctly in the widescreen areas ... will
require ROM-hacks"; at most 96 px per side can still contain objects; window effects need ROM patches. Snes9x has no
widescreen mode. So a generic emulator cannot do this for Mode 1 games; a game-aware layer is required. SoM is
favourable because its whole map is in WRAM.

### What SoM does at the edges (measured)

All 397 reachable maps use BG mode 1, BG1/BG2 64x32 tilemaps (512 px wide, 256 high) and a 32x32 BG3 (HUD, text);
three of them draw Mode 7 in the playfield (253, 488, 489). Facts used:

- The whole map is in WRAM: tile ids `$7F:0000` (layer 1) and `$7F:4000` (layer 2), 128 cells per row; the four
  tilemap words of each 16x16 cell at `$7F:C000 + 8*id` and `$7F:C800 + 8*id`; camera `$A8/$AA` and `$AC/$AE`;
  map size `$C0/$C2` (layer 2: `$C4/$C6`). Layer 2 can scroll with a different camera (parallax, 51 of 397 maps).
- The game streams only about one 8 px column beyond the view into the 512 px tilemap. Measured on the Potos
  state: VRAM matches the WRAM map in columns 30..63 only, i.e. 8 px left of the view and 0 on the right, the rest
  of the 64 columns is stale. Wrapping the tilemap (what a naive extra render shows) gives garbage: compare
  `$WS/l2/sheet_nofix.png` (garbage on both sides) with `$WS/l2/sheet_fix.png`.
- The BG scroll registers are not the camera: the offset between them is chosen at map load (`H - $A8` is 64 in the
  Potos state, other values elsewhere). The prototype reads it per frame instead of assuming it.
- Sprites: the game emits OAM entries only for actors that touch the 256x224 view (NPC at Potos: emitted for
  actor x in -15..263 and y in 8..247, i.e. when its 16 px box intersects the screen; `$WS/sprite_cull.json`). The
  retention bound of the actor table is wider (-64..319, `docs/actor-charge-worker-offscreen.md`), so the actors
  exist but are not drawn.
- Hero feet land on screen y = 114 in most maps (76..152 in 95 %), x = 128; the camera is clamped to the map.

### Prototype: three-pass renderer inside the Snes9x core

`ws_snes9x.patch` (224 lines, applied on Snes9x commit 1bcc369 by `build_core.py PREFIX`, about 2 min to build) hooks
`S9xUpdateScreen`. For every batch of scanlines it renders with the stock renderer: (1) a left pass with BG scroll
minus 256 and sprites shifted by +256, (2) a right pass, (3) the normal pass last, so the libretro frame is exactly
stock. The three 256 px slices go to a 768 px wide buffer read through `retro_som_ws_get()` (`wslib.WCore.wide()`).
Windows, colour math, priorities and palettes inside each slice come from the original renderer. Before the side
passes the BG1/BG2 tilemaps are rewritten from the WRAM map for the whole 512 px window centred on the camera and
restored afterwards (the game's VRAM is never left modified). BG3 is off in the side passes. A layer is used in the
extension only if the rebuilt tilemap equals the game's own tilemap in view (at most 2 % of cells differ, per layer
and per frame); a failing layer is hidden in the side passes. Cells outside the map use an all-zero character, so
the backdrop colour shows. The extension is valid up to 128 px per side (the tilemap holds 512 px); beyond that it
wraps.

Cost: 3 renders per scanline batch; 1,249 versus 1,341 frames/s on this loaded PC with the whole host loop (7 %).
On a 3DS the extra passes would be limited to the extension columns; not measured.

### Results

| Check | Result |
| --- | --- |
| Centre identical | 397 maps, 30 frames each (walking right then left), SOM_WS=0 versus the 256 px centre of the wide buffer, full 256x224 including HUD: 11,910 frames, 0 differing (`identity_check.py`). After this check was first run it found three Mode 7 maps where the buffer was stale; fixed by always copying the centre. Magno translation base state: identical. |
| Tilemap rebuilt from WRAM equals the game's tilemap in view | Both layers valid in 334 of 397 maps, layer 1 only in 15, layer 2 only in 3, neither in 45 (8 of them have a different BG layout, 3 are Mode 7, and many of the rest are maps entered with the new-game flag state where the loader's map size does not match the camera, so those are artefacts of the test state). Magno base state: 3,960 cells compared, 0 differ (`tilemap_check.py`). |
| Extension equals the real game later (walk right, compare the right edge predicted earlier with the real frame, BG only, rows 0..198; 63 maps where the hero could walk and both layers share a camera and passed validation) | 0-8 px: mean 97.0 % of pixels equal, 87 % of maps at 95 % or more; 17-27 px: 95.5 %, 87 %; 27-48 px: 94.4 %; 64-96 px: 92.5 %; 96-128 px: 92.7 %; 128-160 px (beyond the tilemap): 42 %. The residual differences were inspected on map 19: only animated water (time between the two frames), geometry matches. Left side: only 2 maps had enough leftward walking, so the left side is validated by symmetry of the construction, not by this walk. |
| Failure examples | Parallax layer 2 (51 maps): the extension is correct for the instant but the walk comparison cannot be used. Layer failing validation (map 102, 162 images in `$WS/l2/sheet_bad3.png`): black or backdrop sides. Map edges: sides show the backdrop (`$WS/l2/sheet_fix.png`). |
| Sprites | Not extended: actors beyond the view are not in OAM. Sprites that touch the view are drawn whole across the border (the side pass draws their shifted copy). In the maps at spawn, 44 of 429 actors on screen or within 46 px of it are in the 46 px zone (10 %); 38 of 397 maps have at least one (27 px: 27 actors, 26 maps). They pop in when they reach the centre. |
| Ancient Cave | Not verified: the generated ROM was never played past the name entry, so there is no field state. The mechanism reads the engine's WRAM map and tilemap words, which a data-only randomizer does not change; the Magno translation, which also changes text and data, passes. Treat as expected, not proven. |
| HUD | In the wide buffer the HUD stays in the centre rows 199..223 (BG3 and the portrait sprite), so crop rows 199+ (option I). Hiding it without losing rows needs BG3 off in rows >= 199 plus dropping the portrait sprite; the world below is intact (`$WS/l1/hud_layers.png`), but dialogue text is also BG3, so this needs a state check; not implemented. |

### Limits and open points

- Sprites in the extension: the only emulator-side routes are (a) re-running the game's own actor emission with a
  widened camera window (CPU re-entry or a code hook at the cull test: effectively a runtime code patch, which the
  design rules out for hacks) or (b) synthesising sprites from the actor table (needs the animation-to-OAM piece
  tables). Neither was built. With the default design the pop-in at the 256 px border is the visible defect, mostly
  on the right (actors in the extension are all invisible there; on the left the 16 px box rule hides them too).
- Window effects and mosaic stay inside the centre slice; the extension does not apply them.
- Mode 7 field frames (3 maps) and anything not mode 1 fall back to black sides.
- Unmeasured: speed on the Thor and 3DS, the left-side walk comparison, battles and cutscene frames, per-scanline
  scroll effects beyond the HUD split, hero walking behind tall layer-1 tiles in the extension.

## Recommendation

1. **Now (Level 1)**: crop rows 199..223 and fit to the target keeping 8:7 pixels (option D). Thor: 1588x1080 with
   17 % bars; 3DS: 353x240 with a sharp-bilinear filter. Optional integer 5x on the Thor.
2. **Next (Level 2, opt-in)**: use the three-pass core with a 27 px (Thor) or 17 px (3DS) extension per side,
   rows 0..198, so the top screen is filled with no bars and no distortion. Keep the 8:7 shape. Automatic fallback
   to option D when the layer validation fails, the map is Mode 7, or the game is in an event/menu that is not a
   field frame. Accept the sprite pop-in at the centre border (about 10 % of actors) or limit the extension to 17 px
   where it is rarest.
3. Do not pursue extensions above 49 px per side (square pixels, no cropping needed) until the sprite problem has
   a real solution: tiles are validated up to 128 px, actors are not.
4. Port work: the patch lives in `gfx.cpp` of Snes9x; the 3DS and Android cores need the same three-pass logic or
   a software copy of it. The SoM-specific part is only `som_ws_tilemaps` (about 60 lines, WRAM addresses above).

## Files

| File | Role |
| --- | --- |
| `tools/companion/widescreen/ws_snes9x.patch`, `build_core.py` | Core patch and build route (clone, checkout, apply, make). Env: `SOM_WS=1`, `SOM_WS_FIX=0`, `SOM_WS_RAW=1`, `SOM_WS_BG3=1`, `SOM_WS_OBJ=0`, `SOM_WS_MASK=1`. |
| `wslib.py` | Python wrapper of the patched core (`WCore.wide()`, OAM, scroll, counters). |
| `survey.py`, `scenes.py` | Warp to every reachable map through the event VM (`18 nn 00`) and keep states. |
| `level1.py` | Level 1 metrics and images. |
| `tilemap_check.py`, `measure_margin.py`, `identity_check.py`, `sprite_cull.py`, `actor_ext.py`, `make_images.py` | The measurements above. |

Evidence in `$WS`: `survey.json`, `tilemap_check.json`, `margin_all.json`, `actor_ext.json`, `sprite_cull.json`,
`l1/*.png`, `l1/metrics.json`, `l2/sheet_fix.png`, `l2/sheet_nofix.png`, `l2/sheet_bad*.png`, `l2/magno_wide.png`.
Rebuild the core with `python3 tools/companion/widescreen/build_core.py PREFIX` (network needed); the scripts expect
it at `$WS/build/snes9x/libretro/snes9x_libretro.so`.
