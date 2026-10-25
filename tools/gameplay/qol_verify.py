#!/usr/bin/env python3
"""Emulator verification of the qol_* options of som_gameplay.py (headless snes9x through tools/companion/retro.py).

  SOM_ROM=/path/to/vanilla.sfc python3 tools/gameplay/qol_verify.py [--frames 9000] [--only NAME] [--report out.json]

Every check builds a patched COPY of the ROM under SOM_QOL_WORK (never inside the repository), runs it next to the vanilla
ROM and compares RAM or frame traces.  Exit code 0 only when every check passed.  Checks:

  newgame_*   9,000 frames of tools/som_input_newgame.txt, WRAM SHA-1 every 50 frames: options that do not engage in the
              prologue must be bit identical to vanilla; the three screen options must diverge (and are measured below)
  fade        length of every fade episode (brightness current != target) in the first 3,200 frames
  deadzone    hero screen x after walking right / left until the screen scrolls
  unready     damage of a real weapon hit with the weapon gauge at 0 and at 40
  levelup     MP after a kill that levels the party up (girl and sprite start at 0 MP)
  deadexp     experience and HP of a hero at 0 HP when a monster dies
  itemlimit   stack count after giving 9 Candy through the game's own event opcode 0x1E
  magic       spell progress per cast at levels 0..7 through the game's own routine D0:4DCD on the 65816 interpreter
              (tools/cpu65816.py); the routine needs the caster record and cannot be reached from the pad without a spell fixture
  other_roms  (env SOM_QOL_EXTRA_ROMS=a.smc:b.smc) all options on a copy of each ROM, conflicts skipped: boots and matches the
              unpatched ROM until the first screen fade
  chest       frames of the hero "breaking lock" pose (animation code 0x9B, script D1:5FB8) played through the event command
              ACTOR_ANIM: vanilla 141 frames, patched 1 frame.  The two chest-object scripts (D1:EC84, D1:ED01) are verified by
              bytes only: no chest object could be driven (see docs/qol-gameplay-20261020.md)
  magic_cast  spell progress of a real spell cast from the ring menu (girl, five spirits, CFFC = 8): vanilla, x2, x3
  ring        ring menu: frames to open and to turn one step, and the resting layout (positions, selection) after taps and a held turn
  message     frames the top-of-screen level-up messages stay on screen (three heroes, 2,000 experience), vanilla / x2 / x4
  rando       (env SOM_QOL_EXTRA_ROMS) the randomizer ROMs: dead-hero experience applies on top of their hook, their mpAtLevel
              and fasterMagicLevel hooks are recognised (and give x4 spell progress on the interpreter), nothing else is skipped
"""
import argparse
import json
import os
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
sys.path.insert(0, str(HERE.parents[1] / 'tools'))
import qol_harness as H  # noqa: E402

FIXTURE = H.STATES / 'mob_vanilla_344.state'


def slot(n):
    return H.slot(n)


def vanilla_rom():
    p = os.environ.get('SOM_ROM')
    if not p or not pathlib.Path(p).exists():
        sys.exit('set SOM_ROM to a US v1.0 Secret of Mana image')
    return p


def trace_equal(a, b):
    return [x[1] for x in a] == [x[1] for x in b]


def first_divergence(a, b):
    return next((x[0] for x, y in zip(a, b) if x[1] != y[1]), None)


def fade_episodes(rom, frames=3200):
    log = []

    def hook(c, f):
        log.append((f, c.wram[0xE6], c.wram[0xE7]))
    H.run_newgame(rom, frames, 10 ** 6, None, None, hook)
    eps, start = [], None
    for f, cur, tgt in log:
        if cur != tgt and start is None:
            start = f
        if cur == tgt and start is not None:
            eps.append(f - start)
            start = None
    return eps


def walk_screen_x(rom):
    s = H.parse_script() + [(7700, 8000, H.retro.Core.mask(['RIGHT'])), (8100, 8400, H.retro.Core.mask(['LEFT']))]
    log = {}

    def hook(c, f):
        if f in (7990, 8390):
            log[f] = c.u16(0xE002) - c.u16(0xA8)
    H.run_newgame(rom, 8400, 10 ** 6, None, s, hook)
    return log[7990], log[8390]


