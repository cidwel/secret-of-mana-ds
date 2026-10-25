#!/usr/bin/env python3
"""Byte-identity of the PC generation (som-pc, libsomrando linked in) against the standalone C port (sr_cli) and the C# oracle (the upstream
generator built as a NativeAOT executable), for every preset of the start menu and two seeds.

  test_rando_parity.py [ROM] [--seeds 12345,777]

ROM: the owner's US v1.0 image (default: the rommancer path).  The C# oracle is the `somrando` executable of the companion CLI state
(SOM_RANDO_ORACLE overrides).  Everything is written under ~/.local/state/som-decomp/pc/parity/.  A preset that the library reports as not
ported yet (rc 5) is listed as PENDING and does not fail the run; a mismatch, or a preset where only one side fails, does.  Exit status 0 when
no supported preset differs.
"""
import hashlib, json, os, pathlib, subprocess, sys

HERE = pathlib.Path(__file__).resolve().parent
COMP = HERE.parent
sys.path.insert(0, str(COMP / 'cli'))
import launcher_config as lc  # noqa: E402

ST = pathlib.Path.home() / '.local/state/som-decomp'
OUT = pathlib.Path(os.environ.get('SOM_PC_BUILD', ST / 'pc/build'))
WORK = ST / 'pc/parity'
ORACLE = pathlib.Path(os.environ.get('SOM_RANDO_ORACLE', ST / 'companion-cli-20261017/aot/out/somrando'))
SRC_RANDO = ST / 'companion-cli-20261017/repo-moppu/SecretOfManaRandomizer/SoMRandomizer'


def sha(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()


def run(cmd, **kw):
    return subprocess.run([str(c) for c in cmd], capture_output=True, text=True, **kw)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith('--')]
    seeds = ['12345', '777']
    if '--seeds' in sys.argv:
        seeds = sys.argv[sys.argv.index('--seeds') + 1].split(',')
        args = [a for a in args if a != sys.argv[sys.argv.index('--seeds') + 1]]
    rom = args[0] if args else '/home/cidwel/projects/rommancer/roms/snes/Secret of Mana (USA).smc'
    if not os.path.exists(rom) or not ORACLE.exists() or not SRC_RANDO.exists():
        print('SKIP: ROM, C# oracle or upstream sources missing (%s, %s)' % (rom, ORACLE))
        return 0
    r = run([HERE / 'som-pc', '--help'])                       # builds som-pc (and libsomrando) when needed
    lib = OUT / 'rando/libsomrando.a'
    if not lib.exists() or not (OUT / 'som-pc.gen').read_text().startswith('rando'):
        print('SKIP: som-pc was built without libsomrando')
        return 0
    WORK.mkdir(parents=True, exist_ok=True)
    cli = WORK / 'sr_cli'
    if not cli.exists() or cli.stat().st_mtime < lib.stat().st_mtime:
        b = run(['gcc', '-std=c99', '-D_POSIX_C_SOURCE=200809L', '-I', COMP / 'launcher', COMP / 'randomizer/libsomrando/tests/sr_cli.c', lib, '-lm', '-o', cli])
        if b.returncode:
            print(b.stderr)
            return 1
    cat = lc.catalogue()
    bad = ok = pending = 0
    for seed in seeds:
        d = WORK / ('seed' + seed)
        d.mkdir(parents=True, exist_ok=True)
        r = run([OUT / 'som-pc', '--rom', rom, '--generate-all', d / 'pc', '--gen-seed', seed])
        lines = {}
        for ln in r.stdout.splitlines():
            j = json.loads(ln)
            lines[j['preset']] = j
        for p in cat['presets']:
            pid = p['id']
            cfg = lc.from_preset(pid, seed)
            mode, diff = cfg['mode'], cfg.get('difficulty')
            opt = []
            for k, v in sorted(cfg['options'].items()):
                opt += ['--set', '%s=%s' % (k, v)]
            base = ['--mode', mode, '--seed', cfg['seed']] + (['--difficulty', diff] if diff else []) + opt
            pc = lines.get(pid)
            if pc is None:
                print('FAIL %s seed %s: som-pc printed nothing' % (pid, seed)); bad += 1; continue
            sr = run([cli, *base, '--src', rom, '--dst', d / (pid + '.sr.sfc')])
            if pc.get('rc', 5) == 5 or not pc['supported']:
                pending += 1
                print('PENDING %-22s seed %-6s %s (sr_cli exit %d)' % (pid, seed, pc.get('error') or pc.get('why'), sr.returncode))
                if sr.returncode == 0:
                    print('FAIL %s: som-pc refuses but sr_cli generates' % pid); bad += 1
                continue
            w = d / ('work-' + pid)
            w.mkdir(exist_ok=True)
            cs = run([ORACLE, *base, '--src', rom, '--dst', d / (pid + '.cs.sfc'), '--workdir', w])
            a, b, c = pc['sha256'], (sha(d / (pid + '.sr.sfc')) if sr.returncode == 0 else 'sr_cli failed'), (sha(d / (pid + '.cs.sfc')) if cs.returncode == 0 else 'oracle failed: ' + cs.stderr[-120:])
            side = json.loads((d / 'pc' / (pid + '.sfc.json')).read_text())
            key_ok = side['config_key'] == cfg['key']
            if a == b == c and key_ok:
                ok += 1
                print('PASS    %-22s seed %-6s %s key %s' % (pid, seed, a[:16], cfg['key']))
            else:
                bad += 1
                print('FAIL    %-22s seed %-6s som-pc %s  sr_cli %s  oracle %s  key %s/%s' % (pid, seed, a[:16], b[:16], c[:40], side['config_key'], cfg['key']))
    print('---- %d identical (som-pc = sr_cli = C# oracle, same config key), %d pending in the library, %d failed' % (ok, pending, bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
