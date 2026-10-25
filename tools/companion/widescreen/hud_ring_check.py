#!/usr/bin/env python3
"""HUD policy check for snes9x_som_hud=hide_except_ring, three cores in lockstep:
  A  unmodified core                                   (what the game really draws)
  O  unmodified core, BG3 layer off, portrait sprites removed from the OAM staging buffer ($7E:0800)  (oracle "no HUD at all")
  C  widescreen core with the option on (margin 0, or MARGIN px: the centre is compared)
Per frame, with ring = WRAM $7D == 1 (state before and after the step must agree, else the frame is a boundary frame):
  ring frames      C must equal A                                                              -> ring_diff
  other frames     leak    = pixels where C still differs from O although A does (HUD still visible) at rows >= 184
                   removed = pixels where C differs from A although A equals O (a non-HUD pixel was taken away)
Event frames (dialogue/cutscene, $D0 not 0/1) are counted separately for leaks because BG3 dialogue text legitimately stays.
usage: hud_ring_check.py OUT.json walk us|es|ac FRAMES [MARGIN]
       hud_ring_check.py OUT.json maps us|es MAPS...   (warp, 420 frames each: fade out, load, fade in, walk)"""
import json
import random
import sys
import pathlib

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wscore  # noqa: E402
from survey import warp  # noqa: E402
from hud_check import CFG  # noqa: E402
from hud_oracle import hud_slots  # noqa: E402

PLAIN2 = wscore.OUT / 'pc-x86_64/snes9x_plain2_libretro.so'
ROWS_HUD = 184


def removed_chk(A, C, O):
    return bool(((C != A) & (A == O)).any())


class Rig:
    def bg3_rows_nonzero(self):
        """True when BG3 holds anything above the HUD rows (dialogue box text), read from VRAM of the patched core."""
        import ctypes as C_
        self.c.lib.retro_som_regs_get.restype = C_.POINTER(C_.c_uint8)
        base = (self.c.lib.retro_som_regs_get()[0x2109] & 0xFC) << 9
        v = self.c.vram()
        return any((v[base + i * 2] | (v[base + i * 2 + 1] << 8)) & 0x3ff for i in range(23 * 32))

    def __init__(self, rom, margin='disabled'):
        self.a = wscore.WS(rom, wscore.CORE_PLAIN)
        self.o = wscore.WS(rom, PLAIN2, {'snes9x_layer_3': 'disabled'})
        self.c = wscore.WS(rom, wscore.CORE_WS, {'snes9x_som_hud': __import__('os').environ.get('HR_HUD', 'hide_except_ring'), 'snes9x_som_widescreen': margin})
        self.margin = 0 if margin == 'disabled' else int(margin)
        self.r = dict(frames=0, ring=0, ring_diff=0, other=0, leak_frames=0, leak_px=0, removed_frames=0, removed_px=0, boundary=0,
                      event_other=0, event_leak_frames=0, hud_visible_frames_in_ring=0, leak_examples=[], removed_examples=[],
                      portrait_frames=0, bg3_hud_frames=0)

    def load(self, state):
        for c in (self.a, self.o, self.c):
            c.load_state(state)
            c.run(2)

    def each(self, fn):
        for c in (self.a, self.o, self.c):
            fn(c)

    def step(self, mask, tag=''):
        for c in (self.a, self.o, self.c):
            c.set_pad(mask)
        ring_b = self.a.u8(0x7D) == 1
        for i in hud_slots(self.o):
            self.o.wram[0x800 + i * 4 + 1] = 0xE0
        for c in (self.a, self.o, self.c):
            c.step()
        ring_a = self.a.u8(0x7D) == 1
        self.compare(ring_b, ring_a, tag)

    def compare(self, ring_b, ring_a, tag):
        r = self.r
        A, O, C = (wscore.raw16(x) for x in (self.a, self.o, self.c))
        if A.shape != O.shape:
            return
        if C.shape != A.shape:
            if self.margin and C.shape[1] == 256 + 2 * self.margin and A.shape[1] == 256:
                C = C[:, self.margin:self.margin + 256]
            elif A.shape[1] == 512 and C.shape[1] == 256 + 2 * self.margin:
                A, O = wscore.merge_pairs(A), wscore.merge_pairs(O)
                C = C[:, self.margin:self.margin + 256]
            elif A.shape[1] == 512 and C.shape[1] == 256:
                A, O = wscore.merge_pairs(A), wscore.merge_pairs(O)
            else:
                return
        if ring_a != ring_b:
            r['boundary'] += 1
            return
        r['frames'] += 1
        ev = self.a.u8(0xD0) not in (0, 1)
        if ring_a:
            r['ring'] += 1
            if (A != C).any():
                r['ring_diff'] += 1
            return
        r['other'] += 1
        ev_n = 1 if ev else 0
        if ev or self.a.u8(0xD0) == 1:
            g = self.bg3_rows_nonzero()
            if g:
                r['dialog_frames'] = r.get('dialog_frames', 0) + 1
                if removed_chk(A, C, O):
                    r['dialog_removed_frames'] = r.get('dialog_removed_frames', 0) + 1
        r['event_other'] += ev_n
        hudpix = A != O
        leak = (C != O) & hudpix
        leak[:ROWS_HUD] = False
        removed = (C != A) & ~hudpix
        if leak.any():
            if ev:
                r['event_leak_frames'] += 1
            else:
                r['leak_frames'] += 1
                r['leak_px'] += int(leak.sum())
                if len(r['leak_examples']) < 12:
                    r['leak_examples'].append({'tag': tag, 'frame': self.a.frames, 'px': int(leak.sum()), 'E4': self.a.u8(0xE4), 'E6': self.a.u8(0xE6), 'D0': self.a.u8(0xD0), 'bright': -1})
        if removed.any():
            r['removed_frames'] += 1
            r['removed_px'] += int(removed.sum())
            if len(r['removed_examples']) < 12:
                r['removed_examples'].append({'tag': tag, 'frame': self.a.frames, 'px': int(removed.sum()), 'D0': self.a.u8(0xD0)})
        if hudpix.any():
            r['bg3_hud_frames'] += 1