def setup_fight(rom):
    c = H.load_fixture(rom, FIXTURE)
    for s in (4, 5):
        c.write(slot(s) + 2, (0x3000).to_bytes(2, 'little'))
    for s in (1, 2):
        c.write(slot(s) + 2, (0x3200).to_bytes(2, 'little'))
        c.write(slot(s) + 4, (0x3200).to_bytes(2, 'little'))
    c.run(20)
    return c


def hit_damage(rom, gauge, n=10):
    c = setup_fight(rom)
    m, hh = slot(3), slot(0)
    c.write(m + 0x182, (900).to_bytes(2, 'little'))
    c.write(m + 0x184, (900).to_bytes(2, 'little'))
    snap = bytes(c.wram[m:m + 0x200])
    out = []
    for i in range(n):
        c.write(m, snap)
        c.write(hh + 0x1AE, b'\x00')
        for _ in range(1 + (i * 7) % 11):
            c.write(hh + 0x1ED, bytes([gauge]))
            for s in (1, 2):
                c.write(slot(s) + 2, (0x3200).to_bytes(2, 'little'))
                c.write(slot(s) + 4, (0x3200).to_bytes(2, 'little'))
            c.run(1)
        c.write(m + 0x59, b'\x01')
        c.write(hh + 0x1ED, bytes([gauge]))
        hp0 = c.u16(m + 0x182)
        c.run(15)
        out.append(hp0 - c.u16(m + 0x182))
    return out


def kill_party(rom, reward=2000, hero1_hp=None):
    c = setup_fight(rom)
    m = slot(3)
    hs = [slot(i) for i in range(3)]
    for x in hs[1:]:
        c.write(x + 0x186, b'\x00')
    if hero1_hp is not None:
        c.write(hs[1] + 0x182, hero1_hp.to_bytes(2, 'little'))
    c.write(m + 0x182, (5).to_bytes(2, 'little'))
    c.write(m + 0x184, (900).to_bytes(2, 'little'))
    c.write(m + 0x18D, reward.to_bytes(2, 'little'))
    c.write(m + 0x18F, b'\x00')
    c.write(m + 0x1F0, b'\x00')
    c.write(m + 0x1EC, b'\x00')          # killer = hero 0: the experience share runs only when this is < 3
    c.write(m + 0x1F1, (50).to_bytes(2, 'little'))
    for _ in range(200):
        for s in (1, 2):
            c.write(slot(s) + 2, (0x3200).to_bytes(2, 'little'))
            c.write(slot(s) + 4, (0x3200).to_bytes(2, 'little'))
        c.run(1)
    return {'hp': [c.u16(x + 0x182) for x in hs], 'maxhp': [c.u16(x + 0x184) for x in hs], 'mp': [c.u8(x + 0x186) for x in hs],
            'maxmp': [c.u8(x + 0x187) for x in hs], 'exp': [c.u16(x + 0x18D) for x in hs]}


def give_candy(rom, n=9):
    c = H.load_fixture(rom, FIXTURE)
    c.run(5)
    out = []
    for _ in range(n):
        c.write(0x1FF00, bytes([0x1E, 0x40, 0x00]))      # event opcode 1E: give item 0x40 (Candy); 00 ends the script
        c.write(0xD1, bytes([0x00, 0xFF, 0x7F]))
        c.run(1)
        c.write(0xD0, b'\x01')
        c.run(6)
        out.append((c.u8(0xCC48) >> 5, c.u8(0xCF0F)))     # (stack count, result code: 0 added, 2 stack full)
    return out


def spell_progress(rom_path, level, full=True):
    import cpu65816 as cpu
    raw = pathlib.Path(rom_path).read_bytes()
    cpu.set_rom(raw[len(raw) % 0x8000:])
    c = cpu.CPU()
    w = c.wram
    w[0xE1FB] = 0x80          # hero record
    w[0xE177] = 1             # spell 1: level nibble = low nibble of E1C4
    w[0xE1C4] = level
    w[0xCFFC] = 8
    w[0xED] = 0x80 if full else 0
    c.DB, c.D, c.M, c.XF, c.E, c.S = 0x7E, 0, 1, 0, 0, 0x1FF0
    c.push8(0x7E)
    c.push16(0xFFFF)          # RTL target 7E:0000
    c.PB, c.PC, c.Y = 0xD0, 0x4DCD, 0
    for _ in range(20000):
        if c.PB == 0x7E and c.PC == 0:
            return w[0xE1D8 + 1]
        c.step()
    raise RuntimeError('routine did not return')


