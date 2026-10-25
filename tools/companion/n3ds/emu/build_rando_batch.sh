#!/bin/bash
# Builds test/rando_batch.c as a 3DS app (needs DEVKITPRO and a libsomrando.a from build_libsomrando.sh) and the host build for comparison.
#   DEVKITPRO=<dir> build_rando_batch.sh <libsomrando.a> <out dir>        -> <out dir>/rando_batch.3dsx
set -euo pipefail
LIBA=${1:?usage: build_rando_batch.sh <libsomrando.a> <out dir>}
OUT=${2:?out dir}
: "${DEVKITPRO:?set DEVKITPRO}"
HERE=$(cd "$(dirname "$0")" && pwd)
C=$(cd "$HERE/../.." && pwd)
GCC=$DEVKITPRO/devkitARM/bin/arm-none-eabi-gcc
ARCH="-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft"
# --whole-archive is not required (the generator has no weak stubs); NO_WHOLE_ARCHIVE=1 links a plain archive to prove it
if [ "${NO_WHOLE_ARCHIVE:-0}" = 1 ]; then WA1=; WA2=; else WA1=-Wl,--whole-archive; WA2=-Wl,--no-whole-archive; fi
mkdir -p "$OUT"
cd "$OUT"
for f in "$HERE/../test/rando_batch.c" "$C/launcher/som_launcher.c" "$C/launcher/som_launcher_catalog.c" "$C/launcher/somrando_menu.c"; do
  $GCC -std=gnu99 -O2 -mword-relocations -ffunction-sections $ARCH -D__3DS__ -Wno-format-truncation -I"$C" -I"$DEVKITPRO/libctru/include" -c "$f" -o "$(basename "$f" .c).o"
done
$GCC -specs=3dsx.specs $ARCH -L"$DEVKITPRO/libctru/lib" rando_batch.o som_launcher.o som_launcher_catalog.o somrando_menu.o $WA1 "$LIBA" $WA2 -lctru -lm -o rando_batch.elf
"$DEVKITPRO/tools/bin/3dsxtool" rando_batch.elf rando_batch.3dsx
echo "built $OUT/rando_batch.3dsx"
