#!/usr/bin/env python3
"""Equip-weapon macro on every (held weapon, wanted weapon) pair of the Ancient Cave ROM: 8 x 8 = 64 runs per host.

States "held weapon = t" are made by driving the weapon ring by hand (cursor positions known from the initial state).
Hosts: the repository one (stock) and the patched copy of patched_host.py.  Result: evidence/ancientcave/equip_matrix.json.
"""
import json
import sys

import acprobe as ac
import patched_host as ph

sys.path.insert(0, str(ac.HERE.parent))
import common  # noqa: E402

ORDER = [1, 2, 3, 4, 5, 6, 7, 0]      # measured ring order from the initial state (sword held)


def build_states():
    c = ac.open_core()
    paths = {}
    for t in range(8):
        c.load_state(ac.PARTY.read_bytes())
        c.run(2)
        if t != 1:
            ac.open_ring(c)
            ac.goto_category(c, 3)
            ac.goto_cursor(c, ORDER.index(t))
            for _ in range(4):
                c.run(3, 'B')
                c.run(60)
                if c.u8(0x7D) == 0:
                    break
            c.run(150)
        assert c.u8(0xE1E4) == t, (t, c.u8(0xE1E4))
        p = ac.WORK / ('ac_wcur%d.state' % t)
        p.write_bytes(c.save_state())
        paths[t] = p
    return paths


def one(host_kind, path, tgt):
    script = 'LOAD %s\nRUN 2\nSTATE b\nCMD equip 0 %d\nWAIT 900\nRUN 30\nSTATE a\n' % (path, tgt)
    if host_kind == 'patched':
        out = ph.run(script, ac.ROM, 'wm')
    else:
        out = common.run_host(script, rom=ac.ROM, name='ac_wm')
    m = [d for d in out if d.get('event') in ('macro', 'refused')][0]
    a = [d for d in out if d.get('tag') == 'a'][0]
    return [m['result'], a['party'][0]['weapon_type'], m['frames']]


def main():
    paths = build_states()
    res = {'stock': {}, 'patched': {}}
    for kind in res:
        for cur in range(8):
            for tgt in range(8):
                res[kind]['%d>%d' % (cur, tgt)] = one(kind, paths[cur], tgt)
    for kind in ('stock', 'patched'):
        ok = sum(1 for k, v in res[kind].items() if v[0] == 'done' and v[1] == int(k.split('>')[1]))
        res[kind + '_correct'] = ok
        print(kind, 'correct', ok, 'of 64')
    (ac.EVID / 'equip_matrix.json').write_text(json.dumps(res, indent=0))


if __name__ == '__main__':
    main()
