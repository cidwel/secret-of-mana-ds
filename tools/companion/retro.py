#!/usr/bin/env python3
"""Minimal ctypes host for a libretro SNES core (snes9x) with WRAM, VRAM access and core options.

Used by the companion tests and probes.  The ROM is read from disk and handed to the core in
memory; nothing from it is stored anywhere.  Button names follow RETRO_DEVICE_ID_JOYPAD.
"""
import ctypes as C
import os
import pathlib

DEFAULT_CORE = os.environ.get(
    'SOM_CORE', str(pathlib.Path.home() / '.var/app/org.libretro.RetroArch/config/retroarch/cores/snes9x_libretro.so'))
BUTTONS = ['B', 'Y', 'SELECT', 'START', 'UP', 'DOWN', 'LEFT', 'RIGHT', 'A', 'X', 'L', 'R']
MEM_SAVE, MEM_RTC, MEM_WRAM, MEM_VRAM = 0, 1, 2, 3

ENV_FN = C.CFUNCTYPE(C.c_bool, C.c_uint, C.c_void_p)
VID_FN = C.CFUNCTYPE(None, C.c_void_p, C.c_uint, C.c_uint, C.c_size_t)
AS_FN = C.CFUNCTYPE(None, C.c_int16, C.c_int16)
ASB_FN = C.CFUNCTYPE(C.c_size_t, C.c_void_p, C.c_size_t)
POLL_FN = C.CFUNCTYPE(None)
INP_FN = C.CFUNCTYPE(C.c_int16, C.c_uint, C.c_uint, C.c_uint, C.c_uint)


class GameInfo(C.Structure):
    _fields_ = [('path', C.c_char_p), ('data', C.c_void_p), ('size', C.c_size_t), ('meta', C.c_char_p)]


class Var(C.Structure):
    _fields_ = [('key', C.c_char_p), ('value', C.c_char_p)]


