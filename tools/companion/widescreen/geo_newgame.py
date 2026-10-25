#!/usr/bin/env python3
"""New-game run (title, name entry, intro dialogue, first fields) on the widescreen core: width per frame, with
screenshots of the title, name entry and a dialogue to verify that the geometry does not change there.
usage: geo_newgame.py OUT_DIR MARGIN [hud]"""
import json
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wscore  # noqa: E402
from PIL import Image  # noqa: E402

out = pathlib.Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
opts = {'snes9x_som_widescreen': sys.argv[2]}
if len(sys.argv) > 3:
    opts['snes9x_som_hud'] = 'hide_in_field'
script = []
for ln in open(wscore.common.NEWGAME):
    if ln.startswith('#') or not ln.strip():
        continue
    a, b, btn = ln.split()
    script.append((int(a), int(b), btn))
c = wscore.WS(wscore.common.ROM_VANILLA, wscore.CORE_WS, opts)
shots = {200: 'title', 500: 'select', 900: 'newgame', 1030: 'name1', 1400: 'name2', 2500: 'intro', 4900: 'dialog1', 5600: 'dialog2', 7000: 'dialog3', 7800: 'field', 8300: 'field2'}
rl = []
dcs = []
for f in range(8400):
    btn = next((s[2] for s in script if s[0] <= f < s[1]), None)
    c.set_pad(c.mask(btn))
    c.step()
    w = c.width
    if rl and rl[-1][1] == w:
        rl[-1][2] = f
    else:
        rl.append([f, w, f])
    if not dcs or dcs[-1][1] != c.u16(0xDC):
        dcs.append((f, c.u16(0xDC)))
    if f in shots:
        Image.fromarray(c.rgb()).save(str(out / ('%s_%d.png' % (shots[f], f))))
res = {'width_runs': rl, 'dc_changes': dcs, 'geo_events': c.geo_events, 'state': c.state()}
json.dump(res, open(out / 'newgame.json', 'w'), indent=1)
print(json.dumps(res))
