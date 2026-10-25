#!/usr/bin/env python3
"""Regression tests for the widescreen core's side passes (round 3 of the widescreen core, see
docs/companion-widescreen-core-20261017.md, section "Round 3 fixes").  Headless and deterministic.

usage: core_round3_test.py [shake|lead|ring|hold|hires|all] [--core PATH] [--shots DIR] [--margin N]

  shake  screen shake: two cores run in lockstep on the first field map of a new game; the second one gets +-6 px
         added to the BG1/BG2 scroll shadows ($B0/$B2/$B4/$B6) every frame (the game's shake adds to the same scroll
         values while the WRAM camera stays still).  Its frame must equal the unshaken frame shifted by the same amount,
         in the side columns as well as in the centre (before the fix the sides were hidden on 60 % of the frames)
  lead   the same with offsets of up to +-14 px (a camera that is a whole tile row ahead of or behind the scroll
         registers, as when the camera glides): the side columns must not show a wrong row or a wrong column
  ring   the ring menu opened on maps 84 and 347 (the camera glides 14 px per frame during the opening): no side pass
         may be skipped for lack of a valid layer and the side columns never go black
  hold   a momentary mismatch between the game's tilemaps and the map array (3 frames of wiped tilemaps) must keep
         the previous sides instead of black bands, and 5 % wrong cells must still draw (the 2 % limit only decides
         the widen / fall back question)
  hires  new-game flow, frames 0..1300 (title, game select, name entry: the only hi-res frames before the first field):
         the centre equals the stock frame (or its pair average / the native 512 px frame), and in the field the side
         columns are pixel exact (no blending): what the right side shows is found again unchanged in the centre later
Exit status 0 = all selected tests pass.  --core selects another core .so (default wscore.CORE_WS).
"""
import argparse
import ctypes as C
import json
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wscore  # noqa: E402
from survey import warp  # noqa: E402
from PIL import Image  # noqa: E402

OPTS = {'snes9x_som_widescreen': '46', 'snes9x_som_hud': 'hide_except_ring', 'snes9x_som_hires': 'average'}
STATE_FIRST = wscore.WORK / 'ws_r3_firstmap.state'     # first field map after the new-game intro (map 128), made by the plain core
PARTY = wscore.WORK / 'party3_vanilla.state'
M = 46
RES = {}
SHOTS = None


def shot(name, *imgs):
    if SHOTS:
        SHOTS.mkdir(parents=True, exist_ok=True)
        im = np.concatenate([np.asarray(i, np.uint8) for i in imgs], 0)
        Image.fromarray(im).save(str(SHOTS / name))


def script():
    out = []
    for ln in open(wscore.common.NEWGAME):
        if ln.startswith('#') or not ln.strip():
            continue
        a, b, btn = ln.split()
        out.append((int(a), int(b), btn))
    return out


def first_map_state():
    if STATE_FIRST.exists():
        return STATE_FIRST.read_bytes()
    c = wscore.WS(wscore.common.ROM_VANILLA, wscore.CORE_PLAIN)
    sc = script()
    for f in range(7700):
        btn = next((x[2] for x in sc if x[0] <= f < x[1]), None)
        c.set_pad(c.mask(btn))
        c.step()
    assert c.u16(0xDC) == 128, c.u16(0xDC)
    blob = c.save_state()
    STATE_FIRST.write_bytes(blob)
    return blob


def pair(core, opts=None, tag='b'):
    """Two cores of the same build in lockstep (private copies, so no shared globals)."""
    a = wscore.WS(wscore.common.ROM_VANILLA, wscore.copy_core(core, 'a'), opts or OPTS)
    b = wscore.WS(wscore.common.ROM_VANILLA, wscore.copy_core(core, tag), opts or OPTS)
    return a, b


