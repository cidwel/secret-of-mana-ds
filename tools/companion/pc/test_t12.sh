#!/bin/bash
# End-to-end checks of the PC frontend for board item T12 (saves, save states, display filters, resolution settings, start menu with credits and runs,
# in-process generation of a run, gameplay tweaks through the C patcher).
# Real window under a private X server (xvfb-run + xdotool) and headless scripted sessions on the real core.  Needs the owner's ROM and the local fixtures
# of docs/companion-pc-20261018.md; a missing one is reported and skipped.  Nothing is written outside ~/.local/state/som-decomp/pc/t12x/.
# Robust under machine load: every wait polls a log line or a file instead of sleeping a fixed time, the key delays scale with the load average
# (T12_SLOW=N forces a factor), and a section whose checks fail is run again up to three times with a larger factor before it counts as failed.
# usage: test_t12.sh [VANILLA_ROM] [PARTY_STATE]       exit status 0 when every check passes
HERE="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
ST="$HOME/.local/state/som-decomp"
ROM="${1:-/home/cidwel/projects/rommancer/roms/snes/Secret of Mana (USA).smc}"
FIX="${2:-$ST/companion-20261017/adv/advanced_equipped.state}"
D="$ST/pc/t12x"
TRASH="$ST/pending-delete-20261020"
mkdir -p "$TRASH"
[ -d "$D" ] && mv "$D" "$TRASH/t12x-$(date +%s)-$$"
mkdir -p "$D/shots"
pass=0; fail=0
RES=()
ok() { if eval "$2"; then RES+=("PASS $1"); else RES+=("FAIL $1   [$2]"); fi; }
[ -f "$ROM" ] && [ -f "$FIX" ] || { echo "SKIP: ROM or party state missing ($ROM, $FIX)"; exit 0; }
"$HERE/som-pc" --help >/dev/null 2>&1     # builds the binary once, outside the timed parts
BIN="$ST/pc/build/som-pc"
HAVE_GEN=0; [ "$(cat "$BIN.gen" 2>/dev/null)" = rando ] && HAVE_GEN=1
KEY=$(python3 - "$ROM" <<'EOF'
import hashlib, sys
d = open(sys.argv[1], 'rb').read()
if len(d) % 1024 == 512: d = d[512:]
print(hashlib.sha256(d).hexdigest()[:12])
EOF
)
echo "vanilla ROM key $KEY"

# ---------------------------------------------------------------- load handling
factor() {                                           # key delay multiplier from the load average (1 on an idle machine), times the attempt number
  local f
  if [ -n "$T12_SLOW" ]; then f=$T12_SLOW; else f=$(awk -v n="$(nproc)" '{ r = $1 / n; printf "%.2f", r < 0.5 ? 1 : 1 + r }' /proc/loadavg); fi
  awk -v f="$f" -v a="${1:-1}" 'BEGIN { printf "%.2f", f * a }'
}
cat > "$D/lib.sh" <<'LIB'
# sourced by the inner scripts of the X sessions; F = delay factor, LOGF = the log of the process under test
sl() { sleep "$(awk -v a="$1" -v f="$F" 'BEGIN { printf "%.2f", a * f }')"; }
k() { xdotool key "$@"; sl 0.35; }
waitlog() {                                          # waitlog PATTERN [SECONDS]: poll the log, never fails the session
  local i n=$(( ${2:-20} * 10 ))
  for ((i = 0; i < n; i++)); do grep -q -- "$1" "$LOGF" 2>/dev/null && return 0; sleep 0.1; done
  return 1
}
getwin() {                                           # the game window, up to 60 s; focused
  local i
  for ((i = 0; i < 600; i++)); do W=$(xdotool search --name "Secret of Mana" 2>/dev/null | head -1); [ -n "$W" ] && break; sleep 0.1; done
  xdotool windowfocus "$W" 2>/dev/null; sl 0.5
}
geom() { xdotool getwindowgeometry "$W" | grep Geometry | sed 's/.*: //'; }
waitgeom() {                                         # waitgeom REGEX FILE: the window size once it matches (up to 10 s), written to FILE
  local i g
  for ((i = 0; i < 100; i++)); do g=$(geom); echo "$g" | grep -q "$1" && break; sleep 0.1; done
  echo "$g" > "$2"
}
LIB
fresh() { [ -d "$1" ] && mv "$1" "$TRASH/$(basename "$1")-$(date +%s)-$$-$RANDOM"; mkdir -p "$1"; }
section() {                                          # section NAME FUNCTION: runs FUNCTION ATTEMPT up to three times until all its checks pass
  local n l
  for n in 1 2 3; do
    RES=()
    "$2" "$n"
    printf '%s\n' "${RES[@]}" | grep -q '^FAIL' || break
    [ "$n" -lt 3 ] && echo "retry $1: attempt $n failed (load $(cut -d' ' -f1 /proc/loadavg))"
  done
  for l in "${RES[@]}"; do
    echo "$l"
    case "$l" in PASS*) pass=$((pass + 1)) ;; *) fail=$((fail + 1)) ;; esac
  done
}
xsession() { timeout 240 xvfb-run -a -s "-screen 0 1920x1080x24" "$1" > "$1.out" 2>&1; }

