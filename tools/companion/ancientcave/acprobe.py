"""Shared helpers for the Ancient Cave (T5) experiments: paths, core access, input taps, PNG output.

Everything runs through tools/companion/retro.py (ctypes libretro host).  The Ancient Cave ROM is generated
outside the repository (tools/companion/ancientcave_gen) and only read from the state directory.
"""
import os
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))
import retro  # noqa: E402

STATE = pathlib.Path(os.environ.get('SOM_COMPANION_STATE', '/home/cidwel/.local/state/som-decomp/companion-20261017'))
ROM = pathlib.Path(os.environ.get('SOM_ROM_AC', str(STATE / 'ancientcave/out/ac_seed1.smc')))
WORK = STATE / 'work'
OUT = STATE / 'ancientcave/work'
TAG = os.environ.get('SOM_AC_TAG', '')      # '' for the main seed-1 ROM, for example '_s777' for another generated ROM
BASE = WORK / ('ac_base%s.state' % TAG)
PARTY = WORK / ('ac_party%s.state' % TAG)
EVID = STATE / ('evidence/ancientcave%s' % TAG)
for _d in (WORK, OUT, EVID):
    _d.mkdir(parents=True, exist_ok=True)


def open_core(rom=ROM):
    return retro.Core(str(rom))


def tap(c, btn, hold=3, wait=10):
    c.run(hold, btn)
    c.run(wait)


def png(c, path):
    """Writes the current frame as PNG (rendered pixels only, kept in the state directory)."""
    from PIL import Image
    Image.frombytes('RGB', (c.width, c.height), bytes(c.frame_rgb)).save(str(path))


def event(c, hexbytes):
    """Injects an event-VM script at $7F:FF00 and starts it (the same fixture the host's EVENT command uses)."""
    for i, b in enumerate(bytes.fromhex(hexbytes)):
        c.wram[0x1FF00 + i] = b
    c.wram[0xD1], c.wram[0xD2], c.wram[0xD3] = 0x00, 0xFF, 0x7F
    c.run(1)
    c.wram[0xD0] = 1


def ring(c):
    """(open, owner, category, cursor, count, busy)"""
    return tuple(c.u8(a) for a in (0x7D, 0x1808, 0x1809, 0x180A, 0x180B, 0x1812))


def open_ring(c):
    c.run(3, 'Y')
    for _ in range(120):
        c.run(1)
        if c.u8(0x7D) == 1 and c.u8(0x1812) == 0 and c.u8(0x1809) != 0x55 and c.u8(0x180B):
            c.run(4)
            return True
    return False


def close_ring(c):
    for _ in range(12):
        if c.u8(0x7D) == 0:
            return True
        c.run(3, 'A')
        c.run(40)
        if c.u8(0x7D):
            c.run(3, 'Y')
            c.run(40)
    return c.u8(0x7D) == 0


def goto_category(c, cat):
    for _ in range(6):
        if c.u8(0x1809) == cat:
            return True
        c.run(3, 'UP')
        c.run(42)
    return c.u8(0x1809) == cat


def goto_cursor(c, idx):
    for _ in range(20):
        cur, n = c.u8(0x180A), c.u8(0x180B)
        if cur == idx:
            return True
        # LEFT = +1, RIGHT = -1 (mod count)
        fwd = (idx - cur) % n
        c.run(3, 'LEFT' if fwd <= n - fwd else 'RIGHT')
        c.run(19)
    return c.u8(0x180A) == idx
