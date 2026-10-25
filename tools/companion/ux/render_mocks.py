#!/usr/bin/env python3
"""Renders one PNG per UI mode and device from real WRAM captures (capture_wram.py) into <state>/ux/mocks/.

Devices: 3ds 320x240, thor 1240x1080 (landscape), thorp 1080x1240 (portrait).  Uses the graphics pack when present
(<state>/assets/som_ui_pack.bin) and the user's ROM for names and the world map.  Nothing here is committed."""
import ctypes as C
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import common  # noqa: E402
import ux_lib  # noqa: E402
from PIL import Image  # noqa: E402

W = common.STATE / 'ux' / 'wram'
import os  # noqa: E402
OUT = pathlib.Path(os.environ.get('SOM_UX_MOCKS', str(common.STATE / 'ux' / 'mocks')))
PACK = common.STATE / 'assets' / 'som_ui_pack.bin'
DEV = {'3ds': (320, 240), 'thor': (1240, 1080), 'thorp': (1080, 1240),
       # the sizes the Android view really gets once the navigation bar is taken off (1240x970 is the compact three-in-a-row layout)
       'thor_nav': (1240, 970), 'thorp_nav': (1080, 1160)}


def load(n):
    return (W / (n + '.bin')).read_bytes()


def patch(w, edits):
    w = bytearray(w)
    for a, v in edits:
        w[a:a + len(v)] = v
    return bytes(w)


def le(v, n=2):
    return v.to_bytes(n, 'little')


GLYPH = {' ': 0x80}
GLYPH.update({chr(97 + i): 0x81 + i for i in range(26)})
GLYPH.update({chr(65 + i): 0x9B + i for i in range(26)})
GLYPH.update({str(i): 0xB5 + i for i in range(10)})


def enc(s):
    """Game glyph codes of a name (see docs/text.md): lower case 0x81.., upper case 0x9B.., digits 0xB5..."""
    return bytes(GLYPH[c] for c in s)


def hurt(w):
    """Mock variation: wounded boy, nearly dead girl, sprite poisoned (HP and status words only)."""
    return patch(w, [(0xE182, le(310)), (0xE182 + 0x200, le(88)), (0xE186 + 0x200, bytes([9])), (0xE182 + 0x400, le(250)),
                     (0xE190 + 0x400, le(0x2000)), (0xE1ED, bytes([0x30])), (0xE1ED + 0x200, bytes([0x00]))])


def danger(w):
    """Mock variation: boy critical (below 10 percent), girl down, sprite low and poisoned."""
    return patch(w, [(0xE182, le(40)), (0xE182 + 0x200, le(0)), (0xE190 + 0x200, le(0x8000)), (0xE182 + 0x400, le(80)),
                     (0xE190 + 0x400, le(0x2100)), (0xE1ED, bytes([0x30])), (0xE1ED + 0x200, bytes([0x00]))])


# hotlinks shown in the filled scenes: (kind, id) per slot, None = empty "+"
# slots 0..3 are the shoulder slots L1 R1 L2 R2 (bound by position), 4..7 the free touch slots
QUICK = [('SPELL', 3), ('ITEM', 0), ('WEAPON', 1), ('MENU', 3), ('SPIRIT', 4), ('SUBMENU', 2), ('SUBMENU', 0), None]
QUICK_FILLED = {'field_quick', 'field_quick_danger', 'ring_weapon_filled', 'ring_item_armed', 'quick_editor', 'slots_filled', 'slots_armed', 'slot_flash',
                'quick_editor_menu', 'quick_editor_menu2'}
OPTION_VALUES = [('WIDE', 1, 'ON'), ('FALLBACK', 0, None), ('HUD_HIDE', 0, None), ('CORE', 2, 'SNES9X'), ('UI_SOUNDS', 1, None),
                 ('HAPTICS', 1, None), ('LANGUAGE', 0, 'ENGLISH')]
OPEN = {'options_main': 1, 'quick_editor': 3, 'quick_editor_menu': 3, 'quick_editor_menu2': 3}
ARM = {'ring_item_armed': 'BIND_ENTRY'}

