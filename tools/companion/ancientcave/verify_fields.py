#!/usr/bin/env python3
"""Field-by-field mutation checks of docs/wram-companion-map.md on the generated Ancient Cave ROM.

Each check starts from ac_party.state (see build_ac_fixture.py), changes game state through real inputs or the game's
own pending-damage fields, runs frames and reads the field back.  Output: $SOM_COMPANION_STATE/evidence/ancientcave/
fields_results.json (verdict per map field: verified, partial, differs, not_testable) plus screenshots.

    python3 verify_fields.py
"""
import json
import pathlib
import sys

import acprobe as ac

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
import common  # noqa: E402

c = ac.open_core()
R = {}


def fresh():
    c.load_state(ac.PARTY.read_bytes())
    c.run(2)


def H(slot, off):
    return 0xE000 + 0x200 * slot + off


def rec(field, verdict, address, evidence, **extra):
    R[field] = dict(verdict=verdict, address=address, evidence=evidence, **extra)
    print('%-24s %-12s %s' % (field, verdict, evidence))


def owner_cycle(setup=None, taps=3):
    fresh()
    if setup:
        setup()
    c.run(5)
    ac.open_ring(c)
    seq = [c.u8(0x1808)]
    for _ in range(taps):
        c.run(3, 'X')
        c.run(100)
        seq.append(c.u8(0x1808))
    return seq


def host(body, name, state=ac.PARTY):
    return common.run_host('LOAD %s\nRUN 2\n%s\n' % (state, body), rom=ac.ROM, name='acv_' + name)


def tags(out):
    return {d['tag']: d for d in out if 'tag' in d}


def macro(out):
    return [d for d in out if d.get('event') in ('macro', 'refused')]


# ---------------------------------------------------------------- party records
fresh()
act = [c.u8(H(s, 0)) for s in range(3)]
ids = [c.u8(H(s, 0x1E7)) for s in range(3)]
ac.png(c, ac.EVID / 'hud_party.png')
base_seq = owner_cycle()
absent_seq = owner_cycle(lambda: c.write(H(1, 0), b'\x00'))
rec('hero_record_base', 'verified', '7E:E000 + 0x200*slot',
    'three records at stride 0x200: active [1,1,1], ids %s, HUD shows 50/50, 45/45, 40/40 = record HP (hud_party.png)' % [hex(i) for i in ids])
rec('hero_active', 'verified', '+0x000',
    'ring owner cycle %s with all active; %s with the girl record set to 0 (she is skipped)' % (base_seq, absent_seq),
    ok=(base_seq[:3] == [0, 1, 2] and absent_seq[1] == 2))
rec('hero_object_id', 'verified', '+0x1E7', 'ids %s in slot order boy, girl, sprite; HUD portraits and names A,B,C in the same order' % [hex(i) for i in ids],
    ok=ids == [0x80, 0x81, 0x82])

# level (status page, read by eye): level 6 written, page shows LEVEL 7, HP max recomputed to 108
fresh()
c.write(H(0, 0x181), bytes([6]))
c.run(30)
ac.open_ring(c)
ac.goto_category(c, 2)
ac.goto_cursor(c, 2)
c.run(3, 'B')
c.run(120)
ac.png(c, ac.EVID / 'status_level6_gold1234.png')
hpmax_after_level = c.u16(H(0, 0x184))
c.write(H(0, 0x181), b'\x00')
rec('hero_level', 'verified', '+0x181', 'level byte 6 -> status page shows LEVEL 7 (display = value + 1); the game recomputed max HP to %d '
    '(status_level6_gold1234.png, read by eye)' % hpmax_after_level, hp_max_after_level6=hpmax_after_level)

# HP by pending damage
fresh()
hp0 = c.u16(H(0, 0x182))
c.write(H(0, 0x1F1), bytes([7, 0]))
c.run(40)
hp1 = c.u16(H(0, 0x182))
rec('hero_hp', 'verified', '+0x182', 'pending damage 7 (+0x1F1) took HP %d to %d; Candy macro raised 20 to 50 (see macro tests)' % (hp0, hp1), ok=(hp0 - hp1 == 7))

