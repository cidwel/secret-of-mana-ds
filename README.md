# Secret of Mana Companion

Front ends for playing Secret of Mana from your own ROM on PC, Nintendo 3DS and Android handhelds (AYN Thor), with a second screen for the ring menu, status and map, widescreen, save states, shaders, quality-of-life options, and the Ancient Cave and randomizer modes.

Split out of [som-decomp](https://github.com/cidwel/som-decomp). The layout under `tools/` and `data/` is kept as it was there, so the build and test scripts work without changes.

## No ROM in this repository

The apps read the ROM you provide when they start (3DS: `sdmc:/som/rom.sfc`; Android and PC: chosen in the app). Nothing in this repository contains ROM data, and two checks keep it that way:

- `tools/rom_guard.py` refuses ROM and save files, files of 1 MiB or more, the game's SNES header, and, when `SOM_ROM` points to your ROM, any run of 64 bytes copied from it, raw or written as hex in source.
- The pre-commit hook in `.githooks/` runs it on every commit, and the `rom-guard` workflow runs it on every push.

After cloning, enable the hook once:

```bash
git config core.hooksPath .githooks
```

To include the byte comparison, set `SOM_ROM=/path/to/your/rom.sfc` in your shell.

Screenshots of the game itself and the UI graphics pack are not stored here either (see `tools/companion/assets/README.md`).

## Layout

| Path | Contents |
|---|---|
| `tools/companion/pc` | PC version (SDL) |
| `tools/companion/n3ds` | Nintendo 3DS version (devkitPro) |
| `tools/companion/android` | Android version (AYN Thor) |
| `tools/companion/launcher`, `ux` | Launcher, menus and second-screen drawing shared by all versions |
| `tools/companion/widescreen` | Widescreen Snes9x libretro core: a patch over Snes9x 1.63, downloaded at build time |
| `tools/companion/randomizer/libsomrando` | C port of the Secret of Mana Randomizer (LGPL-2.1, see its `NOTICE`) |
| `tools/gameplay` | Quality-of-life and gameplay options |
| `data`, `docs` | Credits, RAM maps and design notes |

## Tests

```bash
bash tools/companion/launcher/run_launcher_tests.sh
```

```bash
bash tools/companion/pc/run_pc_tests.sh
```

```bash
bash tools/companion/n3ds/test/run_n3ds_tests.sh
```

```bash
bash tools/companion/android/tests/run_thor_tests.sh
```

## Licences

The code is GPL-3.0 (`LICENSE`). `libsomrando` is LGPL-2.1. The widescreen core is built from Snes9x, whose licence does not allow commercial use.
