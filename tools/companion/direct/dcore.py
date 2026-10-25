"""Libretro helper for the direct-control experiments: snes9x state parsing and emulator-side routine calls.

snes9x serializes its CPU registers in the "REG" block (PB, DB, P, A, D, S, X, Y, PC).  A routine call is made by
patching that block in a serialized state so that the next instruction executed is a small stub written into WRAM,
which saves every register, runs the routine and jumps back to the interrupted PC.  Nothing in the ROM is touched.
"""
import pathlib
import struct
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent))
import retro  # noqa: E402

HEAD = 14


def blocks(blob):
    """Returns {name: (data_offset, length)} of a snes9x state."""
    out = {}
    p = HEAD
    while p + 11 <= len(blob):
        name = blob[p:p + 3].decode('latin1')
        n = int(blob[p + 4:p + 10])
        out[name] = (p + 11, n)
        p += 11 + n
    return out


REGF = [('PB', 1), ('DB', 1), ('P', 2), ('A', 2), ('D', 2), ('S', 2), ('X', 2), ('Y', 2), ('PC', 2)]


def parse_regs(blob):
    off, n = blocks(blob)['REG']
    r = {}
    for name, sz in REGF:
        r[name] = int.from_bytes(blob[off:off + sz], 'big')
        off += sz
    return r


def patch_regs(blob, **kw):
    off, n = blocks(blob)['REG']
    b = bytearray(blob)
    for name, sz in REGF:
        if name in kw:
            b[off:off + sz] = int(kw[name]).to_bytes(sz, 'big')
        off += sz
    return bytes(b)


class DCore(retro.Core):
    def regs(self):
        return parse_regs(self.save_state())
