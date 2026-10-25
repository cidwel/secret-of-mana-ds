import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest
v, state = sys.argv[1:3]
pre = [a.split('=') for a in sys.argv[3:]]
D = dtest.Direct(v); c = D.core
D.load(state, 4)
print('D9', c.u8(0xD9), 'CC..CE', bytes(c.read(0xCC, 3)).hex(), '1870', bytes(c.read(0x1870, 12)).hex())
for k, val in pre:
    c.write(int(k, 16), bytes([int(val, 0)]))
c.write(0xCC, bytes([c.u8(0xCC) | 4]))
def row(i):
    return (i, c.u8(0x7D), c.u8(0x1812), c.u8(0x1809), c.u8(0x1808), c.u8(0x180A), c.u8(0x180B), c.u8(0xD9), c.u8(0x1803))
last = None
for i in range(80):
    D.frame()
    r = row(i)[1:]
    if r != last: print(row(i)); last = r
print('1870', bytes(c.read(0x1870, 12)).hex())
