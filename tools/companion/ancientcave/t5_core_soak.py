#!/usr/bin/env python3
"""T5 soak of the companion core stack on one generated Ancient Cave ROM (libretro level, headless).

Three cores run in lockstep from reset with the same scripted input:
  A  unmodified Snes9x 1.63 (reference)
  W  widescreen core, 46 px, hires=native, HUD kept
  H  widescreen core copy, 27 px, hires=average, HUD hidden except in the ring
Checks per frame: the centre of W and H equals the reference picture (the centre contract of the widescreen core),
frame widths are legal, WRAM party and enemy records stay sane; options are toggled at run time.  The phases cover the
name entry, floor 1, ring menu tours, an enemy map reached by the game's own warp event, and map changes.

    t5_core_soak.py PRESET [--frames N] [--out DIR]

PRESET is an id from tools/companion/randomizer/presets.json (ROM read from $T5/roms/PRESET.sfc).  Nothing is written
inside the repository; the core copies and results go to $T5 (default ~/.local/state/som-decomp/t5-ac-20261018).
"""
import json
import os
import pathlib
import random
import shutil
import subprocess
import sys
import time

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'widescreen'))
sys.path.insert(0, str(HERE.parent))
import wscore  # noqa: E402

T5 = pathlib.Path(os.environ.get('T5', os.path.expanduser('~/.local/state/som-decomp/t5-ac-20261018')))
HIRES_CORE = T5 / 'core/snes9x_widescreen_hires_libretro.so'
HUD_ROWS = 182     # the Ancient Cave floor line (BG3 row 23, scrolled by one line) starts at picture row 183; the centre is compared above it
LEGAL_W = {256, 290, 310, 348, 512}


class Rig:
    def __init__(self, rom):
        h2 = T5 / 'core/snes9x_widescreen_hires_h_libretro.so'
        if not h2.exists():
            shutil.copy(HIRES_CORE, h2)
        self.a = wscore.WS(rom, wscore.CORE_PLAIN)
        self.w = wscore.WS(rom, HIRES_CORE, {'snes9x_som_widescreen': '46', 'snes9x_som_hires': 'native', 'snes9x_som_hud': 'off'})
        self.h = wscore.WS(rom, h2, {'snes9x_som_widescreen': '27', 'snes9x_som_hires': 'average', 'snes9x_som_hud': 'hide_except_ring'})
        self.cores = (self.a, self.w, self.h)
        self.w_opts = {'snes9x_som_widescreen': '46', 'snes9x_som_hires': 'native', 'snes9x_som_hud': 'off'}
        self.frames = 0
        self.stat = dict(frames=0, w_identity_bad=0, h_identity_bad=0, width_illegal=0, hires_frames=0, w_hires_bad=0, h_hires_bad=0,
                         skipped_shape=0, widths_w={}, widths_h={}, wram_bad=[], examples=[])

    def frame(self, mask=0):
        for c in self.cores:
            c.set_pad(mask)
            c.step()
        self.frames += 1
        self.check_pictures()

    def run(self, n, buttons=None):
        mask = self.a.mask(buttons)
        for _ in range(n):
            self.frame(mask)

    def tap(self, btn, hold=5, wait=10):
        self.run(hold, btn)
        self.run(wait)

    def write_all(self, addr, data):
        for c in self.cores:
            c.write(addr, data)

    def u8(self, a):
        return self.a.u8(a)

    def u16(self, a):
        return self.a.u16(a)

    def set_w(self, key, value):
        self.w_opts[key] = value
        self.w.set_option(key, value)

    def centre_ok(self, X, A, native, rows):
        """True when X shows the reference picture A in its centre (pair average for hi-res frames unless native)."""
        if A.shape[1] == 512:
            if native:
                return X.shape == A.shape and not (X[:rows] != A[:rows]).any()
            A = wscore.merge_pairs(A)
        n = (X.shape[1] - 256) // 2
        return X.shape[1] >= 256 and not (X[:rows, n:n + 256] != A[:rows]).any()

    def check_pictures(self):
        s = self.stat
        s['frames'] += 1
        A, W, H = (wscore.raw16(c) for c in (self.a, self.w, self.h))
        for name, d in (('widths_w', W.shape[1]), ('widths_h', H.shape[1])):
            s[name][str(d)] = s[name].get(str(d), 0) + 1
        if W.shape[1] not in LEGAL_W or H.shape[1] not in LEGAL_W or W.shape[0] != A.shape[0] or H.shape[0] != A.shape[0]:
            s['width_illegal'] += 1
            return
        hires = A.shape[1] == 512
        s['hires_frames'] += hires
        native = self.w_opts['snes9x_som_hires'] == 'native'
        rows_w = HUD_ROWS if self.w_opts['snes9x_som_hud'] != 'off' else A.shape[0]
        if hires and native and W.shape[1] != 512:
            s['w_hires_bad'] += 1
            self.example('w_hires_width', W.shape, A.shape)
        elif not self.centre_ok(W, A, native, rows_w):
            s['w_hires_bad' if hires else 'w_identity_bad'] += 1
            self.example('w_hires' if hires else 'w_identity', W.shape, A.shape, opts=dict(self.w_opts))
        if not self.centre_ok(H, A, False, HUD_ROWS):
            s['h_hires_bad' if hires else 'h_identity_bad'] += 1
            self.example('h_hires' if hires else 'h_identity', H.shape, A.shape, ring=self.u8(0x7D))

    def example(self, kind, *shapes, **kw):
        if len(self.stat['examples']) < 12:
            self.stat['examples'].append(dict(kind=kind, frame=self.frames, shapes=[list(x) for x in shapes], map=self.u8(0xDC), d0=self.u8(0xD0), **kw))

    def invariants(self, tag):
        bad = []
        for s in range(6):
            b = 0xE000 + 0x200 * s
            act = self.u8(b)
            if s < 3 and act not in (0, 1):
                bad.append('hero slot %d active byte %d' % (s, act))
            if act == 1 and s >= 3 and self.u16(b + 0x182) > self.u16(b + 0x184) + 1 and self.u16(b + 0x184):
                bad.append('enemy %d hp %d > max %d' % (s, self.u16(b + 0x182), self.u16(b + 0x184)))
            if act == 1 and s < 3:
                hp, mx = self.u16(b + 0x182), self.u16(b + 0x184)
                if mx == 0 or mx > 999 or hp > mx:
                    bad.append('hero %d hp %d max %d' % (s, hp, mx))
                if self.u8(b + 0x187) > 99 or self.u8(b + 0x186) > self.u8(b + 0x187):
                    bad.append('hero %d mp %d max %d' % (s, self.u8(b + 0x186), self.u8(b + 0x187)))
        if bad and len(self.stat['wram_bad']) < 20:
            self.stat['wram_bad'].append(dict(frame=self.frames, tag=tag, issues=bad))


