#!/usr/bin/env python3
"""Automated overflow and overlap test of the companion UI, run under all three layouts (3DS 320x240, Thor landscape 1240x1080,
Thor portrait 1080x1240) and with the ROM names of every ROM variant (vanilla US, Magno, Ancient Cave seeds 1 and 777).

The renderer counts, while drawing, every text that had to be cut, every label that fits neither one nor two lines in either type
face of its cell, every badge wider than its slot and every pair of card or avatar text rows that overlap (som_ux_audit).  This test
puts every name the UI can show into a cell and fails on the first non-zero audit:
  * every weapon type at every tier (names from the ROM name block), the item grid with all 11 items,
  * the spirit grid and the spell grid of every spirit for the girl and the sprite,
  * the ring info panel for every spell id 0..41, every spirit, weapon, item and config entry,
  * the quick-slot strip with every kind of entry and badges, the quick-slot editor (every picker page and slot), shoulder slot labels,
  * the hero cards and avatars in the danger, KO, poison, ailment and large-number states,
  * the target panel with every enemy name.
Writes <state>/ux/evidence/text_fit.json and exits 1 on any problem.  Missing ROMs are skipped and reported."""
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import common  # noqa: E402
import ux_lib  # noqa: E402

ST = common.STATE
W = ST / 'ux' / 'wram'
PACK = ST / 'assets' / 'som_ui_pack.bin'
DEV = {'3ds': (320, 240), 'thor': (1240, 1080), 'thorp': (1080, 1240),
       # sizes the Android view really gets: the Thor bitmap loses the navigation bar (1240x970 is form 0, the compact three-in-a-row
       # cards at 4x, which the plain panel sizes never reach), plus nearby heights on both sides of every form threshold
       'thor_nav': (1240, 970), 'thor_nav2': (1240, 940), 'thor_edge': (1240, 1000), 'thor_edge2': (1240, 1004),
       'thorp_nav': (1080, 1160), 'thorp_nav2': (1080, 1100)}
ROMS = [('vanilla', common.ROM_VANILLA), ('magno', common.ROM_MAGNO), ('ac_seed1', ST / 'ancientcave/out/ac_seed1.smc'),
        ('ac_seed777', ST / 'ancientcave/out/ac_seed777.smc')]


def load(n):
    return bytearray((W / (n + '.bin')).read_bytes())


def le(v, n=2):
    return v.to_bytes(n, 'little')


class Run:
    def __init__(self, name, rom):
        self.name = name
        self.u = ux_lib.UX(rom=rom, pack=PACK if PACK.exists() else None)
        self.draws = 0
        self.problems = []

    def check(self, label, w, view=(0, 0, 255), pending=None, opts=0):
        self.u.update(bytes(w))
        for dev, (dw, dh) in DEV.items():
            self.u.view(*view)
            if pending:
                self.u.pending(*pending)
            else:
                self.u.pending(0, 0, 0, 0)
            self.u.options(opts)
            n, note = self.u.draw_audit(dw, dh)
            self.draws += 1
            if n:
                self.problems.append('%s %s %s: %d (%s)' % (self.name, label, dev, n, note))
        self.u.options(0)


def hero_base(i):
    return 0xE000 + 0x200 * i


