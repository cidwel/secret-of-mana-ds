#!/bin/sh
# Builds and runs the start menu tests (model, renderer, serialisation cross-check).  Usage: run_launcher_tests.sh [outdir]
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-${SOM_LAUNCHER_BUILD:-/home/cidwel/.local/state/som-decomp/launcher-20261018/build}}
mkdir -p "$OUT"
cd "$HERE"
python3 gen_catalog.py --check || { echo "som_launcher_catalog.c is out of date: run gen_catalog.py"; exit 1; }
gcc -std=c99 -O1 -g -Wall -Wextra -fsanitize=address,undefined -o "$OUT/test_launcher" \
  test_launcher.c som_launcher.c som_launcher_catalog.c som_launcher_draw.c -lm
"$OUT/test_launcher"
"$OUT/test_launcher" --emit > "$OUT/cases.tsv"
python3 test_launcher_config.py "$OUT/cases.tsv"
# optional main menu entries (tweaks, display, credits) and the credits screen / text
gcc -std=c99 -O1 -g -Wall -Wextra -fsanitize=address,undefined -o "$OUT/test_launcher_extras" test_launcher_extras.c som_launcher.c som_launcher_catalog.c som_launcher_draw.c som_credits.c -lm
"$OUT/test_launcher_extras" "$HERE/../../../data/credits.json"
# menu glue against libsomrando (needs `libsomrando/build.sh lib` first, for the generated resources)
LIB="$HERE/../randomizer/libsomrando"
GEN=${SOMRANDO_BUILD:-/home/cidwel/.local/state/som-decomp/randoport-20261018/build}/lib
if [ -f "$GEN/sr_resources_p4_ev1_gen.c" ] && [ -f "$GEN/sr_p4_resources_gen.c" ]; then
  gcc -std=c99 -D_POSIX_C_SOURCE=200809L -O1 -ffp-contract=off -I. -I"$LIB/src" -o "$OUT/test_somrando_menu" test_somrando_menu.c somrando_menu.c \
    som_launcher.c som_launcher_catalog.c som_launcher_draw.c "$LIB"/src/*.c "$LIB/data/sr_quotes_mitch.c" "$LIB/data/sr_quotes_demetri.c" \
    "$GEN/sr_resources_p4_ev1_gen.c" "$GEN/sr_p4_resources_gen.c" -lm
  "$OUT/test_somrando_menu" | tail -1
fi
