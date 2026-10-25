#!/usr/bin/env python3
"""Prints the coverage of the enemy atlas (markdown) from <state>/enemy_sprites.json.  usage: coverage_report.py [STATE_DIR]"""
import collections
import json
import pathlib
import sys

S = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else pathlib.Path('/home/cidwel/.local/state/som-decomp/companion-20261017/enemysprites')
m = json.load(open(S / 'enemy_sprites.json'))['sprites']
for title, lo, hi in (('Ordinary monsters (ids 0..0x56)', 0, 0x57), ('Boss-engine actors (ids 0x57..0x7F)', 0x57, 0x80)):
    sel = [e for e in m if lo <= e['id'] < hi]
    c = collections.Counter(e['source'] for e in sel)
    print('### %s: %d ids' % (title, len(sel)))
    print()
    print('| Source | ids |')
    print('| --- | ---: |')
    for k in ('godot-rom-rip', 'game-rip', 'live', 'none'):
        print('| %s | %d |' % (k, c.get(k, 0)))
    print()
    for k in ('godot-rom-rip', 'game-rip', 'live', 'none'):
        ids = [e for e in sel if e['source'] == k]
        if ids:
            print('* %s: %s' % (k, ', '.join('%d %s%s' % (e['id'], e['name'], '' if k == 'none' else ' (c%d)' % e['confidence']) for e in ids)))
    print()
