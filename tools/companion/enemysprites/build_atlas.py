#!/usr/bin/env python3
"""Builds the enemy sprite atlas (one clean RGBA picture per monster id) from the best source available.

usage: build_atlas.py [STATE_DIR] [--rom ROM]       STATE_DIR default /home/cidwel/.local/state/som-decomp/companion-20261017/enemysprites
Inputs (all in STATE_DIR, produced by godot_sprites.py, rip_shim.py, live_test.py / live_boss.py):
  godot/<id>.png + godot.json     idle picture cut from the Godot project (ROM rips, private use)
  rip/<id>/ord00_down_00.png      idle picture drawn by the game's own code in the ripper (tools/rip_monster_anims.py)
  live/*.png + live_*.json        live renders of tools/companion/enemysprites/som_enemy_sprite.c on the PC harness
  live_boss/*.png + boss_*.json   the same for boss engine actors
Order of preference per id: godot > rip > live.  Outputs in STATE_DIR: png/<id>.png, enemy_sprites.json, som_enemy_sprites.bin
(format SESP, docs/companion-enemysprites-20261017.md), enemy_atlas.png, contact_sheet.png.  Nothing is written to the repository."""
import collections
import glob
import hashlib
import json
import pathlib
import re
import struct
import sys

import numpy as np
from PIL import Image

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parents[1]))
import contact  # noqa: E402
import godot_cmp  # noqa: E402

ROM = '/home/cidwel/projects/rommancer/roms/snes/Secret of Mana (USA).smc'
SRC_CODE = {'none': 0, 'godot-rom-rip': 1, 'game-rip': 2, 'live': 3}
REJECT = {84: 'unused id (the name block holds "~"): nothing to show',
          86: 'Treasure Chest: the ripper picture is garbled (a map object, not a monster) and it is never a target'}
KIND = lambda i: 'enemy' if i < 0x57 else 'boss'


def trim_pad(arr, foot, pad=1):
    ys, xs = np.nonzero(arr[..., 3])
    if len(ys) == 0:
        return None, None
    x0, x1, y0, y1 = int(xs.min()), int(xs.max()) + 1, int(ys.min()), int(ys.max()) + 1
    a = arr[y0:y1, x0:x1]
    out = np.zeros((a.shape[0] + 2 * pad, a.shape[1] + 2 * pad, 4), np.uint8)
    out[pad:-pad, pad:-pad] = a
    return out, (foot[0] - x0 + pad, foot[1] - y0 + pad)


