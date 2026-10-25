#!/bin/bash
# Thor regression: SoM Original -> title -> GAME SELECT -> name entry typed on the bottom keyboard -> START -> opening.
# Fails when the name is wrong or the game stays on map 8 (the black screen after START that an injected UI sound used to cause).
# usage: name_start_device.sh [touch|pad]    START by the bottom panel button (default) or by a pad event on /dev/input/event9.
A="adb -s 5d1ee37e"; PKG=org.somdecomp.companion; MODE=${1:-touch}
tap() { $A shell "input -d 4 tap $1 $2"; }
$A shell am force-stop $PKG; $A shell am start -n $PKG/.LaunchActivity >/dev/null; sleep 4
tap 600 260; sleep 24                                   # SoM Original, logos
$A shell "sendevent /dev/input/event9 1 315 1; sendevent /dev/input/event9 0 0 0; sleep 0.12; sendevent /dev/input/event9 1 315 0; sendevent /dev/input/event9 0 0 0"; sleep 4   # skip the intro
tap 1010 510; sleep 3                                   # GAME SELECT: OK (B) -> name entry
$A logcat -c
for xy in "342 475" "1157 285" "615 475" "205 475"; do tap $xy; sleep 0.4; done   # L I N K
sleep 9
if [ "$MODE" = pad ]; then $A shell "sendevent /dev/input/event9 1 315 1; sendevent /dev/input/event9 0 0 0; sleep 0.12; sendevent /dev/input/event9 1 315 0; sendevent /dev/input/event9 0 0 0"; else tap 1058 860; fi
sleep 25
entered=$($A logcat -d | grep -c "name entry: . entered")
last=$($A logcat -d | grep "core frame" | tail -1 | sed 's/.*map \([0-9]*\) .*/\1/')
silent=$($A logcat -d | grep "audio rms" | tail -1 | sed 's/.*silent-run-max \([0-9]*\).*/\1/')
echo "letters entered $entered of 4, map $last, audio silent run $silent frames"
[ "$entered" = 4 ] && [ "$last" != 8 ] && [ "${silent:-0}" -lt 600 ] && echo PASS || { echo FAIL; exit 1; }
