#!/usr/bin/env python3
"""Revive: somd_revive_options and end-to-end revival of a KO hero with the Cup of Wishes and with the Revivifier.

    python3 test_direct_revive.py VARIANT [STATE]
"""
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest  # noqa: E402

RES = []
BASE = {0: 0xE000, 1: 0xE200, 2: 0xE400}


def check(name, ok, **kw):
    r = dict(check=name, ok=bool(ok), **kw)
    RES.append(r)
    print(json.dumps(r), flush=True)


def rd16(D, a):
    b = D.core.read(a, 2)
    return b[0] | b[1] << 8


def hp(D, h):
    return rd16(D, BASE[h] + 0x182)


def dead(D, h):
    return bool(D.core.read(BASE[h] + 0x191, 1)[0] & 0x80)


def kill(D, h):
    D.core.write(BASE[h] + 0x182, bytes([0, 0]))
    for _ in range(200):
        D.frame()
        if dead(D, h):
            break
    for _ in range(30):         # the game hands the pad to a living hero
        D.frame()
    return dead(D, h)



    for _ in range(n):
        D.frame()
        if D.state != 'running' and hp(D, D._h) > 0 and not dead(D, D._h):
            break


def main(variant, state=None):
    D = dtest.Direct(variant)
    st = state or dtest.STATES[variant]
    D.load(st, 4)
    # options for a living hero: none
    check('options-living-hero-empty', D.revive(0) == [])
    # options for a KO boy
    D.core.write(BASE[1] + 0x186, bytes([60]))
    check('boy-dies', kill(D, 0), hp=hp(D, 0))
    opts = D.revive(0)
    item = [o for o in opts if o['kind'] == 1]
    spell = [o for o in opts if o['kind'] == 2]
    check('options-boy', len(item) == 1 and item[0]['id'] == 5 and item[0]['qty'] >= 1 and len(spell) == 1 and spell[0]['id'] == 33
          and spell[0]['mp_cost'] > 0 and spell[0]['usable'] == 1, options=opts)
    qty0 = item[0]['qty']
    rec = item[0]['record']
    # Cup of Wishes on the KO boy
    e = D.item(rec, 1, 0)
    n = 0
    while D.state == 'running' and n < 90:
        D.frame(); n += 1
    for _ in range(300):
        D.frame()
        if hp(D, 0) > 0 and not dead(D, 0):
            break
    inv = D.core.read(0xCC48, 12)
    qty1 = (inv[rec] >> 5) if inv[rec] != 0xFF and (inv[rec] & 0x1F) == 5 else 0
    check('cup-revives-boy', e == 'ok' and hp(D, 0) > 0 and not dead(D, 0) and qty1 == qty0 - 1, err=e, hp=hp(D, 0), max=rd16(D, BASE[0] + 0x184), qty=(qty0, qty1))
    # Revivifier by the girl on the KO boy
    D.load(st, 4)
    D.core.write(BASE[1] + 0x186, bytes([60]))
    kill(D, 0)
    mp0 = D.core.read(BASE[1] + 0x186, 1)[0]
    r, ci = D.can_cast(1, 5, 0)
    e = D.cast(1, 5, 0, 1, 0)
    for _ in range(500):
        D.frame()
        if hp(D, 0) > 0 and not dead(D, 0):
            break
    for _ in range(150):
        D.frame()
    mp1 = D.core.read(BASE[1] + 0x186, 1)[0]
    check('revivifier-revives-boy', e == 'ok' and hp(D, 0) > 0 and not dead(D, 0) and mp1 < mp0, can_cast=r, err=e, hp=hp(D, 0), mp=(mp0, mp1))
    # Cup of Wishes by the boy's inventory on the KO girl, and the options say the girl cannot cast on herself
    D.load(st, 4)
    kill(D, 1)
    opts = D.revive(1)
    sp = [o for o in opts if o['kind'] == 2]
    it = [o for o in opts if o['kind'] == 1]
    check('options-girl', len(it) == 1 and len(sp) == 1 and sp[0]['usable'] == 0, options=opts)
    e = D.item(it[0]['record'], 1, 1)
    n = 0
    while D.state == 'running' and n < 90:
        D.frame(); n += 1
    for _ in range(300):
        D.frame()
        if hp(D, 1) > 0 and not dead(D, 1):
            break
    check('cup-revives-girl', e == 'ok' and hp(D, 1) > 0 and not dead(D, 1), err=e, hp=hp(D, 1))
    # a living ally is not a valid target of the revive class
    D.load(st, 4)
    e = D.item(it[0]['record'], 1, 1)
    check('cup-on-living-refused', e in ('bad-target', 'no-target'), err=e)
    # queue + KO ally target: Revivifier queued while the girl is busy fires when free
    D.load(st, 4)
    D.core.write(BASE[1] + 0x186, bytes([60]))
    kill(D, 0)
    D.core.write(BASE[1] + 0x60, bytes([5]))
    e, ci = D.queue_cast(1, 5, 0, pk=0, ps=0)
    s0 = D.queue_state(1)[0]
    D.core.write(BASE[1] + 0x60, bytes([0]))
    for _ in range(500):
        D.frame()
        if hp(D, 0) > 0 and not dead(D, 0):
            break
    check('queued-revivifier', e == 'ok' and s0 == 'waiting' and hp(D, 0) > 0 and not dead(D, 0), err=e, queue=s0, hp=hp(D, 0))
    return all(r['ok'] for r in RES)


if __name__ == '__main__':
    ok = main(*sys.argv[1:3])
    (dtest.STATE / ('revive_%s.json' % sys.argv[1])).write_text(json.dumps(RES, indent=1))
    sys.exit(0 if ok else 1)
