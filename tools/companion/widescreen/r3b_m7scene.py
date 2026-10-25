#!/usr/bin/env python3
"""Round 3b item 5: Mode 7 maps (253, 488 ...) through the wide core vs the stock core, same state, N frames with scripted walking.
usage: r3b_m7scene.py OUT_DIR [maps] [frames]"""
import json, pathlib, sys
import numpy as np
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wscore, scenes, wslib  # noqa: E402
from PIL import Image  # noqa: E402
out = pathlib.Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
maps = [int(x) for x in (sys.argv[2] if len(sys.argv) > 2 else '253,488').split(',')]
N = int(sys.argv[3]) if len(sys.argv) > 3 else 400
O = {'snes9x_som_widescreen': '46', 'snes9x_som_hires': 'native'}
res = {}
for m in maps:
    a = wscore.WS(wscore.common.ROM_VANILLA, wscore.CORE_PLAIN, {})
    b = wscore.WS(wscore.common.ROM_VANILLA, wscore.CORE_WS, O)
    st = wscore.WORK / 'ws_scenes' / ('map%d.state' % m)
    if not st.exists():
        wc = wslib.WCore(wslib.common.ROM_VANILLA); scenes.load_scene(wc, m); del wc
        import os
        for k in ('SOM_WS_MARGIN', 'SOM_WS_BG3', 'SOM_WS_OBJ'): os.environ.pop(k, None)
    data = st.read_bytes()
    for c in (a, b):
        c.load_state(data)
    cbad = []; sl = []; widths = set(); row_bad = 0; worst = None
    btns = ['RIGHT', 'UP', 'LEFT', 'DOWN']
    for f in range(N):
        btn = btns[(f // 40) % 4]
        for c in (a, b):
            c.set_pad(c.mask(btn)); c.step()
        A, B = a.rgb(), b.rgb(); W = B.shape[1]; widths.add(W)
        mg = (W - 256) // 2 if (A.shape[1] == 256 and W > 256) else 0
        cen = B[:, mg:mg + A.shape[1]]
        if cen.shape != A.shape:
            cbad.append((f, 'shape', B.shape[1])); continue
        d = np.abs(cen.astype(int) - A.astype(int)).max(2) > 0
        if d.any():
            cbad.append((f, int(d.sum()), sorted(set(np.nonzero(d.any(1))[0].tolist()))[:6]))
            if worst is None: worst = (f, A.copy(), B.copy())
        if mg:
            lit = int((np.concatenate([B[:, :mg], B[:, mg + 256:]], 1).max(2) > 0).sum())
            if lit: sl.append((f, lit))
    res[m] = dict(widths=sorted(widths), n_centre_bad=len(cbad), centre_bad=cbad[:6], n_side_lit=len(sl), side_lit=sl[:4], mode7=int(a.u8(0x2105) if False else 0))
    if worst:
        f, A, B = worst; pad = np.zeros((A.shape[0], max(0, (B.shape[1] - A.shape[1]) // 2), 3), np.uint8)
        Image.fromarray(np.concatenate([np.concatenate([pad, A, pad], 1)[:, :B.shape[1]], B], 0)).save(str(out / ('m7_map%d_f%d.png' % (m, f))))
    Image.fromarray(np.concatenate([np.concatenate([np.zeros((224, max(0, (B.shape[1]-256)//2), 3), np.uint8), A, np.zeros((224, max(0, (B.shape[1]-256)//2), 3), np.uint8)], 1)[:, :B.shape[1]], B], 0)).save(str(out / ('m7_map%d_last.png' % m)))
    print(m, json.dumps(res[m]), flush=True)
json.dump(res, open(out / 'm7scene.json', 'w'), indent=1)

ok = all(r['n_centre_bad'] == 0 and r['n_side_lit'] > 0 and r['widths'] == [348] for r in res.values())
print('PASS' if ok else 'FAIL', 'Mode 7 maps %s (round 3c): wide 348 px, every centre frame identical to the stock core, sides drawn' % maps)
sys.exit(0 if ok else 1)
