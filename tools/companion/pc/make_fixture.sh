#!/bin/bash
# Builds a free-play party state for a ROM by playing the new-game flow through som-pc (headless), giving the girl and the sprite
# every spirit and some MP.  usage: make_fixture.sh ROM OUT.state       (the state is local test data, never committed)
HERE="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
ROM="$1"; OUT="$2"
T="$HOME/.local/state/som-decomp/pc/fixture-script.txt"
mkdir -p "$(dirname "$T")"
{
  cat "$HERE/scripts/boot_newgame.txt"
  echo "POKE E3C4 12345678"; echo "POKE E5C4 12345678"
  echo "POKE E3C8 3F3F3F3F3F3F3F3F"; echo "POKE E5C8 3F3F3F3F3F3F3F3F"
  echo "RUN 60"
  echo "STATE fixture"
  echo "SAVESTATE $OUT"
} > "$T"
"$HERE/run_script.sh" "$ROM" "$T"
