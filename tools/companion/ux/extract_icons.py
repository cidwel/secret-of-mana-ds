#!/usr/bin/env python3
"""Captures what the game's own magic ring shows for each spirit and spell, to check the icon mapping in build_assets.py.

The ring icons are compressed in the ROM and drawn as sprites (some with colour math), so they are not decoded here.  The
tool drives the real game through the libretro harness (vanilla US ROM, saved party state): it opens the ring for the girl
(hero 1) and the sprite (hero 2), walks every spirit position and spell slot and saves the 24x24 frame crop around the
selected icon to <state>/assets/game_ring_crops/ (spell_<id>.png, spirit_h<hero>_b<byte>.png), plus a contact sheet.
Findings (see docs/companion-ux-20261017.md): the spell id is 6 * spirit position + 3 * girl + slot; with the party state's
spirits the ring only has art for four distinct spirits (positions 4-6 repeat positions 2-0), so the icons of the missing
spirits are assigned by meaning.  Nothing here is committed (Square Enix artwork).

Usage: extract_icons.py"""
import pathlib
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import common  # noqa: E402
import retro  # noqa: E402

OUT = common.STATE / 'assets' / 'game_ring_crops'
SOM_STATE = common.WORK / 'party3_vanilla.state'
A_ITEMS, A_WEAPONS = 0xCC48, 0xCC54


def settle(c, n=45):
    c.run(n)


def open_ring(c, hero):
    c.load_state(SOM_STATE.read_bytes())
    c.run(30)
    for t in range(12):
        c.write(A_ITEMS + t, bytes([(1 << 5) | t]) if t < 11 else bytes([0xFF]))
    for t in range(8):
        c.write(A_WEAPONS + t, bytes([0]))
    for h in range(3):
        c.write(0xE000 + 0x200 * h + 0x1C4, bytes([0x11] * 4))
    c.run(10, 'Y')
    settle(c, 80)
    for _ in range(3):
        if c.u8(0x1808) == hero:
            break
        c.run(3, 'X')
        settle(c, 60)


def goto_cat(c, cat):
    for _ in range(8):
        if c.u8(0x1809) == cat:
            return True
        c.run(3, 'UP')
        settle(c, 50)
    return False


def step(c):
    c.run(3, 'RIGHT')
    settle(c)


def crop(c, name, pad=4):
    fr = np.frombuffer(c.frame_rgb, dtype=np.uint8).reshape(c.height, c.width, 3)
    Image.fromarray(fr[115 - pad:131 + pad, 48 - pad:64 + pad]).save(OUT / (name + '.png'))


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    c = retro.Core(str(common.ROM_VANILLA), core_path=str(common.CORE))
    for hero in (1, 2):
        skip = 6 if hero == 1 else 7
        spirits = [e for e in range(8) if e != skip]
        open_ring(c, hero)
        goto_cat(c, 1)
        n = c.u8(0x180B)
        for pos in range(n):
            for _ in range((n - pos) % n):          # RIGHT lowers the cursor; Y from a spell ring resets it to 0
                c.run(3, 'RIGHT')
                settle(c, 60)
            byte = spirits[pos]
            sp = 6 if (hero == 1 and byte == 7) else byte
            crop(c, 'spirit_h%d_b%d' % (hero, byte))
            c.run(3, 'B')
            settle(c, 80)
            for _ in range(3):
                slot = c.u8(0x180A)
                crop(c, 'spell_%d' % (6 * sp + (3 if hero == 1 else 0) + slot))
                step(c)
            c.run(3, 'Y')
            settle(c, 120)
        print('hero', hero, 'done', flush=True)
    names = sorted([p.stem for p in OUT.glob('spell_*.png')], key=lambda s: int(s.split('_')[1]))
    S = 4
    cell = 24 * S
    sheet = Image.new('RGB', (7 * (cell + 4), ((len(names) + 6) // 7) * (cell + 14)))
    d = ImageDraw.Draw(sheet)
    for i, n in enumerate(names):
        x, y = (i % 7) * (cell + 4), (i // 7) * (cell + 14)
        sheet.paste(Image.open(OUT / (n + '.png')).resize((cell, cell), Image.NEAREST), (x, y + 12))
        d.text((x + 2, y), n, fill=(255, 255, 0))
    sheet.save(OUT / 'contact_spells.png')


if __name__ == '__main__':
    main()
