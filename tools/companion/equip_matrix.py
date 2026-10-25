#!/usr/bin/env python3
"""Equip-weapon macro on every (held weapon, wanted weapon) pair of the owned weapon types, for one hero.

    python3 tools/companion/equip_matrix.py --rom ROM --state STATE.state --hero 0 --out RESULT.json

Held states are produced with the macro itself and then checked in WRAM (hero weapon_type), so a pair is only
counted when the held weapon was really reached first.  Every pair then runs on a fresh copy of that state and is
correct when the macro reports `done` and the hero ends up holding the wanted type.
"""
import argparse
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import common  # noqa: E402


def party_state(out, tag):
    return [d for d in out if d.get('tag') == tag][0]


def run_matrix(rom, state, hero, name='eqm'):
    probe = common.run_host('LOAD %s\nRUN 2\nSTATE s\n' % state, rom=rom, name=name + '_probe')
    s = party_state(probe, 's')
    owned = [t for t in range(8) if s['weapons'][t] != 0xFF]
    first = s['party'][hero]['weapon_type']
    result = {'owned': owned, 'initial': first, 'pairs': {}, 'unreachable_held': []}
    held_states = {}
    for t in owned:
        path = common.WORK / ('%s_h%d_w%d.state' % (name, hero, t))
        body = 'LOAD %s\nRUN 2\n' % state
        if t != first:
            body += 'CMD equip %d %d\nWAIT 900\nRUN 120\n' % (hero, t)
        body += 'STATE s\nSAVE %s\n' % path
        out = common.run_host(body, rom=rom, name=name + '_build')
        if party_state(out, 's')['party'][hero]['weapon_type'] != t:
            result['unreachable_held'].append(t)
            continue
        held_states[t] = path
    for held, path in held_states.items():
        for want in owned:
            body = 'LOAD %s\nRUN 2\nCMD equip %d %d\nWAIT 900\nRUN 60\nSTATE a\n' % (path, hero, want)
            out = common.run_host(body, rom=rom, name=name + '_run')
            m = [d for d in out if d.get('event') in ('macro', 'refused')][0]
            got = party_state(out, 'a')['party'][hero]['weapon_type']
            result['pairs']['%d>%d' % (held, want)] = [m['result'], got, m.get('frames')]
    n = len(owned)
    result['correct'] = sum(1 for k, v in result['pairs'].items() if v[0] == 'done' and v[1] == int(k.split('>')[1]))
    result['total'] = n * n
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--rom', required=True)
    ap.add_argument('--state', required=True)
    ap.add_argument('--hero', type=int, default=0)
    ap.add_argument('--out')
    ap.add_argument('--name', default='eqm')
    a = ap.parse_args()
    r = run_matrix(pathlib.Path(a.rom), pathlib.Path(a.state), a.hero, a.name)
    print('hero %d: %d of %d pairs correct, owned %s, unreachable held %s' %
          (a.hero, r['correct'], r['total'], r['owned'], r['unreachable_held']))
    if a.out:
        pathlib.Path(a.out).write_text(json.dumps(r, indent=0))
    return 0 if r['correct'] == r['total'] and not r['unreachable_held'] else 1


if __name__ == '__main__':
    sys.exit(main())
