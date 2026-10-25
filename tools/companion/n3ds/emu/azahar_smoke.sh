#!/bin/bash
# Scenario tests of the 3DS app in headless Azahar (screenshots in $SOM_SHOTS, files under $SOM_SD).  Run under Xvfb:
#
#   xvfb-run -a -s "-screen 0 1280x960x24" emu/azahar_smoke.sh <app.3dsx> <scenario> <tag>
#
# Needs sdmc:/som/rom.sfc (the user's ROM, never in the repository).  Scenarios:
#   menu      boot, start menu, Y settings, Credits (scroll), back
#   state     SoM Original: pause, save state to slot 3, walk, load it back (the picture must return to the saved place), Exit game
#   srm       SoM Original: pause and Exit; sdmc:/som/saves/original.srm must keep its content (write a pattern first)
#   preset    Settings: choose the Arcade preset, start SoM Original; the patched ROM copy appears in sdmc:/som/roms/patched/
#   generate  Create Randomized: first row "Surprise me", generate on the console (takes a minute in the emulator)
# The dev fixture sdmc:/som/dev/state.sav (a libretro state of the original ROM, see build_armhf_fixture.sh) makes "state" start in a field.
source "$(dirname "$0")/azahar_env.sh"
APP=${1:?app.3dsx}; SC=${2:?scenario}; TAG=${3:-$SC}
case $SC in
  menu)
    az_start "$DISPLAY" "$APP" 200; sleep 22
    az_shot "${TAG}_main"
    az_key x 0.3 2; az_shot "${TAG}_settings"
    az_key t 0.2 0.5; az_key a 0.3 1.5; az_shot "${TAG}_credits"
    for i in 1 2 3 4 5 6 7; do az_key g 0.2 0.4; done; az_shot "${TAG}_credits2"
    az_key s 0.3 1; az_key s 0.3 1.5; az_shot "${TAG}_back" ;;
  state)
    az_start "$DISPLAY" "$APP" 300; sleep 22
    az_key a 0.3 40; az_shot "${TAG}_game"
    az_chord; az_key g 0.2 0.4; az_key h 0.2 0.4; az_key h 0.2 0.4; az_key g 0.2 0.4; az_key a 0.3 1.2; az_key a 0.3 2.5   # slot 3, save (an occupied slot asks for a second A; on an empty slot the second A is a harmless press in the game)
    xdotool keydown h; sleep 1.5; xdotool keyup h; sleep 0.5; az_shot "${TAG}_moved"                       # walk right for a moment
    az_chord; az_key g 0.2 0.4; az_key g 0.2 0.4; az_key g 0.2 0.4; az_key a 0.3 3; az_shot "${TAG}_loaded"   # Load state
    az_chord; az_key t 0.2 0.4; az_key a 0.3 3 ;;
  srm)
    az_start "$DISPLAY" "$APP" 200; sleep 22
    az_key a 0.3 40; az_chord; az_key t 0.2 0.4; az_key a 0.3 3 ;;
  preset)
    az_start "$DISPLAY" "$APP" 300; sleep 22
    az_key x 0.3 2; az_key g 0.2 0.5; az_key h 0.2 0.5; az_key h 0.2 0.5; az_shot "${TAG}_preset"; az_key s 0.3 2
    az_key a 0.3 40; az_shot "${TAG}_game"; az_chord; az_shot "${TAG}_pause"; az_key t 0.2 0.4; az_key a 0.3 3 ;;
  generate)
    az_start "$DISPLAY" "$APP" 400; sleep 22
    az_key g 0.2 0.6; az_key a 0.3 1.5; az_key a 0.3 1.5; az_key m 0.3 1.5; az_shot "${TAG}_start"
    sleep 20; az_shot "${TAG}_running"; sleep 45; az_shot "${TAG}_done" ;;
  *) echo "unknown scenario"; exit 2 ;;
esac
