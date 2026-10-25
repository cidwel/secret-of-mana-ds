#!/bin/bash
# Thor round 4 tests: save states, start-menu pages, gameplay applier parity, RunKeys (JVM) and the shared UX layer.
# Usage: run_thor_tests.sh [workdir]   (default ~/.local/state/som-decomp/thor-r4-20261020/tests)
set -e
HERE=$(cd "$(dirname "$0")" && pwd)
C=$HERE/../..
W=${1:-$HOME/.local/state/som-decomp/thor-r4-20261020/tests}
mkdir -p "$W"
CF="-std=c99 -O1 -g -fsanitize=address,undefined -fno-sanitize-recover=undefined -Wall -Wextra"
echo "== save states"
cc $CF -D_POSIX_C_SOURCE=200809L -o "$W/test_states" "$HERE/test_states.c" "$C/som_states.c" && "$W/test_states" "$W/sst-$(date +%s)-$$"
echo "== start-menu pages (gameplay + credits)"
cc $CF -D_POSIX_C_SOURCE=200809L -I"$C" -I"$C/launcher" -o "$W/test_start_pages" "$HERE/test_start_pages.c" "$HERE/../app/src/main/cpp/som_start_pages.c" \
  "$C/som_gameplay_apply.c" "$C/pc/pc_gameplay.c" "$C/launcher/som_launcher.c" "$C/launcher/som_launcher_draw.c" "$C/launcher/som_launcher_catalog.c" "$C/launcher/som_credits.c" "$C/launcher/somrando_stub.c" -lm
mkdir -p "$W/pages" && "$W/test_start_pages" "$W/pages" "$(cd "$C/../.." && pwd)"
echo "== gameplay applier parity (C vs Python)"
python3 "$HERE/gameplay_parity.py"
echo "== RunKeys (JVM)"
mkdir -p "$W/jvm" && javac -d "$W/jvm" "$HERE/../app/src/main/java/org/somdecomp/companion/RunKeys.java" "$HERE/jvm/RunKeysTest.java" && java -cp "$W/jvm" org.somdecomp.companion.RunKeysTest
