#!/usr/bin/env python3
"""T5/T8: the gameplay options (tools/gameplay/som_gameplay.py) on generated Ancient Cave ROMs, including the known conflicts with the
randomizer hacks attacksForever, aggEnemies, noStaminaRun, funSpeedup and fasterWeaponLevel.

    t5_gameplay.py static LABEL [LABEL...]    plan / apply (error and skip) / verify on $T5/roms/LABEL.sfc; JSON per label in $T5/out/gp_static_LABEL.json
    t5_gameplay.py measure LABEL [patched]    boots the (patched) ROM headless and measures gauge, run gauge, charge stage time and walk speed on floor 1

The patched ROM is written to $T5/gp/LABEL.gp.sfc (outside the repository).  The option set is $T5/work/gp_all.json (every option on).
"""
import json
import os
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parent.parent.parent
T5 = pathlib.Path(os.environ.get('T5', os.path.expanduser('~/.local/state/som-decomp/t5-ac-20261018')))
TOOL = REPO / 'tools/gameplay/som_gameplay.py'
OPTS = T5 / 'work/gp_all.json'


def tool(*args):
    r = subprocess.run([sys.executable, str(TOOL)] + [str(a) for a in args], capture_output=True, text=True)
    return r


def static(label):
    rom = T5 / 'roms' / (label + '.sfc')
    out = T5 / 'gp' / (label + '.gp.sfc')
    out.parent.mkdir(exist_ok=True)
    res = dict(label=label)
    plan = json.loads(tool('plan', '--options', OPTS, '--rom', rom).stdout)
    res['plan_conflicts'] = sorted({p['option'] for p in plan if p.get('rom') == 'conflict'})
    res['plan_ok_options'] = sorted({p['option'] for p in plan if p.get('rom') in ('ok', 'already')})
    e = tool('apply', '--rom', rom, '--options', OPTS, '--out', T5 / 'gp' / (label + '.err.sfc'), '--on-conflict', 'error')
    res['error_mode'] = dict(rc=e.returncode, stderr=e.stderr.strip()[-600:])
    s = tool('apply', '--rom', rom, '--options', OPTS, '--out', out, '--on-conflict', 'skip')
    res['skip_mode'] = dict(rc=s.returncode, stderr=s.stderr.strip()[-800:], stdout=s.stdout.strip()[-400:])
    v = tool('verify', '--rom', rom, '--patched', out, '--options', OPTS)
    res['verify'] = dict(rc=v.returncode, out=(v.stdout + v.stderr).strip()[-500:])
    (T5 / 'out' / ('gp_static_%s.json' % label)).write_text(json.dumps(res, indent=1))
    print(label, 'conflicts', res['plan_conflicts'], 'error rc', e.returncode, 'skip rc', s.returncode, 'verify rc', v.returncode)
    return res


def measure(label, patched):
    sys.path.insert(0, str(HERE.parent / 'widescreen'))
    sys.path.insert(0, str(HERE.parent))
    import wscore
    rom = (T5 / 'gp' / (label + '.gp.sfc')) if patched else (T5 / 'roms' / (label + '.sfc'))
    heroes = 1 if label.endswith('hero') else 3
    c = wscore.WS(str(rom), wscore.CORE_PLAIN)

    def tap(b, h=5, w=10):
        c.run(h, b)
        c.run(w)
    c.run(300)
    tap('START', 5, 410)
    tap('B', 5, 300)
    for i in range(heroes):
        for _ in range(i):
            c.run(4, 'RIGHT')
            c.run(30)
        c.run(5, 'B')
        c.run(60)
        c.run(5, 'START')
        c.run(400)
    quiet = 0
    for _ in range(40):
        c.run(4, 'B')
        c.run(60)
        quiet = quiet + 1 if c.u8(0xD0) == 0 else 0
        if quiet >= 3:
            break
    c.run(120)
    res = dict(label=label, patched=patched, free_play=c.u8(0xD0) == 0 and c.u8(0xE000) == 1, frames=c.frames)
    h = 0xE000
    c.write(h + 0x182, (999).to_bytes(2, 'little'))
    c.write(h + 0x184, (999).to_bytes(2, 'little'))
    c.write(h + 0x19C, b'\x08')
    # swing gauge: one press, then record the gauge until zero
    c.run(3, 'B')
    peak, zero_at = 0, None
    for f in range(400):
        c.run(1)
        g = c.u8(h + 0x1ED)
        peak = max(peak, g)
        if peak and g == 0 and zero_at is None:
            zero_at = f
    res['swing_gauge_peak'] = peak
    res['swing_gauge_frames_to_zero'] = zero_at
    # run gauge: A with a direction
    c.run(60)
    c.write(h + 0x1ED, b'\x00')
    for d in ('LEFT', 'RIGHT'):
        c.run(40, 'A,' + d)
        c.run(2)
        res['run_gauge_after_run_' + d] = c.u8(h + 0x1ED)
        c.run(90)
    # charge: hold B and watch the stage byte (+0x19B)
    c.run(30)
    stages = []
    prev = c.u8(h + 0x19B)
    c.pad = c.mask('B')
    for f in range(400):
        c.step()
        s = c.u8(h + 0x19B)
        if s != prev:
            stages.append((f, s))
            prev = s
    c.pad = 0
    res['charge_stage_changes'] = stages[:8]
    res['charge_frames_first_stage'] = stages[0][0] if stages else None
    # walk speed: pixels per frame over 20 frames (best of four directions)
    c.run(120)
    best = 0
    for d, ax, sgn in (('RIGHT', 0xE002, 1), ('LEFT', 0xE002, -1), ('DOWN', 0xE004, 1), ('UP', 0xE004, -1)):
        x0 = c.u16(ax)
        c.run(20, d)
        dx = (c.u16(ax) - x0) * sgn
        dx = dx - 65536 * sgn if abs(dx) > 30000 else dx
        best = max(best, dx / 20.0)
        c.run(40)
    res['walk_px_per_frame_best'] = best
    res['crash_free_frames'] = c.frames
    p = T5 / 'out' / ('gp_measure_%s_%s.json' % (label, 'patched' if patched else 'plain'))
    p.write_text(json.dumps(res, indent=1))
    print(json.dumps(res))


if __name__ == '__main__':
    a = sys.argv
    if a[1] == 'static':
        for l in a[2:]:
            static(l)
    else:
        measure(a[2], len(a) > 3 and a[3] == 'patched')
