#!/usr/bin/env python3
"""Finds usable warp targets in a generated Ancient Cave ROM: the transition table is generator output, so the indices that
reach a finished map with enemies differ per seed and preset.  A candidate is tried with the game's own warp event
(opcode 0x18, as tools/companion/direct/make_mob_states.py) on a free-play state.

    t5_warps.py PRESET [STATE]   prints {"enemy": [...], "quiet": [...], "stuck": [...]} and writes $T5/out/warps_PRESET.json
"""
import json
import os
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import retro  # noqa: E402

T5 = pathlib.Path(os.environ.get('T5', os.path.expanduser('~/.local/state/som-decomp/t5-ac-20261018')))


def discover(preset, limit=40, state=None):
    rom = T5 / 'roms' / (preset + '.sfc')
    st = state or (T5 / 'state/work' / ('ac_party_%s.state' % preset)).read_bytes()
    c = retro.Core(str(rom), core_path=str(T5 / 'core/snes9x_widescreen_hires_libretro.so'))
    out = dict(enemy=[], quiet=[], stuck=[], detail={})
    for n in range(limit):
        c.load_state(st)
        c.run(2)
        c.write(0x1FF00, bytes([0x18 | (n >> 8), n & 0xFF, 8]))
        c.write(0xD1, b'\x00\xff\x7f')
        c.run(1)
        c.write(0xD0, b'\x01')
        c.run(300)
        en = [c.u8(0xE000 + 0x200 * s) == 1 and 0 < c.u16(0xE182 + 0x200 * s) <= c.u16(0xE184 + 0x200 * s) and c.u8(0xE180 + 0x200 * s) != 0 for s in (3, 4, 5)]
        d = dict(map=c.u8(0xDC), d0=c.u8(0xD0), enemies=en)
        out['detail'][str(n)] = d
        if c.u8(0xD0) != 0:
            out['stuck'].append(n)
        elif any(en):
            out['enemy'].append(n)
        else:
            out['quiet'].append(n)
    return out


if __name__ == '__main__':
    r = discover(sys.argv[1], state=pathlib.Path(sys.argv[2]).read_bytes() if len(sys.argv) > 2 else None)
    (T5 / 'out').mkdir(exist_ok=True)
    (T5 / 'out' / ('warps_%s.json' % sys.argv[1])).write_text(json.dumps(r))
    print(json.dumps({k: r[k] for k in ('enemy', 'quiet', 'stuck')}))
