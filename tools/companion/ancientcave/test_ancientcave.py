#!/usr/bin/env python3
"""Companion macro and status tests on the generated Ancient Cave ROM (run after build_ac_fixture.py).

    python3 tools/companion/ancientcave/test_ancientcave.py

Same mutation style as tools/companion/test_companion.py but with Ancient Cave expectations (qty-1 native inventory,
all eight weapon types owned, map ids numbered from 0).  Equip is tested with the repository host (it exposes two
latent defects: ring order and a cursor relative to the held weapon) and with patched_host.py.
Evidence JSON goes to $SOM_COMPANION_STATE/evidence/ancientcave/ and a summary to macro_results.json.
"""
import json
import pathlib
import sys
import unittest

import acprobe as ac

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
import common  # noqa: E402
import patched_host  # noqa: E402

EVID = ac.EVID
OBS = {}


def run(body, name, patched=False, opts=()):
    script = 'LOAD %s\nRUN 2\n%s\n' % (ac.PARTY, body)
    if patched:
        return patched_host.run(script, ac.ROM, name)
    return common.run_host(script, rom=ac.ROM, opts=opts, name='ac_' + name)


def states(out):
    return {d['tag']: d for d in out if 'tag' in d}


def macros(out):
    return [d for d in out if d.get('event') in ('macro', 'refused')]


class AncientCaveTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not ac.PARTY.exists():
            raise unittest.SkipTest('run build_ac_fixture.py first')

    def save(self, name, out):
        (EVID / (name + '.jsonl')).write_text('\n'.join(json.dumps(d) for d in out) + '\n')

    def test_01_status_reads(self):
        out = run('STATE t0\nWRITE E1F1 0700\nWRITE E3F5 02\nRUN 40\nSTATE t1\nWRITE E191 20\nWRITE E1B6 14\nRUN 120\nSTATE t2\n', 'status')
        s = states(out)
        self.save('status', out)
        self.assertEqual(s['t0']['mode'], 'field')
        self.assertTrue(s['t0']['safe'])
        self.assertEqual(s['t0']['map'], 0)                        # generated floor 1 outside map is id 0
        self.assertEqual([h['present'] for h in s['t0']['party']], [1, 1, 1])
        self.assertEqual([h['hp'] for h in s['t0']['party']], [50, 45, 40])
        self.assertEqual(s['t1']['party'][0]['hp'], 43)
        self.assertEqual(s['t1']['party'][1]['mp'], s['t0']['party'][1]['mp'] - 2)
        self.assertLess(s['t2']['party'][0]['hp'], 43)
        self.assertLess(s['t2']['party'][0]['poison_timer'], 0x14)
        OBS['status'] = 'mode field, map 0, party [50,45,40], pending damage 7 and MP 2 applied, poison ticks'

    def test_02_use_item_candy_record_compacts(self):
        out = run('WRITE E182 1400\nRUN 2\nSTATE before\nCMD item 0 0\nWAIT 900\nRUN 150\nSTATE after\n', 'use_item_candy')
        s, m = states(out), macros(out)
        self.save('use_item_candy', out)
        self.assertEqual(m[0]['result'], 'done', m)
        self.assertEqual(s['before']['items'][0], [0, 1])
        self.assertEqual(s['after']['items'][0], [1, 1])           # the qty-1 record is removed and the list shifts up
        self.assertEqual(s['after']['items'][7], [0, 0])
        self.assertEqual(s['after']['party'][0]['hp'], 50)
        self.assertFalse(s['after']['ring']['open'])
        OBS['use_item_candy'] = 'done in %d frames; HP 20 to 50; record list shifts up' % m[0]['frames']

    def test_03_use_item_royal_jam(self):
        out = run('WRITE E182 1400\nRUN 2\nSTATE before\nCMD item 0 2\nWAIT 900\nRUN 150\nSTATE after\n', 'use_item_jam')
        s, m = states(out), macros(out)
        self.save('use_item_jam', out)
        self.assertEqual(m[0]['result'], 'done', m)
        self.assertEqual(s['before']['items'][2], [2, 1])
        self.assertEqual(s['after']['items'][2], [3, 1])           # Faerie Walnut slid into record 2
        self.assertEqual(s['after']['party'][0]['hp'], 50)
        OBS['use_item_jam'] = 'done in %d frames; HP 20 to 50' % m[0]['frames']

    def test_03b_unusable_items_time_out(self):
        out = run('STATE before\nCMD item 0 5\nWAIT 1500\nRUN 60\nSTATE after\n', 'use_item_cup')
        s, m = states(out), macros(out)
        self.save('use_item_cup', out)
        self.assertEqual(m[0]['result'], 'fail-timeout', m)
        self.assertEqual(s['after']['items'][5], [5, 1])
        self.assertFalse(s['after']['ring']['open'])
        OBS['use_item_cup'] = 'Cup of Wishes on a healthy hero: fail-timeout, ring closed by recovery, item kept'

    def test_03c_repurposed_items(self):
        # ids 7 and 6 are "Random Music" and "Stuck?" in this ROM (titles read in the ring)
        out = run('STATE before\nCMD item 0 6\nWAIT 1500\nRUN 100\nSTATE after\n', 'use_item_music')
        m = macros(out)
        self.save('use_item_music', out)
        self.assertEqual(m[0]['result'], 'done-unverified', m)
        out2 = run('STATE before\nCMD item 0 7\nWAIT 1500\nRUN 300\nSTATE after\n', 'use_item_stuck')
        m2, s2 = macros(out2), states(out2)
        self.save('use_item_stuck', out2)
        self.assertEqual(m2[0]['result'], 'fail-unsafe', m2)        # the item starts a fade (transition) while the macro verifies
        self.assertEqual(m2[0]['note'], 'transition')
        self.assertEqual(s2['after']['items'][7], [6, 1])           # not consumed
        OBS['use_item_repurposed'] = 'Random Music: done-unverified (no WRAM effect); Stuck?: fail-unsafe "transition" (warps, item kept)'

    def test_04_equip_stock_host_wrong_weapon(self):
        out = run('STATE before\nCMD equip 0 2\nWAIT 900\nRUN 30\nSTATE after\n', 'equip_stock')
        s, m = states(out), macros(out)
        self.save('equip_stock', out)
        self.assertEqual(m[0]['result'], 'done-unverified', m)
        self.assertEqual(s['after']['party'][0]['weapon_type'], 3)   # asked for the axe (2), got the spear: ring order is 1..7, 0
        OBS['equip_stock'] = 'repository host: equip type 2 equips type 3 (done-unverified); latent ring-order defect, not an AC hack difference'

    def test_04b_equip_matrix(self):
        # 8 held weapons x 8 wanted weapons (equip_matrix.py, about 3 minutes): stock host vs patched host
        path = EVID / 'equip_matrix.json'
        if not path.exists():
            self.skipTest('run equip_matrix.py first')
        mx = json.loads(path.read_text())
        self.assertEqual(mx['patched_correct'], 64, 'patched host must equip every pair correctly')
        self.assertLess(mx['stock_correct'], 64)
        OBS['equip_matrix'] = 'stock host %d of 64 pairs correct, patched host %d of 64' % (mx['stock_correct'], mx['patched_correct'])

    def test_04c_equip_held_weapon(self):
        out = run('STATE before\nCMD equip 0 1\nWAIT 900\nRUN 30\nSTATE after\n', 'equip_same_patched', patched=True)
        m = macros(out)
        self.assertEqual(m[0]['result'], 'done', m)
        out = run('STATE before\nCMD equip 0 1\nWAIT 900\nRUN 30\nSTATE after\n', 'equip_same_stock')
        s, m = states(out), macros(out)
        self.assertEqual(m[0]['result'], 'done-unverified', m)
        self.assertEqual(s['after']['party'][0]['weapon_type'], 2)   # ring rank 1 is the axe, not the sword
        OBS['equip_current'] = 'asking for the held sword: stock host equips the axe (done-unverified, %d frames); patched host returns done at once' % m[0]['frames']

    def test_05_cast_magic_owner_switch(self):
        out = run('STATE before\nCMD cast 1 0 0\nWAIT 1500\nSTATE closed\nRUN 240\nSTATE after\n', 'cast')
        s, m = states(out), macros(out)
        self.save('cast', out)
        self.assertEqual(m[0]['result'], 'done', m)
        self.assertEqual(s['before']['party'][1]['mp'], 8)
        self.assertEqual(s['after']['party'][1]['mp'], 4)
        self.assertFalse(s['closed']['ring']['open'])
        OBS['cast_girl'] = 'girl byte 0 slot 0: MP 8 to 4 in %d frames' % m[0]['frames']

    def test_05b_cast_magic_sprite(self):
        out = run('STATE before\nCMD cast 2 5 2\nWAIT 1500\nRUN 240\nSTATE after\n', 'cast_sprite')
        s, m = states(out), macros(out)
        self.save('cast_sprite', out)
        self.assertEqual(m[0]['result'], 'done', m)
        self.assertLess(s['after']['party'][2]['mp'], s['before']['party'][2]['mp'])
        OBS['cast_sprite'] = 'sprite byte 5 slot 2: MP 8 to %d in %d frames' % (s['after']['party'][2]['mp'], m[0]['frames'])

    def test_06_refused_while_event_runs(self):
        out = run('EVENT 600000\nRUN 5\nSTATE ev\nCMD item 0 0\nWAIT 10\n', 'unsafe_event')
        m = macros(out)
        self.save('unsafe_event', out)
        self.assertEqual((m[0]['event'], m[0]['result']), ('refused', 'fail-unsafe'))

    def test_07_refused_without_game(self):
        out = common.run_host('RUN 200\nSTATE boot\nCMD item 0 0\nWAIT 10\n', rom=ac.ROM, name='ac_unsafe_boot')
        self.save('unsafe_boot', out)
        self.assertEqual(states(out)['boot']['mode'], 'no-game')
        self.assertEqual(macros(out)[0]['result'], 'fail-unsafe')

    def test_08_bad_arguments_refused(self):
        out = run('CMD item 0 9\nWAIT 5\nCMD cast 1 6 0\nWAIT 5\nCMD cast 2 7 0\nWAIT 5\nCMD cast 0 0 0\nWAIT 5\n', 'badargs')
        self.save('badargs', out)
        self.assertEqual([m['result'] for m in macros(out) if m['event'] == 'refused'], ['fail-bad-argument'] * 4)
        # on this ROM every weapon type is owned, so "equip 0 4" is not refused
        out = run('CMD equip 0 4\nWAIT 5\n', 'badargs_equip')
        self.assertFalse([m for m in macros(out) if m['event'] == 'refused'])

    def test_09_abort_closes_ring(self):
        out = run('CMD cast 1 0 0\nRUN 150\nSTATE mid\nABORT\nWAIT 600\nRUN 60\nSTATE after\n', 'abort')
        s, m = states(out), macros(out)
        self.save('abort', out)
        self.assertTrue(s['mid']['ring']['open'])
        self.assertEqual(m[0]['result'], 'aborted')
        self.assertFalse(s['after']['ring']['open'])
        self.assertEqual(s['after']['mode'], 'field')
        self.assertEqual(s['after']['party'][1]['mp'], 8)

    def test_10_event_during_macro_recovers(self):
        out = run('CMD item 0 0\nRUN 80\nSTATE mid\nEVENT 1820\nWAIT 900\nRUN 200\nSTATE after\n', 'event_midway')
        s, m = states(out), macros(out)
        self.save('event_midway', out)
        self.assertTrue(s['mid']['ring']['open'])
        self.assertTrue(m[0]['result'].startswith('fail'), m)

    def test_11_hud_rows(self):
        out = run('TOP %s/top.ppm\nSHOT %s/full.ppm\n' % (EVID, EVID), 'top')
        top = (EVID / 'top.ppm').read_bytes().split(b'\n', 3)
        self.assertEqual(top[1], b'256 199')
        run('SHOT %s/full_bg3_off.ppm\n' % EVID, 'top_bg3', opts=['snes9x_layer_3=disabled'])
        a, b = (EVID / 'full.ppm').read_bytes(), (EVID / 'full_bg3_off.ppm').read_bytes()
        hdr, row = len(b'P6\n256 224\n255\n'), 256 * 3
        rows = [y for y in range(224) if a[hdr + y * row: hdr + (y + 1) * row] != b[hdr + y * row: hdr + (y + 1) * row]]
        OBS['hud_bg3_rows'] = [rows[0], rows[-1]] if rows else []
        self.assertTrue(rows)


class Collect(unittest.TextTestResult):
    outcomes = {}

    def addSuccess(self, test):
        super().addSuccess(test)
        self.outcomes[test.id().split('.')[-1]] = 'pass'

    def addFailure(self, test, err):
        super().addFailure(test, err)
        self.outcomes[test.id().split('.')[-1]] = 'FAIL: ' + str(err[1])[:300]

    def addError(self, test, err):
        super().addError(test, err)
        self.outcomes[test.id().split('.')[-1]] = 'ERROR: ' + str(err[1])[:300]


if __name__ == '__main__':
    suite = unittest.defaultTestLoader.loadTestsFromTestCase(AncientCaveTests)
    res = unittest.TextTestRunner(verbosity=2, resultclass=Collect).run(suite)
    (EVID / 'macro_results.json').write_text(json.dumps(dict(outcomes=Collect.outcomes, observations=OBS), indent=1))
    sys.exit(0 if res.wasSuccessful() else 1)