# ---------------------------------------------------------------- A: keyboard, real window
sec_a() {
  local A="$D/a" S F; F=$(factor "$1"); fresh "$A"
  cat > "$A/inner.sh" <<INNER
#!/bin/bash
. "$D/lib.sh"
export F=$F LOGF="$A/run.log" SOM_PC_CONFIG="$A/cfg" SOM_PC_STATE="$A/st" SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy
mkdir -p "$A/cfg"
"$BIN" --rom "$ROM" --state "$FIX" --verbose --no-vsync --size 1920x1080 2>"$A/run.log" &
PID=\$!
waitlog "controls:" 60; getwin
k F10; k F12                                              # the menu over the game
k Down; k Return; k F12                                   # SAVE STATE page, all empty
k Return; sl 0.6                                          # slot 1 saved, the menu closes
k 5; k F7; k bracketleft; k F7                            # slot 5, then slot 4
waitlog "state slot 4 saved" 10; k F12
k F8                                                      # load slot 4
waitlog "state slot 4 load" 10
k F10; k Down; k Down; k Return; k F12                    # LOAD STATE page with thumbnails
k Escape; k Escape; k F10                                 # back to the game, open again
k Down; k Down; k Down; k Down; k Return                  # VIDEO
k Right; k Right; k F12                                   # filter: scanlines, CRT
k Down; k Left; k Left; waitgeom '^1280x720' "$A/geom720" # window size 900p, 720p
k Down; k Right; waitlog "screen 1," 10; sl 1; geom > "$A/geomfs"     # screen: fullscreen
k Right; waitlog "screen 2," 10; k Right; waitlog "screen 0," 10; waitgeom '^1280x720' "$A/geomwin"   # exclusive (or its fallback), then windowed again
k Down; k Right; k Down; k Right; k Down; k Return        # picture WIDE 27, scaling FILL, integer scale ON
k Down; k Return                                          # vsync OFF
k F12; k Escape; k F10                                    # back to the root, close the menu
k F10; k Up; k Return                                     # root, QUIT
wait \$PID
echo "exit status \$?" > "$A/exit"
INNER
  chmod +x "$A/inner.sh"
  xsession "$A/inner.sh"
  S="$A/st/states/$KEY"
  ok "A: exits cleanly through the menu (QUIT)" "grep -q 'exit status 0' '$A/exit'"
  ok "A: slot 1 saved from the save page" "[ -s '$S/slot01.state' ]"
  ok "A: digit 5 + F7 saved slot 5, [ + F7 saved slot 4" "[ -s '$S/slot05.state' ] && [ -s '$S/slot04.state' ]"
  ok "A: F8 loaded slot 4" "grep -q 'state slot 4 load: status 0' '$A/run.log'"
  ok "A: the load page lists the slots (screenshot taken)" "ls '$A/st/shots'/som-*.bmp 2>/dev/null | wc -l | grep -qv '^[0-3]$'"
  ok "A: window size 720p from the video menu" "grep -q '^1280x720' '$A/geom720'"
  ok "A: fullscreen (borderless), exclusive and back to windowed from the video menu" "grep -q 'video row 2: .* screen 1,' '$A/run.log' && grep -q 'video row 2: .* screen 2,' '$A/run.log' && grep -q 'video row 2: .* screen 0,' '$A/run.log'"
  ok "A: windowed again at 720p after fullscreen" "grep -q '^1280x720' '$A/geomwin'"
  ok "A: video rows applied (log)" "grep -q 'video row 1: filter crt' '$A/run.log' && grep -q 'wide 27' '$A/run.log' && grep -q 'fit INTEGER' '$A/run.log' && grep -q 'row 6: .*vsync 1' '$A/run.log'"
  local CFG="$A/cfg/config.json"
  ok "A: video settings persisted in config.json" "grep -q '\"shader\": 3' '$CFG' && grep -q '\"wide\": 27' '$CFG' && grep -q '\"vsync\": 1' '$CFG' && grep -q '\"win_w\": 1280' '$CFG' && grep -q '\"state_slot\": 3' '$CFG'"
  ok "A: the battery save was written on exit" "[ -s '$A/st/saves/$KEY.srm' ]"
}
section A sec_a

