#!/usr/bin/env python3
"""Is the 256 px centre of the wide render pixel-identical to the stock core?  Two processes (SOM_WS is read once):
identity_check.py MODE OUT.json with MODE = stock or wide; then identity_check.py compare A.json B.json."""
import hashlib
import json
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wslib  # noqa: E402
from survey import warp  # noqa: E402


def main():
    if sys.argv[1] == 'compare':
        a, b = json.load(open(sys.argv[2])), json.load(open(sys.argv[3]))
        bad = [k for k in a if a[k] != b.get(k)]
        print('frames compared', sum(len(v) for v in a.values()), 'maps', len(a), 'maps with a difference', len(bad), bad[:10])
        return
    wide = sys.argv[1] == 'wide'
    survey = json.load(open(wslib.OUT / 'survey.json'))
    base = open(wslib.WORK / 'base_vanilla.state', 'rb').read()
    c = wslib.WCore(wslib.common.ROM_VANILLA, ws=wide)
    out = {}
    for m, info in sorted(survey.items(), key=lambda kv: int(kv[0])):
        c.load_state(base); c.run(2); warp(c, info['trans']); c.run(260)
        hs = []
        for k in range(30):
            c.set_pad(c.mask('RIGHT' if k < 20 else 'LEFT'))
            c.step()
            img = c.wide()[:, 256:512] if wide else c.stock()
            hs.append(hashlib.md5(img.tobytes()).hexdigest()[:12])
        out[m] = hs
    json.dump(out, open(sys.argv[2], 'w'))


main()