class Core:
    def __init__(self, rom_path, core_path=DEFAULT_CORE, options=None):
        self.options = dict(options or {})
        self.fmt = 0
        self.width = self.height = 0
        self._raw = None
        self.pad = 0
        self.audio_frames = 0
        self._dirty = False
        self._keep = []
        self.lib = C.CDLL(core_path)
        lib = self.lib
        lib.retro_get_memory_data.restype = C.POINTER(C.c_uint8)
        lib.retro_get_memory_size.restype = C.c_size_t
        lib.retro_serialize_size.restype = C.c_size_t
        lib.retro_serialize.argtypes = [C.c_void_p, C.c_size_t]
        lib.retro_unserialize.argtypes = [C.c_void_p, C.c_size_t]
        lib.retro_load_game.argtypes = [C.POINTER(GameInfo)]
        self._env_cb = ENV_FN(self._env)
        self._vid_cb = VID_FN(self._vid)
        self._as_cb = AS_FN(lambda l, r: None)
        self._asb_cb = ASB_FN(self._asb)
        self._poll_cb = POLL_FN(lambda: None)
        self._inp_cb = INP_FN(self._inp)
        lib.retro_set_environment(self._env_cb)
        lib.retro_set_video_refresh(self._vid_cb)
        lib.retro_set_audio_sample(self._as_cb)
        lib.retro_set_audio_sample_batch(self._asb_cb)
        lib.retro_set_input_poll(self._poll_cb)
        lib.retro_set_input_state(self._inp_cb)
        lib.retro_init()
        data = pathlib.Path(rom_path).read_bytes()
        self._rom = C.create_string_buffer(data, len(data))
        gi = GameInfo(str(rom_path).encode(), C.cast(self._rom, C.c_void_p), len(data), None)
        if not lib.retro_load_game(C.byref(gi)):
            raise RuntimeError('retro_load_game failed')
        self.wram = lib.retro_get_memory_data(MEM_WRAM)
        if not self.wram:
            raise RuntimeError('core exposes no WRAM')
        self.frames = 0

    def _env(self, cmd, data):
        if cmd == 10:
            self.fmt = C.cast(data, C.POINTER(C.c_int))[0]
            return True
        if cmd in (9, 31):
            p = C.cast(data, C.POINTER(C.c_char_p))
            self._dir = C.c_char_p(b'.')
            p[0] = self._dir
            return True
        if cmd == 3:
            C.cast(data, C.POINTER(C.c_uint8))[0] = 0
            return True
        if cmd == 15:
            v = C.cast(data, C.POINTER(Var))[0]
            key = v.key.decode()
            if key in self.options:
                self._val = C.c_char_p(str(self.options[key]).encode())
                C.cast(data, C.POINTER(Var))[0].value = C.cast(self._val, C.c_char_p)
                return True
            return False
        if cmd == 17:
            C.cast(data, C.POINTER(C.c_uint8))[0] = 1 if self._dirty else 0
            self._dirty = False
            return True
        return False

    def set_option(self, key, value):
        self.options[key] = value
        self._dirty = True

    def _vid(self, d, w, h, pitch):
        if not d:
            return
        self.width, self.height, self._pitch = w, h, pitch
        self._raw = C.string_at(d, pitch * h)

    @property
    def frame_rgb(self):
        """RGB24 bytes of the last video frame (converted on demand)."""
        if not getattr(self, '_raw', None):
            return None
        w, h, pitch, raw = self.width, self.height, self._pitch, self._raw
        import numpy as np
        if self.fmt == 1:
            a = np.frombuffer(raw, np.uint8).reshape(h, pitch)[:, :w * 4].reshape(h, w, 4)
            return np.ascontiguousarray(a[:, :, 2::-1]).tobytes()
        a = np.frombuffer(raw, np.uint16).reshape(h, pitch // 2)[:, :w].astype(np.uint32)
        if self.fmt == 2:
            r, g, b = (a >> 11) << 3, ((a >> 5) & 63) << 2, (a & 31) << 3
        else:
            r, g, b = ((a >> 10) & 31) << 3, ((a >> 5) & 31) << 3, (a & 31) << 3
        return np.stack([r, g, b], -1).astype(np.uint8).tobytes()

    def _asb(self, d, n):
        self.audio_frames += n
        return n

    def _inp(self, port, dev, idx, bid):
        if port != 0 or dev != 1:
            return 0
        if bid == 256:
            return self.pad & 0xFFFF
        return (self.pad >> bid) & 1 if bid < 16 else 0

    @staticmethod
    def mask(names):
        m = 0
        for n in ([] if not names else (names.split(',') if isinstance(names, str) else names)):
            m |= 1 << BUTTONS.index(n)
        return m

    def run(self, n=1, buttons=None):
        self.pad = self.mask(buttons)
        for _ in range(n):
            self.lib.retro_run()
            self.frames += 1

    def set_pad(self, mask):
        self.pad = mask

    def step(self):
        self.lib.retro_run()
        self.frames += 1

    def read(self, addr, n=1):
        return bytes(self.wram[addr:addr + n])

    def u8(self, a):
        return self.wram[a]

    def u16(self, a):
        return self.wram[a] | (self.wram[a + 1] << 8)

    def write(self, addr, data):
        for i, b in enumerate(data):
            self.wram[addr + i] = b

    def dump_wram(self):
        return bytes(self.wram[0:0x20000])

    def vram(self):
        p = self.lib.retro_get_memory_data(MEM_VRAM)
        n = self.lib.retro_get_memory_size(MEM_VRAM)
        return bytes(p[0:n]) if p else b''

    def save_state(self):
        n = self.lib.retro_serialize_size()
        buf = C.create_string_buffer(n)
        if not self.lib.retro_serialize(buf, n):
            raise RuntimeError('serialize failed')
        return buf.raw

    def load_state(self, blob):
        buf = C.create_string_buffer(blob, len(blob))
        if not self.lib.retro_unserialize(buf, len(blob)):
            raise RuntimeError('unserialize failed')

    def save_ppm(self, path):
        with open(path, 'wb') as f:
            f.write(b'P6\n%d %d\n255\n' % (self.width, self.height))
            f.write(self.frame_rgb)
