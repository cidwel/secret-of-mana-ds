#!/usr/bin/env python3
"""T5: HUD hiding (snes9x_som_hud=hide_except_ring) on a generated Ancient Cave ROM, with the three-core lockstep check of
tools/companion/widescreen/hud_ring_check.py (A stock core, O oracle without HUD, C widescreen core): HUD leak and wrongly removed pixels.

    t5_hud.py PRESET FRAMES [MARGIN]      result JSON: $T5/out/hud_PRESET.json
"""
import os
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'widescreen'))
sys.path.insert(0, str(HERE.parent))
import hud_check  # noqa: E402
import hud_ring_check  # noqa: E402

T5 = pathlib.Path(os.environ.get('T5', os.path.expanduser('~/.local/state/som-decomp/t5-ac-20261018')))


def main():
    preset, frames = sys.argv[1], int(sys.argv[2])
    margin = sys.argv[3] if len(sys.argv) > 3 else '46'
    hud_check.CFG['t5'] = (T5 / 'roms' / (preset + '.sfc'), T5 / 'state/work' / ('ac_party_%s.state' % preset), None)
    hud_ring_check.walk(str(T5 / 'out' / ('hud_%s.json' % preset)), 't5', frames, margin)


if __name__ == '__main__':
    main()
