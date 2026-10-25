# Companion on AYN Thor (Android) and Nintendo 3DS (stages 3 and 4 of board item T4)

Date: 2026-10-17. Code: `tools/companion/android/`, `tools/companion/n3ds/`. Shared module and PC prototype:
`docs/companion-prototype-20261017.md`. WRAM map: `docs/wram-companion-map.md`.

Status in one line: both skeletons build from source without any ROM and the shared C module passes its unit tests
cross-compiled for ARM; nothing has run on a real Thor or 3DS, so stages 3 and 4 stay open.

## 1. Feasibility and decisions

### AYN Thor

Facts measured on a real Thor that happened to be attached over adb (read-only queries only: `getprop`,
`dumpsys display`, `pm list`; nothing was installed on it):

| Item | Measured |
| --- | --- |
| Model / build | AYN Thor, `Thor_V1.0.0.377_20260206`, Android 13 (API 33) |
| SoC | QTI QCS8550 (platform `kalama`, Snapdragon 8 Gen 2 family), ABIs `arm64-v8a, armeabi-v7a, armeabi` |
| CPU | 8 cores (parts 0xd46 x3, 0xd47 x2, 0xd4d x2, 0xd4e x1) |
| Display 0 "Built-in Screen" | 1080 x 1920 at rotation 0, 60 or 120 Hz, touch INTERNAL, default display |
| Display 4 "Screen-2" | 1080 x 1240, 60 or 120 Hz, touch EXTERNAL, `FLAG_PRESENTATION`, type INTERNAL, layer stack 4 |
| Controller | the pad shows up as an Android input device named "Odin Controller" |
| Already installed | RetroArch 1.22.2 (`com.retroarch.aarch64`), `me.magnum.melondualds` (a dual-screen DS emulator) and `com.odin.dualscreen.assistant` (vendor service) |

