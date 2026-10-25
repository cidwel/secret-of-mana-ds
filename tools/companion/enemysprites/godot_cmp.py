"""Looks a live-rendered picture up among all pictures of the Godot atlas of the same monster (as is, mirrored)."""
import json
import pathlib

import numpy as np
from PIL import Image

import godot_sprites as G

_cache = {}


def pics_of(mid):
    if mid in _cache:
        return _cache[mid]
    monsters = json.load(open(G.GODOT / 'data/databases/monsters.json'))
    slug = next((m['name'] for m in monsters if m.get('romId') == mid and (G.GODOT / 'data/databases/rom_mobs' / (m['name'] + '.json')).exists()), None)
    out = []
    if slug:
        d = json.load(open(G.GODOT / 'data/databases/rom_mobs' / (slug + '.json')))
        sheet = np.array(Image.open(G.GODOT / d['texture'].replace('res://', '')).convert('RGBA'))
        for x, y, w, h, ox, oy in d['pics']:
            out.append(sheet[y:y + h, x:x + w])
    _cache[mid] = (slug, out)
    return _cache[mid]


def trim(a):
    ys, xs = np.nonzero(a[..., 3])
    if len(ys) == 0:
        return a[:0, :0]
    return a[ys.min():ys.max() + 1, xs.min():xs.max() + 1]


def same(a, b, tol=10):
    if a.shape != b.shape:
        return False
    if not np.array_equal(a[..., 3] > 0, b[..., 3] > 0):
        return False
    m = a[..., 3] > 0
    return int(np.abs(a[..., :3].astype(int) - b[..., :3].astype(int))[m].max(initial=0)) <= tol


def find(mid, live):
    """Returns (slug, index, mirrored) of the first atlas picture equal to the live picture, or (slug, None, None)."""
    slug, pics = pics_of(mid)
    t = trim(live)
    for i, p in enumerate(pics):
        p = trim(p)
        if same(t, p):
            return slug, i, False
        if same(t, p[:, ::-1]):
            return slug, i, True
    return slug, None, None
