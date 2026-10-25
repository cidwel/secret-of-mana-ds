#!/usr/bin/env python3
"""Cross-check: the configurations written by the C start menu (test_launcher --emit, tab separated) are valid against
presets.json, byte-identical to the canonical JSON, and their key string, key and command line equal the Python reference.
Usage: test_launcher_config.py cases.tsv"""
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'cli'))
import launcher_config as lc  # noqa: E402


def main(path):
    cat = lc.catalogue()
    n = bad = 0
    for line in pathlib.Path(path).read_text().splitlines():
        if not line.startswith('CASE\t'):
            continue
        _, name, js, key, ks, cli = line.split('\t')
        cfg = json.loads(js)
        n += 1
        problems = lc.validate(cfg, cat)
        if lc.canonical(cfg) != js:
            problems.append('JSON is not canonical')
        if lc.keystring(cfg) != ks:
            problems.append('key string differs: %r vs %r' % (lc.keystring(cfg), ks))
        if lc.key(cfg) != key or cfg['key'] != key:
            problems.append('key differs')
        if ' '.join(lc.cli_args(cfg)) != cli:
            problems.append('command line differs: %r vs %r' % (' '.join(lc.cli_args(cfg)), cli))
        if name in {p['id'] for p in cat['presets']}:
            ref = lc.from_preset(name, cfg['seed'])
            if lc.canonical(ref) != js:
                problems.append('from_preset differs from the menu output')
        if problems:
            bad += 1
            print('FAIL %s: %s' % (name, '; '.join(problems)))
    print('%s: %d configurations cross-checked, %d failed' % ('FAIL' if bad or not n else 'PASS', n, bad))
    return 1 if bad or not n else 0


sys.exit(main(sys.argv[1]))
