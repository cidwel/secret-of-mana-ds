#!/usr/bin/env python3
"""T5: how often the widescreen core widens on the maps of a generated Ancient Cave ROM.

For each warp index 0..N-1 (the game's own warp event on the free-play state) the core runs 300 frames with margin 46, 27 and 17 and the
settled frame width is recorded.  The core falls back to 256 on Mode 7 maps and on maps whose layers cannot be rebuilt (see
docs/companion-widescreen-core-20261017.md), so a share of 256 is expected; the numbers are compared with the vanilla ROM (285 of 397 maps wide at 27 px).

    t5_wide_maps.py PRESET [N]
"""
import json
import os
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'widescreen'))
sys.path.insert(0, str(HERE.parent))
import wscore  # noqa: E402

T5 = pathlib.Path(os.environ.get('T5', os.path.expanduser('~/.local/state/som-decomp/t5-ac-20261018')))
core = T5 / 'core/snes9x_widescreen_hires_libretro.so'


def main():
    preset = sys.argv[1]
    n_max = int(sys.argv[2]) if len(sys.argv) > 2 else 40
    st = (T5 / 'state/work' / ('ac_party_%s.state' % preset)).read_bytes()
    c = wscore.WS(str(T5 / 'roms' / (preset + '.sfc')), core, {'snes9x_som_widescreen': '46', 'snes9x_som_hires': 'native', 'snes9x_som_hud': 'off'})
    res = {}
    for n in range(n_max):
        row = {}
        for m in ('46', '27', '17'):
            c.set_option('snes9x_som_widescreen', m)
            c.load_state(st)
            c.run(2)
            c.write(0x1FF00, bytes([0x18 | (n >> 8), n & 0xFF, 8]))
            c.write(0xD1, b'\x00\xff\x7f')
            c.run(1)
            c.write(0xD0, b'\x01')
            c.run(300)
            row[m] = c.width
            row['map'] = c.u8(0xDC) | c.u8(0xDD) << 8
            row['d0'] = c.u8(0xD0)
        res[n] = row
    done = {n: r for n, r in res.items() if r['d0'] == 0}
    summ = {m: dict(wide=sum(1 for r in done.values() if r[m] > 256 and r[m] != 512), narrow=sum(1 for r in done.values() if r[m] == 256), other=sum(1 for r in done.values() if r[m] == 512)) for m in ('46', '27', '17')}
    out = dict(preset=preset, warps_tried=n_max, warps_finished=len(done), summary=summ, rows=res)
    (T5 / 'out' / ('wide_maps_%s.json' % preset)).write_text(json.dumps(out))
    print(preset, 'finished', len(done), json.dumps(summ))


if __name__ == '__main__':
    main()