def pose_frames(rom, code=0x9B):
    """Frames the hero spends away from its idle animation after the event command ACTOR_ANIM (31 a b).  The vanilla pose
    (`code`) lasts 140 frames; with the chest option the script ends at once and only a one-frame blip is visible."""
    c = H.load_fixture(rom, FIXTURE)
    c.run(5)
    idle = c.u8(0xE011)
    c.write(0x1FF00, bytes([0x31, 0x00, code, 0x00]))
    c.write(0xD1, b'\x00\xff\x7f')
    c.run(1)
    c.write(0xD0, b'\x01')
    away = 0
    for _ in range(400):
        c.run(1)
        if c.u8(0xE011) != idle:
            away += 1
    return away if away else None


def shop_buy(rom, money):
    """Open the first shop list (event 2E 00 00) with an empty inventory and buy the first item (Candy, price 10) once."""
    c = H.load_fixture(rom, FIXTURE)
    c.run(5)
    c.write(0xCC6A, money.to_bytes(3, 'little'))
    c.write(0xCC48, bytes([0xFF] * 12))
    c.write(0x1FF00, bytes([0x2E, 0, 0, 0]))
    c.write(0xD1, b'\x00\xff\x7f')
    c.run(1)
    c.write(0xD0, b'\x01')
    c.run(120)
    for b in ('B', 'B'):                       # select the row, confirm the purchase
        c.run(3, [b])
        c.run(40)
    c.run(60)
    inv = c.u8(0xCC48)
    return int.from_bytes(c.read(0xCC6A, 3), 'little'), inv >> 5 if inv != 0xFF else 0


GIRL = slot(1)


def cast_progress(rom, spirit, spell=0):
    """Progress counter of `spirit` after one real cast: ring menu of the girl (Y, X, UP to the magic ring, B, B, B)."""
    c = H.load_fixture(rom, FIXTURE)
    c.run(3)
    c.write(GIRL + 0x1C8, bytes([0x3F] * 8))
    c.write(GIRL + 0x186, bytes([60, 60]))
    c.write(0xCFFC, b'\x08')                       # the spell levels only advance below the story cap
    before = c.read(GIRL + 0x1D8, 8)

    def tap(b, hold=3, wait=20):
        c.run(hold, [b])
        c.run(wait)
    c.run(3, ['Y'])
    c.run(70)
    for _ in range(4):
        if c.u8(0x1808) == 1:
            break
        tap('X', 3, 80)
    for _ in range(5):
        if c.u8(0x1809) == 1:
            break
        tap('UP', 3, 45)
    for _ in range(spirit):
        tap('LEFT', 3, 22)
    tap('B', 3, 60)
    for _ in range(spell):
        tap('LEFT', 3, 22)
    tap('B', 3, 60)
    tap('B', 3, 60)
    c.run(300)
    after = c.read(GIRL + 0x1D8, 8)
    return [a - b for a, b in zip(after, before)], c.u8(GIRL + 0x186)


def ring_metrics(rom):
    c = H.load_fixture(rom, FIXTURE)
    for i in range(3):
        c.write(slot(i) + 0x182, (50).to_bytes(2, 'little'))
    c.write(GIRL + 0x1C8, bytes([0x3F] * 8))
    c.run(2, ['X'])
    opened = None
    for f in range(200):
        c.run(1)
        if f > 12 and c.u16(0x1812) == 0:
            opened = f
            break
    c.run(10)

    def layout():
        n = c.u8(0x180B)
        return (c.u8(0x1809), n, c.u8(0x180A), [(c.u16(0x18C0 + 16 * i + 2), c.u16(0x18C0 + 16 * i + 4)) for i in range(n)])
    turns, lay = [], []
    for cat in range(4):
        for d in ('RIGHT', 'LEFT', 'LEFT'):
            c.run(1, [d])
            k = 1
            while c.u8(0x1806) != 0 and k < 100:
                c.run(1)
                k += 1
            turns.append(k)
            c.run(30)
            lay.append(layout())
        c.run(1, ['UP'])
        c.run(60)
    # held turn, then release: the ring has to come to rest on a row
    c.run(60, ['RIGHT'])
    c.run(60)
    held = layout()
    return dict(open=opened, turn=sum(turns) / len(turns), layouts=lay, held_rest=held)


