#!/usr/bin/env python3
"""Images for the report: the wide render of several scenes (rows above the HUD) with the Thor / 3DS crop windows
marked, and a wrap-versus-rebuilt comparison.  usage: make_images.py MODE   (MODE = fix | nofix)"""
import sys
import pathlib

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wslib  # noqa: E402
import scenes  # noqa: E402

MAPS = [131, 133, 162, 28, 102, 230]


def main():
    mode = sys.argv[1]
    c = wslib.WCore(wslib.common.ROM_VANILLA)
    out = wslib.OUT / 'l2'
    out.mkdir(exist_ok=True)
    rows = []
    for m in MAPS:
        scenes.load_scene(c, m)
        c.run(3)
        w = c.wide()[:199, 128:640].copy()      # 512 px: 128 px of extension per side
        im = Image.fromarray(w)
        d = ImageDraw.Draw(im)
        for e, col in ((128, (255, 255, 255)), (128 - 27, (255, 255, 0)), (128 - 17, (0, 255, 255))):
            d.line([(e, 0), (e, 198)], fill=col)
            d.line([(512 - e, 0), (512 - e, 198)], fill=col)
        d.text((4, 4), 'map %d' % m, fill=(255, 0, 255))
        rows.append(np.array(im))
    Image.fromarray(np.concatenate(rows, axis=0)).save(out / ('sheet_%s.png' % mode))


main()
