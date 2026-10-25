#!/bin/bash
# Checks the portable release: builds it into a scratch folder, looks for ROM bytes in it, runs the packaged binary from another directory
# (data found next to the executable, portable user data, in-process generation, gameplay tweaks through the C patcher) and rebuilds the
# program from the source archive it carries.  Needs the owner's ROM for the run checks (skipped without it).
# usage: test_release.sh [ROM]
HERE="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
ST="$HOME/.local/state/som-decomp"
ROM="${1:-/home/cidwel/projects/rommancer/roms/snes/Secret of Mana (USA).smc}"
CORE="${SOM_PC_CORE:-$ST/companion-20261017/widescreen/pc-x86_64/snes9x_widescreen_libretro.so}"
TRASH="$ST/pending-delete-20261020"
W="$ST/pc/release-test"
mkdir -p "$TRASH"
[ -d "$W" ] && mv "$W" "$TRASH/release-test-$(date +%s)-$$"
mkdir -p "$W"
pass=0; fail=0
ok() { if eval "$2"; then pass=$((pass + 1)); echo "PASS $1"; else fail=$((fail + 1)); echo "FAIL $1   [$2]"; fi; }

"$HERE/package_release.sh" --out "$W/out" --version test > "$W/package.log" 2>&1
R="$W/out/som-pc-test-linux-x86_64"
ok "release folder has the binary, README, NOTICE, data, licences, core note and source archive" \
   "[ -x '$R/som-pc' ] && [ -s '$R/README.txt' ] && [ -s '$R/NOTICE.txt' ] && [ -s '$R/data/credits.json' ] && [ -s '$R/data/gameplay_menu.json' ] && [ -s '$R/licenses/LGPL-2.1.txt' ] && [ -s '$R/licenses/GPL-3.0.txt' ] && [ -s '$R/core/README.txt' ] && [ -s '$R/source/som-pc-source.tar.gz' ]"
ok "NOTICE names libsomrando and its LGPL relink offer" "grep -q 'LGPL' '$R/NOTICE.txt' && grep -q 'relink' '$R/NOTICE.txt' && grep -q 'Moppleton' '$R/NOTICE.txt'"
ok "no ROM, save, state or image files in the folder" "[ -z \"\$(find '$R' -type f \( -iname '*.sfc' -o -iname '*.smc' -o -iname '*.srm' -o -iname '*.state' -o -iname '*.bmp' -o -iname '*.png' -o -iname '*.bin' \))\" ]"
ok "the binary does not embed the build tree" "! grep -a -q '$HOME' '$R/som-pc'"
ok "the source archive has the library, the sources list and no game data" "tar -tzf '$R/source/som-pc-source.tar.gz' | grep -q 'randomizer/libsomrando/src/sr_generate.c' && tar -tzf '$R/source/som-pc-source.tar.gz' | grep -q 'pc/sources.sh' && ! tar -tzf '$R/source/som-pc-source.tar.gz' | grep -qiE '\.(sfc|smc|srm|state)$'"

# ROM bytes: every run of 24 or more bytes of the owner's ROM that occurs in a file of the folder or in the source archive.  The only known ones are
# the build-time resources of libsomrando (upstream's Vineshroom and event tile data, about 4 KiB, linked into the binary: see docs/companion-pc-20261020.md);
# anything else, or more than 8 KiB in total, fails.
if [ -f "$ROM" ]; then
  python3 - "$ROM" "$R" > "$W/scan.txt" <<'PY'
import os, sys, tarfile
rom = open(sys.argv[1], 'rb').read()
if len(rom) % 1024 == 512: rom = rom[512:]
blobs = []
for d, _, fs in os.walk(sys.argv[2]):
    for f in fs:
        p = os.path.join(d, f)
        data = open(p, 'rb').read()
        blobs.append((p, data))
        if p.endswith('.tar.gz'):
            with tarfile.open(p) as t:
                for m in t.getmembers():
                    if m.isfile(): blobs.append((p + ':' + m.name, t.extractfile(m).read()))
W = 16
rw = {}
for o in range(0, len(rom) - W):
    w = rom[o:o + W]
    if len(set(w)) > 5: rw.setdefault(w, o)
