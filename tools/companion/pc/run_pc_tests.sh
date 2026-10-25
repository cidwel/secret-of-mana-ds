#!/bin/bash
# Builds and runs the PC frontend tests (ASan + UBSan): the logic / store / gameplay page / overlay golden hashes (test_pc, no SDL), the T12 modules
# (save state slots, display filters, in-game menu, runs list, credits text) and the SDL side with synthetic events on the dummy video and audio
# drivers (test_sdl: controllers, audio, window modes, window size, vsync).
# usage: run_pc_tests.sh [--update-hashes]     (rewrites golden_hashes.txt, golden_shader.txt and golden_menu.txt)
set -e
HERE="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
C="$HERE/.."
OUT="${SOM_PC_BUILD:-$HOME/.local/state/som-decomp/pc/build}"
WORK="$HOME/.local/state/som-decomp/pc/test-work"
TRASH="$HOME/.local/state/som-decomp/pending-delete-20261020"
UX="$C/ux/som_ui_draw.c $C/ux/som_ui_state.c $C/ux/som_ui_sel.c $C/ux/som_ui_zone.c $C/som_companion.c"
mkdir -p "$OUT" "$TRASH"
[ -d "$WORK" ] && mv "$WORK" "$TRASH/test-work-$(date +%s)-$$"
mkdir -p "$WORK"
gcc -std=c99 -Wall -Wextra -O1 -g -fsanitize=address,undefined -I"$C" "$HERE/test_pc.c" "$HERE/pc_logic.c" "$HERE/pc_store.c" "$HERE/pc_gameplay.c" "$HERE/pc_gpc.c" "$C/gameplay_c/som_gameplay.c" "$HERE/pc_shader.c" \
    $UX -lm -o "$OUT/test_pc"
cd "$HERE"
"$OUT/test_pc" "$@"
gcc -std=gnu99 -Wall -Wextra -O1 -g -fsanitize=address,undefined "$HERE/test_states.c" "$HERE/pc_states.c" "$HERE/pc_store.c" -o "$OUT/test_states"
"$OUT/test_states" "$WORK/states"
gcc -std=gnu99 -Wall -Wextra -O1 -g -fsanitize=address,undefined "$HERE/test_shader.c" "$HERE/pc_shader.c" -o "$OUT/test_shader"
"$OUT/test_shader" "$@"
gcc -std=gnu99 -Wall -Wextra -O1 -g -fsanitize=address,undefined -I"$C" "$HERE/test_pcmenu.c" "$HERE/pc_menu.c" "$HERE/pc_gameplay.c" "$HERE/pc_logic.c" "$HERE/pc_shader.c" \
    "$HERE/pc_states.c" "$HERE/pc_store.c" $UX -lm -o "$OUT/test_pcmenu"
"$OUT/test_pcmenu" "$@"
gcc -std=gnu99 -Wall -Wextra -O1 -g -fsanitize=address,undefined -I"$C" "$HERE/test_runs.c" "$HERE/pc_runs.c" "$HERE/pc_store.c" "$HERE/pc_logic.c" "$HERE/pc_shader.c" $UX -lm -o "$OUT/test_runs"
SIDECAR="$HOME/.local/state/som-decomp/companion-cli-20261017/gen/2c845ac55030ff15.sfc.json"
"$OUT/test_runs" "$WORK/runs" $([ -f "$SIDECAR" ] && echo "$SIDECAR")
gcc -std=c99 -Wall -Wextra -O1 -g -fsanitize=address,undefined "$C/launcher/test_credits.c" "$C/launcher/som_credits.c" -o "$OUT/test_credits"
"$OUT/test_credits" "$C/../../data/credits.json"
[ "$1" = "--update-hashes" ] && exit 0
gcc -std=gnu99 -Wall -Wextra -O1 -g -fsanitize=address,undefined -I"$C" $(pkg-config --cflags sdl2) "$HERE/test_sdl.c" "$HERE/pc_sdl.c" "$HERE/pc_logic.c" "$HERE/pc_shader.c" \
    $UX $(pkg-config --libs sdl2) -lm -o "$OUT/test_sdl"
# a real controller on this machine must not count as one of the test pads: only the virtual ones (and their ids) are allowed
SDL_GAMECONTROLLER_IGNORE_DEVICES_EXCEPT=0x0000/0x0000,0x1234/0x0001,0x4321/0x0002,0x2222/0x0003 \
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ASAN_OPTIONS=detect_leaks=0 timeout 120 "$OUT/test_sdl"
# generation and gameplay parity against the C# oracle and som_gameplay.py (need the owner's ROM; each prints SKIP and succeeds when something is missing)
ROM="${SOM_ROM:-/home/cidwel/projects/rommancer/roms/snes/Secret of Mana (USA).smc}"
python3 "$HERE/test_rando_parity.py" "$ROM"
python3 "$HERE/test_gpc_parity.py" "$ROM"
# distribution rule: no ROM bytes in anything built from this tree (skips without the ROM)
SOM_ROM="$ROM" python3 "$C/randomizer/libsomrando/tests/scan_rom_bytes.py" --selftest "$OUT/som-pc" "$OUT/rando/libsomrando.a"
