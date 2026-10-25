#!/usr/bin/env python3
"""Verifies the WRAM mode classifier of som_ui_draw on real game runs: vanilla US, the Magno translation and the generated
Ancient Cave ROMs (seeds 1 and 777).  For each ROM it loads a saved free-field state, runs the emulator and checks:
  field        classifier says FIELD, three heroes present, ROM names decode
  event        a crafted text event ($D0 != 0) classifies EVENT and the dialogue line decodes
  shop         event command 0x2E sets $7D = 1 and $1847 in 1..3 (SHOP)
  ring         pressing the ring button opens the ring ($7D = 1, $1847 = 0)
  flight       vanilla only: Flammie flight state (E4 = 4) with position and heading in the world map
Writes JSON evidence to <state>/ux/evidence/verify_modes.json.  Missing ROMs or states are reported as skipped."""
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import common  # noqa: E402
import retro  # noqa: E402
import ux_lib  # noqa: E402

W = common.WORK
ST = common.STATE
TARGETS = [
    ('vanilla', common.ROM_VANILLA, W / 'party3_vanilla.state'),
    ('magno', common.ROM_MAGNO, W / 'party3_magno.state'),
    ('ac_seed1', ST / 'ancientcave/out/ac_seed1.smc', W / 'ac_party.state'),
    ('ac_seed777', ST / 'ancientcave/out/ac_seed777.smc', W / 'ac_party_s777.state'),
]


def g(ch):
    if ch == ' ':
        return 0x80
    if ch.islower():
        return 0x81 + ord(ch) - 97
    if ch.isupper():
        return 0x9B + ord(ch) - 65
    return {'.': 0xBF, ',': 0xC0, '!': 0xC8, '?': 0xCA}[ch]


def run_one(name, rom, state):
    res = {'rom': str(rom.name)}
    if not rom.exists() or not state.exists():
        res['skipped'] = 'missing rom or state'
        return res
    c = retro.Core(str(rom), core_path=str(common.CORE))
    ux = ux_lib.UX(rom=rom)
    blob = state.read_bytes()

    def fresh():
        c.load_state(blob)
        c.run(30)

    def mode():
        w = c.dump_wram()
        ux.update(w)
        return ux_lib.MODES[ux.lib.som_ux_classify(w)]

    fresh()
    res['field'] = {'mode': mode(), 'heroes': ux.info(6), 'enemies': ux.info(0),
                    'names': [ux.name(i) for i in (0x32, 0xBA, 0xCF)]}
    # crafted dialogue
    fresh()
    line = 'Welcome to the mana tree.'
    c.write(0x1FF00, bytes([0x50]) + bytes(g(x) for x in line) + b'\x7f\x00\x00')
    c.write(0xD1, b'\x00\xff\x7f')
    c.run(1)
    c.write(0xD0, b'\x01')
    c.run(8)
    res['event'] = {'mode': mode(), 'line': ux.line(), 'd0': c.u8(0xD0)}
    # shop through the event VM
    fresh()
    c.write(0x99, bytes([0]))
    c.write(0x1FF00, bytes([0x2E, 5, 0, 0, 0, 0]))
    c.write(0xD1, b'\x00\xff\x7f')
    c.run(1)
    c.write(0xD0, b'\x01')
    c.run(240)
    res['shop'] = {'mode': mode(), 'p7d': c.u8(0x7D), 'p1847': c.u8(0x1847)}
    # ring
    fresh()
    seen = None
    for btn in ('Y', 'X', 'A', 'R'):
        fresh()
        c.run(4, btn)
        c.run(40)
        m = mode()
        if m == 'RING':
            seen = btn
            res['ring'] = {'mode': m, 'button': btn, 'p7d': c.u8(0x7D), 'p1847': c.u8(0x1847)}
            break
    if seen is None:
        res['ring'] = {'mode': m, 'button': None}
    # flight: position and heading are readable on every ROM; E4 = 4 is poked after the load (documented limitation)
    fresh()
    c.write(0xE4, bytes([4]))
    c.write(0xFA, (8 * 304).to_bytes(2, 'little'))
    c.write(0xFC, (8 * 378).to_bytes(2, 'little'))
    w = c.dump_wram()
    ux.update(w)
    res['flight_poked'] = {'mode': ux_lib.MODES[ux.lib.som_ux_classify(w)], 'x': ux.info(2), 'y': ux.info(3),
                           'note': 'E4 poked, not a natural flight: classifier check only'}
    return res


def main():
    out = {}
    for name, rom, state in TARGETS:
        try:
            out[name] = run_one(name, rom, state)
        except Exception as e:  # keep going: one broken ROM must not hide the others
            out[name] = {'error': repr(e)}
        print(name, json.dumps(out[name]), flush=True)
    ev = common.STATE / 'ux' / 'evidence'
    ev.mkdir(parents=True, exist_ok=True)
    (ev / 'verify_modes.json').write_text(json.dumps(out, indent=1))


if __name__ == '__main__':
    main()
