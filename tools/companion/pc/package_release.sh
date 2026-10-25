#!/bin/bash
# Builds the portable release folder of the PC frontend: the binary, the bundled data, README, NOTICE and the licences, plus the source files
# the LGPL asks for (libsomrando is linked statically).  No ROM bytes go into it, and neither does the Snes9x core (its licence forbids
# redistribution for commercial use: the folder tells the user how to build it, or --core copies a local build in for personal use).
#
#   package_release.sh [--platform linux|windows] [--out DIR] [--version V] [--core FILE] [--bundle-sdl] [--no-generator]
#
# linux    gcc and SDL2 development files (pkg-config).  Output DIR/som-pc-V-linux-x86_64/ and a .tar.gz next to it.
# windows  mingw-w64 (x86_64-w64-mingw32-gcc, or the MSYS2 gcc on Windows) and the SDL2 mingw development package:
#          SDL2_PREFIX=<dir with include/SDL2, lib and bin> when cross compiling from Linux; with MSYS2 pkg-config finds it.  Output .zip.
# libsomrando needs the upstream sources for its build-time resources: SOM_RANDO_SRC=<checkout>/SoMRandomizer (commit 1404b7b of
# Moppu/SecretOfManaRandomizer).  Without it the build stops, unless --no-generator links the stub (the start menu then offers no generation).
# At most 4 compiler processes at a time.
set -euo pipefail
HERE="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
C="$HERE/.."
REPO="$(cd "$C/../.." && pwd)"
PLAT=linux; OUT="${SOM_PC_RELEASE:-$HOME/.local/state/som-decomp/pc/release}"; VER=$(date +%Y%m%d); CORE=""; BUNDLE=0; GEN=1
while [ $# -gt 0 ]; do
  case "$1" in
    --platform) PLAT=$2; shift 2 ;;
    --out) OUT=$2; shift 2 ;;
    --version) VER=$2; shift 2 ;;
    --core) CORE=$2; shift 2 ;;
    --bundle-sdl) BUNDLE=1; shift ;;
    --no-generator) GEN=0; shift ;;
    *) echo "unknown option $1" >&2; exit 2 ;;
  esac
done
. "$HERE/sources.sh"
mkdir -p "$OUT"
case "$PLAT" in
  linux)
    CC=${CC:-gcc}; AR=${AR:-ar}; EXE=som-pc; TAG=linux-x86_64
    SDLC=$(pkg-config --cflags sdl2); SDLL=$(pkg-config --libs sdl2)
    LDX=(-lm -ldl "-Wl,-rpath,\$ORIGIN/lib") ;;
  windows)
    CC=${CC:-x86_64-w64-mingw32-gcc}; AR=${AR:-x86_64-w64-mingw32-ar}; EXE=som-pc.exe; TAG=windows-x86_64
    if [ -n "${SDL2_PREFIX:-}" ]; then SDLC="-I$SDL2_PREFIX/include/SDL2 -I$SDL2_PREFIX/include"; SDLL="-L$SDL2_PREFIX/lib -lmingw32 -lSDL2main -lSDL2"
    else SDLC=$(pkg-config --cflags sdl2); SDLL=$(pkg-config --libs sdl2); fi
    LDX=(-lm -static-libgcc) ;;
  *) echo "platform must be linux or windows" >&2; exit 2 ;;
esac
BUILD="$OUT/build-$PLAT"
NAME="som-pc-$VER-$TAG"
DIR="$OUT/$NAME"
mkdir -p "$BUILD"
if [ $GEN = 1 ]; then
  CC=$CC AR=$AR "$HERE/build_rando.sh" "$BUILD/rando" >&2 || { echo "package_release: libsomrando did not build (SOM_RANDO_SRC?); use --no-generator to link the stub" >&2; exit 3; }
  GENLIB="$BUILD/rando/libsomrando.a"
else
  GENLIB="$C/launcher/somrando_stub.c"
fi
# SOM_PC_REPO points nowhere: a release finds its data next to the executable
$CC -std=gnu99 -O2 -Wall -Wextra -Wno-unused-parameter -Wno-format-truncation -DSOM_PC_REPO='"/nonexistent-som-pc-source-tree"' $SDLC $SRC "$GENLIB" $SDLL "${LDX[@]}" -o "$BUILD/$EXE"

