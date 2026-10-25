#!/usr/bin/env python3
"""Warps the party fixture to the first transition of every target map and saves a thumbnail contact sheet.

Used to find the Pure Land maps for the advanced test state.  Reads the ROM from the path given by SOM_ROM (default:
the user's US ROM); nothing from the ROM is stored in the repository.
"""
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[2]))
import common  # noqa: E402
import retro  # noqa: E402
from PIL import Image, ImageDraw  # noqa: E402

OUT = common.STATE / 'adv'
rom = open(common.ROM_VANILLA, 'rb').read()


def trans(n):
    o = 0x083000 + 4 * n
    w = rom[o] | rom[o + 1] << 8
    return w & 0x1FF, w >> 9, rom[o + 2] >> 1


def warp(c, n):
    ev = bytes([0x18 | (n >> 8), n & 0xFF, 0])
    c.write(0x1FF00, ev)
    c.write(0xD1, b'\x00\xff\x7f')
    c.step()
    c.write(0xD0, b'\x01')


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / 'thumbs').mkdir(exist_ok=True)
    c = retro.Core(str(common.ROM_VANILLA), core_path=str(common.CORE))
    base = open(common.WORK / 'party3_vanilla.state', 'rb').read()
    seen = {}
    for n in range(0x400):
        m, tx, ty = trans(n)
        if m in seen or m > 0x1FF:
            continue
        c.load_state(base)
        c.run(2)
        warp(c, n)
        c.run(300)
        ok = c.u16(0xDC) == m
        seen[m] = {'trans': n, 'ok': ok, 'got': c.u16(0xDC), 'tx': tx, 'ty': ty}
        if ok:
            w, h = c.width, c.height
            im = Image.frombytes('RGB', (w, h), bytes(c.frame_rgb)).resize((128, 112))
            ImageDraw.Draw(im).text((2, 2), str(m), fill=(255, 255, 0))
            im.save(OUT / 'thumbs' / ('%03d.png' % m))
        print(m, seen[m], flush=True)
    json.dump(seen, open(OUT / 'survey.json', 'w'), indent=1)


if __name__ == '__main__':
    main()
