# Companion on the Nintendo 3DS (stage 4 of board item T4)

> Update 2026-10-20: save states, battery RAM, on-device generation (libsomrando), gameplay toggles, filters and credits were added; see `docs/companion-3ds-20261020.md`. The `state.sav` fixture now lives in `sdmc:/som/dev/`.

Date: 2026-10-18. Code: `tools/companion/n3ds/` (`source/main.c`, `source/menu.c`, `source/fb.h`, `Makefile`, `emu/`). Shared engine: `som_companion`, `direct/`, `ux/`,
`launcher/` (all unchanged except one format fix, below). Earlier 3DS notes: `docs/companion-platforms-20261017.md` sec. 5, `docs/companion-device-20261017.md` sec. 5.

Status in one line: the `.3dsx` (snes9x2005 core, shoulder-slot model, direct control, start menu) builds without warnings with a user-space toolchain and was run
and driven in the Azahar emulator on this PC (boot, both screens, touch, slot assignment, equip, item, cast, saved randomized game); it has **not** run on a real
3DS, so old-3DS speed and memory are still unmeasured.

## 1. What was built

| Piece | State |
| --- | --- |
| `som_companion.3dsx` | 914,380 B, `.elf` text 851 KB, data 35 KB, bss 2.64 MB. Contains no ROM data (the ROM, the state fixture and the art pack are read from `sdmc:/som/` at run time). |
| Core | snes9x2005 (`a79dfe9`, `make platform=ctr`) linked statically; hi-res and interlaced frames are box-filtered to 256 x 224. |
| Top screen | the whole 256 x 224 frame at 8:7, nearest-neighbour scaled into 400 x 240 (the HUD stays: snes9x2005 has no layer option). |
| Bottom screen | shared UX (`ux/`): party cards, 8 quick slots, WEAPON / ITEM / MAGIC / MAP tabs, target panel, options. |
| Shoulder model | slots 0..3 are L, R, ZL, ZR (`ux_glue_shoulder_labels`); slots 4..7 are touch only. Old 3DS (no ZL/ZR): only L and R are physical, ZL and ZR slots are used by tapping them (`som_bind_platform_inputs` masks the missing keys). New 3DS: all four. The old per-button binding flows are gone; the slots persist in `sdmc:/som/quick_<variant>.txt`. |
| Direct control | `somd_pre_frame`/`somd_post_frame` around `retro_run`; equip, item and cast go through `som_hk_start_cmd`; Magic Rope / Flammie Drum fall back to the ring macro. |
| Start menu | new, `source/menu.c` + `launcher/` (the model and renderer shared with Android and PC): SoM Original, Create Randomized (presets, tweak rows, seed), Saved Randomized (newest four, delete moves files to the trash folder, never removes). Both screens (menu on the bottom, summary on the top), pad and touch. Generation on the console is not possible yet (libsomrando is pending), so a randomized game must be a pre-generated ROM in `sdmc:/som/roms/gen/<key>.sfc` (4 MiB, header title `Secret of MANA`; sidecar `<key>.json` or `<key>.sfc.json` optional, it only names the slot). A configuration without a ROM shows "No ROM for this setup (key ...)" with the key to generate on the PC. |
| Perf log | once a second, `sdmc:/som/perf.txt`: `<ms per frame> R<core> U<bottom UI> T<top scaling> D<UI redraws in the last 60 frames> model=old|new heap_used=<bytes> linear_free=<bytes>` (also `svcOutputDebugString`). |
| Dev fixture | `sdmc:/som/state.sav`, a libretro save state of the same core, is loaded into the **original** ROM only (see sec. 5; never into a randomized ROM). |

Fixes made while bringing it up:

1. **Stack overflow (real bug, would crash on hardware too).** libctru's default 32 KiB main stack overflowed inside `retro_unserialize` (PC in the core, unmapped write below SP).
   `main.c` now sets `u32 __stacksize__ = 512 * 1024`.
2. The bottom screen is single buffered and redrawn only when `som_ux_needs_redraw()` says so (it was redrawn every frame, 8 to 14 ms in the emulator). In practice the
   redraw hash includes the live actor table, so on a field map with moving sprites it still redraws most frames; on a title or menu it costs 0 (`D0`).
3. `ux/som_ui_draw.c`: two `%u` with `uint32_t` (`"%u G"`) warned on 32-bit ARM; now `%lu` with a cast. One-line change in a shared file, golden hashes unaffected.
4. `launcher/som_launcher.c` formats an 8 digit seed with `%ld` into a 9 byte buffer: GCC cannot prove the bound on a 32-bit `long` and warns. It is T9's file, so the
   Makefile adds `-Wno-format-truncation` for that one object instead of editing it.
5. Pure C helpers moved to `source/fb.h` (column-major BGR8 blit shared by the game and the menu).

`make` prints no warning with `-Wall -Wextra`. `tools/companion/run_bind_tests.sh` (PC with ASan/UBSan and qemu-arm): 702,794 + 48 checks passed on both.

## 2. Install (real console, not tested)

