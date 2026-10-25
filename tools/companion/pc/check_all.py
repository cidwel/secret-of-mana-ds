#!/usr/bin/env python3
"""End-to-end checks of the PC frontend on the three ROM families, headless (no window, no audio): unit tests, the full session on vanilla
(picker, cast, queue, item), and the generic scenario on vanilla, Magno and a generated Ancient Cave ROM.

    python3 check_all.py [--skip-unit]

Needs the owner's ROMs and the local fixtures under ~/.local/state/som-decomp (see docs/companion-pc-20261018.md); a missing Ancient Cave
fixture is built with make_fixture.sh from the ROM that som-dualscreen generate wrote.  Exit status 0 when every check passes."""
import json
import os
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
HOME = pathlib.Path.home()
STATE = HOME / '.local/state/som-decomp'
ROMS = pathlib.Path('/home/cidwel/projects/rommancer/roms/snes')
VANILLA = ROMS / 'Secret of Mana (USA).smc'
MAGNO = ROMS / 'Secret of Mana (USA) [T-Esp101.2_Magno].smc'
ADV = STATE / 'companion-20261017/adv/advanced_equipped.state'
MAGNO_STATE = STATE / 'companion-20261017/work/party3_magno.state'
AC_ROM = STATE / 'companion-cli-20261017/gen/2c845ac55030ff15.sfc'      # som-dualscreen generate, preset ancientcave-short, seed 12345
AC_STATE = STATE / 'pc/ac12345_party.state'

fails = 0


def check(name, ok, detail=''):
    global fails
    print('%s  %s %s' % ('PASS' if ok else 'FAIL', name, '' if ok else detail))
    if not ok:
        fails += 1


def raw_states(rom, script, tag):
    """Runs som-pc directly and returns {tag: state dict}."""
    env = dict(os.environ, SOM_PC_CONFIG=str(STATE / ('pc/check_cfg_' + tag)), SOM_PC_STATE=str(STATE / ('pc/check_state_' + tag)),
               SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    os.makedirs(env['SOM_PC_CONFIG'], exist_ok=True)
    os.makedirs(STATE / 'pc/shots', exist_ok=True)
    subprocess.run([str(HERE / 'som-pc'), '--help'], capture_output=True)
    r = subprocess.run([str(HERE / 'som-pc'), '--rom', str(rom), '--script', str(script)], capture_output=True, text=True, env=env, timeout=900)
    out = {}
    for line in r.stdout.splitlines():
        if line.startswith('{'):
            try:
                d = json.loads(line)
            except ValueError:
                continue
            if 'tag' in d:
                out[d['tag']] = d
    return out, r


def mp(d, hero):
    return d['party'][hero]['mp']


def main():
    if '--skip-unit' not in sys.argv:
        r = subprocess.run([str(HERE / 'run_pc_tests.sh')], capture_output=True, text=True)
        check('unit tests and overlay goldens (run_pc_tests.sh): ' + r.stdout.strip().splitlines()[-1] if r.stdout.strip() else 'unit tests', r.returncode == 0, r.stdout[-300:] + r.stderr[-300:])

    # 1. vanilla, the full session
    s, r = raw_states(VANILLA, HERE / 'scripts/s3_session.txt', 'vanilla_s3')
    check('vanilla: picker opens after a 1 s hold of L1', s.get('picker_open', {}).get('modal') == 1)
    check('vanilla: picker assigns Fireball (spell 12) with policy NEAR to L1 and returns to the game', s.get('l1_bound', {}).get('slots', [[0]])[0] == [3, 12, 1] and s['l1_bound']['modal'] == 0)
    check('vanilla: the second press while the caster is busy is queued', s.get('queued', {}).get('queue') == [0, 1])
    check('vanilla: both casts happened (sprite MP 47 -> 43)', s.get('after_queue', {}).get('party', [0, 0, {'mp': 0}])[2]['mp'] == 43)
    check('vanilla: picker assigns item 0 with policy LESS HP to R2', s.get('r2_bound', {}).get('slots', [0, 0, 0, [0]])[3] == [2, 0, 3])
    check('vanilla: R2 uses the item (7 -> 6)', s.get('item_used', {}).get('items', [[0, 0]])[0][1] == 6)

    # 2. generic scenario on the three ROMs
    if not AC_STATE.exists():
        subprocess.run([str(HERE / 'make_fixture.sh'), str(AC_ROM), str(AC_STATE)], capture_output=True)
    for label, rom, st in (('vanilla', VANILLA, ADV), ('magno', MAGNO, MAGNO_STATE), ('ancientcave', AC_ROM, AC_STATE)):
        if not (rom.exists() and st.exists()):
            check('%s: ROM and fixture present' % label, False, '%s %s' % (rom, st))
            continue
        script = STATE / ('pc/check_s4_%s.txt' % label)
        shots = STATE / 'pc/shots'
        os.makedirs(shots, exist_ok=True)
        script.write_text('LOAD %s\n%s' % (st, (HERE / 'scripts/s4_generic.txt').read_text().replace('@SHOTS@', str(shots / ('check_' + label)))))
        s, r = raw_states(rom, script, label + '_s4')
        b, a, it = s.get('bound_r2'), s.get('after_cast'), s.get('after_item')
        check('%s: loads, wide frame %s' % (label, b['frame_w'] if b else '?'), bool(b) and b['frame_w'] in (310, 348))
        check('%s: L1 = girl Undine spell, R2 = item through the picker' % label, bool(b) and b['slots'][0] == [3, 9, 3] and b['slots'][3] == [2, 0, 3])
        check('%s: the slot cast spends MP (%s -> %s)' % (label, mp(b, 1) if b else '?', mp(a, 1) if a else '?'), bool(a) and mp(a, 1) < mp(b, 1))
        check('%s: the item slot uses the item (%s -> %s)' % (label, b['items'][0][1] if b else '?', it['items'][0][1] if it else '?'), bool(it) and it['items'][0][1] < b['items'][0][1])
    print('%d failure(s)' % fails)
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