SCENES = {
    # name: (wram builder, tab, hero, spirit, pending(kind, cls, arg, spell) or None, devices)
    'field_device': ('field_215', 0, 0, 255, None, ('3ds', 'thor', 'thor_nav', 'thorp', 'thorp_nav')),   # map 215, HP 523/524 ...
    'field_hp4': ('field_215', 0, 0, 255, None, ('3ds', 'thor', 'thor_nav', 'thorp', 'thorp_nav')),     # 4-digit HP, 3-digit MP
    'field_weapon': ('field_215', 0, 0, 255, None, ('3ds', 'thor', 'thorp')),
    'field_item': ('field_215', 1, 0, 255, None, ('3ds', 'thor', 'thorp')),
    'field_magic': ('field_215', 2, 1, 255, None, ('3ds', 'thor')),
    'field_spells': ('field_215', 2, 1, 0, None, ('3ds', 'thor')),
    'field_map': ('field_222', 3, 0, 255, None, ('3ds', 'thorp')),
    'field_enemies': ('field_222', 0, 0, 255, None, ('thor',)),
    'target_ally': ('field_215', 1, 0, 255, (1, 1, 0, 0), ('3ds', 'thor', 'thorp', 'thor_nav')),
    'target_ally_all': ('field_215', 2, 1, 255, (2, 2, 0, 0), ('3ds', 'thor', 'thorp')),
    'target_enemy': ('enemies_222', 2, 1, 255, (2, 3, 0, 0), ('3ds', 'thor', 'thorp', 'thor_nav')),
    'target_enemy_215': ('enemies_215', 2, 1, 255, (2, 3, 0, 0), ('3ds', 'thor')),
    'target_enemy_228': ('enemies_228', 2, 1, 255, (2, 3, 0, 0), ('3ds', 'thor')),
    'target_enemy_232': ('enemies_232', 2, 1, 255, (2, 4, 0, 0), ('3ds', 'thor')),
    'target_enemy_nopic': ('enemies_222', 2, 1, 255, (2, 3, 0, 0), ('thor',)),
    'policy_enemy': ('field_215', 0, 0, 255, None, ('3ds', 'thor', 'thorp')),
    'policy_ally': ('field_215', 0, 0, 255, None, ('3ds', 'thor', 'thorp')),
    'picker_spirits': ('field_215', 0, 0, 255, None, ('3ds', 'thor', 'thorp')),
    'picker_spells': ('field_215', 0, 0, 255, None, ('3ds', 'thor', 'thorp')),
    'revive': ('field_215', 0, 0, 255, None, ('3ds', 'thor', 'thorp', 'thor_nav')),
    'revive_grey': ('field_215', 0, 0, 255, None, ('3ds', 'thor')),
    'revive_queued': ('field_215', 0, 0, 255, None, ('thor',)),
    'revive_ring': ('ring_weapon_o1', 0, 0, 255, None, ('thor',)),
    'spells_states': ('field_215', 2, 1, 5, None, ('3ds', 'thor', 'thorp')),
    'slots_states': ('field_215', 0, 0, 255, None, ('3ds', 'thor', 'thorp')),
    'notice_queued': ('field_215', 0, 0, 255, None, ('3ds', 'thor', 'thorp')),
    'notice_cancelled': ('field_215', 2, 1, 5, None, ('thor',)),
    'notice_dropped': ('field_215', 0, 0, 255, None, ('3ds', 'thor', 'thorp')),
    'slot_editor': ('field_215', 0, 0, 255, None, ('3ds', 'thor')),
    'pressed_tab': ('field_215', 0, 0, 255, None, ('3ds', 'thor')),
    'pressed_cell': ('field_215', 0, 0, 255, None, ('thor',)),
    'pressed_target': ('enemies_222', 2, 1, 255, (2, 3, 0, 0), ('thor',)),
    'map_215': ('field_215', 3, 0, 255, None, ('3ds', 'thor', 'thorp')),
    'map_232': ('field_232', 3, 0, 255, None, ('3ds', 'thor', 'thorp')),
    'map_222': ('field_222', 3, 0, 255, None, ('3ds', 'thor', 'thorp')),
    'map_215_small': ('field_215', 3, 0, 255, None, ('3ds', 'thor', 'thorp')),
    'map_222_small': ('enemies_222', 3, 0, 255, None, ('3ds', 'thor', 'thorp')),
    'map_232_small': ('field_232', 3, 0, 255, None, ('3ds', 'thor', 'thorp')),
    'field_boss': ('field_232', 0, 0, 255, None, ('3ds', 'thor')),
    'ring_weapon': ('ring_weapon_o0', 0, 0, 255, None, ('3ds', 'thor', 'thorp')),
    'ring_weapon_filled': ('ring_weapon_o0', 0, 0, 255, None, ('3ds', 'thor', 'thor_nav', 'thorp', 'thorp_nav')),
    'ring_item': ('ring_item_o0', 0, 0, 255, None, ('3ds', 'thor')),
    'ring_item_armed': ('ring_item_o0', 0, 0, 255, None, ('3ds', 'thor')),
    'ring_spirit': ('ring_magic_o1', 0, 0, 255, None, ('3ds', 'thor', 'thorp')),
    'ring_spell': ('ring_spell_o1', 0, 0, 255, None, ('3ds', 'thor', 'thorp')),
    'ring_config': ('ring_config_o0', 0, 0, 255, None, ('3ds', 'thor')),
    'field_quick': ('field_215', 0, 0, 255, None, ('3ds', 'thor', 'thor_nav', 'thorp', 'thorp_nav')),
    'field_quick_danger': ('field_215', 2, 1, 255, None, ('thor',)),
    'options_main': ('field_215', 0, 0, 255, None, ('3ds', 'thor', 'thor_nav', 'thorp', 'thorp_nav')),
    'quick_editor': ('field_215', 0, 0, 255, None, ('3ds', 'thor', 'thor_nav', 'thorp', 'thorp_nav')),
    'quick_editor_menu': ('field_215', 0, 0, 255, None, ('3ds', 'thor_nav', 'thorp_nav')),     # MENU picker: ring categories and config screens with their icons
    'quick_editor_menu2': ('field_215', 0, 0, 255, None, ('3ds', 'thor_nav')),
    'slots_empty': ('field_215', 0, 0, 255, None, ('3ds', 'thor_nav', 'thorp_nav')),
    'slots_filled': ('field_215', 0, 0, 255, None, ('3ds', 'thor_nav', 'thorp_nav')),
    'slots_armed': ('field_215', 0, 0, 255, None, ('3ds', 'thor_nav', 'thorp_nav')),
    'slots_3ds_labels': ('field_215', 0, 0, 255, None, ('3ds',)),
    'slot_flash': ('field_215', 2, 1, 0, None, ('3ds', 'thor_nav', 'thorp_nav')),     # frame-sequence strip, see flash_strips()
    'names_custom': ('field_215', 0, 0, 255, None, ('3ds', 'thor_nav', 'thorp_nav')),
    'mp_full': ('field_215', 0, 0, 255, None, ('3ds', 'thor_nav', 'thorp_nav')),
    'mp_low': ('field_215', 0, 0, 255, None, ('3ds', 'thor_nav', 'thorp_nav')),
    'mp_zero': ('field_215', 0, 0, 255, None, ('3ds', 'thor_nav', 'thorp_nav')),
    'shop_buy': ('shop_buy', 0, 0, 255, None, ('3ds', 'thor')),
    'shop_sell': ('shop_sell', 0, 0, 255, None, ('3ds', 'thor')),
    'transition': ('transition', 0, 0, 255, None, ('3ds', 'thor')),
    'flammie': ('flammie_potos', 0, 0, 255, None, ('3ds', 'thor', 'thorp')),
    'flammie_otherworld': ('flammie_pureland', 0, 0, 255, None, ('3ds', 'thor')),
    'field_danger': ('field_215', 0, 0, 255, None, ('3ds', 'thor', 'thorp')),
    'boot': ('boot', 0, 0, 255, None, ('3ds', 'thor')),
    'frontend_select': ('frontend_select', 0, 0, 255, None, ('3ds', 'thor')),
    'frontend_name': ('frontend_name', 0, 0, 255, None, ('3ds', 'thor')),
}


