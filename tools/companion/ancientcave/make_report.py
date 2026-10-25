#!/usr/bin/env python3
"""Merges the Ancient Cave evidence into the repository data files (no ROM bytes are written, only addresses,
sizes, names and measured values):

  data/companion_ancientcave_20261017.json      full report (fields, macros, hooks, static diff, detection proposal)
  data/wram_companion_map_ancientcave.json      sibling of data/wram_companion_map.json with the AC column filled

The original map is only read.  Needs both ROMs for the static diff (read from disk, never copied).
"""
import bisect
import hashlib
import json
import pathlib

import acprobe as ac

ROOT = ac.HERE.parent.parent.parent
VANILLA = pathlib.Path('/home/cidwel/projects/rommancer/roms/snes/Secret of Mana (USA).smc')
SEEDS = {'1': ac.STATE / 'ancientcave/out/ac_seed1.smc', '777': ac.STATE / 'ancientcave/out/ac_seed777.smc'}

HOOK_CLASS = {
    'menu_ring': ('request_event_menu_loop', 'project_and_publish_all_native_menu_ring_rows', 'set_native_menu_ring_center',
                  'run_native_menu_coordinator', 'service_native_menu_scroll_tick', 'execute_native_menu_page_record_commands'),
    'inventory': ('insert_or_increment_inventory_record',),
    'shop': ('shop',),
    'controller_pad': ('apply_native_button_mappings', 'transfer_controller_to_eligible_party_actor',
                       'produce_hero_signed_velocity_pose', 'advance_selected_hero_charge_timing'),
    'charge_gauge_hud': ('charge_gauge', 'prepare_ui_sprite_grid', 'prepare_signed_ui_sprite', 'select_and_emit_actor_sprite_lists',
                         'prepare_object_weapon_graphics'),
    'dialogue': ('decode_dialogue_font_stream',),
}


def classify(name):
    for k, subs in HOOK_CLASS.items():
        if any(s in name for s in subs):
            return k
    return 'combat_actor_map'


def hooks(v, a):
    fs = sorted((int(f['entry'], 16), f.get('size', 1), f['name'])
                for f in json.load(open(ROOT / 'data/functions.json'))['functions'] if f.get('address_space', 'ROM') == 'ROM')
    ent = [e[0] for e in fs]
    out = []
    for off in range(0, 0x10000 - 3):
        if a[off] == 0x22 and v[off:off + 4] != a[off:off + 4] and a[off + 3] >= 0xE0:
            i = bisect.bisect_right(ent, 0xC00000 + off) - 1
            name = fs[i][2] if i >= 0 and 0xC00000 + off < fs[i][0] + max(fs[i][1], 1) else None
            out.append(dict(site='C0:%04X' % off, replaces_bytes=4, jsl_target_bank='E0+', function=name,
                            entry='%06X' % fs[i][0] if name else None, companion_area=classify(name or '')))
    return out


def regions(v, a, gap=16):
    reg, i, n = [], 0, 0x200000
    while i < n:
        if v[i] != a[i]:
            j = i
            while j < n and (v[j] != a[j] or any(v[k] != a[k] for k in range(j, min(j + gap, n)))):
                j += 1
            reg.append((i, j))
            i = j
        else:
            i += 1
    return reg