def shifted_diff(b, a, sx, sy, mg=16):
    """Fraction of differing pixels of b[y][x] versus a[y + sy][x + sx] for the left side, the right side and the centre."""
    h, w = a.shape[:2]
    y0, y1, x0, x1 = mg, 190 - 0, mg, w - mg
    d = (np.abs(b[y0:y1, x0:x1] - a[y0 + sy:y1 + sy, x0 + sx:x1 + sx]).sum(2) > 0)
    return d[:, :M - mg].mean(), d[:, -(M - mg):].mean(), d[:, M - mg + 12:M - mg + 12 + 220].mean()


def shake(core, amp, frames=120, name='shake'):
    a, b = pair(core)
    st = first_map_state()
    a.load_state(st)
    b.load_state(st)
    for _ in range(10):
        a.step()
        b.step()
    base = [b.u16(x) for x in (0xB0, 0xB2, 0xB4, 0xB6)]
    seed = 12345 + amp
    worst = (-1.0, None)
    bad = 0
    mg = max(16, amp + 2)
    for k in range(frames):
        seed = (seed * 1103515245 + 12345) & 0x7fffffff
        sx = (seed >> 16) % (2 * amp + 1) - amp
        seed = (seed * 1103515245 + 12345) & 0x7fffffff
        sy = (seed >> 16) % (2 * amp + 1) - amp
        for x, v, d in zip((0xB0, 0xB2, 0xB4, 0xB6), base, (sx, sy, sx, sy)):
            b.write(x, ((v + d) & 0xffff).to_bytes(2, 'little'))
        a.step()
        b.step()
        fa, fb = a.rgb().astype(int), b.rgb().astype(int)
        l, r, c = shifted_diff(fb, fa, sx, sy, mg)
        m = max(l, r)
        if m > 0.02:
            bad += 1
        if m > worst[0]:
            worst = (m, (fa.astype(np.uint8), fb.astype(np.uint8), sx, sy))
    RES['%s_amp%d' % (name, amp)] = dict(amp=amp, frames=frames, bad_frames=bad, worst_side_mismatch=round(float(worst[0]), 4))
    if worst[1] is not None:
        shot('%s_amp%d_worst.png' % (name, amp), worst[1][0], worst[1][1])
    ok = bad == 0
    print('%s amp %d: %d of %d frames with a side mismatch > 2 %% (worst %.3f)  %s' % (name, amp, bad, frames, worst[0], 'PASS' if ok else 'FAIL'))
    return ok


def ring(core):
    survey = json.load(open(wscore.OUT / 'survey.json'))
    party = PARTY.read_bytes()
    ok = True
    c = wscore.WS(wscore.common.ROM_VANILLA, core, OPTS)
    for m in (84, 347):
        c.load_state(party)
        c.run(3)
        warp(c, survey[str(m)]['trans'])
        c.run(300)
        s0 = c.stat()
        skipped, dark, frames = 0, 0, []
        base = c.rgb()[:199]
        black0 = [(base[:, :M].sum(2) == 0).mean(), (base[:, M + 256:].sum(2) == 0).mean()]
        for i in range(90):
            c.set_pad(c.mask('Y') if i < 3 else 0)
            c.step()
            s1 = c.stat()
            if s1[1] != s0[1] or s1[0] != s0[0]:
                skipped += 1
            s0 = s1
            im = c.rgb()
            fr = im[:199]
            for part, b0 in zip((fr[:, :M], fr[:, M + 256:]), black0):
                if (part.sum(2) == 0).mean() > b0 + 0.2:       # more black than before the ring opened (map edges are black already)
                    dark += 1
            frames.append(im)
        good = skipped == 0 and dark == 0
        ok &= good
        RES['ring_%d' % m] = dict(frames_with_skipped_side_pass=skipped, frames_with_black_side=dark)
        shot('ring_map%d_frames2to5.png' % m, np.concatenate(frames[1:5], 1))
        print('ring map %d: %d frames with a skipped side pass, %d with a black side  %s' % (m, skipped, dark, 'PASS' if good else 'FAIL'))
    return ok


