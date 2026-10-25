#!/usr/bin/env python3
"""somd_sound: menu sound presets through the stub in field, ring-menu and event states; audio energy versus a control run.

    python3 test_direct_sound.py VARIANT STATE
"""
import ctypes as C
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest  # noqa: E402

CB = C.CFUNCTYPE(C.c_size_t, C.POINTER(C.c_int16), C.c_size_t)
energy = [0]


def audio(data, frames):
    energy[0] += sum(abs(data[i]) for i in range(0, frames * 2, 7))
    return frames


def main(variant, state):
    D = dtest.Direct(variant)
    c = D.core
    cb = CB(audio)
    c.lib.retro_set_audio_sample_batch(cb)
    lib = D.lib
    lib.shim_sound.argtypes = [C.c_void_p, C.c_void_p, C.c_int]
    results = []

    def setup(mode):
        D.load(state, 4)
        if mode == 'ring':
            for _ in range(40):
                D.frame(1 << 1)      # Y opens the ring
            D.frame(0)
            for _ in range(30):
                D.frame()
        elif mode == 'event':
            c.write(0xD0, bytes([0x81]))
            for _ in range(3):
                D.frame()

    def run(mode, sfx):
        setup(mode)
        ring = c.u8(0x7D)
        ev = c.u8(0xD0)
        energy[0] = 0
        rc = lib.shim_sound(D.d, D.wram, sfx)
        n = 0
        while D.state == 'running' and n < 60:
            D.frame(); n += 1
        for _ in range(20):
            D.frame()
        e_cmd = energy[0]
        st, er, fr = D.state, D.err, D.frames
        setup(mode)
        energy[0] = 0
        for _ in range(n + 20):
            D.frame()
        e_ctl = energy[0]
        rec = dict(mode=mode, sfx=hex(sfx), rc=dtest.ERR[rc], state=st, err=er, frames=fr, ring_flag=ring, event=ev,
                   audio_cmd=e_cmd, audio_ctl=e_ctl, audio_changed=e_cmd != e_ctl)
        print(json.dumps(rec), flush=True)
        results.append(rec)

    for mode in ('field', 'ring', 'event'):
        for sfx in (0x29, 0x2A, 0x2B, 0x0E, 0x0D):
            run(mode, sfx)
    setup('field')
    bad = dtest.ERR[lib.shim_sound(D.d, D.wram, 0x01)]
    print(json.dumps(dict(check='reject-unknown-id', result=bad, ok=bad == 'bad-argument')))
    lib.shim_sound(D.d, D.wram, 0x29)
    busy = dtest.ERR[lib.shim_sound(D.d, D.wram, 0x2A)]
    print(json.dumps(dict(check='busy', result=busy, ok=busy == 'busy')))
    return all(r['state'] == 'finished' for r in results) and bad == 'bad-argument' and busy == 'busy'


if __name__ == '__main__':
    sys.exit(0 if main(sys.argv[1], sys.argv[2]) else 1)
