#!/usr/bin/env python3
"""Target classes, group casts, tap-to-target and failure paths, run through som_direct on a state with enemies.

    python3 test_direct_targets.py VARIANT STATE
Prints one JSON line per check ({"check", "ok", ...}); evidence file targets_<variant>_<state>.json.
"""
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest  # noqa: E402

RESULTS = []


def check(name, ok, **kw):
    rec = dict(check=name, ok=bool(ok), **kw)
    RESULTS.append(rec)
    print(json.dumps(rec), flush=True)


def hp(c, s):
    return c.u16(0xE182 + 0x200 * s)


def main(variant, state):
    D = dtest.Direct(variant)
    c = D.core
    st = pathlib.Path(state)
    tag = st.stem

    def fresh(settle=2):
        D.load(st, settle)

    # --- scan and classes ------------------------------------------------------------------
    fresh()
    sc = D.scan(3, 0, 1)      # enemy one-or-all
    check('scan-enemy-class', sc.n_enemies_valid >= 1 and sc.n_allies_valid == 0,
          enemies=[(a.slot, a.sx, a.sy, a.hp, a.valid) for a in sc.actor[3:]], card=D.group_card(sc))
    sa = D.scan(2, 0, 1)      # ally one-or-all
    check('scan-ally-class', sa.n_allies_valid == 3 and sa.n_enemies_valid == 0, allies=[(a.slot, a.sx, a.sy, a.valid) for a in sa.actor[:3]], card=D.group_card(sa))

    # --- single enemy, non default slot ------------------------------------------------------
    valid = [a.slot for a in sc.actor[3:] if a.valid]
    if len(valid) >= 2:
        tgt = valid[1]
        fresh()
        before = [hp(c, s) for s in (3, 4, 5)]
        rc = D.cast(2, 0, 0, 1, tgt)
        D.wait()
        c.run(420)
        after = [hp(c, s) for s in (3, 4, 5)]
        hit = [i + 3 for i in range(3) if after[i] < before[i]]
        req = bytes(c.read(0xE400 + 0x170, 11)).hex()
        check('cast-one-enemy-slot%d' % tgt, rc == 'ok' and tgt in hit, before=before, after=after, request=req)
    # --- all enemies ----------------------------------------------------------------------------
    fresh()
    before = [hp(c, s) for s in (3, 4, 5)]
    rc = D.cast(2, 0, 0, 3)
    D.wait()
    req = bytes(c.read(0xE400 + 0x170, 11)).hex()
    c.run(420)
    after = [hp(c, s) for s in (3, 4, 5)]
    n_valid = sc.n_enemies_valid
    check('cast-all-enemies', rc == 'ok' and sum(1 for i in range(3) if after[i] < before[i]) == n_valid, before=before, after=after, request=req, n_valid=n_valid)
    # --- all allies (Cure Water, girl, spirit Undine slot 2) ---------------------------------
    fresh()
    for h in range(3):
        c.write(0xE182 + 0x200 * h, bytes([5, 0]))
    before = [hp(c, h) for h in range(3)]
    rc = D.cast(1, 1, 2, 2)
    D.wait()
    req = bytes(c.read(0xE200 + 0x170, 11)).hex()
    c.run(400)
    after = [hp(c, h) for h in range(3)]
    check('cast-all-allies', rc == 'ok' and all(after[h] > before[h] for h in range(3)), before=before, after=after, request=req)
    # --- one ally that is not the caster ---------------------------------------------------
    fresh()
    c.write(0xE182 + 0x200 * 0, bytes([25, 0]))
    before = [hp(c, h) for h in range(3)]
    rc = D.cast(1, 1, 2, 1, 0)
    D.wait()
    req = bytes(c.read(0xE200 + 0x170, 11)).hex()
    peak = [hp(c, h) for h in range(3)]
    for _ in range(10):
        c.run(50)
        peak = [max(peak[h], hp(c, h)) for h in range(3)]
    after = [hp(c, h) for h in range(3)]
    # peak HP: monsters may hit the target after the heal lands in these fixtures
    check('cast-one-other-ally', rc == 'ok' and peak[0] > before[0] and peak[2] <= before[2], before=before, peak=peak, after=after, request=req)

    # --- refusals -----------------------------------------------------------------------------
    fresh()
    check('refuse-ally-spell-on-enemy', D.cast(1, 1, 2, 1, 3) == 'bad-target')
    check('refuse-enemy-spell-on-ally', D.cast(2, 0, 0, 1, 0) == 'bad-target')
    fresh()
    check('refuse-analyzer-group', D.cast(1, 3, 2, 3) == 'bad-target')
    check('refuse-boy-caster', D.cast(0, 0, 0, 1, 3) == 'bad-argument')
    fresh()
    c.write(0xE586, bytes([0]))
    check('refuse-no-mp', D.cast(2, 0, 0, 1, 3) == 'no-mp')
    fresh()
    c.write(0xE3C8 + 0, bytes([0]))
    check('refuse-spirit-not-owned', D.cast(1, 0, 0, 2) == 'bad-argument')
    fresh()
    vslot = next(a.slot for a in D.scan(3, 0, 1).actor[3:] if a.valid)
    rc1 = D.cast(2, 0, 0, 1, vslot)
    rc2 = D.cast(2, 0, 1, 1, vslot)
    check('refuse-busy', rc1 == 'ok' and rc2 == 'busy', rc1=rc1, rc2=rc2)
    D.wait()
    c.run(5)
    rc3 = D.cast(2, 0, 1, 1, vslot)
    check('refuse-caster-busy', rc3 == 'caster-busy', rc=rc3)
    # unsafe states
    for name, addr, val in (('event', 0xD0, 0x81), ('ring', 0x7D, 1), ('fade', 0xE6, 0x05), ('input-lock', 0xF1, 0x80)):
        fresh()
        old = c.u8(addr)
        c.write(addr, bytes([val]))
        r = (D.cast(2, 0, 0, 1, 3), D.equip(0, 2), D.item(0, 1, 0))
        c.write(addr, bytes([old]))
        check('unsafe-' + name, all(x == 'unsafe-state' for x in r), results=r)
    # rollback: the game leaves field mode before the first call
    fresh()
    before = bytes(c.read(0xCC54, 8)) + bytes(c.read(0xE1E3, 6))
    rc = D.equip(0, 2)
    c.write(0xD0, bytes([0x81]))
    D.wait(40)
    c.write(0xD0, bytes([0]))
    after = bytes(c.read(0xCC54, 8)) + bytes(c.read(0xE1E3, 6))
    check('rollback-on-event', rc == 'ok' and D.state == 'failed' and D.err == 'unsafe-state' and before == after, state=D.state, err=D.err)
    # modal items
    fresh()
    for rec in range(12):
        v = c.u8(0xCC48 + rec)
        if v != 0xFF and (v & 31) in (6, 7):
            check('refuse-modal-item-%d' % (v & 31), D.item(rec, 1, 0) == 'needs-ring')
    # no enemy on screen
    fresh()
    for s in (3, 4, 5):
        c.write(0xE000 + 0x200 * s + 0x20, bytes([0, 1]))        # push them off screen
    check('refuse-no-target', D.cast(2, 0, 0, 3) in ('no-target',), result=D.cast(2, 0, 0, 3))

    out = dtest.STATE / ('targets_%s_%s.json' % (variant, tag))
    bad = sum(1 for r in RESULTS if not r['ok'])
    out.write_text(json.dumps(dict(variant=variant, state=tag, checks=len(RESULTS), failed=bad, results=RESULTS), indent=1))
    print('checks', len(RESULTS), 'failed', bad)
    return bad


if __name__ == '__main__':
    sys.exit(1 if main(*sys.argv[1:3]) else 0)