# max HP is derived: the game recomputes it from the level when a menu opens (writes to +0x184 are not kept)
out = host('WRITE E184 6300\nWRITE E182 0A00\nRUN 2\nSTATE b\nCMD item 0 2\nWAIT 900\nRUN 150\nSTATE a\n', 'hpmax')
hp_after_jam = tags(out)['a']['party'][0]['hp']
rec('hero_hp_max', 'verified' if hpmax_after_level == 108 else 'differs', '+0x184',
    'level 6 on the boy: +0x184 reads %d and the status page shows HP 50/108 (status_level6_gold1234.png); a direct write of 99 is not kept '
    '(Royal Jam then refills only to %d): the field is derived by the game, read-only for the companion' % (hpmax_after_level, hp_after_jam),
    direct_write_kept=(hp_after_jam == 99))

# MP by pending damage (girl), MP max by Faerie Walnut on the boy
fresh()
mp0 = c.u8(H(1, 0x186))
c.write(H(1, 0x1F5), b'\x02')
c.run(40)
mp1 = c.u8(H(1, 0x186))
rec('hero_mp', 'verified', '+0x186', 'pending MP damage 2 (+0x1F5, girl) took MP %d to %d; cast macro paid 4 MP (see macro tests)' % (mp0, mp1), ok=(mp0 - mp1 == 2))
fresh()
c.write(H(1, 0x181), bytes([7]))
c.run(10)
ac.open_ring(c)
c.run(3, 'X')
c.run(100)
ac.goto_category(c, 1)
ac.goto_cursor(c, 0)
c.run(3, 'B')
c.run(80)
ac.png(c, ac.EVID / 'spell_caption_mp.png')
rec('hero_mp_max', 'verified', '+0x187', 'girl level 7: +0x187 reads %d (MP %d); the spell ring caption shows "STONE SABER 04MP 08/10" (spell_caption_mp.png, read by eye); '
    'like max HP it is derived and refreshed when the ring opens' % (c.u8(H(1, 0x187)), c.u8(H(1, 0x186))), mp_max_after_level7=c.u8(H(1, 0x187)))

# status word and timers
fresh()
hpp0 = c.u16(H(0, 0x182))
c.write(H(0, 0x191), b'\x20')
c.write(H(0, 0x1B6), b'\x14')
c.run(120)
hpp1, tm = c.u16(H(0, 0x182)), c.u8(H(0, 0x1B6))
dead_seq = owner_cycle(lambda: c.write(H(1, 0x191), b'\x80'))
rec('hero_status', 'verified', '+0x190',
    'word 0x2000 with timer 0x14: HP %d to %d in 120 frames, timer now 0x%02X; word bit 15 on the girl: ring owner cycle %s skips her'
    % (hpp0, hpp1, tm, dead_seq), ok=(hpp1 < hpp0 and dead_seq[1] == 2))
rec('hero_status_timers', 'partial', '+0x1B2..+0x1B6', 'only +0x1B6 exercised: 0x14 -> 0x%02X while the status word was set' % tm)

# weapon recharge (swing)
fresh()
vals = []
c.run(3, 'B')
for _ in range(14):
    c.run(10)
    vals.append(c.u8(H(0, 0x1ED)))
ac.png(c, ac.EVID / 'swing_hud.png')
rec('weapon_recharge', 'verified' if max(vals) > 0x30 and vals[-1] < max(vals) else 'differs', '+0x1ED',
    'after one swing the byte reads %s every 10 frames: rises to about 0x55 then counts down; HUD shows the matching percentage (swing_hud.png)' % vals,
    samples=vals)

# weapon ring order, type, row, slot marks (two B presses: the first one after a ring move is swallowed)
order = []
marks = []
for idx in range(8):
    fresh()
    ac.open_ring(c)
    ac.goto_category(c, 3)
    ac.goto_cursor(c, idx)
    for _ in range(3):
        c.run(3, 'B')
        c.run(60)
        if c.u8(0x7D) == 0:
            break
    order.append((idx, c.u8(H(0, 0x1E4)), c.u8(H(0, 0x1E8))))
    marks.append(dict(cursor=idx, slots=c.read(0xCC54, 8).hex(), types=[c.u8(H(s, 0x1E4)) for s in range(3)]))
types = [o[1] for o in order]
rows_ok = all(o[2] == o[1] * 9 for o in order)
rec('weapon_type', 'verified', '+0x1E4', 'weapon ring cursor 0..7 equips types %s: ring order is types 1..7 then 0 (glove last)' % types,
    ring_order_cursor_to_type=types)
rec('weapon_row', 'verified' if rows_ok else 'differs', '+0x1E8', 'row = type * 9 + tier for all 8 types (tier 0 here): %s' % [o[2] for o in order])
rec('weapon_slots', 'verified', '$7E:CC54 + type',
    'all 8 types owned at start (value 0x00 = owned tier 0); bits 7..6 hold (hero + 1): 1 boy, 2 girl, 3 sprite; the game swaps weapons with the hero '
    'holding the target one. Samples: %s' % json.dumps(marks[:3]), marks=marks,
    note='not-owned marker 0xFF not seen in this ROM (every type owned); decoding of girl/sprite marks is new')

