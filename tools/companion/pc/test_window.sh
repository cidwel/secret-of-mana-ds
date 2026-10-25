#!/bin/bash
# Real window test inside a private X server (xvfb-run): SDL window, accelerated or software renderer, audio written to a WAV file by
# SDL's disk driver (nothing is played), a virtual game controller driven by an autopilot timeline.  Prints the JSON lines and the stats.
# usage: test_window.sh ROM STATE TIMELINE [extra som-pc options]
HERE="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
ROM="$1"; ST="$2"; TL="$3"; shift 3
D="$HOME/.local/state/som-decomp/pc"
mkdir -p "$D/shots"
sed "s#@SHOTS@#$D/shots/win#g" "$TL" > "$D/timeline.txt"
"$HERE/som-pc" --help >/dev/null 2>&1   # build first, outside the X server
SOM_PC_CONFIG="${SOM_PC_CONFIG:-$D/testcfg_win}" SOM_PC_STATE="$D/teststate" SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE="$D/win_audio.wav" \
  xvfb-run -a -s "-screen 0 1920x1080x24" "$HERE/som-pc" --rom "$ROM" --state "$ST" --autopilot "$D/timeline.txt" "$@"