```
sdmc:/3ds/som_companion.3dsx          copy the built file (Homebrew Launcher: run it from the list)
sdmc:/som/rom.sfc                     your US v1.0 ROM (the tests used the headerless 2 MiB file)
sdmc:/som/roms/gen/<key>.sfc          optional pre-generated randomized ROMs (4 MiB) + <key>.json
sdmc:/som/som_ui_pack.bin             optional art pack (tools/companion/ux/build_assets.py; ROM-derived, never commit it)
```

Controls: pad or touch in the menu; in game Select+Start exits. Audio needs the DSP firmware dump (`dspfirm.cdc`, "Dump DSP firmware" in Rosalina or GodMode9);
without it the app runs silently (it checks `ndspInit`).

## 3. What was tested (Azahar 2126.2 on this PC, emulator only)

Setup: the official AppImage (sha256 verified), run headless under Xvfb (`xvfb-run`, OpenGL through llvmpipe), `sdmc` redirected with `XDG_DATA_HOME` to
`~/.local/state/som-decomp/azahar-20261018/xdg/data/azahar-emu/sdmc`, input through `xdotool` (keys, mouse down / up on the bottom screen). Scripts: `tools/companion/n3ds/emu/azahar_env.sh`.
Evidence (ROM-derived screenshots, so outside the repository): `~/.local/state/som-decomp/companion-3ds-20261018/evidence/` (`azahar-20261018/shots/` has every capture).

| Check | Result |
| --- | --- |
| Boot to the start menu | yes: top wallpaper and title, bottom three entries with status lines (`menu_final_full.png`). |
| Menu by pad (D-pad, A, B) | MAIN to PRESETS to TWEAK (17 rows) to GENERATE: unavailable message with the key; B chain back to MAIN (`n1234.png`, `g12.png`). |
| Menu by touch | tap on Saved Randomized opens SAVED: two slots from `roms/gen`, top screen summary of the selected game (`sv1.png`). |
| Saved randomized game | A on slot 1: the Ancient Cave ROM (4 MiB) boots, "ANCIENT CAVE" title screen on top (`ac3_full.png`). |
| SoM Original | boots the US ROM to the title; with `state.sav` (party fixture) lands on Potos Falls with three heroes (`field_final_full.png`). |
| Bottom screen UI | party cards, 8 slots with `ZL L . . . . R ZR` labels, tabs, weapon / item / magic cells. |
| Long press arms an entry, tap on a slot stores it | axe long-pressed (1.2 s) then slot L tapped: the axe icon is in slot L; policy chooser (SELECT / LESS HP) shown for items (`s3b_full.png`, `y123.png`). |
| Physical shoulder key fires the slot | key L (Azahar `q`): the axe becomes the equipped weapon (orange rim moves from Rusty Sword to Stout Axe, `s4b_full.png`). The slot file `quick_*.txt` holds `0 1 2 0`. |
| Item by touch | ITEM, Candy, target panel, Boy: Candy x3 becomes x2 (`i123.png`). |
| Cast by touch through direct control | MAGIC, Girl, Undine, Ice Saber, target panel, Boy: banner "ICE SABER Lv. 2" and the effect on the top screen (`c3_full.png`), girl MP 8 to 6 (`cast_mp_after.png`). |
| Old 3DS model (`is_new_3ds` off) | ZL slot assigned by tapping (Candy, SELECT policy) and used by tapping the on-screen ZL slot: Candy x3 becomes x2 (`x12.png`, `old3ds_zl_slot_candy_full.png`); the physical L key works on both models. |
| Exit | Select+Start leaves the app cleanly (no exception dialog). |
| New 3DS model, key `1` (ZL) / `2` (ZR) | not exercised; labels and slots are the same code path. |
| Not tested | audio (no DSP dump in the emulator), circle pad / C-stick, option toggles, Magic Rope / Drum macro, generating on the console, save games (SRAM), swipe scrolling in the menu. |

Frame time **in Azahar** (an emulator measure on this PC, ARM11 at the emulated 268 MHz, `ConfigureNew3DSCPU` is unimplemented so the 804 MHz mode is not modelled;
it says nothing reliable about a real 3DS): title screen and menu 50 ms per frame (core 25 ms, UI 8 ms, top scaling 3 ms); Potos Falls field map 67 to 69 ms
(core 43, bottom UI 11 to 14, top scaling 3); the same field with the UI cached (`D0`, nothing moving) 51 ms. Azahar showed "App: 12 to 20 FPS" in all of them, so
the game ran at about a third of speed in the emulator's timing model. Do not read this as "too slow": the timing model of the emulator is not calibrated against
hardware (the earlier Android run measured 50 ms in the same model), only the relative cost of the parts is informative: the core dominates (about 64 percent), the
bottom UI is next (up to 20 percent), the 8:7 top scaling is cheap.

## 4. Real-hardware risks (open)

* **Old 3DS speed is unmeasured.** snes9x2005 on a 268 MHz ARM11 plus a software status UI and per-frame top scaling. Read `sdmc:/som/perf.txt` on the console
  (model=old / new, title and field). Fallbacks if it is under 50 fps, in this order: draw the top screen through a citro3d quad instead of the CPU loop, lower the
  bottom UI redraw rate (it is already skipped when nothing changes), snes9x2002 as the core, then the `snes9x_3ds` fork.
