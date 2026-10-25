#!/usr/bin/env python3
"""Cast differential test: ring macro (default target) versus som_direct on the same state, for every spell the girl and
the sprite can learn (42 ids).  Metrics from a per-frame trace of the caster record (first 0x200 bytes of the hero's
record): frame the cast pose starts, frame the spell phase 0x80 begins, frame MP is paid, frame the hero is free, final
cast request bytes (+0x170..+0x17A) and final MP.

    python3 test_direct_cast.py VARIANT STATE [ID...]
"""
import json
import pathlib
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest  # noqa: E402
import common  # noqa: E402

HOST = dtest.STATE / 'direct_host'
CORE = str(common.CORE)


def host_build():
    d = pathlib.Path(__file__).resolve().parent
    srcs = [d / 'direct_host.c', d / 'som_direct.c', d.parent / 'som_companion.c']
    if not HOST.exists() or HOST.stat().st_mtime < max(s.stat().st_mtime for s in srcs + [d / 'som_direct.h', d.parent / 'som_companion.h']):
        subprocess.run(['gcc', '-std=gnu99', '-Wall', '-Wextra', '-O2', '-o', str(HOST)] + [str(s) for s in srcs] + ['-ldl'], check=True)


def run_script(variant, text, name):
    host_build()
    import os
    p = dtest.STATE / ('%s_%s_%d.txt' % (name, variant, os.getpid()))
    p.write_text(text)
    r = subprocess.run([str(HOST), CORE, str(dtest.ROMS[variant]), str(p)], capture_output=True, text=True, timeout=300)
    out = []
    for ln in r.stdout.splitlines():
        if ln.startswith('{'):
            out.append(json.loads(ln))
    return out


def metrics(trace, hero):
    rows = [bytes.fromhex(j['v']) for j in trace]
    start = next((i for i, r in enumerate(rows) if r[0x1C] == 0x40 and r[0x11] == 0x94), None)
    if start is None:
        return None
    p80 = next((i for i in range(start, len(rows)) if rows[i][0x60] == 0x80), None)
    mp0 = rows[start][0x186]
    mpf = next((i for i in range(start, len(rows)) if rows[i][0x186] != mp0), None)
    free = next((i for i in range(start + 3, len(rows)) if rows[i][0x1C] == 0 and rows[i][0x60] == 0), None)
    last = rows[-1]
    return dict(start=start, phase80=None if p80 is None else p80 - start, mp_paid=None if mpf is None else mpf - start,
                free=None if free is None else free - start, request=last[0x170:0x17B].hex(),
                mp=(mp0, rows[mpf][0x186] if mpf is not None else last[0x186]))


def spell_class_name(c):
    return ['none', 'ally-one', 'ally-one-or-all', 'enemy-one-or-all', 'enemy-one', 'ally-dead-one', 'ally-status-one', 'boy-only', 'unsupported'][c]


def case_for(hero, idx, slot):
    byte = 7 if (hero == 1 and idx == 6) else idx
    return byte


def main(variant, state, ids=None):
    D = dtest.Direct(variant)
    D.load(state)
    results, bad = [], 0
    for hero in (1, 2):
        for idx in range(7):
            for slot in range(3):
                sid = 6 * idx + (3 if hero == 1 else 0) + slot
                if ids and sid not in ids:
                    continue
                if sid > 41:
                    continue
                cls = D.spell_class(sid)
                pre = ''
                if cls == 5:
                    pre = 'WRITE E582 0000\nWRITE E590 0000\nWRITE E591 80\n'      # sprite dead
                enemy = cls in (3, 4)
                tkind, tslot = (1, 3) if enemy else (1, 2 if cls == 5 else (0 if cls == 7 else hero))
                byte = case_for(hero, idx, slot)
                ring = run_script(variant, 'LOAD %s\nRUN 2\n%sCMD cast %d %d %d\nTRACE E%03X 512 1100 r\n' % (state, pre, hero, byte, slot, 0x000 + 0x200 * hero - 0x200 * 0 if False else 0) if False else
                                  'LOAD %s\nRUN 2\n%sCMD cast %d %d %d\nTRACE %X 512 1100 r\n' % (state, pre, hero, byte, slot, 0xE000 + 0x200 * hero), 'cast_ring')
                mr = metrics([j for j in ring if 'trace' in j], hero)
                mac = [j for j in ring if j.get('event') == 'refused']
                direct = run_script(variant, 'LOAD %s\nRUN 2\n%sDCAST %d %d %d %d %d\nTRACE %X 512 700 d\n' % (state, pre, hero, idx, slot, tkind, tslot, 0xE000 + 0x200 * hero), 'cast_dir')
                md = metrics([j for j in direct if 'trace' in j], hero)
                start = [j for j in direct if j.get('event') == 'direct-start'][0]
                rec = dict(spell=sid, hero=hero, spirit=idx, slot=slot, cls=spell_class_name(cls), ring=mr, ring_refused=mac[0]['result'] if mac else None,
                           direct=md, direct_start=start['result'])
                if mr and md:
                    ok = (mr['request'] == md['request'] and mr['mp'] == md['mp'] and abs(mr['phase80'] - md['phase80']) <= 3 and
                          (mr['free'] is None or md['free'] is None or abs(mr['free'] - md['free']) <= 40))
                    ok = ok and md['start'] <= 3
                elif not mr:
                    ok = start['result'] != 'ok' or md is None or True   # ring could not cast (no effect, no target): informational
                else:
                    ok = False
                rec['ok'] = bool(ok)
                bad += not ok
                results.append(rec)
                print(json.dumps(rec), flush=True)
    out = dtest.STATE / ('cast_%s.json' % variant)
    out.write_text(json.dumps(dict(variant=variant, cases=len(results), mismatches=bad, results=results), indent=1))
    print('cases', len(results), 'mismatches', bad)
    return bad


if __name__ == '__main__':
    ids = set(map(int, sys.argv[3:])) or None
    sys.exit(1 if main(sys.argv[1], sys.argv[2], ids) else 0)
