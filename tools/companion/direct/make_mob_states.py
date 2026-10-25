#!/usr/bin/env python3
"""Saves party states placed on maps with enemies:  make_mob_states.py VARIANT TRANS [TRANS...]
The game's own warp event (opcode 0x18) moves the fixture party; MP of the girl and sprite is raised so casts are affordable."""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest  # noqa: E402


def warp(c, n):
    c.write(0x1FF00, bytes([0x18 | (n >> 8), n & 0xFF, 0]))
    c.write(0xD1, b'\x00\xff\x7f')
    c.step()
    c.write(0xD0, b'\x01')


def trans_map(rom, n):
    o = 0x083000 + 4 * n
    return (rom[o] | rom[o + 1] << 8) & 0x1FF


def inject(variant, donor):
    """Ancient Cave floor 1 has no enemies: copy the three monster records of a vanilla enemy map into this state and move
    them next to the party.  The monsters run the real game code (they attack and can be hit); only their sprites are
    wrong because the donor's graphics are not loaded."""
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
    import dcore
    D = dtest.Direct(variant)
    blob = pathlib.Path(donor).read_bytes()
    off, n = dcore.blocks(blob)['RAM']
    ram = blob[off:off + n]
    D.load()
    D.core.run(10)
    c = D.core
    c.write(0xE600, ram[0xE600:0xEC00])
    hx, hy = c.u16(0xE002), c.u16(0xE004)
    for s, (dx, dy) in zip((3, 4, 5), ((0, -50), (-30, -30), (30, -30))):
        b = 0xE000 + 0x200 * s
        c.write(b + 2, (hx + dx).to_bytes(2, 'little'))
        c.write(b + 4, (hy + dy).to_bytes(2, 'little'))
    for h in (1, 2):
        c.write(0xE000 + 0x200 * h + 0x186, bytes([60]))
        c.write(0xE000 + 0x200 * h + 0x187, bytes([60]))
    c.run(1)
    path = dtest.STATE / ('mob_%s_inj.state' % variant)
    path.write_bytes(c.save_state())
    print(variant, 'injected ->', path.name, [(s, c.u8(0xE000 + 0x200 * s), c.u16(0xE020 + 0x200 * s), c.u16(0xE022 + 0x200 * s)) for s in range(3, 6)])


if __name__ == '__main__' and sys.argv[1] == '--inject':
    inject(sys.argv[2], sys.argv[3])
elif __name__ == '__main__':
    v = sys.argv[1]
    D = dtest.Direct(v)
    base = dtest.STATES[v].read_bytes()
    for n in map(int, sys.argv[2:]):
        D.core.load_state(base)
        D.core.run(2)
        warp(D.core, n)
        D.core.run(200)
        m = D.core.u16(0xDC)
        for h in (1, 2):
            D.core.write(0xE000 + 0x200 * h + 0x186, bytes([60]))
            D.core.write(0xE000 + 0x200 * h + 0x187, bytes([60]))
        D.core.run(30)
        path = dtest.STATE / ('mob_%s_%d.state' % (v, m))
        path.write_bytes(D.core.save_state())
        mobs = []
        for s in range(3, 6):
            b = 0xE000 + 0x200 * s
            r = D.core.read(b, 0x200)
            mobs.append((s, r[0], r[0x20] | r[0x21] << 8, r[0x22] | r[0x23] << 8, r[0x182] | r[0x183] << 8, hex(r[0x180])))
        print(v, 'map', m, 'trans', n, '->', path.name, mobs)


