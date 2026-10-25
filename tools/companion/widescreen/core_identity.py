#!/usr/bin/env python3
"""Lockstep comparison of the widescreen core against the unmodified core, frame by frame.

usage: core_identity.py OUT.json MARGIN [--rom us|magno] [--maps N] [--hud]
For every map (warp through the event VM from the base state) both cores get the same inputs: 260 frames of fade
and settling, then 30 frames walking right and left.  The stock 256 px picture must equal the centre of the wide
frame (or the whole frame when the core reports 256).  Also records the width of every frame and the decisions.
"""
import argparse
import json
import sys
import pathlib

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wscore  # noqa: E402
from survey import warp  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('out')
    ap.add_argument('margin')
    ap.add_argument('--rom', default='us')
    ap.add_argument('--maps', type=int, default=0)
    ap.add_argument('--opt', action='append', default=[])
    a = ap.parse_args()
    rom = wscore.common.ROM_VANILLA if a.rom == 'us' else wscore.common.ROM_MAGNO
    base = open(wscore.WORK / ('base_vanilla.state' if a.rom == 'us' else 'base_magno.state'), 'rb').read()
    survey = json.load(open(wscore.OUT / 'survey.json'))
    maps = sorted((int(m), i) for m, i in survey.items() if i.get('ok'))
    if a.maps:
        step = max(1, len(maps) // a.maps)
        maps = maps[::step][:a.maps]
    opts = {'snes9x_som_widescreen': a.margin}
    opts.update(dict(o.split('=') for o in a.opt))
    plain = wscore.WS(rom, wscore.CORE_PLAIN)
    ws = wscore.WS(rom, wscore.CORE_WS, opts)
    res = {'frames': 0, 'diff_frames': 0, 'maps': {}}
    for m, info in maps:
        for c in (plain, ws):
            c.load_state(base)
            c.run(2)
            warp(c, info['trans'])
        n0 = len(ws.sizes)
        bad = 0
        widths = []
        total = 0
        for f in range(290):
            pad = 0 if f < 260 else plain.mask('RIGHT' if f < 275 else 'LEFT')
            plain.set_pad(pad); ws.set_pad(pad)
            plain.step(); ws.step()
            w = ws.width
            widths.append(w)
            a_ = wscore.raw16(plain)
            if plain.width == 512:
                a_ = wscore.merge_pairs(a_)
            b_ = wscore.raw16(ws)
            if w != 256:
                mg = (w - 256) // 2
                b_ = b_[:, mg:mg + 256]
            total += 1
            if not np.array_equal(a_, b_):
                bad += 1
        # run-length encoded widths
        rl = []
        for w in widths:
            if rl and rl[-1][0] == w:
                rl[-1][1] += 1
            else:
                rl.append([w, 1])
        st = ws.state()
        res['maps'][m] = {'diff': bad, 'frames': total, 'widths': rl, 'dc': st['dc'], 'final_width': widths[-1]}
        res['frames'] += total
        res['diff_frames'] += bad
        print(m, bad, rl, flush=True)
    res['n_maps'] = len(maps)
    res['maps_wide_at_end'] = sum(1 for v in res['maps'].values() if v['final_width'] > 256)
    json.dump(res, open(a.out, 'w'), indent=1)
    print('maps', len(maps), 'frames', res['frames'], 'differing', res['diff_frames'], 'wide at end', res['maps_wide_at_end'])


main()
