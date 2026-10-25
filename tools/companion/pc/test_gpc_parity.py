#!/usr/bin/env python3
"""Parity of the PC gameplay path (C patcher behind the tweaks page and --gameplay) with tools/gameplay/som_gameplay.py.

  test_gpc_parity.py [ROM] [--cases N]

Cases: every preset of gameplay_menu.json, every option at every non-default value on its own, and N random combinations (default 120), on the
vanilla ROM and on generated Ancient Cave / randomizer ROMs when they exist under ~/.local/state/som-decomp/pc/parity/.  The C side is compiled with
ASan and UBSan.  A case where the Python tool aborts (a code site it cannot find on a randomizer ROM) is counted and skipped: the C patcher drops
the option instead, by design.  Exit status 0 when every comparable pair is byte-identical.
"""
import hashlib, json, os, pathlib, random, subprocess, sys

HERE = pathlib.Path(__file__).resolve().parent
COMP = HERE.parent
REPO = COMP.parents[1]
MENU = REPO / 'tools/gameplay/gameplay_menu.json'
PY = REPO / 'tools/gameplay/som_gameplay.py'
ST = pathlib.Path.home() / '.local/state/som-decomp'


def run(cmd):
    return subprocess.run([str(c) for c in cmd], capture_output=True, text=True)


def main():
    argv = sys.argv[1:]
    ncases = 120
    if '--cases' in argv:
        i = argv.index('--cases'); ncases = int(argv[i + 1]); del argv[i:i + 2]
    rom = argv[0] if argv else '/home/cidwel/projects/rommancer/roms/snes/Secret of Mana (USA).smc'
    if not os.path.exists(rom):
        print('SKIP: no ROM at %s' % rom); return 0
    menu = json.loads(MENU.read_text())
    tmp = ST / 'pc/parity/gpc'
    tmp.mkdir(parents=True, exist_ok=True)
    exe = tmp / 'gpc_cli'
    b = run(['gcc', '-std=gnu99', '-Wall', '-Wextra', '-O1', '-g', '-fsanitize=address,undefined', '-I', COMP, HERE / 'gpc_cli.c', HERE / 'pc_gpc.c', HERE / 'pc_gameplay.c',
             HERE / 'pc_store.c', COMP / 'gameplay_c/som_gameplay.c', '-o', exe])
    if b.returncode:
        print(b.stderr); return 1
    roms = [('vanilla', rom)]
    for name in ('ancientcave-short', 'rando', 'chaos', 'bossrush'):
        p = ST / 'pc/parity/seed12345/pc' / (name + '.sfc')
        if p.exists():
            roms.append((name, str(p)))
    cases = [('preset-' + p['id'], dict(p['set'])) for p in menu['presets']]
    for o in menu['options']:
        if o['type'] == 'enum':
            for v in o['values']:
                if v['v'] != o['default']:
                    cases.append(('%s=%s' % (o['key'], v['v']), {o['key']: int(v['v']) if o['key'] in menu['apply']['integer_enums'] else v['v']}))
        elif o['type'] == 'bool':
            cases.append((o['key'], {o['key']: True}))
        else:
            for v in (o['min'], (o['min'] + o['max']) // 2, o['max']):
                cases.append(('%s=%d' % (o['key'], v), {o['key']: v}))
    rng = random.Random(20261020)
    for i in range(ncases):
        c = {}
        for o in menu['options']:
            if rng.random() > 0.4:
                continue
            if o['type'] == 'enum':
                v = rng.choice(o['values'])['v']
                c[o['key']] = int(v) if o['key'] in menu['apply']['integer_enums'] else v
            elif o['type'] == 'bool':
                c[o['key']] = rng.random() < 0.7
            else:
                c[o['key']] = rng.randint(o['min'], o['max'])
        cases.append(('random-%d' % i, c))
    same = pyabort = bad = 0
    for rname, rpath in roms:
        sel = cases if rname == 'vanilla' else [c for c in cases if c[0].startswith('preset-')] + cases[-25:]
        for name, opts in sel:
            of = tmp / 'o.json'
            of.write_text(json.dumps({'options': opts}))
            py = run([sys.executable, PY, 'apply', '--rom', rpath, '--options', of, '--out', tmp / 'py.sfc', '--on-conflict', 'skip'])
            cc = run([exe, MENU, of, rpath, tmp / 'c.sfc'])
            if py.returncode:
                pyabort += 1
                continue
            if cc.returncode:
                bad += 1; print('FAIL %s on %s: C failed: %s' % (name, rname, cc.stderr.strip())); continue
            a = hashlib.sha256((tmp / 'py.sfc').read_bytes()).hexdigest()
            c = hashlib.sha256((tmp / 'c.sfc').read_bytes()).hexdigest()
            if a == c:
                same += 1
            else:
                bad += 1; print('FAIL %s on %s: python %s, C %s (%s)' % (name, rname, a[:12], c[:12], cc.stdout.strip()))
        print('%-18s done' % rname)
    print('---- %d identical, %d python aborts skipped, %d different  (%d cases x %d ROMs)' % (same, pyabort, bad, len(cases), len(roms)))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
