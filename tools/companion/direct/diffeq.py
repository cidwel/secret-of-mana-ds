"""Differential equip test: the ring macro (C host) versus som_direct from the same saved state."""
import json
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest  # noqa: E402
import common  # noqa: E402

WORK = common.WORK


def ring_dump(variant, state, hero, want, tag):
    import os
    out = dtest.STATE / ('ring_%s_%d.bin' % (tag, os.getpid()))
    body = 'LOAD %s\nRUN 2\nCMD equip %d %d\nWAIT 900\nRUN 60\nDUMP %s\nSTATE a\n' % (state, hero, want, out)
    res = common.run_host(body, rom=dtest.ROMS[variant], name='ring_%s_%d' % (tag, os.getpid()))
    macro = [r for r in res if r.get('event') in ('macro', 'refused')][0]
    return out.read_bytes(), macro


def ranges(a, b, lo, hi):
    return [(i, a[i], b[i]) for i in range(lo, hi) if a[i] != b[i]]


if __name__ == '__main__':
    variant, state, hero, want = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
    ring, macro = ring_dump(variant, state, hero, want, 'probe')
    print('ring macro', macro)
    D = dtest.Direct(variant)
    D.load(state)
    print('equip ->', D.equip(hero, want))
    n = D.wait()
    used = D.frames
    D.core.run(299 - used + 0)
    direct = D.core.dump_wram()
    for name, lo, hi in (('heroes', 0xE000, 0xE600), ('inventory', 0xCC00, 0xCC80)):
        print(name, [(hex(i), hex(x), hex(y)) for i, x, y in ranges(ring, direct, lo, hi)])
