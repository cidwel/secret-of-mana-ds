#!/usr/bin/env python3
"""Where does the game stop emitting OAM entries for an actor?  Moves an active NPC slot across the screen and
counts the OAM entries it owns (entries added compared with the same frame with the actor parked far away)."""
import json
import struct
import sys
import pathlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import wslib  # noqa: E402
import scenes  # noqa: E402


def entries(c):
    o = c.oam()
    r = []
    for i in range(128):
        x, y, t, a = o[i * 4:i * 4 + 4]
        hi = (o[512 + i // 4] >> ((i % 4) * 2)) & 3
        r.append((i, x - 256 * (hi & 1), y, t, a, hi >> 1))
    return r


def count_for(c, state, slot_base, sx, sy):
    c.load_state(state)
    c.run(1)
    camx, camy = c.u16(0xA8), c.u16(0xAA)
    c.write(slot_base + 2, struct.pack('<H', camx + sx))
    c.write(slot_base + 4, struct.pack('<H', camy + sy))
    c.run(2)
    return [e for e in entries(c) if e[2] < 240]


def main():
    out = {}
    c = wslib.WCore(wslib.common.ROM_VANILLA, ws=False)
    for name, loader in (('potos_start', lambda: c.load_state(open(wslib.WORK / 'base_vanilla.state', 'rb').read())),
                         ('map131', lambda: scenes.load_scene(c, 131))):
        loader()
        c.run(2)
        state = c.save_state()
        slots = [0xE000 + 0x200 * k for k in range(3, 12) if c.u8(0xE000 + 0x200 * k) == 1]
        if not slots:
            continue
        sb = slots[0]
        far = count_for(c, state, sb, 2000 % 65536, 100)
        ref = count_for(c, state, sb, 120, 100)
        tiles = {e[3] for e in ref} - {e[3] for e in far}
        res = {'slot': hex(sb), 'tiles': sorted(tiles)}

        def seen(sx, sy):
            return any(e[3] in tiles for e in count_for(c, state, sb, sx, sy))
        lo = [sx for sx in range(-120, 20) if seen(sx, 100)]
        hi = [sx for sx in range(236, 380) if seen(sx, 100)]
        tp = [sy for sy in range(-60, 60) if seen(120, sy)]
        bt = [sy for sy in range(180, 330) if seen(120, sy)]
        res.update(left_first_emitted_actor_x=min(lo) if lo else None, right_last_emitted_actor_x=max(hi) if hi else None,
                   top_first_emitted_actor_y=min(tp) if tp else None, bottom_last_emitted_actor_y=max(bt) if bt else None)
        out[name] = res
        print(name, res, flush=True)
    json.dump(out, open(wslib.OUT / 'sprite_cull.json', 'w'), indent=1)


main()
