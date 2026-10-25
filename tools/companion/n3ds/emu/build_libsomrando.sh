#!/bin/bash
# Builds libsomrando (C port of Moppu/SecretOfManaRandomizer v1.49, LGPL-2.1+) as a static library for the 3DS with devkitARM (newlib).
#
#   DEVKITPRO=<dir> SOM_RANDO_SRC=<upstream SoMRandomizer/ folder> build_libsomrando.sh <out dir>      -> <out dir>/libsomrando.a
#
# Same flags as the 3DS app (ARM11, hard float, no fused multiply-add: the generator's double arithmetic must match the .NET results bit for bit).
# At most 4 compiler processes at a time.  The build-time resources are generated from the upstream checkout and are not stored in the repository.
set -euo pipefail
OUT=${1:?usage: build_libsomrando.sh <out dir>}
: "${DEVKITPRO:?set DEVKITPRO}"
: "${SOM_RANDO_SRC:?set SOM_RANDO_SRC to the upstream SoMRandomizer folder}"
HERE=$(cd "$(dirname "$0")" && pwd)
LIB=$(cd "$HERE/../../randomizer/libsomrando" && pwd)
LAUNCHER=$(cd "$HERE/../../launcher" && pwd)
CC=$DEVKITPRO/devkitARM/bin/arm-none-eabi-gcc
AR=$DEVKITPRO/devkitARM/bin/arm-none-eabi-ar
mkdir -p "$OUT/obj" "$OUT/gen"
python3 "$LIB/tools/gen_p4_ev1_resources.py" "$SOM_RANDO_SRC" "$OUT/gen/sr_resources_p4_ev1_gen.c" > /dev/null
python3 "$LIB/tools/gen_p4_hmisc_resources.py" "$SOM_RANDO_SRC" "$OUT/gen/sr_p4_resources_gen.c" > /dev/null
GEN="$OUT/gen/sr_resources_p4_ev1_gen.c $OUT/gen/sr_p4_resources_gen.c"   # explicit: a stale file of an older layout must not be archived
ls "$LIB"/src/*.c "$LIB/data/sr_quotes_mitch.c" "$LIB/data/sr_quotes_demetri.c" $GEN > "$OUT/sources.txt"
export CC LIB LAUNCHER OUT
xargs -P4 -I{} sh -c '
  o="$OUT/obj/$(basename {} .c).o"
  "$CC" -std=gnu99 -D_POSIX_C_SOURCE=200809L -O2 -ffp-contract=off -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -ffunction-sections \
    -D__3DS__ -DSOMRANDO_ADVERTISE=1 -I"$LAUNCHER" -I"$LIB/src" -c {} -o "$o"' < "$OUT/sources.txt"
[ -e "$OUT/libsomrando.a" ] && mv -f "$OUT/libsomrando.a" "$OUT/libsomrando.a.old"
"$AR" rcs "$OUT/libsomrando.a" $(sed "s|.*/\(.*\)\.c$|$OUT/obj/\1.o|" "$OUT/sources.txt")
echo "built $OUT/libsomrando.a"
