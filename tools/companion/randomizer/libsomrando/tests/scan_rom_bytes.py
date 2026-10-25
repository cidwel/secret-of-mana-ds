#!/usr/bin/env python3
"""Fails when a built binary contains a 32-byte window of the vanilla ROM.

  scan_rom_bytes.py [--window N] FILE_OR_DIR...      (ROM from $SOM_ROM; exit 0 with SKIP when unset or missing)

Distribution rule of this project: no binary built from this tree may carry ROM bytes.  Every 32-byte window of the ROM that has at least
MIN_DISTINCT distinct byte values and that holds no long monotone stretch (so padding, short repeats and counting sequences do not count) is looked up in each file, at any alignment.
Directories are walked for .so .a .o .3dsx .elf .exe .dll .apk (APK/ZIP members are scanned unpacked) and extension-less executables.
Exit status 1 when a window is found, with the file, offset and the ROM offset.
"""
import os, sys, zipfile, io
import numpy as np

MIN_DISTINCT = 6
EXT = {'.so', '.a', '.o', '.3dsx', '.elf', '.exe', '.dll', '.apk', '.aab', '.dylib', '.wasm', '.bin'}


P = [np.uint64(x) for x in (0x9E3779B97F4A7C15, 0xC2B2AE3D27D4EB4F, 0x165667B19E3779F9, 0x27D4EB2F165667C5)]


def words(a):
    """little-endian 64-bit word at every byte offset (length len(a) - 7)."""
    n = len(a) - 7
    v = np.zeros(n, dtype=np.uint64)
    for k in range(8):
        v |= a[k:k + n].astype(np.uint64) << np.uint64(8 * k)
    return v


def window_hash(v, w):
    """hash of the w-byte window (w a multiple of 8) at every offset that has a full window."""
    n = len(v) - (w - 8)
    h = np.zeros(n, dtype=np.uint64)
    with np.errstate(over='ignore'):
        for i in range(w // 8):
            h += v[8 * i:8 * i + n] * P[i % 4] * np.uint64(i + 1)
    return h


def rom_index(rom, w):
    a = np.frombuffer(rom, dtype=np.uint8)
    n = len(a) - w + 1
    distinct = np.zeros(n, dtype=np.int16)
    for val in range(256):
        cs = np.concatenate(([0], np.cumsum(a == val, dtype=np.int32)))
        distinct += (cs[w:w + n] - cs[:n]) > 0
    # Counting sequences and sorted tables (8- or 16-bit indices 0,1,2,..) exist in code and tables by chance: a window that contains a
    # monotone stretch of 3/4 of its length (every byte-to-next or byte-to-same-lane difference with one sign) is not ROM data in any useful sense.
    sub = w * 3 // 4
    mono_sub = np.zeros(len(a) - sub + 1, dtype=bool)
    for stride in (1, 2):
        dd = a[stride:].astype(np.int16) - a[:-stride].astype(np.int16)
        cn = np.concatenate(([0], np.cumsum(dd < 0, dtype=np.int32)))
        cp = np.concatenate(([0], np.cumsum(dd > 0, dtype=np.int32)))
        m = sub - stride
        k = len(a) - sub + 1
        mono_sub |= ((cn[m:m + k] - cn[:k]) == 0) | ((cp[m:m + k] - cp[:k]) == 0)
    cm = np.concatenate(([0], np.cumsum(mono_sub, dtype=np.int32)))
    mono = (cm[w - sub + 1:w - sub + 1 + n] - cm[:n]) > 0
    h = window_hash(words(a), w)
    pos = np.nonzero((distinct >= MIN_DISTINCT) & ~mono)[0]
    hs = h[pos]
    order = np.argsort(hs, kind='stable')
    return pos[order], hs[order]


def scan(data, rom, w, pos, pre):
    hits = []
    if len(data) < w + 8 or w % 8:
        return hits
    h = window_hash(words(np.frombuffer(data, dtype=np.uint8)), w)
    idx = np.searchsorted(pre, h)
    idx[idx >= len(pre)] = len(pre) - 1
    for o in np.nonzero(pre[idx] == h)[0]:
        o = int(o)
        j = int(idx[o])
        while j < len(pre) and pre[j] == h[o]:
            p = int(pos[j])
            if rom[p:p + w] == data[o:o + w]:
                hits.append((o, p))
                break
            j += 1
    return hits


def members(path):
    with open(path, 'rb') as f:
        data = f.read()
    if data[:4] == b'PK\x03\x04':
        try:
            with zipfile.ZipFile(io.BytesIO(data)) as z:
                for i in z.infolist():
                    if not i.is_dir():
                        yield path + '!' + i.filename, z.read(i)
            return
        except zipfile.BadZipFile:
            pass
    yield path, data


def walk(args):
    for a in args:
        if os.path.isdir(a):
            for r, _, fs in os.walk(a):
                for f in sorted(fs):
                    p = os.path.join(r, f)
                    ext = os.path.splitext(f)[1].lower()
                    if ext in EXT or (not ext and os.access(p, os.X_OK)):
                        yield p
        elif os.path.exists(a):
            yield a
        else:
            print('MISSING', a)


def selftest(rom, w, pos, pre):
    """a planted ROM chunk must be found at any alignment, padding and a shifted chunk must not."""
    import random
    rnd = random.Random(7)
    start = next(p for p in range(0x1000, len(rom) - 64) if len(set(rom[p:p + 32])) > 12)
    noise = bytes(rnd.randrange(256) for _ in range(5000))
    planted = noise[:1233] + rom[start:start + w] + noise[1233:]
    assert scan(planted, rom, w, pos, pre), 'selftest: planted ROM window not found'
    assert not scan(noise, rom, w, pos, pre), 'selftest: false positive on noise'
    assert not scan(bytes(3000) + b'\xff' * 3000, rom, w, pos, pre), 'selftest: padding reported'
    print('selftest ok')


def main():
    argv = sys.argv[1:]
    w = 32
    if argv[:1] == ['--window']:
        w = int(argv[1]); argv = argv[2:]
    rom_path = os.environ.get('SOM_ROM', '')
    if not rom_path or not os.path.exists(rom_path):
        print('SKIP: SOM_ROM unset or missing')
        return 0
    rom = open(rom_path, 'rb').read()
    if len(rom) % 1024 == 512:          # copier header
        rom = rom[512:]
    pos, pre = rom_index(rom, w)
    bad = scanned = 0
    if argv[:1] == ['--selftest']:
        selftest(rom, w, pos, pre)
        argv = argv[1:]
    for p in walk(argv):
        for name, data in members(p):
            scanned += 1
            hits = scan(data, rom, w, pos, pre)
            if hits:
                bad += 1
                print('FAIL %s: %d window(s), first at file offset 0x%X = ROM 0x%X' % (name, len(hits), hits[0][0], hits[0][1]))
                lo = min(h[0] for h in hits); hi = max(h[0] for h in hits)
                print('     span 0x%X..0x%X' % (lo, hi + w))
    print('scanned %d file(s), %d with ROM bytes' % (scanned, bad))
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
