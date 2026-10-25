import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest, numpy as np
from PIL import Image
v, state = sys.argv[1:3]
D = dtest.Direct(v); c = D.core
D.load(state, 4)
c.write(0xCC, bytes([c.u8(0xCC) | 4]))
for i in range(60): D.frame()
print('ready', c.u8(0x7D), c.u8(0x1812), '1826', c.u8(0x1826), '19F0', bytes(c.read(0x19F0,4)).hex(), '1847', c.u8(0x1847))
c.write(0x1826, bytes([0]))
for i in range(40):
    D.frame()
    a = np.frombuffer(c.frame_rgb, np.uint8)
    print(i, '7D', c.u8(0x7D), '1812', c.u8(0x1812), '1826', c.u8(0x1826), 'D0', c.u8(0xD0), 'E4/E6', c.u8(0xE4), c.u8(0xE6), 'sum', int(a.sum() // 1000))
    if i in (0, 2, 5, 12, 39):
        Image.frombytes('RGB', (c.width, c.height), c.frame_rgb).save('/tmp/claude-1000/-home-cidwel-projects-som-decomp/c17d9ba5-56df-4d0c-b798-dd323a0d3003/scratchpad/close_%02d.png' % i)
