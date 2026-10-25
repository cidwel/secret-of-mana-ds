"""ctypes binding of som_ui_draw (builds libsomux.so on demand) for the mock renderer and the mode checks."""
import ctypes as C
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import common  # noqa: E402

LIB = common.STATE / 'ux' / 'build' / 'libsomux.so'
SRC = [HERE / 'som_ui_draw.c', HERE / 'som_ui_state.c', HERE / 'som_ui_sel.c', HERE / 'som_ui_zone.c', HERE.parent / 'som_companion.c']
HDR = [HERE / 'som_ui_draw.h', HERE / 'som_ui_priv.h', HERE.parent / 'som_companion.h']
MODES = ['BOOT', 'FRONTEND', 'FIELD', 'EVENT', 'TRANSITION', 'RING', 'SHOP', 'FLAMMIE']
ACTS = ['NONE', 'CMD', 'PAD', 'NAME_LETTER', 'BIND_BEGIN', 'BIND_CLEAR', 'BIND_CANCEL', 'OPTION', 'AVATAR', 'OPEN_MENU',
        'QUICK_SET', 'QUICK_EDIT', 'CLOSE_RING', 'CANCEL_QUEUED', 'SWITCH_HERO', 'NAME_DELETE']
HIT = {n: i for i, n in enumerate(['NONE', 'PARTY', 'TAB', 'CELL', 'T_ALLY', 'T_ENEMY', 'T_ALL_ALLIES', 'T_ALL_ENEMIES', 'CANCEL',
                                   'PAD', 'LETTER', 'NEXT', 'KEYROW', 'KEYS_SCROLL', 'BIND_CANCEL', 'BIND_CLEAR', 'OPT', 'OPTION',
                                   'OPT_NAV', 'AVATAR', 'QUICK', 'BIND_ENTRY', 'BIND_MENU', 'QE_TAB', 'QE_CELL', 'QE_PAGE',
                                   'QE_CLEAR', 'QE_KEY', 'MAPVIEW', 'POLICY', 'POL_CANCEL', 'QE_BACK', 'REVIVE', 'REV_CANCEL'])}
BK = {'NONE': 0, 'WEAPON': 1, 'ITEM': 2, 'SPELL': 3, 'SPIRIT': 4, 'MENU': 5, 'SUBMENU': 6}
OPT = {'WIDE': 0, 'FALLBACK': 1, 'HUD_HIDE': 2, 'CORE': 3, 'UI_SOUNDS': 4, 'HAPTICS': 5, 'LANGUAGE': 6}


def build():
    LIB.parent.mkdir(parents=True, exist_ok=True)
    if not LIB.exists() or LIB.stat().st_mtime < max(p.stat().st_mtime for p in SRC + HDR):
        subprocess.run(['gcc', '-std=c99', '-Wall', '-Wextra', '-O2', '-fPIC', '-shared', '-o', str(LIB)] +
                       [str(s) for s in SRC] + ['-lm'], check=True)
    return LIB


class Cmd(C.Structure):
    _fields_ = [('kind', C.c_int), ('hero', C.c_uint8), ('arg', C.c_uint8), ('spell', C.c_uint8)]


class Action(C.Structure):
    _fields_ = [('kind', C.c_uint8), ('cmd', Cmd), ('target_kind', C.c_uint8), ('target_slot', C.c_uint8),
                ('pad', C.c_uint16), ('letter', C.c_uint8), ('bind_kind', C.c_uint8), ('bind_id', C.c_uint8),
                ('hero', C.c_uint8), ('slot', C.c_uint8), ('policy', C.c_uint8), ('opt_id', C.c_uint8), ('opt_val', C.c_uint8)]


class SelInfo(C.Structure):
    _fields_ = [('valid', C.c_uint8), ('kind', C.c_uint8), ('id', C.c_uint8), ('menu', C.c_uint8), ('owner', C.c_uint8),
                ('sub_ring', C.c_uint8), ('head', C.c_char * 26), ('title', C.c_char * 26), ('line', (C.c_char * 26) * 5),
                ('n', C.c_uint8)]


