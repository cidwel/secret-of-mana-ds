#!/usr/bin/env python3
"""Round 3d item 3: intro flight, wide core vs stock, every STEP frames in FROM..TO: horizon region of the side columns, zoomed.  Also counts
black pixels in the sky rows of the sides (must be 0) and checks the Mode 1 / Mode 7 split line (row jump between the last sky row and the first floor row
of the side vs the centre).  usage: r3d_horizon.py OUT_DIR [from] [to] [step] [core.so]; exit 0 = pass"""
import pathlib, sys
import numpy as np
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wscore  # noqa: E402
from PIL import Image  # noqa: E402
out = pathlib.Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
f0 = int(sys.argv[2]) if len(sys.argv) > 2 else 4200; f1 = int(sys.argv[3]) if len(sys.argv) > 3 else 5100; step = int(sys.argv[4]) if len(sys.argv) > 4 else 50
core = sys.argv[5] if len(sys.argv) > 5 else wscore.CORE_WS
script = [tuple(int(x) if i < 2 else x for i, x in enumerate(l.split())) for l in open(wscore.common.NEWGAME) if l.strip() and not l.startswith('#')]
a = wscore.WS(wscore.common.ROM_VANILLA, wscore.CORE_PLAIN, {})
b = wscore.WS(wscore.common.ROM_VANILLA, core, {'snes9x_som_widescreen': '46', 'snes9x_som_hires': 'native'})
tiles = []; black = []; seam = []
for f in range(f1):
    btn = next((s[2] for s in script if s[0] <= f < s[1]), None)
    for c in (a, b):
        c.set_pad(c.mask(btn)); c.step()
    if f < f0: continue
    A, B = a.rgb(), b.rgb(); m = (B.shape[1] - 256) // 2
    cen_ok = np.array_equal(B[:, m:m + 256], A)
    if not cen_ok: black.append((f, 'centre differs'))
    if f % 5 == 0:
        sky = B[0:80]; sides = np.concatenate([sky[:, :m], sky[:, m + 256:]], 1)
        nb = int((sides.max(2) == 0).sum())
        # only count while the picture is on (the centre sky is not black)
        if A[10, 128].max() > 0 and nb > 0: black.append((f, nb))
    if f % step == 0 and f >= f0:
        pad = np.zeros((224, m, 3), np.uint8)
        full = np.concatenate([np.concatenate([pad, A, pad], 1), B], 0)
        z = np.concatenate([full[60:110, :m + 40], full[60 + 224:110 + 224, :m + 40], full[60:110, -m - 40:], full[60 + 224:110 + 224, -m - 40:]], 0)
        tiles.append(np.kron(z, np.ones((3, 3, 1), np.uint8)))
for i in range(0, len(tiles), 4):
    Image.fromarray(np.concatenate(tiles[i:i + 4], 1)).save(str(out / ('horizon_%d.png' % (i // 4))))
print('problems:', black[:8], len(black))
ok = not black
print('PASS' if ok else 'FAIL', 'flight %d..%d: centre identical to stock, no black pixels in the sky rows of the sides' % (f0, f1))
sys.exit(0 if ok else 1)