def warp(rig, n):
    ev = bytes([0x18 | (n >> 8), n & 0xFF, 8])
    rig.write_all(0x1FF00, ev)
    rig.write_all(0xD1, b'\x00\xff\x7f')
    rig.frame(0)
    rig.write_all(0xD0, b'\x01')


def boot(rig, heroes):
    rig.run(300)
    rig.tap('START', 5, 410)
    rig.tap('B', 5, 300)
    for i in range(heroes):
        for _ in range(i):
            rig.run(4, 'RIGHT')
            rig.run(30)
        rig.run(5, 'B')
        rig.run(60)
        rig.run(5, 'START')
        rig.run(400)
    quiet = 0
    for _ in range(40):
        rig.run(4, 'B')
        rig.run(60)
        quiet = quiet + 1 if rig.u8(0xD0) == 0 else 0
        if quiet >= 3:
            break
    rig.run(120)
    return rig.u8(0xD0) == 0 and rig.u8(0xE000) == 1


def top_up(rig, heroes):
    for s in range(heroes):
        b = 0xE000 + 0x200 * s
        mx = rig.u16(b + 0x184)
        if mx:
            rig.write_all(b + 0x182, mx.to_bytes(2, 'little'))
            mp = rig.u8(b + 0x187)
            rig.write_all(b + 0x186, bytes([mp]))


def walk_phase(rig, rnd, frames, heroes, tag, ring_prob=0.0):
    names = ['UP', 'DOWN', 'LEFT', 'RIGHT'] * 5 + ['B', 'B,UP', 'B,DOWN', 'A,LEFT', 'A,RIGHT', 'UP,RIGHT', 'DOWN,LEFT', 'X']
    done = 0
    while done < frames:
        if rig.u8(0x7D) == 1 and rnd.random() < 0.6:
            btn = rnd.choice(['B', 'A', 'UP', 'DOWN', 'LEFT', 'RIGHT'])
        elif rnd.random() < ring_prob:
            btn = 'Y'
        else:
            btn = rnd.choice(names)
        n = rnd.randint(6, 40)
        rig.run(n, btn)
        done += n
        if done % 30 < n:
            rig.invariants(tag)
        if done % 240 < n:
            top_up(rig, heroes)


