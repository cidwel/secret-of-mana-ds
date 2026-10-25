#!/usr/bin/env python3
"""Item differential test: ring macro (`CMD item HERO RECORD`, target = the ring owner) versus som_direct on the same
prepared state.  Compared after settling: item records, hero HP/MP/status/timers of the three heroes.

    python3 test_direct_item.py VARIANT
"""
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest  # noqa: E402
import common  # noqa: E402


def prep(core, preset, hero):
    b = 0xE000 + 0x200 * hero
    if preset == 'hurt':
        core.write(b + 0x182, bytes([20, 0]))
    elif preset == 'poison':
        core.write(b + 0x182, bytes([20, 0])); core.write(b + 0x190, bytes([0x00, 0x20])); core.write(b + 0x1B6, bytes([0x14]))
    elif preset == 'dead':
        core.write(b + 0x182, bytes([0, 0])); core.write(b + 0x190, bytes([0x00, 0x80]))
    elif preset == 'mp':
        core.write(b + 0x186, bytes([0]))


def prep_script(preset, hero):
    b = 0xE000 + 0x200 * hero
    w = {'hurt': [(b + 0x182, '1400')], 'poison': [(b + 0x182, '1400'), (b + 0x190, '0020'), (b + 0x1B6, '14')],
         'dead': [(b + 0x182, '0000'), (b + 0x190, '0080')], 'mp': [(b + 0x186, '00')], 'none': []}[preset]
    return ''.join('WRITE %X %s\n' % x for x in w)


def watched():
    out = list(range(0xCC48, 0xCC54))
    for h in range(3):
        b = 0xE000 + 0x200 * h
        out += list(range(b + 0x180, b + 0x1B2)) + list(range(b + 0x1F1, b + 0x1F9))
    return out


def cases_for(items):
    out = []
    for rec, v in enumerate(items):
        if v == 0xFF:
            continue
        i = v & 31
        if i in (0, 1, 2):
            out += [(rec, 0, 'hurt'), (rec, 2, 'hurt')]
        elif i == 3:
            out += [(rec, 1, 'mp'), (rec, 2, 'mp')]
        elif i == 4:
            out += [(rec, 0, 'poison'), (rec, 1, 'none')]
        elif i == 5:
            out += [(rec, 1, 'dead'), (rec, 0, 'hurt')]
        elif i in (6, 7):
            out += [(rec, 0, 'none')]
        else:
            out += [(rec, 0, 'hurt')]
    return out


def main(variant, state=None):
    state = state or dtest.STATES[variant]
    D = dtest.Direct(variant)
    D.load(state)
    items = [(D.core.u8(0xCC48 + i)) for i in range(12)]
    print('records', [hex(x) for x in items])
    res, bad = [], 0
    for rec, hero, preset in cases_for(items):
        v = items[rec]
        body = 'LOAD %s\nRUN 2\n%sCMD item %d %d\nWAIT 900\nRUN 240\nDUMP %s\nSTATE a\n' % (state, prep_script(preset, hero), hero, rec, dtest.STATE / 'ring_i.bin')
        r = common.run_host(body, rom=dtest.ROMS[variant], name='ring_item_' + variant)
        macro = [x for x in r if x.get('event') in ('macro', 'refused')][0]
        ring = (dtest.STATE / 'ring_i.bin').read_bytes()
        D.load(state)
        prep(D.core, preset, hero)
        rc = D.item(rec, 1, hero)
        D.wait()
        used = D.frames
        D.core.run(max(0, 2 + 255 + 240 - 2 - used))
        direct = D.core.dump_wram()
        diffs = [(hex(i), ring[i], direct[i]) for i in watched() if ring[i] != direct[i]]
        if preset == 'poison':
            diffs = [x for x in diffs if x[0] != hex(0xE000 + 0x200 * hero + 0x182)]      # poison ticks drift with the paused ring
        if macro['result'] == 'done':
            ok = rc == 'ok' and D.state == 'finished' and not diffs
        elif macro['result'] == 'fail-bad-argument' and preset == 'dead':
            if (v & 31) == 5:
                ok = rc == 'ok' and D.core.u8(0xE000 + 0x200 * hero + 0x191) & 0x80 == 0     # the ring cannot own a dead hero; direct revives
            else:
                ok = rc == 'bad-target'                                                      # nothing else works on a dead hero
        else:
            ok = rc != 'ok' or not diffs     # the ring refused or timed out: direct must refuse or change nothing
        rec_ = dict(rec=rec, id=v & 31, qty=v >> 5, hero=hero, preset=preset, ring=macro['result'], direct=rc, frames=used, diffs=diffs[:8], ok=ok)
        res.append(rec_)
        bad += not ok
        print(rec_, flush=True)
    (dtest.STATE / ('item_%s.json' % variant)).write_text(json.dumps(dict(variant=variant, cases=len(res), mismatches=bad, results=res), indent=1))
    print('cases', len(res), 'mismatches', bad)
    return bad


if __name__ == '__main__':
    sys.exit(1 if main(*sys.argv[1:]) else 0)
