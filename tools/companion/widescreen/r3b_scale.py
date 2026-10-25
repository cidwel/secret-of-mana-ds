#!/usr/bin/env python3
"""Round 3b item 6: how the app's scalers treat the log/trunk scene (wide 348 px frame, Thor top panel 1920x1080, FILL layout).
Emulates GameView: nearest, plain bilinear, the AGSL sharp-bilinear shader (rr = max(0, .5-.5/scale)) with the old fractional destination
and with the whole-pixel destination of round 3b.  Reports the share of output pixels that are not a flat copy of one source texel (blur)
and writes a crop PNG per method.   usage: r3b_scale.py OUT_DIR [frame_pkl] [frame]"""
import pathlib, pickle, sys
import numpy as np
from PIL import Image
out = pathlib.Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
pk = sys.argv[2] if len(sys.argv) > 2 else str(out / 'wide_nat.pkl'); fr = int(sys.argv[3]) if len(sys.argv) > 3 else 5800
img = pickle.load(open(pk, 'rb'))[fr][0].astype(np.float32)
h, w = img.shape[:2]
W, H = 1920, 1080
pxw = W / w; sy = pxw * 7 / 8
def dst(snap):
    l, t = (W - w * pxw) / 2, 0.0
    r, b = l + w * pxw, h * sy
    return (round(l), round(t), round(r), round(b)) if snap else (l, t, r, b)
def sample(kind, d):
    l, t, r, b = d
    ys, xs = np.mgrid[0:int(np.ceil(b)), 0:int(np.ceil(r))]
    sx, sy_ = (r - l) / w, (b - t) / h
    px = (xs + 0.5 - l) / sx; py = (ys + 0.5 - t) / sy_
    if kind == 'nearest':
        ix = np.clip(np.floor(px).astype(int), 0, w - 1); iy = np.clip(np.floor(py).astype(int), 0, h - 1)
        return img[iy, ix]
    if kind == 'bilinear':
        qx, qy = px - 0.5, py - 0.5
    else:   # sharp bilinear
        pf, qf = np.floor(px), np.floor(py)
        cdx, cdy = px - pf - 0.5, py - qf - 0.5
        rx, ry = max(0, 0.5 - 0.5 / sx), max(0, 0.5 - 0.5 / sy_)
        qx = pf + 0.5 + (cdx - np.clip(cdx, -rx, rx)) * sx - 0.5; qy = qf + 0.5 + (cdy - np.clip(cdy, -ry, ry)) * sy_ - 0.5
    x0, y0 = np.floor(qx).astype(int), np.floor(qy).astype(int); fx, fy = (qx - x0)[..., None], (qy - y0)[..., None]
    g = lambda yy, xx: img[np.clip(yy, 0, h - 1), np.clip(xx, 0, w - 1)]
    return (g(y0, x0) * (1 - fx) + g(y0, x0 + 1) * fx) * (1 - fy) + (g(y0 + 1, x0) * (1 - fx) + g(y0 + 1, x0 + 1) * fx) * fy
res = {}
for name, kind, snap in (('nearest', 'nearest', True), ('bilinear', 'bilinear', False), ('sharp_fractional_old', 'sharp', False), ('sharp_snapped_new', 'sharp', True)):
    o = sample(kind, dst(snap))
    # share of output pixels that are not exactly some source texel colour = blended pixels
    src = {tuple(p) for p in np.unique(img.reshape(-1, 3), axis=0).astype(int)}
    sub = o[300:700, 600:1300].reshape(-1, 3).astype(int)
    blended = np.mean([tuple(p) not in src for p in sub[::7]])
    res[name] = round(float(blended) * 100, 2)
    Image.fromarray(np.clip(o[250:550, 1000:1400], 0, 255).astype(np.uint8)).resize((800, 600), Image.NEAREST).save(str(out / ('scale_%s.png' % name)))
print(res)
