"""Helpers for the widescreen experiments: patched Snes9x core wrapper and image utilities.

The patched core (ws_snes9x.patch) renders each scanline batch three times (left, right, centre)
and exposes the 768 px wide buffer through retro_som_ws_get().  Nothing from the ROM is stored.
"""
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
WSCORE = OUT / 'core/src/libretro/snes9x_libretro.so'
WORK = common.WORK


class WCore(retro.Core):
    """retro.Core on the patched core; wide() returns the 768 px wide RGB image of the last frame."""

    def __init__(self, rom, options=None, ws=True, bg3=False, obj=True):
        os.environ['SOM_WS_MARGIN'] = '128' if ws else '0'   # debug override: full 768 px render, never falls back
        options = dict(options or {}, snes9x_som_widescreen_fallback='black')
        os.environ['SOM_WS_BG3'] = '1' if bg3 else '0'
        os.environ['SOM_WS_OBJ'] = '1' if obj else '0'
        super().__init__(rom, core_path=str(WSCORE), options=options)
        self.lib.retro_som_ws_get.restype = C.POINTER(C.c_uint16)
        self.lib.retro_som_oam_get.restype = C.POINTER(C.c_uint8)
        self.lib.retro_som_regs_get.restype = C.POINTER(C.c_uint8)
        self.lib.retro_som_cgram_get.restype = C.POINTER(C.c_uint16)

    def oam(self):
        return bytes(self.lib.retro_som_oam_get()[0:544])

    def regs(self, a, n=1):
        p = self.lib.retro_som_regs_get()
        return bytes(p[a:a + n])

    def line_bg(self, y, i):
        h, v = C.c_int(), C.c_int()
        self.lib.retro_som_line_get(y, i, C.byref(h), C.byref(v))
        return h.value, v.value

    def bg(self, i):
        h, v = C.c_int(), C.c_int()
        self.lib.retro_som_bg_get(i, C.byref(h), C.byref(v))
        return h.value, v.value

    def wide(self):
        h = self.height
        a = np.ctypeslib.as_array(self.lib.retro_som_ws_get(), shape=(240 * 768,)).reshape(240, 768)[:h].astype(np.uint32)
        r, g, b = (a >> 11) << 3, ((a >> 5) & 63) << 2, (a & 31) << 3
        return np.stack([r, g, b], -1).astype(np.uint8)

    def stock(self):
        w, h = self.width, self.height
        return np.frombuffer(self.frame_rgb, np.uint8).reshape(h, w, 3).copy()


def save_png(arr, path):
    from PIL import Image
    Image.fromarray(np.ascontiguousarray(arr)).save(str(path))
