#!/usr/bin/env python3
"""area_names.json (the evidence table) must agree with som_ux_area_name() for every map id 0..511: same name where listed, no name
elsewhere, ranges well formed and not contradicting each other, confidence stated."""
import ctypes as C
import json
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1]))
import ux_lib  # noqa: E402

HERE = pathlib.Path(__file__).resolve().parent


def main():
    d = json.load(open(HERE / 'area_names.json'))
    lib = C.CDLL(str(ux_lib.build()))
    lib.som_ux_area_name.restype = C.c_char_p
    want = {}
    bad = []
    for r in d['ranges']:
        if r['confidence'] not in ('high', 'medium', 'low') or r['evidence'] not in d['evidence'] or r['lo'] > r['hi']:
            bad.append('malformed range %r' % r)
        for i in range(r['lo'], r['hi'] + 1):
            if i in want and want[i] != r['name']:
                bad.append('map %d has two names: %s / %s' % (i, want[i], r['name']))
            want.setdefault(i, r['name'])
    for i in range(512):
        got = lib.som_ux_area_name(i)
        got = got.decode() if got else None
        if got != want.get(i):
            bad.append('map %d: C says %r, json says %r' % (i, got, want.get(i)))
    for b in bad[:20]:
        print('FAIL', b)
    print('area names: %s (%d maps named)' % ('FAILED' if bad else 'OK', len(want)))
    sys.exit(1 if bad else 0)


if __name__ == '__main__':
    main()
