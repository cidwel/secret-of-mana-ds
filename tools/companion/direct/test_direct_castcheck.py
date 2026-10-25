#!/usr/bin/env python3
"""Cast availability: the game's own ring (ground truth) versus somd_can_cast for every status bit, MP short and busy casters.

    python3 test_direct_castcheck.py VARIANT [STATE]
For each status bit set on the girl (timers loaded long enough to outlast the macro), the real ring macro tries to cast spell 3
and the trace says whether the cast pose started; somd_can_cast must say OK exactly when it did.
"""
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest  # noqa: E402
import test_direct_cast as T  # noqa: E402

RES = []
GIRL = 0xE200
BITS = [0x0004, 0x0008, 0x0010, 0x0020, 0x0040, 0x0080, 0x0100, 0x0200, 0x0400, 0x0800, 0x1000, 0x2000, 0x4000, 0x8000]
TIMER = {0x007C: 0x1B2, 0x0180: 0x1B3, 0x1800: 0x1B5, 0x6000: 0x1B6}


def check(name, ok, **kw):
    r = dict(check=name, ok=bool(ok), **kw)
    RES.append(r)
    print(json.dumps(r), flush=True)


def pre_for(status, mp=None):
    lines = ['WRITE %X %02X%02X' % (GIRL + 0x190, status & 255, status >> 8)]
    for m, off in TIMER.items():
        if status & m:
            lines.append('WRITE %X 90' % (GIRL + off))
    if mp is not None:
        lines.append('WRITE %X %02X' % (GIRL + 0x186, mp))
    return '\n'.join(lines) + '\n'


def ring_cast(variant, state, pre):
    res = T.run_script(variant, 'LOAD %s\nRUN 2\n%sCMD cast 1 0 0\nTRACE %X 512 900 r\n' % (state, pre, GIRL), 'castcheck')
    m = T.metrics([j for j in res if 'trace' in j], 1)
    mac = [j for j in res if j.get('event') == 'refused']
    return m is not None, (mac[0]['result'] if mac else None)


def main(variant, state=None):
    D = dtest.Direct(variant)
    st = state or dtest.STATES[variant]
    mask = 0xD5F0           # ring mask D1F0 plus the Barrel disguise 0400 (the ring cannot complete a cast with it)
    for bit in BITS:
        D.load(st, 4)
        D.core.write(GIRL + 0x190, bytes([bit & 255, bit >> 8]))
        for m, off in TIMER.items():
            if bit & m:
                D.core.write(GIRL + off, bytes([0x90]))
        reason, ci = D.can_cast(1, 0, 0)
        cast, refused = ring_cast(variant, st, pre_for(bit))
        expect_block = bool(bit & mask)
        check('status-%04x' % bit, cast == (reason == 'ok') and cast == (not expect_block), ring_cast=cast, ring_refused=refused, reason=reason,
              eta=ci.eta_frames, mask_blocks=expect_block)
    # MP short: ring and direct agree
    D.load(st, 4)
    D.core.write(GIRL + 0x186, bytes([0]))
    reason, ci = D.can_cast(1, 0, 0)
    cast, refused = ring_cast(variant, st, pre_for(0, mp=0))
    check('mp-zero', (not cast) and reason == 'no-mp', ring_cast=cast, ring_refused=refused, reason=reason, mp_cost=ci.mp_cost, mp_have=ci.mp_have)
    D.load(st, 4)
    D.core.write(GIRL + 0x186, bytes([ci.mp_cost]))
    reason, ci = D.can_cast(1, 0, 0)
    cast, refused = ring_cast(variant, st, pre_for(0, mp=ci.mp_cost))
    check('mp-exact', cast and reason == 'ok', ring_cast=cast, reason=reason, mp_have=ci.mp_have)
    # reason table at the state level
    D.load(st, 4)
    check('reason-ok', D.can_cast(1, 0, 0)[0] == 'ok')
    check('reason-boy', D.can_cast(0, 0, 0)[0] == 'bad-argument')
    D.core.write(GIRL + 0x1C8 + 2, bytes([0]))
    check('reason-not-owned', D.can_cast(1, 2, 0)[0] == 'not-owned')
    D.load(st, 4)
    D.core.write(GIRL + 0x60, bytes([5]))
    check('reason-busy', D.can_cast(1, 0, 0)[0] == 'busy')
    for name, addr, val, want in (('event', 0xD0, 0x81, 'event'), ('fade', 0xE6, 5, 'fade'), ('lock', 0xF1, 0x80, 'input-lock'), ('ring', 0x7D, 1, 'ring-open')):
        D.load(st, 4)
        D.core.write(addr, bytes([val]))
        check('reason-' + name, D.can_cast(1, 0, 0)[0] == want)
    D.load(st, 4)
    D.core.write(GIRL + 0x190, bytes([0x80, 0]))
    D.core.write(GIRL + 0x1B3, bytes([4]))
    r, ci = D.can_cast(1, 0, 0)
    check('silence-eta', r == 'silenced' and ci.eta_frames == 80 and ci.will_clear == 1, eta=ci.eta_frames)
    # the direct cast refuses what the ring refuses
    D.load(st, 4)
    D.core.write(GIRL + 0x190, bytes([0x80, 0]))
    D.core.write(GIRL + 0x1B3, bytes([200]))
    check('direct-refuses-silenced', D.cast(1, 0, 0, 1, 1) == 'status')
    return all(r['ok'] for r in RES)


if __name__ == '__main__':
    ok = main(*sys.argv[1:3])
    (dtest.STATE / ('castcheck_%s.json' % sys.argv[1])).write_text(json.dumps(RES, indent=1))
    sys.exit(0 if ok else 1)
