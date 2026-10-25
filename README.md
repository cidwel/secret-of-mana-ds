# Secret of Mana DS

Secret of Mana with a second screen, for the **AYN Thor** (Android), the **Nintendo 3DS** and the **PC**: the ring menu, status and map on the second screen, widescreen, save states, shaders, quality-of-life options, and the Ancient Cave and randomizer modes.

**This repository contains no game data.** You need your own Secret of Mana ROM (SNES, USA, v1.0). The apps read it on your device; it is never uploaded or built into anything.

## Quick start: the som-ds program

1. Download `som-ds` for your system from the [latest release](https://github.com/cidwel/secret-of-mana-ds/releases/latest): `som-ds-windows.exe`, `som-ds-macos` or `som-ds-linux`.
2. Open it, choose your ROM and your device, and press **Get it ready**:
   - **AYN Thor**: with the Thor connected over USB and USB debugging on, it installs the app and copies the ROM. Without a cable it writes the APK and the ROM to a folder, to copy by hand.
   - **Nintendo 3DS**: choose the root of the SD card. It copies the app to `3ds/secret-of-mana-ds/` and the ROM to `som/rom.sfc`. Start it from the Homebrew Launcher.
   - **PC** (Windows and Linux): choose a folder. It installs the PC version there with a `start` script.

It downloads the apps from the latest release, already built. Tick **Compile from source** to build them on your computer instead (see below).

On macOS the downloaded program may need a right click and **Open** the first time, because it is not signed.

The same program works from a terminal, with Python 3.8 or newer:

```bash
python3 som-ds.py "Secret of Mana (USA).sfc" --target 3ds --out /path/to/sd-card
```

```bash
python3 som-ds.py "Secret of Mana (USA).sfc" --target thor
```

```bash
python3 som-ds.py "Secret of Mana (USA).sfc" --target pc --out ~/Games/secret-of-mana-ds
```

## Prebuilt apps

Every release has the apps ready to use, with no ROM inside:

| File | Device |
|---|---|
| `secret-of-mana-ds-thor.apk` | AYN Thor, or any Android 10+ arm64 device. The app asks for the ROM on first start. |
| `secret-of-mana-ds.3dsx` | Nintendo 3DS with the Homebrew Launcher. Put your ROM at `sdmc:/som/rom.sfc`. |
| `som-pc-latest-windows-x86_64.zip`, `som-pc-latest-linux-x86_64.tar.gz` | PC. Run `som-pc --menu --rom your-rom.sfc`. |

GitHub builds them on every push and publishes them on every `v*` tag (`.github/workflows/build.yml`).

## Building from source

`build.py` builds everything with one command. It fetches the emulator cores (Snes9x) and the randomizer sources at fixed commits into `.deps/`, and writes the results to `dist/`. No ROM is needed.

```bash
python3 build.py thor
```

```bash
python3 build.py 3ds
```

```bash
python3 build.py pc
```

```bash
python3 build.py all
```

### The easy way on any system: Docker

Install [Docker Desktop](https://www.docker.com/products/docker-desktop/) (Windows, macOS) or Docker Engine (Linux), plus [Git](https://git-scm.com/downloads) and [Python 3](https://www.python.org/downloads/). Then build the Thor and 3DS apps:

```bash
git clone https://github.com/cidwel/secret-of-mana-ds.git
```

```bash
cd secret-of-mana-ds
```

```bash
python3 build.py thor --docker
```

```bash
python3 build.py 3ds --docker
```

On Windows type `python` instead of `python3`. The first run downloads the toolchains into a Docker image (a few GB, once). On Apple Silicon Macs the image runs under emulation and is slower.

### Without Docker

Every system needs Git and Python 3.

**Thor (Android)**

| | |
|---|---|
| Windows, macOS, Linux | Install [Android Studio](https://developer.android.com/studio) (it brings JDK 17 and the SDK). In SDK Manager, SDK Tools, tick **NDK 28.2.13676358** and **CMake 3.22.1** (enable "Show Package Details"). |
| Environment | `ANDROID_HOME` = the SDK folder (Windows: `%LOCALAPPDATA%\Android\Sdk`, macOS: `~/Library/Android/sdk`, Linux: `~/Android/Sdk`). `JAVA_HOME` = a JDK 17 (Android Studio's own is in its `jbr` folder). |
| Build | `python3 build.py thor` gives `dist/secret-of-mana-ds-thor.apk`. |

**Nintendo 3DS**

| | |
|---|---|
| Windows | Install devkitPro with the [graphical installer](https://github.com/devkitPro/installer/releases), ticking the **3DS Development** group. Build from the **devkitPro MSYS2** shell it installs. |
| macOS | Install the [devkitPro pacman package](https://github.com/devkitPro/pacman/releases), then `sudo dkp-pacman -S 3ds-dev`. Needs the Xcode command line tools (`xcode-select --install`). |
| Linux | Install [devkitPro pacman](https://devkitpro.org/wiki/devkitPro_pacman), then `sudo dkp-pacman -S 3ds-dev`. |
| Environment | `DEVKITPRO` = `/opt/devkitpro` (the installers set it; open a new terminal). |
| Build | `python3 build.py 3ds` gives `dist/secret-of-mana-ds.3dsx`. |

**PC**

| | |
|---|---|
| Linux | `sudo apt install gcc g++ make pkg-config libsdl2-dev` (or your distribution's equivalent), then `python3 build.py pc`. |
| Windows | Install [MSYS2](https://www.msys2.org/), open the **MSYS2 MINGW64** shell and run `pacman -S git zip make mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 mingw-w64-x86_64-pkg-config mingw-w64-x86_64-python`, then `python build.py pc` in that shell. |
| macOS | Not supported yet: use the Thor or 3DS apps. |

Add `--no-rando` to any build to leave out the randomizer and Ancient Cave generator.

## No ROM in this repository

Two checks keep ROM data out:

- `tools/rom_guard.py` refuses ROM and save files, files of 1 MiB or more, the game's SNES header, and, when `SOM_ROM` points to your ROM, any run of 64 bytes copied from it, raw or written as hex in source.
- The pre-commit hook in `.githooks/` runs it on every commit, and the `rom-guard` workflow runs it on every push.

After cloning, enable the hook once:

```bash
git config core.hooksPath .githooks
```

## Layout

| Path | Contents |
|---|---|
| `som-ds.py` | The program that takes your ROM and gets a device ready |
| `build.py`, `docker/` | Builds every app from source |
| `tools/companion/android` | Thor (Android) app |
| `tools/companion/n3ds` | Nintendo 3DS app |
| `tools/companion/pc` | PC version (SDL2) |
| `tools/companion/launcher`, `ux` | Launcher, menus and second-screen drawing shared by all versions |
| `tools/companion/widescreen` | Widescreen patch for the Snes9x core |
| `tools/companion/randomizer/libsomrando` | C port of the Secret of Mana Randomizer |
| `tools/gameplay` | Quality-of-life and gameplay options |
| `data`, `docs` | Credits, RAM maps and design notes |

Part of the [som-decomp](https://github.com/cidwel/som-decomp) project, the Secret of Mana decompilation.

## Credits and licences

- The code of this repository is GPL-3.0 (`LICENSE`).
- `libsomrando` is a C port of the [Secret of Mana Randomizer](https://github.com/Moppu/SecretOfManaRandomizer) by Moppleton and contributors, LGPL-2.1 (see its `NOTICE`).
- The emulator cores are [Snes9x](https://github.com/snes9xgit/snes9x) through [libretro](https://github.com/libretro/snes9x). Snes9x may be used and shared for free, but never for commercial purposes. That is why these apps are free and may not be sold.
- The PC version uses [SDL2](https://libsdl.org) (zlib licence).
