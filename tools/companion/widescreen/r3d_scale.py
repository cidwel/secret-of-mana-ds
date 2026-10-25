#!/usr/bin/env python3
"""Round 3d item 4: sharpness of the waterfall / rock wall of map 256 under every scaler the app can use, at the Thor's top panel (1920x1080,
348x224 wide frame, FILL layout: 5.52 x 4.83), with and without the de-dither pass, hires average / native.
Metrics over the wall region: share of output pixels that are not a source colour (blended), texel flatness (mean std of the output pixels inside
one source texel, 0 = flat blocks), Laplacian variance relative to a nearest-neighbour render of the same geometry.
usage: r3d_scale.py OUT_DIR   (needs OUT/wide_nat.pkl and wide_avg.pkl from r3b_dump.py)"""
import pathlib, pickle, sys, json
import numpy as np
from PIL import Image
out = pathlib.Path(sys.argv[1])
FR = 5800
W, H = 1920, 1080
def load(n): return pickle.load(open(out / (n + '.pkl'), 'rb'))[FR][0].astype(np.float32)
def geom(w, h, mode):
    if mode == 'fill':
        pxw = W / w; sy = pxw * 7 / 8; return (0, 0, W, round(h * sy))
    sy = 4; pxw = 5; dw, dh = w * pxw, h * sy; return ((W - dw) // 2, (H - dh) // 2, (W - dw) // 2 + dw, (H - dh) // 2 + dh)
def render(img, kind, mode):
    h, w = img.shape[:2]; l, t, r, b = geom(w, h, mode)
    ys, xs = np.mgrid[0:b - t, 0:r - l]
    sx, sy = (r - l) / w, (b - t) / h
    px = (xs + 0.5) / sx; py = (ys + 0.5) / sy
    g = lambda yy, xx: img[np.clip(yy, 0, h - 1), np.clip(xx, 0, w - 1)]
    if kind == 'nearest':
        return g(np.floor(py).astype(int), np.floor(px).astype(int)), (sx, sy)
    if kind in ('bilinear', 'sharp'):
        if kind == 'sharp':
            pf, qf = np.floor(px), np.floor(py); cdx, cdy = px - pf - 0.5, py - qf - 0.5
            rx, ry = max(0, 0.5 - 0.5 / sx), max(0, 0.5 - 0.5 / sy)
            qx = pf + (cdx - np.clip(cdx, -rx, rx)) * sx; qy = qf + (cdy - np.clip(cdy, -ry, ry)) * sy
        else:
            qx, qy = px - 0.5, py - 0.5
        x0, y0 = np.floor(qx).astype(int), np.floor(qy).astype(int); fx, fy = (qx - x0)[..., None], (qy - y0)[..., None]
        return (g(y0, x0) * (1 - fx) + g(y0, x0 + 1) * fx) * (1 - fy) + (g(y0 + 1, x0) * (1 - fx) + g(y0 + 1, x0 + 1) * fx) * fy, (sx, sy)
    if kind == 'area':    # nearest at 8x then box average down: every output pixel is the area mean of the texels it covers
        k = 8; big = np.repeat(np.repeat(img, k, 0), k, 1)
        yy, xx = np.arange(b - t), np.arange(r - l)
        y0 = (yy * h * k / (b - t)).astype(int); y1 = np.maximum(((yy + 1) * h * k / (b - t)).astype(int), y0 + 1)
        x0 = (xx * w * k / (r - l)).astype(int); x1 = np.maximum(((xx + 1) * w * k / (r - l)).astype(int), x0 + 1)
        cs = big.cumsum(0).cumsum(1); cs = np.pad(cs, ((1, 0), (1, 0), (0, 0)))
        A = cs[y1][:, x1] - cs[y0][:, x1] - cs[y1][:, x0] + cs[y0][:, x0]
        return A / ((y1 - y0)[:, None, None] * (x1 - x0)[None, :, None]), (sx, sy)
def metrics(o, img, sc):
    reg = o[300:700, 700:1500] if o.shape[0] >= 700 else o[:, :]
    src = {tuple(p) for p in np.unique(img.reshape(-1, 3), axis=0).astype(int)}
    sub = reg.reshape(-1, 3).astype(int)[::5]
    blended = float(np.mean([tuple(p) not in src for p in sub]))
    L = reg @ [0.299, 0.587, 0.114]
    lap = L[1:-1, 1:-1] * 4 - L[:-2, 1:-1] - L[2:, 1:-1] - L[1:-1, :-2] - L[1:-1, 2:]
    ty = np.floor((np.arange(reg.shape[0]) + 300 + .5) / sc[1]).astype(int)[:, None] if o.shape[0] >= 700 else 0
    return round(blended * 100, 1), float(lap.var())
res = {}
for hires in ('native', 'average'):
    for dd in (False, True):
        img = load('wide_nat' if hires == 'native' else 'wide_avg')
        if dd:
            import ctypes as C, subprocess
            so = out / 'libdd48.so'
            lib = C.CDLL(str(so)); a = np.ascontiguousarray(np.dstack([img.astype(np.uint8), np.zeros(img.shape[:2], np.uint8)]).view(np.uint32)[..., 0])
            lib.dd(a.ctypes.data_as(C.c_void_p), a.shape[1], a.shape[0]); img = a.view(np.uint8).reshape(a.shape[0], a.shape[1], 4)[..., :3].astype(np.float32)
        for mode in ('fill', 'integer'):
            ref = None
            for kind in ('nearest', 'bilinear', 'sharp', 'area'):
                o, sc = render(img, kind, mode)
                if mode == 'integer' and kind != 'nearest': continue
                bl, lv = metrics(o, img, sc)
                if kind == 'nearest': ref = lv
                key = '%s|dedither=%s|%s|%s' % (hires, dd, mode, kind)
                res[key] = dict(blended_pct=bl, lap_var_ratio=round(lv / ref, 3))
                if hires == 'native' and not dd or (hires == 'native' and dd and kind in ('nearest', 'area')):
                    crop = np.clip(o[300:600, 760:1160], 0, 255).astype(np.uint8)
                    Image.fromarray(crop).save(str(out / ('scale2_%s_%s_%s_dd%d.png' % (hires, mode, kind, dd))))
json.dump(res, open(out / 'scale2.json', 'w'), indent=1)
for k, v in res.items(): print('%-46s blended %5.1f %%   Laplacian variance vs nearest %.3f' % (k, v['blended_pct'], v['lap_var_ratio']))
