#!/usr/bin/env python3
"""Black-row scan through the C host: ring macro vs direct, per frame.

    python3 blackhost.py VARIANT STATE ring|direct equip HERO TYPE
"""
import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import test_direct_cast as T  # noqa: E402

variant, state, mode, kind = sys.argv[1:5]
a = sys.argv[5:]
if mode == 'ring':
    cmd = 'CMD %s %s' % (kind, ' '.join(a))
    body = 'LOAD %s\nRUN 4\nBLACKSCAN 1\n%s\nWAIT 900\nRUN 120\n' % (state, cmd)
else:
    dc = {'equip': 'DEQUIP', 'item': 'DITEM', 'cast': 'DCAST'}[kind]
    body = 'LOAD %s\nRUN 4\nBLACKSCAN 1\n%s %s\nDWAIT 60\nRUN 120\n' % (state, dc, ' '.join(a))
res = T.run_script(variant, body, 'black')
for r in res:
    print(r)
