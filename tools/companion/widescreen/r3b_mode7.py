#!/usr/bin/env python3
"""Round 3b item 5: Mode 7 (intro flyover) on the wide core. Every frame of FROM..TO: centre vs the stock core, side columns (black expected).
usage: r3b_mode7.py OUT_DIR [from] [to] [core.so]"""
import json, pathlib, sys
import numpy as np
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wscore  # noqa: E402
from PIL import Image  # noqa: E402
out = pathlib.Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
f0 = int(sys.argv[2]) if len(sys.argv) > 2 else 4200
f1 = int(sys.argv[3]) if len(sys.argv) > 3 else 5000
core = sys.argv[4] if len(sys.argv) > 4 else wscore.CORE_WS
script = [tuple(int(x) if i < 2 else x for i, x in enumerate(l.split())) for l in open(wscore.common.NEWGAME) if l.strip() and not l.startswith('#')]
O = {'snes9x_som_widescreen': '46', 'snes9x_som_hires': 'native'}
a = wscore.WS(wscore.common.ROM_VANILLA, wscore.CORE_PLAIN, {})
b = wscore.WS(wscore.common.ROM_VANILLA, core, O)
res = {'frames': 0, 'm7': 0, 'centre_bad': [], 'side_lit': [], 'widths': set()}
worst = None
for f in range(f1):
    btn = next((s[2] for s in script if s[0] <= f < s[1]), None)
    for c in (a, b):
        c.set_pad(c.mask(btn)); c.step()
    if f < f0:
        continue
    A, B = a.rgb(), b.rgb(); W = B.shape[1]; res['widths'].add(W)
    m = (W - A.shape[1]) // 2 if A.shape[1] == 256 else 0
    cen = B[:, m:m + A.shape[1]] if B.shape[1] >= A.shape[1] else B
    res['frames'] += 1
    d = int((np.abs(cen.astype(int) - A.astype(int)).max(2) > 0).sum()) if cen.shape == A.shape else -1
    sides = np.concatenate([B[:, :m], B[:, m + 256:]], 1) if m else np.zeros((1, 1, 3), np.uint8)
    lit = int((sides.max(2) > 0).sum())
    if d: res['centre_bad'].append((f, d))
    if lit:
        res['side_lit'].append((f, lit))
        if worst is None or lit > worst[1]:
            worst = (f, lit, A.copy(), B.copy())
res['widths'] = sorted(res['widths'])
if worst:
    f, lit, A, B = worst
    pad = np.zeros((A.shape[0], (B.shape[1] - A.shape[1]) // 2, 3), np.uint8)
    Image.fromarray(np.concatenate([np.concatenate([pad, A, pad], 1), B], 0)).save(str(out / ('mode7_worst_%d.png' % f)))
json.dump({k: (v if not isinstance(v, list) else v[:40]) for k, v in res.items()} | {'n_centre_bad': len(res['centre_bad']), 'n_side_lit': len(res['side_lit'])}, open(out / 'mode7.json', 'w'), indent=1)
print(json.dumps({k: (v if not isinstance(v, list) else v[:8]) for k, v in res.items()} | {'n_centre_bad': len(res['centre_bad']), 'n_side_lit': len(res['side_lit'])}))

ok = not res['centre_bad'] and res['widths'] == [348] and len(res['side_lit']) > 500
print('PASS' if ok else 'FAIL', 'intro flight (Mode 7, round 3c: real widescreen) frames %d..%d: centre identical to the stock core, the sides carry the continued floor (%d frames with lit sides)' % (f0, f1, len(res['side_lit'])))
sys.exit(0 if ok else 1)
