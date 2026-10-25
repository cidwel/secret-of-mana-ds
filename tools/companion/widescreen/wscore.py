"""Host wrapper for the widescreen core (snes9x_som_widescreen option): records geometry events and frame sizes."""
import ctypes as C
import os
import pathlib
import sys

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import retro  # noqa: E402
import common  # noqa: E402

OUT = pathlib.Path(os.environ.get('SOM_WS_STATE', '/home/cidwel/.local/state/som-decomp/companion-20261017/widescreen'))
CORE_WS = OUT / 'core/src/libretro/snes9x_libretro.so'
CORE_PLAIN = OUT / 'core_plain/src/libretro/snes9x_libretro.so'   # unmodified Snes9x 1.63 at the same commit
WORK = common.WORK


class Geo(C.Structure):
    _fields_ = [('base_width', C.c_uint), ('base_height', C.c_uint), ('max_width', C.c_uint), ('max_height', C.c_uint),
                ('aspect_ratio', C.c_float)]


class AV(C.Structure):
    _fields_ = [('geometry', Geo), ('fps', C.c_double), ('sample_rate', C.c_double)]


class WS(retro.Core):
    def __init__(self, rom, core=CORE_WS, options=None):
        self.geo_events = []     # (frame, base_width, base_height, aspect) for every SET_GEOMETRY
        self.sizes = []          # (w, h) of every video frame
        super().__init__(rom, core_path=str(core), options=options)
        self.has_ws = hasattr(self.lib, 'retro_som_ws_state')

    def _env(self, cmd, data):
        if cmd in (37, 32):      # SET_GEOMETRY, SET_SYSTEM_AV_INFO
            g = C.cast(data, C.POINTER(Geo if cmd == 37 else AV))[0]
            g = g if cmd == 37 else g.geometry
            self.geo_events.append((self.frames, g.base_width, g.base_height, g.aspect_ratio))
            return True
        return super()._env(cmd, data)

    def _vid(self, d, w, h, pitch):
        self.sizes.append((w, h))
        super()._vid(d, w, h, pitch)

    def av(self):
        a = AV()
        self.lib.retro_get_system_av_info(C.byref(a))
        return a.geometry.base_width, a.geometry.base_height, a.geometry.max_width, a.geometry.aspect_ratio

    def state(self):
        v = [C.c_int() for _ in range(6)]
        d = (C.c_int * 4)()
        self.lib.retro_som_ws_state(*[C.byref(x) for x in v[:5]], d)
        return dict(margin=v[0].value, active=v[1].value, pending=v[2].value, width=v[3].value, dc=v[4].value, dec=list(d))

    def rgb(self):
        return np.frombuffer(self.frame_rgb, np.uint8).reshape(self.height, self.width, 3).copy()

    def oam(self):
        self.lib.retro_som_oam_frame_get.restype = C.POINTER(C.c_uint8)
        return bytes(self.lib.retro_som_oam_frame_get()[0:544])


def _stat(self):
    """Cumulative side-pass counters: [batches without a side render (unsupported mode), batches with no drawable layer,
    side passes drawn, tilemap offset recoveries]."""
    if not hasattr(self.lib, 'retro_som_ws_stat'):      # cores older than round 3
        return [0, 0, 0, 0]
    self.lib.retro_som_ws_stat.restype = C.POINTER(C.c_int)
    p = self.lib.retro_som_ws_stat()
    return [p[i] for i in range(4)]


WS.stat = _stat


def copy_core(path, tag):
    """A private copy of a core .so: loading the same path twice would share its global state."""
    import shutil
    import tempfile
    d = pathlib.Path(tempfile.mkdtemp(prefix='wscore_'))
    dst = d / ('%s_%s' % (tag, pathlib.Path(path).name))
    shutil.copy(str(path), str(dst))
    return dst


def raw16(c):
    """Last video frame as a (h, w) uint16 array in the core's native RGB565."""
    return np.frombuffer(c._raw, np.uint16).reshape(c.height, c._pitch // 2)[:, :c.width].copy()


def merge_pairs(a):
    """Average horizontal pixel pairs exactly like the core does for hi-res frames."""
    l, r = a[:, 0::2].astype(np.uint32), a[:, 1::2].astype(np.uint32)
    return ((l & r) + (((l ^ r) & 0xF7DE) >> 1)).astype(np.uint16)


def _regs(self, a, n=1):
    self.lib.retro_som_regs_get.restype = C.POINTER(C.c_uint8)
    p = self.lib.retro_som_regs_get()
    return bytes(p[a:a + n])


WS.regs = _regs
