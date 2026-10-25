#!/usr/bin/env python3
"""Unit tests for som_gameplay.py.  The ROM is never stored in the repository: set SOM_ROM to a US v1.0 image (and optionally
SOM_ROM_MAGNO); tests are skipped without it.  Output goes to the temp directory (TMPDIR) only."""
import json
import os
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import som_gameplay as sg  # noqa: E402

ROM = os.environ.get('SOM_ROM')
MAGNO = os.environ.get('SOM_ROM_MAGNO')
ALL = {'weapon_recharge': 'instant', 'run_recharge': 'none', 'monster_flinch': 'heavy', 'charge_speed': 'fast',
       'charge_speed_up_bonus': True, 'hero_crit_bonus_girl': 40, 'hero_crit_bonus_others': 40}


def body_of(path):
    raw = pathlib.Path(path).read_bytes()
    return raw[sg.split_header(raw):]


class Pure(unittest.TestCase):
    def test_all_default_is_empty(self):
        self.assertEqual(sg.plan({}), [])
        self.assertEqual(sg.plan({'weapon_recharge': 'vanilla', 'monster_flinch': 'vanilla', 'charge_speed': 'vanilla'}), [])

    def test_rejects_unknown_and_bad(self):
        with self.assertRaises(ValueError):
            sg.plan({'nope': 1})
        with self.assertRaises(ValueError):
            sg.plan({'charge_frames_per_stage': 0})
        with self.assertRaises(ValueError):
            sg.plan({'hero_crit_bonus_girl': 500})

    def test_free_area_fits(self):
        recs = sg.plan(ALL)
        top = max(r.off + len(r.after) for r in recs if r.opt != 'x' and r.off >= sg.FREE_BASE)
        self.assertLessEqual(top, sg.FREE_BASE + sg.FREE_LEN)

    def test_assembler_branches_in_range(self):
        for mon in ('vanilla', 'strong', 'heavy', 'never'):
            for hero in ('vanilla', 'light', 'never'):
                a = sg.Asm(sg.FREE_BASE)
                sg.flinch_routine(a, mon, hero, 0x4F1D, 0x4EC1)
                a.done()

    def test_new_options_plan_and_validate(self):
        for opts in ({'hero_flinch': 'light'}, {'crit_multiplier': 3}, {'walk_speed': 4}, {'weapon_level_rate': 2},
                     {'charge_bonus_item_id': 77}, {'monster_flinch': 'strong', 'hero_flinch': 'never'}):
            self.assertTrue(sg.plan(opts), opts)
        for bad in ({'crit_multiplier': 5}, {'walk_speed': 1}, {'weapon_level_rate': 0}, {'hero_flinch': 'x'},
                    {'charge_bonus_item_id': 300}):
            with self.assertRaises(ValueError):
                sg.plan(bad)
        self.assertEqual(sg.plan({'crit_multiplier': 2, 'walk_speed': 2, 'weapon_level_rate': 1, 'hero_flinch': 'vanilla'}), [])

    def test_menu_matches_catalogue(self):
        m = sg.menu_json()
        self.assertEqual({o['key'] for o in m['options']}, set(sg.MENU_META))
        self.assertEqual(set(sg.MENU_META), set(sg.CATALOGUE) - {'fix_checksum'})
        for o in m['options']:
            self.assertIn(o['type'], ('enum', 'bool', 'num'))
            if o['type'] == 'enum':
                self.assertIn(o['default'], [v['v'] for v in o['values']])
        self.assertEqual([p['id'] for p in m['presets']], ['vanilla', 'quick-combat', 'arcade', 'qol'])
        for p in m['presets']:
            sg.plan(p['set'])

    def test_mark_detector(self):
        body = bytearray(0x200000)
        self.assertEqual(sg.scan_mark_use(bytes(body)), [])
        body[0x1000:0x1003] = b'\xbd\x6b\x01'
        body[0x2000:0x2004] = b'\xaf\x6b\xe1\x7e'
        self.assertEqual(sg.scan_mark_use(bytes(body)), [0x1000, 0x2000])
        body[0x3000:0x3003] = b'\x00\x6b\x01'          # not an operand-carrying opcode
        self.assertEqual(sg.scan_mark_use(bytes(body)), [0x1000, 0x2000])


