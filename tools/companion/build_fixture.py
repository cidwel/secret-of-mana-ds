#!/usr/bin/env python3
"""Builds the core save states used by the tests, by driving the real game (no ROM bytes are stored).

  base_<rom>.state     route to a controllable hero in the first village (vanilla) / first map (any ROM)
  party3.state         base + girl and sprite activated through the game's own event opcode 0x2B, healed
                       with opcode 0x2F, plus WRAM fixtures: items, weapons, spirit masks, MP (see FIXTURE)
"""
import argparse
import sys

sys.path.insert(0, str(__import__('pathlib').Path(__file__).resolve().parent))
import common  # noqa: E402

# test fixtures poked into WRAM (documented in docs/companion-prototype-20261017.md)
FIXTURE = """
# girl (slot 1) and sprite (slot 2): the game's own activation opcode, then refill HP (operand = slot + 1)
EVENT 2B012B0200
RUN 150
EVENT 2F022F0300
RUN 90
# clear the dead flag (+0x191 bit 7) left by the level-0 initializer, give MP, spirits and magic levels
WRITE E391 00
WRITE E591 00
WRITE E386 08
WRITE E586 08
WRITE E3C4 12345678
WRITE E5C4 12345678
WRITE E3C8 3F3F3F3F3F3F3F3F
WRITE E5C8 3F3F3F3F3F3F3F3F
# story flag $CF00 = 1 enables the pad-driven ring menus (already set after the Potos elder scene); sword in the ring
WRITE CF00 01
WRITE CC55 40
# consumable records: Candy x3, Medical Herb x1, Cup of Wishes x2, Chocolate x1, Royal Jam x2, Faerie Walnut x1
WRITE CC48 602445214223
# weapons owned: sword (already), axe tier 2, spear tier 1, javelin tier 3
WRITE CC56 0201
WRITE CC5B 03
RUN 60
"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--rom', default=str(common.ROM_VANILLA))
    ap.add_argument('--tag', default='vanilla')
    ap.add_argument('--route-frames', type=int, default=23000)
    ap.add_argument('--route', default=str(common.ROUTE))
    a = ap.parse_args()
    common.WORK.mkdir(parents=True, exist_ok=True)
    base = common.WORK / ('base_%s.state' % a.tag)
    script = 'SPANS %s %d\nRUN 5\nSAVE %s\nSTATE base\n' % (a.route, a.route_frames, base)
    out = common.run_host(script, rom=a.rom, name='fixture_base_' + a.tag)
    print(out[-1]['mode'], 'map', out[-1]['map'])
    party = common.WORK / ('party3_%s.state' % a.tag)
    script = 'LOAD %s\nRUN 2\n%s\nSAVE %s\nSTATE party\n' % (base, FIXTURE, party)
    out = common.run_host(script, rom=a.rom, name='fixture_party_' + a.tag)
    p = out[-1]
    print(p['mode'], [(h['present'], h['hp'], h['mp']) for h in p['party']], p['items'][:6])


if __name__ == '__main__':
    main()
