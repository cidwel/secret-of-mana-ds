import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest
v, state, kind, hero, arg = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
D = dtest.Direct(v); c = D.core
D.load(state, 4)
print('open', D.open_menu(kind, hero, arg))
last = None
for i in range(400):
    D.frame()
    s = (D.state, D.err, D.hidden(), c.u8(0xD9), c.u8(0x7D), c.u8(0x1812), c.u8(0x1809), c.u8(0x1808), c.u8(0x180B), c.u8(0x1803), c.u8(0x1826), c.u8(0x19F0))
    if s != last: print(i, s); last = s
    if D.state != 'running': break
print('frames', D.frames)
print('close', D.close_menu())
last = None
for i in range(400):
    D.frame()
    s = (D.state, D.err, D.hidden(), c.u8(0x7D), c.u8(0x1812), c.u8(0x1826), c.u8(0x1803))
    if s != last: print(i, s); last = s
    if D.state != 'running': break
print('frames', D.frames, 'edge', c.u8(0xCC), 'ring', c.u8(0x7D))
