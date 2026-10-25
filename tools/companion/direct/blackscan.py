#!/usr/bin/env python3
"""Frame-by-frame top-screen capture around a direct command; reports fully black rows per frame.

    python3 blackscan.py VARIANT STATE equip HERO TYPE | item HERO REC | cast HERO SPIRIT SLOT KIND SLOT
"""
import json, pathlib, sys
import numpy as np
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest  # noqa: E402


def rows_black(c):
    a = np.frombuffer(c.frame_rgb, np.uint8).reshape(c.height, c.width, 3)
    blk = (a.reshape(c.height, -1).max(axis=1) == 0)
    return a, [i for i in range(c.height) if blk[i]]


def spans(rows):
    out = []
    for r in rows:
        if out and r == out[-1][1] + 1:
            out[-1][1] = r
        else:
            out.append([r, r])
    return out


def run(variant, state, cmd, args, pre=4, post=14, ring=False, dump=None):
    D = dtest.Direct(variant)
    c = D.core
    D.load(state, 4)
    rep = []
    for i in range(pre):
        D.frame()
        a, b = rows_black(c)
        rep.append(('pre', i, spans(b), None))
    if cmd == 'equip':
        rc = D.equip(int(args[0]), int(args[1]))
    elif cmd == 'item':
        rc = D.item(int(args[1]), 0, 0) if False else D.item(int(args[1]), 1, int(args[0]))
    else:
        h, sp, sl, kind, tslot = map(int, args)
        rc = D.cast(h, sp, sl, kind, tslot)
    prev = None
    for i in range(post):
        D.frame()
        a, b = rows_black(c)
        st = D.state
        inidisp = None
        rep.append((st, i, spans(b), int(c.u8(0xE4)) if False else None))
        if dump:
            from PIL import Image
            Image.fromarray(a).save('%s_f%02d.png' % (dump, i))
    return rc, rep


if __name__ == '__main__':
    variant, state, cmd = sys.argv[1:4]
    rc, rep = run(variant, state, cmd, sys.argv[4:], dump=None)
    print(rc)
    for r in rep:
        print(r)
