#!/bin/bash
# Builds a libretro save state that the 3DS build can load (state.sav).  A state made by an x86-64 core is NOT portable: the 3DS core reads its
# pointer-sized fields differently and crashes.  Here snes9x2005 is built as a 32-bit ARM Linux library (same ABI layout as the 3DS build) and the
# companion host runs it under qemu-arm to play the TAS route and set up the party fixture (tools/companion/build_fixture.py).
#
#   build_armhf_fixture.sh <snes9x2005-source-dir> <workdir>      (needs arm-linux-gnueabihf-gcc, qemu-arm, /usr/arm-linux-gnueabihf)
# Result: <workdir>/fixture/work/party3_vanilla.state  ->  copy it to sdmc:/som/state.sav (loaded into the ORIGINAL ROM only).
set -euo pipefail
SRC=${1:?snes9x2005 source dir}; WORK=${2:?empty work dir}
HERE=$(cd "$(dirname "$0")" && pwd); COMP=$(cd "$HERE/../.." && pwd)
mkdir -p "$WORK/core" "$WORK/fixture"
cp -r "$SRC"/. "$WORK/core/"
( cd "$WORK/core" && CFLAGS="-O2" make -j8 platform=unix CC=arm-linux-gnueabihf-gcc CXX=arm-linux-gnueabihf-g++ AR=arm-linux-gnueabihf-ar >/dev/null )
arm-linux-gnueabihf-gcc -std=gnu99 -O2 -o "$WORK/host_armhf" "$COMP/som_companion.c" "$COMP/som_companion_host.c" -ldl
cat > "$WORK/hostwrap.sh" <<WRAP
#!/bin/bash
exec qemu-arm -L /usr/arm-linux-gnueabihf $WORK/host_armhf "\$@"
WRAP
chmod +x "$WORK/hostwrap.sh"
cat > "$WORK/mkfix.py" <<PY
import sys, pathlib
sys.path.insert(0, '$COMP')
import common
common.build_host = lambda: pathlib.Path('$WORK/hostwrap.sh')
import build_fixture
sys.argv = ['build_fixture.py']
build_fixture.main()
PY
SOM_COMPANION_STATE=$WORK/fixture SOM_CORE=$WORK/core/snes9x2005_libretro.so python3 "$WORK/mkfix.py"
