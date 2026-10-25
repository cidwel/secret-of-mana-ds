#!/usr/bin/env python3
"""Headless snes9x (libretro) harness shared by the QOL verification scripts.

The ROM is read from disk and handed to the core in memory; no ROM byte is stored in the repository.  Paths come from the
environment: SOM_ROM (US v1.0, headerless or with a copier header), SOM_CORE (snes9x_libretro.so), SOM_STATES (fixtures dir).
"""
import hashlib
import os
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'tools' / 'companion'))
import retro  # noqa: E402

WORK = pathlib.Path(os.environ.get('SOM_QOL_WORK', pathlib.Path.home() / '.local/state/som-decomp/qol-20261020'))
STATES = pathlib.Path(os.environ.get('SOM_STATES', pathlib.Path.home() / '.local/state/som-decomp/companion-direct'))
NEWGAME = ROOT / 'tools' / 'som_input_newgame.txt'


def slot(n):
    return 0xE000 + 0x200 * n


def parse_script(path=NEWGAME):
    """[(first, last_exclusive, mask)] from the plan file used by the fidelity tools."""
    out = []
    for line in pathlib.Path(path).read_text().splitlines():
        line = line.split('#')[0].split()
        if len(line) >= 3:
            out.append((int(line[0]), int(line[1]), retro.Core.mask(line[2].split(','))))
    return out


def pad_at(script, frame):
    m = 0
    for a, b, mask in script:
        if a <= frame < b:
            m |= mask
    return m


def wram_hash(c, lo=0, hi=0x20000):
    return hashlib.sha1(bytes(c.wram[lo:hi])).hexdigest()[:16]


def run_newgame(rom, frames=9000, every=100, probes=None, script=None, hook=None):
    """Runs the new-game script; returns {'trace': [(frame, wram sha, probe values)], 'core': core}.
    probes: dict name -> (addr, length) sampled at every checkpoint.  hook(core, frame) runs before each frame."""
    script = script or parse_script()
    c = retro.Core(str(rom))
    trace = []
    for f in range(frames):
        if hook:
            hook(c, f)
        c.set_pad(pad_at(script, f))
        c.step()
        if f % every == every - 1:
            vals = {k: bytes(c.wram[a:a + n]).hex() for k, (a, n) in (probes or {}).items()}
            trace.append((f + 1, wram_hash(c), vals))
    return {'trace': trace, 'core': c}


def load_fixture(rom, state):
    c = retro.Core(str(rom))
    c.run(2)
    c.load_state(pathlib.Path(state).read_bytes())
    c.run(1)
    return c


def build_rom(base, options, name):
    """Applies `options` (dict) to a copy of `base` with som_gameplay; returns the patched ROM path (inside WORK/roms)."""
    import json
    import subprocess
    d = WORK / 'roms'
    d.mkdir(parents=True, exist_ok=True)
    opt = d / (name + '.json')
    opt.write_text(json.dumps({'options': options}))
    out = d / (name + '.sfc')
    subprocess.run([sys.executable, str(ROOT / 'tools/gameplay/som_gameplay.py'), 'apply', '--rom', str(base), '--options', str(opt),
                    '--out', str(out), '--on-conflict', 'skip'], check=True, capture_output=True)
    return out
