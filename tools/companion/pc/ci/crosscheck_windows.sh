#!/bin/bash
# Cross compile check for Windows from Linux with mingw-w64, without the SDL2 mingw package: builds libsomrando and every source of som-pc for
# x86_64-w64-mingw32 (SDL2 headers are taken from the Linux package, they are the same API), links, and lists what stays unresolved.  The only
# unresolved symbols must be SDL_* (the import library of the SDL2 mingw package provides them) and WinMain (SDL2main).
# For a real Windows binary see package_release.sh --platform windows with SDL2_PREFIX, or the GitHub Actions recipe next to this file.
# usage: crosscheck_windows.sh           exit status 0 when nothing but SDL is missing
set -euo pipefail
HERE="$(cd "$(dirname "$(readlink -f "$0")")/.." && pwd)"
C="$HERE/.."
REPO="$(cd "$C/../.." && pwd)"
CC=${CC:-x86_64-w64-mingw32-gcc}; AR=${AR:-x86_64-w64-mingw32-ar}
W="${SOM_PC_BUILD_WIN:-$HOME/.local/state/som-decomp/pc/build-win}"
INC="$W/sdl-headers"
. "$HERE/sources.sh"
mkdir -p "$INC/SDL2"
cp /usr/include/SDL2/*.h "$INC/SDL2/"
cp /usr/include/x86_64-linux-gnu/SDL2/_real_SDL_config.h "$INC/SDL2/" 2>/dev/null || true
CC=$CC AR=$AR "$HERE/build_rando.sh" "$W/rando" >&2
set +e
OUT=$($CC -std=gnu99 -O2 -DSOM_PC_REPO='"."' -Wno-format -I"$INC/SDL2" -I"$INC" $SRC "$W/rando/libsomrando.a" -lm -o "$W/som-pc.exe" 2>&1)
set -e
BAD=$(echo "$OUT" | grep -E "error:|fatal error" | grep -v "ld returned" || true)
UNRES=$(echo "$OUT" | grep "undefined reference" | sed "s/.*undefined reference to .//; s/'.*//" | sort -u | grep -v -E '^(SDL_|WinMain)|ld.bfd|more undefined' || true)
if [ -n "$BAD" ] || [ -n "$UNRES" ]; then echo "$BAD"; echo "unexpected unresolved: $UNRES"; exit 1; fi
echo "windows cross check: all sources compile, only SDL2 symbols are left to the import library ($(echo "$OUT" | grep -c 'undefined reference') references)"