[ -d "$DIR" ] && mv "$DIR" "$OUT/$NAME.old-$(date +%s)-$$"
mkdir -p "$DIR/data" "$DIR/licenses" "$DIR/core" "$DIR/source"
cp "$BUILD/$EXE" "$DIR/$EXE"
cp "$REPO/data/credits.json" "$DIR/data/credits.json"
cp "$REPO/tools/gameplay/gameplay_menu.json" "$DIR/data/gameplay_menu.json"
cp "$REPO/LICENSE" "$DIR/licenses/GPL-3.0.txt"
cp "$C/randomizer/libsomrando/COPYING.LIB" "$DIR/licenses/LGPL-2.1.txt"
cp "$C/randomizer/libsomrando/NOTICE" "$DIR/licenses/libsomrando-NOTICE.txt"
[ "$PLAT" = linux ] && : > "$DIR/portable.txt"
if [ "$BUNDLE" = 1 ]; then
  if [ "$PLAT" = linux ]; then
    mkdir -p "$DIR/lib"
    cp -L "$(ldd "$BUILD/$EXE" | awk '/libSDL2-2.0/ {print $3}' | head -1)" "$DIR/lib/"
  else
    cp "${SDL2_PREFIX:?SDL2_PREFIX is needed to bundle SDL2.dll}/bin/SDL2.dll" "$DIR/"
  fi
  cat > "$DIR/licenses/SDL2-zlib.txt" <<'EOF'
