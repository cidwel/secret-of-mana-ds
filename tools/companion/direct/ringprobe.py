import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest
v, state = sys.argv[1:3]
pre = dict(a.split('=') for a in sys.argv[3:])
D = dtest.Direct(v); c = D.core
D.load(state, 4)
for k, val in pre.items():
    c.write(int(k, 16), bytes([int(val, 0)]))
def row(i):
    return (i, c.u8(0x7D), c.u8(0x1812), c.u8(0x1809), c.u8(0x1808), c.u8(0x180A), c.u8(0x180B), c.u8(0x1803), c.u8(0x1814))
print('start', row(-1))
for i in range(75):
    D.frame(2 if i < 3 else 0)    # bit1 = Y
    if i < 12 or i % 4 == 0 or c.u8(0x1812) == 0 and i < 60:
        print(row(i))
