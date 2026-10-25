# Source this file.  Headless Azahar (PC) for the 3DS frontend; everything lives under $SOM_AZ (default ~/.local/state/som-decomp/azahar-20261018).
#
#   one time:  mkdir -p $SOM_AZ/dl $SOM_AZ/ex; curl -L -o $SOM_AZ/dl/azahar.AppImage \
#                https://github.com/azahar-emu/azahar/releases/download/2126.2/azahar.AppImage     # sha256 af037f8b775198f636b14bcfe63fb401b02246ebfefe1025bb905ce042556960
#              chmod +x $SOM_AZ/dl/azahar.AppImage; (cd $SOM_AZ/ex && ../dl/azahar.AppImage --appimage-extract)
#   per test:  xvfb-run -a -s "-screen 0 1280x960x24" <script that sources this file and calls az_start ... >
#
# ROM-derived files (the user's ROM, save-state fixtures, art pack, screenshots) stay under $SOM_AZ, never in the repository.
SOM_AZ=${SOM_AZ:-$HOME/.local/state/som-decomp/azahar-20261018}
SOM_SD=$SOM_AZ/xdg/data/azahar-emu/sdmc            # sdmc:/ of the emulated console
SOM_SHOTS=$SOM_AZ/shots
mkdir -p "$SOM_SD/som" "$SOM_SHOTS" "$SOM_AZ/xdg/config" "$SOM_AZ/xdg/cache" "$SOM_AZ/run"

# az_start <display> <file.3dsx> [seconds]: runs Azahar in the background and ends it by itself after [seconds] (default 600).
az_start() {
  ( XDG_DATA_HOME=$SOM_AZ/xdg/data XDG_CONFIG_HOME=$SOM_AZ/xdg/config XDG_CACHE_HOME=$SOM_AZ/xdg/cache QT_QPA_PLATFORM=xcb DISPLAY=$1 \
    LIBGL_ALWAYS_SOFTWARE=1 timeout "${3:-600}" "$SOM_AZ/ex/squashfs-root/AppRun" "$2" > "$SOM_AZ/run/azahar.log" 2>&1 & )
}

# Default Azahar keys (Qt key codes): A=a B=s X=z Y=x L=q R=w ZL=1 ZR=2 Start=m Select=n D-pad t/g/f/h.  The default layout shows the top
# screen scaled into 320 x 240 above the bottom screen at 1:1: bottom screen = 320 x 240 at (BX,BY) in the Xvfb root.
BX=480; BY=478
az_key()  { xdotool keydown "$1"; sleep "${2:-0.25}"; xdotool keyup "$1"; sleep "${3:-0.3}"; }
az_tap()  { xdotool mousemove $((BX + $1)) $((BY + $2)); sleep 0.1; xdotool mousedown 1; sleep "${3:-0.3}"; xdotool mouseup 1; sleep "${4:-0.4}"; }   # a tap shorter than one emulated frame is missed
az_hold() { xdotool mousemove $((BX + $1)) $((BY + $2)); sleep 0.1; xdotool mousedown 1; sleep "$3"; xdotool mouseup 1; sleep 0.5; }               # >= 0.5 s is a long press
az_shot() { import -window root "$SOM_SHOTS/$1_full.png"; convert "$SOM_SHOTS/$1_full.png" -crop 320x240+$BX+$BY +repage "$SOM_SHOTS/$1_bottom.png"; }
az_perf() { cat "$SOM_SD/som/perf.txt" 2>/dev/null; }   # written by the app once a second: ms/frame, core, bottom UI, top scaling, UI redraws
# is_new_3ds is rewritten by Azahar when it closes: edit $SOM_AZ/xdg/config/azahar-emu/qt-config.ini only while it is not running.
az_chord() { xdotool keydown n; sleep 0.2; xdotool keydown m; sleep 0.3; xdotool keyup m; xdotool keyup n; sleep "${1:-1.0}"; }   # Select+Start: the pause menu
# Never stop Azahar by name (pkill -f azahar also kills other people's emulators): az_start ends it by itself after its timeout.
