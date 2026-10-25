#!/usr/bin/env python3
"""Equips the three heroes of the advanced test state (weapons tier 8, best armor of each slot they can wear).

  equip_party.py [--in STATE] [--out STATE]

Writes the game's own representation: armor inventory bytes `(hero + 1) << 6 | row` in the head ($CC24), body ($CC30)
and accessory ($CC3C) spans, equipped rows in hero +0x1E0..0x1E2, weapon holder marks in $CC54 + type and
`+0x1E3 = +0x1E8 = type * 9 + tier`, `+0x1E4 = type`.  The game's map-load code then rebuilds the stats from the equipment.
"""
import argparse
import json
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1]))
sys.path.insert(0, str(HERE))
import common  # noqa: E402
import retro  # noqa: E402
from survey_maps import warp  # noqa: E402
from build_advanced_state import trans_for  # noqa: E402

ROOT = HERE.parents[3]
HERO_BIT = (0x80, 0x40, 0x20)          # armor equip_mask bit of boy, girl, sprite
WEAPON = (1, 3, 6)                     # boy sword, girl spear, sprite bow
TIER = 8
SPANS = ((0x24, range(0, 21)), (0x30, range(21, 42)), (0x3C, range(42, 63)))


def best_rows(rows):
    out = []
    for h in range(3):
        pick = []
        for _, ids in SPANS:
            cand = [r for r in rows if r['id'] in ids and r['equip_mask'] & HERO_BIT[h]]
            cand.sort(key=lambda r: (r['defense'] + r['mdef'], r['evade'] + r['mev']), reverse=True)
            pick.append(cand[0]['id'])
        out.append(pick)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--in', dest='src', default=str(common.STATE / 'adv' / 'advanced_vanilla.state'))
    ap.add_argument('--out', default=str(common.STATE / 'adv' / 'advanced_equipped.state'))
    ap.add_argument('--map', type=int, default=215)
    a = ap.parse_args()
    rows = json.load(open(ROOT / 'data' / 'armors.json'))['rows']
    picks = best_rows(rows)
    c = retro.Core(str(common.ROM_VANILLA), core_path=str(common.CORE))
    c.load_state(open(a.src, 'rb').read()); c.run(2)
    for s, (base, _) in enumerate(SPANS):
        c.write(0xCC00 + base, bytes([((h + 1) << 6) | picks[h][s] for h in range(3)]) + b'\xff' * 9)
    for h in range(3):
        b = 0xE000 + 0x200 * h
        c.write(b + 0x1E0, bytes(picks[h]))
        t = WEAPON[h]
        row = t * 9 + TIER
        c.write(b + 0x1E3, bytes([row])); c.write(b + 0x1E8, bytes([row])); c.write(b + 0x1E4, bytes([t]))
    slots = bytearray(c.read(0xCC54, 8))
    for h in range(3):
        slots[WEAPON[h]] = ((h + 1) << 6) | TIER
    for t in range(8):
        if t not in WEAPON:
            slots[t] = TIER                                  # owned, held by nobody
    c.write(0xCC54, bytes(slots))
    c.run(2)
    warp(c, trans_for(a.map)); c.run(300)                    # the game's map-load code rebuilds the stats
    # the map-load rebuild skips slot 0: rebuild the boy's stats by running them through slot 1 with his object id
    girl = bytes(c.read(0xE200, 0x200))
    boy = bytes(c.read(0xE000, 0x200))
    c.write(0xE200 + 0x1E0, boy[0x1E0:0x1E9]); c.write(0xE200 + 0x1C0, boy[0x1C0:0x1C4]); c.write(0xE200 + 0x181, boy[0x181:0x182])
    c.write(0xE200 + 0x1E7, b'\x80')
    warp(c, trans_for(a.map)); c.run(300)
    rebuilt = bytes(c.read(0xE200, 0x200))
    c.write(0xE200, girl)
    for off, n in [(0x17D, 3), (0x184, 2), (0x187, 6), (0x197, 2), (0x1A4, 2), (0x1A7, 2)] + [(0x188, 0x24)]:
        c.write(0xE000 + off, rebuilt[off:off + n])
    c.run(2)
    warp(c, trans_for(a.map)); c.run(300)
    for h in range(3):
        b = 0xE000 + 0x200 * h
        d = c.read(b, 0x200)
        c.write(b + 0x182, bytes(d[0x184:0x186])); c.write(b + 0x186, bytes([d[0x187]])); c.write(b + 0x1F1, b'\x01\x00')
    c.run(60)
    for h in range(3):
        d = c.read(0xE000 + 0x200 * h, 0x200)
        print(h, 'armor', list(d[0x1E0:0x1E3]), 'weapon', d[0x1E4], d[0x1E8], 'def', d[0x1A4] | d[0x1A5] << 8,
              'hp', d[0x182] | d[0x183] << 8, '/', d[0x184] | d[0x185] << 8)
    print('CC54', c.read(0xCC54, 8).hex(), 'armor inv', c.read(0xCC24, 0x24).hex(' '))
    open(a.out, 'wb').write(c.save_state())
    print('wrote', a.out)


if __name__ == '__main__':
    main()
