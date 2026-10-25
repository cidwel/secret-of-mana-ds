#!/usr/bin/env python3
"""Device-path test: the UX layer rendered through the proposed Android glue (test_glue_path.c -> android_patch/som_ux_glue.h) at the
panel sizes the device reports, with the real pack and ROM.

  1. the C glue run must pass the layout audit and the label contract (every HP/MP label a separate box with a gap of >= 2 vp,
     no text over a portrait, no overlapping text boxes) for map 215 with 3-digit, 4-digit and low values;
  2. every render must be bit-identical to the ctypes renderer used for the mocks (so mocks == device path);
  3. every render must match the golden hash in glue_hashes.txt (--update-hashes rewrites it; the hashes depend on the pack and ROM
     kept in the state directory);
  4. (informational since round 4, hard with SOM_UX_STRICT_CAPTURE=1) outside the hero cards the 1240x970 render is compared with the real device capture (device/final/field_bottom.png) except for the
     Android overlay and RGB565 rounding (the capture predates this fix, so the cards themselves are not compared)."""
import os
import pathlib
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import common  # noqa: E402
import ux_lib  # noqa: E402
import numpy as np  # noqa: E402
from PIL import Image  # noqa: E402

HERE = pathlib.Path(__file__).resolve().parent
ST = common.STATE
PACK = ST / 'assets' / 'som_ui_pack.bin'
WRAM = ST / 'ux' / 'wram' / 'field_215.bin'
OUT = ST / 'ux' / 'devpath'
BIN = ST / 'ux' / 'build' / 'test_glue'
HASHES = HERE / 'glue_hashes.txt'
DEVICE = ST / 'device' / 'final' / 'field_bottom.png'
# name: (hp, hp max, mp, mp max); mp None keeps the capture's values.  Must mirror STATE[] in test_glue_path.c.
STATES = {'map215': ((523, 416, 411), (524, 417, 412), None, None), 'map215_999': ((999,) * 3, (999,) * 3, (99,) * 3, (99,) * 3),
          'map215_9999': ((9999,) * 3, (9999,) * 3, (255,) * 3, (255,) * 3), 'map215_low': ((52, 41, 9), (524, 417, 412), (5, 4, 0), (47,) * 3)}


def fnv(b):
    h = 1469598103934665603
    for x in b:
        h = ((h ^ x) * 1099511628211) & 0xFFFFFFFFFFFFFFFF
    return h


def main():
    update = '--update-hashes' in sys.argv
    BIN.parent.mkdir(parents=True, exist_ok=True)
    OUT.mkdir(parents=True, exist_ok=True)
    srcs = [HERE / n for n in ('test_glue_path.c', 'som_ui_draw.c', 'som_ui_state.c', 'som_ui_sel.c', 'som_ui_zone.c')] + [HERE.parent / 'som_companion.c']
    subprocess.run(['gcc', '-std=c99', '-Wall', '-Wextra', '-O1', '-I' + str(HERE.parent), '-o', str(BIN)] + [str(s) for s in srcs] + ['-lm'], check=True)
    run = subprocess.run([str(BIN), str(PACK), str(common.ROM_VANILLA), str(WRAM), str(OUT)], capture_output=True, text=True)
    print(run.stdout.strip().splitlines()[-1] if run.stdout.strip() else run.stderr)
    bad = []
    if run.returncode:
        bad.append('glue audit / label contract failed:\n' + run.stdout)
    got = {}
    for ln in run.stdout.splitlines():
        p = ln.split()
        if len(p) == 3 and 'x' in p[1]:
            got[(p[0], p[1])] = p[2]
    # 2. identical to the ctypes renderer
    u = ux_lib.UX(rom=common.ROM_VANILLA, pack=PACK)
    base = bytearray(WRAM.read_bytes())
    base[0xCF00] = 1                      # round 3: the bottom screen is dark until the story flag is set
    for name, (hp, hpm, mp, mpm) in STATES.items():
        w = bytearray(base)
        for i in range(3):
            b = 0xE000 + 0x200 * i
            w[b + 0x182:b + 0x184] = hp[i].to_bytes(2, 'little')
            w[b + 0x184:b + 0x186] = hpm[i].to_bytes(2, 'little')
            if mp:
                w[b + 0x186] = mp[i]
                w[b + 0x187] = mpm[i]
        u.update(bytes(w))
        for (n, sz), hv in got.items():
            if n != name:
                continue
            dw, dh = map(int, sz.split('x'))
            im, raw = u.draw(dw, dh)
            if '%016x' % fnv(raw) != hv:
                bad.append('ctypes render differs from the glue render: %s %s' % (name, sz))
    # 3. goldens
    if update:
        HASHES.write_text(''.join('%s %s %s\n' % (n, s, h) for (n, s), h in sorted(got.items())))
        print('wrote', HASHES)
    else:
        gold = {tuple(l.split()[:2]): l.split()[2] for l in HASHES.read_text().splitlines()} if HASHES.exists() else {}
        for k, h in got.items():
            if gold.get(k) != h:
                bad.append('golden mismatch %s %s: %s != %s' % (k[0], k[1], h, gold.get(k)))
    # 4. device capture, outside the cards and the Android overlay
    if DEVICE.exists():
        dev = np.array(Image.open(DEVICE).convert('RGB')).astype(int)
        a = np.fromfile(OUT / 'map215_1240x970.argb', dtype=np.uint32).reshape(970, 1240)
        a = np.stack([(a >> 16) & 255, (a >> 8) & 255, a & 255], -1).astype(int)
        reg = np.ones((960, 1240), bool)
        reg[84:280, :] = False                      # hero cards: the capture predates the fix
        reg[:84, 1040:] = False                     # OPT button (the capture shows the live gear)
        reg[840:960, 1100:1230] = False             # Android overlay gear
        d = (np.abs(a[:960] - dev[:960]).max(2) > 32) & reg
        frac = d.sum() / reg.sum()
        print('device capture mismatch outside cards: %.3f%%' % (100 * frac))
        # The capture predates the hi-res font, header and slot redesign (round 4), so it is informational until the Android agent
        # takes a fresh capture; SOM_UX_STRICT_CAPTURE=1 restores the hard check.
        if frac > 0.005 and os.environ.get('SOM_UX_STRICT_CAPTURE'):
            bad.append('render differs from the device capture outside the cards: %.2f%%' % (100 * frac))
    for b in bad:
        print('FAIL', b)
    print('device path: %s' % ('FAILED' if bad else 'OK'))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
