# Companion UI assets

| Path | Contents |
| --- | --- |
| `ui-src/` | Game font sheets, item and spell icons, and the three hero portraits. `tools/companion/ux/build_assets.py` turns them into the graphics pack `som_ui_pack.bin` |
| `fonts/IBMPlexSansCondensed-SemiBold.ttf` | Menu face (SIL OFL 1.1), rasterised into the same pack |
| `som_bg.jpg` | Start-menu wallpaper of the Thor app |
| `icon/` | App icon: the rabite pixel art (`rabite.png`) and `make_icons.py`, which writes the Android mipmaps, the 3DS icon and the PC `.ico`/`.png` |

`python3 build.py thor` builds the pack (needs Pillow and numpy) and puts it, with the wallpaper, in the APK. Without the pack the renderer falls back to its built-in 5x7 font and placeholder shapes.

Pack format: `SUXP` v1, described at the top of `build_assets.py`.
