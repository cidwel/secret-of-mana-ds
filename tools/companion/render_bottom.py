#!/usr/bin/env python3
"""Draws the companion 'bottom screen' (320x240) from the JSON state emitted by som_companion_host, and optionally
composes it under the top screen (the game frame without HUD rows).  Image-only: no window system needed.

    render_bottom.py STATE.jsonl TAG OUT.png [--top TOP.ppm]
"""
import argparse
import json

from PIL import Image, ImageDraw

ITEM = ['Candy', 'Chocolate', 'Royal Jam', 'Faerie Walnut', 'Medical Herb', 'Cup of Wishes', 'Magic Rope', 'Flammie Drum',
        'Moogle Belt', 'Midge Mallet', 'Barrel']
WEAPON = {0: 'Glove?', 1: 'Sword', 2: 'Axe', 3: 'Spear', 7: 'Javelin'}
SPIRIT = ['Gnome', 'Undine', 'Salamando', 'Sylphid', 'Luna', 'Dryad', 'Lumina', 'Shade']
HERO = ['BOY', 'GIRL', 'SPRITE']
BG, FG, DIM, ACC = (16, 20, 40), (235, 235, 235), (120, 125, 150), (255, 200, 60)


def bar(d, x, y, w, h, frac, col):
    d.rectangle([x, y, x + w, y + h], outline=DIM)
    d.rectangle([x + 1, y + 1, x + 1 + int((w - 2) * max(0.0, min(1.0, frac))), y + h - 1], fill=col)


def render(st, size=(320, 240)):
    im = Image.new('RGB', size, BG)
    d = ImageDraw.Draw(im)
    d.text((6, 3), 'MAP %d   MODE %s   %s' % (st['map'], st['mode'].upper(), 'READY' if st['safe'] else 'LOCKED'),
           fill=ACC if st['safe'] else (230, 90, 90))
    y = 18
    for i, h in enumerate(st['party']):
        if not h['present']:
            continue
        d.rectangle([4, y, 315, y + 40], outline=DIM)
        d.text((8, y + 3), '%s  Lv%d%s' % (HERO[i], h['level'], '  KO' if not h['alive'] else ''), fill=FG)
        d.text((8, y + 14), 'HP %3d/%3d' % (h['hp'], h['hp_max']), fill=FG)
        bar(d, 80, y + 15, 100, 7, h['hp'] / max(1, h['hp_max']), (90, 200, 90))
        if h['mp_max']:
            d.text((8, y + 26), 'MP %3d/%3d' % (h['mp'], h['mp_max']), fill=FG)
            bar(d, 80, y + 27, 100, 7, h['mp'] / max(1, h['mp_max']), (90, 140, 230))
        d.text((196, y + 3), WEAPON.get(h['weapon_type'], 'type %d' % h['weapon_type']), fill=ACC)
        d.text((196, y + 14), 'charge %d%%' % h['recharge_pct'], fill=FG)
        bar(d, 196, y + 27, 100, 7, h['recharge_pct'] / 100.0, (230, 190, 70))
        y += 44
    d.text((6, y + 2), 'ITEMS', fill=ACC)
    x = 6
    for r in st['items']:
        if r[1]:
            d.text((x, y + 14), '%s x%d' % (ITEM[r[0]] if r[0] < len(ITEM) else '#%d' % r[0], r[1]), fill=FG)
            x += 100
            if x > 230:
                x, y = 6, y + 11
    y += 40
    names = [WEAPON.get(t, 't%d' % t) for t, v in enumerate(st['weapons']) if v != 255]
    d.text((6, y), 'WEAPONS: ' + ', '.join(names), fill=FG)
    g = st['ring']
    if g['open']:
        d.text((6, y + 12), 'RING open: owner %d category %d cursor %d/%d' % (g['owner'], g['category'], g['cursor'], g['count']),
               fill=(230, 90, 90))
    return im


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('states')
    ap.add_argument('tag')
    ap.add_argument('out')
    ap.add_argument('--top')
    a = ap.parse_args()
    st = next(json.loads(l) for l in open(a.states) if l.startswith('{') and json.loads(l).get('tag') == a.tag)
    bottom = render(st)
    if a.top:
        top = Image.open(a.top).convert('RGB')
        top = top.resize((top.width * 3 // 2 * 1, top.height * 3 // 2 * 1), Image.NEAREST) if False else top
        canvas = Image.new('RGB', (max(top.width, bottom.width), top.height + bottom.height), BG)
        canvas.paste(top, (0, 0))
        canvas.paste(bottom, (0, top.height))
        bottom = canvas
    bottom.save(a.out)


if __name__ == '__main__':
    main()