AVAIL = {'spells_states': {33: 3, 34: 2}, 'slots_states': {33: 3, 34: 2}, 'revive_queued': {33: 3}}


def hook(u, name, d):
    """Scene-specific UI state set through real taps on the device `d` (the same path the player takes); returns a cleanup callable."""
    w, h = DEV[d]
    if name in ('policy_enemy', 'policy_ally', 'picker_spirits', 'picker_spells'):
        u.options(3)
        u.tap_hit(w, h, 'QE_TAB', 4)
        if name == 'picker_spirits':
            return lambda: u.options(0)
        if name == 'picker_spells':
            u.tap_hit(w, h, 'QE_CELL', 0)
            return lambda: (u.tap_hit(w, h, 'QE_BACK', 0), u.options(0))
        want = 1 if name == 'policy_enemy' else 3                       # NEAR for enemy spells, LESS_HP for ally spells
        for sp in range(7):
            u.tap_hit(w, h, 'QE_CELL', sp)
            for i in range(6):
                u.tap_hit(w, h, 'QE_CELL', i)
                if u.center(w, h, 'POLICY', want):
                    return lambda: (u.tap_hit(w, h, 'POL_CANCEL', 0), u.tap_hit(w, h, 'QE_BACK', 0), u.options(0))
                if u.center(w, h, 'POLICY', 0):
                    u.tap_hit(w, h, 'POL_CANCEL', 0)
            u.tap_hit(w, h, 'QE_BACK', 0)
        print('no chooser for', name)
        return lambda: u.options(0)
    if name in AVAIL:
        u.spell_avail(AVAIL[name])
        if name == 'slots_states':
            for i, sp in enumerate((33, 34, 35, 3, 4)):
                u.quick(i, 'SPELL', sp)
        cl = [lambda: u.spell_avail(None)]
        if name == 'revive_queued':
            u.tap_hit(w, h, 'PARTY', 2)
            cl.append(lambda: u.tap_hit(w, h, 'REV_CANCEL', 0))
        if name == 'slots_states':
            cl.append(lambda: [u.quick(i, 0, 0) for i in range(8)])
        return lambda: [f() for f in cl]
    if name in ('revive', 'revive_grey'):
        u.tap_hit(w, h, 'PARTY', 2)
        return lambda: u.tap_hit(w, h, 'REV_CANCEL', 0)
    if name == 'revive_ring':
        u.tap_hit(w, h, 'AVATAR', 2)
        return lambda: u.tap_hit(w, h, 'REV_CANCEL', 0)
    if name.startswith('notice_'):
        k = {'queued': 1, 'cancelled': 2, 'dropped': 3}[name.split('_')[1]]
        u.notice(k, 'NO MP' if k == 3 else None)
        return lambda: u.lib.som_ux_notice_clear(u.p)
    if name in ('quick_editor_menu', 'quick_editor_menu2'):
        u.options(3)
        u.tap_hit(w, h, 'QE_TAB', 0)
        if name.endswith('2'):
            u.tap_hit(w, h, 'QE_PAGE', 1)
        return lambda: u.options(0)
    if name == 'slots_armed':
        u.view(1, 0, 255)
        c = u.center(w, h, 'CELL', 2)
        u.longpress(c[0], c[1], w, h)
        return lambda: (u.tap_hit(w, h, 'TAB', 0), u.lib.som_ux_notice_clear(u.p))
    if name == 'slot_editor':
        u.options(3)
        return lambda: u.options(0)
    if name == 'pressed_tab':
        u.press('TAB', 1)
        return lambda: u.press('TAB', 1, False)
    if name == 'pressed_cell':
        u.press('CELL', 1)
        return lambda: u.press('CELL', 1, False)
    if name == 'pressed_target':
        u.press('T_ALL_ENEMIES', 0)
        return lambda: u.press('T_ALL_ENEMIES', 0, False)
    if name.endswith('_small') and name.startswith('map_'):
        u.tap_hit(w, h, 'MAPVIEW', 0)
        return lambda: u.tap_hit(w, h, 'MAPVIEW', 0)
    return None