def walk(out, key, nfr, margin):
    rom, st, pre = CFG[key]
    rig = Rig(rom, margin)
    rig.load(open(st, 'rb').read())
    if pre == 'potos':
        survey = json.load(open(wscore.OUT / 'survey.json'))
        for c in (rig.a, rig.o, rig.c):
            warp(c, survey['131']['trans'])
            c.run(300)
    rnd = random.Random(21)
    names = ['UP', 'DOWN', 'LEFT', 'RIGHT'] * 6 + ['B', 'A', 'B,RIGHT', 'A,UP', 'LEFT,DOWN', 'RIGHT,DOWN'] * 2 + ['Y', 'START', 'X']
    f = 0
    while f < nfr:
        btn = rnd.choice(names)
        if rig.a.u8(0x7D) == 1 and rnd.random() < 0.7:
            btn = 'B'
        for _ in range(rnd.randint(8, 40)):
            rig.step(rig.a.mask(btn), key)
            f += 1
    finish(out, rig)


def maps(out, key, ms):
    rom, st, pre = CFG[key]
    state = open(st, 'rb').read()
    survey = json.load(open(wscore.OUT / 'survey.json'))
    rig = Rig(rom)
    rnd = random.Random(5)
    for m in ms:
        rig.load(state)
        if pre == 'potos':
            for c in (rig.a, rig.o, rig.c):
                warp(c, survey['131']['trans'])
                c.run(300)
        for c in (rig.a, rig.o, rig.c):
            warp(c, survey[str(m)]['trans'])
        for f in range(420):
            btn = ('A' if f % 30 < 3 else None) if __import__('os').environ.get('HR_A') else (None if f < 300 else rnd.choice(['RIGHT', 'DOWN', 'LEFT', 'UP']))
            rig.step(rig.a.mask(btn), '%s:%d' % (key, m))
        print(m, {k: v for k, v in rig.r.items() if not k.endswith('examples')}, flush=True)
    finish(out, rig)


def finish(out, rig):
    rig.r['widths'] = sorted(set(rig.c.sizes_set())) if hasattr(rig.c, 'sizes_set') else sorted(set(rig.c.sizes))
    rig.r['panels_seen_frames'] = rig.c.lib.retro_som_hud_panels()
    rig.r['sprite_slots_hidden'] = rig.c.lib.retro_som_hud_hidden()
    json.dump(rig.r, open(out, 'w'), indent=1)
    print(json.dumps({k: v for k, v in rig.r.items() if not k.endswith('examples')}))


if __name__ == '__main__':
    a = sys.argv
    if a[2] == 'walk':
        walk(a[1], a[3], int(a[4]), a[5] if len(a) > 5 else 'disabled')
    else:
        maps(a[1], a[3], [int(x) for x in a[4:]])
