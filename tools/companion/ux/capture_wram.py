#!/usr/bin/env python3
"""Captures real WRAM images of every UI mode from the game (vanilla US) into <state>/ux/wram/ for the mock renderer.

Needs the companion fixtures (tools/companion/build_fixture.py, android/advstate) and the user's ROM.  The dumps contain
game state and stay in the state directory.  Usage: capture_wram.py [rom_key]  (rom_key: vanilla)
"""
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import common  # noqa: E402
import retro  # noqa: E402

OUT = common.STATE / 'ux' / 'wram'
OUT.mkdir(parents=True, exist_ok=True)
manifest = {}


def save(name, c, note):
    (OUT / (name + '.bin')).write_bytes(c.dump_wram())
    manifest[name] = note
    print(name, '-', note, flush=True)


def newgame_events():
    ev = {}
    for ln in open(common.NEWGAME):
        if ln.startswith('#') or not ln.strip():
            continue
        a, b, btn = ln.split()
        ev[(int(a), int(b))] = btn
    return ev


def main():
    c = retro.Core(str(common.ROM_VANILLA), core_path=str(common.CORE))
    adv = common.STATE / 'adv'
    for n in ('215', '222', '232'):
        c.load_state(open(adv / ('adv%s.state' % n), 'rb').read())
        c.run(120)
        save('field_%s' % n, c, 'advanced fixture, map %s, free field' % n)
    # shops through the event VM: command 0x2E buy, 0x0E sell, 0x0F upgrade
    for nm, op in (('buy', 0x2E), ('sell', 0x0E), ('upgrade', 0x0F)):
        c.load_state(open(adv / 'advanced_vanilla.state', 'rb').read())
        c.run(5)
        c.write(0x99, bytes([0]))
        c.write(0x1FF00, bytes([op, 5, 0, 0, 0, 0]))
        c.write(0xD1, b'\x00\xff\x7f')
        c.run(1)
        c.write(0xD0, b'\x01')
        c.run(240)
        save('shop_' + nm, c, 'shop event command 0x%02X' % op)
    # flight from Potos with the Flammie Drum (record 0 poked to the Drum): see the host script in the doc
    # (the dump comes from the host macro run; see run_flight below)
    # dialogue lines through crafted events (text blocks: 0x50 then glyph codes)
    def g(ch):
        if ch == ' ':
            return 0x80
        if ch.islower():
            return 0x81 + ord(ch) - 97
        if ch.isupper():
            return 0x9B + ord(ch) - 65
        return {'.': 0xBF, ',': 0xC0, '!': 0xC8, '?': 0xCA}[ch]
    lines = ["Welcome to Potos, traveler.", "The Mana Fortress is rising again!", "We have to find the sword in the falls.",
             "Be careful out there, Randi."]
    c.load_state(open(common.STATE / 'work' / 'party3_vanilla.state', 'rb').read())
    c.run(5)
    for ln in lines:
        ev = bytes([0x50]) + bytes(g(x) for x in ln) + b'\x7f\x00\x00'
        c.write(0x1FF00, ev)
        c.write(0xD1, b'\x00\xff\x7f')
        c.run(1)
        c.write(0xD0, b'\x01')
        c.run(8)
        (OUT / ('event_line%d.bin' % lines.index(ln))).write_bytes(c.dump_wram())
        manifest['event_line%d' % lines.index(ln)] = 'crafted text event, D0=%02X' % c.u8(0xD0)
    # title, game select and name entry from the new-game script
    ev = newgame_events()
    c2 = retro.Core(str(common.ROM_VANILLA), core_path=str(common.CORE))
    want = {100: 'boot', 650: 'frontend_select', 900: 'frontend_name', 4700: 'event_flyover', 1400: 'event_cutscene'}
    f = 0
    while f < 4701:
        btn = None
        for (a, b), bt in ev.items():
            if a <= f < b:
                btn = bt
        c2.run(1, btn)
        f += 1
        if f in want:
            save(want[f], c2, 'new-game script frame %d' % f)
    json.dump(manifest, open(OUT / 'manifest.json', 'w'), indent=1)


if __name__ == '__main__':
    main()
