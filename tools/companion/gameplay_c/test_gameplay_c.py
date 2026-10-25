#!/usr/bin/env python3
"""Compares the C patcher (som_gameplay.c) with tools/gameplay/som_gameplay.py on random option combinations.

  SOM_ROM=<US v1.0 rom> [SOM_ROM_EXTRA=<more roms, colon separated>] python3 test_gameplay_c.py [cases]

Outputs go to the temp directory.  A case where the Python tool aborts (a code site it cannot find) is counted and skipped: the C patcher drops the
option instead of aborting, by design.
"""
import json, os, pathlib, random, subprocess, sys, tempfile

HERE = pathlib.Path(__file__).resolve().parent
PY = HERE.parents[1] / 'gameplay' / 'som_gameplay.py'
sys.path.insert(0, str(PY.parent))
import som_gameplay as sg  # noqa

ENUMS = {'weapon_recharge': ['vanilla', 'fast', 'faster', 'instant'], 'run_recharge': ['vanilla', 'fast', 'none'],
         'charge_speed': ['vanilla', 'fast', 'faster', 'instant'], 'monster_flinch': ['vanilla', 'strong', 'heavy', 'never'],
         'hero_flinch': ['vanilla', 'light', 'never'], 'qol_scroll_deadzone': ['vanilla', 'tight', 'tighter']}
INTS = {'weapon_level_rate': [1, 2, 3], 'crit_multiplier': [2, 3, 4], 'walk_speed': [2, 3, 4], 'qol_item_limit': [4, 7], 'qol_magic_level_rate': [1, 2, 3],
        'qol_ring_speed': [1, 2], 'qol_message_speed': [1, 2, 4]}
BOOLS = ['charge_speed_up_bonus', 'qol_chest_fast', 'qol_fast_transitions', 'qol_fast_fades', 'qol_unready_damage', 'qol_levelup_mp_restore', 'qol_dead_exp', 'qol_shop_fill']
NUMS = {'charge_frames_per_stage': (1, 90), 'hero_crit_bonus_girl': (0, 120), 'hero_crit_bonus_others': (0, 120), 'charge_bonus_item_id': (0, 254)}


def rand_case(rng):
    py, c = {}, {}
    for k, vals in ENUMS.items():
        if rng.random() < 0.5:
            i = rng.randrange(len(vals)); py[k] = vals[i]; c[k] = i
    for k, vals in INTS.items():
        if rng.random() < 0.4:
            py[k] = rng.choice(vals); c[k] = py[k]
    for k, (lo, hi) in NUMS.items():
        if rng.random() < 0.3:
            py[k] = rng.randint(lo, hi); c[k] = py[k]
    for k in BOOLS:
        if rng.random() < 0.4:
            py[k] = True; c[k] = 1
    return py, c


def main():
    n = int(sys.argv[1]) if len(sys.argv) > 1 else 200
    roms = [os.environ['SOM_ROM']] + [p for p in os.environ.get('SOM_ROM_EXTRA', '').split(':') if p]
    tmp = pathlib.Path(tempfile.mkdtemp(prefix='sgpc-'))
    cli = tmp / 'sgp_cli'
    subprocess.run(['gcc', '-std=c99', '-Wall', '-Wextra', '-O2', '-fsanitize=address,undefined', '-o', str(cli), str(HERE / 'sgp_cli.c'), str(HERE / 'som_gameplay.c')], check=True)
    rng = random.Random(20261020)
    ok = skipped_py = bad = 0
    cases = [({}, {})] + [rand_case(rng) for _ in range(n)]
    for rom in roms:
        raw = pathlib.Path(rom).read_bytes()
        variants = [(rom, raw)]
        if len(raw) % 0x8000 == 0:
            hp = tmp / 'hdr.sfc'; hp.write_bytes(b'\0' * 512 + raw); variants.append((str(hp), b'\0' * 512 + raw))
        for path, _ in variants:
            for i, (py, c) in enumerate(cases):
                (tmp / 'o.json').write_text(json.dumps({'options': py}))
                r = subprocess.run([sys.executable, str(PY), 'apply', '--rom', path, '--options', str(tmp / 'o.json'), '--out', str(tmp / 'py.sfc'), '--on-conflict', 'skip'], capture_output=True, text=True)
                pyskip = [l for l in r.stderr.splitlines() if l.startswith('skipped option')]
                if r.returncode != 0:
                    skipped_py += 1; continue
                args = [str(cli), path, str(tmp / 'c.sfc')] + ['%s=%d' % kv for kv in c.items()]
                rc = subprocess.run(args, capture_output=True, text=True)
                if rc.returncode != 0:
                    print('C failed', path, py, rc.stderr); bad += 1; continue
                same = (tmp / 'py.sfc').read_bytes() == (tmp / 'c.sfc').read_bytes()
                if not same:
                    print('DIFFERENT', path, py, 'python skipped:', pyskip, 'C:', rc.stdout.strip()); bad += 1
                else:
                    ok += 1
    print('identical %d, differ %d, python aborted %d' % (ok, bad, skipped_py))
    sys.exit(1 if bad else 0)


main()
