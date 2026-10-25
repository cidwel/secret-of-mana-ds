"""PC harness helpers: widescreen-core wrapper, map warps and per-actor reads.  Nothing from the ROM is stored in the repository."""
import json
import pathlib
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / 'widescreen'))
import wslib  # noqa: E402
from survey import warp  # noqa: E402

STATE = pathlib.Path('/home/cidwel/.local/state/som-decomp/companion-20261017/enemysprites')
SCENES = STATE / 'scenes'
SURVEY = wslib.OUT / 'survey.json'


def core():
    return wslib.WCore(wslib.common.ROM_VANILLA)


def load_map(c, m, frames=260):
    SCENES.mkdir(parents=True, exist_ok=True)
    p = SCENES / ('map%d.state' % m)
    if p.exists():
        c.load_state(p.read_bytes())
        c.run(2)
        return
    survey = json.load(open(SURVEY))
    c.load_state(open(wslib.WORK / 'base_vanilla.state', 'rb').read())
    c.run(2)
    warp(c, survey[str(m)]['trans'])
    c.run(frames)
    p.write_bytes(c.save_state())


def actors(c):
    """Enemy-like actor records 3..15: dict slot -> fields."""
    out = {}
    for s in range(3, 16):
        r = c.read(0xE000 + 0x200 * s, 0x200)
        u16 = lambda o: r[o] | r[o + 1] << 8
        if r[0] != 1:
            continue
        out[s] = dict(slot=s, id=r[0x180], hp=u16(0x182), hpmax=u16(0x184), sx=u16(0x20), sy=u16(0x22), status=u16(0x190),
                      boss=bool(r[0x1FB] & 0x40), n1=r[0x83] & 15, n2=r[0x84] & 15, b74=r[0x74], h45=r[0x45], raw=r)
    return out


def ppu(c):
    """(oam 544 B, vram 64 KB, cgram 512 B, obsel) as the PPU holds them after the last frame."""
    import ctypes as C
    c.lib.retro_som_cgram_get.restype = C.POINTER(C.c_uint8)
    cg = bytes(c.lib.retro_som_cgram_get()[0:512])
    return c.oam(), c.vram(), cg, c.regs(0x2101)[0]


def gather(c, frames=6, offsets=((0, -50), (-50, -30), (50, -30), (-70, 20), (70, 20), (0, 50), (-40, 60), (40, 60))):
    """Moves up to 8 non-boss enemy actors next to the party (the game keeps running its own code), runs `frames` frames."""
    hx, hy = c.u16(0xE002), c.u16(0xE004)
    a = actors(c)
    live = [s for s, x in a.items() if x['id'] < 0x57 and x['hpmax'] > 0]
    for s, (dx, dy) in zip(live, offsets):
        b = 0xE000 + 0x200 * s
        c.write(b + 2, ((hx + dx) & 0xFFFF).to_bytes(2, 'little'))
        c.write(b + 4, ((hy + dy) & 0xFFFF).to_bytes(2, 'little'))
    c.run(frames)
    return actors(c)
