#!/usr/bin/env python3
"""T5: the desktop frontend (som-pc, headless SDL dummy drivers, widescreen libretro core) on one generated Ancient Cave ROM.

    t5_pc.py PRESET [--soak-frames N]

1. builds a free-play party state with the new-game flow (scripts/boot_newgame.txt),
2. runs a feature script: widescreen 46/27/off, HUD hide, shoulder slots L1 R1 L2 R2 (weapon, item, spell, spirit), a queued cast,
   an enemy map reached by the game's own warp event with the actor cards (party and enemy RAM) and a cast on it, the target panel,
3. runs a soak script of N frames (default 20,500) with random input, slot presses, warps and option toggles.
Results: $T5/out/pc_PRESET.json and the raw JSON lines in $T5/out/pc_PRESET_*.log.  Nothing is written inside the repository.
"""
import json
import os
import pathlib
import random
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
PC = HERE.parent / 'pc'
T5 = pathlib.Path(os.environ.get('T5', os.path.expanduser('~/.local/state/som-decomp/t5-ac-20261018')))


EXTRA = []      # extra som-pc arguments (for example --gameplay FILE)
TAG = ''        # suffix of the result files


def run_script(preset, name, text, timeout=1800):
    d = T5 / 'pc'
    d.mkdir(parents=True, exist_ok=True)
    script = d / ('%s%s_%s.txt' % (preset, TAG, name))
    script.write_text(text)
    env = dict(os.environ, SOM_PC_CONFIG=str(d / ('cfg_' + preset + TAG)), SOM_PC_STATE=str(d / ('state_' + preset + TAG)), SDL_VIDEODRIVER='dummy', SDL_AUDIODRIVER='dummy')
    os.makedirs(env['SOM_PC_CONFIG'], exist_ok=True)
    r = subprocess.run([str(PC / 'som-pc'), '--rom', str(T5 / 'roms' / (preset + '.sfc')), '--script', str(script)] + EXTRA, capture_output=True, text=True, env=env, timeout=timeout)
    (T5 / 'out').mkdir(exist_ok=True)
    (T5 / 'out' / ('pc_%s%s_%s.log' % (preset, TAG, name))).write_text(r.stdout + '\n--- stderr\n' + r.stderr[-4000:])
    return r, parse(r.stdout + "\n" + r.stderr)


def parse(stdout):
    st, peek, tg, ev, stats = {}, {}, {}, [], None
    for line in stdout.splitlines():
        if not line.startswith('{'):
            continue
        try:
            j = json.loads(line)
        except ValueError:
            continue
        if 'party' in j:
            st[j['tag']] = j
        elif 'peek' in j:
            peek[j['peek']] = bytes.fromhex(j['hex'])
        elif 'targets' in j:
            tg[j['targets']] = j
        elif 'stats' in j:
            stats = j
        else:
            ev.append(j)
    return dict(state=st, peek=peek, targets=tg, events=ev, stats=stats)


def party_pokes():
    return ['POKE E3C4 12345678', 'POKE E5C4 12345678', 'POKE E3C8 3F3F3F3F3F3F3F3F', 'POKE E5C8 3F3F3F3F3F3F3F3F']


def warp_lines(n, wait=300):
    return ['POKE 1FF00 %02X%02X08' % (0x18 | (n >> 8), n & 0xFF), 'POKE D1 00FF7F', 'RUN 1', 'POKE D0 01', 'RUN %d' % wait]


def pick_enemy_warp(preset, state, candidates, heroes):
    """Tries the enemy warps in order; the first one where the slot cast on an enemy fires (request bytes written) is used."""
    tried = []
    for n in candidates[:6]:
        L = ['LOAD %s' % state, 'RUN 60', 'CLEARBIND'] + warp_lines(n) + ['TARGETS t', 'BIND L1 spell 12 near', 'RUN 2', 'SLOT L1', 'WAIT 900', 'RUN 300', 'PEEK req E570 12', 'TARGETS t2', 'STATE p_after']
        r, o = run_script(preset, 'pick%d' % n, '\n'.join(L) + '\n')
        req = o['peek'].get('req', b'\x00')
        t = o['targets'].get('t', {})
        t2 = o['targets'].get('t2', {})
        ps = o['state'].get('p_after', {}).get('party', [])
        tried.append(dict(warp=n, enemies_valid=t.get('enemies_valid'), cast_fired=bool(req[0]) if heroes > 1 else None, request=req.hex(),
                          enemy_hp_before=[a[5] for a in t.get('actors', [])[3:]], enemy_hp_after=[a[5] for a in t2.get('actors', [])[3:]],
                          sprite_mp_after=ps[2]['mp'] if len(ps) > 2 else None))
        if heroes == 1 and t.get('enemies_valid'):
            return n, tried
        if heroes > 1 and req[0]:
            return n, tried
    return (candidates[0] if candidates else 0), tried


