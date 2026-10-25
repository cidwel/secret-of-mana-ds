#!/usr/bin/env python3
"""Round 3 end to end on the real US ROM and the stock core (headless): the bottom screen through a new game.

    python3 test_round3_flow.py new      # title, name entry typed by taps (direct text), intro: blank until the first sword
    python3 test_round3_flow.py route    # the TAS input route up to the sword scene: the screen turns on when $CF00 is set
Writes PNGs to <state>/shots/ux/r3flow/ and exits 0 when every check holds.  Nothing from the ROM is stored."""
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parent))
sys.path.insert(0, str(HERE.parent / 'direct'))
import numpy as np  # noqa: E402
import common  # noqa: E402
import retro  # noqa: E402
import ux_lib  # noqa: E402

OUT = pathlib.Path('/home/cidwel/.local/state/som-decomp/companion-20261018/shots/ux/r3flow')
OUT.mkdir(parents=True, exist_ok=True)
FAILS = []
DEV = (1240, 970)


def check(name, ok, info=''):
    print(('PASS ' if ok else 'FAIL ') + name + (' ' + str(info) if info else ''))
    if not ok:
        FAILS.append(name)


def lit(u):
    im, raw = u.draw(*DEV)
    return int((np.asarray(im).sum(axis=2) > 0).sum()), im


def phase_new():
    import dtest
    D = dtest.Direct('vanilla')
    c = D.core
    u = ux_lib.UX(rom=common.ROM_VANILLA)
    log = []

    def step(n=1, btn=None):
        for _ in range(n):
            c.set_pad(retro.Core.mask(btn)); D.frame(0) if False else c.step()
            u.update(c.dump_wram() if False else bytes(c.wram[0:0x20000]))

    step(300); step(6, ['START']); step(400); step(6, ['B']); step(300)
    names = ['ADAM']
    shot = False
    for who, nm in enumerate(names):
        for _ in range(900):
            if u.lib.som_ux_info(u.p, 0) is not None and bytes(c.wram[0x9C00:0x9C01]) != b'\xf8' and c.u8(0xA1CC) == 0 and c.u8(0x9C01) < 0xE0:
                break
            step(1)
        step(20)
        n, im = lit(u)
        check('name entry %d draws the keyboard' % who, n > 1000, n)
        if who == 0:
            im.save(OUT / 'name_entry_before.png')
        for ch in nm:
            cen = u.center(*DEV, 'LETTER', ord(ch) - 65)
            r, act = u.touch(cen[0], cen[1], *DEV)
            check('tap %s -> NAME_LETTER' % ch, r == 1 and act['kind'] == 'NAME_LETTER' and act['letter'] == ord(ch) - 65)
            e = D.name_letter(act['letter'])
            check('direct write %s' % ch, e == 'ok', e)
            step(3)
        if who == 0:
            step(6)
            c.save_ppm(str(OUT / 'name_entry_top_screen.ppm'))
            from PIL import Image
            Image.open(OUT / 'name_entry_top_screen.ppm').save(OUT / 'name_entry_top_screen.png')
            u.draw(*DEV)[0].save(OUT / 'name_entry_after.png')
        cen = u.center(*DEV, 'LETTER', 26)
        r, act = u.touch(cen[0], cen[1], *DEV)                       # DEL
        check('DEL tap -> NAME_DELETE', act['kind'] == 'NAME_DELETE')
        check('direct delete', D.name_delete() == 'ok')
        step(3)
        check('letter count %d' % len(nm), c.u8(0xA1CC) == len(nm) - 1, c.u8(0xA1CC))
        D.name_letter(ord(nm[-1]) - 65); step(3)
        step(30, ['START']); step(120)
    glyph = lambda s: bytes(0x9B + ord(x) - 65 for x in s)
    for who, nm in enumerate(names):
        got = c.read(0xCC00 + 12 * who, len(nm) + 1)
        check('hero %d name is %s' % (who, nm), got[:len(nm)] == glyph(nm), got.hex())
    # intro: every sampled frame is blank until the sword flag
    samples = []
    for f in range(9000):
        step(1, ['B'] if (f % 70) < 3 else None)
        if f % 150 == 0:
            n, im = lit(u)
            samples.append((f, u.classify(bytes(c.wram[0:0x20000])), c.u8(0xCF00), n))
            if f == 3000:
                im.save(OUT / 'intro_blank.png')
        if c.u8(0xCF00):
            break
    blank_ok = all(n == 0 for f, m, cf, n in samples if cf == 0 and m != 'FRONTEND')
    check('intro frames are blank until $CF00 (%d samples)' % len(samples), blank_ok, [s for s in samples if s[3]][:3])
    ev = u.lib.som_ux_game_event(u.p) if hasattr(u.lib, 'som_ux_game_event') else None
    print('samples', samples[:6], '...', samples[-2:])
    return not FAILS


def phase_route():
    sys.path.insert(0, str(HERE.parent / 'direct'))
    ev = []
    for ln in open(common.ROUTE):
        if ln.startswith('#') or not ln.strip():
            continue
        p = ln.strip().split(None, 2)
        ev.append((int(p[0]), int(p[1]), p[2].split(',') if len(p) > 2 else []))
    c = retro.Core(str(common.ROM_VANILLA))
    u = ux_lib.UX(rom=common.ROM_VANILLA)
    first = None
    blank_before = True
    for f in range(14000):
        btn = []
        for a, b, n in ev:
            if a <= f <= b:
                btn += n
        c.set_pad(retro.Core.mask(btn)); c.step()
        w = bytes(c.wram[0:0x20000])
        u.update(w)
        if f % 60 == 0:
            n, im = lit(u)
            if (c.u8(0xCF00) == 0 or f <= 3000) and u.classify(w) != 'FRONTEND' and n:
                blank_before = False
            if c.u8(0xCF00) and first is None and f > 3000:
                first = f
                im.save(OUT / 'route_armed_first_frame.png')
            if f == 6000:
                im.save(OUT / 'route_before_sword.png')
        if first is not None and f % 120 == 0:
            print('post', f - first, u.classify(w), lit(u)[0])
        if first is not None and f >= first + 2400:
            n, im = lit(u)
            im.save(OUT / 'route_after_sword.png')
            check('panel visible after the sword flag', n > 5000, n)
            break
    check('blank from the title to the sword flag', blank_before)
    check('sword flag reached', first is not None, first)
    return not FAILS


if __name__ == '__main__':
    if len(sys.argv) > 1 and sys.argv[1] == 'route':
        ok = phase_route()
    elif len(sys.argv) > 1 and sys.argv[1] == 'new':
        ok = phase_new()
    else:
        ok = subprocess.run([sys.executable, __file__, 'new']).returncode == 0 and subprocess.run([sys.executable, __file__, 'route']).returncode == 0
    print('ALL PASS' if ok else 'FAILED %s' % FAILS)
    sys.exit(0 if ok else 1)