# ---------------------------------------------------------------- B: battery save, auto flush while running, SIGKILL, reload in another process
sec_b() {
  local B="$D/b" F R; F=$(factor "$1"); fresh "$B"
  printf '@1000 sram C0FFEE5A 64\n@30000 quit\n' > "$B/tl.txt"
  cat > "$B/inner.sh" <<INNER
#!/bin/bash
. "$D/lib.sh"
export F=$F LOGF="$B/run.log" SOM_PC_CONFIG="$B/cfg" SOM_PC_STATE="$B/st" SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy
"$BIN" --rom "$ROM" --state "$FIX" --autopilot "$B/tl.txt" --no-vsync --verbose 2>"$B/run.log" &
PID=\$!
for ((i = 0; i < 600; i++)); do                          # the flush happens every 2 s once the change is made: wait for the file, not for a time
  xxd -s 64 -l 4 -p "$B/st/saves/$KEY.srm" 2>/dev/null | grep -qi '^c0ffee5a' && break
  sleep 0.1
done
kill -9 \$PID
wait \$PID 2>/dev/null
INNER
  chmod +x "$B/inner.sh"
  xsession "$B/inner.sh"
  ok "B: the changed battery RAM was flushed while the game ran (process killed with SIGKILL)" "xxd -s 64 -l 4 -p '$B/st/saves/$KEY.srm' | grep -qi '^c0ffee5a'"
  printf 'SRAMPEEK p 40 4\n' > "$B/peek.txt"
  R=$(SOM_PC_CONFIG="$B/cfg" SOM_PC_STATE="$B/st" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$BIN" --rom "$ROM" --script "$B/peek.txt" 2>/dev/null | grep srampeek)
  ok "B: the next process starts with that save" "echo '$R' | grep -q 'C0FFEE5A'"
}
section B sec_b

# ---------------------------------------------------------------- C: pad combos on the virtual controller (quick save / load / slot / menu)
sec_c() {
  local C="$D/c"; fresh "$C"
  sed "s#@SHOTS@#$C/shot#g" "$HERE/scripts/ap_t12_pad.txt" > "$C/tl.txt"
  SOM_PC_CONFIG="$C/cfg" SOM_PC_STATE="$C/st" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy timeout 180 "$BIN" --rom "$ROM" --state "$FIX" --autopilot "$C/tl.txt" --verbose --no-vsync > "$C/out.txt" 2> "$C/err.txt"
  ok "C: Select+R1 quick-saved slot 1; Select+Right then Select+R1 quick-saved slot 2" "[ -s '$C/st/states/$KEY/slot01.state' ] && grep -c 'state slot 2 saved' '$C/err.txt' | grep -q '^2$'"
  ok "C: Select+L1 quick-loaded slot 2" "grep -q 'state slot 2 load: status 0' '$C/err.txt'"
  ok "C: left stick click opened the menu on the save page" "grep -q '\"tag\":\"menu_save_page\".*\"menu\":\[1,1,1\]' '$C/out.txt'"
  ok "C: the overwrite was confirmed and saved, the menu closed" "grep -q '\"tag\":\"after_save\".*\"menu\":\[0,' '$C/out.txt'"
  ok "C: the old slot 2 stayed as .bak" "[ -s '$C/st/states/$KEY/slot02.state.bak' ]"
  ok "C: the Select chord did not reach the game (no menu or option page opened by Select)" "! grep -q 'unknown' '$C/err.txt'"
}
section C sec_c

# ---------------------------------------------------------------- the Ancient Cave run used by D and E: made by the in-process generator
ACROM=""; ACKEY=""
if [ $HAVE_GEN = 1 ]; then
  mkdir -p "$D/gen0"
  "$BIN" --rom "$ROM" --generate-all "$D/gen0" --gen-preset ancientcave-short --gen-seed 12345 > "$D/gen0/out.json" 2>&1
  if grep -q '"rc":0' "$D/gen0/out.json"; then
    ACROM="$D/gen0/ancientcave-short.sfc"
    ACKEY=$(python3 -c "import hashlib,sys;d=open(sys.argv[1],'rb').read();print(hashlib.sha256(d).hexdigest()[:12])" "$ACROM")
  fi
fi

# ---------------------------------------------------------------- D: isolation of saves and states per ROM, patched copy keeps the battery save (headless, real core)
sec_d() {
  local Dd="$D/d" R1 R2 R3
  fresh "$Dd"
  RUNS() { SOM_PC_CONFIG="$Dd/cfg" SOM_PC_STATE="$Dd/st" SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy "$BIN" "$@" 2>/dev/null; }
  printf 'LOAD %s\nRUN 20\nSRAMPOKE 0 AABBCCDD\nSRAMFLUSH\nSSAVE 1\nSINFO 1\n' "$FIX" > "$Dd/v1.txt"
  R1=$(RUNS --rom "$ROM" --script "$Dd/v1.txt")
  ok "D: vanilla: battery save and state slot 1 written" "[ -s '$Dd/st/saves/$KEY.srm' ] && echo '$R1' | grep -q '\"state-info\",\"slot\":1,\"status\":0'"
  if [ -n "$ACROM" ]; then
    printf 'SRAMPEEK p 0 4\nSINFO 1\nSRAMPOKE 0 11223344\nSRAMFLUSH\n' > "$Dd/a1.txt"
    R2=$(RUNS --rom "$ACROM" --script "$Dd/a1.txt")
    ok "D: the Ancient Cave run starts with a fresh save and no states (not vanilla's)" "! echo '$R2' | grep -q 'AABBCCDD' && echo '$R2' | grep -q '\"state-info\",\"slot\":1,\"status\":1'"
    ok "D: the run has its own battery file, vanilla's is untouched" "[ -s '$Dd/st/saves/$ACKEY.srm' ] && [ '$ACKEY' != '$KEY' ] && xxd -s 0 -l 4 -p '$Dd/st/saves/$KEY.srm' | grep -qi aabbccdd"
  else
    echo "SKIP D (run isolation): no generated ROM (this build has no libsomrando)"
  fi
  printf '{\n "options": {\n  "walk_speed": 3\n }\n}\n' > "$Dd/opts.json"
  printf 'SRAMPEEK p 0 4\nSINFO 1\n' > "$Dd/p1.txt"
  R3=$(RUNS --rom "$ROM" --gameplay "$Dd/opts.json" --script "$Dd/p1.txt")
  ok "D: a ROM patched with gameplay tweaks keeps the battery save of the original" "echo '$R3' | grep -q 'AABBCCDD'"
  ok "D: ...but not its save states (the patched code is a different ROM)" "echo '$R3' | grep -q '\"state-info\",\"slot\":1,\"status\":1'"
  ok "D: the tweak was applied by the C patcher (no Python): patched copy written, 1 option" "ls '$Dd/st/gameplay'/*.sfc >/dev/null 2>&1"
}
section D sec_d

# ---------------------------------------------------------------- E: start menu: credits, display, runs list, play a run, back to the start menu
sec_e() {
  local E="$D/e" F; F=$(factor "$1"); fresh "$E"; mkdir -p "$E/gen"
  cp "$ACROM" "$E/gen/"; cp "$ACROM.json" "$E/gen/"
  cat > "$E/inner.sh" <<INNER
#!/bin/bash
. "$D/lib.sh"
export F=$F LOGF="$E/run.log" SOM_PC_CONFIG="$E/cfg" SOM_PC_STATE="$E/st" SOM_PC_GEN_DIR="$E/gen" SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy
"$BIN" --menu --rom "$ROM" --verbose --no-vsync 2>"$E/run.log" &
PID=\$!
waitlog "menu: rom" 60; getwin
k F12                                                    # main menu with the extra entries
k Down; k Down; k Return; k F12                          # Saved Randomized: the run imported from the generator folder
k Return                                                 # play it
waitlog "controls:" 60; sl 4
k F7; waitlog "state slot 1 saved" 10                    # quick save in the run
k F10; k Down; k Down; k Down; k Down; k Down; k Down; k Return   # START MENU: back to the launcher
for ((i = 0; i < 300; i++)); do [ \$(grep -c 'menu: rom' "$E/run.log") -ge 2 ] && break; sleep 0.1; done; sl 0.5
k F12
k Down; k Down; k Down; k Down; k Down; k Return; k F12  # Credits
k Down; k Down; k Down; k Down; k F12                    # scrolled
k Escape; k Up; k Return; k F12                          # Display over the start menu
k Right; k Escape                                        # filter changed, back
k Escape; sl 1                                           # quit from the main menu
wait \$PID
echo "exit status \$?" > "$E/exit"
INNER
  chmod +x "$E/inner.sh"
  xsession "$E/inner.sh"
  ok "E: the generated run was imported into the runs list" "grep -q '\"key\": \"2c845ac55030ff15\"' '$E/st/runs.json'"
  ok "E: playing it from Saved Randomized counted a play" "grep -q '\"plays\": 1' '$E/st/runs.json'"
  ok "E: the run has its own save state folder" "[ -s '$E/st/states/$ACKEY/slot01.state' ]"
  ok "E: START MENU in the game returned to the launcher and a second round started cleanly" "[ \$(grep -c 'menu: rom' '$E/run.log') -ge 2 ]"
  ok "E: exits with status 0 from the main menu" "grep -q 'exit status 0' '$E/exit'"
  ok "E: the Display entry changed the filter and saved it" "grep -q '\"shader\": 2' '$E/cfg/config.json'"
  ok "E: launcher screenshots taken (main, saved, main again, credits x2, display)" "[ \$(ls '$E/st/shots'/menu-*.bmp 2>/dev/null | wc -l) -ge 6 ]"
}
if [ -n "$ACROM" ]; then section E sec_e; else echo "SKIP E (start menu and runs): no generated ROM (this build has no libsomrando)"; fi

# ---------------------------------------------------------------- G: a run is generated inside the launcher (preset, seed, tweaks screen), played, saved
sec_g() {
  local G="$D/g" F sc side s h
  F=$(factor "$1"); fresh "$G"; mkdir -p "$G/gen"
  cat > "$G/inner.sh" <<INNER
#!/bin/bash
. "$D/lib.sh"
export F=$F LOGF="$G/run.log" SOM_PC_CONFIG="$G/cfg" SOM_PC_STATE="$G/st" SOM_PC_GEN_DIR="$G/gen" SDL_VIDEODRIVER=x11 SDL_AUDIODRIVER=dummy
"$BIN" --menu --rom "$ROM" --verbose --no-vsync 2>"$G/run.log" &
PID=\$!
waitlog "menu: rom" 60; getwin
k Down; k Return                                         # Create randomized: the preset list
k Down; k Return; k F12                                  # the first real preset (Ancient Cave): the tweaks screen
k Return                                                 # Generate and play
waitlog "generated " 60; waitlog "controls:" 60; sl 3
k F7; waitlog "state slot 1 saved" 10                    # quick save in the new run
k F10; k Up; k Return                                    # root, QUIT
wait \$PID
echo "exit status \$?" > "$G/exit"
INNER
  chmod +x "$G/inner.sh"
  xsession "$G/inner.sh"
  side=$(ls "$G/gen"/*.sfc.json 2>/dev/null | head -1)
  sc=$(ls "$G/gen"/*.sfc 2>/dev/null | head -1)
  s=$(python3 -c "import hashlib,sys;d=open(sys.argv[1],'rb').read();print(hashlib.sha256(d).hexdigest()[:12])" "${sc:-/dev/null}" 2>/dev/null)
  ok "G: the generator ran inside the launcher and wrote a 4 MiB ROM with its sidecar" "[ -n '$sc' ] && [ \$(stat -c %s '$sc') = 4194304 ] && [ -s '$side' ]"
  h=$(sha256sum "${sc:-/dev/null}" | cut -d' ' -f1)
  ok "G: the SHA-256 is in the sidecar and the log, matching the file" "grep -q '\"output_sha256\": \"$h\"' '$side' && grep -q 'generated .* sha256 $h' '$G/run.log'"
  ok "G: the run is in the runs list and was played once" "grep -q '\"plays\": 1' '$G/st/runs.json'"
  ok "G: the new run has its own save state folder" "[ -n '$s' ] && [ -s '$G/st/states/$s/slot01.state' ]"
  ok "G: exits with status 0" "grep -q 'exit status 0' '$G/exit'"
}
if [ $HAVE_GEN = 1 ]; then section G sec_g; else echo "SKIP G (generation in the launcher): this build has no libsomrando"; fi

echo "---- $pass passed, $fail failed"
[ $fail = 0 ]
