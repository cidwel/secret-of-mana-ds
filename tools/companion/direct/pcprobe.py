import sys, pathlib, collections
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest
v, state, mode = sys.argv[1:4]
D = dtest.Direct(v); c = D.core
D.load(state, 4)
if mode == 'ring':
    for _ in range(40): D.frame(1 << 1)
    D.frame(0)
    for _ in range(30): D.frame()
STUB = 0x1FF94; MB = 0x1FF90
orig = bytes(c.read(0x100, 4))
code = bytearray([0xC2, 0x30, 0x48, 0xA3, 0x04, 0x8F, 0x90, 0xFF, 0x7F,   # PC -> 7F:FF90
                  0xE2, 0x20, 0xA3, 0x06, 0x8F, 0x92, 0xFF, 0x7F, 0xC2, 0x20, 0x68, 0x5C, orig[1], orig[2], orig[3]])
c.write(STUB, bytes(code))
c.write(0x100, bytes([0x5C, STUB & 255, (STUB >> 8) & 255, 0x7F]))
hist = collections.Counter()
for _ in range(120):
    D.frame()
    pc = c.u8(MB) | (c.u8(MB + 1) << 8)
    hist[(c.u8(MB + 2), pc)] += 1
for (b, pc), n in hist.most_common(8): print('%02X:%04X' % (b, pc), n)