So the second screen is an ordinary secondary display flagged for presentations, and a dual-screen app already exists on
the device (melonDS Dual DS), which proves the route works on this firmware. The marketing specs agree (two AMOLED
panels, 3.92 in bottom at 60 Hz, 6 in top at 120 Hz, 8 Gen 2, Android 13):
[Liliputing](https://liliputing.com/ayn-thor-is-dual-screen-android-handheld-game-console-with-oled-displays-and-qualcomm-snapdragon-inside/),
[Retro Game Corps guide](https://retrogamecorps.com/?cat=-1) (notes that Android treats the top screen as primary on
the Thor, and one user report of tearing on the 120 Hz bottom mode).

How an app reaches the second display: `DisplayManager.getDisplays(DISPLAY_CATEGORY_PRESENTATION)` returns Screen-2
and `android.app.Presentation` (a Dialog bound to a `Display`) shows a window there; input is delivered per display, so
touches on Screen-2 reach the presentation's views
([Presentation reference](https://developer.android.com/reference/android/app/Presentation)). Alternatives are an
activity launched with `ActivityOptions.setLaunchDisplayId()`, or a second task; the Presentation is the simplest
and is what the skeleton uses. A presentation is dismissed if the display disappears, so the app must handle
`onStop` and display listeners.

Options for the Thor frontend:

| Option | Verdict |
| --- | --- |
| A. Thin custom Android app: Java UI plus one JNI library that dlopens a libretro core, runs it, calls `som_companion` | Chosen. We own both screens, touch layout and the macro loop, need no RetroArch changes, the module is plain C. |
| B. RetroArch overlay or second-screen support | Rejected. No evidence that RetroArch drives a second physical display (searched, nothing found); an overlay lives on the same screen and has no per-frame WRAM hook. |
| C. Fork RetroArch | Rejected for now: large GPL codebase, long rebases, for something A does in about 700 lines. |

The core is an unmodified snes9x libretro core (`libretro/snes9x`, builds with the NDK; arm64 `.so` is 3.0 MB). It is
packed in the APK as `libsnes9x_core.so` and dlopened from `nativeLibraryDir` (Android 10+ forbids executing files
from the app data directory, and `extractNativeLibs=true` is set so a real file exists). The user's ROM is chosen with
the system file picker and kept in memory only. Snes9x's licence is non-commercial: do not redistribute the core
inside a store APK; it stays a local build.

### Nintendo 3DS

Hardware (3dbrew): old 3DS has a 2-core ARM11 MPCore at 268 MHz, the New 3DS 4 cores and 804 MHz for apps that set
the flag ([Hardware](https://3dbrew.org/wiki/Hardware)). Screens: top 400x240 (800x240 in wide mode), bottom 320x240
touch.

Existing work:

- `bubble2k16/snes9x_3ds`: standalone Snes9x port for 3DS homebrew. Claims "many games at full speed (60 fps)" on old
  3DS, SuperFX/SA-1 games slow there and much faster on New 3DS, bottom-screen tap opens its menu, built with
  devkitARM/libctru/citro3d ([README](https://github.com/bubble2k16/snes9x_3ds)). Secret of Mana uses no coprocessor.
  No benchmark table exists; the numbers are qualitative.
- RetroArch ships libretro 3DS builds. Its nightly buildbot has `snes9x2002`, `snes9x2005`, `snes9x2005_plus` and
  `snes9x2010` as 3dsx/cia (static, one core per binary; no dlopen on 3DS)
  ([buildbot](https://buildbot.libretro.com/nightly/nintendo/3ds/latest/3dsx/)). The libretro docs say 2002 (Snes9x 1.39,
  "heavily optimized for ARM") and 2005 (1.43) are for underpowered hardware and trade accuracy for speed; they give no fps
  figures ([2005](https://docs.libretro.com/library/snes9x_2005/), [2002](https://docs.libretro.com/library/snes9x_2002/)).
  A forum thread reports crackling and frame drops with 2002/2005 on a 2DS XL
  ([libretro forum](https://forums.libretro.com/t/2ds-xl-w-luma-cfw-snes9x-2002-2005-audio-crackling-frame-drops/12338)).
  Full-accuracy `libretro/snes9x` has a `platform=ctr` target but is not aimed at the old 3DS.

Options for the 3DS frontend:

| Option | Verdict |
| --- | --- |
| A. Custom 3dsx linking a libretro core statically (snes9x2005, `platform=ctr` gives a 782 KB `.a`) plus the shared module | Chosen. Same code path as Android and the PC harness; the module needs only a WRAM pointer. |
| B. Patch `snes9x_3ds` | Rejected: unmaintained fork with a different core API; WRAM access and the macro loop would be re-plumbed by hand. Fallback if A is too slow, because it is the fastest known SNES core on the old 3DS. |
| C. RetroArch 3DS with a plugin | Rejected: no plugin API, same fork problem as Android. |

Decision on the core: **snes9x2005**. The whole companion test suite was run on it (PC build, see section 3): all
macro tests pass. Speed on the old 3DS is unmeasured everywhere (no source gives numbers), so the first on-device
check is frame time; the skeleton prints milliseconds per frame on the bottom screen.

## 2. Toolchains (all user level, under `/home/cidwel/.local/state/som-decomp/toolchains/`)

| Piece | How | Result |
| --- | --- | --- |
| Android SDK / NDK / JDK / Gradle | Already on lumina: `~/Android/Sdk` (platform 34, build-tools 34.0.0, NDK 27.3 and 28.2 r28c, CMake 3.22.1), JDK 17, Gradle 8.10.2 from `~/.gradle/wrapper/dists` | Used as is; AGP 8.7.3 resolved from Google Maven |
| devkitARM r68 (GCC 16.1) + libctru 2.7 + citro2d/3d + 3dsxtool/smdhtool | `pkg.devkitpro.org` blocks curl (Cloudflare 403), so the layers of the official Docker image `devkitpro/devkitarm` were fetched from Docker Hub with curl (digests verified) and only `opt/devkitpro` was extracted to `devkitpro-root/`. No docker daemon, no sudo | `arm-none-eabi-gcc 16.1.0`, 625 MB |
| snes9x cores | `libretro/snes9x` (`fae2fea`, NDK, arm64 + x86_64 `.so`), `libretro/snes9x2005` (`a79dfe9`, devkitARM static `.a` and a PC `.so`) under `src/`, outputs in `cores/` | built, see below |
| Archives | downloaded packages in `dl/` (582 MB) can be deleted once the extracted trees are trusted | pending cleanup with the user |

Set up a shell for the 3DS with `export DEVKITPRO=/home/cidwel/.local/state/som-decomp/toolchains/devkitpro-root/opt/devkitpro`.
Provenance caveat: the devkitPro tree comes from the Docker Hub image (blob sha256 digests matched the manifest), not from
a signed pacman repository.

## 3. Measured results

Cross-compiling the shared module `som_companion.c` (`-std=c99 -O2 -Wall -Wextra`, no warnings):

| Target | Compiler | Object text size |
| --- | --- | --- |
| Android arm64 (API 30) | NDK r28c clang | 6,729 B |
| Android armv7a (API 30) | NDK r28c clang | 6,250 B |
| 3DS ARM11 (`-march=armv6k -mfloat-abi=hard`) | devkitARM GCC 16.1 | 4,724 B |

Unit tests run under qemu-user (`qemu-aarch64`, `qemu-arm`):

| Test | Build | Result |
| --- | --- | --- |
| `test_som_companion.c` (module) | NDK clang arm64, static Bionic | passed |
| `test_som_companion.c` | aarch64-linux-gnu-gcc | passed |
| `test_som_companion.c` | arm-linux-gnueabihf-gcc | passed |
| `android/native/test_status_draw.c` (renderer + touch layout, 16 assertions) | same three | passed; the 320x240 images from x86-64, arm64 and armhf are byte-identical |

The 3DS binary itself cannot run under qemu-user (it needs the 3DS kernel); no 3DS emulator (Citra/Azahar) is installed
on lumina, so the 3dsx was not executed.

Builds of the skeletons:

| Artifact | Command | Result |
| --- | --- | --- |
| Android APK, demo (no core) | `gradle :app:assembleDebug` | 1.0 MB, cold 75 s, incremental about 35 s |
| Android APK with cores | `-PsomCorePath=.../snes9x_libretro_android_arm64.so -PsomCorePathX86=...x86_64.so` | 2.1 MB, includes both ABIs |
| snes9x libretro core, Android arm64 | `ndk-build` | 46 s, 3.0 MB `.so` (x86_64: 3.2 MB) |
| snes9x2005 for 3DS | `make platform=ctr` | 10.6 s, 782 KB `.a` |
| `som_companion.3dsx`, demo | `make` in `n3ds/` | 123 KB |
| `som_companion.3dsx` with core | `make CORE_A=.../snes9x2005_libretro_ctr.a` | 684 KB, `.elf` text 623 KB, bss 1.45 MB |

snes9x2005 against the companion tests (PC x86-64 build of the core, fixtures rebuilt from scratch with
`SOM_CORE=... SOM_COMPANION_STATE=...-s2005`, 22 s): **11 of 13 pass**. Use item, fifth-record item, unusable item with
recovery, equip weapon, girl magic with owner switch, sprite magic, refusal during events, refusal without game, bad
arguments, abort and event during macro all pass. The two failures are not the module: test 01 expects map 131 but this
core reaches map 128 with the same input route (the TAS route drifts on 1.43), and test 11 needs the
`snes9x_layer_3` core option, which snes9x2005 does not have (the HUD crop itself is just rows 199 and up). The 3DS
frontend therefore keeps the crop.

Android emulator run (x86-64 build of the same app on the existing `DxThor` AVD, Android 13, started `-read-only`
`-no-window`; a second display was simulated with `settings put global overlay_display_devices 1240x1080/320`, which
Android exposes as `FLAG_PRESENTATION`, like the Thor's Screen-2; the attached real Thor was not touched):

| Check | Result |
| --- | --- |
| App starts, no crash (`logcat` FATAL) | yes, also after `force-stop` and restart |
| Presentation window | `dumpsys window` shows a `org.somdecomp.companion` window with `mDisplayId=2` (the overlay display) next to the activity on display 0 |
| Status UI on the second display | rendered by the shared C renderer through JNI (demo state: three heroes, bars, tabs, weapon grid) |
| Touch on the second display | `input -d 2 tap` on the MAGIC tab switched the tab, selected the girl and listed Gnome and Dryad as enabled |
| libretro core dlopen | the x86-64 snes9x core loaded from `nativeLibraryDir` ("Core loaded") |
| Real ROM | the user's US ROM pushed to the app's external files dir (`rom.sfc` shortcut): "ROM loaded", the title screen ran on the main display and the second display showed live state: `NO-GAME`, `LOCKED`, all buttons disabled |
| Not verified | audio (emulator had no audio), speed (emulator under load 170), gamepad input, entering the game and running a macro from touch, the real Screen-2 geometry |

## 4. What the skeletons do

Shared: `tools/companion/android/native/som_status_draw.{c,h}` renders the 320x240 bottom screen (party bars, three
tabs weapon/item/magic, a 4x3 grid of touch buttons) into a `0x00RRGGBB` buffer with a built-in 5x7 font and turns a tap
into a `som_cmd` (select hero by tapping its row; magic is two taps: spirit then spell). It lives under `android/` only
because this task could not add files at `tools/companion/`; move it next to `som_companion.c` when that is allowed, the
3DS Makefile references it by path.

Android (`tools/companion/android`, Gradle project, CMake for the JNI library, package `org.somdecomp.companion`):

- `MainActivity`: game `GameView` (256x199, HUD rows cropped, nearest-neighbour fit) on the default display; looks for
  a presentation display and shows `StatusPresentation` there; with a single display the status view is stacked below
  the game. Gamepad keys and hat map to the libretro mask (south = SNES B, east = A, west = Y, north = X).
- `som_jni.c`: libretro host (dlopen, environment, video to ARGB, AAudio output with a ring buffer), the frame loop on a
  thread paced at 60.0988 Hz, `som_macro_step` before `retro_run`, `som_read_state` after it, touch queue.
- Dev shortcut: a ROM pushed to `/sdcard/Android/data/org.somdecomp.companion/files/rom.sfc` is loaded at start, no picker.
- Without a core it renders a synthetic state, so the two-screen layout can be checked without any ROM.
- Build: `ANDROID_HOME=~/Android/Sdk JAVA_HOME=/usr/lib/jvm/java-17-openjdk-amd64 gradle -p tools/companion/android :app:assembleDebug [-PsomCorePath=...]`
  (no wrapper jar is committed; any Gradle 8.9+ works).

3DS (`tools/companion/n3ds`, plain Makefile on `3ds_rules`-free devkitARM calls):

- Top screen: game frame, HUD rows cropped, scaled 256x199 to 308x240 centred in 400x240. Bottom: shared UI plus a
  status line and milliseconds per frame. D-pad/buttons/L/R map straight; touch taps go through `som_ui_touch`.
- With `CORE_A` it reads the user's ROM from `sdmc:/som/rom.sfc`, runs snes9x2005 and plays audio through ndsp
  (4 x 1024-sample buffers, drops samples when the queue is full). Without it: demo state.
- `osSetSpeedupEnable(true)` is requested for the New 3DS 804 MHz mode.

Known gaps (honest list): macro results are only shown as text; the top screen on 3DS is software scaled (a citro3d
textured quad is the upgrade if the CPU budget is tight); no 800x240 stereo-off wide mode and no true widescreen (stage
5); no savestate/SRAM handling on either frontend (SRAM is not persisted yet); the Android loop is timed by the clock,
not by audio; Android keeps the screen on only because the activity is foreground; spell and weapon names for types
4 to 6 are generic; the Thor's Screen-2 orientation/rotation is untested.

## 5. Device test checklist

Thor (install `app-debug.apk` built with `-PsomCorePath`; `adb install -r`, then pick the ROM from the picker):

1. App starts; the top screen shows the game area and the bottom screen shows the status UI (not mirrored, not black).
   If Screen-2 stays empty check `adb shell dumpsys display` for the presentation display and the logcat tag
   `somcompanion`.
2. The status image is upright and fills the bottom panel; taps land on the right cells (tap each tab and a party row).
3. ROM loads (text changes to "ROM loaded"); video at the right speed, no tearing at 120 Hz on the top screen; sound present.
4. Gamepad: D-pad, A/B/X/Y, L/R, Start/Select. Check D-pad works both as keys and as hat.
5. Walk to a field map; status shows MAP, FIELD, READY and live HP/MP.
6. Tap a weapon, an item (Candy), a girl spell: each macro runs, the header shows RUNNING then DONE, the ring closes.
7. Tap during an event/dialogue: header shows LOCKED, the macro is refused, no stray presses.
8. Fold/unfold or display hotplug: no crash (presentation dismissed and recreated).
9. Leave and return to the app: state still consistent; note any audio underruns.
10. Record frame time (adb `dumpsys gfxinfo` or the on-screen counter once added) and battery draw for 10 minutes.

3DS (copy `som_companion.3dsx` to `sdmc:/3ds/`, the ROM to `sdmc:/som/rom.sfc`; run from the Homebrew Launcher):

1. Demo build: bottom screen shows the status UI, taps change tabs, top screen black.
2. Core build: "CORE RUNNING" on the bottom status line; game on the top screen at the right aspect; read ms/frame:
   target 16.7 ms. Record old 3DS and New 3DS separately, both on the title screen and in a busy field map.
3. Audio crackle check; if present increase `ACHUNK`/buffer count or lower the core's "reduce slowdown" settings.
4. Buttons, L/R, Start and Select map correctly; Select+Start exits.
5. The same macro checks as Thor items 5 to 7.
6. If old 3DS is below 50 fps: try snes9x2002 (`platform=ctr`) as the core, then the `snes9x_3ds` fallback.

## 6. Next steps

1. Run the two device checklists and write the numbers here (old 3DS speed is the open T4 item).
2. Persist SRAM on both frontends; add a settings page (HUD crop on or off, scale).
3. Replace the 3DS software scaling with a citro3d quad, and add the 800x240 mode.
4. Move `som_status_draw.*` next to `som_companion.c` and add it to the PC unit-test run.
