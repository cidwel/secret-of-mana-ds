import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest
from PIL import Image
v, state, ent = sys.argv[1], sys.argv[2], int(sys.argv[3])
D = dtest.Direct(v); c = D.core
D.load(state, 4)
D.open_menu(3, 0, ent)
while D.state == 'running': D.frame()
print('open', D.state, D.err, D.frames)
def st(): return (c.u8(0x7D), c.u8(0x1826), c.u8(0x19F0), c.u8(0x1812), c.u8(0x1803), c.u8(0xE4), c.u8(0xE6), c.u8(0xD0))
Image.frombytes('RGB', (c.width, c.height), c.frame_rgb).save('/tmp/claude-1000/-home-cidwel-projects-som-decomp/c17d9ba5-56df-4d0c-b798-dd323a0d3003/scratchpad/ex_%d.png' % ent)
print(st())
for name, bit in (('B', 0), ('Y', 1), ('A', 8), ('X', 9), ('START', 3), ('SELECT', 2), ('L', 10), ('R', 11)):
    for _ in range(4): D.frame(1 << bit)
    for _ in range(40): D.frame()
    print(name, st())