tot = {}
for p, b in blobs:
    i = 0
    n = len(b)
    while i < n - W:
        o = rw.get(b[i:i + W])
        if o is None: i += 1; continue
        k = W
        while o + k < len(rom) and i + k < n and rom[o + k] == b[i + k]: k += 1
        if k >= 24: tot[p] = tot.get(p, 0) + k
        i += k
elsewhere = {p: n for p, n in tot.items() if not p.endswith('/som-pc')}
print('files %d, ROM bytes in the binary %d, in other files %d' % (len(blobs), sum(n for p, n in tot.items() if p.endswith('/som-pc')), sum(elsewhere.values())))
for p, n in elsewhere.items(): print('HIT', p, n)
PY
  cat "$W/scan.txt" | head -1
  ok "ROM bytes: none in the binary or in any other file (runs of 24 bytes or more)" "[ \$(wc -l < '$W/scan.txt') = 1 ] && [ \$(sed -n 's/.*in the binary \([0-9]*\),.*/\1/p' '$W/scan.txt') = 0 ]"
  ok "no 32-byte window of the ROM in any file of the release (libsomrando/tests/scan_rom_bytes.py)" "SOM_ROM='$ROM' python3 '$HERE/../randomizer/libsomrando/tests/scan_rom_bytes.py' '$R'"
fi

# a rebuild from the archive (stub generator: the upstream sources are only needed for libsomrando) proves the archive is complete
mkdir -p "$W/src" && tar -xzf "$R/source/som-pc-source.tar.gz" -C "$W/src"
"$W/src/tools/companion/pc/package_release.sh" --no-generator --out "$W/rebuild" --version rb > "$W/rebuild.log" 2>&1
ok "the source archive rebuilds a working binary (stub generator)" "[ -x '$W/rebuild/som-pc-rb-linux-x86_64/som-pc' ] && '$W/rebuild/som-pc-rb-linux-x86_64/som-pc' --help 2>&1 | grep -q generate-all"

if [ -f "$ROM" ] && [ -f "$CORE" ]; then
  mkdir -p "$W/cwd"
  cd "$W/cwd" || exit 1
  export SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy
  unset SOM_PC_CONFIG SOM_PC_STATE SOM_PC_DATA SOM_PC_GEN_DIR
  "$R/som-pc" --rom "$ROM" --generate-all "$W/gen" --gen-preset rando --gen-seed 12345 > "$W/gen.json" 2>&1
  ok "packaged binary generates a preset in-process (rando, seed 12345) with the known hash" "grep -q 'b4e8ef96523b0f62e60683514e17c8190d344e03617c11d85bcf68791b26b00d' '$W/gen.json'"
  printf '{"options": {"walk_speed": 3, "qol_chest_fast": true}}\n' > "$W/opts.json"
  printf 'RUN 5\n' > "$W/s.txt"
  "$R/som-pc" --rom "$ROM" --core "$CORE" --gameplay "$W/opts.json" --script "$W/s.txt" --verbose > "$W/play.out" 2> "$W/play.err"
  ok "data found next to the executable: credits and the gameplay menu" "grep -q 'credits: $R/data/credits.json' '$W/play.err' && grep -q 'gameplay menu: $R/data/gameplay_menu.json' '$W/play.err'"
  ok "gameplay tweaks applied by the C patcher into the portable state folder" "grep -q 'gameplay tweaks applied: $R/userdata/state/gameplay/' '$W/play.err' && ls '$R'/userdata/state/gameplay/*.sfc >/dev/null 2>&1"
  ok "portable mode: config and state live in userdata/ next to the program" "[ -d '$R/userdata/state' ] && [ ! -e '$W/cwd/.config' ]"
  ok "nothing was written to the home directory by the packaged run" "[ -z \"\$(find '$HOME/.config/som-decomp-pc' -newer '$W/package.log' -type f 2>/dev/null)\" ]"
else
  echo "SKIP run checks: ROM or core missing"
fi
echo "---- $pass passed, $fail failed"
[ $fail = 0 ]
