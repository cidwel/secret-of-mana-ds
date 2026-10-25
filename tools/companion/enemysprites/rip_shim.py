#!/usr/bin/env python3
"""Runs tools/rip_monster_anims.py on a snes9x core state instead of a ZSNES state (only the WRAM block is used).

usage: rip_shim.py ROM SNES9X_STATE OUTDIR ID[,ID...] [--only ord00] [--dirs down]
The ripper lets the game's own spawner create the monster in slot 3 of a map state and draws it with the game's own code (nothing is
emulated by hand), so a picture is the game's picture with the palette the game uploads.  Output stays outside the repository."""
import pathlib
import runpy
import sys

TOOLS = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS.parent / 'tools' / 'companion' / 'direct'))
import dcore  # noqa: E402
import zsnes_state  # noqa: E402


def load_wram(path):
    b = open(path, 'rb').read()
    off, n = dcore.blocks(b)['RAM']
    return bytearray(b[off:off + 0x20000])


zsnes_state.load_wram = load_wram
sys.argv = [str(TOOLS / 'rip_monster_anims.py')] + sys.argv[1:]
runpy.run_path(sys.argv[0], run_name='__main__')
