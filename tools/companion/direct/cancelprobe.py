import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest
v, state = sys.argv[1:3]
D = dtest.Direct(v); c = D.core
D.load(state, 4)
D.open_menu(2, 1, 3)
while D.state == 'running': D.frame()
def st(): return (c.u8(0x7D), c.u8(0x1812), c.u8(0x1809), c.u8(0x180B), c.u8(0x1803), c.u8(0x1826))
print('sub ring', st())
for name, bit in (('A', 8), ('X', 9), ('Y', 1), ('B', 0)):
    for _ in range(4): D.frame(1 << bit)
    for _ in range(40): D.frame()
    print(name, st())