QOL_ALL = {'qol_chest_fast': True, 'qol_unready_damage': True, 'qol_fast_transitions': True, 'qol_fast_fades': True, 'qol_item_limit': 7,
           'qol_levelup_mp_restore': True, 'qol_dead_exp': True, 'qol_magic_level_rate': 3, 'qol_scroll_deadzone': 'tighter',
           'qol_ring_speed': 2, 'qol_message_speed': 4, 'qol_shop_fill': True}


class QolPure(unittest.TestCase):
    def test_defaults_are_empty(self):
        d = {k: v['default'] for k, v in sg.CATALOGUE.items() if k.startswith('qol_')}
        self.assertEqual(len(d), 12)
        self.assertEqual(sg.plan(d), [])

    def test_each_option_plans_alone(self):
        for k, v in QOL_ALL.items():
            self.assertTrue(sg.plan({k: v}), k)

    def test_rejects_bad_values(self):
        for bad in ({'qol_item_limit': 9}, {'qol_magic_level_rate': 4}, {'qol_scroll_deadzone': 'x'}, {'qol_chest_fast': 'yes'}, {'qol_shop_fill': 1},
                    {'qol_ring_speed': 3}, {'qol_message_speed': 3}):
            with self.assertRaises(ValueError):
                sg.plan(bad)

    def test_free_area_with_everything(self):
        recs = sg.plan(dict(ALL, **QOL_ALL))
        top = max(r.off + len(r.after) for r in recs if r.off >= sg.FREE_BASE)
        self.assertLessEqual(top, sg.FREE_BASE + sg.FREE_LEN)
        offs = sorted((r.off, r.off + len(r.after)) for r in recs)
        for (a0, a1), (b0, b1) in zip(offs, offs[1:]):
            self.assertLessEqual(a1, b0, 'overlapping records')

    def test_menu_group_and_compat(self):
        m = sg.menu_json()
        self.assertEqual(m['groups'][:3], ['Weapon', 'Enemies', 'Heroes'])
        self.assertIn('Quality of life', m['groups'])
        keys = [o['key'] for o in m['options']]
        self.assertEqual(keys[:13], list(sg.MENU_META)[:13])
        q = [o for o in m['options'] if o['group'] == 'Quality of life']
        self.assertEqual(len(q), 12)
        for o in q:
            self.assertEqual(o['advanced'], False)
            self.assertTrue(o['help'])
            if o['type'] == 'bool':
                self.assertEqual(o['default'], 'no')
        by = {o['key']: o for o in q}
        self.assertEqual(by['qol_fast_fades']['conflicts'], {'qol_fast_transitions': 'redundant'})
        self.assertIn('fasterMagicLevel', by['qol_magic_level_rate']['randomizer_conflicts'])
        self.assertIn('moreItems', by['qol_item_limit']['randomizer_already'])
        self.assertIn('qol_item_limit', m['apply']['integer_enums'])
        self.assertEqual(next(p for p in m['presets'] if p['id'] == 'qol')['set']['qol_item_limit'], 7)

    def test_committed_menu_is_current(self):
        committed = json.loads((sg.HERE / 'gameplay_menu.json').read_text())
        self.assertEqual(committed, json.loads(json.dumps(sg.menu_json())))

    def test_levelup_routine_branches(self):
        for mp in (False, True):
            a = sg.Asm(sg.FREE_BASE)
            sg.levelup_routine(a, mp)
            a.done()
        a = sg.Asm(sg.FREE_BASE)
        sg.deadguard_routine(a)
        blob = a.done()
        self.assertEqual(blob[-2:], b'\x68\x6b')
        self.assertEqual(blob[blob.index(b'\x10') + 1], 7)      # BPL over REP/STZ/SEP

    def test_dead_exp_is_independent_of_the_level_up_hook(self):
        recs = sg.plan({'qol_dead_exp': True})
        self.assertEqual({r.off for r in recs if r.off < 0x10000}, {0x446B, 0x44DB})
        both = sg.plan({'qol_dead_exp': True, 'qol_levelup_mp_restore': True})
        self.assertEqual({r.off for r in both if r.off < 0x10000}, {0x446B, 0x44DB, 0x44D3})

    def test_ring_and_message_sites_are_relative_to_their_signatures(self):
        recs = sg.plan({'qol_ring_speed': 2, 'qol_message_speed': 2})
        self.assertEqual(len([r for r in recs if r.opt == 'qol_ring_speed']), 7)
        msg = next(r for r in recs if r.opt == 'qol_message_speed')
        self.assertEqual((msg.before, msg.after), (b'\x60', b'\x30'))


