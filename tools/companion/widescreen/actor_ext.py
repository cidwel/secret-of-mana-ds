#!/usr/bin/env python3
"""Actors alive but not emitted: for every map, count active actors (slots 3..) whose screen position falls in the
extension zone (E px beyond the 256 px view, inside the 224 rows) versus inside the view; also records the hero's
screen position (slot 0)."""
import json
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wslib  # noqa: E402
from survey import warp  # noqa: E402


def s16(v):
    return v - 65536 if v >= 32768 else v


def main():
    survey = json.load(open(wslib.OUT / 'survey.json'))
    base = open(wslib.WORK / 'base_vanilla.state', 'rb').read()
    c = wslib.WCore(wslib.common.ROM_VANILLA, ws=False)
    out = {}
    for m, info in sorted(survey.items(), key=lambda kv: int(kv[0])):
        c.load_state(base); c.run(2); warp(c, info['trans']); c.run(260)
        camx, camy = c.u16(0xA8), c.u16(0xAA)
        hero = (s16((c.u16(0xE002) - camx) & 0xFFFF), s16((c.u16(0xE004) - camy) & 0xFFFF))
        act = []
        for k in range(3, 24):
            b = 0xE000 + 0x200 * k
            if b + 6 > 0x20000 or c.u8(b) != 1:
                continue
            act.append((s16((c.u16(b + 2) - camx) & 0xFFFF), s16((c.u16(b + 4) - camy) & 0xFFFF)))
        out[m] = {'hero': hero, 'actors': act, 'w': info['w'], 'h': info['h']}
    json.dump(out, open(wslib.OUT / 'actor_ext.json', 'w'))
    print('done', len(out))


main()