def scenario(r):
    u = r.u
    for i in range(8):
        u.quick(i, 0, 0)
    # weapons: every type at every tier
    for tier in range(9):
        w = load('field_215')
        for t in range(8):
            w[0xCC54 + t] = tier
        for hero in range(3):
            r.check('weapons tier %d hero %d' % (tier, hero), w, (0, hero, 255))
    # items: 11 distinct ids
    w = load('field_215')
    for rec in range(11):
        w[0xCC48 + rec] = (3 << 5) | rec
    r.check('items', w, (1, 0, 255))
    # spirits and spells for the girl and the sprite
    w = load('field_215')
    for hero in (1, 2):
        for e in range(8):
            w[hero_base(hero) + 0x1C8 + e] = 7
    for hero in (1, 2):
        r.check('spirits hero %d' % hero, w, (2, hero, 255))
        for sp in range(8):
            r.check('spells hero %d spirit %d' % (hero, sp), w, (2, hero, sp))
    # target panel with every enemy name
    base = load('field_222')
    for first in range(0, 96, 6):
        w = bytearray(base)
        for k in range(6):
            b = 0xE000 + 0x200 * (3 + k)
            w[b] = 1
            w[b + 0x180] = first + k
            w[b + 0x182:b + 0x184] = le(50)
            w[b + 0x184:b + 0x186] = le(100)
        r.check('enemies %d..%d' % (first, first + 5), w, (2, 1, 255), pending=(2, 3, 0, 0))
    # target grids: 1..9 enemies with the longest names, allies, group choice, and the zone map tab
    base = load('field_222')
    for n_en in (1, 2, 3, 5, 6, 9):
        w = bytearray(base)
        for k in range(n_en):
            b = 0xE000 + 0x200 * (3 + k)
            w[b] = 1
            w[b + 0x180] = (k * 11 + 7) % 96
            w[b + 0x182:b + 0x184] = le(3000 + k)
            w[b + 0x184:b + 0x186] = le(9999)
        r.check('enemy grid %d' % n_en, w, (2, 1, 255), pending=(2, 3, 0, 0))
        r.check('enemy grid %d one' % n_en, w, (2, 1, 255), pending=(2, 4, 0, 0))
    for cls in (1, 2, 5, 6, 7):
        r.check('ally cards class %d' % cls, load('field_215'), (1, 0, 255), pending=(1, cls, 0, 0))
    for src in ('field_215', 'field_222', 'field_232'):
        r.check('map tab ' + src, load(src), (3, 0, 255))
    r.check('locked event dim', load('event_line0'))
    # ring info: every entry of every ring, every spell id
    for src in ('ring_weapon_o0', 'ring_item_o0', 'ring_config_o0', 'ring_magic_o1'):
        base = load(src)
        for cur in range(8):
            w = bytearray(base)
            w[0x180A] = cur
            r.check('%s cursor %d' % (src, cur), w)
    base = load('ring_spell_o1')
    for sid in range(42):
        for owner in (1, 2):
            w = bytearray(base)
            w[0x1839] = sid
            w[0x1808] = owner
            r.check('spell sub-ring id %d owner %d' % (sid, owner), w)
    # quick slots with every kind, long badges; editor pages; bindings list
    w = load('field_215')
    kinds = [('WEAPON', 1), ('ITEM', 8), ('SPELL', 41), ('SPIRIT', 2), ('MENU', 2), ('SUBMENU', 3), ('ITEM', 10), ('SPELL', 38)]
    for i, k in enumerate(kinds):
        u.quick(i, *k)
    r.check('quick strip full', w, (0, 0, 255))
    r.check('ring with quick strip', load('ring_weapon_o0'))
    u.shoulder_labels('ZL', 'ZR', 'L3', 'R3')      # the widest labels a host could pass
    r.check('quick strip, long shoulder labels', w, (0, 0, 255))
    u.shoulder_labels()
    r.check('options main (reserved bindings page)', w, opts=2)
    for cat, n in enumerate((11, 8, 11, 8, 7)):
        u.quick(0, 0, 0)
        for page in range((n + 7) // 8):
            u.options(3)
            u.update(bytes(w))
            for dev, (dw, dh) in DEV.items():
                ux = u
                ux.lib.som_ux_open_options(ux.p, 3)
                # select picker category and page through taps
                ux.tap_hit(dw, dh, 'QE_TAB', cat)
                for _ in range(page):
                    ux.tap_hit(dw, dh, 'QE_PAGE', 1)
                n_, note = ux.draw_audit(dw, dh)
                r.draws += 1
                if n_:
                    r.problems.append('%s editor cat %d page %d %s: %d (%s)' % (r.name, cat, page, dev, n_, note))
                ux.lib.som_ux_open_options(ux.p, 0)
    editor_screens(r, w)
    revive_screens(r)
    for i in range(8):
        u.quick(i, 0, 0)
    # options panel with the longest value texts
    u.option(ux_lib.OPT['CORE'], 2, 'SNES9X')
    u.option(ux_lib.OPT['LANGUAGE'], 3, 'ESPANOL')
    r.check('options', load('field_215'), opts=1)
    # hero cards and avatars in every state
    states = [
        ('full', 524, 524, 47, 47, 0), ('three digits', 999, 999, 99, 99, 0), ('map 215 device', 523, 524, 47, 47, 0), ('real four digits', 9999, 9999, 255, 255, 0),
        ('four digits low', 1000, 9999, 100, 255, 0), ('four digits ailments', 9998, 9999, 255, 255, 0x2105), ('low', 100, 524, 9, 47, 0), ('critical', 30, 524, 0, 47, 0),
        ('ko', 0, 524, 0, 47, 0x8000), ('poison', 250, 412, 47, 47, 0x2000), ('ailments', 250, 412, 10, 47, 0x0105),
        ('poison and critical', 30, 412, 5, 47, 0x2000), ('everything', 30, 999, 99, 99, 0x2105)]
    for label, hp, hpm, mp, mpm, st in states:
        for scene in ('field_215', 'ring_weapon_o0', 'shop_buy'):
            w = load(scene)
            for hero in range(3):
                b = hero_base(hero)
                w[b + 0x182:b + 0x184] = le(hp)
                w[b + 0x184:b + 0x186] = le(hpm)
                w[b + 0x186] = mp
                w[b + 0x187] = mpm
                w[b + 0x190:b + 0x192] = le(st)
            r.check('cards %s in %s' % (label, scene), w)
    r.check('event party', load('event_line0'))


def audit_dev(r, label):
    for dev, (dw, dh) in DEV.items():
        n, note = r.u.draw_audit(dw, dh)
        r.draws += 1
        if n:
            r.problems.append('%s %s %s: %d (%s)' % (r.name, label, dev, n, note))


def editor_screens(r, w):
    """Second level of the spell picker, the target-policy chooser for every spell and item, and the bind screen for every kind."""
    u = r.u
    u.update(bytes(w))
    for dev, (dw, dh) in DEV.items():
        for sp in range(7):
            u.options(3)
            u.tap_hit(dw, dh, 'QE_TAB', 4)
            u.tap_hit(dw, dh, 'QE_CELL', sp)
            n, note = u.draw_audit(dw, dh)
            r.draws += 1
            if n:
                r.problems.append('%s spell picker spirit %d %s: %d (%s)' % (r.name, sp, dev, n, note))
            for i in range(6):
                u.tap_hit(dw, dh, 'QE_CELL', i)
                if u.center(dw, dh, 'POLICY', 0):                          # chooser opened for this spell
                    n, note = u.draw_audit(dw, dh)
                    r.draws += 1
                    if n:
                        r.problems.append('%s chooser spell %d %s: %d (%s)' % (r.name, sp * 6 + i, dev, n, note))
                    u.tap_hit(dw, dh, 'POL_CANCEL', 0)
            u.options(0)
        u.options(3)
        u.tap_hit(dw, dh, 'QE_TAB', 2)
        for i in range(8):
            u.tap_hit(dw, dh, 'QE_CELL', i)
            if u.center(dw, dh, 'POLICY', 0):
                n, note = u.draw_audit(dw, dh)
                r.draws += 1
                if n:
                    r.problems.append('%s chooser item %d %s: %d (%s)' % (r.name, i, dev, n, note))
                u.tap_hit(dw, dh, 'POL_CANCEL', 0)
        u.options(0)
    for slot in range(8):                                   # slot editor header for every slot (SLOT L1 ... SLOT 4)
        u.lib.som_ux_open_options(u.p, 3)
        u.tap_hit(*DEV[next(iter(DEV))], 'QUICK', slot)
        audit_dev(r, 'slot editor slot %d' % slot)
        u.lib.som_ux_open_options(u.p, 0)


def revive_screens(r):
    """REVIVE chooser for every fallen hero (field card and ring avatar), with and without Cups and MP, queued and disabled spell states."""
    u = r.u
    for dead in range(3):
        for variant in ('cups', 'nocups', 'queued', 'nomp'):
            w = load('field_215')
            b = hero_base(dead)
            w[b + 0x182:b + 0x184] = le(0)
            w[b + 0x190:b + 0x192] = le(0x8000)
            if variant == 'nocups':
                for i in range(11):
                    w[0xCC48 + i] = 0
            else:
                w[0xCC48 + 5] = (7 << 5) | 5
            if variant == 'nomp':
                w[hero_base(1) + 0x186] = 0
            u.update(bytes(w))
            u.spell_avail({33: 3} if variant == 'queued' else None)
            for dev, (dw, dh) in DEV.items():
                u.view(0, 0, 255)
                u.pending(0, 0, 0, 0)
                if not u.tap_hit(dw, dh, 'PARTY', dead) and not u.center(dw, dh, 'PARTY', dead):
                    continue
                n, note = u.draw_audit(dw, dh)
                r.draws += 1
                if n:
                    r.problems.append('%s revive hero %d %s %s: %d (%s)' % (r.name, dead, variant, dev, n, note))
                u.tap_hit(dw, dh, 'REV_CANCEL', 0)
            u.spell_avail(None)
    # spell lists and slots with queued / disabled states
    w = load('field_215')
    u.update(bytes(w))
    for i, sp in enumerate((33, 34, 35, 3, 4, 8, 9, 41)):
        u.quick(i, 'SPELL', sp)
    u.spell_avail({33: 3, 34: 2, 3: 3, 41: 3, 8: 2})
    for dev, (dw, dh) in DEV.items():
        for hero, spirit in ((1, 5), (1, 0), (2, 5)):
            u.view(2, hero, spirit)
            n, note = u.draw_audit(dw, dh)
            r.draws += 1
            if n:
                r.problems.append('%s spell states hero %d spirit %d %s: %d (%s)' % (r.name, hero, spirit, dev, n, note))
        u.view(0, 0, 255)
        n, note = u.draw_audit(dw, dh)
        r.draws += 1
        if n:
            r.problems.append('%s slot states %s: %d (%s)' % (r.name, dev, n, note))
    u.spell_avail(None)
    for i in range(8):
        u.quick(i, 0, 0)


def main():
    out = {}
    bad = 0
    if '--sweep' in sys.argv:       # every panel height a device may report: find where a form needs a different layout
        DEV.clear()
        for h in range(932, 1101, 4):
            DEV['l%d' % h] = (1240, h)
        for h in range(1100, 1301, 4):
            DEV['p%d' % h] = (1080, h)
        ROMS[:] = ROMS[:1]
    for name, rom in ROMS:
        if not rom.exists():
            out[name] = {'skipped': 'missing rom'}
            print(name, 'skipped')
            continue
        r = Run(name, rom)
        scenario(r)
        out[name] = {'draws': r.draws, 'problems': r.problems[:40], 'n_problems': len(r.problems)}
        bad += len(r.problems)
        print(name, 'draws', r.draws, 'problems', len(r.problems), flush=True)
        if '--sweep' in sys.argv:
            agg = {}
            seen = set()
            for p in r.problems:
                dev = p.split(':')[0].split()[-1]
                kind = p.split('(')[1].split(':')[0]
                agg.setdefault(dev, set()).add(kind)
                if dev in ('l1080', 'p1240', 'l960', 'l1040', 'p1160') and kind not in seen:
                    seen.add(kind); print('      sample', p)
            for dev in sorted(agg, key=lambda d: (d[0], int(d[1:]))):
                print('   ', dev, sorted(agg[dev]))
        for p in r.problems[:12]:
            print('   ', p)
    ev = ST / 'ux' / 'evidence'
    ev.mkdir(parents=True, exist_ok=True)
    (ev / 'text_fit.json').write_text(json.dumps(out, indent=1))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
