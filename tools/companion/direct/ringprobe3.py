import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest
v, state = sys.argv[1:3]
pre = [a.split('=') for a in sys.argv[3:]]
D = dtest.Direct(v); c = D.core
D.load(state, 4)
for k, val in pre:
    c.write(int(k, 16), bytes([int(val, 0)]))
c.write(0xCC, bytes([c.u8(0xCC) | 4]))
for i in range(60): D.frame()
print('ready', c.u8(0x7D), c.u8(0x1812), 'cat', c.u8(0x1809), 'own', c.u8(0x1808), 'cur', c.u8(0x180A), 'cnt', c.u8(0x180B))
print('1820..1840', bytes(c.read(0x1820, 0x21)).hex())
print('1870..187F', bytes(c.read(0x1870, 16)).hex())
print('items', bytes(c.read(0xCC48, 12)).hex(), 'wpn', bytes(c.read(0xCC54, 8)).hex())
