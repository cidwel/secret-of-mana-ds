#!/usr/bin/env python3
"""Gets the generated Ancient Cave ROM through the title, the name entry and the intro dialogue, and saves
core states.  No ROM data is stored in the repository; states go to $SOM_COMPANION_STATE/work.

Why earlier attempts stalled: the name entry rejects a name equal to another hero's (START is ignored), so
typing "A" three times never leaves the sprite screen.  Names "A", "B", "C" (cursor moved RIGHT) work.

    python3 build_ac_fixture.py        # writes ac_base.state (free play on floor 1) and ac_party.state
"""
import sys

import acprobe as ac

# fixture pokes, the same idea as tools/companion/build_fixture.py: spirits/MP for the girl and sprite
PARTY_POKES = [
    (0xE3C4, bytes.fromhex('12345678')), (0xE5C4, bytes.fromhex('12345678')),
    (0xE3C8, bytes.fromhex('3F3F3F3F3F3F3F3F')), (0xE5C8, bytes.fromhex('3F3F3F3F3F3F3F3F')),
]


def name_entry(c):
    c.run(300)
    ac.tap(c, 'START')
    c.run(410)                       # title -> game select
    c.run(5, 'B')
    c.run(300)                       # NEW GAME -> name entry (boy)
    for i in range(3):
        for _ in range(i):           # a different letter per hero: A, B, C
            c.run(4, 'RIGHT')
            c.run(30)
        c.run(5, 'B')
        c.run(60)
        c.run(5, 'START')
        c.run(400)
    # floor-1 start message ($D0 != 0): B until the event VM is idle
    quiet = 0
    for _ in range(40):
        c.run(4, 'B')
        c.run(60)
        quiet = quiet + 1 if c.u8(0xD0) == 0 else 0
        if quiet >= 3:
            return True
    return False


def main():
    c = ac.open_core()
    ok = name_entry(c)
    c.run(120)
    print('free play:', ok, 'D0', c.u8(0xD0), 'hero0', c.u8(0xE000), 'frame', c.frames)
    if not ok:
        sys.exit(1)
    ac.BASE.write_bytes(c.save_state())
    for a, b in PARTY_POKES:
        c.write(a, b)
    c.run(60)
    ac.PARTY.write_bytes(c.save_state())
    ac.png(c, ac.OUT / 'base.png')
    print('saved', ac.BASE, ac.PARTY)


if __name__ == '__main__':
    main()