def vram_bg(c, bg):
    regs = c.regs(0x2107 + bg, 1)[0]
    return (regs & 0xFC) << 9


def hold(core):
    a, b = pair(core)
    st = first_map_state()
    a.load_state(st)
    b.load_state(st)
    for _ in range(20):
        a.step()
        b.step()
    vp = b.lib.retro_get_memory_data(3)
    bases = [vram_bg(b, 0), vram_bg(b, 1)]
    saved = [bytes(vp[x:x + 0x1000]) for x in bases]
    results = {}
    # (a) three frames of wiped tilemaps, then restored: the sides keep the last good picture (the animated waterfall freezes meanwhile,
    # so b is compared with the frame before the wipe)
    worst_a = 0.0
    for k in range(40):
        if k == 5:
            for x in bases:
                for i in range(0x1000):
                    vp[x + i] = 0
        if k == 8:
            for x, s in zip(bases, saved):
                for i in range(0x1000):
                    vp[x + i] = s[i]
        a.step()
        b.step()
        fa, fb = a.rgb().astype(int), b.rgb().astype(int)
        if k == 4:
            last_good = fa
        if 5 <= k <= 7:
            for sl in (slice(0, M), slice(M + 256, M + 256 + M)):
                worst_a = max(worst_a, (np.abs(last_good[:199, sl] - fb[:199, sl]).sum(2) > 0).mean())
        if k == 6:
            shot('hold_wiped_tilemaps_frame6.png', fa.astype(np.uint8), fb.astype(np.uint8))
    results['wiped_3_frames_side_mismatch'] = round(float(worst_a), 4)
    ok_a = worst_a < 0.02
    # (b) 5 % wrong cells in view for 30 frames: still drawn
    for _ in range(5):
        a.step()
        b.step()
    seed, n = 7, 0
    hits = []
    while n < 40:
        seed = (seed * 1103515245 + 12345) & 0x7fffffff
        r = (seed >> 16) % 24
        seed = (seed * 1103515245 + 12345) & 0x7fffffff
        cc = (seed >> 16) % 32
        hits.append((r, cc))
        n += 1
    for x in bases:
        for r, cc in hits:
            o = x + (r * 32 + cc) * 2
            vp[o] = (vp[o] + 1) & 0xff
    worst_b = 0.0
    for k in range(30):
        a.step()
        b.step()
        fa, fb = a.rgb().astype(int), b.rgb().astype(int)
        for sl in (slice(0, M), slice(M + 256, M + 256 + M)):
            worst_b = max(worst_b, (np.abs(fa[:199, sl] - fb[:199, sl]).sum(2) > 0).mean())
    results['five_percent_wrong_cells_side_mismatch'] = round(float(worst_b), 4)
    ok_b = worst_b < 0.15      # the wrong cells themselves lie in the sides too (the corruption wraps), so allow a few
    shot('hold_5pct_wrong_cells.png', fa.astype(np.uint8), fb.astype(np.uint8))
    RES['hold'] = results
    print('hold: wiped tilemaps for 3 frames, side mismatch %.3f  %s' % (worst_a, 'PASS' if ok_a else 'FAIL'))
    print('hold: 5 %% wrong cells for 30 frames, side mismatch %.3f  %s' % (worst_b, 'PASS' if ok_b else 'FAIL'))
    return ok_a and ok_b


