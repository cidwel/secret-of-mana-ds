import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest
from PIL import Image
v, state, ent = sys.argv[1], sys.argv[2], int(sys.argv[3])
D = dtest.Direct(v); c = D.core
D.load(state, 4)
B, A, Y = 1<<0, 1<<8, 1<<1
def snap(): return bytes(c.read(0x1800, 0x60)) + bytes(c.read(0xD0, 1)) + bytes(c.read(0x7D, 1))
def st(): return dict(r=c.u8(0x7D), ph=c.u8(0x1812), cat=c.u8(0x1809), cur=c.u8(0x180A), cnt=c.u8(0x180B), f03=c.u8(0x1803), f04=c.u8(0x1804), a47=c.u8(0x1847), a06=c.u8(0x1806), d0=c.u8(0xD0), a26=c.u8(0x1826), mb=bytes(c.read(0x19F0,4)).hex())
c.write(0x1870, bytes([2])); c.write(0x1876, bytes([ent]))
c.write(0xCC, bytes([c.u8(0xCC) | 4]))
for _ in range(60): D.frame()
print('ring', st())
b0 = snap()
for _ in range(3): D.frame(B)
last = None
for i in range(150):
    D.frame()
    s = st()
    if s != last and i % 1 == 0:
        print(i, s); last = s
    if i in (30, 100):
        Image.frombytes('RGB', (c.width, c.height), c.frame_rgb).save('/tmp/claude-1000/-home-cidwel-projects-som-decomp/c17d9ba5-56df-4d0c-b798-dd323a0d3003/scratchpad/page_%d_%d.png' % (ent, i))
