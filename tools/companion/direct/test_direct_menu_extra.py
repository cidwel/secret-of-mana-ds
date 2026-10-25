#!/usr/bin/env python3
"""Direct menu jumps: retargeting an open ring, rapid sequences, refusals and frame-sequence sheets.

    python3 test_direct_menu_extra.py VARIANT [OUTDIR]
"""
import json
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest  # noqa: E402

RES = []


def check(name, ok, **kw):
    r = dict(check=name, ok=bool(ok), **kw)
    RES.append(r)
    print(json.dumps(r), flush=True)


def run(D, limit=700):
    n = 0
    while D.state == 'running' and n < limit:
        D.frame(); n += 1
    return n


def black(c):
    a = np.frombuffer(c.frame_rgb, np.uint8).reshape(c.height, -1)
    return int((a.max(axis=1) == 0).sum())


def main(variant, outdir=None):
    D = dtest.Direct(variant)
    c = D.core
    st = dtest.STATES[variant]
    D.load(st, 4)
    out = pathlib.Path(outdir) if outdir else None
    if out:
        out.mkdir(parents=True, exist_ok=True)

    # 1. retarget an open ring: weapon ring of the boy, then the magic list of the sprite, then the item ring of the girl
    D.load(st, 4)
    check('open-weapon-boy', D.open_menu(1, 0, 3) == 'ok'); run(D)
    ok1 = D.state == 'finished' and (c.u8(0x1809), c.u8(0x1808)) == (3, 0)
    check('weapon-ring-boy', ok1, cat=c.u8(0x1809), owner=c.u8(0x1808))
    seen = set()
    check('retarget-spirit-sprite', D.open_menu(2, 2, 3) == 'ok')
    frames = []
    while D.state == 'running' and len(frames) < 700:
        D.frame()
        if c.u8(0x7D) == 1 and c.u8(0x1812) == 0 and c.u8(0x1826) == 1 and c.u8(0x1809) != 0x55:
            seen.add((c.u8(0x1809), c.u8(0x1808)))
        frames.append(D.hidden())
    ok2 = D.state == 'finished' and c.u8(0x1808) == 2 and c.u8(0x180B) == 3 and (c.u8(0x1803) & 0x80)
    check('spirit-list-sprite', ok2, rings_seen=sorted(seen), frames=D.frames, hidden_all=all(frames[:-1]), sel=hex(c.u16(0x1839)))
    check('retarget-item-girl', D.open_menu(1, 1, 0) == 'ok'); run(D)
    check('item-ring-girl', D.state == 'finished' and (c.u8(0x1809), c.u8(0x1808)) == (0, 1), cat=c.u8(0x1809), owner=c.u8(0x1808))
    check('close', D.close_menu() == 'ok'); run(D)
    check('closed', D.state == 'finished' and c.u8(0x7D) == 0 and c.u8(0xCC) & 6 == 0)

    # 2. rapid sequence open -> close -> open -> close with the pad walking in between
    D.load(st, 4)
    good = True
    for k, (kind, hero, arg) in enumerate(((1, 1, 3), (1, 2, 1), (2, 1, 2), (3, 0, 2), (1, 0, 0))):
        rc = D.open_menu(kind, hero, arg); run(D)
        good &= rc == 'ok' and D.state == 'finished'
        D.close_menu(); run(D)
        good &= D.state == 'finished' and c.u8(0x7D) == 0
        for _ in range(8):
            D.frame(1 << 7)
    check('rapid-sequence', good, ring=c.u8(0x7D), d9=c.u8(0xD9))

    # 3. refusals
    D.load(st, 4)
    check('refuse-boy-magic', D.open_menu(1, 0, 1) == 'bad-argument')
    check('refuse-boy-spirit', D.open_menu(2, 0, 1) == 'bad-argument')
    check('refuse-bad-page', D.open_menu(3, 0, 9) == 'bad-argument')
    check('refuse-bad-kind', D.open_menu(7, 0, 0) == 'bad-argument')
    c.write(0xE200 + 0x1C8 + 5, bytes([0]))
    check('refuse-unowned-spirit', D.open_menu(2, 1, 5) == 'not-owned')
    D.load(st, 4)
    c.write(0xD0, bytes([0x81]))
    check('refuse-event', D.open_menu(1, 0, 3) == 'unsafe-state')
    D.load(st, 4)
    c.write(0xE6, bytes([5]))
    check('refuse-fade', D.open_menu(1, 0, 3) == 'unsafe-state')
    D.load(st, 4)
    c.write(0xE200 + 0x190, bytes([0, 0x80]))           # girl dead
    check('refuse-dead-hero', D.open_menu(1, 1, 3) == 'bad-argument')
    D.load(st, 4)
    check('busy', D.open_menu(1, 0, 3) == 'ok' and D.open_menu(1, 0, 2) == 'busy' and D.close_menu() == 'busy')
    run(D)
    # a ring still animating (not idle) is not retargetable until it is idle: the command runs on after the open finished
    D.load(st, 4)
    D.close_menu()
    check('close-when-closed', D.state == 'finished' and D.err == 'ok')
    # an event that starts while the menu command runs ends it cleanly
    D.load(st, 4)
    D.open_menu(1, 0, 3)
    for _ in range(5):
        D.frame()
    c.write(0xD0, bytes([0x81]))
    run(D, 300)
    check('event-during-open', D.state in ('failed', 'finished'), state=D.state, err=D.err, edge=c.u8(0xCC) & 6)

    # 4. frame sequence sheets: all frames (hidden ones marked) of one open and one close
    if out:
        from PIL import Image, ImageDraw
        D.load(st, 4)
        D.open_menu(2, 1, 3)
        shots = []
        n = 0
        while D.state == 'running' and n < 300:
            D.frame()
            shots.append((n, D.hidden(), Image.frombytes('RGB', (c.width, c.height), c.frame_rgb).copy(), black(c)))
            n += 1
        idx = [0, 3, 6, 9, 14, 25, 40, 48, 55, len(shots) - 1]
        w, h = 128, 112
        sheet = Image.new('RGB', (w * 5, (h + 12) * 2))
        dr = ImageDraw.Draw(sheet)
        for k, i in enumerate(idx):
            i = min(i, len(shots) - 1)
            f, hid, im, bl = shots[i]
            x, y = (k % 5) * w, (k // 5) * (h + 12)
            sheet.paste(im.resize((w, h)), (x, y + 12))
            dr.text((x + 2, y), 'f%d %s black%d' % (f, 'HIDDEN' if hid else 'SHOWN', bl), fill=(255, 255, 0))
        sheet.save(out / ('%s_open_sequence.png' % variant))
        pres = [s for s in shots if not s[1]]
        check('sheet', len(pres) == 1, shown=[s[0] for s in pres], hidden=sum(1 for s in shots if s[1]), black_in_hidden=max(s[3] for s in shots if s[1]))
    return all(r['ok'] for r in RES)


if __name__ == '__main__':
    ok = main(*sys.argv[1:3])
    (dtest.STATE / ('menu_extra_%s.json' % sys.argv[1])).write_text(json.dumps(RES, indent=1))
    sys.exit(0 if ok else 1)