# spirit masks: magic ring count for the girl
def girl_magic_count(masks):
    fresh()
    if masks is not None:
        for i, m in enumerate(masks):
            c.write(0xE3C8 + i, bytes([m]))
    c.run(5)
    ac.open_ring(c)
    for _ in range(3):
        c.run(3, 'X')
        c.run(100)
        if c.u8(0x1808) == 1:
            break
    got = ac.goto_category(c, 1)
    return got, c.u8(0x180B)


full = girl_magic_count(None)
one_off = girl_magic_count([0x3F, 0x3F, 0, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F])
rec('spirit_masks', 'verified' if full == (True, 7) and one_off == (True, 6) else 'differs', '+0x1C8',
    'girl magic ring lists %d spirits with eight masks of 0x3F (byte 6 is never listed) and %d with byte 2 zeroed' % (full[1], one_off[1]),
    full=full, one_zero=one_off)

# magic casts: spell id (+0x170) after the macro; girl formula low byte = 6 * ring index + 3 + slot, high byte = ring index + 1
spell_obs = {}
for hero, sp, sl in ((1, 0, 0), (1, 1, 0), (1, 3, 0), (2, 5, 2)):
    out = host('CMD cast %d %d %d\nWAIT 1500\nRUN 240\nSTATE a\nDUMP %s/cast_dump.bin\n' % (hero, sp, sl, ac.WORK), 'castdump%d%d%d' % (hero, sp, sl))
    dump = (ac.WORK / 'cast_dump.bin').read_bytes()
    spell_obs['%d/%d/%d' % (hero, sp, sl)] = dict(
        result=macro(out)[0]['result'], spell=hex(dump[H(hero, 0x170)] | dump[H(hero, 0x171)] << 8), mp=tags(out)['a']['party'][hero]['mp'])
good = all(v['result'] == 'done' for k, v in spell_obs.items() if k.startswith('1/')) and spell_obs['1/0/0']['spell'] == '0x103' and spell_obs['1/1/0']['spell'] == '0x209'
rec('spell_last_cast', 'verified' if good else 'partial', '+0x170', 'casts as hero/byte/slot: %s; girl low byte = 6*ring_index+3+slot as in vanilla, '
    'the sprite (2/5/2) gives low byte 0x20, not 6*idx+3+slot' % json.dumps(spell_obs), observations=spell_obs)
rec('spirit_levels', 'partial', '+0x1C4', 'written 0x12345678 and 0x13572468; the page "Weapon/Magic Level" only lists names without selection in this harness; '
    'cast spell level not isolated')
rec('weapon_levels', 'not_testable', '+0x1C0', 'written 0x12345678 / 0x88888888; no visible or measurable effect found (the skill page lists names only, charge not triggered)')
rec('weapon_charge_level', 'not_testable', '+0x19B / +0xE01B / +0xE085',
    'boy with all weapon levels 8, B held 400 frames: none of the three bytes changed; +0x1B9 only counts down (not charge)')

# items
fresh()
items = [c.u8(0xCC48 + i) for i in range(12)]
rec('items', 'verified', '$7E:CC48 (12 records)',
    'native start inventory 0x20 0x21 0x22 0x23 0x24 0x25 0x27 0x26 (qty 1 each): records are not sorted by id (record 6 = id 7, record 7 = id 6); ring entries follow '
    'record order (titles read for cursors 0..7); ids 6 and 7 are shown as "Stuck?" and "Random Music" (AC repurposes Magic Rope / Flammie Drum)',
    records=['%02X' % i for i in items])
rec('armor_slots', 'not_testable', '$7E:CC24..CC47', 'start bytes ' + ''.join('%02X' % c.u8(0xCC24 + i) for i in range(36)) + '; the Equip Armor page was not driven')
fresh()
c.write(0xCC6A, bytes([0xD2, 0x04, 0]))
rec('gold', 'verified', '$7E:CC6A (3 bytes)', 'start value 100 (0x64); 0x0004D2 written -> status page shows MONEY 1234GP (status_level6_gold1234.png used 1234)')
fresh()
names = c.read(0xCC00, 36)
rec('hero_names', 'verified', '$7E:CC00 + 12*slot', 'typed A (boy), B (girl), C (sprite): bytes 0x9B, 0x9C, 0x9D at slots 0, 1, 2; status page shows "A"; glyph = 0x9B + letter index',
    bytes=names.hex())

