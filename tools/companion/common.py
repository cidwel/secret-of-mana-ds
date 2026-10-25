"""Shared paths and helpers for the companion prototype tests (no ROM data is stored anywhere)."""
import json
import os
import pathlib
import subprocess

HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
STATE = pathlib.Path(os.environ.get('SOM_COMPANION_STATE', '/home/cidwel/.local/state/som-decomp/companion-20261017'))
BUILD = STATE / 'build'
WORK = STATE / 'work'
ROMS = pathlib.Path('/home/cidwel/projects/rommancer/roms/snes')
ROM_VANILLA = pathlib.Path(os.environ.get('SOM_ROM', str(ROMS / 'Secret of Mana (USA).smc')))
ROM_MAGNO = pathlib.Path(os.environ.get('SOM_ROM_MAGNO', str(ROMS / 'Secret of Mana (USA) [T-Esp101.2_Magno].smc')))
CORE = pathlib.Path(os.environ.get(
    'SOM_CORE', str(pathlib.Path.home() / '.var/app/org.libretro.RetroArch/config/retroarch/cores/snes9x_libretro.so')))
ROUTE = ROOT / 'tools/fidelity_d_inputs/hhs_glitch_bk2_s9x14.txt'
NEWGAME = ROOT / 'tools/som_input_newgame.txt'


def build_host():
    BUILD.mkdir(parents=True, exist_ok=True)
    out = BUILD / 'som_companion_host'
    srcs = [HERE / 'som_companion.c', HERE / 'som_companion_host.c']
    if not out.exists() or out.stat().st_mtime < max(s.stat().st_mtime for s in srcs + [HERE / 'som_companion.h']):
        subprocess.run(['gcc', '-std=gnu99', '-Wall', '-Wextra', '-O2', '-o', str(out)] + [str(s) for s in srcs] + ['-ldl'],
                       check=True)
    return out


def run_host(script_text, rom=ROM_VANILLA, opts=(), name='script', timeout=600):
    """Runs the host on a script; returns the parsed JSON lines."""
    host = build_host()
    WORK.mkdir(parents=True, exist_ok=True)
    path = WORK / (name + '.txt')
    path.write_text(script_text)
    cmd = [str(host), str(CORE), str(rom), str(path)]
    for o in opts:
        cmd += ['--opt', o]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)
    if r.returncode:
        raise RuntimeError('host failed (%d): %s' % (r.returncode, r.stderr[-500:]))
    out = []
    for ln in r.stdout.splitlines():
        if ln.startswith('{'):
            out.append(json.loads(ln))
    return out