def main():
    a = sys.argv
    preset = a[1]
    frames = int(a[a.index('--frames') + 1]) if '--frames' in a else 20500
    out = pathlib.Path(a[a.index('--out') + 1]) if '--out' in a else T5 / 'out'
    out.mkdir(parents=True, exist_ok=True)
    heroes = 1 if preset.endswith('hero') else 3
    rom = T5 / 'roms' / (preset + '.sfc')
    wr = subprocess.run([sys.executable, str(HERE / 't5_warps.py'), preset], capture_output=True, text=True)
    warps = json.loads([l for l in wr.stdout.splitlines() if l.startswith('{')][-1])
    enemy_maps = (warps['enemy'][:2] or [0])
    quiet_maps = warps['quiet'][:3]
    rnd = random.Random(sum(map(ord, preset)))
    t0 = time.time()
    rig = Rig(rom)
    res = dict(preset=preset, rom=str(rom), heroes_expected=heroes)
    ok = boot(rig, heroes)
    act = [rig.u8(0xE000 + 0x200 * i) for i in range(3)]
    res['boot'] = dict(free_play=ok, frames=rig.frames, map=rig.u8(0xDC), d0=rig.u8(0xD0), actives=act, width_w=rig.w.width, width_h=rig.h.width,
                       hp=[rig.u16(0xE182 + 0x200 * i) for i in range(3)], mp=[rig.u8(0xE186 + 0x200 * i) for i in range(3)])
    res['boot']['first_floor'] = ok and rig.u8(0xDC) == 0 and act == ([1, 0, 0] if heroes == 1 else [1, 1, 1])
    boot_frames = rig.frames
    # phase 1: floor 1
    walk_phase(rig, rnd, frames * 3 // 10, heroes, 'floor1', 0.01)
    # phase 2: ring tours with option toggles
    for k in range(12):
        rig.tap('Y', 4, 30)
        for _ in range(rnd.randint(2, 6)):
            rig.tap(rnd.choice(['UP', 'DOWN', 'LEFT', 'RIGHT', 'X']), 3, 22)
        for _ in range(4):
            rig.tap('B' if k % 2 else 'A', 3, 25)
            if rig.u8(0x7D) == 0:
                break
        rig.invariants('ring')
    rig.run(60)
    # run-time option toggles on the wide cores (a legal width must follow, the centre must stay identical)
    for opt in (('snes9x_som_widescreen', '17'), ('snes9x_som_widescreen', 'disabled'), ('snes9x_som_widescreen', '46'),
                ('snes9x_som_hud', 'hide_except_ring'), ('snes9x_som_hud', 'off'), ('snes9x_som_hires', 'average'), ('snes9x_som_hires', 'native')):
        rig.set_w(*opt)
        rig.run(90)
    # phase 3: enemy map by the game's own warp event
    enemy_ok = {}
    for n in enemy_maps:
        warp(rig, n)
        rig.run(300)
        en = []
        for s in (3, 4, 5):
            b = 0xE000 + 0x200 * s
            en.append(dict(slot=s, active=rig.u8(b), id=rig.u8(b + 0x180), hp=rig.u16(b + 0x182), hp_max=rig.u16(b + 0x184), x=rig.u16(b + 0x20), y=rig.u16(b + 0x22)))
        enemy_ok['warp%d' % n] = dict(map=rig.u8(0xDC), enemies=en, pass_=any(e['active'] == 1 and e['hp'] > 0 and e['hp'] <= e['hp_max'] for e in en))
        walk_phase(rig, rnd, frames * 2 // 10, heroes, 'enemy_warp%d' % n)
        en2 = [rig.u16(0xE182 + 0x200 * s) for s in (3, 4, 5)]
        enemy_ok['warp%d' % n]['hp_after'] = en2
    res['warps'] = dict(enemy=warps['enemy'], quiet=warps['quiet'][:10], stuck=warps['stuck'])
    res['enemy_maps'] = enemy_ok
    # phase 4: map changes (doors) and random walk
    for door in quiet_maps + enemy_maps[:1]:
        warp(rig, door)
        rig.run(200)
        walk_phase(rig, rnd, frames // 20, heroes, 'door%d' % door, 0.01)
    walk_phase(rig, rnd, max(0, frames - (rig.frames - boot_frames)), heroes, 'tail', 0.005)
    res['soak_frames'] = rig.frames - boot_frames
    res['total_frames'] = rig.frames
    res['stat'] = rig.stat
    res['seconds'] = round(time.time() - t0, 1)
    s = rig.stat
    res['pass'] = bool(res['boot']['first_floor'] and res['soak_frames'] >= 20000 and not s['width_illegal'] and not s['w_identity_bad']
                       and not s['h_identity_bad'] and not s['w_hires_bad'] and not s['h_hires_bad'] and not s['wram_bad'])
    p = out / ('core_soak_%s.json' % preset)
    p.write_text(json.dumps(res, indent=1))
    print(json.dumps({k: res[k] for k in ('preset', 'pass', 'total_frames', 'soak_frames', 'seconds')}), json.dumps(res['boot']))
    print(json.dumps({k: v for k, v in s.items() if k != 'examples'}))
    for e in s['examples'][:6]:
        print('  example', e)
    return 0 if res['pass'] else 1


if __name__ == '__main__':
    sys.exit(main())
