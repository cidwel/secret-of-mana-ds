#!/bin/sh
# Builds and runs the binding tests on the PC (with ASan/UBSan) and as a 32-bit ARM Linux binary under qemu-arm (the 3DS CPU family).
set -e
cd "$(dirname "$0")"
SRC="som_bind.c som_hotkey.c som_companion.c direct/som_direct.c ux/som_ui_draw.c ux/som_ui_state.c ux/som_ui_sel.c ux/som_ui_zone.c"
OUT=${TMPDIR:-/tmp}
for T in test_som_bind test_som_bind_ux; do
  gcc -std=c99 -Wall -Wextra -O1 -g -fsanitize=address,undefined -I. $T.c $SRC -lm -o $OUT/${T}_pc && $OUT/${T}_pc
done
if command -v arm-linux-gnueabihf-gcc >/dev/null && command -v qemu-arm >/dev/null; then
  for T in test_som_bind test_som_bind_ux; do
    arm-linux-gnueabihf-gcc -std=c99 -Wall -Wextra -O2 -static -I. $T.c $SRC -lm -o $OUT/${T}_arm
    qemu-arm $OUT/${T}_arm
  done
else echo "qemu-arm not available: ARM run skipped"; fi
