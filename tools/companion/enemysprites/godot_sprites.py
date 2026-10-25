#!/usr/bin/env python3
"""Cuts the idle picture of every monster out of the user's Godot project (private local use only: Square Enix art, ROM rips).

usage: godot_sprites.py [OUTDIR]     default OUTDIR = <state>/enemysprites/godot
Writes OUTDIR/<id>.png (RGBA, 1 px transparent padding) and OUTDIR/godot.json: id -> slug, picture, foot point (x, y inside the PNG), source.
The idle picture is the first picture of animation set `ord00`, facing down, the "stand" set of the project (mob.gd ROM_POSE_STAND_SET);
bosses use their first sequence.  Mirrored pictures (flip bit of the picture id) are un-mirrored.
"""
import json
import pathlib
import sys

from PIL import Image

GODOT = pathlib.Path('/home/cidwel/projects/godot/doner-engine-godot/mana-magic-godot')
STATE = pathlib.Path('/home/cidwel/.local/state/som-decomp/companion-20261017/enemysprites')


def first_pic(rle):
    return rle[0]


def cut(sheet, pics, pid):
    x, y, w, h, ox, oy = pics[pid >> 1]
    img = sheet.crop((x, y, x + w, y + h))
    if pid & 1:
        img = img.transpose(Image.FLIP_LEFT_RIGHT)
        ox = w - ox
    return img, (ox, oy)


def pad(img, origin, n=1):
    out = Image.new('RGBA', (img.width + 2 * n, img.height + 2 * n), (0, 0, 0, 0))
    out.paste(img, (n, n))
    return out, (origin[0] + n, origin[1] + n)


def main():
    outdir = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else STATE / 'godot'
    outdir.mkdir(parents=True, exist_ok=True)
    monsters = json.load(open(GODOT / 'data/databases/monsters.json'))
    by_slug = {}
    for m in monsters:
        if m.get('romId', -1) >= 0:
            by_slug.setdefault(m['name'], m['romId'])
    res = {}
    for jp in sorted((GODOT / 'data/databases/rom_mobs').glob('*.json')):
        slug = jp.stem
        if slug not in by_slug:
            continue
        d = json.load(open(jp))
        anim = d['animations'].get('ord00_down')
        while anim and 'alias' in anim:
            anim = d['animations'][anim['alias']]
        if not anim:
            continue
        sheet = Image.open(GODOT / d['texture'].replace('res://', '')).convert('RGBA')
        img, org = cut(sheet, d['pics'], first_pic(anim['pic']))
        if img.getbbox() is None:
            continue
        img, org = pad(img, org)
        rid = by_slug[slug]
        img.save(outdir / ('%d.png' % rid))
        res[rid] = dict(slug=slug, source='godot-rom-rip', set='ord00_down', size=list(img.size), foot=list(org))
    bp = GODOT / 'data/databases/rom_bosses/mantis_ant.json'
    d = json.load(open(bp))
    seq = d['sequences']['D09E:0']
    sheet = Image.open(GODOT / d['texture'].replace('res://', '')).convert('RGBA')
    img, org = cut(sheet, d['pics'], seq['pic'][0])
    img, org = pad(img, org)
    img.save(outdir / '87.png')
    res[87] = dict(slug='mantis_ant', source='godot-rom-rip', set='boss D09E:0', size=list(img.size), foot=list(org))
    json.dump({str(k): v for k, v in sorted(res.items())}, open(outdir / 'godot.json', 'w'), indent=1)
    print(len(res), 'sprites ->', outdir)


if __name__ == '__main__':
    main()
