#!/usr/bin/env python3
"""Renders the app icons from the rabite pixel art (rabite.png, 22x17) with nearest-neighbour scaling only.

Writes the Android mipmaps and adaptive-icon layers, the 3DS 48x48 icon and the PC icons (icon.ico, icon.png).
Run it again after editing rabite.png; the outputs are committed so the builds need no Pillow for them.
"""
import os
from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.abspath(os.path.join(HERE, '..', '..', '..', '..'))
RES = os.path.join(ROOT, 'tools', 'companion', 'android', 'app', 'src', 'main', 'res')
TOP, BOTTOM = (0x3f, 0x9a, 0x5c), (0x1d, 0x5a, 0x3c)  # Mana-forest green behind the yellow rabite
RIM = (0x12, 0x3a, 0x26)
ART = Image.open(os.path.join(HERE, 'rabite.png')).convert('RGBA')


def rabite(box):
    """The art scaled by the largest integer factor that fits box, or smoothly reduced when even 1x does not."""
    k = min(box // ART.width, box // ART.height)
    if k >= 1:
        return ART.resize((ART.width * k, ART.height * k), Image.NEAREST)
    big = ART.resize((ART.width * 8, ART.height * 8), Image.NEAREST)
    f = box / ART.width
    return big.resize((max(1, round(ART.width * f)), max(1, round(ART.height * f))), Image.LANCZOS)


def backdrop(size, radius):
    grad = Image.new('RGBA', (size, size))
    d = ImageDraw.Draw(grad)
    for y in range(size):
        t = y / max(1, size - 1)
        d.line([(0, y), (size, y)], fill=tuple(round(a + (b - a) * t) for a, b in zip(TOP, BOTTOM)) + (255,))
    mask = Image.new('L', (size, size))
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, size - 1, size - 1], radius, fill=255)
    out = Image.new('RGBA', (size, size))
    out.paste(grad, (0, 0), mask)
    if size >= 32:
        ImageDraw.Draw(out).rounded_rectangle([0, 0, size - 1, size - 1], radius, outline=RIM + (255,), width=max(1, size // 48))
    return out


def paste_center(base, art, dy=0):
    base.alpha_composite(art, ((base.width - art.width) // 2, (base.height - art.height) // 2 + dy))
    return base


def tile(size, fill=0.86, round_=0.22):
    return paste_center(backdrop(size, round(size * round_)), rabite(round(size * fill)))


def main():
    for name, px in (('mdpi', 48), ('hdpi', 72), ('xhdpi', 96), ('xxhdpi', 144), ('xxxhdpi', 192)):
        d = os.path.join(RES, 'mipmap-' + name)
        os.makedirs(d, exist_ok=True)
        tile(px).save(os.path.join(d, 'ic_launcher.png'))
        # adaptive foreground: 108dp canvas, art inside the 66dp safe circle
        fg = px * 108 // 48
        paste_center(Image.new('RGBA', (fg, fg)), rabite(round(fg * 0.56))).save(os.path.join(d, 'ic_launcher_foreground.png'))
    tile(48, fill=0.92, round_=0.18).save(os.path.join(HERE, 'icon_3ds.png'))
    tile(256).save(os.path.join(HERE, 'icon.png'))
    sizes = [16, 24, 32, 48, 64, 128, 256]
    frames = [tile(s) for s in sizes]
    frames[-1].save(os.path.join(HERE, 'icon.ico'), sizes=[(s, s) for s in sizes], append_images=frames[:-1])


if __name__ == '__main__':
    main()
