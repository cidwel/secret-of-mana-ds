#!/usr/bin/env python3
"""Builds the companion UI graphics pack (font, icons, portraits) from the owner's mana-magic-godot project.

The pack is written OUTSIDE the repository (default: the companion state directory under assets/).  Nothing produced
here may be committed: the pictures are Square Enix artwork (the Godot project keeps them only as stand-ins).  The C
renderer (som_ui_draw.c) runs without a pack, using built-in placeholder shapes; a pack only replaces them.

Pack format (little endian), loaded by som_ux_pack_load():
  "SUXP", u16 version=1, u16 atlas_w, u16 atlas_h, u16 nrects,
  nrects * { u16 id, u16 x, u16 y, u16 w, u16 h, i16 ox, i16 oy, u16 adv }   (16 bytes each)
  atlas_w * atlas_h * 4 bytes, RGBA, straight alpha.
Ids: 0x4000 + si * 0x200 + face * 0x100 + ch  UI font glyph at device scale SCALES[si] (face 0 normal, 1 condensed): straight RGBA, grey = fill
level (white fill, black outline), alpha = coverage, w/h/ox/oy in DEVICE pixels, adv = advance in device pixels.
0x0100+ch normal font glyph (white fill, black outline), 0x0200+ch small font glyph (white), 0x1000+n icon n of
spr_iconsGeneral (16x16), 0x2000+h portrait h (98x108), 0x2010+h portrait h at 24x26.  h: 0 boy, 1 girl, 2 sprite.
"""
import argparse
import hashlib
import json
import pathlib
import re
import struct
import sys

from PIL import Image, ImageDraw, ImageFilter, ImageFont

DEFAULT_SRC = pathlib.Path('/home/cidwel/projects/godot/doner-engine-godot/mana-magic-godot/assets')
DEFAULT_OUT = pathlib.Path('/home/cidwel/.local/state/som-decomp/companion-20261017/assets')


SPELL_CELL = [9, 13, 11, 108, 112, 110,                              # gnome: sprite Earth Slide, Gem Missile, Speed Down; girl Stone Saber, Speed Up, Defender
              2, 4, 0, 57, 61, 59,                                   # undine
              17, 15, 19, 63, 70, 65,                                # salamando: Fireball, Exploder, Lava Wave; Flame Saber, Fire Bouquet, Blaze Wall
              24, 29, 31, 72, 74, 77,                                # sylphid
              33, 37, 35, 78, 83, 85,                                # luna: Change Form, Magic Absorb, Lunar Magic; Moon Saber, Lunar Boost, Moon Energy
              41, 39, 120, 87, 89, 122,                              # dryad: Sleep Flower, Burst, Mana Magic; Revivifier, Wall, Mana Magic
              52, 51, 49, 104, 106, 99]                              # shade: Evil Gate, Dark Force, Dispel Magic; lumina: Light Saber, Lucent Beam, Lucid Barrier
# Every row was compared with the icons the game draws in its own magic ring (docs/companion-device-20261017.md, device/ico/cmp_*).


def read_fnt(path):
    chars = {}
    for ln in pathlib.Path(path).read_text().splitlines():
        if ln.startswith('char '):
            kv = dict(re.findall(r'(\w+)=(-?\d+)', ln))
            chars[int(kv['id'])] = {k: int(v) for k, v in kv.items()}
    return chars


def bbox_alpha(im):
    a = im.getchannel('A').point(lambda v: 255 if v > 0 else 0)
    return a.getbbox()


class Packer:
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.img = Image.new('RGBA', (w, h), (0, 0, 0, 0))
        self.x = self.y = self.rowh = 0
        self.rects = []

    def add(self, rid, im, ox=0, oy=0, adv=0):
        w, h = im.size
        if self.x + w > self.w:
            self.x, self.y, self.rowh = 0, self.y + self.rowh + 1, 0
        if self.y + h > self.h:
            raise RuntimeError('atlas full')
        self.img.paste(im, (self.x, self.y))
        self.rects.append((rid, self.x, self.y, w, h, ox, oy, adv))
        self.x += w + 1
        self.rowh = max(self.rowh, h)


