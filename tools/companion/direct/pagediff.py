import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest
v, state, ent = sys.argv[1], sys.argv[2], int(sys.argv[3])
D = dtest.Direct(v); c = D.core
D.load(state, 4)
B = 1
c.write(0x1870, bytes([2])); c.write(0x1876, bytes([ent]))
c.write(0xCC, bytes([c.u8(0xCC) | 4]))
for _ in range(60): D.frame()
ring = c.dump_wram()
for _ in range(3): D.frame(B)
tl = []
for i in range(200):
    D.frame()
    tl.append((i, c.u8(0x7D), c.u8(0xD0), c.u8(0xE4), c.u8(0xE6), c.u8(0x1847), c.u8(0x1826), c.u8(0x19F0), c.u8(0xF1), c.u8(0x7C), c.u8(0x7E)))
for t in tl[::10]: print(t)
page = c.dump_wram()
d = [(i, ring[i], page[i]) for i in range(0x0, 0x2000) if ring[i] != page[i]]
print(len(d), [(hex(i), hex(a), hex(b)) for i, a, b in d[:60]])