SDL2 (https://libsdl.org) is distributed here under the zlib licence:

Copyright (C) 1997-2026 Sam Lantinga <slouken@libsdl.org>
This software is provided 'as-is', without any express or implied warranty.  In no event will the authors be held liable for any damages
arising from the use of this software.  Permission is granted to anyone to use this software for any purpose, including commercial
applications, and to alter it and redistribute it freely, subject to the following restrictions:
1. The origin of this software must not be misrepresented; you must not claim that you wrote the original software.  If you use this
   software in a product, an acknowledgment in the product documentation would be appreciated but is not required.
2. Altered source versions must be plainly marked as such, and must not be misrepresented as being the original software.
3. This notice may not be removed or altered from any source distribution.
EOF
fi
if [ -n "$CORE" ]; then
  if [ "$PLAT" = windows ]; then cp "$CORE" "$DIR/core/snes9x_widescreen_libretro.dll"; else cp "$CORE" "$DIR/core/snes9x_widescreen_libretro.so"; fi
fi
cat > "$DIR/core/README.txt" <<'EOF'
The emulator core is not part of this folder.  som-pc runs Secret of Mana on the Snes9x widescreen libretro core of this project
(Snes9x 1.63 plus tools/companion/widescreen/ws_snes9x.patch).  Snes9x is licensed for non-commercial use only and may not be
redistributed commercially, so build it yourself (needs git, g++, make):

    python3 tools/companion/widescreen/build_core.py <empty folder>

and copy the resulting libretro core here as

    snes9x_widescreen_libretro.so     (Linux)
    snes9x_widescreen_libretro.dll    (Windows)

or point to it with --core FILE.
EOF
cat > "$DIR/README.txt" <<'EOF'
som-pc: Secret of Mana for the PC
=================================

A frontend for the Secret of Mana decompilation project: 1080p widescreen (extra world on each side of the picture), a start menu, saves and
save states, display filters, the quality-of-life tweaks, and the generator for Ancient Cave and randomizer runs, all built in.  It needs
YOUR copy of the game: a US v1.0 ROM (2 MiB, .sfc or .smc).  No ROM data is included or written anywhere but your own save folders.

First run
---------
1. Put the emulator core in core/ (see core/README.txt).
2. Start the start menu:      ./som-pc --menu --rom "/path/to/Secret of Mana (USA).sfc"      (Windows: som-pc.exe --menu --rom "C:\path\to\rom.sfc")
   Add --save-config once and the options (window size, filter...) are remembered; the ROM path is kept too.
3. Play the original, create a randomized game, or open Saved Randomized, Tweaks, Display and Credits.

Where things are saved
----------------------
With the file portable.txt next to the program (always on Windows) everything goes to userdata/ in this folder:
    userdata/config/     config.json, gameplay.json (the tweaks), slot bindings, controller database
    userdata/state/      saves/ (battery saves), states/ (10 save state slots per game), gen/ (your generated runs), runs.json
Delete portable.txt to use ~/.config/som-decomp-pc and ~/.local/state/som-decomp/pc instead (SOM_PC_CONFIG and SOM_PC_STATE override both).
Every generated run is its own ROM, so it has its own save and its own states.  The save of a tweaked game stays that of the plain ROM.

Controls
--------
Keyboard: arrows, Z = B, X = A, A = Y, S = X, Enter = Start, Backspace = Select.  Esc or F10 opens the menu (save, load, video, credits, quit).
F7 quick save, F8 quick load, 1..0 pick a slot, [ ] previous/next slot, F11 fullscreen, F12 screenshot, Shift+F3 next display filter, F9 mute.
Pad: any SDL game controller.  Hold Select: R1 quick save, L1 quick load, D-pad left/right change slot.
Start menu: arrows and Enter/Esc (or the pad), the mouse works too.  On the menu, G (pad Back) opens the gameplay tweaks page.

Create a randomized game
------------------------
Create randomized -> choose a preset (Ancient Cave, Open World, Randomizer, Boss Rush, Chaos and their variants) -> the settings screen
(seed, difficulty and every option of the generator) -> Generate and play.  The game is made on your machine in a fraction of a second and
kept in Saved Randomized (the newest four are shown).  The generator is libsomrando, a C port of Moppleton's Secret of Mana Randomizer
1.49; it makes byte for byte the same ROM as the original program for the settings it lists as available.

Gameplay tweaks and quality of life
-----------------------------------
Tweaks page: weapon and run recharge, charge speed, flinch, critical hits, walking speed and the quality-of-life group (faster chests, fades
and transitions, 7 items per stack, MP on level-up, experience for fallen heroes, tighter scrolling).  P (pad Y) cycles the presets
(Vanilla, Quick Combat, Arcade, Quality of life).  The tweaks are applied to a copy of the ROM when the game starts; the original is never
modified.  Command line: --gameplay options.json with {"options": {"walk_speed": 3, ...}} (data/gameplay_menu.json lists every key).

Display
-------
Window sizes 720p to 2160p, windowed / borderless / exclusive fullscreen, integer scaling, picture width (wide 46/27/17 or 4:3), vsync and the
filters none / sharp bilinear / scanlines / CRT-lite / LCD grid (drawn by the program itself: there are no shader files).

Command line
------------
    som-pc --help                    all options
    som-pc --rom ROM --generate-all DIR [--gen-seed N] [--gen-preset ID]
                                     makes every preset (or one) into DIR without opening a window and prints one JSON line each

Credits and licences
--------------------
The in-game Credits screen and data/credits.json name everyone whose work or ideas are used.  NOTICE.txt and licences/ carry the licences.
EOF
cat > "$DIR/NOTICE.txt" <<'EOF'
som-pc: notices and licences
============================

This program: Copyright (C) 2026 cidwel, GNU General Public License version 3 or later (licenses/GPL-3.0.txt).
Secret of Mana is a work and trademark of its owners.  No ROM data, graphics, music or text of the game is part of this folder.

libsomrando (linked into the program)
  A C port of the generator of Moppu/SecretOfManaRandomizer 1.49, Copyright (c) Moppleton and contributors, GNU Lesser General Public
  License version 2.1 or later (licenses/LGPL-2.1.txt, licenses/libsomrando-NOTICE.txt).  The library is linked statically.  As the LGPL requires,
  you may modify it and relink: source/som-pc-source.tar.gz holds the complete source of the program and of the library, and
  pc/package_release.sh in it rebuilds the folder (libsomrando needs the upstream sources at commit 1404b7b for its build-time resources:
  see licenses/libsomrando-NOTICE.txt).  The Ancient Cave quote pools carried in the library belong to their authors and are used as in upstream.

SDL2 (https://libsdl.org), zlib licence; loaded from the system, or bundled with its notice in licenses/SDL2-zlib.txt when this folder was built with --bundle-sdl.

Snes9x (emulator core)
  Not included.  Snes9x is licensed for non-commercial use only; you build the core yourself (core/README.txt).

Quality-of-life ideas and patches
  The authors and sources of every tweak are listed in data/credits.json and in the in-game Credits screen (the Ancient Cave mode is Moppleton's).
EOF
# the LGPL source offer: everything needed to rebuild the program and relink the library, no game data
( cd "$REPO" && {
    find tools/companion -maxdepth 1 -type f \( -name '*.c' -o -name '*.h' \)
    find tools/companion/pc tools/companion/launcher tools/companion/gameplay_c tools/companion/ux tools/companion/direct -type f \( -name '*.c' -o -name '*.h' -o -name '*.sh' -o -name '*.yml' -o -name 'som-pc' \) -not -path '*/evidence/*'
    find tools/companion/randomizer/libsomrando -type f -not -path '*/__pycache__/*' -not -path '*/oracle/*'
    echo tools/companion/widescreen/ws_snes9x.patch; echo tools/companion/widescreen/build_core.py
    echo tools/companion/randomizer/presets.json
    echo tools/gameplay/gameplay_menu.json; echo data/credits.json; echo LICENSE
  } | sort -u | tar -czf "$DIR/source/som-pc-source.tar.gz" -T - )
if [ "$PLAT" = windows ]; then
  ( cd "$OUT" && zip -qr "$NAME.zip" "$NAME" )
else
  ( cd "$OUT" && tar -czf "$NAME.tar.gz" "$NAME" )
fi
echo "release folder: $DIR"