def make_fixture(preset, heroes):
    boot = (PC / 'scripts/boot_newgame.txt').read_text().replace('STATE booted', 'STATE booted')
    lines = [boot]
    if heroes > 1:
        lines += party_pokes()
    out = T5 / 'pc' / (preset + TAG + '_party.state')
    lines += ['RUN 60', 'STATE fixture', 'SAVESTATE %s' % out]
    r, p = run_script(preset, 'fixture', '\n'.join(lines) + '\n')
    return out, p


def feature_script(state, shots, heroes, warp_n):
    L = ['LOAD %s' % state, 'RUN 60', 'CLEARBIND', 'SET overlay small', 'STATE start', 'PEEK weapons CC54 8', 'PEEK items CC48 24', 'TARGETS floor1']
    L += ['SET wide 27', 'RUN 8', 'STATE w27', 'SET wide off', 'RUN 8', 'STATE woff', 'SET wide 46', 'RUN 8', 'STATE w46']
    L += ['SET hud hide', 'RUN 10', 'STATE hud_hide', 'SHOT %s_hudhide_w46.bmp' % shots, 'SET hud show', 'RUN 10', 'STATE hud_show']
    # shoulder slots: weapon on L1, item on R1, ally spell on L2, spirit list on R2
    L += ['BIND L1 weapon 2 select', 'BIND R1 item 0 lesshp', 'BIND L2 spell 9 lesshp', 'BIND R2 spirit 1 select', 'RUN 4', 'STATE slots_bound']
    L += ['SLOT L1', 'WAIT 900', 'RUN 60', 'STATE after_L1_weapon']
    L += ['SLOT R1', 'WAIT 900', 'RUN 60', 'STATE after_R1_item']
    L += ['SLOT L2', 'WAIT 900', 'RUN 450', 'STATE after_L2_spell']
    L += ['SLOT R2', 'WAIT 900', 'RUN 90', 'PEEK ring_after_R2 7D 1', 'PEEK ringcat_after_R2 1808 4', 'STATE after_R2_spirit']
    if heroes > 1:
        # queue: the same caster twice (a slot press while the ring is open closes the ring first)
        L += ['SLOT L2', 'RUN 3', 'STATE cast_start', 'SLOT L2', 'RUN 2', 'STATE queued', 'WAIT 900', 'RUN 300', 'STATE after_queue']
    else:
        L += ['SLOT L1', 'WAIT 900', 'RUN 120', 'STATE after_refusals']
    # enemy map
    L += warp_lines(warp_n) + ['TARGETS enemy_map', 'STATE enemy_map', 'PEEK enemy3 E600 8']
    if heroes > 1:
        L += ['BIND L1 spell 12 near', 'RUN 2', 'PEEK req_before E570 12', 'SLOT L1', 'WAIT 900', 'RUN 500', 'TARGETS after_cast', 'STATE after_enemy_cast', 'PEEK req_after E570 12']
        L += ['BIND L2 spell 12 select', 'RUN 2', 'SLOT L2', 'RUN 12', 'STATE target_panel', 'SHOT %s_target_panel.bmp' % shots, 'UIBACK', 'RUN 30', 'STATE after_panel']
    else:
        L += ['BIND L2 item 0 select', 'RUN 2', 'SLOT L2', 'RUN 12', 'STATE target_panel', 'SHOT %s_target_panel.bmp' % shots, 'UIBACK', 'RUN 30', 'STATE after_panel', 'TARGETS after_panel']
    L += ['SET wide 46', 'RUN 30', 'STATE end']
    return '\n'.join(L) + '\n'


def soak_script(state, frames, seed, heroes, warps):
    rnd = random.Random(seed)
    L = ['LOAD %s' % state, 'RUN 60', 'CLEARBIND', 'SET overlay small', 'BIND L1 weapon 3 select', 'BIND R1 item 0 lesshp', 'BIND L2 spell 9 lesshp', 'BIND R2 spell 12 near']
    done = 0
    pads = ['UP', 'DOWN', 'LEFT', 'RIGHT'] * 5 + ['B', 'B,UP', 'B,DOWN', 'A,LEFT', 'A,RIGHT', 'UP,RIGHT', 'DOWN,LEFT', 'X', 'Y', 'START']
    k = 0
    while done < frames:
        r = rnd.random()
        if r < 0.70:
            n = rnd.randint(6, 40)
            L.append('PAD %s %d' % (rnd.choice(pads), n))
            done += n
        elif r < 0.84:
            L.append('SLOT %s' % rnd.choice(['L1', 'R1', 'L2', 'R2']))
            L.append('RUN %d' % rnd.randint(2, 20))
            done += 8 + 10
        elif r < 0.87:
            L += warp_lines(rnd.choice(warps), 200)
            done += 200
        elif r < 0.91:
            L.append('SET wide %s' % rnd.choice(['46', '27', 'off', '46']))
        elif r < 0.94:
            L.append('SET hud %s' % rnd.choice(['hide', 'show']))
        elif r < 0.97:
            L.append('SET overlay %s' % rnd.choice(['off', 'small', 'medium']))
        else:
            L += ['TARGETS s%d' % k, 'STATE s%d' % k]
            k += 1
        if done // 300 > (done - 40) // 300:
            for h in range(heroes):                       # keep the heroes alive: HP and MP back to a legal value
                b = 0xE000 + 0x200 * h
                L.append('POKE %X 2800' % (b + 0x182))
                if h:
                    L.append('POKE %X 08' % (b + 0x186))
        if done % 600 < 20:
            L.append('RUN 1')
    L += ['SET wide 46', 'SET hud show', 'RUN 30', 'STATE final', 'TARGETS final']
    return '\n'.join(L) + '\n'


