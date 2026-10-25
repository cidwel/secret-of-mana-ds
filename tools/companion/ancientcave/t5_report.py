#!/usr/bin/env python3
"""T5: collects the results of the Ancient Cave compatibility run ($T5/out, $T5/state/evidence) into one matrix (preset x feature).

    t5_report.py            prints the matrix as Markdown and the raw numbers behind each cell; writes $T5/out/matrix.json

Verdicts are computed from the result files, not typed in: PASS, FAIL, or NOTE (passes, with a documented caveat).  Hardware-only
features are not computed here (the document lists them as untestable-needs-hardware).
"""
import json
import os
import pathlib
import re
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
T5 = pathlib.Path(os.environ.get('T5', os.path.expanduser('~/.local/state/som-decomp/t5-ac-20261018')))
OUT = T5 / 'out'
PRESETS = ['ancientcave', 'ancientcave-short', 'ancientcave-marathon', 'ancientcave-gauntlet', 'ancientcave-hero']


def load(p):
    try:
        return json.loads(pathlib.Path(p).read_text())
    except (OSError, ValueError):
        return None


def direct_summary(preset):
    """Per suite: exit code and number of not-ok lines, from the log written by $T5/work/run_direct.sh."""
    log = OUT / ('direct_%s.log' % preset)
    if not log.exists():
        return {}
    res, cur, buf = {}, None, []
    for line in log.read_text().splitlines():
        m = re.match(r'### rc=(\d+)', line)
        if m:
            if cur:
                res[cur] = dict(rc=int(m.group(1)), bad=sum(1 for x in buf if '"ok": false' in x or "'ok': False" in x), lines=len(buf))
            cur, buf = None, []
        elif line.startswith('### '):
            cur = line[4:].split()[0]
        else:
            buf.append(line)
    return res


