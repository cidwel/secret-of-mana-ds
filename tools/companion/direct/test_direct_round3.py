#!/usr/bin/env python3
"""Round 3 direct-layer checks on the vanilla US ROM: name entry direct text, hero switch by tap, real-time menu jumps.

    python3 test_direct_round3.py
Exit status 0 when every check passes.  Nothing from the ROM is stored."""
import pathlib
import struct
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest  # noqa: E402

FAILS = []


def check(name, ok, info=''):
    print(('PASS ' if ok else 'FAIL ') + name + (' ' + str(info) if info else ''))
    if not ok:
        FAILS.append(name)


def to_name_entry(c):
    c.run(3300); c.run(6, ['START']); c.run(400); c.run(6, ['B']); c.run(500)


def glyph(ch):
    return 0x9B + ord(ch) - 65


def main():
    D = dtest.Direct('vanilla')
    c = D.core
    to_name_entry(c)
    # --- name entry: direct text equals what the pad does ---
    ref = None
    for ch in 'LINK':
        check('letter %s accepted' % ch, D.name_letter(ord(ch) - 65) == 'ok')
        c.run(2)
    buf = c.read(0x9000 + 3 * 64, 64) + c.read(0x9000 + 4 * 64, 64)
    check('count 4', c.u8(0xA1CC) == 4, c.u8(0xA1CC))
    check('delete', D.name_delete() == 'ok' and c.u8(0xA1CC) == 3)
    check('retype', D.name_letter(ord('K') - 65) == 'ok')
    for ch in 'AB':
        r = D.name_letter(ord(ch) - 65)
    check('name full is refused (6 letters)', D.name_letter(0) == 'no-target', c.u8(0xA1CC))
    for _ in range(8):
        D.name_delete()
    check('delete on empty is refused', D.name_delete() == 'no-target')
    for ch in 'LINK':
        D.name_letter(ord(ch) - 65)
    c.run(4)
    c.save_ppm('/home/cidwel/.local/state/som-decomp/companion-20261018/shots/ux/name_direct_test.ppm')
    c.run(30); c.run(3, ['START']); c.run(200)
    name = bytes(c.read(0xCC00, 5))
    check('confirmed name is LINK', name == bytes([glyph(x) for x in 'LINK']) + b'\x00', name.hex())
    # pad reference on a second run is covered by android/tests/name_entry_start.py (same result: a6 a3 a8 a5)
    check('name entry refused outside the screen', D.name_letter(3) == 'unsafe-state')

    # --- hero switch and real-time menu jumps on the party state ---
    D.load(None, 4)
    pad_hero = lambda: c.u8(0xD9)
    for h, want in ((1, 2), (2, 4), (0, 1)):
        r = D.switch_hero(h)
        D.wait(120)
        check('switch to hero %d' % h, r == 'ok' and D.state == 'finished' and pad_hero() == want, (r, D.state, pad_hero()))
    check('switch to the controlled hero is a no-op', D.switch_hero(0) == 'ok' and D.frames == 0)
    check('hero 3 is refused', D.switch_hero(3) == 'bad-argument')
    c.write(0xD0, bytes([0x81]))
    check('refused during an event', D.switch_hero(1) == 'unsafe-state')
    c.write(0xD0, bytes([0]))
    check('menu jump is not hidden by default', D.open_menu(1, 0, 3) == 'ok' and not any([D.frame() or D.hidden() for _ in range(60)]))
    D.wait(300)
    check('ring is open after the real-time jump', c.u8(0x7D) == 1 and c.u8(0x1809) == 3)
    check('switch refused while the ring is open', D.switch_hero(1) == 'unsafe-state')
    D.load(None, 4)
    D.fast = True
    D.load(None, 4)
    D.open_menu(1, 0, 3)
    D.frame()
    check('fast mode still available for tests', D.hidden() == 1)
    print('FAILED: %d' % len(FAILS) if FAILS else 'ALL PASS')
    return not FAILS


if __name__ == '__main__':
    sys.exit(0 if main() else 1)
