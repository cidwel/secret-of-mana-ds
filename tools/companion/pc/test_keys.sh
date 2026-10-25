#!/bin/bash
# Keyboard test in a private X server: xdotool types the hotkeys of the real window (fit, wide, hold-to-assign, picker, fullscreen, screenshot, quit).
# usage: test_keys.sh ROM STATE
HERE="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
ROM="$1"; ST="$2"
D="$HOME/.local/state/som-decomp/pc"
mkdir -p "$D/shots"
"$HERE/som-pc" --help >/dev/null 2>&1
cat > "$D/keys-inner.sh" <<INNER
#!/bin/bash
export SOM_PC_CONFIG="$D/testcfg_keys" SOM_PC_STATE="$D/teststate_keys" SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=disk SDL_DISKAUDIOFILE="$D/keys_audio.wav"
"$HERE/som-pc" --rom "$ROM" --state "$ST" --verbose 2>"$D/keys.log" &
PID=\$!
sleep 4
W=\$(xdotool search --name "Secret of Mana" | head -1)
xdotool windowfocus \$W 2>/dev/null; sleep 0.3
xdotool key F12; sleep 0.5                         # screenshot: default wide 46, FIT
xdotool key F3; sleep 0.3; xdotool key F12; sleep 0.5   # FILL
xdotool key F3; sleep 0.3; xdotool key F12; sleep 0.5   # INTEGER
xdotool key F3; sleep 0.3; xdotool key F3; sleep 0.3    # back to FIT
xdotool key F4; sleep 0.8; xdotool key F12; sleep 0.5   # wide OFF
xdotool key F4; sleep 0.3; xdotool key F4; sleep 0.8; xdotool key F12; sleep 0.5   # wide 27
xdotool key F4; sleep 0.5                                # 46 again
xdotool key F5; sleep 0.3; xdotool key F12; sleep 0.5    # overlay medium
xdotool key F5; sleep 0.3; xdotool key F5; sleep 0.3     # hidden, then small again
xdotool keydown q; sleep 1.1; xdotool keyup q; sleep 0.6  # hold Q: the picker opens on L1
xdotool key F12; sleep 0.5
xdotool key Escape; sleep 0.5                            # back closes the picker
xdotool key F11; sleep 1.5; xdotool key F12; sleep 0.5; xdotool key F11; sleep 1   # fullscreen and back
xdotool key Escape; sleep 0.4; xdotool key Up; xdotool key Return   # Escape opens the menu, Up wraps to QUIT
wait \$PID
echo "exit status \$?"
INNER
chmod +x "$D/keys-inner.sh"
xvfb-run -a -s "-screen 0 1920x1080x24" "$D/keys-inner.sh"
grep -E "stats|renderer|screenshot|som-pc:" "$D/keys.log" | cut -c1-250