@unittest.skipUnless(ROM and pathlib.Path(ROM).exists(), 'SOM_ROM not set')
class WithRom(unittest.TestCase):
    def setUp(self):
        self.tmp = pathlib.Path(tempfile.mkdtemp(prefix='gp_test_'))
        self.opt = self.tmp / 'o.json'

    def run_apply(self, rom, opts, **kw):
        self.opt.write_text(json.dumps({'options': opts}))
        body = bytearray(body_of(rom))
        recs = sg.plan(opts, bytes(body))
        applied = sg.apply_recs(body, recs, kw.get('on_conflict', 'error'))
        return body, recs, applied

    def test_identity_when_off(self):
        body, recs, _ = self.run_apply(ROM, {})
        self.assertEqual(recs, [])
        self.assertEqual(bytes(body), body_of(ROM))

    def test_all_sites_match_vanilla(self):
        body, recs, (applied, conflicts, bad) = self.run_apply(ROM, ALL)
        self.assertEqual(conflicts, [])
        self.assertTrue(all(st == 'ok' for _, st in applied))

    def test_idempotent(self):
        body, recs, _ = self.run_apply(ROM, ALL)
        again = sg.apply_recs(body, recs)
        self.assertTrue(all(st == 'already' for _, st in again[0]))

    def test_conflict_with_randomizer_no_energy_to_run(self):
        body = bytearray(body_of(ROM))
        body[0xB78A:0xB78D] = b'\xEA\xEA\xEA'
        recs = sg.plan({'run_recharge': 'none', 'weapon_recharge': 'instant'}, bytes(body))
        with self.assertRaises(sg.Conflict):
            sg.apply_recs(bytearray(body), recs, 'error')
        applied, conflicts, bad = sg.apply_recs(body, recs, 'skip')
        self.assertEqual(bad, ['run_recharge'])
        self.assertTrue(any(r.opt == 'weapon_recharge' for r, _ in applied))

    def test_conflict_with_attacks_forever(self):
        body = bytearray(body_of(ROM))
        body[0xF944] = 0x9E
        recs = sg.plan({'weapon_recharge': 'fast'}, bytes(body))
        _, conflicts, bad = sg.apply_recs(body, recs, 'skip')
        self.assertEqual(bad, ['weapon_recharge'])

    def test_vanilla_has_no_foreign_mark_use(self):
        self.assertEqual(sg.scan_mark_use(body_of(ROM)), [])

    def test_round2_options_match_vanilla(self):
        opts = dict(ALL, hero_flinch='light', crit_multiplier=3, walk_speed=3, weapon_level_rate=2, charge_bonus_item_id=77)
        body, recs, (applied, conflicts, bad) = self.run_apply(ROM, opts)
        self.assertEqual(conflicts, [])

    def test_checksum_valid_after_patch(self):
        body, recs, _ = self.run_apply(ROM, ALL)
        sg.set_checksum(body)
        self.assertTrue(sg.checksum_ok(bytes(body)))


