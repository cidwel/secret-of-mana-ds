"""Python test harness: libretro core (retro.Core) + the C module som_direct through ctypes."""
import ctypes as C
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import retro  # noqa: E402
import common  # noqa: E402

STATE = common.STATE.parent / 'companion-direct'
STATE.mkdir(parents=True, exist_ok=True)
LIB = STATE / 'libsomd.so'

ROMS = {
    'vanilla': common.ROM_VANILLA,
    'magno': common.ROM_MAGNO,
    'ac1': common.STATE / 'ancientcave/out/ac_seed1.smc',
    'ac777': common.STATE / 'ancientcave/out/ac_seed777.smc',
}
STATES = {'vanilla': common.WORK / 'party3_vanilla.state', 'magno': common.WORK / 'party3_magno.state',
          'ac1': common.WORK / 'ac_party.state', 'ac777': common.WORK / 'ac_party_s777.state'}

import os  # noqa: E402
for _spec in filter(None, os.environ.get('SOM_DTEST_EXTRA', '').split(',')):    # NAME|ROM|STATE: more variants (generated Ancient Cave ROMs)
    _n, _r, _s = _spec.split('|')
    ROMS[_n] = pathlib.Path(_r)
    STATES[_n] = pathlib.Path(_s)

ERR = ['ok', 'busy', 'unsafe-state', 'bad-argument', 'not-owned', 'no-mp', 'caster-busy', 'bad-target', 'no-target',
       'needs-ring', 'timeout', 'rejected', 'unsupported-rom', 'ko', 'status', 'expired', 'cancelled']
STATE_NAMES = ['idle', 'running', 'finished', 'failed']


def build_lib():
    srcs = [HERE / 'som_direct.c', HERE / 'somd_shim.c']
    if not LIB.exists() or LIB.stat().st_mtime < max(s.stat().st_mtime for s in srcs + [HERE / 'som_direct.h']):
        subprocess.run(['gcc', '-std=c99', '-Wall', '-Wextra', '-O2', '-shared', '-fPIC', '-o', str(LIB)] + [str(s) for s in srcs], check=True)
    return C.CDLL(str(LIB))


class Actor(C.Structure):
    _fields_ = [('slot', C.c_uint8), ('present', C.c_uint8), ('on_screen', C.c_uint8), ('alive', C.c_uint8), ('id', C.c_uint8),
                ('sx', C.c_uint16), ('sy', C.c_uint16), ('wx', C.c_int16), ('wy', C.c_int16), ('hp', C.c_uint16), ('hp_max', C.c_uint16), ('status', C.c_uint16),
                ('valid', C.c_uint8)]


class CastInfo(C.Structure):
    _fields_ = [('reason', C.c_uint8), ('permanent', C.c_uint8), ('will_clear', C.c_uint8), ('eta_frames', C.c_uint16), ('status', C.c_uint16),
                ('spell_id', C.c_uint8), ('mp_have', C.c_uint8), ('mp_cost', C.c_uint8), ('cls', C.c_uint8)]


class QueueInfo(C.Structure):
    _fields_ = [('state', C.c_uint8), ('reason', C.c_uint8), ('err', C.c_uint8), ('spell_id', C.c_uint8), ('waited', C.c_uint16), ('timeout', C.c_uint16),
                ('replaced', C.c_uint8), ('replaced_id', C.c_uint8)]


class ReviveOpt(C.Structure):
    _fields_ = [(n, C.c_uint8) for n in ('kind', 'id', 'qty', 'record', 'caster', 'spirit', 'slot', 'mp_cost', 'reason', 'usable')]


REASONS = ['ok', 'bad-argument', 'not-owned', 'ko', 'no-mp', 'silenced', 'status', 'busy', 'command', 'ring-open', 'event', 'fade', 'input-lock', 'unsafe']
QSTATE = ['empty', 'waiting', 'fired', 'dropped']