def main():
    v = VANILLA.read_bytes()
    a = SEEDS['1'].read_bytes()
    b = SEEDS['777'].read_bytes()
    fields = json.loads((ac.STATE / 'evidence/ancientcave/fields_results.json').read_text())
    fields777 = json.loads((ac.STATE / 'evidence/ancientcave_s777/fields_results.json').read_text())
    macro = json.loads((ac.STATE / 'evidence/ancientcave/macro_results.json').read_text())
    matrix = json.loads((ac.STATE / 'evidence/ancientcave/equip_matrix.json').read_text())
    h1, h2 = hooks(v, a), hooks(v, b)
    reg = regions(v, a)
    same_sites = [x['site'] for x in h1] == [x['site'] for x in h2]
    verdicts = {}
    for k, r in fields.items():
        verdicts[r['verdict']] = verdicts.get(r['verdict'], 0) + 1
    seed_match = {k: fields[k]['verdict'] == fields777[k]['verdict'] for k in fields}

    report = {
        'title': 'Ancient Cave compatibility of the dual-screen companion (board item T5)',
        'date': '2026-10-17',
        'rom': {
            'source': 'Moppu/SecretOfManaRandomizer v1.49 (LGPL-2.1), Ancient Cave mode, via tools/companion/ancientcave_gen',
            'size_bytes': len(a), 'vanilla_size_bytes': len(v),
            'header_identical_to_vanilla': a[0xFFC0:0x10000] == v[0xFFC0:0x10000],
            'sha256': {'seed_1': hashlib.sha256(a).hexdigest(), 'seed_777': hashlib.sha256(b).hexdigest(), 'vanilla_us': hashlib.sha256(v).hexdigest()},
            'location': 'generated under $SOM_COMPANION_STATE/ancientcave/out, never in the repository',
        },
        'boot': {
            'status': 'playable state reached headless on both seeds',
            'cause_of_earlier_stall': 'the name entry ignores START when a name equals another hero name; the earlier script typed "A" for all three heroes',
            'procedure': ['frame 300 tap START (title)', '410 frames later tap B (NEW GAME)', '300 frames later: boy name: B once; START',
                          'girl: RIGHT once (letter B), B, START', 'sprite: RIGHT twice (letter C), B, START (about 400 frames after each START)',
                          'floor-1 dialogue ($D0 = 129..131): tap B every 60 frames until $D0 = 0 for 3 samples'],
            'frames_to_free_play': 3044,
            'script': 'tools/companion/ancientcave/build_ac_fixture.py (states ac_base.state, ac_party.state; party = base + spirit masks 0x3F and levels poked for girl and sprite)',
            'fixture_note': 'unlike the vanilla fixture nothing else is poked: AC already starts with three active heroes, HP 50/45/40, girl and sprite MP 8, '
                            'eight weapon types owned, one of each consumable and 100 gold; girl and sprite start with no spirits (masks 0)',
        },
        'fields': fields,
        'field_counts': dict(total=len(fields), **verdicts),
        'percent_verified': round(100.0 * verdicts.get('verified', 0) / len(fields), 2),
        'seed_777_same_verdicts': all(seed_match.values()),
        'macro_tests': macro,
        'equip_matrix': {k: v_ for k, v_ in matrix.items() if k.endswith('_correct')},
        'static_diff': {
            'method': 'byte compare of the first 2 MiB with regions merged when closer than 16 bytes',
            'regions': len(reg), 'differing_bytes': sum(e - s for s, e in reg),
            'bank_c0_regions': len([r for r in reg if r[0] < 0x10000]),
            'expansion': 'second 2 MiB (E0:0000 onward in HiROM addressing) holds new code and data; seed 1 uses 320,088 non-zero bytes up to file offset 0x2532A4',
            'header': 'bytes 0xFFC0..0xFFFF identical to vanilla (title, map mode 0x21, size byte 0x0B, checksum not updated): the header cannot tell the variants apart',
            'seed_to_seed': 'the 45 hook sites are identical for seeds 1 and 777 (same_hook_sites=%s); the JSL operands, expansion layout and map data differ' % same_sites,
        },
        'hooks_bank_c0': dict(count=len(h1), same_sites_seed_1_and_777=same_sites, sites=h1),
        'differences_for_the_companion': [
            'No WRAM field moved: all 34 map fields read the same addresses; no relocated variable was found.',
            'Map id ($DC) is generator numbering: floor 1 outside map = 0, other generated maps 1.., boss maps 256 + 2*floor (from the generator source, not measured); vanilla ids (131 Potos) never appear.',
            'Items: native inventory is one of each, qty 1, in order 0 1 2 3 4 5 7 6; qty-1 records are removed when used and the list shifts up; ids 7 and 6 are '
            'titled "Random Music" and "Stuck?" (Stuck? starts a fade and is not consumed).',
            'Weapons: all eight types owned from the start (slot byte 0x00 = owned tier 0); bits 7..6 of the slot byte = holder + 1 (1 boy, 2 girl, 3 sprite).',
            'Party: girl and sprite are active from frame 0, with no spirits until the cave gives them; max HP / max MP are derived by the game from the level.',
            'HUD: drawn on BG3 at rows 184..215 (FLOOR n and a clock row at 184..191, party bars at 199..215) instead of vanilla rows 199..216; the crop at row 199 leaves the floor/clock row visible.',
            'Floor clock: $CF1A counts seconds (+1 every 60 frames); minutes, hours and the floor number were not located.',
            'Name entry rejects duplicate hero names (relevant only to automation of new games).',
        ],
        'latent_companion_defects_exposed': [
            'som_weapon_ring_index ranks owned weapons in type order; the real ring order is types 1..7 then 0 (vanilla too). Stock host: 7 of 64 (held, wanted) equip pairs correct.',
            'The weapon ring cursor is relative to the held weapon: $180A at open holds the held weapon rank, the first directional press re-centres it on 0, and '
            'ring position = (cursor + held rank) mod n (vanilla too). Patched host (order + one sync tap + relative target + no-op when already held): 64 of 64.',
        ],
        'variant_detection_proposal': {
            'inputs': 'ROM size and a few structural facts; never a hash of the whole ROM (two seeds differ in 18,345 bytes of the first 2 MiB)',
            'steps': [
                'size 2,097,152 (or +512 header) -> candidate vanilla or translation; 4,194,304 -> candidate Ancient Cave',
                'for a 4 MiB ROM require the vanilla title and header bytes at 0xFFC0 (identical in all variants) plus opcode 0x22 (JSL) at C0:692F and C0:6D1E '
                '(file offsets 0x692F, 0x6D1E) with an operand bank >= 0xE0',
                'confirm with the ASCII strings "ancient`cave" and "moppleton" found in the expansion (positions vary by seed: E0:26D3 in seed 777, E0:272B in seed 1)',
                'otherwise vanilla table; if the title matches but a hook site differs (a different randomizer mode), use the unknown-variant table (read-only, no macros)',
            ],
            'tables': 'data/wram_companion_map_ancientcave.json holds per-field AC status; because no address moved, the companion needs one address table and a per-variant '
                      'behaviour record (item names, map id space, weapon ring rank rule, HUD crop row 184, weapon slot decoding)',
            'runtime_check': 'independent of the ROM: on a running game compare $E000/$E200/$E400 activity and the ids 0x80..0x82; the map-id space check is not reliable',
        },
        'limits': [
            'one generator configuration (default Ancient Cave settings, difficulty fields all "1" in the harness, music randomizer off); other modes (Open World, Chaos) were not generated',
            'snes9x 1.63 core only; only floor 1 and doors were exercised, no boss, no enemy fights, no chest or orb that gives magic, no shop',
            'hero_level, hero_hp_max, hero_mp_max, gold and the clock were confirmed by reading screenshots by eye (evidence PNGs in the state directory)',
            'weapon_levels, weapon_charge_level and armor_slots could not be provoked; spirit_levels and hero_status_timers are single data points',
            'attack spells need an enemy in range; only support spells were cast to completion (same limit as the vanilla fixture)',
        ],
        'tools': ['build_ac_fixture.py', 'verify_fields.py', 'test_ancientcave.py', 'equip_matrix.py', 'patched_host.py', 'make_report.py'],
    }
    out = ROOT / 'data/companion_ancientcave_20261017.json'
    out.write_text(json.dumps(report, indent=1) + '\n')

    orig = json.loads((ROOT / 'data/wram_companion_map.json').read_text())
    mp = json.loads(json.dumps(orig))
    mp['title'] = orig['title'] + ' (Ancient Cave column filled)'
    mp['derived_from'] = 'data/wram_companion_map.json (unchanged); this sibling only replaces verification.ancient_cave and adds ancient_cave_* keys'
    mp['roms']['ancient_cave'] = dict(
        orig['roms']['ancient_cave'],
        status='boots headless to free play (floor 1) after the name entry; field checks and macro tests run (docs/companion-ancientcave-20261017.md)',
        runtime_state='tools/companion/ancientcave/build_ac_fixture.py', static_diff=report['static_diff'])
    name_alias = {'hero_record_base': 'hero_record_base'}
    for f in mp['fields']:
        r = fields.get(f['name'])
        if r is None:
            f['verification']['ancient_cave'] = 'unverified'
            continue
        f['verification']['ancient_cave'] = {'verified': 'verified', 'partial': 'partial', 'not_testable': 'not-obtained', 'differs': 'differs'}[r['verdict']]
        f['ancient_cave_address'] = r['address']
        f['ancient_cave_evidence'] = r['evidence']
    missing = sorted(set(fields) - {f['name'] for f in mp['fields']})
    mp['ancient_cave_extra_fields_tested'] = missing
    mp['ancient_cave'] = dict(
        summary=report['field_counts'], percent_verified=report['percent_verified'],
        differences=report['differences_for_the_companion'], latent_defects=report['latent_companion_defects_exposed'],
        detection=report['variant_detection_proposal'])
    (ROOT / 'data/wram_companion_map_ancientcave.json').write_text(json.dumps(mp, indent=1) + '\n')
    print(report['field_counts'], report['percent_verified'], 'missing', missing, 'seeds agree', report['seed_777_same_verdicts'])


if __name__ == '__main__':
    main()
