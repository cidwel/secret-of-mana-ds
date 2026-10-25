"""Contact sheet of every PNG named <id>.png in a directory (or a list), with id and name labels.  usage: contact.py OUT.png DIR_OR_FILES..."""
import sys, pathlib
from PIL import Image, ImageDraw
def sheet(items, out, cols=10, cell=(120, 120), scale=2, bg=(70, 70, 80, 255)):
    from PIL import ImageFont
    rows = (len(items) + cols - 1) // cols
    W = Image.new('RGBA', (cols * cell[0], rows * (cell[1] + 14)), bg)
    d = ImageDraw.Draw(W)
    for k, (label, im) in enumerate(items):
        x, y = (k % cols) * cell[0], (k // cols) * (cell[1] + 14)
        if im is not None:
            s = scale
            while im.width * s > cell[0] - 2 or im.height * s > cell[1] - 2:
                s -= 1
                if s < 1: break
            if s >= 1:
                im2 = im.resize((im.width * s, im.height * s), Image.NEAREST)
            else:
                f = min((cell[0] - 2) / im.width, (cell[1] - 2) / im.height); im2 = im.resize((max(1, int(im.width * f)), max(1, int(im.height * f))), Image.LANCZOS)
            W.alpha_composite(im2, (x + (cell[0] - im2.width) // 2, y + cell[1] - im2.height))
        d.text((x + 2, y + cell[1]), label[:20], fill=(255, 255, 255, 255))
    W.convert('RGB').save(out)
if __name__ == '__main__':
    out = sys.argv[1]
    files = []
    for a in sys.argv[2:]:
        p = pathlib.Path(a)
        files += sorted(p.glob('*.png')) if p.is_dir() else [p]
    sheet([(f.stem, Image.open(f).convert('RGBA')) for f in files], out)