* **Memory.** bss 2.64 MB, ROM buffer 2 or 4 MiB kept for the direct module plus the core's own copy, the optional pack 9.4 MB (read twice: once for the menu, freed, then for the UX
  layer, freed after init), 512 KiB stack. In Azahar (field map, original ROM, pack installed) `perf.txt` reports `heap_used=25780992` (25.8 MB) and `linear_free=31452032` (31.5 MB). Whether 25.8 MB of malloc heap fits the heap a real old 3DS gives homebrew by default is unknown and is the first thing to check on hardware (the emulator does not enforce the console's limits); if it does not fit, find what holds the memory (candidates: the core's ROM and VRAM copies, the UX module's decoded pack, the 4 MiB ROM buffer).
* **File times.** `stat` on `sdmc:` may report no modification time on real hardware (it does not in Azahar); the Saved list then sorts by key and shows no date.
* **Audio** untested (needs the DSP dump), likely crackle on the old model if the core is slow.
* The art pack load takes a visible moment on the SD card (9.4 MB, twice).
* Touch is stylus-first (4 mm cells); the resistive panel accepts a finger but the small slots are tight.
* The Azahar `is_new_3ds` switch changes only what `APT_CheckNew3DS` reports; the New 3DS clock and extra cores are not exercised.

## 5. Test fixture: save states must be built for ARM

A save state made by the x86-64 snes9x2005 (the one used by the PC tests, 562,192 B) has the same size as the 3DS core's but is **not** loadable: the 3DS core ran into an
invalid pointer (crash in `Op42`). `tools/companion/n3ds/emu/build_armhf_fixture.sh` builds snes9x2005 as a 32-bit ARM Linux library (same layout as the 3DS build; only the
buffer allocation of `_3DS` differs and is not needed), runs the companion host under `qemu-arm` through the TAS route and the party fixture (`build_fixture.py`), and
yields a 529,140 B `party3_vanilla.state` that loads on the 3DS core (`cp` it to `sdmc:/som/state.sav`). The ROM stays outside the repository.

## 6. Toolchain recipe without sudo

The earlier agents reported "no devkitPro": `pkg.devkitpro.org` answers 403 to curl (Cloudflare) and there is no `dkp-pacman` without root. User-space route that works:

1. `tools/companion/n3ds/emu/fetch_devkitpro.sh <empty dir>`: takes the official Docker Hub image `devkitpro/devkitarm` (no docker daemon): token from `auth.docker.io`,
   manifest list, linux/amd64 manifest, then each layer blob with `curl`, **checked against its sha256 digest**, extracted in its own empty directory (only `opt/devkitpro`,
   whiteout markers excluded) and merged in order. Result: devkitARM r68 (GCC 16.1.0), libctru 2.7, citro2d / citro3d, `3dsxtool`, `smdhtool`, `picasso`, `tex3ds`.
   About 514 MB downloaded, 625 MB of toolchain on disk (the script keeps the layer archives and their extracted copies in `<dir>/dl`, another 1.2 GB; move that folder to the trash when done). Provenance caveat: Docker Hub, not a signed pacman repository.
2. `export DEVKITPRO=<dir>/devkitpro`.
3. The core: `libretro/snes9x2005` (`a79dfe9`), `make platform=ctr` gives `snes9x2005_libretro_ctr.a` (10 s, 782 KB).
4. `cd tools/companion/n3ds && make CORE_A=<path>/snes9x2005_libretro_ctr.a` (omit `CORE_A` for the demo build with a synthetic state, no menu).

Check performed today: a second full download to a new directory (4 layers, all digests matched) and a build with it from an empty build directory gave a `.3dsx`
**byte-identical** to the one built with the earlier toolchain tree (`toolchains/devkitpro-root`, kept). The 582 MB archives of the earlier run in
`~/.local/state/som-decomp/toolchains/dl/` are reusable and were left in place (nothing deleted).

Emulator: `https://github.com/azahar-emu/azahar/releases/download/2126.2/azahar.AppImage` (sha256 `af037f8b...5960`), `--appimage-extract`, run under `xvfb-run`; see `emu/azahar_env.sh`.
Azahar writes about 100 MB of log per crash with the default filter (`log_filter=*:Error` in `qt-config.ini` avoids most of it) and rewrites `is_new_3ds` when it closes.

## 7. Files

New: `tools/companion/n3ds/source/menu.c`, `source/fb.h`, `emu/azahar_env.sh`, `emu/build_armhf_fixture.sh`, `emu/fetch_devkitpro.sh`, this document.
Changed: `tools/companion/n3ds/source/main.c` (menu, ROM path, stack, state fixture, perf log, redraw gating), `tools/companion/n3ds/Makefile` (menu and launcher sources, one warning
flag), `tools/companion/ux/som_ui_draw.c` (two format specifiers). Not touched: `tools/companion/android/`.
