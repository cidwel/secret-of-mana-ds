#!/usr/bin/env python3
"""Level 1: aspect-handling options for the top screen, measured on real frames.

Outputs (state dir, widescreen/l1/): one image per scene and target with every option, plus metrics.json.
Targets: AYN Thor top 1920x1080, 3DS top 400x240, 3DS wide mode 800x240 (pixels half as wide as tall).
"""
import json
import math
import sys
import pathlib

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wslib  # noqa: E402
import scenes  # noqa: E402

PAR = 8 / 7           # SNES pixel aspect on a 4:3 display
HUD_ROW = 199         # first HUD row (BG3 plus sprites, measured by the prototype)
TARGETS = {
    'thor_1920x1080': (1920, 1080, 1.0),
    '3ds_400x240': (400, 240, 1.0),
    '3ds_wide_800x240': (800, 240, 0.5),
}


def plan(name, tw, th, pw, rows, width, mode):
    """Returns the per-axis scale (sx, sy in target pixels per source pixel), the source window and the notes."""
    sw, sh = width, rows
    if mode == 'int':
        n = max(1, int(min(tw / (sw * pw), th / sh)))
        sx, sy = n / pw if pw != 1 else n, n
        if pw != 1:
            sx = 2 * n
        win = (sw, sh)
    elif mode == 'fitpar':
        sy = min(th / sh, tw / (sw * PAR / pw))
        sx = sy * PAR / pw
        win = (sw, sh)
    elif mode == 'stretch':
        sx, sy = tw / sw, th / sh
        win = (sw, sh)
    elif mode == 'wide':
        sy = th / sh
        sx = sy * PAR / pw
        need = tw / sx
        e = min(128.0, max(0.0, (need - sw) / 2))
        win = (sw + 2 * e, sh)
        if need > sw + 256:
            sx = tw / win[0]
    elif mode == 'cover':
        # fill the whole target with square-ish (PAR) pixels, crop what does not fit
        sx_over_sy = PAR / pw
        s = max(th / sh, tw / (sw * sx_over_sy))
        sy = s
        sx = s * sx_over_sy
        win = (min(sw, tw / sx), min(sh, th / sy))
    else:
        raise ValueError(mode)
    return sx, sy, win


def render(src, tw, th, sx, sy, win, rows=224):
    """Nearest-neighbour render of the centred source window (inside the first `rows` rows) into a tw x th canvas."""
    src = src[:rows]
    h, w = src.shape[:2]
    ww, wh = win
    x0 = (w - ww) / 2
    y0 = 0.0 if wh >= h else (h - wh) / 2
    canvas = np.zeros((th, tw, 3), np.uint8)
    ox = (tw - ww * sx) / 2
    oy = (th - wh * sy) / 2
    ys = np.arange(th)
    xs = np.arange(tw)
    srcy = np.floor((ys - oy) / sy + y0).astype(int)
    srcx = np.floor((xs - ox) / sx + x0).astype(int)
    vy = (ys >= oy - 1e-6) & (ys < oy + wh * sy - 1e-6) & (srcy >= 0) & (srcy < h)
    vx = (xs >= ox - 1e-6) & (xs < ox + ww * sx - 1e-6) & (srcx >= 0) & (srcx < w)
    canvas[np.ix_(ys[vy], xs[vx])] = src[np.ix_(srcy[vy], srcx[vx])]
    return canvas


def metrics(tw, th, pw, sx, sy, win, rows_full=224, width_full=256):
    ww, wh = win
    shown_w, shown_h = min(tw, ww * sx), min(th, wh * sy)
    area_t = tw * th
    jit = lambda s: 0.0 if abs(s - round(s)) < 1e-6 else (math.ceil(s) - math.floor(s)) / s
    return {
        'scale_x': round(sx, 3), 'scale_y': round(sy, 3),
        'shown_px': [round(shown_w), round(shown_h)],
        'bar_percent': round(100 * (1 - shown_w * shown_h / area_t), 1),
        'aspect_error_percent': round(100 * ((sx * pw) / sy / PAR - 1), 1),
        'source_rows_shown': round(wh), 'source_cols_shown': round(ww),
        'game_area_kept_percent': round(100 * ww * wh / (width_full * rows_full), 1),
        'pixel_size_jitter_percent': round(100 * max(jit(sx), jit(sy)), 1),
    }