def norm(s):
    return re.sub(r'[^a-z0-9]', '', s.lower())


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    S = pathlib.Path(args[0]) if args else pathlib.Path('/home/cidwel/.local/state/som-decomp/companion-20261017/enemysprites')
    sys.path.insert(0, str(HERE.parents[2] / 'tools'))
    import names as N
    import romio
    names = N.load(romio.load(ROM))
    rname = lambda i: names[0xCF + i]

    cand = collections.defaultdict(dict)                  # id -> source -> dict(arr, foot, ...)
    gj = json.load(open(S / 'godot/godot.json')) if (S / 'godot/godot.json').exists() else {}
    for k, v in gj.items():
        arr = np.array(Image.open(S / 'godot' / ('%s.png' % k)).convert('RGBA'))
        cand[int(k)]['godot-rom-rip'] = dict(arr=arr, foot=tuple(v['foot']), note='%s %s' % (v['slug'], v['set']), slug=v['slug'])
    for dd in sorted((S / 'rip').glob('*/index.json')):
        mid = int(dd.parent.name)
        for d in sorted(dd.parent.glob('ord*_down_00.png')):     # ord00 is the stand set; a monster with an empty ord00 uses the first set that draws
            j = json.load(open(str(d)[:-7] + '.json'))
            arr, foot = trim_pad(np.array(Image.open(d).convert('RGBA')), j['origin'])
            if arr is not None:
                cand[mid]['game-rip'] = dict(arr=arr, foot=foot, note='%s_down frame 0 drawn by the game code (rip_monster_anims.py)' % d.name[:5])
                break
    live = collections.defaultdict(list)
    for jf in list((S / 'live').glob('live_*.json')) + list((S / 'live_boss').glob('boss_*.json')) + list((S / 'live_boss_walk').glob('boss_*.json')):
        d = jf.parent.name
        for r in json.load(open(jf)):
            if not r.get('ok'):
                continue
            p = S / d / r.get('png', 'map%d_slot%d_id%d.png' % (r['map'], r['slot'], r['id']))
            if p.exists():
                live[r['id']].append((r, p))
    stats = {}
    for mid, lst in live.items():
        full = [x for x in lst if x[0]['matched'] == x[0]['pieces']] or lst
        hs = collections.Counter()
        imgs = {}
        for r, p in full:
            a = np.array(Image.open(p).convert('RGBA'))
            h = hashlib.sha1(a.tobytes()).hexdigest()
            hs[h] += 1; imgs[h] = (a, r)
        h = hs.most_common(1)[0][0]
        a, r = imgs[h]
        cand[mid]['live'] = dict(arr=a, foot=tuple(r['foot']), note='map %d slot %d' % (r['map'], r['slot']), complete=r['matched'] == r['pieces'],
                                 renders=len(lst), complete_renders=sum(1 for x in lst if x[0]['matched'] == x[0]['pieces']))
        stats[mid] = dict(renders=len(lst), godot_match=sum(1 for x in lst if x[0].get('godot_pic') is not None))

    manifest = []
    (S / 'png').mkdir(exist_ok=True)
    items = []
    for mid in range(0, 0x80):
        c = {} if mid in REJECT else cand.get(mid, {})
        entry = dict(id=mid, name=rname(mid), kind=KIND(mid), source='none', size=None, foot=None, confidence=0, alternatives=sorted(c))
        if mid in REJECT:
            entry['rejected'] = REJECT[mid]
        pick = None
        for src in ('godot-rom-rip', 'game-rip', 'live'):
            if src in c:
                if src == 'live' and mid >= 0x57 and not c[src]['complete'] and len(c) > 1:
                    continue
                pick = src
                break
        st = stats.get(mid)
        if pick:
            e = c[pick]
            entry.update(source=pick, size=[e['arr'].shape[1], e['arr'].shape[0]], foot=list(e['foot']), note=e.get('note', ''))
            if pick == 'godot-rom-rip':
                entry['godot_name'] = e['slug']
                nm = norm(e['slug']) == norm(rname(mid))
                entry['name_matches_rom'] = nm
                if st and st['godot_match']:
                    entry.update(confidence=3, verified='live render of the game equals a picture of this Godot atlas (%d of %d renders)' % (st['godot_match'], st['renders']))
                else:
                    entry.update(confidence=2 if nm else 1)
                rc = S / 'rip_check' / str(mid) / 'ord00_down_00.png'
                if rc.exists():                              # the game's own code drew the same picture in the ripper
                    ra = np.array(Image.open(rc).convert('RGBA'))
                    if godot_cmp.same(godot_cmp.trim(ra), godot_cmp.trim(e['arr']), tol=0):
                        entry['confidence'] = 3
                        entry['verified_rip'] = 'tools/rip_monster_anims.py draws the identical ord00 picture for this id'
                    else:
                        entry['rip_check'] = 'differs'
            elif pick == 'game-rip':
                entry['confidence'] = 2
                if 'live' in c and mid in stats:
                    entry['live_renders'] = stats[mid]['renders']
            else:
                entry['confidence'] = 3 if (mid < 0x57 and e['complete']) else (2 if e['complete'] else 1)
                entry['live_renders'] = e['renders']; entry['live_complete_renders'] = e['complete_renders']
            Image.fromarray(e['arr']).save(S / 'png' / ('%d.png' % mid))
            items.append((mid, e['arr'], e['foot'], pick, entry['confidence']))
        manifest.append(entry)
    json.dump(dict(format=1, note='private local art: not for the repository', sprites=manifest), open(S / 'enemy_sprites.json', 'w'), indent=1)

    # pack
    n = len(items)
    hdr = 16 + 24 * n
    blob = bytearray()
    ents = bytearray()
    off = hdr
    for mid, arr, foot, src, conf in items:
        h, w = arr.shape[:2]
        ents += struct.pack('<BBBBHHhhII', mid, SRC_CODE[src], conf, 0, w, h, foot[0], foot[1], off, w * h * 4) + b'\0\0\0\0'
        px = arr.astype(np.uint8).tobytes()
        blob += px; off += len(px)
    head = struct.pack('<4sHHII', b'SESP', 1, n, off, 0)
    pack = head + bytes(ents) + bytes(blob)
    assert len(head) == 16 and len(ents) == 24 * n
    (S / 'som_enemy_sprites.bin').write_bytes(pack)
    # atlas png: shelf pack, 512 wide
    W = 512
    x = y = rh = 0
    place = {}
    for mid, arr, foot, src, conf in sorted(items, key=lambda t: -t[1].shape[0]):
        h, w = arr.shape[:2]
        if x + w > W:
            x, y, rh = 0, y + rh, 0
        place[mid] = (x, y, w, h); x += w; rh = max(rh, h)
    atlas = Image.new('RGBA', (W, y + rh), (0, 0, 0, 0))
    for mid, arr, foot, src, conf in items:
        px, py, w, h = place[mid]
        atlas.paste(Image.fromarray(arr), (px, py))
    atlas.save(S / 'enemy_atlas.png')
    json.dump({str(k): v for k, v in place.items()}, open(S / 'enemy_atlas_rects.json', 'w'))
    # contact sheet
    cs = []
    for e in manifest:
        im = Image.open(S / 'png' / ('%d.png' % e['id'])).convert('RGBA') if e['source'] != 'none' else None
        tag = {'godot-rom-rip': 'G', 'game-rip': 'R', 'live': 'L', 'none': '-'}[e['source']]
        cs.append(('%d%s%d %s' % (e['id'], tag, e['confidence'], e['name']), im))
    contact.sheet(cs, S / 'contact_sheet.png', cols=12, cell=(100, 84), scale=2)
    by = collections.Counter(e['source'] for e in manifest if e['id'] < 0x57)
    byb = collections.Counter(e['source'] for e in manifest if e['id'] >= 0x57)
    print('ordinary 0..0x56:', dict(by), ' boss 0x57..0x7F:', dict(byb), ' pack', len(pack), 'bytes,', n, 'sprites')
    bad = [(e['id'], e['name'], e['godot_name']) for e in manifest if e.get('name_matches_rom') is False]
    print('godot name != rom name:', bad)


if __name__ == '__main__':
    main()
