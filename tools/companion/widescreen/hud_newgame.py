#!/usr/bin/env python3
"""New-game flow (title, new game, name entry, intro dialogue, first fields; 8,400 frames) with snes9x_som_hud=hide_except_ring versus the
unmodified core.  Reports differing frames by BG mode and whether every difference lies in rows >= 184 (HUD rows).
usage: hud_newgame.py OUT.json"""
import collections
import json
import sys
import pathlib

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wscore  # noqa: E402

script = []
for ln in open(wscore.common.NEWGAME):
    if ln.startswith('#') or not ln.strip():
        continue
    a, b, btn = ln.split()
    script.append((int(a), int(b), btn))
a = wscore.WS(wscore.common.ROM_VANILLA, wscore.CORE_PLAIN)
c = wscore.WS(wscore.common.ROM_VANILLA, wscore.CORE_WS, {'snes9x_som_hud': 'hide_except_ring'})
res = collections.Counter()
for f in range(8400):
    btn = next((s[2] for s in script if s[0] <= f < s[1]), None)
    m = a.mask(btn)
    a.set_pad(m); c.set_pad(m)
    a.step(); c.step()
    A, C = wscore.raw16(a), wscore.raw16(c)
    mode = c.regs(0x2105)[0] & 7
    res['frames'] += 1
    res['mode%d' % mode] += 1
    if A.shape != C.shape:
        res['shape_mismatch'] += 1
        continue
    d = A != C
    if d.any():
        res['diff_frames'] += 1
        res['diff_mode%d' % mode] += 1
        if d[:184].any():
            res['diff_above_row_184'] += 1
json.dump(dict(res), open(sys.argv[1], 'w'), indent=1)
print(dict(res))
