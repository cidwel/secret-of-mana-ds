#!/usr/bin/env python3
"""Independent oracle for the HUD mask: the unmodified core, with the three portrait entries removed from the game's
OAM staging buffer in WRAM ($7E:0800, y = 0xE0) after every frame, versus the widescreen core with
snes9x_som_hud=hide_in_field.  Walks several maps, including to the bottom map edge so that the hero stands
in the HUD rows.  Counts field frames, frames where an actor sprite overlaps a portrait box, and frames whose
pixels differ (any difference means an actor sprite was removed or a HUD pixel survived).
usage: hud_oracle.py OUT.json FRAMES_PER_MAP MAPS..."""
import json
import sys
import pathlib

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wscore  # noqa: E402
from survey import warp  # noqa: E402
from hud_check import field, BOXES  # noqa: E402


def hud_slots(c):
    ws = c.wram
    r = []
    for i in range(3):
        b = 0x800 + i * 4
        hi = (ws[0x800 + 512 + (i >> 2)] >> ((i & 3) * 2)) & 3
        if ws[b + 1] == 199 and (ws[b + 3] & 0x30) == 0x30 and 0xE8 <= ws[b + 2] <= 0xEE and ws[b] in (8, 88, 168) and hi == 0:
            r.append(i)
    return r


def main():
    out, per = sys.argv[1], int(sys.argv[2])
    maps = [int(x) for x in sys.argv[3:]]
    base = open(wscore.WORK / 'party3_vanilla.state', 'rb').read()
    survey = json.load(open(wscore.OUT / 'survey.json'))
    orc = wscore.WS(wscore.common.ROM_VANILLA, wscore.CORE_PLAIN)
    ws = wscore.WS(wscore.common.ROM_VANILLA, wscore.CORE_WS, {'snes9x_som_hud': 'hide_in_field'})
    res = {'field_frames': 0, 'frames_compared': 0, 'frames_differing': 0, 'overlap_frames': 0, 'overlap_differing': 0,
           'hero_in_hud_rows_frames': 0, 'per_map': {}}
    for m in maps:
        for c in (orc, ws):
            c.load_state(base)
            c.run(2)
            if m != 131:
                warp(c, survey[str(m)]['trans'])
                c.run(300)
        seq = ['DOWN'] * 5 + ['RIGHT'] * 2 + ['DOWN'] * 5 + ['LEFT'] * 2 + ['UP'] + ['DOWN'] * 6
        stats = {'field': 0, 'diff': 0, 'overlap': 0, 'overlap_diff': 0, 'hero_low': 0}
        for f in range(per):
            btn = seq[(f // 40) % len(seq)]
            mk = orc.mask(btn)
            orc.set_pad(mk); ws.set_pad(mk)
            fb = field(orc)
            for i in hud_slots(orc) if fb else []:
                orc.wram[0x800 + i * 4 + 1] = 0xE0
            orc.step(); ws.step()
            if not fb or not field(orc):
                continue
            a, b = wscore.raw16(orc), wscore.raw16(ws)
            o = np.frombuffer(ws.oam(), np.uint8)
            ov = low = False
            for i in range(3, 128):
                y = o[i * 4 + 1]
                if y < 224:
                    x = o[i * 4] | (((o[512 + i // 4] >> ((i % 4) * 2)) & 1) << 8)
                    x = x - 512 if x >= 256 else x
                    if y + 16 > 199 and y < 215 and any(x + 16 > x0 and x < x1 for x0, x1 in BOXES):
                        ov = True
                    if y + 16 > 184 and y < 224:
                        low = True
            d = bool((a != b).any())
            stats['field'] += 1; stats['diff'] += d; stats['overlap'] += ov; stats['overlap_diff'] += ov and d; stats['hero_low'] += low
        res['per_map'][m] = stats
        for k, k2 in (('field_frames', 'field'), ('frames_differing', 'diff'), ('overlap_frames', 'overlap'),
                      ('overlap_differing', 'overlap_diff'), ('hero_in_hud_rows_frames', 'hero_low')):
            res[k] += stats[k2]
        print(m, stats, flush=True)
    res['frames_compared'] = res['field_frames']
    json.dump(res, open(out, 'w'), indent=1)
    print(json.dumps({k: v for k, v in res.items() if k != 'per_map'}))


if __name__ == '__main__':
    main()
