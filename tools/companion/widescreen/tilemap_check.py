#!/usr/bin/env python3
"""For every reachable map: does the tilemap rebuilt from the WRAM map match the game's own streamed tilemap
inside the view?  Counters come from the patched core (cells compared / cells different, per layer)."""
import ctypes as C
import json
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wslib  # noqa: E402
from survey import warp  # noqa: E402


def main():
    survey = json.load(open(wslib.OUT / 'survey.json'))
    base = open(wslib.WORK / 'base_vanilla.state', 'rb').read()
    c = wslib.WCore(wslib.common.ROM_VANILLA)
    c.lib.retro_som_cmp_get.restype = C.POINTER(C.c_uint32)
    out = {}
    for m, info in sorted(survey.items(), key=lambda kv: int(kv[0])):
        c.load_state(base); c.run(2); warp(c, info['trans']); c.run(250)
        p = c.lib.retro_som_cmp_get()
        before = [p[i] for i in range(4)]
        c.run(10)
        after = [p[i] for i in range(4)]
        d = [a - b for a, b in zip(after, before)]
        out[m] = {'cmp1': d[0], 'bad1': d[1], 'cmp2': d[2], 'bad2': d[3], 'a8': c.u16(0xA8), 'aa': c.u16(0xAA), 'ac': c.u16(0xAC),
                  'ae': c.u16(0xAE), 'w': c.u16(0xC0), 'h': c.u16(0xC2), 'bg1': c.bg(0), 'bg2': c.bg(1), 'wide_nonblack': 0}
        print(m, out[m], flush=True)
    json.dump(out, open(wslib.OUT / 'tilemap_check.json', 'w'))


main()