def banner_frames(rom, frames=900):
    import numpy as np
    on = []

    def hook(c, f):
        a = np.frombuffer(c.frame_rgb, np.uint8).reshape(c.height, c.width, 3)
        on.append(bool(a[12, 100:140].max() >= 200))
    c = setup_fight(rom)
    m = slot(3)
    c.write(m + 0x182, (5).to_bytes(2, 'little'))
    c.write(m + 0x184, (900).to_bytes(2, 'little'))
    c.write(m + 0x18D, (2000).to_bytes(2, 'little'))
    c.write(m + 0x18F, b'\x00')
    c.write(m + 0x1F0, b'\x00')
    c.write(m + 0x1EC, b'\x00')
    c.write(m + 0x1F1, (50).to_bytes(2, 'little'))
    for f in range(frames):
        for s in (1, 2):
            c.write(slot(s) + 2, (0x3200).to_bytes(2, 'little'))
            c.write(slot(s) + 4, (0x3200).to_bytes(2, 'little'))
        c.run(1)
        hook(c, f)
    return sum(on)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--frames', type=int, default=9000)
    ap.add_argument('--only')
    ap.add_argument('--report')
    a = ap.parse_args()
    V = vanilla_rom()
    res = {}
    fails = []

    def check(name, ok, **info):
        res[name] = dict(ok=bool(ok), **info)
        print('%-28s %s %s' % (name, 'ok  ' if ok else 'FAIL', json.dumps(info)[:230]))
        if not ok:
            fails.append(name)

    def want(name):
        return a.only is None or a.only == name

    opts = {
        'chest': {'qol_chest_fast': True},
        'unready': {'qol_unready_damage': True},
        'itemlimit': {'qol_item_limit': 7},
        'mp': {'qol_levelup_mp_restore': True},
        'deadexp': {'qol_dead_exp': True},
        'magic2': {'qol_magic_level_rate': 2},
        'magic3': {'qol_magic_level_rate': 3},
        'fades': {'qol_fast_fades': True},
        'trans': {'qol_fast_transitions': True},
        'dz_tight': {'qol_scroll_deadzone': 'tight'},
        'dz_tighter': {'qol_scroll_deadzone': 'tighter'},
        'ring': {'qol_ring_speed': 2},
        'msg2': {'qol_message_speed': 2},
        'msg4': {'qol_message_speed': 4},
        'deadmp': {'qol_dead_exp': True, 'qol_levelup_mp_restore': True},
        'shop': {'qol_shop_fill': True},
        'shop7': {'qol_shop_fill': True, 'qol_item_limit': 7},
    }
    roms = {k: H.build_rom(V, o, 'v_' + k) for k, o in opts.items()}
    quiet = {k: v for k, o in opts.items() if k in ('chest', 'unready', 'itemlimit', 'mp', 'deadexp', 'magic2', 'magic3')
             for k, v in o.items()}
    roms['quiet_all'] = H.build_rom(V, quiet, 'v_quiet_all')
    roms['all'] = H.build_rom(V, {**quiet, 'qol_fast_fades': True, 'qol_fast_transitions': True, 'qol_scroll_deadzone': 'tighter'}, 'v_all')

    if want('newgame'):
        base = H.run_newgame(V, a.frames, 50)['trace']
        for k in ('chest', 'unready', 'itemlimit', 'mp', 'deadexp', 'magic2', 'magic3', 'quiet_all', 'ring', 'msg2', 'msg4', 'deadmp', 'shop'):
            t = H.run_newgame(roms[k], a.frames, 50)['trace']
            check('newgame_identical_' + k, trace_equal(base, t), frames=a.frames, checkpoints=len(t))
        for k in ('fades', 'trans', 'dz_tight', 'dz_tighter', 'all'):
            t = H.run_newgame(roms[k], a.frames, 50)['trace']
            check('newgame_diverges_' + k, not trace_equal(base, t), first_divergence=first_divergence(base, t), frames=a.frames)
    if want('chest'):
        v, p = pose_frames(V), pose_frames(roms['chest'])
        check('chest_pose', v is not None and p is not None and v >= 130 and p <= 3, vanilla_frames=v, patched_frames=p)
    if want('shop'):
        res = {}
        for money in (3, 25, 100000, 65541):          # too poor, money for two, plenty, plenty with a borrow across the low word
            res[money] = {'vanilla': shop_buy(V, money), 'fill': shop_buy(roms['shop'], money), 'fill7': shop_buy(roms['shop7'], money)}
        ok = (all(r['vanilla'][1] == (0 if m == 3 else 1) and r['vanilla'][0] == m - (0 if m == 3 else 10) for m, r in res.items())
              and res[3]['fill'] == (3, 0) and res[25]['fill'] == (5, 2)
              and all(res[m]['fill'] == (m - 40, 4) and res[m]['fill7'] == (m - 70, 7) for m in (100000, 65541)))
        check('shop_fill', ok, results={str(m): r for m, r in res.items()})
    if want('magic_cast'):
        res = {}
        for k, rom in (('vanilla', V), ('x2', roms['magic2']), ('x3', roms['magic3'])):
            res[k] = [cast_progress(rom, sp) for sp in range(5)]
        van = [d for d, _ in res['vanilla']]
        ok = all(sum(1 for x in d if x) == 1 for d in van)
        gain = lambda r: [max(d) for d, _ in r]      # noqa: E731
        g1, g2, g3 = gain(res['vanilla']), gain(res['x2']), gain(res['x3'])
        ok = ok and g1 == [8, 7, 6, 5, 4] and g2 == [2 * x for x in g1] and g3 == [3 * x for x in g1]
        check('magic_cast', ok, gain_vanilla=g1, gain_x2=g2, gain_x3=g3, mp_after=[m for _, m in res['vanilla']])
    if want('ring'):
        v, p = ring_metrics(V), ring_metrics(roms['ring'])
        same = v['layouts'] == p['layouts']
        check('ring_speed', same and p['open'] < v['open'] and p['turn'] < 0.7 * v['turn'] and sorted(p['held_rest'][3]) == sorted(v['held_rest'][3]),
              open_vanilla=v['open'], open_x2=p['open'], turn_vanilla=round(v['turn'], 1), turn_x2=round(p['turn'], 1), same_layouts=same,
              held_selection=(v['held_rest'][2], p['held_rest'][2]))
    if want('message'):
        v, h, q = banner_frames(V), banner_frames(roms['msg2']), banner_frames(roms['msg4'])
        check('message_speed', 0 < q < h < v, frames_vanilla=v, frames_half=h, frames_quarter=q)
    if want('fade'):
        v, f, t = fade_episodes(V), fade_episodes(roms['fades']), fade_episodes(roms['trans'])
        check('fade_faster', len(v) == len(f) and sum(f) < sum(v) and all(x <= y for x, y in zip(f, v)), vanilla=v, fast_fades=f)
        check('fade_transitions', len(v) == len(t) and sum(t) < sum(f), vanilla=v, no_fade=t)
    if want('deadzone'):
        v, ti, tr = walk_screen_x(V), walk_screen_x(roms['dz_tight']), walk_screen_x(roms['dz_tighter'])
        check('deadzone', ti[0] < v[0] and tr[0] < ti[0] and ti[1] > v[1] and tr[1] > ti[1], vanilla_right_left=v, tight=ti, tighter=tr)
    if want('unready'):
        v0, v40 = hit_damage(V, 0), hit_damage(V, 40)
        p0, p40 = hit_damage(roms['unready'], 0), hit_damage(roms['unready'], 40)
        check('unready_damage', v0 == p0 and sum(p40) >= 2 * sum(v40) - 2 and sum(v40) > 0, ready_vanilla=v0, ready_patched=p0, gauge40_vanilla=v40,
              gauge40_patched=p40)
    if want('levelup'):
        v, p = kill_party(V), kill_party(roms['mp'])
        check('levelup_mp', v['mp'][1:] == [0, 0] and p['mp'][1:] == p['maxmp'][1:] and p['maxmp'][1] > 0 and v['hp'] == p['hp'], vanilla=v, patched=p)
    if want('deadexp'):
        v, p = kill_party(V, hero1_hp=0), kill_party(roms['deadexp'], hero1_hp=0)
        live = kill_party(roms['deadexp'])
        check('dead_exp', v['exp'][1] == 0 and p['exp'][1] == v['exp'][2] and p['hp'][1] == 0 and live['hp'][1] > 0 and live['exp'][1] > 0,
              vanilla_dead_hero=v, patched_dead_hero=p, patched_live=live)
    if want('itemlimit'):
        v, p = give_candy(V), give_candy(roms['itemlimit'])
        check('item_limit', max(c for c, _ in v) == 4 and max(c for c, _ in p) == 7 and p[-1][1] == 2 and v[-1][1] == 2, vanilla=v, limit7=p)
    if want('magic'):
        out = {}
        for k in ('magic2', 'magic3'):
            r = roms[k]
            mult = int(k[-1])
            out[k] = [(spell_progress(r, lv), spell_progress(r, lv, False)) for lv in range(8)]
        van = [(spell_progress(V, lv), spell_progress(V, lv, False)) for lv in range(8)]
        ok = all(out['magic2'][lv][0] == 2 * van[lv][0] and out['magic3'][lv][0] == 3 * van[lv][0] for lv in range(8))
        ok = ok and all(out['magic2'][lv][1] >= van[lv][1] for lv in range(8))
        check('magic_rate', ok, vanilla=van, x2=out['magic2'], x3=out['magic3'])
    if want('other_roms'):
        for extra in filter(None, os.environ.get('SOM_QOL_EXTRA_ROMS', '').split(':')):
            name = pathlib.Path(extra).stem
            full = {**quiet, 'qol_fast_fades': True, 'qol_fast_transitions': True, 'qol_scroll_deadzone': 'tight', 'qol_unready_damage': True}
            pr = H.build_rom(extra, full, 'x_' + name)
            b = H.run_newgame(extra, 2400, 100)['trace']
            t = H.run_newgame(pr, 2400, 100)['trace']
            check('other_rom_boots_' + name, len(t) == len(b) and trace_equal(b[:10], t[:10]), first_divergence=first_divergence(b, t))
    if want('rando'):
        import som_gameplay as sg
        for extra in filter(None, os.environ.get('SOM_QOL_EXTRA_ROMS', '').split(':')):
            raw = pathlib.Path(extra).read_bytes()
            body = raw[sg.split_header(raw):]
            R = sg.resolve(body)
            name = pathlib.Path(extra).stem
            if not (R['rando_mp'] or R['rando_magic']):
                continue                                   # not a randomizer image (Magno): covered by other_roms
            opts = {'qol_dead_exp': True, 'qol_levelup_mp_restore': True, 'qol_magic_level_rate': 3, 'qol_chest_fast': True, 'qol_item_limit': 7}
            work = bytearray(body)
            applied, conflicts, bad = sg.apply_recs(work, sg.plan(opts, body), 'skip')
            mine = {r.opt for r, _ in applied}
            prog = [spell_progress(extra, lv) for lv in range(8)]
            van = [spell_progress(V, lv) for lv in range(8)]
            check('rando_compose_' + name, not bad and R['rando_mp'] and R['rando_magic'] and 'qol_dead_exp' in mine
                  and 'qol_levelup_mp_restore' not in mine and 'qol_magic_level_rate' not in mine and prog == [4 * x for x in van],
                  hooks=dict(mp=R['rando_mp'], magic=R['rando_magic']), skipped=bad, progress_x4=prog == [4 * x for x in van], rando_progress=prog)
    if a.report:
        pathlib.Path(a.report).write_text(json.dumps(res, indent=1))
    print('FAILED: %s' % fails if fails else 'all checks passed')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