class UX:
    def __init__(self, rom=None, pack=None):
        self.lib = C.CDLL(str(build()))
        L = self.lib
        L.som_ux_new.restype = C.c_void_p
        L.som_ux_pack_load.restype = C.c_void_p
        L.som_ux_pack_load.argtypes = [C.c_char_p, C.c_size_t]
        L.som_ux_set_pack.argtypes = [C.c_void_p, C.c_void_p]
        L.som_ux_set_rom.argtypes = [C.c_void_p, C.c_char_p, C.c_size_t]
        L.som_ux_update.argtypes = [C.c_void_p, C.c_char_p]
        L.som_ux_classify.argtypes = [C.c_char_p]
        for fn in ('clear_enemy_sprites', 'set_enemy_sprite', 'set_spell_avail', 'press_hit', 'revive_open', 'revive_count'):
            getattr(L, 'som_ux_' + fn).argtypes = [C.c_void_p] + {'set_enemy_sprite': [C.c_int, C.c_void_p, C.c_int, C.c_int], 'set_spell_avail': [C.c_void_p],
                                                                   'press_hit': [C.c_int, C.c_int, C.c_int], 'revive_open': [C.c_int]}.get(fn, [])
        L.som_ux_draw.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p, C.c_int, C.c_int, C.c_int]
        L.som_ux_touch.argtypes = [C.c_void_p, C.c_int, C.c_int, C.c_int, C.c_int, C.c_void_p]
        L.som_ux_set_view.argtypes = [C.c_void_p, C.c_int, C.c_int, C.c_int]
        L.som_ux_set_pending.argtypes = [C.c_void_p] + [C.c_int] * 4
        L.som_ux_min_target_vp.argtypes = [C.c_void_p, C.c_int, C.c_int]
        L.som_ux_name_next_pad.argtypes = [C.c_void_p, C.c_int]
        L.som_ux_name_next_pad.restype = C.c_uint16
        L.som_ux_name.argtypes = [C.c_void_p, C.c_int]
        L.som_ux_name.restype = C.c_char_p
        L.som_ux_scale.argtypes = [C.c_int, C.c_int]
        L.som_ux_delete.argtypes = [C.c_void_p]
        L.som_ux_info.argtypes = [C.c_void_p, C.c_int]
        L.som_ux_line.argtypes = [C.c_void_p]
        L.som_ux_line.restype = C.c_char_p
        L.som_ux_slot_flash.argtypes = [C.c_void_p, C.c_int, C.c_int]
        L.som_ux_flash_entry.argtypes = [C.c_void_p, C.c_int, C.c_int, C.c_int]
        L.som_ux_shoulder_press.argtypes = [C.c_void_p, C.c_int, C.c_void_p]
        L.som_ux_set_shoulder_labels.argtypes = [C.c_void_p] + [C.c_char_p] * 4
        L.som_ux_shoulder_label.argtypes = [C.c_void_p, C.c_int]
        L.som_ux_shoulder_label.restype = C.c_char_p
        L.som_ux_longpress.argtypes = [C.c_void_p, C.c_int, C.c_int, C.c_int, C.c_int, C.c_void_p]
        L.som_ux_selection.argtypes = [C.c_void_p]
        L.som_ux_selection.restype = C.POINTER(SelInfo)
        L.som_ux_quick_set.argtypes = [C.c_void_p] + [C.c_int] * 3
        L.som_ux_quick_get.argtypes = [C.c_void_p, C.c_int, C.c_void_p, C.c_void_p]
        L.som_ux_quick_bind_selected.argtypes = [C.c_void_p, C.c_int]
        L.som_ux_quick_bind_menu.argtypes = [C.c_void_p, C.c_int]
        L.som_ux_set_option.argtypes = [C.c_void_p, C.c_int, C.c_int, C.c_char_p]
        L.som_ux_open_options.argtypes = [C.c_void_p, C.c_int]
        L.som_ux_options_page.argtypes = [C.c_void_p]
        L.som_ux_target_center.argtypes = [C.c_void_p] + [C.c_int] * 4 + [C.c_void_p, C.c_void_p]
        L.som_ux_target_min_vp.argtypes = [C.c_void_p] + [C.c_int] * 3
        L.som_ux_audit.argtypes = [C.c_int]
        L.som_ux_audit_note.restype = C.c_char_p
        self.p = C.c_void_p(L.som_ux_new())
        self._keep = []
        if pack:
            b = bytes(pathlib.Path(pack).read_bytes())
            self._keep.append(b)
            pk = L.som_ux_pack_load(b, len(b))
            if not pk:
                raise RuntimeError('pack rejected')
            L.som_ux_set_pack(self.p, pk)
        if rom:
            b = bytes(pathlib.Path(rom).read_bytes())
            self._keep.append(b)
            L.som_ux_set_rom(self.p, b, len(b))

    def update(self, wram):
        assert len(wram) == 0x20000
        self.lib.som_ux_update(self.p, wram)

    def classify(self, wram):
        return MODES[self.lib.som_ux_classify(wram)]

    def view(self, tab=None, hero=None, spirit=None):
        self.lib.som_ux_set_view(self.p, 0 if tab is None else tab, 0 if hero is None else hero, 255 if spirit is None else spirit)

    def pending(self, kind, cls, arg, spell=0):
        self.lib.som_ux_set_pending(self.p, kind, cls, arg, spell)

    def draw(self, w, h):
        buf = (C.c_uint32 * (w * h))()
        self.lib.som_ux_draw(self.p, None, buf, w, w, h)
        from PIL import Image
        import numpy as np
        a = np.frombuffer(buf, dtype=np.uint32).reshape(h, w)
        rgb = np.stack([(a >> 16) & 255, (a >> 8) & 255, a & 255], -1).astype(np.uint8)
        return Image.fromarray(rgb, 'RGB'), bytes(buf)

    def text_sheet(self, w, h):
        buf = (C.c_uint32 * (w * h))()
        self.lib.som_ux_text_sheet.argtypes = [C.c_void_p, C.c_void_p, C.c_int, C.c_int, C.c_int]
        self.lib.som_ux_text_sheet(self.p, buf, w, w, h)
        from PIL import Image
        import numpy as np
        a = np.frombuffer(buf, dtype=np.uint32).reshape(h, w)
        return Image.fromarray(np.stack([(a >> 16) & 255, (a >> 8) & 255, a & 255], -1).astype(np.uint8), 'RGB')

    def zone(self):
        import numpy as np
        L = self.lib
        L.som_ux_zone_pixels.argtypes = [C.c_void_p]
        L.som_ux_zone_pixels.restype = C.c_void_p
        w, h, m = C.c_int(), C.c_int(), C.c_int()
        L.som_ux_zone_info.argtypes = [C.c_void_p, C.c_void_p, C.c_void_p, C.c_void_p]
        ok = L.som_ux_zone_info(self.p, C.byref(w), C.byref(h), C.byref(m))
        if not ok:
            return None, 0
        buf = (C.c_uint32 * (w.value * h.value)).from_address(L.som_ux_zone_pixels(self.p))
        a = np.frombuffer(buf, dtype=np.uint32).reshape(h.value, w.value)
        return np.stack([(a >> 16) & 255, (a >> 8) & 255, a & 255], -1).astype(np.uint8), m.value

    def touch(self, x, y, w, h):
        a = Action()
        r = self.lib.som_ux_touch(self.p, x, y, w, h, C.byref(a))
        return r, {'kind': ACTS[a.kind], 'cmd': (a.cmd.kind, a.cmd.hero, a.cmd.arg, a.cmd.spell), 'tk': a.target_kind,
                   'ts': a.target_slot, 'pad': a.pad, 'letter': a.letter, 'bind': (a.bind_kind, a.bind_id), 'hero': a.hero,
                   'slot': a.slot, 'policy': a.policy, 'opt': (a.opt_id, a.opt_val)}

    def sprites(self, frame, wram):
        """MOCK ONLY: stand-in for the sprite atlas.  Cuts each on-screen enemy out of the top-screen frame and makes the background
        transparent by flood fill from the crop border, giving straight-alpha ARGB sprites like the real atlas will deliver."""
        import numpy as np
        L = self.lib
        L.som_ux_clear_enemy_sprites(self.p)
        n = 0
        for slot in range(3, 16):
            b = 0xE000 + 0x200 * slot
            if wram[b] != 1:
                continue
            eid = wram[b + 0x180]
            sx = int.from_bytes(wram[b + 0x20:b + 0x22], 'little', signed=True)
            sy = int.from_bytes(wram[b + 0x22:b + 0x24], 'little', signed=True)
            if not (8 <= sx < 248 and 0 <= sy < 216):
                continue
            S = 48
            x0 = min(max(sx - S // 2, 0), 256 - S)
            y0 = min(max(sy - S + 4, 0), 224 - S)
            c = frame[y0:y0 + S, x0:x0 + S].astype(int)
            bg = np.zeros((S, S), bool)
            stack = [(0, i) for i in range(S)] + [(S - 1, i) for i in range(S)] + [(i, 0) for i in range(S)] + [(i, S - 1) for i in range(S)]
            while stack:
                y, x = stack.pop()
                if bg[y, x]:
                    continue
                bg[y, x] = True
                for dy, dx in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < S and 0 <= xx < S and not bg[yy, xx] and np.abs(c[yy, xx] - c[y, x]).sum() < 70:
                        stack.append((yy, xx))
            a = np.where(bg, 0, 255).astype(np.uint32)
            argb = ((a << 24) | (c[..., 0].astype(np.uint32) << 16) | (c[..., 1].astype(np.uint32) << 8) | c[..., 2].astype(np.uint32)).ravel()
            buf = (C.c_uint32 * (S * S))(*argb.tolist())
            L.som_ux_set_enemy_sprite(self.p, eid, buf, S, S)
            n += 1
        return n

    def spell_avail(self, states):
        """states: {spell_id: 1 enabled | 2 disabled | 3 queued}; None clears."""
        L = self.lib
        if states is None:
            L.som_ux_set_spell_avail(self.p, None)
            return
        a = (C.c_uint8 * 42)()
        for k, v in states.items():
            a[k] = v
        L.som_ux_set_spell_avail(self.p, a)

    def notice(self, kind, text=None):
        self.lib.som_ux_notice.argtypes = [C.c_void_p, C.c_int, C.c_char_p]
        self.lib.som_ux_notice(self.p, kind, text.encode() if text else None)

    def press(self, hit, a=0, on=True):
        self.lib.som_ux_press_hit(self.p, HIT[hit], a, 1 if on else 0)

    def min_target(self, w, h):
        return self.lib.som_ux_min_target_vp(self.p, w, h)

    def name(self, i):
        v = self.lib.som_ux_name(self.p, i)
        return v.decode() if v else None

    def info(self, what):
        return self.lib.som_ux_info(self.p, what)

    def line(self):
        return self.lib.som_ux_line(self.p).decode('latin-1')

    def slot_flash(self, slot, kind=1):
        self.lib.som_ux_slot_flash(self.p, slot, kind)

    def flash_entry(self, kind, entry_kind, entry_id):
        self.lib.som_ux_flash_entry(self.p, kind, entry_kind, entry_id)

    def shoulder_press(self, which):
        a = Action()
        r = self.lib.som_ux_shoulder_press(self.p, which, C.byref(a))
        return r, {'kind': ACTS[a.kind], 'bind': (a.bind_kind, a.bind_id), 'hero': a.hero, 'slot': a.slot}

    def shoulder_labels(self, l1=None, r1=None, l2=None, r2=None):
        self.lib.som_ux_set_shoulder_labels(self.p, *[x.encode() if x else None for x in (l1, r1, l2, r2)])

    def shoulder_label(self, which):
        return self.lib.som_ux_shoulder_label(self.p, which).decode()

    def longpress(self, x, y, w, h):
        a = Action()
        r = self.lib.som_ux_longpress(self.p, x, y, w, h, C.byref(a))
        return r, {'kind': ACTS[a.kind], 'bind': (a.bind_kind, a.bind_id), 'hero': a.hero, 'slot': a.slot}

    def selection(self):
        s = self.lib.som_ux_selection(self.p).contents
        return {'valid': s.valid, 'kind': s.kind, 'id': s.id, 'menu': s.menu, 'owner': s.owner, 'sub_ring': s.sub_ring,
                'head': s.head.decode(), 'title': s.title.decode(), 'lines': [s.line[i].value.decode() for i in range(s.n)]}

    def quick(self, slot, kind, ident=0):
        return self.lib.som_ux_quick_set(self.p, slot, BK[kind] if isinstance(kind, str) else kind, ident)

    def option(self, ident, value, text=None):
        self.lib.som_ux_set_option(self.p, ident, value, text.encode() if text else None)

    def options(self, page):
        self.lib.som_ux_open_options(self.p, page)

    def center(self, w, h, hit, a=0):
        x, y = C.c_int(), C.c_int()
        ok = self.lib.som_ux_target_center(self.p, w, h, HIT[hit], a, C.byref(x), C.byref(y))
        return (x.value, y.value) if ok else None

    def tap_hit(self, w, h, hit, a=0):
        c = self.center(w, h, hit, a)
        return None if c is None else self.touch(c[0], c[1], w, h)

    def min_hit(self, w, h, hit):
        return self.lib.som_ux_target_min_vp(self.p, w, h, HIT[hit])

    def draw_audit(self, w, h):
        """Draws one frame without converting it and returns (count, note) of the layout audit (cut or overlapping text)."""
        buf = (C.c_uint32 * (w * h))()
        self.lib.som_ux_audit(1)
        self.lib.som_ux_draw(self.p, None, buf, w, w, h)
        n = self.lib.som_ux_audit(1)
        return n, self.lib.som_ux_audit_note().decode() if n else ''