def main():
    a = sys.argv
    preset = a[1]
    soak = int(a[a.index('--soak-frames') + 1]) if '--soak-frames' in a else 20500
    global EXTRA, TAG
    if '--gameplay' in a:
        EXTRA = ['--gameplay', a[a.index('--gameplay') + 1]]
        TAG = '_gp'
    heroes = 1 if preset.endswith('hero') else 3
    res = dict(preset=preset, heroes_expected=heroes, gameplay=EXTRA[1] if EXTRA else None)
    state, fx = make_fixture(preset, heroes)
    f = fx['state'].get('fixture')
    res['fixture'] = dict(ok=bool(f), map=f and f['map'], mode=f and f['mode'], frame_w=f and f['frame_w'], present=f and [p['present'] for p in f['party']],
                          hp=f and [p['hp'] for p in f['party']], mp=f and [p['mp'] for p in f['party']])
    shots = str(T5 / 'pc/shots' / preset)
    (T5 / 'pc/shots').mkdir(parents=True, exist_ok=True)
    wr = subprocess.run([sys.executable, str(HERE / 't5_warps.py'), preset, str(state)], capture_output=True, text=True)
    warps = json.loads([l for l in wr.stdout.splitlines() if l.startswith('{')][-1])
    res['warps'] = warps
    enemy_n, res['enemy_warp_tried'] = pick_enemy_warp(preset, state, ([n for n in warps['enemy'] if n != 0] + [0 for _ in [0] if 0 in warps['enemy']]) or [0], heroes)
    res['enemy_warp'] = enemy_n
    soak_warps = warps['enemy'][:4] + warps['quiet'][:3] or [0]
    r, ft = run_script(preset, 'features', feature_script(state, shots, heroes, enemy_n))
    res['features_rc'] = r.returncode
    S = ft['state']
    res['feature_states'] = {k: dict(frame=v['frame'], mode=v['mode'], map=v['map'], frame_w=v['frame_w'], safe=v['safe'], modal=v['modal'],
                                     party=[(p['present'], p['hp'], p['mp'], p['weapon']) for p in v['party']], items=[tuple(i) for i in v['items'] if i[1]][:8],
                                     queue=v['queue'], slots=v['slots'], err=v['direct']['err']) for k, v in S.items()}
    res['targets'] = ft['targets']
    res['peek'] = {k: v.hex() for k, v in ft['peek'].items()}
    res['events'] = [e for e in ft['events'] if e.get('event') in ('unknown-command', 'error', 'shot-failed', 'load-failed', 'uitap-missing')]
    rs, sk = run_script(preset, 'soak', soak_script(state, soak, sum(map(ord, preset)), heroes, soak_warps), timeout=3600)
    res['soak_rc'] = rs.returncode
    res['soak_stats'] = sk['stats']
    res['soak_states'] = len(sk['state'])
    res['soak_errs'] = sorted({v['direct']['err'] for v in sk['state'].values()})
    res['soak_modes'] = sorted({v['mode'] for v in sk['state'].values()})
    res['soak_maps'] = sorted({v['map'] for v in sk['state'].values()})
    res['soak_widths'] = sorted({v['frame_w'] for v in sk['state'].values()})
    res['soak_bad_events'] = [e for e in sk['events'] if e.get('event') in ('unknown-command', 'error', 'load-failed')][:10]
    p = T5 / 'out' / ('pc_%s%s.json' % (preset, TAG))
    p.write_text(json.dumps(res, indent=1))
    print('wrote', p, 'features_rc', res['features_rc'], 'soak_rc', res['soak_rc'], 'soak_frames', sk['stats'] and sk['stats']['frames'])
    return 0


if __name__ == '__main__':
    sys.exit(main())
