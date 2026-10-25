"""ctypes wrapper of som_enemy_sprite.c (the C function under test) plus PNG helpers."""
import ctypes as C
import pathlib

import numpy as np

HERE = pathlib.Path(__file__).resolve().parent
LIB = pathlib.Path('/home/cidwel/.local/state/som-decomp/companion-20261017/enemysprites/build/libsomes.so')
BUF = (258 * 258 * 4)


class Src(C.Structure):
    _fields_ = [('oam', C.c_char_p), ('vram', C.c_char_p), ('cgram', C.c_char_p), ('obsel', C.c_uint8)]


class Img(C.Structure):
    _fields_ = [('rgba', C.POINTER(C.c_uint8)), ('w', C.c_int), ('h', C.c_int), ('foot_x', C.c_int), ('foot_y', C.c_int),
                ('n_sprites', C.c_int), ('n_pieces', C.c_int), ('n_matched', C.c_int)]


lib = C.CDLL(str(LIB))
lib.som_es_render_actor.argtypes = [C.POINTER(Src), C.c_char_p, C.c_int, C.POINTER(Img)]
lib.som_es_render_box.argtypes = [C.POINTER(Src), C.c_int, C.c_int, C.c_int, C.c_int, C.c_int, C.c_int, C.POINTER(Img)]


def _result(img, ok):
    info = dict(ok=bool(ok), pieces=img.n_pieces, matched=img.n_matched, sprites=img.n_sprites)
    if not ok:
        return None, info
    arr = np.ctypeslib.as_array(img.rgba, shape=(img.h * img.w * 4,)).reshape(img.h, img.w, 4).copy()
    info.update(w=img.w, h=img.h, foot=(img.foot_x, img.foot_y))
    return arr, info


def render_actor(oam, vram, cgram, obsel, wram, slot):
    buf = (C.c_uint8 * BUF)()
    img = Img(C.cast(buf, C.POINTER(C.c_uint8)))
    src = Src(bytes(oam), bytes(vram), bytes(cgram), obsel)
    ok = lib.som_es_render_actor(C.byref(src), bytes(wram), slot, C.byref(img))
    return _result(img, ok)


def render_box(oam, vram, cgram, obsel, x0, y0, x1, y1, fx, fy):
    buf = (C.c_uint8 * BUF)()
    img = Img(C.cast(buf, C.POINTER(C.c_uint8)))
    src = Src(bytes(oam), bytes(vram), bytes(cgram), obsel)
    ok = lib.som_es_render_box(C.byref(src), x0, y0, x1, y1, fx, fy, C.byref(img))
    return _result(img, ok)


def src_from_wram(wram, vram):
    """What som_es_src_wram() does: (oam, vram, cgram, obsel) taken from the WRAM shadows."""
    cg = 0xBE00 if wram[0x2A] else 0x0600
    return bytes(wram[0x0800:0x0800 + 544]), vram, bytes(wram[cg:cg + 512]), wram[0x91]


lib.som_es_sprite_for_id.argtypes = [C.c_char_p, C.c_size_t, C.POINTER(Src), C.c_char_p, C.c_int, C.POINTER(C.c_uint8), C.POINTER(C.c_uint32),
                                     C.POINTER(C.c_int), C.POINTER(C.c_int), C.POINTER(C.c_int)]


def sprite_for_id(pack, wram, vram, enemy_id, live=True):
    """som_es_sprite_for_id through the WRAM-shadow route: returns (source, conf, rgba array or None)."""
    oam, vr, cg, ob = src_from_wram(wram, vram)
    src = Src(oam, vr, cg, ob)
    scratch = (C.c_uint8 * BUF)()
    out = (C.c_uint32 * (96 * 96))()
    w, h, conf = C.c_int(), C.c_int(), C.c_int()
    r = lib.som_es_sprite_for_id(pack if pack else None, len(pack) if pack else 0, C.byref(src) if live else None, bytes(wram) if live else None,
                                 enemy_id, scratch, out, C.byref(w), C.byref(h), C.byref(conf))
    if not r:
        return 0, 0, None
    a = np.frombuffer(out, np.uint32, count=w.value * h.value).copy().reshape(h.value, w.value)
    rgba = np.stack([(a >> 16) & 255, (a >> 8) & 255, a & 255, a >> 24], -1).astype(np.uint8)
    return r, conf.value, rgba
