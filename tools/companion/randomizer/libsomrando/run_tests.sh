#!/bin/bash
# Builds libsomrando and runs every test of the port.  Usage: run_tests.sh [--quick]
#   1. .NET reference vectors (RNG streams, hash codes, Math.Pow sweeps) are generated from the upstream sources with `dotnet`
#   2. test_core: C against the vectors, x86-64 (-O2), x86-64 with ASan/UBSan, and 32-bit ARM under qemu-arm (when available)
#   3. diff_harness.py: C# generator against libsomrando, byte for byte (options, baseline seeds, option matrix, random configurations, API,
#      presets); once with the -O2 build and, unless --quick, a smaller run with the ASan build and the ARM build
# Environment: SOM_ROM (the user's US v1.0 ROM), SOM_RANDO_SRC (upstream SoMRandomizer folder), SOM_CLI_STATE (C# oracle build, see
# tools/companion/randomizer/build_native.sh exe-aot), SOMRANDO_PORT_STATE (output root).  Nothing here writes into the repository.
set -euo pipefail
HERE=$(cd "$(dirname "$0")" && pwd)
STATE=${SOMRANDO_PORT_STATE:-/home/cidwel/.local/state/som-decomp/randoport-20261018}
CLI_STATE=${SOM_CLI_STATE:-/home/cidwel/.local/state/som-decomp/companion-cli-20261017}
export SOM_RANDO_SRC=${SOM_RANDO_SRC:-$CLI_STATE/repo-moppu/SecretOfManaRandomizer/SoMRandomizer}
export NUGET_PACKAGES=${NUGET_PACKAGES:-$CLI_STATE/aot/nuget} DOTNET_CLI_TELEMETRY_OPTOUT=1 DOTNET_NOLOGO=1
ROM=${SOM_ROM:-/home/cidwel/projects/rommancer/roms/snes/Secret of Mana (USA).smc}
export SOMRANDO_BUILD=$STATE/build SOMRANDO_PORT_STATE=$STATE
mkdir -p "$STATE"

if [ ! -f "$STATE/oracle-bin/OracleVectors.dll" ]; then
  (cd "$HERE/tests/oracle" && dotnet build -c Release -p:BaseIntermediateOutputPath="$STATE/oracle-obj/" -o "$STATE/oracle-bin" >/dev/null)
fi
dotnet "$STATE/oracle-bin/OracleVectors.dll" rng > "$STATE/vec_rng.txt"
dotnet "$STATE/oracle-bin/OracleVectors.dll" pow "$SOM_RANDO_SRC/Resources" > "$STATE/vec_pow.txt"

"$HERE/build.sh" lib
"$HERE/build.sh" asan
echo "== test_core (x86-64 -O2)"
"$STATE/build/lib/test_core" "$STATE/vec_rng.txt" "$STATE/vec_pow.txt" "$ROM"
echo "== test_core (x86-64 ASan/UBSan)"
"$STATE/build/asan/test_core" "$STATE/vec_rng.txt" "$STATE/vec_pow.txt" "$ROM"

ARM=0
if command -v arm-linux-gnueabihf-gcc >/dev/null && command -v qemu-arm >/dev/null; then
  ARM=1
  mkdir -p "$STATE/build/arm32"
  API=$(cd "$HERE/../../launcher" && pwd)
  for t in test_core sr_cli; do
    arm-linux-gnueabihf-gcc -static -std=c99 -D_POSIX_C_SOURCE=200809L -DSOMRANDO_DEBUG -O2 -ffp-contract=off -Wall -Wextra -I"$API" -I"$HERE/src" \
      -o "$STATE/build/arm32/$t" "$HERE/tests/$t.c" "$HERE"/src/*.c "$HERE/data/sr_quotes_mitch.c" "$HERE/data/sr_quotes_demetri.c" "$STATE/build/lib/sr_resources_p4_ev1_gen.c" "$STATE/build/lib/sr_p4_resources_gen.c" -lm
  done
  echo "== test_core (32-bit ARM under qemu-arm)"
  SR_QUIET=1 qemu-arm "$STATE/build/arm32/test_core" "$STATE/vec_rng.txt" "$STATE/vec_pow.txt"
fi

echo "== differential harness (-O2)"
python3 "$HERE/tests/diff_harness.py" --jobs 6 --hashes "$STATE/diff/hashes.json" --preset-hashes "$STATE/diff/preset_hashes.json"
echo "== regression hashes (C only, no oracle)"
python3 "$HERE/tests/diff_harness.py" --jobs 6 --verify-hashes "$HERE/tests/regression_hashes.json"
if [ "${1:-}" != "--quick" ]; then
  echo "== differential harness (ASan/UBSan, reduced)"
  SOMRANDO_CLI="$STATE/build/asan/sr_cli" python3 "$HERE/tests/diff_harness.py" --jobs 4 --seeds 6 --random 10 --preset-seeds 6 --feature-subsets 6 --open-seeds 4 --open-random 12 --report "$STATE/diff/report_asan.json"
  if [ "$ARM" = 1 ]; then
    echo "== differential harness (32-bit ARM under qemu-arm, reduced)"
    cat > "$STATE/qemu_cli.sh" <<EOS
#!/bin/sh
exec qemu-arm "$STATE/build/arm32/sr_cli" "\$@"
EOS
    chmod +x "$STATE/qemu_cli.sh"
    SOMRANDO_CLI="$STATE/qemu_cli.sh" python3 "$HERE/tests/diff_harness.py" --jobs 4 --seeds 4 --random 6 --skip-matrix --preset-seeds 3 --feature-subsets 3 --open-seeds 2 --open-random 6 --report "$STATE/diff/report_arm.json"
  fi
fi
echo "== no ROM bytes in the built binaries and libraries"
SOM_ROM="$ROM" python3 "$HERE/tests/scan_rom_bytes.py" --selftest "$STATE/build/lib/sr_cli" "$STATE/build/lib/test_core" "$STATE/build/asan/sr_cli" $([ "$ARM" = 1 ] && echo "$STATE/build/arm32/sr_cli")
echo "ALL TESTS PASSED"
