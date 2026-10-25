#!/usr/bin/env python3
"""Equip differential test: for every (held, wanted) pair on the saved equip-matrix states, the ring macro (C host) and
som_direct start from the same state; hero records and weapon marks must agree outside timing noise.

    python3 test_direct_equip.py VARIANT      (vanilla | magno | ac1 | ac777), one ROM per process
Evidence: $STATE/companion-direct/equip_<variant>.json
"""
import json
import pathlib
import re
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest  # noqa: E402
import diffeq  # noqa: E402
import common  # noqa: E402

PATTERNS = {'vanilla': r'eqm_(vanilla_h0|v_h1|v_h2)_.*w\d\.state', 'magno': r'eqm_magno_h0_w\d\.state',
            'ac1': r'eqm_ac1_h\d_h\d_w\d\.state', 'ac777': r'eqm_ac7_h\d_h\d_w\d\.state'}


def noise_mask(D, state):
    D.load(state)
    D.core.run(301)
    a = D.core.dump_wram()
    D.load(state)
    D.core.run(307)
    b = D.core.dump_wram()
    return {i for i in range(0xE000, 0xE600) if a[i] != b[i]}


def watched():
    """Semantic fields only: sprite id, spell request, stats, levels, masks, equipment rows and the weapon marks.
    Timers, animation phases and pending-damage words differ between the paused ring and the free-running direct path."""
    out = list(range(0xCC00, 0xCC80))
    for h in range(3):
        b = 0xE000 + 0x200 * h
        out += [b, b + 1] + list(range(b + 0x170, b + 0x1B2)) + list(range(b + 0x1C0, b + 0x1ED))
    return out


def main(variant):
    D = dtest.Direct(variant)
    files = sorted(p for p in common.WORK.iterdir() if re.fullmatch(PATTERNS[variant], p.name))
    results = []
    bad = 0
    for st in files:
        m = re.search(r'_h(\d)_(?:h\d_)?w(\d)', st.name)
        hero = int(m.group(1))
        # the hero field in the file name is the holder; names like eqm_v_h1_h1_w1 mean hero 1 holds type 1
        D.load(st)
        owned = [t for t in range(8) if D.core.u8(0xCC54 + t) != 0xFF]
        for want in owned:
            ring, macro = diffeq.ring_dump(variant, st, hero, want, 'm_' + variant)
            D.load(st)
            before = D.core.dump_wram()
            rc = D.equip(hero, want)
            D.wait()
            used = D.frames
            D.core.run(299 - used)
            direct = D.core.dump_wram()
            diffs = [(i, ring[i], direct[i]) for i in watched() if ring[i] != direct[i]]
            ok = (rc == 'ok' and D.state == 'finished' and not diffs) or (rc != 'ok' and macro['result'] != 'done')
            rec = dict(state=st.name, hero=hero, want=want, ring=macro['result'], direct=rc, frames=used, diffs=[(hex(i), x, y) for i, x, y in diffs], ok=ok)
            results.append(rec)
            bad += not ok
            print(rec['state'], 'want', want, 'ring', macro['result'], 'direct', rc, 'frames', used, 'OK' if ok else 'MISMATCH %s' % rec['diffs'][:6], flush=True)
    out = dtest.STATE / ('equip_%s.json' % variant)
    out.write_text(json.dumps(dict(variant=variant, pairs=len(results), mismatches=bad, results=results), indent=1))
    print('pairs', len(results), 'mismatches', bad)
    return bad


if __name__ == '__main__':
    sys.exit(1 if main(sys.argv[1]) else 0)
