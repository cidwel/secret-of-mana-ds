#!/usr/bin/env python3
"""Cast queue: silence + queue, busy + queue, KO / no MP not queued, rapid replacement, cancel, timeout, direct cast cancels.

    python3 test_direct_queue.py VARIANT [STATE]
"""
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest  # noqa: E402

RES = []
GIRL = 0xE200
MOB = {'vanilla': 'mob_vanilla_344', 'magno': 'mob_magno_365', 'ac1': 'mob_ac1_inj', 'ac777': 'mob_ac777_inj'}


def check(name, ok, **kw):
    r = dict(check=name, ok=bool(ok), **kw)
    RES.append(r)
    print(json.dumps(r), flush=True)


def mp(D):
    return D.core.read(GIRL + 0x186, 1)[0]


def run(D, n, until=None):
    for i in range(n):
        D.frame()
        if until and until():
            return i + 1
    return None


def qs(D, h=1):
    s, q = D.queue_state(h)
    return s, q


def main(variant, state=None):
    D = dtest.Direct(variant)
    st = state or dtest.STATES[variant]

    cur = [st]

    def fresh():
        D.load(cur[0], 4)
        D.core.write(GIRL + 0x186, bytes([60]))

    # silence + queue: fires when the status expires, MP drops (cast happened)
    fresh()
    D.core.write(GIRL + 0x190, bytes([0x80, 0]))
    D.core.write(GIRL + 0x1B3, bytes([3]))
    m0 = mp(D)
    e, ci = D.queue_cast(1, 0, 0, timeout=3000)
    s, q = qs(D)
    check('silence-queued', e == 'ok' and s == 'waiting' and dtest.REASONS[q.reason] == 'silenced', err=e, state=s, reason=dtest.REASONS[q.reason])
    n = run(D, 1500, lambda: qs(D)[0] != 'waiting')
    s, q = qs(D)
    cleared = (D.core.read(GIRL + 0x190, 2)[0] | D.core.read(GIRL + 0x190, 2)[1] << 8) & 0x80 == 0
    run(D, 400, lambda: mp(D) < m0)
    check('silence-fires-on-expiry', s == 'fired' and cleared and mp(D) < m0, frames_waited=n, state=s, mp=(m0, mp(D)))
    D.queue_ack(1) if hasattr(D, 'queue_ack') else None

    # busy + queue
    fresh()
    D.core.write(GIRL + 0x60, bytes([5]))
    m0 = mp(D)
    e, ci = D.queue_cast(1, 0, 0)
    s, q = qs(D)
    first = s == 'waiting' and dtest.REASONS[q.reason] == 'busy'
    D.core.write(GIRL + 0x60, bytes([0]))
    n = run(D, 200, lambda: qs(D)[0] != 'waiting')
    run(D, 400, lambda: mp(D) < m0)
    s, q = qs(D)
    check('busy-queue', first and s == 'fired' and mp(D) < m0, waited_frames=n, state=s, mp=(m0, mp(D)))

    # KO caster: not queued
    fresh()
    D.core.write(GIRL + 0x190, bytes([0, 0x80]))
    e, ci = D.queue_cast(1, 0, 0)
    s, q = qs(D)
    check('ko-not-queued', e == 'ko' and s == 'empty', err=e, state=s)

    # no MP: not queued
    fresh()
    D.core.write(GIRL + 0x186, bytes([0]))
    e, ci = D.queue_cast(1, 0, 0)
    s, q = qs(D)
    check('no-mp-not-queued', e == 'no-mp' and s == 'empty', err=e, state=s)

    # not owned: not queued
    fresh()
    D.core.write(GIRL + 0x1C8 + 2, bytes([0]))
    e, ci = D.queue_cast(1, 2, 0)
    s, q = qs(D)
    check('not-owned-not-queued', dtest.REASONS[ci.reason] == 'not-owned' and e == 'bad-argument' and s == 'empty', err=e, reason=dtest.REASONS[ci.reason], state=s)

    # rapid replacement
    fresh()
    D.core.write(GIRL + 0x60, bytes([5]))
    D.queue_cast(1, 0, 0)
    s1, q1 = qs(D)
    id1 = q1.spell_id
    e, ci = D.queue_cast(1, 1, 0)
    s2, q2 = qs(D)
    D.core.write(GIRL + 0x60, bytes([0]))
    m0 = mp(D)
    run(D, 200, lambda: qs(D)[0] != 'waiting')
    s3, q3 = qs(D)
    check('rapid-replacement', s1 == 'waiting' and s2 == 'waiting' and q2.spell_id != id1 and s3 == 'fired' and q3.spell_id == q2.spell_id,
          first=id1, second=q2.spell_id, final=(s3, q3.spell_id))

    # cancel
    fresh()
    D.core.write(GIRL + 0x60, bytes([5]))
    D.queue_cast(1, 0, 0)
    D.queue_cancel(1)
    s, q = qs(D)
    D.core.write(GIRL + 0x60, bytes([0]))
    m0 = mp(D)
    run(D, 150)
    check('cancel', s == 'dropped' and dtest.ERR[q.err] == 'cancelled' and mp(D) == m0 and qs(D)[0] != 'fired', state=s, err=dtest.ERR[q.err])

    # timeout
    fresh()
    D.core.write(GIRL + 0x60, bytes([5]))
    D.queue_cast(1, 0, 0, timeout=30)
    run(D, 60, lambda: qs(D)[0] != 'waiting')
    s, q = qs(D)
    check('timeout', s == 'dropped' and dtest.ERR[q.err] == 'expired', state=s, err=dtest.ERR[q.err], waited=q.waited)

    # immediate fire when castable
    fresh()
    m0 = mp(D)
    e, ci = D.queue_cast(1, 0, 0)
    run(D, 150, lambda: mp(D) < m0)
    check('immediate-fire', e == 'ok' and mp(D) < m0, err=e, mp=(m0, mp(D)))

    # direct cast cancels a waiting entry
    fresh()
    D.core.write(GIRL + 0x60, bytes([5]))
    D.queue_cast(1, 0, 0)
    D.core.write(GIRL + 0x60, bytes([0]))
    e = D.cast(1, 1, 0, 1, 1)
    s, q = qs(D)
    check('direct-cancels-queue', s in ('dropped', 'empty'), direct=e, state=s, err=dtest.ERR[q.err])

    # one queue per caster: girl (hero 1) and sprite (hero 2) wait at the same time, each fires when its own status clears
    cur[0] = str(dtest.STATE / (MOB[variant] + '.state'))   # the sprite's spells need enemies on screen
    fresh()
    SPR = 0xE400
    D.core.write(SPR + 0x186, bytes([60]))
    for sp_ in range(7):
        D.core.write(GIRL + 0x1C8 + sp_, bytes([1]))
        D.core.write(SPR + 0x1C8 + sp_, bytes([1]))
    D.core.write(GIRL + 0x190, bytes([0x80, 0]))
    D.core.write(SPR + 0x190, bytes([0x80, 0]))
    D.core.write(GIRL + 0x1B3, bytes([255]))
    D.core.write(SPR + 0x1B3, bytes([255]))
    sp_pick = next((a, b) for a in range(7) for b in range(3) if D.spell_class(6 * a + b) in (3, 4))
    gm0, sm0 = mp(D), D.core.read(SPR + 0x186, 1)[0]
    e1, _ = D.queue_cast(1, 0, 0, timeout=3000)
    e2, _ = D.queue_cast(2, sp_pick[0], sp_pick[1], timeout=3000)
    sg, qg = qs(D, 1)
    ss, qsp = qs(D, 2)
    check('two-casters-both-queued', e1 == 'ok' and e2 == 'ok' and sg == 'waiting' and ss == 'waiting', girl=sg, sprite=ss)
    check('two-casters-ids-differ', qg.spell_id != qsp.spell_id and not qg.replaced and not qsp.replaced, girl=qg.spell_id, sprite=qsp.spell_id)
    D.queue_cancel(2)
    sg, _ = qs(D, 1)
    ss, qsp = qs(D, 2)
    check('cancel-is-per-caster', sg == 'waiting' and ss == 'dropped' and dtest.ERR[qsp.err] == 'cancelled', girl=sg, sprite=ss)
    D.queue_ack(2)
    check('ack-is-per-caster', qs(D, 2)[0] == 'empty' and qs(D, 1)[0] == 'waiting')
    e2, _ = D.queue_cast(2, sp_pick[0], sp_pick[1], timeout=3000)
    # clear the girl first: only she fires, the sprite keeps waiting
    D.core.write(GIRL + 0x190, bytes([0, 0]))
    run(D, 400, lambda: qs(D, 1)[0] == 'fired')
    check('first-caster-fires-alone', qs(D, 1)[0] == 'fired' and qs(D, 2)[0] == 'waiting', girl=qs(D, 1)[0], sprite=qs(D, 2)[0])
    D.queue_ack(1)
    run(D, 600, lambda: D.state_name() == 'finished' if hasattr(D, 'state_name') else False)
    run(D, 120)
    D.core.write(SPR + 0x190, bytes([0, 0]))
    run(D, 1200, lambda: qs(D, 2)[0] == 'fired')
    check('second-caster-fires-later', qs(D, 2)[0] == 'fired', sprite=qs(D, 2)[0], err=dtest.ERR[qs(D, 2)[1].err], sprite_mp=(sm0, D.core.read(SPR + 0x186, 1)[0]))
    # a second spell of the SAME caster replaces the first
    fresh()
    for sp_ in range(7):
        D.core.write(GIRL + 0x1C8 + sp_, bytes([1]))
    D.core.write(GIRL + 0x190, bytes([0x80, 0]))
    D.core.write(GIRL + 0x1B3, bytes([255]))
    D.queue_cast(1, 0, 0, timeout=3000)
    e, _ = D.queue_cast(1, 0, 1, timeout=3000)
    s, q = qs(D, 1)
    check('same-caster-replaces', e == 'ok' and s == 'waiting' and q.replaced == 1 and q.replaced_id == 3 and q.spell_id == 4, state=s, spell=q.spell_id, replaced=(q.replaced, q.replaced_id))
    D.queue_cast(1, 0, 1, timeout=3000)
    s, q = qs(D, 1)
    check('same-spell-again-still-one', s == 'waiting' and q.spell_id == 4)

    # target policy resolved at fire time (a map with enemies on screen)
    cur[0] = str(dtest.STATE / (MOB[variant] + '.state'))
    fresh()
    for sp_ in range(7):
        D.core.write(GIRL + 0x1C8 + sp_, bytes([1]))
    pick = None
    for pos in range(7):
        for sl in range(3):
            if D.spell_class(6 * pos + 3 + sl) in (3, 4) and D.can_cast(1, pos, sl)[0] == 'ok':
                pick = (pos, sl)
                break
        if pick:
            break
    check('enemy-spell-found', pick is not None, spell=pick)
    pos, sl = pick

    def waiting_enemy_cast(pk, ps=0, mutate=None):
        fresh()
        for sp_ in range(7):
            D.core.write(GIRL + 0x1C8 + sp_, bytes([1]))
        D.core.write(GIRL + 0x60, bytes([5]))
        e, ci = D.queue_cast(1, pos, sl, pk=pk, ps=ps)
        s0 = qs(D)[0]
        if mutate:
            mutate()
        D.core.write(GIRL + 0x60, bytes([0]))
        run(D, 40, lambda: qs(D)[0] != 'waiting')
        s1, q = qs(D)
        return e, s0, s1, dtest.ERR[q.err]

    def kill_enemies():
        for slot in range(3, 6):
            D.core.write(0xE000 + 0x200 * slot, bytes([0, 0]))

    for name, pk, ps, want in (('nearest', 1, 0, 'fired'), ('lowest-hp', 2, 0, 'fired'), ('all', 3, 0, None), ('slot', 0, 3, 'fired')):
        e, s0, s1, er = waiting_enemy_cast(pk, ps)
        check('policy-%s-resolves-at-fire' % name, s0 == 'waiting' and (s1 == 'fired' if want else s1 in ('fired', 'dropped')), queued=e, final=s1, err=er)
    e, s0, s1, er = waiting_enemy_cast(1, 0, kill_enemies)
    check('vanished-target-nearest', s0 == 'waiting' and s1 == 'dropped' and er == 'no-target', final=s1, err=er)
    e, s0, s1, er = waiting_enemy_cast(0, 3, kill_enemies)
    check('vanished-target-slot', s0 == 'waiting' and s1 == 'dropped' and er == 'bad-target', final=s1, err=er)
    e, s0, s1, er = waiting_enemy_cast(3, 0, kill_enemies)
    check('vanished-target-all', s0 == 'waiting' and s1 == 'dropped' and er == 'no-target', final=s1, err=er)
    return all(r['ok'] for r in RES)


if __name__ == '__main__':
    ok = main(*sys.argv[1:3])
    (dtest.STATE / ('queue_%s.json' % sys.argv[1])).write_text(json.dumps(RES, indent=1))
    sys.exit(0 if ok else 1)