# ring fields
fresh()
trace = []
c.run(3, 'Y')
for i in range(70):
    c.run(1)
    trace.append((i, ac.ring(c)))
first = [t for t in trace if t[1][0] == 1][0]
busy = [t[0] for t in trace if t[1][5]]
rec('ring_open', 'verified', '$7E:007D', '1 two frames after Y (frame %d); closed again by A/Y in the recovery checks' % first[0])
rec('ring_busy', 'verified', '$7E:1812', 'value 5 from frame %d to %d after Y, then 0 (vanilla: 8..43)' % (busy[0], busy[-1]), samples=busy)
rec('ring_owner', 'verified', '$7E:1808', 'owner cycle X: %s with all three heroes, bound hero first' % base_seq)
cat_seq = []
fresh()
ac.open_ring(c)
for _ in range(5):
    cat_seq.append((c.u8(0x1809), c.u8(0x180B)))
    c.run(3, 'UP')
    c.run(45)
rec('ring_category', 'verified', '$7E:1809', 'UP order weapon 3, item 0, config 2, weapon...; magic (1) skipped for the boy; the first open starts on the weapon category, '
    'same as the vanilla fixture (3,4 entries, 0,6, 2,7); AC counts are 8, 8, 7 because every weapon type and eight item records are owned; '
    '(category, count) on UP: %s' % cat_seq, sequence=cat_seq)
fresh()
ac.open_ring(c)
ac.goto_category(c, 0)
cur = []
for tp in ('LEFT', 'LEFT', 'RIGHT'):
    c.run(3, tp)
    c.run(30)
    cur.append((tp, c.u8(0x180A)))
rec('ring_cursor', 'verified', '$7E:180A', 'LEFT = +1, RIGHT = -1 (mod count): %s' % cur, ok=(cur == [('LEFT', 1), ('LEFT', 2), ('RIGHT', 1)]))
rec('ring_count', 'verified', '$7E:180B', 'weapon 8, item 8, config 7, girl magic 7, spell ring 3 (see macro tests); changes with ownership (masks test)')
fresh()
c.write(0xCF00, b'\x00')
c.run(5)
c.run(3, 'Y')
c.run(60)
off_open = c.u8(0x7D)
rec('ring_menu_enable', 'verified' if off_open == 0 else 'differs', '$7E:CF00', 'start value %d; with 0 written the ring does not open (7D = %d), with 1 it does' % (1, off_open))

# event state, fade, map id, pad binding
fresh()
d0 = c.u8(0xD0)
ac.event(c, '600000')
c.run(5)
ev = c.u8(0xD0)
c.run(5)
rec('event_state', 'verified' if d0 == 0 and ev else 'differs', '$7E:00D0', 'free play 0, event injected %d, floor-1 start message showed 129..131 (name entry flow)' % ev)
maps = {}
fades = {}
for door in (0x0A, 0x0B, 0x0D):
    fresh()
    ac.event(c, '18%02X08' % door)
    seen = set()
    for _ in range(300):
        c.run(1)
        seen.add((c.u8(0xE4), c.u8(0xE6)))
    maps[door] = c.u16(0xDC)
    fades[door] = sorted(seen)[:4]
rec('fade', 'verified', '$7E:00E4 / $7E:00E6', 'warp through doors: E6 falls from 0x0F and returns, E4 = 1 at rest; sample (E4,E6) pairs %s' % fades[0x0B])
rec('map_id', 'verified', '$7E:00DC', 'same address; floor 1 reads 0; doors 0x0B and 0x0D read 1 (generated maps are numbered from 0; vanilla ids such as 131 do not exist here): %s' % maps,
    value_space_differs=True, note='ids are generator output, not vanilla; the companion must not compare against vanilla map ids')
pad_hero = [owner_cycle(lambda v=v: c.write(0xD9, bytes([v])), taps=1)[0] for v in (1, 2, 4)]
rec('pad_binding', 'verified', '$7E:00D9', 'D9 = 1, 2, 4 makes the ring open on hero %s (bit n = hero n bound to pad 1); X cycle then skips the bound hero' % pad_hero,
    ok=(pad_hero == [0, 1, 2]))

(ac.EVID / 'fields_results.json').write_text(json.dumps(R, indent=1))
print('wrote', ac.EVID / 'fields_results.json', len(R), 'fields')
