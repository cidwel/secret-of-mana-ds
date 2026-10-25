#!/bin/bash
# Start menu test in a private X server (xdotool drives the real menu): MODE original -> "SoM Original" starts the game;
# MODE random -> "Create Randomized", Ancient Cave preset, Generate and play (runs som-dualscreen generate for real), then the generated ROM plays.
# usage: test_menu.sh original|random ROM
HERE="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
MODE="$1"; ROM="$2"
D="$HOME/.local/state/som-decomp/pc"
mkdir -p "$D/shots"
"$HERE/som-pc" --help >/dev/null 2>&1
sed "s#@SHOTS@#$D/shots/$MODE#g" "$HERE/scripts/ap_quit.txt" > "$D/menu-timeline.txt"
cat > "$D/menu-inner.sh" <<INNER
#!/bin/bash
export SOM_PC_CONFIG="$D/testcfg_menu_$MODE" SOM_PC_STATE="$D/teststate_menu_$MODE" SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE="$D/menu_audio.wav" SOM_PC_GEN_DIR="$D/gen"
mkdir -p "$D/gen"
"$HERE/som-pc" --menu --rom "$ROM" --autopilot "$D/menu-timeline.txt" 2>"$D/menu-$MODE.log" &
PID=\$!
sleep 3
W=\$(xdotool search --name "Secret of Mana" | head -1)
xdotool windowfocus \$W 2>/dev/null; sleep 0.3
xdotool key F12; sleep 0.4
if [ "$MODE" = original ]; then
  xdotool key Return                 # SoM Original
else
  xdotool key Down; sleep 0.2; xdotool key F12; sleep 0.3
  xdotool key Return; sleep 0.6; xdotool key F12; sleep 0.3          # Create Randomized -> presets
  xdotool key Down; sleep 0.2; xdotool key Return; sleep 0.6; xdotool key F12; sleep 0.3   # first Ancient Cave preset -> tweak screen
  xdotool key Return                 # Generate and play
  sleep 1.0; xdotool key F12; sleep 4.5; xdotool key F12                   # generating ... then PLAY -> game
fi
wait \$PID
echo "exit status \$?"
INNER
chmod +x "$D/menu-inner.sh"
xvfb-run -a -s "-screen 0 1920x1080x24" "$D/menu-inner.sh"
grep -E "som-pc:|in_game|shot" "$D/menu-$MODE.log" | cut -c1-200