@unittest.skipUnless(ROM and pathlib.Path(ROM).exists(), 'SOM_ROM not set')
class QolWithRom(unittest.TestCase):
    def test_shop_fill_site_and_fail_target(self):
        body = body_of(ROM)
        r = sg.resolve(body)
        self.assertEqual((r['shop'], r['shop_fail']), (0x7AF2, 0x7B12))
        recs = [x for x in sg.plan({'qol_shop_fill': True}, body) if x.off == 0x7AF2]
        self.assertEqual(len(recs), 1)
        self.assertEqual(recs[0].before, bytes.fromhex('D01E200D7D'))
        self.assertEqual(recs[0].after[:1] + recs[0].after[4:], b'\x22\xEA')

    def test_randomizer_hooks_are_recognised_by_their_routine(self):
        body = bytearray(body_of(ROM))
        # a copy of the randomizer routines in the free area, hooks pointing at them
        mp = sg.FREE_BASE
        body[mp:mp + len(sg.RANDO_MP_ROUTINE)] = sg.RANDO_MP_ROUTINE
        mg = sg.FREE_BASE + 0x40
        body[mg:mg + len(sg.RANDO_MAGIC_ROUTINE)] = sg.RANDO_MAGIC_ROUTINE
        body[0x44D3:0x44DB] = b'\x22' + sg.snes_long(mp) + b'\xEA\xEA\xE2\x20'
        body[0x44DB:0x44E1] = bytes.fromhex('A580094085 80'.replace(' ', ''))
        body[0x104DEC:0x104DF3] = b'\x22' + sg.snes_long(mg) + b'\xEA\xEA\xEA'
        r = sg.resolve(bytes(body))
        self.assertTrue(r['rando_mp'])
        self.assertTrue(r['rando_magic'])
        recs = sg.plan({'qol_levelup_mp_restore': True, 'qol_magic_level_rate': 3}, bytes(body))
        self.assertEqual(recs, [])
        body[mp] ^= 0xFF                                         # another routine behind the hook: not recognised
        self.assertFalse(sg.resolve(bytes(body))['rando_mp'])

    def test_sites_match_vanilla_and_idempotent(self):
        body = bytearray(body_of(ROM))
        recs = sg.plan(QOL_ALL, bytes(body))
        applied, conflicts, bad = sg.apply_recs(body, recs)
        self.assertEqual(conflicts, [])
        self.assertTrue(all(st == 'ok' for _, st in applied))
        again, c2, _ = sg.apply_recs(body, recs)
        self.assertTrue(all(st == 'already' for _, st in again))

    def test_only_planned_bytes_change(self):
        orig = body_of(ROM)
        body = bytearray(orig)
        recs = sg.plan(QOL_ALL, orig)
        sg.apply_recs(body, recs)
        covered = set()
        for r in recs:
            covered.update(range(r.off, r.off + len(r.after)))
        diff = [i for i in range(len(orig)) if orig[i] != body[i]]
        self.assertTrue(diff)
        self.assertTrue(all(i in covered for i in diff))

    def test_randomizer_style_conflict_skips_only_that_option(self):
        body = bytearray(body_of(ROM))
        recs0 = sg.plan({'qol_levelup_mp_restore': True}, bytes(body))
        hook = next(r for r in recs0 if r.off < 0x10000)
        body[hook.off:hook.off + 4] = b'\x22\x00\x80\xE0'          # the randomizer mpAtLevel hook
        recs = sg.plan({'qol_levelup_mp_restore': True, 'qol_item_limit': 7}, bytes(body))
        applied, conflicts, bad = sg.apply_recs(body, recs, 'skip')
        self.assertEqual(bad, ['qol_levelup_mp_restore'])
        self.assertTrue(any(r.opt == 'qol_item_limit' for r, _ in applied))

    def test_item_limit_already_applied_is_not_a_conflict(self):
        body = bytearray(body_of(ROM))
        body[0x6559:0x655B] = b'\xEA\xEA'
        applied, conflicts, bad = sg.apply_recs(body, sg.plan({'qol_item_limit': 7}, bytes(body)))
        self.assertEqual(conflicts, [])
        self.assertEqual([st for _, st in applied], ['already'])


@unittest.skipUnless(MAGNO and pathlib.Path(MAGNO).exists(), 'SOM_ROM_MAGNO not set')
class WithMagno(unittest.TestCase):
    def test_magno_relocated_block(self):
        body = body_of(MAGNO)
        r = sg.resolve(body)
        self.assertEqual(r['flinch'], 0x4ED3)
        recs = sg.plan(ALL, body)
        _, conflicts, bad = sg.apply_recs(bytearray(body), recs, 'skip')
        self.assertEqual(conflicts, [])
        recs = sg.plan(QOL_ALL, body)
        _, conflicts, bad = sg.apply_recs(bytearray(body), recs, 'skip')
        self.assertEqual(bad, ['qol_message_speed'])       # the translation rewrote the message builder: reported, not patched


if __name__ == '__main__':
    unittest.main()
