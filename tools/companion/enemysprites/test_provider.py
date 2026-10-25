"""PC proof of som_es_sprite_for_id on live scenes: for every enemy in the given maps (moved next to the party) the provider must give a sprite from the pack, and with
a pack that lacks the id, from the live render.  usage: test_provider.py MAP[,MAP...]"""
import sys

import esrender
import harness

pack = open(harness.STATE / 'som_enemy_sprites.bin', 'rb').read()
c = harness.core()
res = dict(pack=0, live=0, none=0, live_only_when_no_pack=0, live_only_none=0)
for m in [int(x) for x in sys.argv[1].split(',')]:
    harness.load_map(c, m)
    c.run(3)
    acts = harness.gather(c)
    wram, vram = c.dump_wram(), c.vram()
    for s, a in acts.items():
        if a['hpmax'] == 0:
            continue
        r, conf, img = esrender.sprite_for_id(pack, wram, vram, a['id'])
        res['pack' if r == 1 else 'live' if r == 2 else 'none'] += 1
        r2, conf2, img2 = esrender.sprite_for_id(None, wram, vram, a['id'])
        res['live_only_when_no_pack' if r2 == 2 else 'live_only_none'] += 1
        print(m, s, a['id'], 'pack-route', r, conf, None if img is None else img.shape, ' live-only', r2, conf2, flush=True)
print(res)
