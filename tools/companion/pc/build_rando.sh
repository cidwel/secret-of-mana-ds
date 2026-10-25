#!/bin/bash
# Builds libsomrando (C port of Moppu/SecretOfManaRandomizer v1.49, LGPL-2.1+) as a static library for the PC frontend.
#   build_rando.sh [OUT_DIR]            -> OUT_DIR/libsomrando.a   (default: $SOM_PC_BUILD/rando or ~/.local/state/som-decomp/pc/build/rando)
# Environment: SOM_RANDO_SRC  upstream SoMRandomizer/ project folder (build-time resources only: Open World pokedex text and nesoberi art; no ROM data)
#              CC, AR         cross compiler and archiver (default gcc, ar), e.g. x86_64-w64-mingw32-gcc for Windows
#              JOBS           compiler processes at a time (default 4, the shared-machine limit)
# Objects are rebuilt only when a source or a header is newer.  Without SOM_RANDO_SRC the script exits with status 3 and the PC build
# links the stub (no generator), see pc/som-pc.
set -euo pipefail
HERE=$(cd "$(dirname "$(readlink -f "$0")")" && pwd)
LIB=$(cd "$HERE/../randomizer/libsomrando" && pwd)
API=$(cd "$HERE/../launcher" && pwd)
OUT=${1:-${SOM_PC_BUILD:-$HOME/.local/state/som-decomp/pc/build}/rando}
CC=${CC:-gcc}; AR=${AR:-ar}; JOBS=${JOBS:-4}
[ "$JOBS" -gt 4 ] && JOBS=4
STATE=${SOM_CLI_STATE:-$HOME/.local/state/som-decomp/companion-cli-20261017}
SOM_RANDO_SRC=${SOM_RANDO_SRC:-$STATE/repo-moppu/SecretOfManaRandomizer/SoMRandomizer}
if [ ! -d "$SOM_RANDO_SRC" ]; then echo "build_rando: upstream sources not found (SOM_RANDO_SRC=$SOM_RANDO_SRC)" >&2; exit 3; fi
mkdir -p "$OUT/obj" "$OUT/gen"
newest_hdr=$(ls -t "$LIB"/src/*.h "$LIB"/src/*.inc "$API/somrando_api.h" 2>/dev/null | head -1)
regen() { if [ -s "$OUT/gen/$1" ] && [ "$OUT/gen/$1" -nt "$LIB/tools/$2" ]; then return 0; fi; python3 "$LIB/tools/$2" "$SOM_RANDO_SRC" "$OUT/gen/$1" > /dev/null; }
regen sr_resources_p4_ev1_gen.c gen_p4_ev1_resources.py
regen sr_p4_resources_gen.c gen_p4_hmisc_resources.py
# explicit list: an object or generated file left by an older layout must never be archived
ls "$LIB"/src/*.c "$LIB/data/sr_quotes_mitch.c" "$LIB/data/sr_quotes_demetri.c" "$OUT/gen/sr_resources_p4_ev1_gen.c" "$OUT/gen/sr_p4_resources_gen.c" > "$OUT/sources.txt"
export CC LIB API OUT newest_hdr
xargs -P"$JOBS" -I{} sh -c '
  o="$OUT/obj/$(basename {} .c).o"
  if [ -e "$o" ] && [ "$o" -nt {} ] && [ "$o" -nt "$newest_hdr" ]; then exit 0; fi
  "$CC" -std=gnu99 -D_POSIX_C_SOURCE=200809L -O2 -ffp-contract=off -DSOMRANDO_ADVERTISE=1 -I"$API" -I"$LIB/src" -c {} -o "$o.tmp.o" && mv "$o.tmp.o" "$o"' < "$OUT/sources.txt"
OBJS=$(sed "s|.*/\(.*\)\.c$|$OUT/obj/\1.o|" "$OUT/sources.txt")
newest_obj=$(ls -t $OBJS | head -1)
if [ ! -f "$OUT/libsomrando.a" ] || [ "$newest_obj" -nt "$OUT/libsomrando.a" ]; then
  "$AR" rcs "$OUT/libsomrando.a.new" $OBJS
  mv -f "$OUT/libsomrando.a.new" "$OUT/libsomrando.a"
fi
touch "$OUT/libsomrando.a"
echo "built $OUT/libsomrando.a"
