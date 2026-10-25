#!/usr/bin/env python3
"""How far beyond the 256 px centre is the wide render correct?

For each map: warp in, render the wide frame W0 (BG layers only), walk the hero sideways and compare every
later centre image with the columns of W0 that predicted it.  Output: per-map, per-distance match fractions.

usage: measure_margin.py OUT.json [--maps a,b,c] [--limit N]   (env SOM_WS_FIX=0 disables the tilemap rewrite)
"""
import json
import sys
import pathlib

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wslib  # noqa: E402
from survey import trans, warp  # noqa: E402,F401

DMAX = 160


def walk(c, base, n, btn, w0, cam0):
    """Returns (match, total) arrays shaped (2, DMAX) for [left, right] by distance from the centre edge."""
    c.load_state(base)
    c.run(1)
    match = np.zeros((2, DMAX)); total = np.zeros((2, DMAX)); moved = 0
    c.set_pad(c.mask(btn))
    nxt = 32
    for _ in range(n):
        c.step()
        cx, cy = c.line_bg(10, 0)
        if cy != cam0[1]:
            continue
        sh = (cx - cam0[0] + 512) % 1024 - 512
        moved = max(moved, abs(sh))
        if abs(sh) < nxt:
            continue
        nxt += 32
        ct = c.stock().astype(np.int16)
        for j in range(256):
            col = j + sh + 256
            if 0 <= col < 256:
                side, d = 0, 255 - col
            elif col >= 512 and col < 768:
                side, d = 1, col - 512
            else:
                continue
            if d >= DMAX:
                continue
            eq = (w0[:199, col].astype(np.int16) == ct[:199, j]).all(axis=1)
            match[side, d] += eq.sum(); total[side, d] += len(eq)
    c.set_pad(0)
    return match, total, moved


def main():
    out = sys.argv[1]
    maps = None
    limit = None
    if '--maps' in sys.argv:
        maps = [int(x) for x in sys.argv[sys.argv.index('--maps') + 1].split(',')]
    if '--limit' in sys.argv:
        limit = int(sys.argv[sys.argv.index('--limit') + 1])
    survey = json.load(open(wslib.OUT / 'survey.json'))
    base = open(wslib.WORK / 'base_vanilla.state', 'rb').read()
    opts = {'snes9x_layer_3': 'disabled', 'snes9x_layer_5': 'disabled'}
    c = wslib.WCore(wslib.common.ROM_VANILLA, options=opts)
    res = {}
    for m, info in sorted(survey.items(), key=lambda kv: int(kv[0])):
        if maps and int(m) not in maps:
            continue
        if limit is not None and len(res) >= limit:
            break
        c.load_state(base)
        c.run(2)
        warp(c, info['trans'])
        c.run(260)
        st = c.save_state()
        w0 = c.wide()
        cam0 = c.line_bg(10, 0)
        r = {'cam0': cam0, 'w': info['w'], 'h': info['h']}
        mt = np.zeros((2, DMAX)); tt = np.zeros((2, DMAX)); mv = 0
        for btn, n in (("RIGHT", 220), ("LEFT", 220)):
            if btn in ('DOWN', 'UP'):
                continue
            a, b, moved = walk(c, st, n, btn, w0, cam0)
            mt += a; tt += b; mv = max(mv, moved)
        r['moved'] = mv
        r['frac'] = [[(mt[s, d] / tt[s, d]) if tt[s, d] else None for d in range(DMAX)] for s in range(2)]
        r['n'] = [[int(tt[s, d]) for d in range(DMAX)] for s in range(2)]
        res[m] = r
        print(m, info['w'], info['h'], 'moved', mv, flush=True)
    json.dump(res, open(out, 'w'))


if __name__ == '__main__':
    main()
