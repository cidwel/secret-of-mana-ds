#!/usr/bin/env python3
"""Warps to the first transition of every distinct target map and records size and video setup."""
import json
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wslib  # noqa: E402

sys.path.insert(0, str(wslib.common.ROOT / 'tools'))
import romio  # noqa: E402

rom = romio.load(str(wslib.common.ROM_VANILLA))


def trans(n):
    o = 0x083000 + 4 * n
    w = rom[o] | rom[o + 1] << 8
    return w & 0x1FF, w >> 9, rom[o + 2] >> 1


def fil(state):
    p = state.find(b'\n') + 1
    while p < len(state):
        n = state[p:p + 3]
        L = int(state[p + 4:p + 10])
        if n == b'FIL':
            return state[p + 11:p + 11 + L]
        p += 11 + L


def warp(c, n):
    ev = bytes([0x18 | (n >> 8), n & 0xFF, 0])
    c.write(0x1FF00, ev)
    c.write(0xD1, b'\x00\xff\x7f')
    c.step()
    c.write(0xD0, b'\x01')


def main():
    base = open(wslib.WORK / 'base_vanilla.state', 'rb').read()
    c = wslib.WCore(wslib.common.ROM_VANILLA)
    seen = {}
    for n in range(0x400):
        m, tx, ty = trans(n)
        if m in seen or m > 0x1FF:
            continue
        c.load_state(base)
        c.run(2)
        warp(c, n)
        c.run(260)
        if c.u16(0xDC) != m:
            seen[m] = {'trans': n, 'ok': False, 'got': c.u16(0xDC)}
            continue
        f = fil(c.save_state())
        seen[m] = {'trans': n, 'ok': True, 'w': c.u16(0xC0), 'h': c.u16(0xC2), 'bgmode': f[0x2105] & 7, 'bg3prio': f[0x2105] >> 3 & 1,
                   'tm': f[0x212C], 'ts': f[0x212D], 'cgwsel': f[0x2130], 'cgadsub': f[0x2131], 'sc': [f[0x2107 + i] for i in range(3)],
                   'bgsz': f[0x2105] >> 4, 'mosaic': f[0x2106], 'w12': f[0x2123:0x2126].hex(), 'wlog': f[0x212A:0x212C].hex(),
                   'wm': f[0x212E:0x2130].hex(), 'frame_hash': hash(c.frame_rgb) & 0xFFFFFF, 'D0': c.u8(0xD0)}
        print(m, seen[m], flush=True)
    json.dump(seen, open(wslib.OUT / 'survey.json', 'w'), indent=1)


if __name__ == "__main__":
    main()
