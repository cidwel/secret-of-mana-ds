#!/usr/bin/env python3
"""Compares the output of test/rando_batch.c built for the 3DS with the host build: same preset ids, same status, same ROM SHA-256.

  compare_rando_batch.py <3ds rando_batch.txt> <host lines.txt>
"""
import sys


def parse(path):
    d = {}
    for line in open(path):
        f = line.split()
        if len(f) < 2 or f[0] == 'done':
            continue
        d[f[0]] = (f[1], f[2] if len(f) > 2 and not f[2].startswith('ms=') else '')
    return d


a, b = parse(sys.argv[1]), parse(sys.argv[2])
bad = [k for k in b if a.get(k) != b[k]]
missing = [k for k in b if k not in a]
print('presets on the host: %d, on the 3DS: %d, identical: %d, differ: %s, missing on the 3DS: %s' % (len(b), len(a), len(b) - len(bad), [k for k in bad if k not in missing], missing))
sys.exit(1 if bad else 0)
