#!/usr/bin/env python3
"""Black-row regression over equip / item / cast on one ROM variant (one process per ROM).

    python3 blackall.py VARIANT      -> JSON line per case, exit 1 when any frame has black rows
"""
import json, pathlib, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import test_direct_cast as T  # noqa: E402
import common  # noqa: E402

W = common.WORK
S = T.dtest.STATE
EQ = {'vanilla': (W / 'eqm_vanilla_h0_w1.state', [(0, 2), (0, 3)]), 'magno': (W / 'eqm_magno_h0_w1.state', [(0, 2), (0, 3)]),
      'ac1': (W / 'eqm_ac1_h0_h0_w0.state', [(0, 1), (0, 2), (0, 5)]), 'ac777': (W / 'eqm_ac7_h0_h0_w0.state', [(0, 1), (0, 2), (0, 5)])}
MOB = {'vanilla': 'mob_vanilla_344', 'magno': 'mob_magno_365', 'ac1': 'mob_ac1_inj', 'ac777': 'mob_ac777_inj'}


def case(variant, state, name, lines):
    body = 'LOAD %s\nRUN 4\nBLACKSCAN 1\n%s\nRUN 90\n' % (state, lines)
    res = T.run_script(variant, body, 'blackall')
    ctrl = T.run_script(variant, 'LOAD %s\nRUN 4\nBLACKSCAN 1\nRUN 200\n' % state, 'blackctl')
    base = {r['frame']: r['spans'] for r in ctrl if r.get('event') == 'black'}
    # rows that are black with the command but not in the same frame of the no-command control run
    blk = [r for r in res if r.get('event') == 'black' and base.get(r['frame']) != r['spans']]
    st = [r for r in res if r.get('event') == 'direct']
    out = dict(variant=variant, case=name, black_frames=len(blk), rows=sum(r['rows'] for r in blk), detail=blk[:3],
               err=st[0]['err'] if st else None, latency=st[0].get('latency') if st else None)
    print(json.dumps(out), flush=True)
    return not blk and out['err'] == 'ok'


def main(v):
    ok = True
    st, pairs = EQ[v]
    for h, t in pairs:
        ok &= case(v, st, 'equip %d %d' % (h, t), 'DEQUIP %d %d\nDWAIT 60' % (h, t))
    mob = S / (MOB[v] + '.state')
    for rec in (0, 1):
        ok &= case(v, mob, 'item rec %d hero 0' % rec, 'DITEM %d 1 0\nDWAIT 60' % rec)
    ok &= case(v, mob, 'cast enemy single', 'DCAST 2 3 0 1 5\nDWAIT 60')
    ok &= case(v, mob, 'cast enemy all', 'DCAST 2 3 0 3 0\nDWAIT 60')
    ok &= case(v, mob, 'cast ally single', 'DCAST 1 1 2 1 0\nDWAIT 60')
    ok &= case(v, mob, 'cast ally all', 'DCAST 1 1 2 2 0\nDWAIT 60')
    # revive on a KO ally (the inventory slot 11 holds a Cup of Wishes x2; the girl owns Dryad with MP)
    kill = 'WRITE E182 0000\nRUN 60\nWRITE CC53 45\nWRITE E386 3C\nWRITE E3CD 01\n'
    ok &= case(v, mob, 'revive item on KO boy', kill + 'DITEM 11 1 0\nDWAIT 60')
    ok &= case(v, mob, 'revive spell on KO boy', kill + 'DCAST 1 5 0 1 0\nDWAIT 60')
    # queue-fired casts: the girl is busy at queue time, free a few frames later
    ok &= case(v, mob, 'queued cast enemy', 'WRITE E586 3C\nWRITE E460 05\nDQUEUE 2 3 0 1 0 600\nRUN 5\nWRITE E460 00\nDQWAIT 2 100\nDWAIT 60')
    ok &= case(v, mob, 'queued cast ally', 'WRITE E386 3C\nWRITE E260 05\nDQUEUE 1 1 2 1 0 600\nRUN 5\nWRITE E260 00\nDQWAIT 1 100\nDWAIT 60')
    return ok


if __name__ == '__main__':
    sys.exit(0 if main(sys.argv[1]) else 1)
