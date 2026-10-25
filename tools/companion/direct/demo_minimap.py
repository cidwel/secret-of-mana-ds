#!/usr/bin/env python3
"""Mini-map and tap-to-cast demonstration (one ROM per process).

    python3 demo_minimap.py VARIANT STATE OUTDIR

For a hostile spell the bottom screen is rendered from the scan, a tap on the first valid enemy is converted with somd_pick, the
cast is issued and the top screen is saved before/after.  Evidence: JSON with the request bytes, ring flag ($7D) per frame and
the latency in frames."""
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest  # noqa: E402
import render_minimap as R  # noqa: E402
from PIL import Image  # noqa: E402


def top(c, path):
    rgb = c.frame_rgb
    Image.frombytes('RGB', (c.width, c.height), rgb).save(path)


def main(variant, state, out):
    out = pathlib.Path(out)
    out.mkdir(parents=True, exist_ok=True)
    D = dtest.Direct(variant)
    c = D.core
    tag = pathlib.Path(state).stem
    rec = dict(variant=variant, state=tag, cases=[])
    # (title, class, id, is_spell, caster hero, spirit pos, slot)
    cases = [('Enemy spell (single/all)', 3, 18, 1, (2, 3, 0)), ('Ally heal (single/all)', 2, 11, 1, (1, 1, 2))]
    for title, cls, ident, is_sp, (h, sp, sl) in cases:
        D.load(state, 2)
        sc = D.scan(cls, ident, is_sp)
        grp = D.group_card(sc)
        valid = [a for a in sc.actor if a.valid]
        case = dict(title=title, cls=cls, spell=ident, group_card=grp, valid=[(a.slot, a.sx, a.sy, a.hp) for a in valid])
        if not valid:
            case['skipped'] = 'no valid target'
            rec['cases'].append(case)
            continue
        tgt = valid[-1]
        if tgt.slot < 3:
            # allies: tap the party card (the three heroes overlap on the playfield)
            x0, y0, x1, y1 = R.card_rect(tgt.slot)
            px, py = (x0 + x1) // 2 - 3, (y0 + y1) // 2 + 2
            slot_hit = tgt.slot if (x0 <= px + 3 <= x1 and y0 <= py - 2 <= y1) else -1
        else:
            px, py = R.point(tgt)
            slot_hit = D.pick(sc, px + 3, py - 2)
        R.render(sc, title, picked=slot_hit if slot_hit >= 0 else None, tap=(px + 3, py - 2), group=grp).save(out / ('%s_%s_cls%d.png' % (variant, tag, cls)))
        top(c, out / ('%s_%s_cls%d_top_before.png' % (variant, tag, cls)))
        ring = []
        rc = D.cast(h, sp, sl, 1, slot_hit) if slot_hit >= 0 else 'no-pick'
        fr = 0
        req = ''
        while D.state == 'running' and fr < 60:
            D.frame()
            ring.append(c.u8(0x7D))
            fr += 1
        c.run(3)
        req = bytes(c.read(0xE000 + 0x200 * h + 0x170, 11)).hex()
        c.run(22)
        ring.append(c.u8(0x7D))
        top(c, out / ('%s_%s_cls%d_top_after.png' % (variant, tag, cls)))
        case.update(tap=(px + 3, py - 2), picked_slot=slot_hit, expected_slot=tgt.slot, rc=rc, frames=D.frames,
                    request=req, ring_flag_max=max(ring),
                    ok=(rc == 'ok' and slot_hit == tgt.slot and max(ring) == 0))
        rec['cases'].append(case)
    (out / ('minimap_%s_%s.json' % (variant, tag))).write_text(json.dumps(rec, indent=1))
    print(json.dumps(rec))
    return all(c_.get('ok', True) for c_ in rec['cases'])


if __name__ == '__main__':
    sys.exit(0 if main(*sys.argv[1:4]) else 1)
