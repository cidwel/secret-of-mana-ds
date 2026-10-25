#!/bin/bash
# Builds libsomrando and its tests outside the repository tree.
#   build.sh [lib|asan|clean-list]       lib: static library + sr_cli + test_core (-O2); asan: same with ASan/UBSan
# Environment: SOM_RANDO_SRC (upstream SoMRandomizer/ project folder, for the build-time resources), SOMRANDO_BUILD (output root).
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
API=$(cd "$HERE/../../launcher" && pwd)
STATE=${SOM_CLI_STATE:-/home/cidwel/.local/state/som-decomp/companion-cli-20261017}
export SOM_RANDO_SRC=${SOM_RANDO_SRC:-$STATE/repo-moppu/SecretOfManaRandomizer/SoMRandomizer}
what=${1:-lib}
OUT=${SOMRANDO_BUILD:-/home/cidwel/.local/state/som-decomp/randoport-20261018/build}/$what
mkdir -p "$OUT"
python3 "$HERE/tools/gen_p4_ev1_resources.py" "$SOM_RANDO_SRC" "$OUT/sr_resources_p4_ev1_gen.c"
python3 "$HERE/tools/gen_p4_hmisc_resources.py" "$SOM_RANDO_SRC" "$OUT/sr_p4_resources_gen.c"
# -ffp-contract=off: no fused multiply-add, so double arithmetic matches the .NET results bit for bit
CFLAGS=(-std=c99 -D_POSIX_C_SOURCE=200809L -DSOMRANDO_DEBUG -Wall -Wextra -ffp-contract=off -I"$API" -I"$HERE/src")
case $what in
  lib)  CFLAGS+=(-O2) ;;
  asan) CFLAGS+=(-O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined) ;;
  *) echo "usage: build.sh lib|asan"; exit 2 ;;
esac
# the quote text of Ancient Cave is third-party data kept in data/ (SOMRANDO_QUOTES=0 builds with the placeholder pools instead)
if [ "${SOMRANDO_QUOTES:-1}" = 0 ]; then QUOTES=("$HERE/data/sr_quotes_placeholder.c"); else QUOTES=("$HERE/data/sr_quotes_mitch.c" "$HERE/data/sr_quotes_demetri.c"); fi
SRCS=("$HERE"/src/*.c "${QUOTES[@]}" "$OUT/sr_resources_p4_ev1_gen.c" "$OUT/sr_p4_resources_gen.c")
gcc "${CFLAGS[@]}" -o "$OUT/sr_cli" "$HERE/tests/sr_cli.c" "${SRCS[@]}" -lm
gcc "${CFLAGS[@]}" -o "$OUT/ac_replay" "$HERE/tests/ac_replay.c" "${SRCS[@]}" -lm
gcc "${CFLAGS[@]}" -o "$OUT/test_core" "$HERE/tests/test_core.c" "${SRCS[@]}" -lm
echo "built $OUT"