def flash_strip(u, wram, d, path):
    """Frame sequence of the ~0.3 s slot feedback: a spell is cast from the spell list, so the L1 slot holding it and the spell button of the list
    flash together.  Frames at 0, 3, 6, 9, 12, 15 and 17 of 18 (60 fps), each crop scaled to a common height, slot on top, list button below."""
    from PIL import Image
    w, h = DEV[d]
    c0, c1 = u.center(w, h, 'QUICK', 0), u.center(w, h, 'QUICK', 2)
    cb = u.center(w, h, 'CELL', 0)
    s = ux_lib.UX.scale(w, h) if hasattr(ux_lib.UX, 'scale') else max(1, (min(w, h) + 125) // 250)
    sw, sh_ = 36 * s, 34 * s
    rows = []
    marks = (0, 3, 6, 9, 12, 15, 17)
    u.flash_entry(2, 3, 3)                                  # CAST, spell 3
    frames = []
    for f in range(18):
        if f in marks:
            im, raw = u.draw(w, h)
            frames.append(im)
        u.update(wram)
    for centre, (cw, ch) in ((c0, (sw, sh_)), (cb, (max(sw, 2 * s * 40), 40 * s))):
        crops = []
        for im in frames:
            x0 = max(0, min(w - cw, centre[0] - cw // 2)); y0 = max(0, min(h - ch, centre[1] - ch // 2))
            crops.append(im.crop((x0, y0, x0 + cw, y0 + ch)))
        rows.append(crops)
    zoom = 2 if s < 3 else 1
    cwid = max(r[0].width for r in rows) * zoom
    out = Image.new('RGB', ((cwid + 6) * len(marks) + 6, sum(r[0].height * zoom + 6 for r in rows) + 6), (20, 20, 30))
    y = 6
    for crops in rows:
        for i, cim in enumerate(crops):
            out.paste(cim.resize((cim.width * zoom, cim.height * zoom), Image.NEAREST), (6 + i * (cwid + 6), y))
        y += crops[0].height * zoom + 6
    out.save(path)
    u.update(wram)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    rom = common.ROM_VANILLA
    u = ux_lib.UX(rom=rom, pack=PACK if PACK.exists() else None)
    done = {}
    for name, (src, tab, hero, spirit, pend, devs) in SCENES.items():
        w = load(src)
        if name == 'field_device':
            w = patch(w, [(0xE182 + 0x200 * i, le(v)) for i, v in enumerate((523, 416, 411))])
        elif name == 'field_hp4':
            w = patch(w, [(0xE182 + 0x200 * i + o, le(9999 - i)) for i in range(3) for o in (0, 2)] +
                      [(0xE186 + 0x200 * i, bytes([255, 255])) for i in range(3)])
        elif name == 'names_custom':
            w = patch(w, [(0xCC00 + 12 * i, enc(n) + b'\x00') for i, n in enumerate(('RANDI', 'Purim', 'POPOI'))])
        elif name.startswith('mp_'):
            mp = {'mp_full': (40, 50), 'mp_low': (3, 4), 'mp_zero': (0, 0)}[name]
            w = patch(w, [(0xE186 + 0x200, bytes([mp[0], 40])), (0xE186 + 0x400, bytes([mp[1], 50]))])
        elif name.startswith('revive'):
            w = patch(w, [(0xE182 + 0x400, le(0)), (0xE190 + 0x400, le(0x8000))])         # the sprite is down
            if name == 'revive_grey':
                w = patch(w, [(0xCC48 + i, bytes([0])) for i in range(11)] + [(0xE186 + 0x200, bytes([0]))])   # no Cup left, girl without MP
        elif src.startswith('field_'):
            w = danger(w) if name in ('field_danger', 'field_quick_danger') else hurt(w)
        u.shoulder_labels(*(('L', 'R', 'ZL', 'ZR') if name == 'slots_3ds_labels' else (None,) * 4))
        for i in range(8):
            u.quick(i, 0, 0)
        if name in QUICK_FILLED:
            for i, q in enumerate(QUICK):
                if q:
                    u.quick(i, *q)
        for ident, val, txt in OPTION_VALUES:
            u.option(ux_lib.OPT[ident], val, txt)
        u.options(0)
        u.update(w)
        mode = u.classify(w)
        if name == 'event':
            continue
        u.view(tab, hero, spirit)
        u.lib.som_ux_clear_enemy_sprites(u.p)
        if name.startswith('target_enemy') and name != 'target_enemy_nopic' or name == 'pressed_target':
            import numpy as np
            fr = W / (src + '.frame.npy')
            if fr.exists():
                u.sprites(np.load(fr), w)
        if pend:
            u.pending(*pend)
        else:
            u.pending(0, 0, 0, 0)
        if name in OPEN:
            u.options(OPEN[name])
        for d in devs:
            if name in ARM:
                u.options(0)
                u.tap_hit(*DEV[d], ARM[name])
            if name == 'slot_flash':
                flash_strip(u, w, d, OUT / ('slot_flash_%s.png' % d))
                continue
            cleanup = hook(u, name, d)
            im, raw = u.draw(*DEV[d])
            if cleanup:
                cleanup()
            p = OUT / ('%s_%s.png' % (name, d))
            im.save(p)
            done['%s_%s' % (name, d)] = {'mode': mode, 'src': src, 'min_target_vp': u.min_target(*DEV[d])}
            if name in ARM:
                u.tap_hit(*DEV[d], 'OPT')       # open and close the panel: clears the armed state
                u.tap_hit(*DEV[d], 'OPT')
                u.options(0)
    # event with a hurt party: static markers only
    for d in ('3ds', 'thor'):
        u.view(0, 0, 255)
        u.pending(0, 0, 0, 0)
        for i in range(4):
            u.update(danger(load('event_line%d' % i)))
        im, raw = u.draw(*DEV[d])
        im.save(OUT / ('event_danger_%s.png' % d))
    # event: feed several lines so the log fills
    for d in ('3ds', 'thor'):
        u.view(0, 0, 255)
        u.pending(0, 0, 0, 0)
        for i in range(4):
            w = load('event_line%d' % i)
            u.update(patch(w, []))
        im, raw = u.draw(*DEV[d])
        im.save(OUT / ('event_%s.png' % d))
        done['event_' + d] = {'mode': u.classify(load('event_line3'))}
    # startup: the very first frame of a fresh renderer, drawn before any game frame or touch
    for d in ('3ds', 'thor', 'thor_nav'):
        f = ux_lib.UX(rom=rom, pack=PACK if PACK.exists() else None)
        im, raw = f.draw(*DEV[d])
        im.save(OUT / ('startup_first_frame_%s.png' % d))
    # thin / pale sprite (spider-leg style) on the enemy card
    import numpy as np
    w = load('enemies_222')
    u.update(w)
    u.view(2, 1, 255)
    u.pending(2, 4, 0, 0)
    for eid in range(256):
        u.lib.som_ux_set_enemy_sprite.argtypes = [C.c_void_p, C.c_int, C.c_void_p, C.c_int, C.c_int]
    thin = np.zeros((40, 40), np.uint32)
    for i in range(40):
        thin[i, i] = thin[i, 39 - i] = 0x70D0D0D0
        thin[20, i] = 0x40C0C0C0
    buf = (C.c_uint32 * 1600)(*thin.ravel().tolist())
    ids = (C.c_uint8 * 16)()
    nw = u.lib.som_ux_enemy_sprites_wanted(u.p, ids, 16)
    for i in range(nw):
        u.lib.som_ux_set_enemy_sprite(u.p, ids[i], buf, 40, 40)
    for d in ('3ds', 'thor'):
        im, raw = u.draw(*DEV[d])
        im.save(OUT / ('target_thin_sprite_%s.png' % d))
    u.lib.som_ux_clear_enemy_sprites(u.p)
    u.pending(0, 0, 0, 0)
    json.dump(done, open(OUT / 'index.json', 'w'), indent=1)
    print(json.dumps(done, indent=1))


if __name__ == '__main__':
    main()
