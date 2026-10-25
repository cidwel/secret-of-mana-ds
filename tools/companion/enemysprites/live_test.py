"""PC proof: for the given maps, move the enemies next to the party, render every enemy with som_enemy_sprite.c and compare with the Godot atlas.
usage: live_test.py MAP[,MAP...] [OUTDIR]"""
import json
import sys

import numpy as np
from PIL import Image

import esrender
import godot_cmp
import harness

maps = [int(x) for x in sys.argv[1].split(',')]
out = harness.STATE / 'live' if len(sys.argv) < 3 else __import__('pathlib').Path(sys.argv[2])
out.mkdir(parents=True, exist_ok=True)
c = harness.core()
res = []
for m in maps:
    harness.load_map(c, m)
    c.run(3)
    acts = harness.gather(c)
    oam, vram, cg, ob = harness.ppu(c)
    wram = c.dump_wram()
    for s, a in acts.items():
        if a['id'] >= 0x57 or a['hpmax'] == 0:
            continue
        img, info = esrender.render_actor(oam, vram, cg, ob, wram, s)
        row = dict(map=m, slot=s, id=a['id'], sx=a['sx'], sy=a['sy'], **info)
        o2, v2, c2, b2 = esrender.src_from_wram(wram, vram)          # the no-core-patch route: WRAM shadows + RETRO_MEMORY_VIDEO_RAM
        img2, info2 = esrender.render_actor(o2, v2, c2, b2, wram, s)
        if img2 is not None:
            row['wram_godot_pic'] = godot_cmp.find(a['id'], img2)[1]
        row['wram_route'] = 'same' if (img is None and img2 is None) or (img is not None and img2 is not None and img.shape == img2.shape and (img == img2).all()) else 'differs'
        if img is not None:
            Image.fromarray(img).save(out / ('map%d_slot%d_id%d.png' % (m, s, a['id'])))
            slug, idx, mir = godot_cmp.find(a['id'], img)
            row.update(godot=slug, godot_pic=idx, mirrored=mir)
        res.append(row)
        print(row, flush=True)
json.dump(res, open(out / ('live_%s.json' % sys.argv[1].replace(',', '_')[:60]), 'w'), indent=1, default=str)
