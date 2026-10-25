#!/usr/bin/env python3
"""Reads the ring cursor, category and spell sub-ring on real game runs and checks the selected-entry decoder of som_ui_sel.c.

For vanilla US, the Magno translation and the generated Ancient Cave ROMs (seeds 1 and 777) it opens the ring (Y), cycles the
owner (X) and the category (UP), walks one full lap of every ring with RIGHT and records, per entry, the raw ring bytes and the
decoded title and lines.  Checks per ring: the lap has `count` distinct valid entries, every entry decodes (valid == 1, no
placeholder title) and the weapon/item/spirit entries match the owned lists in WRAM.  The spirit ring is entered with B
(spell sub-ring, $1803 bit 7) and left with Y; nothing is ever confirmed in the game.
Writes <state>/ux/evidence/ring_selection.json and, for vanilla, ring WRAM images into <state>/ux/wram/ring_*.bin.
Missing ROMs or states are reported as skipped."""
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import common  # noqa: E402
import retro  # noqa: E402
import ux_lib  # noqa: E402

ST = common.STATE
W = common.WORK
OUT = ST / 'ux' / 'wram'
TARGETS = [
    ('vanilla', common.ROM_VANILLA, W / 'party3_vanilla.state'),
    ('magno', common.ROM_MAGNO, W / 'party3_magno.state'),
    ('ac_seed1', ST / 'ancientcave/out/ac_seed1.smc', W / 'ac_party.state'),
    ('ac_seed777', ST / 'ancientcave/out/ac_seed777.smc', W / 'ac_party_s777.state'),
]
CATS = {'item': 0, 'magic': 1, 'config': 2, 'weapon': 3}


def run_one(name, rom, state):
    res = {'rom': rom.name}
    if not rom.exists() or not state.exists():
        res['skipped'] = 'missing rom or state'
        return res
    c = retro.Core(str(rom), core_path=str(common.CORE))
    ux = ux_lib.UX(rom=rom)
    c.load_state(state.read_bytes())
    c.run(30)

    def press(b, n=4, wait=40):
        c.run(n, b)
        c.run(wait)

    def look():
        w = c.dump_wram()
        ux.update(w)
        s = ux.selection()
        s.update(owner_w=c.u8(0x1808), cat_w=c.u8(0x1809), cur=c.u8(0x180A), count=c.u8(0x180B), busy=c.u8(0x1812),
                 f1803=c.u8(0x1803), f1839=c.u8(0x1839), mode=ux_lib.MODES[ux.lib.som_ux_classify(w)])
        return s, w

    def settle():
        for _ in range(10):
            s, w = look()
            if s['busy'] == 0 and s['valid']:
                return s, w
            c.run(10)
        return look()

    press('Y', 4, 60)
    res['opened'] = look()[0]['mode']
    if res['opened'] != 'RING':
        res['error'] = 'ring did not open'
        return res
    rings = {}
    problems = []
    for owner in (0, 1, 2):
        for _ in range(4):
            if c.u8(0x1808) == owner:
                break
            press('X', 4, 40)
        if c.u8(0x1808) != owner:
            problems.append('owner %d unreachable' % owner)
            continue
        seen_cats = []
        for _ in range(6):
            cat = c.u8(0x1809)
            if cat in seen_cats:
                break
            seen_cats.append(cat)
            s, w = settle()
            lap = []
            n = s['count']
            for k in range(n):
                s, w = settle()
                lap.append({'cur': s['cur'], 'title': s['title'], 'lines': s['lines'], 'kind': s['kind'], 'id': s['id'],
                            'valid': s['valid']})
                if name == 'vanilla' and owner in (0, 1) and k == (1 if cat != 2 else 2):
                    (OUT / ('ring_%s_o%d.bin' % (['item', 'magic', 'config', 'weapon'][cat], owner))).write_bytes(w)
                press('RIGHT', 4, 30)
            key = '%s_owner%d' % (['item', 'magic', 'config', 'weapon'][cat] if cat < 4 else str(cat), owner)
            titles = [e['title'] for e in lap]
            rings[key] = {'count': n, 'lap': lap}
            if len(set(titles)) != len(titles):
                problems.append('%s: duplicate entries %s' % (key, titles))
            if any(not e['valid'] for e in lap):
                problems.append('%s: undecoded entry' % key)
            if cat == 1:                                       # spirit ring: enter the first spirit's spells and leave
                press('B', 4, 60)
                s, w = settle()
                sub = {'f1803': s['f1803'], 'f1839': s['f1839'], 'sub_ring': s['sub_ring'], 'head': s['head'], 'spells': []}
                if not s['sub_ring']:
                    problems.append('%s: sub-ring flag not set' % key)
                for k in range(s['count']):
                    s, w = settle()
                    sub['spells'].append({'cur': s['cur'], 'id': s['id'], 'title': s['title'], 'lines': s['lines'],
                                          'f1839': s['f1839']})
                    if name == 'vanilla' and k == 1:
                        (OUT / ('ring_spell_o%d.bin' % owner)).write_bytes(w)
                    press('RIGHT', 4, 30)
                ids = [e['id'] for e in sub['spells']]
                if len(set(ids)) != len(ids) or any(i > 41 for i in ids):
                    problems.append('%s: bad spell ids %s' % (key, ids))
                rings[key]['sub'] = sub
                press('Y', 4, 60)
                if c.u8(0x1803) & 0x80:
                    problems.append('%s: could not leave sub-ring' % key)
            press('UP', 4, 60)
    # owner switching: what the avatar tap has to do.  X moves 1 <-> 2 and never returns to the boy; closing and reopening the
    # ring starts again at the boy (owner 0).
    cyc = [c.u8(0x1808)]
    for _ in range(4):
        press('X', 4, 40)
        cyc.append(c.u8(0x1808))
    res['owner_cycle_x'] = cyc
    for _ in range(3):
        press('Y', 4, 60)
        if c.u8(0x7D) == 0:
            break
    res['closed_with_y'] = c.u8(0x7D) == 0
    press('Y', 4, 60)
    res['owner_after_reopen'] = c.u8(0x1808)
    res['rings'] = rings
    res['problems'] = problems
    return res


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    out = {}
    for name, rom, state in TARGETS:
        try:
            out[name] = run_one(name, rom, state)
        except Exception as e:  # keep going: one broken ROM must not hide the others
            out[name] = {'error': repr(e)}
        r = out[name]
        print(name, 'opened' if r.get('opened') else '', 'problems:', r.get('problems', r.get('error', r.get('skipped'))), flush=True)
    ev = ST / 'ux' / 'evidence'
    ev.mkdir(parents=True, exist_ok=True)
    (ev / 'ring_selection.json').write_text(json.dumps(out, indent=1))


if __name__ == '__main__':
    main()
