#!/usr/bin/env python3
"""Round 3b: new-game run on the stock core and on the wide core (hires average / native), frames dumped every STEP frames.
usage: r3b_dump.py OUT_DIR [step] [last]"""
import pathlib, sys, pickle
import numpy as np
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wscore  # noqa: E402

out = pathlib.Path(sys.argv[1]); out.mkdir(parents=True, exist_ok=True)
step = int(sys.argv[2]) if len(sys.argv) > 2 else 100
last = int(sys.argv[3]) if len(sys.argv) > 3 else 9000
script = []
for ln in open(wscore.common.NEWGAME):
    if ln.startswith('#') or not ln.strip():
        continue
    a, b, btn = ln.split(); script.append((int(a), int(b), btn))
CFG = {'plain': (wscore.CORE_PLAIN, {}),
       'wide_avg': (wscore.CORE_WS, {'snes9x_som_widescreen': '46', 'snes9x_som_hud': 'hide_except_ring', 'snes9x_som_hires': 'average'}),
       'wide_nat': (wscore.CORE_WS, {'snes9x_som_widescreen': '46', 'snes9x_som_hud': 'hide_except_ring', 'snes9x_som_hires': 'native'})}
for name, (core, opts) in CFG.items():
    c = wscore.WS(wscore.common.ROM_VANILLA, core, opts)
    fr = {}
    for f in range(last):
        btn = next((s[2] for s in script if s[0] <= f < s[1]), None)
        c.set_pad(c.mask(btn)); c.step()
        if f % step == 0:
            fr[f] = (c.rgb(), c.u8(0xDC) if hasattr(c, 'u8') else 0)
    pickle.dump(fr, open(out / (name + '.pkl'), 'wb'))
    print(name, len(fr), flush=True)