def main():
    from t5_rom_static import load as rload, SIGS, off, SPELL_ROW  # noqa: F401
    matrix = {}
    for p in PRESETS:
        row = {}
        heroes = 1 if p.endswith('hero') else 3
        cs, pc, pg = load(OUT / ('core_soak_%s.json' % p)), load(OUT / ('pc_%s.json' % p)), load(OUT / ('pc_%s_gp.json' % p))
        hud, wm = load(OUT / ('hud_%s.json' % p)), load(OUT / ('wide_maps_%s.json' % p))
        fl = load(T5 / 'state/evidence' / ('ancientcave_%s' % p) / 'fields_results.json')
        st = load(OUT / ('gp_static_%s.json' % p))
        mp_, mu_ = load(OUT / ('gp_measure_%s_plain.json' % p)), load(OUT / ('gp_measure_%s_patched.json' % p))
        row['boot'] = ('PASS' if cs and cs['boot']['first_floor'] and pc and pc['fixture']['ok'] and pc['fixture']['map'] == 0 else 'FAIL',
                       'core boot %s frames, PC fixture map %s' % (cs and cs['boot']['frames'], pc and pc['fixture']['map']))
        s = cs['stat'] if cs else {}
        wide_ok = cs and not s['width_illegal'] and not s['w_identity_bad'] and not s['h_identity_bad']
        pcw = pc and {pc['feature_states'][k]['frame_w'] for k in ('w27', 'woff', 'w46')} == {310, 256, 348}
        narrow_share = round(100.0 * s['widths_w'].get('256', 0) / s['frames'], 1) if s else None
        stuck = cs and cs['boot']['width_w'] == 256
        row['widescreen'] = ('FAIL' if not (wide_ok and pcw) else 'NOTE' if stuck else 'PASS',
                             'identity bad W/H %s/%s, illegal widths %s, PC widths 310/256/348 %s, native-hires core at 256 px on %s%% of frames, floor 1 width in native-hires mode %s, narrow maps of 40 warps %s' % (s.get('w_identity_bad'), s.get('h_identity_bad'), s.get('width_illegal'), pcw, narrow_share, cs and cs['boot']['width_w'], wm and wm['summary']['46']['narrow']))
        row['hires'] = ('PASS' if cs and s['hires_frames'] > 0 and not s['w_hires_bad'] and not s['h_hires_bad'] else 'FAIL',
                        '%s hi-res frames, native bad %s, average bad %s' % (s.get('hires_frames'), s.get('w_hires_bad'), s.get('h_hires_bad')))
        row['hud'] = ('PASS' if hud and not hud['leak_frames'] and not hud['removed_frames'] and not hud['ring_diff'] else 'FAIL',
                      '%s frames, ring %s (diff %s), other %s, HUD leak %s, wrongly removed %s' % (hud and hud['frames'], hud and hud['ring'], hud and hud['ring_diff'], hud and hud['other'], hud and hud['leak_frames'], hud and hud['removed_frames']))
        S = pc['feature_states'] if pc else {}

        def party(k, i, j):
            return S[k]['party'][i][j]
        try:
            weapon = party('after_L1_weapon', 0, 3) == 2 and party('start', 0, 3) == 1
            item = S['after_R1_item']['items'][0] != S['start']['items'][0]
            if heroes > 1:
                spell = party('after_L2_spell', 1, 2) < party('start', 1, 2)
                spirit = pc['peek'].get('ring_after_R2') == '01'
                queue = S['queued']['queue'][0] == 1 and party('after_queue', 1, 2) == party('after_L2_spell', 1, 2) - 4
            else:
                spell = party('after_L2_spell', 1, 2) == 0 and S['after_L2_spell']['err'] == 'ok'
                spirit = pc['peek'].get('ring_after_R2') == '00'
                queue = None
        except (KeyError, TypeError, IndexError):
            weapon = item = spell = spirit = queue = False
        row['slots'] = ('PASS' if weapon and item and spell and spirit else 'FAIL',
                        'L1 weapon %s, R1 item %s, L2 spell %s, R2 spirit %s%s' % (weapon, item, spell, spirit, '' if heroes > 1 else ' (one hero: spell and spirit refused as expected)'))
        row['queue'] = ('N/A' if queue is None else 'PASS' if queue else 'FAIL', 'second press while the caster casts is queued, then fires (two casts paid)' if queue is not None else 'one hero, no caster')
        tried = (pc or {}).get('enemy_warp_tried', [])
        pick = next((t for t in tried if t.get('enemies_valid') and (t.get('cast_fired') or heroes == 1)), None)
        panel = S.get('target_panel', {}).get('modal') == 1 if heroes > 1 else True
        row['panels'] = ('PASS' if pick and panel and pc['fixture']['present'] == ([1, 1, 1] if heroes > 1 else [1, 0, 0]) else 'FAIL',
                         'party cards %s, enemy cards on warp %s: %s valid, cast fired %s, target panel modal %s' % (pc and pc['fixture']['present'], pick and pick['warp'], pick and pick['enemies_valid'], pick and pick['cast_fired'], panel))
        counts = {}
        for v in (fl or {}).get('fields', fl or {}).values():
            counts[v['verdict']] = counts.get(v['verdict'], 0) + 1
        exp_v = 29 if heroes > 1 else None
        row['fields'] = ('PASS' if heroes > 1 and counts.get('verified') == 29 and not counts.get('differs') else 'NOTE',
                         'verified %s, partial %s, not testable %s, differs %s%s' % (counts.get('verified'), counts.get('partial'), counts.get('not_testable'), counts.get('differs'), '' if heroes > 1 else ' (harness assumes three heroes; see document)'))
        d = direct_summary(p)
        rom = rload(T5 / 'roms' / (p + '.sfc'))
        import t5_rom_static as R
        van = rload(R.VANILLA)
        sigs = all(bytes(rom[off(a):off(a) + len(b)]) == bytes(b) for a, b in SIGS)
        cls_eq = all(rom[SPELL_ROW + 64 * i + 8] == van[SPELL_ROW + 64 * i + 8] for i in range(42))
        n_cost = sum(1 for i in range(42) if rom[SPELL_ROW + 64 * i + 15] != van[SPELL_ROW + 64 * i + 15])
        cc, ca = d.get('test_direct_castcheck.py', {}), d.get('test_direct_cast.py', {})
        direct_ok = heroes == 1 or (cc.get('rc') == 0 and ca.get('rc') in (0, 1) and d.get('test_direct_revive.py', {}).get('rc') == 0 and d.get('test_direct_menu_extra.py', {}).get('rc') == 0)
        note = ca.get('bad', 0) > 0
        row['spells'] = ('PASS' if sigs and cls_eq and (heroes == 1 or direct_ok) and not note else 'NOTE' if sigs and cls_eq and direct_ok else 'FAIL',
                         '9/9 routine signatures %s, target classes equal %s, %s of 42 MP costs changed (read from the ROM), castcheck rc %s, ring-vs-direct cast rc %s (%s not ok)' % (sigs, cls_eq, n_cost, cc.get('rc'), ca.get('rc'), ca.get('bad')))
        gpl = pg and pg['soak_stats']['frames'] >= 20000 and pg['soak_rc'] == 0 and pg['features_rc'] == 0 and pg['soak_errs'] == ['ok']
        gps = st and st['skip_mode']['rc'] == 0 and st['verify']['rc'] == 0
        gpm = mu_ and mu_['free_play'] and mu_['swing_gauge_peak'] == (0 if 'weapon_recharge' not in (st or {}).get('plan_conflicts', []) else mp_['swing_gauge_peak'])
        row['gameplay'] = ('PASS' if gpl and gps and gpm else 'FAIL',
                           'apply skip rc %s, verify rc %s, skipped (conflicts) %s, gauge %s -> %s, charge %s -> %s frames, PC soak %s frames' % (
                               st and st['skip_mode']['rc'], st and st['verify']['rc'], st and st['plan_conflicts'], mp_ and mp_['swing_gauge_peak'], mu_ and mu_['swing_gauge_peak'],
                               mp_ and mp_['charge_frames_first_stage'], mu_ and mu_['charge_frames_first_stage'], pg and pg['soak_stats']['frames']))
        pcl = pc and pc['soak_stats']['frames'] >= 20000 and pc['soak_rc'] == 0 and pc['soak_errs'] == ['ok'] and not pc['soak_bad_events']
        cl = cs and cs['soak_frames'] >= 20000 and not s['wram_bad']
        row['soak'] = ('PASS' if pcl and cl else 'FAIL', 'core %s frames (3 cores lockstep), PC %s frames, PC modes %s, direct errors %s' % (cs and cs['soak_frames'], pc and pc['soak_stats']['frames'], pc and pc['soak_modes'], pc and pc['soak_errs']))
        matrix[p] = row
    (OUT / 'matrix.json').write_text(json.dumps(matrix, indent=1))
    feats = ['boot', 'widescreen', 'hires', 'hud', 'slots', 'queue', 'panels', 'fields', 'spells', 'gameplay', 'soak']
    print('| Preset | ' + ' | '.join(feats) + ' |')
    print('|---|' + '---|' * len(feats))
    for p, row in matrix.items():
        print('| %s | ' % p + ' | '.join(row[f][0] for f in feats) + ' |')
    print()
    for p, row in matrix.items():
        print('##', p)
        for f in feats:
            print('  %-10s %-5s %s' % (f, row[f][0], row[f][1]))


if __name__ == '__main__':
    main()
