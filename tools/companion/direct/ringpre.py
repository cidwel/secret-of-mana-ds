import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest
v, state = sys.argv[1:3]
pre = [a.split('=') for a in sys.argv[3:]]
D = dtest.Direct(v); c = D.core
D.load(state, 4)
B, Y, UP, LEFT = 1<<0, 1<<1, 1<<4, 1<<6
def st(): return dict(r=c.u8(0x7D), ph=c.u8(0x1812), cat=c.u8(0x1809), own=c.u8(0x1808), cur=c.u8(0x180A), cnt=c.u8(0x180B), f1803=c.u8(0x1803), e=c.u8(0x182E), s=bytes(c.read(0x1870, 10)).hex(), sel=hex(c.u16(0x1839)))
c.write(0xCC, bytes([c.u8(0xCC) | 2]))
for _ in range(30): D.frame()
for k, val in pre: c.write(int(k, 16), bytes([int(val, 0)]))
c.write(0xCC, bytes([c.u8(0xCC) | 4]))
for _ in range(60): D.frame()
print('ready', st())
for _ in range(3): D.frame(B)
for _ in range(60): D.frame()
print('after B', st())
