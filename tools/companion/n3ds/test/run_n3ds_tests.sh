#!/bin/bash
# Host tests of the 3DS frontend modules (no console, no emulator): sui (screens), savefile (battery RAM and states), settings (options table),
# the C gameplay patcher against the Python tool, and the direct-control signature check on a patched ROM.
#
#   SOM_ROM=<US v1.0 rom> [SOM_ROM_EXTRA=a.sfc:b.sfc] test/run_n3ds_tests.sh [scratch dir]
#
# Without SOM_ROM the ROM dependent tests are skipped.  Output goes to the scratch dir (default ~/.local/state/som-decomp/n3ds-tests), never the repository.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
N3=$(cd "$HERE/.." && pwd)
C=$(cd "$N3/.." && pwd)
T=${1:-$HOME/.local/state/som-decomp/n3ds-tests}
mkdir -p "$T/gen" "$T/files"
export TMPDIR=$T
F="-std=c99 -Wall -Wextra -O1 -fsanitize=address,undefined -fno-sanitize-recover=undefined"
python3 "$N3/gen_data.py" "$T/gen/gen_data.h"
gcc $F -o "$T/test_sui" "$HERE/test_sui.c" "$N3/source/sui.c" && "$T/test_sui" "$T/files/sui"
gcc $F -o "$T/test_savefile" "$HERE/test_savefile.c" "$N3/source/savefile.c" && "$T/test_savefile" "$T/files"
gcc $F -I"$T/gen" -I"$C" -I"$N3/source" -o "$T/test_settings" "$HERE/test_settings.c" "$N3/source/settings.c" "$C/gameplay_c/som_gameplay.c" && "$T/test_settings" "$T/files"
if [ -n "${SOM_ROM:-}" ]; then
  python3 "$C/gameplay_c/test_gameplay_c.py" 60
  gcc -std=gnu99 -Wall -Wextra -O1 -fsanitize=address,undefined -I"$C/gameplay_c" -I"$C" -o "$T/test_direct_ok" "$C/gameplay_c/test_direct_ok.c" "$C/gameplay_c/som_gameplay.c" "$C/direct/som_direct.c" -lm && "$T/test_direct_ok" "$SOM_ROM"
else
  echo "SOM_ROM not set: gameplay comparison and direct-control check skipped"
fi
echo "n3ds host tests: ok"
