#!/usr/bin/env python3
"""End-to-end tests of som_companion through the libretro host (mutation-style: every check changes game state
through real button presses and reads the result back from WRAM).

    python3 tools/companion/test_companion.py            # fixtures are built on first use (about 1 minute)
Evidence (JSON lines and PPM screenshots) goes to $SOM_COMPANION_STATE/evidence.
"""
import json
import pathlib
import sys
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import common  # noqa: E402
import build_fixture  # noqa: E402

import os
TAG = os.environ.get('SOM_TEST_ROM', 'vanilla')           # vanilla | magno
ROM = common.ROM_MAGNO if TAG == 'magno' else common.ROM_VANILLA
EXPECT_MAP = 128 if TAG == 'magno' else 131               # the Magno run stays in the first map (route desyncs there)
EVID = common.STATE / 'evidence' / TAG
PARTY = common.WORK / ('party3_%s.state' % TAG)
BASE = common.WORK / ('base_%s.state' % TAG)


def need_fixtures():
    if not (PARTY.exists() and BASE.exists()):
        sys.argv = ['build_fixture', '--rom', str(ROM), '--tag', TAG]
        if TAG == 'magno':
            sys.argv += ['--route-frames', '20000']
        build_fixture.main()
    EVID.mkdir(parents=True, exist_ok=True)


def run(body, name, rom=None, opts=(), state=None):
    rom = rom or ROM
    state = state or PARTY
    return common.run_host('LOAD %s\nRUN 2\n%s\n' % (state, body), rom=rom, opts=opts, name=name)


def states(out):
    return {d['tag']: d for d in out if 'tag' in d}


def macros(out):
    return [d for d in out if d.get('event') in ('macro', 'refused')]


class CompanionTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        need_fixtures()

    def save(self, name, out):
        (EVID / (name + '.jsonl')).write_text('\n'.join(json.dumps(d) for d in out) + '\n')

    # ---- live status reading ----
    def test_01_status_reads_follow_game_state(self):
        out = run('STATE t0\nWRITE E1F1 0700\nWRITE E3F5 02\nRUN 40\nSTATE t1\n'
                  'WRITE E191 20\nWRITE E1B6 14\nRUN 120\nSTATE t2\n', 'status_' + TAG)
        s = states(out)
        self.save('status', out)
        self.assertEqual(s['t0']['mode'], 'field')
        self.assertTrue(s['t0']['safe'])
        self.assertEqual(s['t0']['map'], EXPECT_MAP)
        self.assertEqual([h['present'] for h in s['t0']['party']], [1, 1, 1])
        self.assertEqual(s['t0']['party'][0]['hp'], 50)
        self.assertEqual(s['t1']['party'][0]['hp'], 43)           # pending damage 7 applied by the game
        self.assertEqual(s['t1']['party'][1]['mp'], s['t0']['party'][1]['mp'] - 2)   # pending MP damage 2 (girl)
        self.assertLess(s['t2']['party'][0]['hp'], 43)             # status word 0x2000: 1 HP per timer drop
        self.assertLess(s['t2']['party'][0]['poison_timer'], 0x14)

    # ---- macros ----
    def test_02_use_item_macro(self):
        out = run('WRITE E182 1400\nRUN 2\nSTATE before\nCMD item 0 0\nWAIT 900\nRUN 150\nSTATE after\n'
                  'TOP %s/use_item_top.ppm\nSHOT %s/use_item_full.ppm\n' % (EVID, EVID), 'use_item_' + TAG)
        s, m = states(out), macros(out)
        self.save('use_item', out)
        self.assertEqual(m[0]['result'], 'done', m)
        self.assertEqual(s['before']['items'][0], [0, 3])
        self.assertEqual(s['after']['items'][0], [0, 2])          # Candy x3 -> x2
        self.assertEqual(s['before']['party'][0]['hp'], 20)
        self.assertGreater(s['after']['party'][0]['hp'], 20)      # candy healed the hero
        self.assertEqual(s['after']['mode'], 'field')
        self.assertFalse(s['after']['ring']['open'])

    def test_03_use_item_fifth_record(self):
        out = run('WRITE E182 1400\nRUN 2\nSTATE before\nCMD item 0 4\nWAIT 900\nRUN 150\nSTATE after\n', 'use_item2_' + TAG)
        s, m = states(out), macros(out)
        self.save('use_item2', out)
        self.assertEqual(m[0]['result'], 'done', m)
        self.assertEqual(s['before']['items'][4], [2, 2])         # Royal Jam x2 (ring index 4)
        self.assertEqual(s['after']['items'][4], [2, 1])
        self.assertEqual(s['after']['items'][0], [0, 3])          # others untouched
        self.assertEqual(s['after']['party'][0]['hp'], 50)        # Royal Jam restores all HP

    def test_03b_unusable_item_times_out_and_closes_ring(self):
        # Cup of Wishes on a healthy hero: the game keeps the target selection open; the macro must give up and close it
        out = run('STATE before\nCMD item 0 2\nWAIT 1500\nRUN 60\nSTATE after\n', 'use_item_unusable_' + TAG)
        s, m = states(out), macros(out)
        self.save('use_item_unusable', out)
        self.assertEqual(m[0]['result'], 'fail-timeout', m)
        self.assertEqual(s['after']['items'][2], [5, 2])
        self.assertFalse(s['after']['ring']['open'])
        self.assertEqual(s['after']['mode'], 'field')

    def test_04_equip_weapon_macro(self):
        out = run('STATE before\nCMD equip 0 2\nWAIT 900\nRUN 30\nSTATE after\n', 'equip_' + TAG)
        s, m = states(out), macros(out)
        self.save('equip', out)
        self.assertEqual(m[0]['result'], 'done', m)
        self.assertEqual(s['before']['party'][0]['weapon_type'], 1)
        self.assertEqual(s['after']['party'][0]['weapon_type'], 2)   # axe
        self.assertEqual(s['after']['party'][0]['weapon_row'], 20)   # 2 * 9 + tier 2
        self.assertEqual(s['after']['weapons'][1], 0)                # sword no longer marked as held
        self.assertEqual(s['after']['weapons'][2], 0x42)

    def test_04b_equip_matrix_every_owned_pair(self):
        # every (held, wanted) pair of the owned weapon types for the boy: the ring is relative to the held weapon
        import equip_matrix
        r = equip_matrix.run_matrix(ROM, PARTY, 0, 'eqm_' + TAG)
        (EVID / 'equip_matrix.json').write_text(json.dumps(r, indent=0))
        self.assertEqual(r['unreachable_held'], [])
        self.assertEqual(r['correct'], r['total'], r['pairs'])

    def test_05_cast_magic_macro_with_owner_switch(self):
        out = run('STATE before\nCMD cast 1 0 0\nWAIT 1500\nSTATE closed\nRUN 240\nSTATE after\n'
                  'SHOT %s/cast_full.ppm\n' % EVID, 'cast_' + TAG)
        s, m = states(out), macros(out)
        self.save('cast', out)
        self.assertEqual(m[0]['result'], 'done', m)
        self.assertEqual(s['before']['party'][1]['mp'], 8)
        self.assertEqual(s['after']['party'][1]['mp'], 4)             # girl paid 4 MP (Undine, first spell)
        self.assertEqual(s['after']['party'][0]['mp'], 0)
        self.assertFalse(s['closed']['ring']['open'])

    # ---- safety / recovery ----
    def test_05b_cast_magic_sprite(self):
        out = run('STATE before\nCMD cast 2 5 2\nWAIT 1500\nRUN 240\nSTATE after\n', 'cast_sprite_' + TAG)
        s, m = states(out), macros(out)
        self.save('cast_sprite', out)
        self.assertEqual(m[0]['result'], 'done', m)
        self.assertLess(s['after']['party'][2]['mp'], s['before']['party'][2]['mp'])

    def test_06_refused_while_event_runs(self):
        out = run('EVENT 600000\nRUN 5\nSTATE ev\nCMD item 0 0\nWAIT 10\n', 'unsafe_event_' + TAG)
        m = macros(out)
        self.save('unsafe_event', out)
        self.assertEqual(m[0]['event'], 'refused')
        self.assertEqual(m[0]['result'], 'fail-unsafe')

    def test_07_refused_without_game(self):
        out = common.run_host('RUN 200\nSTATE boot\nCMD item 0 0\nWAIT 10\n', rom=ROM, name='unsafe_boot_' + TAG)
        self.save('unsafe_boot', out)
        self.assertEqual(states(out)['boot']['mode'], 'no-game')
        self.assertEqual(macros(out)[0]['result'], 'fail-unsafe')

    def test_08_bad_arguments_refused(self):
        out = run('CMD equip 0 4\nWAIT 5\nCMD item 0 9\nWAIT 5\nCMD cast 1 6 0\nWAIT 5\nCMD cast 2 7 0\nWAIT 5\nCMD cast 0 0 0\nWAIT 5\n', 'badargs_' + TAG)
        self.save('badargs', out)
        self.assertEqual([m['result'] for m in macros(out) if m['event'] == 'refused'], ['fail-bad-argument'] * 5)

    def test_09_abort_closes_the_ring(self):
        out = run('CMD cast 1 0 0\nRUN 150\nSTATE mid\nABORT\nWAIT 600\nRUN 60\nSTATE after\n', 'abort_' + TAG)
        s, m = states(out), macros(out)
        self.save('abort', out)
        self.assertTrue(s['mid']['ring']['open'])
        self.assertEqual(m[0]['result'], 'aborted')
        self.assertFalse(s['after']['ring']['open'])
        self.assertEqual(s['after']['mode'], 'field')
        self.assertEqual(s['after']['party'][1]['mp'], 8)             # nothing was cast

    def test_10_event_during_macro_recovers(self):
        out = run('CMD item 0 0\nRUN 80\nSTATE mid\nEVENT 1820\nWAIT 900\nRUN 200\nSTATE after\n', 'event_midway_' + TAG)
        s, m = states(out), macros(out)
        self.save('event_midway', out)
        self.assertTrue(s['mid']['ring']['open'])
        self.assertTrue(m[0]['result'].startswith('fail'), m)

    # ---- top screen ----
    def test_11_top_screen_crop_excludes_hud(self):
        out = run('TOP %s/top.ppm\nSHOT %s/full.ppm\n' % (EVID, EVID), 'top_' + TAG)
        top = (EVID / 'top.ppm').read_bytes().split(b'\n', 3)
        self.assertEqual(top[1], b'256 199')
        if TAG != 'vanilla':
            return        # the Magno fixture is before the HUD is enabled by the story: nothing to compare
        # HUD rows 199..216 exist in the full frame and differ when BG3 is disabled (HUD text and frames live on BG3)
        out2 = run('SHOT %s/full_bg3_off.ppm\n' % EVID, 'top_bg3_' + TAG, opts=['snes9x_layer_3=disabled'])
        a = (EVID / 'full.ppm').read_bytes()
        b = (EVID / 'full_bg3_off.ppm').read_bytes()
        hdr = len(b'P6\n256 224\n255\n')
        row = 256 * 3
        hud_a = a[hdr + 199 * row: hdr + 217 * row]
        hud_b = b[hdr + 199 * row: hdr + 217 * row]
        self.assertNotEqual(hud_a, hud_b)


if __name__ == '__main__':
    unittest.main(verbosity=2)
