"""Reference drawing of the direct-control bottom screen (320 x 240): a live top-down mini-map of the party and the
nearby enemies plus group cards.  The Android and 3DS apps draw their own version with the same geometry:
mini-map area 320 x 200 (the 256 x 224 game screen scaled), card strip y 200..240.
"""
from PIL import Image, ImageDraw, ImageFont

MW, MH = 320, 200
ROLE = {0: ('BOY', (70, 140, 255)), 1: ('GIRL', (255, 120, 190)), 2: ('SPRITE', (120, 230, 120))}


def font(n=11):
    try:
        return ImageFont.truetype('/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf', n)
    except OSError:
        return ImageFont.load_default()


def point(a):
    x = a.sx - 0x10000 if a.sx > 0x7FFF else a.sx
    y = a.sy - 0x10000 if a.sy > 0x7FFF else a.sy
    return x * MW // 256, y * MH // 224


def party_offset(a):
    # the three heroes usually stand on the same pixel; the mini-map fans them out 12 px apart, the cards are the reliable pick
    return (a.slot - 1) * 12 if a.slot < 3 else 0


def card_rect(index):
    return (2 + 72 * index, MH + 2, 2 + 72 * index + 70, MH + 38)


def render(targets, title, picked=None, tap=None, group=0):
    im = Image.new('RGB', (320, 240), (18, 22, 30))
    d = ImageDraw.Draw(im)
    f, fs = font(11), font(9)
    d.rectangle([0, 0, MW - 1, MH - 1], outline=(70, 80, 100))
    for gx in range(0, MW, 40):
        d.line([gx, 0, gx, MH], fill=(30, 36, 48))
    for gy in range(0, MH, 40):
        d.line([0, gy, MW, gy], fill=(30, 36, 48))
    d.text((4, 2), title, fill=(230, 230, 120), font=f)
    for a in targets.actor:
        if not a.present:
            continue
        x, y = point(a)
        x += party_offset(a)
        if a.slot < 3:
            name, col = ROLE[a.slot]
            r = 8
            d.ellipse([x - r, y - r, x + r, y + r], fill=col if a.valid or a.alive else (70, 70, 70), outline=(255, 255, 255) if a.valid else (90, 90, 90), width=2 if a.valid else 1)
            d.text((x - 5, y - 6), name[0], fill=(0, 0, 0), font=f)
        else:
            r = 9
            col = (230, 70, 60) if a.valid else (110, 70, 70)
            d.rectangle([x - r, y - r, x + r, y + r], fill=col, outline=(255, 255, 255) if a.valid else (90, 90, 90), width=2 if a.valid else 1)
            d.text((x - 6, y - 6), str(a.slot - 2), fill=(255, 255, 255), font=f)
        if a.hp_max or a.hp:
            w = 18
            d.rectangle([x - w // 2, y + 11, x + w // 2, y + 14], outline=(120, 120, 120))
            frac = min(1.0, a.hp / max(1, a.hp_max or a.hp))
            d.rectangle([x - w // 2 + 1, y + 12, x - w // 2 + 1 + int((w - 2) * frac), y + 13], fill=(90, 220, 90))
        if picked is not None and a.slot == picked:
            d.ellipse([x - 16, y - 16, x + 16, y + 16], outline=(255, 255, 0), width=2)
    if tap:
        d.line([tap[0] - 8, tap[1], tap[0] + 8, tap[1]], fill=(255, 255, 0))
        d.line([tap[0], tap[1] - 8, tap[0], tap[1] + 8], fill=(255, 255, 0))
    # cards strip
    y0 = MH + 2
    x = 2
    for a in targets.actor:
        if a.present and (a.slot < 3 and targets.cls not in (3, 4)):
            name, col = ROLE[a.slot]
            ok = a.valid
            d.rounded_rectangle([x, y0, x + 70, y0 + 36], 4, outline=col if ok else (80, 80, 80), width=2, fill=(30, 34, 46))
            d.text((x + 4, y0 + 2), name, fill=col if ok else (110, 110, 110), font=fs)
            d.text((x + 4, y0 + 14), '%d/%d' % (a.hp, a.hp_max), fill=(220, 220, 220) if ok else (110, 110, 110), font=fs)
            x += 72
    if targets.cls in (3, 4):
        for a in targets.actor[3:]:
            if a.present and a.valid:
                d.rounded_rectangle([x, y0, x + 50, y0 + 36], 4, outline=(230, 70, 60), width=2, fill=(30, 34, 46))
                d.text((x + 4, y0 + 2), 'ENEMY %d' % (a.slot - 2), fill=(230, 120, 110), font=fs)
                d.text((x + 4, y0 + 14), 'HP %d' % a.hp, fill=(220, 220, 220), font=fs)
                x += 52
    if group:
        d.rounded_rectangle([x, y0, x + 78, y0 + 36], 4, outline=(255, 220, 80), width=2, fill=(50, 44, 20))
        d.text((x + 4, y0 + 8), 'WHOLE PARTY' if group == 1 else 'ALL ENEMIES', fill=(255, 230, 120), font=fs)
    return im