def options(tw, th, pw):
    res = []
    for label, rows, mode in (
            ('A integer scale (square px), HUD kept', 224, 'int'),
            ('B integer scale, HUD rows cropped', HUD_ROW, 'int'),
            ('C fit, 8:7 pixel aspect, HUD kept', 224, 'fitpar'),
            ('D fit, 8:7 pixel aspect, HUD rows cropped', HUD_ROW, 'fitpar'),
            ('E stretch to fill, HUD kept', 224, 'stretch'),
            ('F stretch to fill, HUD rows cropped', HUD_ROW, 'stretch'),
            ('G cover (crop rows to fill), HUD kept', 224, 'cover'),
            ('H cover (crop rows to fill), HUD rows cropped', HUD_ROW, 'cover'),
            ('I true wide (level 2), 8:7 pixels, HUD rows cropped', HUD_ROW, 'wide'),
            ('J true wide (level 2), 8:7 pixels, HUD kept', 224, 'wide')):
        res.append((label, rows, mode))
    return res


def main():
    out = wslib.OUT / 'l1'
    out.mkdir(parents=True, exist_ok=True)
    maps = [131, 133, 162, 28]
    c = wslib.WCore(wslib.common.ROM_VANILLA)
    frames = {}
    for m in maps:
        scenes.load_scene(c, m)
        c.run(3)
        frames['map%d' % m] = c.wide()
    c.load_state(open(wslib.WORK / 'base_vanilla.state', 'rb').read())
    c.run(3)
    frames['potos_start'] = c.wide()
    allm = {}
    for tname, (tw, th, pw) in TARGETS.items():
        allm[tname] = {}
        for label, rows, mode in options(tw, th, pw):
            sx, sy, win = plan(tname, tw, th, pw, rows, 256, mode)
            allm[tname][label] = dict(metrics(tw, th, pw, sx, sy, win), extra_px_per_side=round((win[0] - 256) / 2, 1))
        for sname, frame in frames.items():
            tiles = []
            for label, rows, mode in options(tw, th, pw):
                sx, sy, win = plan(tname, tw, th, pw, rows, 256, mode)
                src = frame if mode == 'wide' else frame[:, 256:512]
                img = render(src, tw, th, sx, sy, win, rows)
                # downscale for the sheet only (the metrics are measured on the target-size canvas)
                im = Image.fromarray(img)
                scale = 480 / tw if tw >= 800 else 1.0
                if scale != 1.0:
                    im = im.resize((int(tw * scale), int(th * scale)), Image.NEAREST)
                if pw != 1.0:
                    im = im.resize((im.width // 2, im.height), Image.NEAREST) if False else im
                d = ImageDraw.Draw(im)
                d.text((3, 3), label.split(' ')[0], fill=(255, 255, 0))
                tiles.append(np.array(im))
            hmax = max(t.shape[0] for t in tiles)
            wmax = max(t.shape[1] for t in tiles)
            pad = [np.pad(t, ((0, hmax - t.shape[0]), (0, wmax - t.shape[1]), (0, 0)), constant_values=40) for t in tiles]
            rowsimg = [np.concatenate([np.pad(p, ((2, 2), (2, 2), (0, 0)), constant_values=90) for p in pad[i:i + 4]], axis=1) for i in range(0, len(pad), 4)]
            wmx = max(r.shape[1] for r in rowsimg)
            rowsimg = [np.pad(r, ((0, 0), (0, wmx - r.shape[1]), (0, 0))) for r in rowsimg]
            sheet = np.concatenate(rowsimg, axis=0)
            Image.fromarray(sheet).save(out / ('%s_%s.png' % (tname, sname)))
        # full-size example of the two best candidates for the main scene
    json.dump(allm, open(out / 'metrics.json', 'w'), indent=1)
    for t, v in allm.items():
        print(t)
        for k, m in v.items():
            print('  ', k, m)


if __name__ == '__main__':
    main()
