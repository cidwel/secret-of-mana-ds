#!/usr/bin/env python3
"""Builds the advanced test save state (vanilla US, level 50 party in the Pure Land) from the PC fixture.

  build_advanced_state.py [--map 215] [--out PATH]

Input: work/party3_vanilla.state (real game route to Potos village, girl and sprite activated by the game's own event
opcode, ring flag set) written by tools/companion/build_fixture.py.  Needs the user's US ROM (SOM_ROM) and the PC
libretro snes9x core (SOM_CORE); nothing from the ROM is stored.

What is REAL: the route to Potos, the activation of girl and sprite, and the stat tables: levels are set to 49
(displays 50) and the game's own map-load code rebuilds max HP/MP, the five stats and the experience from the level
(it does this for slots 1 and 2; slot 0, the boy, is rebuilt the same way through a temporary swap of the object id).
What is INJECTED into WRAM: levels, boy rebuilt fields (copied), weapon levels, spirit levels, spirit masks, items,
weapons owned, gold, HP and MP (HP is max minus 1 through the game's pending-damage field so the HUD stays in sync).  The map change to the Pure Land is the game's own warp event (opcode 0x18).
"""
import argparse
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parents[1]))
sys.path.insert(0, str(HERE))
import common  # noqa: E402
import retro  # noqa: E402
from survey_maps import trans, warp  # noqa: E402

BOY_COPY = [(0x17D, 3), (0x184, 2), (0x187, 6), (0x197, 2), (0x1A4, 2), (0x1A7, 2)]   # fields rebuilt by the game
LEVEL = 49          # internal, displays as 50
ITEM_IDS = range(11)


def rec(c, h):
    d = c.read(0xE000 + 0x200 * h, 0x200)
    return {'lv': d[0x181] + 1, 'hp': d[0x182] | d[0x183] << 8, 'mhp': d[0x184] | d[0x185] << 8, 'mp': d[0x186], 'mmp': d[0x187],
            'stats': list(d[0x188:0x18D]), 'id': d[0x1E7]}


def trans_for(m):
    return [i for i in range(0x400) if trans(i)[0] == m][0]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--map', type=int, default=215)
    ap.add_argument('--out', default=str(common.STATE / 'adv' / 'advanced_vanilla.state'))
    a = ap.parse_args()
    base = open(common.WORK / 'party3_vanilla.state', 'rb').read()
    c = retro.Core(str(common.ROM_VANILLA), core_path=str(common.CORE))

    # 1. harvest the boy's level 50 rebuild: put the boy's object id in slot 1 and let the map-load code rebuild it
    c.load_state(base); c.run(2)
    for h in range(3):
        c.write(0xE000 + 0x200 * h + 0x181, bytes([LEVEL]))
    c.write(0xE200 + 0x1E7, b'\x80')
    warp(c, trans_for(a.map)); c.run(300)
    boy = c.read(0xE200, 0x200)
    print('boy rebuilt', rec(c, 1))

    # 2. real build
    c.load_state(base); c.run(2)
    for h in range(3):
        b = 0xE000 + 0x200 * h
        c.write(b + 0x181, bytes([LEVEL]))
        c.write(b + 0x1C0, bytes([0x88] * 4))          # weapon levels: all eight types at the maximum nibble
        if h:
            c.write(b + 0x1C4, bytes([0x88] * 4))      # spirit (spell) levels maxed
            c.write(b + 0x1C8, bytes([0x3F] * 8))      # all spells of all spirits
    for off, n in BOY_COPY:
        c.write(0xE000 + off, bytes(boy[off:off + n]))
    c.write(0xE188, bytes(boy[0x188:0x18D]))
    # items: all eleven consumables, quantity 7 ((qty << 5) | id), last record empty
    recs = bytes([0xE0 | i for i in ITEM_IDS]) + b'\xff' * (12 - len(ITEM_IDS))
    c.write(0xCC48, recs)
    # weapons owned: every type at tier 8, keeping the equipped marks of the fixture
    slots = c.read(0xCC54, 8)
    c.write(0xCC54, bytes([(0 if v == 0xFF else v & 0xF0) | 8 for v in slots]))
    c.write(0xCC6A, b'\xff\xff\x00')                    # gold 65,535 (the game's cap)
    c.write(0xCF00, b'\x01')                            # ring menus enabled
    c.run(2)
    warp(c, trans_for(a.map)); c.run(300)
    assert c.u16(0xDC) == a.map, c.u16(0xDC)
    # HP at the maximum minus the 1 HP of a pending damage: the game applies it and refreshes its HUD counters (a plain
    # write or the heal opcode leaves the HUD showing stale numbers); MP is written
    for h in range(3):
        b = 0xE000 + 0x200 * h
        d = c.read(b, 0x200)
        c.write(b + 0x182, bytes(d[0x184:0x186]))
        c.write(b + 0x186, bytes([d[0x187]]))
        c.write(b + 0x1F1, b'\x01\x00')
    c.run(60)
    for h in range(3):
        print(h, rec(c, h))
    print('map', c.u16(0xDC), 'D0', c.u8(0xD0), 'items', c.read(0xCC48, 12).hex(), 'weapons', c.read(0xCC54, 8).hex())
    open(a.out, 'wb').write(c.save_state())
    print('wrote', a.out)


if __name__ == '__main__':
    main()