def hires(core):
    plain = wscore.WS(wscore.common.ROM_VANILLA, wscore.CORE_PLAIN)
    avg_o = dict(OPTS, snes9x_som_hud='off')
    nat_o = dict(avg_o, snes9x_som_hires='native')
    avg = wscore.WS(wscore.common.ROM_VANILLA, wscore.copy_core(core, 'avg'), avg_o)
    nat = wscore.WS(wscore.common.ROM_VANILLA, wscore.copy_core(core, 'nat'), nat_o)
    sc = script()
    n_hi = bad = 0
    for f in range(1300):
        btn = next((x[2] for x in sc if x[0] <= f < x[1]), None)
        for c in (plain, avg, nat):
            c.set_pad(c.mask(btn))
            c.step()
        p = wscore.raw16(plain)
        if plain.width == 512:
            n_hi += 1
            ea = wscore.merge_pairs(p)
            ga = wscore.raw16(avg)[:, M:M + 256] if avg.width != 256 else wscore.raw16(avg)
            gn = wscore.raw16(nat)
            good = nat.width == 512 and np.array_equal(gn, p) and np.array_equal(ga, ea)
            if f in (700, 1100):
                shot('hires_frame%d_plain512_avg_native.png' % f, np.asarray(Image.fromarray(plain.rgb()).resize((348, 224), Image.NEAREST)),
                     avg.rgb(), np.asarray(Image.fromarray(nat.rgb()).resize((348, 224), Image.NEAREST)))
        else:
            gA = wscore.raw16(avg)[:, M:M + 256] if avg.width != 256 else wscore.raw16(avg)
            gN = wscore.raw16(nat)[:, M:M + 256] if nat.width != 256 else wscore.raw16(nat)
            good = np.array_equal(gA, p) and np.array_equal(gN, p)
        bad += 0 if good else 1
    ok1 = bad == 0 and n_hi > 100
    print('hires: %d frames, %d hi-res (512 px stock), %d with a centre that is not the stock / averaged / native frame  %s' % (1300, n_hi, bad, 'PASS' if ok1 else 'FAIL'))
    # side exactness on the first field map: walking right, the right side of an earlier frame reappears unchanged in the centre
    c = wscore.WS(wscore.common.ROM_VANILLA, wscore.copy_core(core, 'sides'), avg_o)
    c.load_state(first_map_state())
    fr, hs = [], []
    for i in range(130):
        c.set_pad(c.mask('RIGHT'))
        c.step()
        fr.append(c.rgb().astype(int))
        h = C.c_int()
        v = C.c_int()
        c.lib.retro_som_line_get(100, 0, C.byref(h), C.byref(v))
        hs.append(h.value)
    res = []
    for t, t2 in ((30, 70), (30, 100), (40, 128)):
        d = hs[t2] - hs[t]
        a_, b_ = fr[t][8:190], fr[t2][8:190]
        xs = [X for X in range(256, 256 + M) if 0 <= X - d < 256]
        res.append(float(np.mean([(a_[:, M + X] == b_[:, M + X - d]).all(1).mean() for X in xs])))
    RES['hires'] = dict(frames=1300, hires_frames=n_hi, centre_bad=bad, side_exact_fraction=[round(x, 3) for x in res])
    ok2 = min(res) > 0.9
    print('hires: right side predicted exactly %s of its pixels (no blending, static background)  %s' % ([round(x, 3) for x in res], 'PASS' if ok2 else 'FAIL'))
    shot('hires_first_map_sides.png', fr[30].astype(np.uint8), fr[100].astype(np.uint8))
    return ok1 and ok2


def main():
    global SHOTS, M
    ap = argparse.ArgumentParser()
    ap.add_argument('tests', nargs='?', default='all')
    ap.add_argument('--core', default=str(wscore.CORE_WS))
    ap.add_argument('--shots')
    ap.add_argument('--json')
    a = ap.parse_args()
    SHOTS = pathlib.Path(a.shots) if a.shots else None
    sel = a.tests
    ok = True
    if sel in ('shake', 'all'):
        r4 = shake(a.core, 4, name='shake')
        r6 = shake(a.core, 6, name='shake')
        ok &= r4 and r6
    if sel in ('lead', 'all'):
        ok &= shake(a.core, 14, name='lead')
    if sel in ('ring', 'all'):
        ok &= ring(a.core)
    if sel in ('hold', 'all'):
        ok &= hold(a.core)
    if sel in ('hires', 'all'):
        ok &= hires(a.core)
    if a.json:
        json.dump(RES, open(a.json, 'w'), indent=1)
    print('ALL PASS' if ok else 'FAILED')
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
