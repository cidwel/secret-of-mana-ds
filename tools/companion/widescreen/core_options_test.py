#!/usr/bin/env python3
"""Option values, reported geometry and fallback behaviour of the widescreen core.
usage: core_options_test.py OUT.json"""
import json
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wscore  # noqa: E402
from survey import warp  # noqa: E402

import subprocess  # noqa: E402

base = open(wscore.WORK / 'base_vanilla.state', 'rb').read()
survey = json.load(open(wscore.OUT / 'survey.json'))


def case(kind, a, b):
    """One configuration per process: the core is a singleton (globals), two instances would share state."""
    opts = {'snes9x_som_widescreen': a}
    if kind == 'map':
        opts['snes9x_som_widescreen_fallback'] = b[0]
    if kind == 'blargg':
        opts['snes9x_blargg'] = 'rgb'
    c = wscore.WS(wscore.common.ROM_VANILLA, wscore.CORE_WS, opts)
    c.load_state(base)
    c.run(2)
    if kind == 'map':
        warp(c, survey[str(b[1])]['trans'])
        c.run(300)
        return {'width': c.width, 'state': c.state()}
    c.run(3)
    w, h, mw, ar = c.av()
    return {'base_width': w, 'base_height': h, 'max_width': mw, 'aspect': round(ar, 4), 'frame': [c.width, c.height]}


if len(sys.argv) > 2 and sys.argv[1] == '--case':
    k, a, b = sys.argv[2], sys.argv[3], sys.argv[4:]
    print('RESULT', json.dumps(case(k, a, (b[0], int(b[1])) if k == 'map' else None)))
    sys.exit(0)


def sub(*args):
    r = subprocess.run([sys.executable, __file__, '--case'] + [str(x) for x in args], capture_output=True, text=True)
    return json.loads([ln for ln in r.stdout.splitlines() if ln.startswith('RESULT')][0][7:])


res = {'av_info': {}, 'maps': {}}
for val in ('disabled', '17', '27', '46'):
    res['av_info'][val] = sub('av', val)
for m in (131, 253, 488, 102, 305, 8):
    res['maps'][str(m)] = {fb: sub('map', '27', fb, m) for fb in ('normal', 'black')}
res['blargg'] = sub('blargg', '27')
json.dump(res, open(sys.argv[1], 'w'), indent=1)
print(json.dumps(res))
