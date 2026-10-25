#!/usr/bin/env python3
"""HUD sprite mask check: unmodified core versus the widescreen core with snes9x_som_hud=hide_in_field.

usage: hud_check.py OUT.json us|es|ac FRAMES [MARGIN]
Random walk with attacks, menus (Y, START) and dialogue buttons.  For every frame the 256x224 stock picture is
compared with the centre of the patched core.  Field frames (game mode FIELD of docs/wram-companion-map.md) may
differ only inside the three 16x16 portrait boxes; every other frame must be identical.
"""
import json
import random
import sys
import pathlib

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wscore  # noqa: E402
from survey import warp  # noqa: E402

R = wscore.common.ROMS
W = wscore.WORK
CFG = {
    'us': (wscore.common.ROM_VANILLA, W / 'party3_vanilla.state', None),
    'es': (wscore.common.ROM_MAGNO, W / 'party3_magno.state', 'potos'),
    'ac': (wscore.OUT.parent / 'ancientcave/out/ac_seed1.smc', W / 'ac_party.state', None),
}
BOXES = [(8, 24), (88, 104), (168, 184)]


def field(c):
    return c.u8(0xD0) == 0 and c.u8(0xE4) == 1 and c.u8(0xE6) == 15 and c.u8(0x7D) == 0 and c.u8(0xE000) == 1


def main():
    out, key, nfr = sys.argv[1], sys.argv[2], int(sys.argv[3])
    margin = sys.argv[4] if len(sys.argv) > 4 else 'disabled'
    rom, st, pre = CFG[key]
    state = open(st, 'rb').read()
    plain = wscore.WS(rom, wscore.CORE_PLAIN)
    opts = {'snes9x_som_hud': 'hide_in_field', 'snes9x_som_widescreen': margin}
    ws = wscore.WS(rom, wscore.CORE_WS, opts)
    for c in (plain, ws):
        c.load_state(state)
        c.run(5)
        if pre == 'potos':
            survey = json.load(open(wscore.OUT / 'survey.json'))
            warp(c, survey['131']['trans'])
            c.run(300)
    rnd = random.Random(7)
    names = ['UP', 'DOWN', 'LEFT', 'RIGHT', 'DOWN', 'DOWN', 'B', 'A', 'B,RIGHT', 'Y', 'START', 'X', 'DOWN,LEFT', 'DOWN,RIGHT']
    res = dict(frames=0, field=0, nonfield=0, field_diff=0, field_diff_outside_boxes=0, nonfield_diff=0, overlap_frames=0,
               overlap_diff_outside=0, hud_visible_nonfield=0, widths=sorted(set()), geo_events=0)
    widths = set()
    f = 0
    hero_low = 0
    while f < nfr:
        btn = rnd.choice(names)
        for _ in range(rnd.randint(8, 40)):
            if f >= nfr:
                break
            m = plain.mask(btn)
            fb = field(plain)
            plain.set_pad(m); ws.set_pad(m)
            plain.step(); ws.step()
            f += 1
            a = wscore.raw16(plain)
            if plain.width == 512 and ws.width != 512:
                a = wscore.merge_pairs(a)
            w = ws.width
            widths.add(w)
            b = wscore.raw16(ws)
            if w != 256 and w != plain.width:
                mg = (w - 256) // 2
                b = b[:, mg:mg + 256]
            fld = field(plain)
            if fld != fb:
                res['boundary_frames'] = res.get('boundary_frames', 0) + 1
                res['boundary_diff'] = res.get('boundary_diff', 0) + int((a != b).any())
                continue
            res['frames'] += 1
            d = a != b
            ndiff = int(d.sum())
            o = np.frombuffer(ws.oam(), np.uint8)
            if fld:
                res['field'] += 1
                if ndiff:
                    res['field_diff'] += 1
                    m_ = d.copy()
                    for x0, x1 in BOXES:
                        m_[199:215, x0:x1] = False
                    if m_.any():
                        res['field_diff_outside_boxes'] += 1
                # actors (slot >= 3) whose 16 px box touches a HUD box
                ov = False
                for i in range(3, 128):
                    y = o[i * 4 + 1]
                    if y < 224:
                        x = o[i * 4] | (((o[512 + i // 4] >> ((i % 4) * 2)) & 1) << 8)
                        x = x - 512 if x >= 256 else x
                        if y + 16 > 199 and y < 215 and any(x + 16 > x0 and x < x1 for x0, x1 in BOXES):
                            ov = True
                            break
                if ov:
                    res['overlap_frames'] += 1
            else:
                res['nonfield'] += 1
                if ndiff:
                    res['nonfield_diff'] += 1
    res['widths'] = sorted(widths)
    res['hud_hidden_slots_total'] = ws.lib.retro_som_hud_hidden()
    res['state_final'] = ws.state()
    json.dump(res, open(out, 'w'), indent=1)
    print(json.dumps(res))


if __name__ == "__main__":
    main()
