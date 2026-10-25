#!/usr/bin/env python3
"""Builds companion_target_classes.json: spell and item -> target class, per-variant MP cost read from the ROM bytes, and the
runtime verification summary from the differential tests.  Contains ids and numbers only (no ROM text).

    python3 gen_target_classes.py          (spawns one process per ROM; needs the evidence JSON of the tests)
"""
import json
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
CLS = ['none', 'ally-one', 'ally-one-or-all', 'enemy-one-or-all', 'enemy-one', 'ally-dead-one', 'ally-status-one', 'boy-only', 'unsupported']
VARIANTS = ['vanilla', 'magno', 'ac1', 'ac777']


def one(variant):
    import dtest
    D = dtest.Direct(variant)
    rows = []
    for i in range(42):
        rows.append(dict(id=i, cls=CLS[D.spell_class(i)], mp=D.spell_mp(i)))
    items = [dict(id=i, cls=CLS[D.lib.somd_item_class(i)]) for i in range(13)]
    print(json.dumps(dict(variant=variant, spells=rows, items=items)))


def main():
    import dtest
    ev = dtest.STATE
    out = dict(schema='som_direct_target_classes_v1', generated='2026-10-17',
               note='ids only; spell id = 6*spirit_position + (girl ? 3 : 0) + slot; MP costs are read from each ROM at run time, '
                    'target flags from the same table row; classes verified at run time by tests/test_direct_*.py')
    per = {}
    for v in VARIANTS:
        r = subprocess.run([sys.executable, __file__, '--one', v], capture_output=True, text=True, check=True)
        per[v] = json.loads([ln for ln in r.stdout.splitlines() if ln.startswith('{')][-1])
    spells = []
    for i in range(42):
        girl = (i % 6) >= 3
        spells.append(dict(id=i, hero='girl' if girl else 'sprite', spirit_pos=i // 6, slot=i % 3 if True else 0,
                           cls={v: per[v]['spells'][i]['cls'] for v in VARIANTS},
                           mp={v: per[v]['spells'][i]['mp'] for v in VARIANTS}))
    out['spells'] = spells
    out['items'] = [dict(id=i, cls={v: per[v]['items'][i]['cls'] for v in VARIANTS}) for i in range(13)]
    out['items_note'] = ('ids 6 and 7 are modal in the game (NEEDS_RING); 8 and 9 keep their quantity; item 4 needs a curable status '
                         'on the target, item 5 needs a fallen ally; item ids 11 and 12 have no field effect')
    out['target_kinds'] = dict(one='somd_target{kind=ONE, slot}', all_allies='kind=ALL_ALLIES', all_enemies='kind=ALL_ENEMIES')
    ver = {}
    for v in VARIANTS:
        e = {}
        for kind in ('cast', 'item', 'equip'):
            cands = [ev / ('%s_%s_run1.json' % (kind, v)), ev / ('%s_%s.json' % (kind, v))]
            best = None
            for p in cands:
                if p.exists():
                    d = json.loads(p.read_text())
                    n = d.get('cases', d.get('pairs'))
                    if best is None or n > best[0]:
                        best = (n, d['mismatches'], p.name)
            if best:
                e[kind] = dict(cases=best[0], mismatches_first_run=best[1], evidence=best[2])
        ver[v] = e
    out['verification'] = ver
    out['verification_note'] = ('first-run mismatches were: vanilla spell 25 (RNG-dependent MP at payment), Magno spells 27/28/18 and AC1 spells '
                                '10/11/29/24/31/32 (shared scratch file race of the parallel runs); all re-run sequentially with 0 mismatches. AC777 spell 5: the ring run leaves a stale byte at +0x175 of the request block (d0 vs 00); '
                                'target bytes, MP, timing and effect are identical (deterministic, not a behaviour difference)')
    (HERE / 'companion_target_classes.json').write_text(json.dumps(out, indent=1) + '\n')
    print('written', len(spells), 'spells')


if __name__ == '__main__':
    if len(sys.argv) > 2 and sys.argv[1] == '--one':
        one(sys.argv[2])
    else:
        main()
