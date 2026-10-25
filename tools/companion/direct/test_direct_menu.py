#!/usr/bin/env python3
"""Direct menu jumps: every ring, spirit list and config page the quick slots can bind, on one ROM variant.

    python3 test_direct_menu.py VARIANT [STATE] [OUTDIR]

Per case: open the menu with somd_open_menu, run frames until the command finishes, record per frame whether the host may
present it (somd_hide_video) and what the ring state was; check that every PRESENTED frame is either field or the final menu
and has no black rows, that no ring with another category/owner ever existed (even in hidden frames), then close with
somd_close_menu and check the field is consistent (no stuck flags, the pad moves the hero, the ring can be reopened).
"""
import json
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest  # noqa: E402

RIGHT = 1 << 7
OK = []


def black_rows(c):
    a = np.frombuffer(c.frame_rgb, np.uint8).reshape(c.height, -1)
    return [i for i in range(c.height) if a[i].max() == 0]


def hero_x(c):
    return c.u16(0xE000 + 0x02)


def cases(D):
    c = D.core
    out = []
    for hero in (0, 1, 2):
        for cat in (0, 2, 3):
            out.append(('ring', 1, hero, cat))
        if hero:
            out.append(('ring', 1, hero, 1))
            owned = []
            for b in range(8):
                if (hero == 1 and b == 6) or (hero == 2 and b == 7):
                    continue
                if c.u8(0xE000 + 0x200 * hero + 0x1C8 + b):
                    owned.append(b)
            for b in owned:
                out.append(('spirit', 2, hero, b))
    for hero in (0, 1):
        for e in range(7):
            out.append(('page', 3, hero, e))
    return out


def run_case(D, state, name, kind, hero, arg, outdir):
    c = D.core
    D.load(state, 4)
    res = dict(case='%s h%d a%d' % (name, hero, arg))
    seen = set()
    presented = []
    rc = D.open_menu(kind, hero, arg)
    res['open_rc'] = rc
    if rc != 'ok':
        return res
    base_black = len(black_rows(c))     # rows that are black in the field itself (AC dungeon frame)
    for i in range(500):
        D.frame()
        if c.u8(0x7D) == 1 and c.u8(0x1812) == 0 and c.u8(0x1809) != 0x55 and c.u8(0x1826) == 1:
            seen.add((c.u8(0x1809), c.u8(0x1808), c.u8(0x1803) & 0xE8))
        if not D.hidden():
            presented.append((i, max(0, len(black_rows(c)) - base_black), c.u8(0x7D)))
        if D.state != 'running':
            break
    res.update(state=D.state, err=D.err, frames=D.frames, presented=presented[:6], cat=c.u8(0x1809), owner=c.u8(0x1808),
               count=c.u8(0x180B), flags=c.u8(0x1803), sel=hex(c.u16(0x1839)), pad_hero=c.u8(0xD9),
               rings_seen=sorted(seen), black_presented=sum(p[1] for p in presented))
    if outdir:
        from PIL import Image
        Image.frombytes('RGB', (c.width, c.height), c.frame_rgb).save('%s/%s_%s_h%d_a%d.png' % (outdir, D.variant, name, hero, arg))
    ok = D.state == 'finished'
    if ok and kind == 1:
        ok = c.u8(0x1809) == arg and c.u8(0x1808) == hero and c.u8(0x180B) > 0
    if ok and kind == 2:
        ok = c.u8(0x1808) == hero and c.u8(0x180B) == 3 and (c.u8(0x1803) & 0x80) != 0
        spell = c.u8(0x1839)
        res['spell_low'] = spell
    # no ring of another category or owner at any frame, hidden or not
    want_cat = arg if kind == 1 else (1 if kind == 2 else None)
    stray = [s for s in seen if s[1] != hero or (want_cat is not None and s[0] != want_cat and not (kind == 3 and s[0] in (2, 5, 3)))]
    res['stray_rings'] = stray
    ok = ok and not stray and res['black_presented'] == 0 and all(p[2] == 1 or p[2] == 0 for p in presented)
    # presented frames: only the final one may show the menu
    ok = ok and len([p for p in presented if p[2] == 1 and p[0] != presented[-1][0]]) == 0
    # close
    crc = D.close_menu()
    for i in range(500):
        D.frame()
        if D.state != 'running':
            break
    res.update(close_rc=crc, close_state=D.state, close_err=D.err, close_frames=D.frames)
    flags = dict(ring=c.u8(0x7D), active=c.u8(0x1826), phase=c.u8(0x1812), f1803=c.u8(0x1803), f1804=c.u8(0x1804), a1847=c.u8(0x1847),
                 edge=c.u8(0xCC) & 0x06, event=c.u8(0xD0), fade=(c.u8(0xE4), c.u8(0xE6)))
    res['after_close'] = flags
    ok = ok and D.state == 'finished' and flags['ring'] == 0 and flags['edge'] == 0 and flags['event'] == 0 and flags['fade'] == (1, 15)
    # input works: walk right
    x0 = c.u16(0xE000 + 0x200 * c.u8(0xD9).bit_length() - 0x200 + 2) if c.u8(0xD9) else 0
    for _ in range(30):
        D.frame(RIGHT)
    x1 = c.u16(0xE000 + 0x200 * (c.u8(0xD9).bit_length() - 1) + 2)
    res['moved'] = x1 != x0
    ok = ok and res['moved']
    # reopen with the plain Y button works
    for _ in range(3):
        D.frame(1 << 1)
    for _ in range(70):
        D.frame()
    res['reopen'] = c.u8(0x7D) == 1 and c.u8(0x1812) == 0
    ok = ok and res['reopen']
    res['ok'] = bool(ok)
    return res


def main(variant, state=None, outdir=None):
    D = dtest.Direct(variant)
    D.fast = True          # this test measures the optional hidden fast mode; the default is real time (no hidden frames)
    st = state or dtest.STATES[variant]
    D.load(st, 4)
    if outdir:
        pathlib.Path(outdir).mkdir(parents=True, exist_ok=True)
    allc = cases(D)
    results = []
    for name, kind, hero, arg in allc:
        r = run_case(D, st, name, kind, hero, arg, outdir)
        results.append(r)
        print(json.dumps(r), flush=True)
    # page 1 (Targeting) with no enemy on screen is refused with no-target by design
    bad = [r['case'] for r in results if not r.get('ok') and not (r['case'].startswith('page') and r['case'].endswith('a1') and r.get('err') == 'no-target' and r.get('close_state') == 'finished' and r.get('moved') and r.get('reopen'))]
    print('cases', len(results), 'failed', bad)
    (dtest.STATE / ('menu_%s.json' % variant)).write_text(json.dumps(dict(variant=variant, results=results, failed=bad), indent=1))
    return not bad


if __name__ == '__main__':
    sys.exit(0 if main(*sys.argv[1:4]) else 1)