FONT = pathlib.Path(__file__).resolve().parent.parent / 'assets' / 'fonts' / 'IBMPlexSansCondensed-SemiBold.ttf'   # SIL OFL 1.1
SCALES = (2, 3, 4, 6, 8)        # device pixels per vp the UI font is rasterised for (scale x text multiplier)
CAP_VP = 8.5                    # capital height in vp; the line box is 11 vp


def add_ui_font(pk):
    """Rasterises the UI font (readable M N W at 2 mm) for every scale: anti-aliased fill with a dark outline, pen advance in device px."""
    import numpy as np
    for si, k in enumerate(SCALES):
        cap = round(CAP_VP * k)
        size = 20
        while True:                                     # font size whose capital H is `cap` pixels tall
            f = ImageFont.truetype(str(FONT), size)
            bb = f.getbbox('H')
            if bb[3] - bb[1] >= cap:
                break
            size += 1
        base = round((11 - CAP_VP) / 2 * k) + cap       # baseline below the top of the 11 vp line box
        o = max(1, round(k * 0.5))                      # outline width in device px
        for face in (0, 1):
            xs = 1.0 if face == 0 else 0.8              # face 1: condensed to 80 percent width
            for ch in range(32, 127):
                rid = 0x4000 + si * 0x200 + face * 0x100 + ch
                adv = round(f.getlength(chr(ch)) * xs)
                if ch == 32:
                    pk.add(rid, Image.new('RGBA', (1, 1)), 0, 0, adv)
                    continue
                pad = size
                m = Image.new('L', (int(f.getlength(chr(ch))) + 2 * pad, 3 * size), 0)
                ImageDraw.Draw(m).text((pad, 2 * size), chr(ch), fill=255, font=f, anchor='ls')
                if xs != 1.0:
                    m = m.resize((max(1, round(m.width * xs)), m.height), Image.LANCZOS)
                px0 = round(pad * xs)
                bx = m.getbbox()
                if bx is None:
                    continue
                box = (bx[0] - o, bx[1] - o, bx[2] + o, bx[3] + o)
                fill = m.crop(box)
                fa = np.asarray(fill, dtype=np.float32) / 255.0
                oa = np.asarray(fill.filter(ImageFilter.MaxFilter(2 * o + 1)), dtype=np.float32) / 255.0
                alpha = np.maximum(fa, oa)
                grey = fa / np.maximum(alpha, 1e-6)
                rgba = np.zeros(alpha.shape + (4,), dtype=np.uint8)
                rgba[..., 0] = rgba[..., 1] = rgba[..., 2] = np.round(grey * 255)
                rgba[..., 3] = np.round(alpha * 255)
                pk.add(rid, Image.fromarray(rgba, 'RGBA'), box[0] - px0, base + (box[1] - 2 * size), adv)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--src', default=str(DEFAULT_SRC))
    ap.add_argument('--out', default=str(DEFAULT_OUT))
    ap.add_argument('--fonts-only', action='store_true',
                    help='only the UI font (IBM Plex, SIL OFL): no Square Enix artwork, so the pack can ship in public builds')
    a = ap.parse_args()
    src, out = pathlib.Path(a.src), pathlib.Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    pk = Packer(2048, 2048)
    if a.fonts_only:
        add_ui_font(pk)
        return write_pack(pk, out, 'IBM Plex Sans Condensed (SIL OFL 1.1) only; may be distributed.', str(FONT))

    big = Image.open(src / 'fonts/sprfont_som.png').convert('RGBA')
    bfnt = read_fnt(src / 'fonts/sprfont_som.fnt')
    for cid, c in sorted(bfnt.items()):
        if cid < 33 or cid > 126:
            continue
        cell = big.crop((c['x'], c['y'], c['x'] + c['width'], c['y'] + c['height']))
        half = cell.resize((cell.width // 2, cell.height // 2), Image.NEAREST)
        bb = bbox_alpha(half)
        if bb is None:
            continue
        pk.add(0x0100 + cid, half.crop(bb), bb[0], bb[1], max(1, c['xadvance'] // 2))
    pk.add(0x0100 + 32, Image.new('RGBA', (1, 1)), 0, 0, 5)

    small = Image.open(src / 'fonts/sprfont_somSmall.png').convert('RGBA')
    sfnt = read_fnt(src / 'fonts/sprfont_somSmall.fnt')
    for cid, c in sorted(sfnt.items()):
        if cid < 33 or cid > 126:
            continue
        cell = small.crop((c['x'], c['y'], c['x'] + c['width'], c['y'] + c['height']))
        cell = cell.resize((cell.width, cell.height // 2), Image.NEAREST)   # the sheet stores every row twice
        bb = bbox_alpha(cell)
        if bb is None:
            continue
        pk.add(0x0200 + cid, cell.crop(bb), bb[0], bb[1], max(1, c['xadvance']))
    pk.add(0x0200 + 32, Image.new('RGBA', (1, 1)), 0, 0, 3)

    icons = Image.open(src / 'sprites/sheets/spr_iconsGeneral.png').convert('RGBA')
    for n in range(49):
        cx, cy = n % 10, n // 10
        pk.add(0x1000 + n, icons.crop((cx * 16, cy * 16, cx * 16 + 16, cy * 16 + 16)))

    # spell icons: spell id (ROM name block order) -> cell of spr_iconsSkills (10 columns).  Cells 0-11, 18-23 (sylphid set),
    # 27-29 and 36-38 were matched against the icons the game draws in its own magic ring (extract_icons.py / crops, see
    # docs/companion-ux-20261017.md); the other cells are assigned by the spell's meaning.
    skills = Image.open(src / 'sprites/sheets/spr_iconsSkills.png').convert('RGBA')
    for sid, cell in enumerate(SPELL_CELL):
        cx, cy = cell % 10, cell // 10
        pk.add(0x1100 + sid, skills.crop((cx * 16, cy * 16, cx * 16 + 16, cy * 16 + 16)))

    faces = ['randi', 'purim', 'popoie']
    for h, nm in enumerate(faces):
        f = Image.open(src / ('sprites/spr_artBattleHud_face/face_%s.png' % nm)).convert('RGBA')
        pk.add(0x2000 + h, f)
        pk.add(0x2010 + h, f.resize((24, 26), Image.LANCZOS))

    add_ui_font(pk)
    return write_pack(pk, out, 'Square Enix artwork from the owner project; never commit.', str(src))


def write_pack(pk, out, note, source):
    hh = max(r[2] + r[4] for r in pk.rects)
    pk.img = pk.img.crop((0, 0, pk.w, hh)); pk.h = hh
    blob = bytearray(b'SUXP')
    blob += struct.pack('<HHHH', 1, pk.w, pk.h, len(pk.rects))
    for r in pk.rects:
        blob += struct.pack('<HHHHHhhH', *r)
    blob += pk.img.tobytes()
    (out / 'som_ui_pack.bin').write_bytes(bytes(blob))
    pk.img.save(out / 'som_ui_pack_atlas.png')
    man = {'file': 'som_ui_pack.bin', 'sha256': hashlib.sha256(blob).hexdigest(), 'bytes': len(blob),
           'atlas': [pk.w, pk.h], 'rects': len(pk.rects), 'source': source, 'note': note}
    (out / 'som_ui_pack.json').write_text(json.dumps(man, indent=1))
    print(json.dumps(man))

if __name__ == '__main__':
    sys.exit(main())
