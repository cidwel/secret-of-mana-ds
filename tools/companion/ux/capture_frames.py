#!/usr/bin/env python3
"""Captures WRAM together with the matching video frame of the advanced fixture maps into <state>/ux/wram/ (<name>.bin and <name>.frame.npy, 256x224 RGB)
so the mocks can show enemy crops taken from the live frame.  State directory only: the frames are game art.  Usage: capture_frames.py"""
import pathlib
import sys

import numpy as np

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import common  # noqa: E402
import retro  # noqa: E402

OUT = common.STATE / 'ux' / 'wram'


def main():
    c = retro.Core(str(common.ROM_VANILLA), core_path=str(common.CORE))
    adv = common.STATE / 'adv'
    for n in ('215', '222', '232', '228'):
        c.load_state(open(adv / ('adv%s.state' % n), 'rb').read())
        best = None
        for step in range(0, 600, 30):
            c.run(30)
            w = c.dump_wram()
            on = 0
            for s in range(3, 16):
                b = 0xE000 + 0x200 * s
                sx = int.from_bytes(w[b + 0x20:b + 0x22], 'little'); sy = int.from_bytes(w[b + 0x22:b + 0x24], 'little')
                if w[b] == 1 and w[b + 0x180] < 0x80 and int.from_bytes(w[b + 0x184:b + 0x186], 'little') and 24 <= sx < 232 and 40 <= sy < 200:
                    on += 1
            if best is None or on > best[0]:
                best = (on, w, np.frombuffer(c.frame_rgb, dtype=np.uint8).reshape(c.height, c.width, 3).copy())
        (OUT / ('enemies_%s.bin' % n)).write_bytes(best[1])
        np.save(OUT / ('enemies_%s.frame.npy' % n), best[2])
        print(n, 'enemies on screen', best[0], best[2].shape, flush=True)


if __name__ == '__main__':
    main()
