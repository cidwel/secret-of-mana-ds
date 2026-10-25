import sys, pathlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest
v, state = sys.argv[1:3]
D = dtest.Direct(v); c = D.core
D.load(state, 4)
B, Y, X, UP, DOWN, LEFT, RIGHT, A = 1<<0, 1<<1, 1<<9, 1<<4, 1<<5, 1<<6, 1<<7, 1<<8
def tap(m, n=3, gap=25):
    for _ in range(n): D.frame(m)
    for _ in range(gap): D.frame(0)
def st(): return dict(d9=c.u8(0xD9), r=c.u8(0x7D), ph=c.u8(0x1812), cat=c.u8(0x1809), own=c.u8(0x1808), cur=c.u8(0x180A), cnt=c.u8(0x180B), f1803=c.u8(0x1803), s=bytes(c.read(0x1870, 12)).hex(), e=c.u8(0x182E))
print('start', st(), 'masks h1', bytes(c.read(0xE200+0x1C8, 8)).hex())
# switch pad to girl with the game's own field cycle
c.write(0xCC, bytes([c.u8(0xCC) | 2]))
for _ in range(30): D.frame()
print('after cycle', st())
tap(Y, 3, 60)
print('ring', st())
for k in range(4):
    if c.u8(0x1809) == 1: break
    tap(UP)
print('magic?', st())
for n in range(3):
    tap(LEFT, 3, 25)
    print('left', n + 1, st())
tap(Y, 3, 60)
print('closed', st())