class Targets(C.Structure):
    _fields_ = [('actor', Actor * 6), ('n_allies_valid', C.c_uint8), ('n_enemies_valid', C.c_uint8), ('cls', C.c_uint8)]


# one libretro core per process (the core keeps global state)
_core = {}


def core_for(rom):
    if 'c' not in _core or _core['rom'] != str(rom):
        if 'c' in _core:
            raise RuntimeError('one ROM per process: run each ROM in its own process')
        _core['c'] = retro.Core(str(rom), core_path=str(common.CORE))
        _core['rom'] = str(rom)
    return _core['c']


class Direct:
    def __init__(self, variant):
        self.variant = variant
        self.fast = False      # True: the old hidden fast-forward of menu jumps (test_direct_menu.py measures it)
        self.rom_path = ROMS[variant]
        self.core = core_for(self.rom_path)
        self.lib = build_lib()
        L = self.lib
        L.shim_size.restype = C.c_size_t
        for f in ('shim_state', 'shim_err', 'shim_frames', 'shim_rom_ok'):
            getattr(L, f).restype = C.c_int
            getattr(L, f).argtypes = [C.c_void_p]
        self.rom = pathlib.Path(self.rom_path).read_bytes()
        self.romb = C.create_string_buffer(self.rom, len(self.rom))
        self.d = C.create_string_buffer(L.shim_size())
        L.somd_init.argtypes = [C.c_void_p, C.c_char_p, C.c_size_t]
        L.somd_init(self.d, self.romb, len(self.rom))
        self.wram = self.core.wram
        L.somd_pre_frame.argtypes = [C.c_void_p, C.c_void_p]
        L.somd_post_frame.argtypes = [C.c_void_p, C.c_void_p]
        L.shim_equip.argtypes = [C.c_void_p, C.c_void_p, C.c_int, C.c_int]
        L.shim_item.argtypes = [C.c_void_p, C.c_void_p] + [C.c_int] * 3
        L.shim_cast.argtypes = [C.c_void_p, C.c_void_p] + [C.c_int] * 5
        L.shim_spell_class.argtypes = [C.c_void_p, C.c_int]
        L.shim_spell_mp.argtypes = [C.c_void_p, C.c_int]
        L.shim_scan.argtypes = [C.c_void_p, C.c_int, C.c_int, C.c_int, C.c_void_p]

    def load(self, path=None, settle=2):
        self.core.load_state(pathlib.Path(path or STATES[self.variant]).read_bytes())
        L = self.lib
        L.somd_init(self.d, self.romb, len(self.rom))
        if self.fast:
            L.shim_fast_menu.argtypes = [C.c_void_p, C.c_int]
            L.shim_fast_menu(self.d, 1)
        self.core.run(settle)

    @property
    def state(self):
        return STATE_NAMES[self.lib.shim_state(self.d)]

    @property
    def err(self):
        return ERR[self.lib.shim_err(self.d)]

    @property
    def frames(self):
        return self.lib.shim_frames(self.d)

    def frame(self, pad=0):
        self.lib.somd_pre_frame(self.d, self.wram)
        self.lib.shim_menu_pad.restype = C.c_int
        self.lib.shim_menu_pad.argtypes = [C.c_void_p]
        self.core.set_pad(pad | self.lib.shim_menu_pad(self.d))
        self.core.step()
        self.lib.somd_post_frame(self.d, self.wram)

    def wait(self, maxf=60):
        n = 0
        while self.state == 'running' and n < maxf:
            self.frame()
            n += 1
        return n

    def can_cast(self, hero, spirit, slot):
        self.lib.shim_can_cast.argtypes = [C.c_void_p, C.c_void_p, C.c_int, C.c_int, C.c_int, C.c_void_p]
        ci = CastInfo()
        r = self.lib.shim_can_cast(self.d, self.wram, hero, spirit, slot, C.byref(ci))
        return REASONS[r], ci

    def queue_cast(self, hero, spirit, slot, pk=1, ps=0, timeout=0):
        self.lib.shim_queue_cast.argtypes = [C.c_void_p, C.c_void_p] + [C.c_int] * 6 + [C.c_void_p]
        ci = CastInfo()
        r = self.lib.shim_queue_cast(self.d, self.wram, hero, spirit, slot, pk, ps, timeout, C.byref(ci))
        return ERR[r], ci

    def queue_state(self, hero):
        self.lib.shim_queue_state.argtypes = [C.c_void_p, C.c_int, C.c_void_p]
        q = QueueInfo()
        st = self.lib.shim_queue_state(self.d, hero, C.byref(q))
        return QSTATE[st], q

    def queue_cancel(self, hero):
        self.lib.shim_queue_cancel.argtypes = [C.c_void_p, C.c_int]
        self.lib.shim_queue_cancel(self.d, hero)

    def queue_ack(self, hero):
        self.lib.shim_queue_ack.argtypes = [C.c_void_p, C.c_int]
        self.lib.shim_queue_ack(self.d, hero)

    def revive(self, hero):
        self.lib.shim_revive.argtypes = [C.c_void_p, C.c_void_p, C.c_int, C.c_void_p]
        arr = (ReviveOpt * 4)()
        n = self.lib.shim_revive(self.d, self.wram, hero, arr)
        return [dict((f, getattr(arr[i], f)) for f, _ in ReviveOpt._fields_) for i in range(n)]

    def hidden(self):
        self.lib.shim_hide.argtypes = [C.c_void_p]
        return self.lib.shim_hide(self.d)

    def open_menu(self, kind, hero, arg):
        self.lib.shim_open_menu.argtypes = [C.c_void_p, C.c_void_p, C.c_int, C.c_int, C.c_int]
        return ERR[self.lib.shim_open_menu(self.d, self.wram, kind, hero, arg)]

    def switch_hero(self, hero):
        self.lib.shim_switch_hero.argtypes = [C.c_void_p, C.c_void_p, C.c_int]
        return ERR[self.lib.shim_switch_hero(self.d, self.wram, hero)]

    def name_letter(self, letter):
        self.lib.shim_name_letter.argtypes = [C.c_void_p, C.c_void_p, C.c_int]
        return ERR[self.lib.shim_name_letter(self.d, self.wram, letter)]

    def name_delete(self):
        self.lib.shim_name_delete.argtypes = [C.c_void_p, C.c_void_p]
        return ERR[self.lib.shim_name_delete(self.d, self.wram)]

    def close_menu(self):
        self.lib.shim_close_menu.argtypes = [C.c_void_p, C.c_void_p]
        return ERR[self.lib.shim_close_menu(self.d, self.wram)]

    def equip(self, h, t):
        return ERR[self.lib.shim_equip(self.d, self.wram, h, t)]

    def item(self, rec, kind, slot=0):
        return ERR[self.lib.shim_item(self.d, self.wram, rec, kind, slot)]

    def cast(self, h, sp, sl, kind, slot=0):
        return ERR[self.lib.shim_cast(self.d, self.wram, h, sp, sl, kind, slot)]

    def scan(self, cls, ident=0, is_spell=0):
        t = Targets()
        self.lib.shim_scan(self.wram, cls, ident, is_spell, C.byref(t))
        return t

    def pick(self, targets, x, y, mw=320, mh=240, r=24):
        self.lib.shim_pick.argtypes = [C.c_void_p] + [C.c_int] * 5
        return self.lib.shim_pick(C.byref(targets), x, y, mw, mh, r)

    def group_card(self, targets):
        self.lib.shim_group_card.argtypes = [C.c_void_p]
        return self.lib.shim_group_card(C.byref(targets))

    def spell_class(self, i):
        return self.lib.shim_spell_class(self.d, i)

    def spell_mp(self, i):
        return self.lib.shim_spell_mp(self.d, i)


def hexs(b):
    return ' '.join('%02x' % x for x in b)
