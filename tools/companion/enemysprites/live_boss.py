"""PC proof for boss-engine actors (object ids 0x57..0x7F): load each map that has one, let the game run, and sample the screen every STEP frames;
per (map, slot) the complete render (all listed pieces found in the OAM) with the most opaque pixels is kept.
usage: live_boss.py MAP[,MAP...] [TOTAL_FRAMES [STEP]]"""
import json
import sys

import numpy as np
from PIL import Image

import esrender
import harness

maps = [int(x) for x in sys.argv[1].split(',')]
total = int(sys.argv[2]) if len(sys.argv) > 2 else 600
step = int(sys.argv[3]) if len(sys.argv) > 3 else 20
walk = sys.argv[4].split(',') if len(sys.argv) > 4 and sys.argv[4] else None   # buttons held while sampling (walk toward the arena)
out = harness.STATE / ('live_boss_walk' if walk else 'live_boss')
out.mkdir(parents=True, exist_ok=True)
c = harness.core()
res = []
for m in maps:
    harness.load_map(c, m)
    best = {}
    for t in range(0, total, step):
        c.run(step, buttons=walk)
        oam, vram, cg, ob = harness.ppu(c)
        wram = c.dump_wram()
        for s, a in harness.actors(c).items():
            if a['id'] < 0x57 or a['id'] >= 0x80:
                continue
            img, info = esrender.render_actor(oam, vram, cg, ob, wram, s)
            if img is None:
                continue
            complete = info['matched'] == info['pieces']
            score = (complete, int((img[..., 3] > 0).sum()))
            if s not in best or score > best[s][0]:
                best[s] = (score, img, dict(map=m, slot=s, id=a['id'], sx=a['sx'], sy=a['sy'], hp=a['hp'], frame=t + step, **info))
    for s, (score, img, row) in best.items():
        row['png'] = 'map%d_slot%d_id%d%s.png' % (m, s, row['id'], '_' + '_'.join(walk) if walk else '')
        Image.fromarray(img).save(out / row['png'])
        res.append(row)
        print(row, flush=True)
json.dump(res, open(out / ('boss_%s%s.json' % (sys.argv[1].replace(',', '_')[:60], '_' + '_'.join(walk) if walk else '')), 'w'), indent=1, default=str)
