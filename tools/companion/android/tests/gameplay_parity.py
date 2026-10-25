#!/usr/bin/env python3
"""Byte-for-byte parity of tools/companion/som_gameplay_apply.c with tools/gameplay/som_gameplay.py (apply, --on-conflict skip).

Usage: gameplay_parity.py [WORKDIR]   ROMs: SOM_ROM / SOM_ROM_MAGNO (companion/common.py) and up to three randomizer outputs (SOM_GEN_ROM glob).
Also fails when the option catalogue of the Python tool has a key the C port does not know (a new QOL option must be ported first)."""
import glob
import json
import os
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parents[3]
sys.path.insert(0, str(ROOT / 'tools' / 'gameplay'))
sys.path.insert(0, str(HERE.parents[1]))
import common  # noqa: E402
import som_gameplay as sg  # noqa: E402

W = pathlib.Path(sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser('~/.local/state/som-decomp/thor-r4-20261020/parity'))
W.mkdir(parents=True, exist_ok=True)
CSRC = HERE.parent.parent / 'som_gameplay_apply.c'
CLI = W / 'sga_cli'
subprocess.run(['gcc', '-std=c99', '-O1', '-g', '-Wall', '-Wextra', '-fsanitize=address,undefined', '-o', str(CLI), str(HERE / 'sga_cli.c'), str(CSRC)], check=True)

menu = json.loads((ROOT / 'tools/gameplay/gameplay_menu.json').read_text())
keys = set(sg.CATALOGUE) | {o['key'] for o in menu['options']}
src = CSRC.read_text()
missing = sorted(k for k in keys if '"%s"' % k not in src)
if missing:
    print('FAIL: options not ported to som_gameplay_apply.c:', missing)
    sys.exit(1)

SETS = [{}, {'weapon_recharge': 'instant'}, {'weapon_recharge': 'fast'}, {'weapon_recharge': 'faster'}, {'run_recharge': 'none'}, {'run_recharge': 'fast'},
        {'monster_flinch': 'strong'}, {'monster_flinch': 'heavy'}, {'monster_flinch': 'never'}, {'hero_flinch': 'light'}, {'hero_flinch': 'never'},
        {'monster_flinch': 'heavy', 'hero_flinch': 'light'}, {'monster_flinch': 'strong', 'hero_flinch': 'never'}, {'charge_speed': 'fast'},
        {'charge_speed': 'faster'}, {'charge_speed': 'instant'}, {'charge_frames_per_stage': 30}, {'charge_frames_per_stage': 60, 'charge_speed_up_bonus': True},
        {'charge_speed': 'fast', 'charge_speed_up_bonus': True}, {'charge_bonus_item_id': 77},
        {'charge_speed': 'faster', 'charge_bonus_item_id': 12, 'charge_speed_up_bonus': True},
        {'hero_crit_bonus_girl': 40}, {'hero_crit_bonus_others': 40, 'hero_crit_bonus_girl': 0}, {'crit_multiplier': 3}, {'crit_multiplier': 4},
        {'walk_speed': 3}, {'walk_speed': 4}, {'weapon_level_rate': 2}, {'weapon_level_rate': 3}, {'fix_checksum': 'never', 'weapon_recharge': 'instant'},
        {'fix_checksum': 'always', 'walk_speed': 3},
        {'weapon_recharge': 'instant', 'run_recharge': 'none', 'monster_flinch': 'heavy', 'hero_flinch': 'light', 'charge_speed': 'fast',
         'charge_speed_up_bonus': True, 'hero_crit_bonus_girl': 40, 'hero_crit_bonus_others': 40, 'crit_multiplier': 3, 'walk_speed': 4,
         'weapon_level_rate': 3, 'charge_bonus_item_id': 20}]

SETS += [{'qol_chest_fast': True}, {'qol_unready_damage': True}, {'qol_fast_transitions': True}, {'qol_fast_fades': True},
         {'qol_item_limit': 7}, {'qol_levelup_mp_restore': True}, {'qol_dead_exp': True}, {'qol_dead_exp': True, 'qol_levelup_mp_restore': True},
         {'qol_magic_level_rate': 2}, {'qol_magic_level_rate': 3}, {'qol_scroll_deadzone': 'tight'}, {'qol_scroll_deadzone': 'tighter'},
         {'qol_chest_fast': True, 'qol_fast_fades': True, 'qol_item_limit': 7,
          'qol_levelup_mp_restore': True, 'qol_dead_exp': True, 'qol_scroll_deadzone': 'tight', 'qol_magic_level_rate': 3, 'weapon_recharge': 'faster',
          'monster_flinch': 'strong', 'charge_speed': 'instant'}]

SETS += [{'qol_ring_speed': 2}, {'qol_message_speed': 2}, {'qol_message_speed': 4}, {'qol_shop_fill': True},
         {'qol_dead_exp': True, 'qol_levelup_mp_restore': True, 'qol_ring_speed': 2, 'qol_message_speed': 4, 'qol_shop_fill': True, 'qol_item_limit': 7,
          'qol_magic_level_rate': 2}]

gen = sorted(glob.glob(os.environ.get('SOM_GEN_ROM', str(pathlib.Path.home() / '.local/state/som-decomp/companion-cli-20261017/gen/*.sfc'))))[:3]
roms = [('us', common.ROM_VANILLA), ('magno', common.ROM_MAGNO)] + [('gen%d' % i, pathlib.Path(p)) for i, p in enumerate(gen)]
fails = n = 0
for name, rom in roms:
    if not rom.exists():
        print('skip', name, rom)
        continue
    for i, opts in enumerate(SETS):
        of = W / 'opts.json'
        of.write_text(json.dumps({'options': opts}))
        py, c = W / ('py_%s_%d.sfc' % (name, i)), W / ('c_%s_%d.sfc' % (name, i))
        rp = subprocess.run([sys.executable, str(ROOT / 'tools/gameplay/som_gameplay.py'), 'apply', '--rom', str(rom), '--options', str(of), '--out', str(py),
                             '--on-conflict', 'skip'], capture_output=True, text=True)
        rc = subprocess.run([str(CLI), str(rom), str(of), str(c)], capture_output=True, text=True)
        n += 1
        if rp.returncode != 0 or rc.returncode != 0:
            if not (rp.returncode != 0 and rc.returncode != 0):   # both refusing (a hack rewrote a site) is the same verdict
                fails += 1
                print('FAIL %s set %d %s: python rc %d %s | c rc %d %s' % (name, i, opts, rp.returncode, rp.stderr.strip()[:120], rc.returncode, rc.stderr.strip()[:120]))
            continue
        a, b = py.read_bytes(), c.read_bytes()
        if a != b:
            fails += 1
            d = [k for k in range(min(len(a), len(b))) if a[k] != b[k]][:5]
            print('FAIL %s set %d %s: bytes differ at %s (len %d vs %d)' % (name, i, opts, [hex(k) for k in d], len(a), len(b)))
        py.write_bytes(b'')   # keep the work dir small
        c.write_bytes(b'')
print('parity: %d comparisons, %d failures' % (n, fails))
sys.exit(1 if fails else 0)
