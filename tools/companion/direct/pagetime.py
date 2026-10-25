import sys, pathlib, hashlib
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import dtest
v, state = sys.argv[1], sys.argv[2]
D = dtest.Direct(v); c = D.core
for ent in range(7):
    D.load(state, 4)
    c.write(0x1870, bytes([2])); c.write(0x1876, bytes([ent]))
    c.write(0xCC, bytes([c.u8(0xCC) | 4]))
    for _ in range(60): D.frame()
    for _ in range(3): D.frame(1)
    hs = []; sig = []; br = []
    for i in range(260):
        D.frame()
        hs.append(hashlib.md5(c.frame_rgb).hexdigest()[:6]); br.append(sum(c.frame_rgb[::97]))
        sig.append((c.u8(0x7D), c.u8(0x1826), c.u8(0x19F0), c.u8(0x1809), c.u8(0x180B), c.u8(0x1812), c.u8(0x1803), c.u8(0x1847)))
    final = hs[-1]
    stab = next(i for i in range(len(hs)) if all(h == final for h in hs[i:])) if all(h == final for h in hs[-30:]) else None
    changes = [(i, sig[i]) for i in range(len(sig)) if i == 0 or sig[i] != sig[i - 1]]
    fb = sum(br[-20:]) / 20
    first = next(i for i in range(len(br)) if br[i] >= 0.9 * fb)
    print('entry', ent, 'bright>=90% from', first, 'image stable from', stab, 'changes', changes[:6], 'final', sig[-1])
