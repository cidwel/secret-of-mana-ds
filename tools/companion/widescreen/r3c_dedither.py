#!/usr/bin/env python3
"""Round 3c item A: checkerboard (dither) energy of the field frame before and after som_dedither.h, on the log/trunk scene of a new game
(wide core 46 px, hires native), plus a stock snes9x pair average for reference, and proof that the dialogue font does not change.
usage: r3c_dedither.py OUT_DIR       Exit status 0 = all checks pass.  Needs <OUT>/wide_nat.pkl from r3b_dump.py."""
import ctypes as C, pathlib, pickle, subprocess, sys
import numpy as np
from PIL import Image
HERE = pathlib.Path(__file__).resolve().parent
out = pathlib.Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
so = out / 'libdd.so'
src = out / 'dd.c'
src.write_text('#include "som_dedither.h"\nint dd(uint32_t *p, int w, int h) { return som_dedither_rows(p, w, h, w); }\n')
subprocess.check_call(['cc', '-O2', '-shared', '-fPIC', '-I', str(HERE.parent), '-o', str(so), str(src)])
lib = C.CDLL(str(so))
def dedither(img):
    a = np.ascontiguousarray(np.dstack([img, np.zeros(img.shape[:2], np.uint8)]).view(np.uint32)[..., 0])
    n = lib.dd(a.ctypes.data_as(C.c_void_p), a.shape[1], a.shape[0])
    return a.view(np.uint8).reshape(a.shape[0], a.shape[1], 4)[..., :3].copy(), n
def energy(img, box):
    y0, y1, x0, x1 = box
    L = img[y0:y1, x0:x1].astype(float) @ [0.299, 0.587, 0.114]
    return float(np.abs(L[:, :-2] - 2 * L[:, 1:-1] + L[:, 2:]).mean())
P = pickle.load(open(out / 'wide_nat.pkl', 'rb'))
BOX = (12, 52, 70, 280)          # dialogue window interior, frame 5500
ROCK = (110, 220, 90, 260)       # rock wall under the window
RES = {}
fails = []
for fr in (5500, 5800, 6000):
    im = P[fr][0]
    o, n = dedither(im)
    RES[fr] = dict(changed=n, box_before=energy(im, BOX), box_after=energy(o, BOX), rock_before=energy(im, ROCK), rock_after=energy(o, ROCK))
    pad = np.zeros((224, 0, 3), np.uint8)
    Image.fromarray(np.kron(np.concatenate([im[0:140, 40:300], o[0:140, 40:300]], 0), np.ones((3, 3, 1), np.uint8))).save(str(out / ('dedither_%d.png' % fr)))
    print(fr, RES[fr])
r = RES[5500]
if not r['box_after'] < r['box_before'] * 0.35: fails.append('dialogue box energy not reduced enough')
if not r['rock_after'] > r['rock_before'] * 0.97: fails.append('rock wall texture was smoothed')
# text: every near white pixel (glyph) stays exactly the same
im = P[5500][0]; o, _ = dedither(im)
white = (im.min(2) > 200)
if (o[white] != im[white]).any(): fails.append('glyph pixels changed')
print('glyph pixels', int(white.sum()), 'changed', int((o[white] != im[white]).any(1).sum()))
print('PASS' if not fails else 'FAIL ' + '; '.join(fails))
sys.exit(1 if fails else 0)
