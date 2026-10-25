#!/usr/bin/env python3
"""Geometry contract test: random buttons (walk, attack, ring menu, START status screen, dialogue) with the
widescreen option on.  Every change of the frame width must be a SET_GEOMETRY issued by the core, must follow a
change of the map id ($DC) and must never happen in dialogs/menus.  Also toggles the option mid-run.
usage: geo_mash.py OUT.json us|es|ac FRAMES MARGIN"""
import json
import random
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wscore  # noqa: E402
from hud_check import CFG  # noqa: E402
from survey import warp  # noqa: E402


def main():
    out, key, nfr, margin = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
    rom, st, pre = CFG[key]
    c = wscore.WS(rom, wscore.CORE_WS, {'snes9x_som_widescreen': margin})
    c.load_state(open(st, 'rb').read())
    survey = json.load(open(wscore.OUT / 'survey.json'))
    maps = [m for m, i in survey.items() if i.get('ok')]
    rnd = random.Random(11)
    names = ['UP', 'DOWN', 'LEFT', 'RIGHT', 'UP', 'DOWN', 'LEFT', 'RIGHT', 'B', 'A', 'A', 'B,RIGHT', 'Y', 'START', 'A,UP', 'LEFT,DOWN']
    widths, dcs, flags = [], [], []
    toggles = {nfr // 2: 'disabled', nfr // 2 + 150: '46', nfr // 2 + 300: margin}
    f = 0
    last_dc = c.u16(0xDC)
    last_dc_frame = -10 ** 6
    last_toggle = -10 ** 6
    changes = []
    prev_w = None
    ring_frames = event_frames = 0
    while f < nfr:
        btn = rnd.choice(names)
        for _ in range(rnd.randint(8, 40)):
            if f >= nfr:
                break
            if f in toggles:
                c.set_option('snes9x_som_widescreen', toggles[f])
                last_toggle = f
            if f and f % 900 == 0:
                for _k in range(60):
                    if c.u8(0x7D) == 0 and c.u8(0xD0) == 0:
                        break
                    c.set_pad(c.mask('B')); c.step(); f += 1
                warp(c, survey[rnd.choice(maps)]['trans'])
            c.set_pad(c.mask(btn))
            c.step()
            f += 1
            dc = c.u16(0xDC)
            if dc != last_dc:
                last_dc, last_dc_frame = dc, f
            w = c.width
            ring = c.u8(0x7D) == 1
            ev = c.u8(0xD0) not in (0, 1)
            ring_frames += ring
            event_frames += ev
            if prev_w is not None and w != prev_w:
                changes.append({'frame': f, 'from': prev_w, 'to': w, 'since_dc_change': f - last_dc_frame, 'since_toggle': f - last_toggle,
                                'ring_open': bool(ring), 'event_state': c.u8(0xD0)})
            prev_w = w
    bad = [x for x in changes if x['since_toggle'] > 2 and (x['since_dc_change'] > 700 or x['ring_open'] or x['event_state'] not in (0, 1))]
    res = {'frames': nfr, 'ring_frames': int(ring_frames), 'event_frames': int(event_frames), 'width_changes': changes,
           'violations': bad, 'geo_events': len(c.geo_events), 'frame_sizes': sorted(set(c.sizes)), 'state': c.state()}
    json.dump(res, open(out, 'w'), indent=1)
    print(json.dumps({k: (v if k != 'width_changes' else len(v)) for k, v in res.items()}))


if __name__ == '__main__':
    main()
