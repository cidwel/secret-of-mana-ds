#!/bin/bash
# Gameplay tweaks page in the real start menu (private X server, xdotool): the saved options show as ACTIVE, G opens the page, a value is changed
# and saved, "SoM Original" starts the game and the ROM that runs is the patched copy (tools/gameplay/som_gameplay.py apply), not the original.
# usage: test_gameplay.sh ROM
HERE="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
ROM="$1"
D="$HOME/.local/state/som-decomp/pc/gp_test"
mkdir -p "$D/cfg" "$D/state"
mv "$D/state/gameplay" "$D/state/gameplay.old.$$" 2>/dev/null || true
printf '{\n "options": {\n  "walk_speed": 3\n }\n}\n' > "$D/cfg/gameplay.json"
"$HERE/som-pc" --help >/dev/null 2>&1
sed "s#@SHOTS@#$D/state/shots/gp#g" "$HERE/scripts/ap_quit.txt" > "$D/timeline.txt"
cat > "$D/inner.sh" <<INNER
#!/bin/bash
export SOM_PC_CONFIG="$D/cfg" SOM_PC_STATE="$D/state" SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy
"$HERE/som-pc" --menu --rom '$ROM' --autopilot "$D/timeline.txt" 2>"$D/run.log" &
PID=\$!
sleep 3
W=\$(xdotool search --name "Secret of Mana" | head -1)
xdotool windowfocus \$W 2>/dev/null; sleep 0.3
xdotool key F12; sleep 0.4                       # menu with the ACTIVE hint
xdotool key g; sleep 0.4; xdotool key F12; sleep 0.4      # the page
xdotool key Down; xdotool key Down; xdotool key Right; sleep 0.2     # third option: next value
xdotool key F12; sleep 0.4
xdotool key Escape; sleep 0.4                    # back to the menu, options saved
xdotool key Return                               # SoM Original
sleep 6; xdotool key F12; sleep 0.5; xdotool key Escape; sleep 0.4; xdotool key Up; xdotool key Return     # in game: shot, then the menu and QUIT
wait \$PID
echo "exit status \$?"
INNER
chmod +x "$D/inner.sh"
timeout 90 xvfb-run -a -s "-screen 0 1920x1080x24" "$D/inner.sh"
grep -E "som-pc:" "$D/run.log" | grep -E "gameplay|screenshot|ROM" | cut -c1-200
cat "$D/cfg/gameplay.json"
