#!/usr/bin/env python3
"""Round 3c item D: the first-sword pick-up effect (a window ellipse plus colour math on frames ~9250-9480 of the TAS route) must not be
repeated in the side columns of the wide core.  Compares wide 46 px (centre vs stock, sides: the effect's blue must not appear) and
writes PNG strips.   usage: r3c_sword.py OUT_DIR [core.so [previous-core.so]]      exit 0 = pass"""
import pathlib, sys
import numpy as np
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wscore  # noqa: E402
from PIL import Image  # noqa: E402
out = pathlib.Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
core = sys.argv[2] if len(sys.argv) > 2 else wscore.CORE_WS
ev = []
for ln in open(wscore.common.ROUTE):
    if ln.startswith('#') or not ln.strip(): continue
    p = ln.strip().split(None, 2); ev.append((int(p[0]), int(p[1]), p[2].split(',') if len(p) > 2 else []))
a = wscore.WS(wscore.common.ROM_VANILLA, wscore.CORE_PLAIN, {})
o = wscore.WS(wscore.common.ROM_VANILLA, pathlib.Path(sys.argv[3]) if len(sys.argv) > 3 else core, {'snes9x_som_widescreen': '46', 'snes9x_som_hires': 'native'})
b = wscore.WS(wscore.common.ROM_VANILLA, core, {'snes9x_som_widescreen': '46', 'snes9x_som_hires': 'native', 'snes9x_som_hud': 'hide_except_ring'})
white = lambda X: int((X.min(2) > 215).sum())
tot_new = tot_old = 0; cen_bad = 0; strip = []
for f in range(9330):
    btn = []
    for s_, e_, n in ev:
        if s_ <= f <= e_: btn += n
    for c in (a, b, o):
        c.set_pad(c.mask(btn)); c.step()
    if f < 9240: continue
    A, B, OLD = a.rgb(), b.rgb(), o.rgb(); m = (B.shape[1] - 256) // 2
    cen_bad += int((B[:, m:m + 256] != A).any(2).sum() > 0)
    side = lambda X: np.concatenate([X[:, :m], X[:, m + 256:]], 1)
    tot_new += white(side(B)); tot_old += white(side(OLD))
    if f in (9260, 9300, 9330):
        pad = np.zeros((224, m, 3), np.uint8)
        strip.append(np.concatenate([np.concatenate([pad, A, pad], 1), OLD, B], 0))
Image.fromarray(np.concatenate(strip, 1)).save(str(out / 'sword_stock_old_new.png'))
print('centre frames differing from stock:', cen_bad, ' near-white side pixels (frames 9240-9330): old core', tot_old, 'new core', tot_new)
ok = cen_bad == 0 and tot_new < 0.7 * tot_old
print('PASS' if ok else 'FAIL', 'sword effect is not repeated in the sides (rows: stock / before round 3c / after)')
sys.exit(0 if ok else 1)
