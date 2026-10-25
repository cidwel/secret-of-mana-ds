import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest
v, state = sys.argv[1:3]
D = dtest.Direct(v); c = D.core
D.load(state, 4)
D.open_menu(3, 0, 1)
while D.state == 'running': D.frame()
print('open', D.err, c.u8(0x1803), c.u8(0x1804))
for _ in range(4): D.frame(1 << 1)    # real Y
for _ in range(60): D.frame()
print('after real Y', c.u8(0x7D), c.u8(0x1803), c.u8(0x1804), c.u8(0x1826))
for _ in range(4): D.frame(1 << 1)
for _ in range(60): D.frame()
print('after 2nd Y', c.u8(0x7D), c.u8(0x1803), c.u8(0x1804), c.u8(0x1826))
