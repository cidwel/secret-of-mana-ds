#!/usr/bin/env python3
"""Refuses files that carry Secret of Mana ROM data, so the ROM never reaches git.

Checks, for every file given (or every staged file with --staged):
  1. ROM-like files: .sfc/.smc/.fig/.swc/.srm/.sst extensions, or any file of 1 MiB or more.
  2. The SNES internal header "SECRET OF MANA" at 0x7FC0/0xFFC0 (also after a 512-byte copier header).
  3. With the ROM at hand (SOM_ROM=path), runs of 64 bytes copied from it, either raw or written as
     0xNN / hex text in source files.  Runs with fewer than 12 distinct byte values are ignored (fill, zeros).
Exit status 1 and one line per offending file when anything is found.
"""
import os
import pathlib
import re
import subprocess
import sys

WINDOW = 64
MIN_DISTINCT = 12
MAX_SIZE = 1 << 20
ROM_EXT = {'.sfc', '.smc', '.fig', '.swc', '.srm', '.sst', '.rom'}
TITLE = b'SECRET OF MANA'
HEX_ITEM = re.compile(rb'(?:0x|\$)([0-9A-Fa-f]{2})\b')
HEX_LIST = re.compile(rb'(?:(?:0x|\$)[0-9A-Fa-f]{2}\b[\s,]*(?:\.?(?:byte|db|BYTE|DB)\s+)?){%d,}' % WINDOW)
HEX_RUN = re.compile(rb'(?:[0-9A-Fa-f]{2}[ ,]?){%d,}' % WINDOW)


def load_rom(path):
    d = pathlib.Path(path).read_bytes()
    if len(d) % 0x8000 == 512:
        d = d[512:]
    return d


def rom_index(rom):
    # Only windows starting at multiples of 16 are indexed; the scan checks every offset of the file,
    # so any copied run of WINDOW + 15 bytes still hits one indexed window.
    idx = set()
    for off in range(0, len(rom) - WINDOW + 1, 16):
        w = rom[off:off + WINDOW]
        if len(set(w)) >= MIN_DISTINCT:
            idx.add(w)
    return idx


def has_rom_run(data, idx):
    for off in range(0, len(data) - WINDOW + 1):
        w = data[off:off + WINDOW]
        if w in idx and len(set(w)) >= MIN_DISTINCT:
            return off
    return None


def text_bytes(data):
    """Byte strings written as text: 0xNN/$NN lists and plain hex dumps."""
    out = [bytes(int(h, 16) for h in HEX_ITEM.findall(m.group(0))) for m in HEX_LIST.finditer(data)]
    for m in HEX_RUN.finditer(data):
        out.append(bytes.fromhex(re.sub(rb'[ ,]', b'', m.group(0)).decode()))
    return out


def check(path, data, idx):
    p = pathlib.Path(path)
    if p.suffix.lower() in ROM_EXT:
        return 'ROM or save extension'
    if len(data) >= MAX_SIZE:
        return 'file of 1 MiB or more'
    for base in (0x7FC0, 0xFFC0, 0x7FC0 + 512, 0xFFC0 + 512):
        if data[base:base + len(TITLE)] == TITLE:
            return 'SNES header "SECRET OF MANA" at 0x%X' % base
    if idx:
        off = has_rom_run(data, idx)
        if off is not None:
            return '%d bytes copied from the ROM at file offset 0x%X' % (WINDOW, off)
        for blob in text_bytes(data):
            off = has_rom_run(blob, idx)
            if off is not None:
                return '%d ROM bytes written as hex text' % WINDOW
    return None


def staged():
    out = subprocess.run(['git', 'diff', '--cached', '--name-only', '--diff-filter=ACMR', '-z'],
                         capture_output=True, check=True).stdout
    for name in filter(None, out.split(b'\0')):
        blob = subprocess.run(['git', 'show', b':' + name], capture_output=True, check=True).stdout
        yield name.decode(), blob


def files(args):
    for a in args:
        for p in ([pathlib.Path(a)] if os.path.isfile(a) else
                  (q for q in pathlib.Path(a).rglob('*') if q.is_file() and '.git' not in q.parts)):
            yield str(p), p.read_bytes()


def main(argv):
    rom_path = os.environ.get('SOM_ROM')
    idx = rom_index(load_rom(rom_path)) if rom_path and os.path.isfile(rom_path) else None
    items = staged() if argv[:1] == ['--staged'] else files(argv or ['.'])
    bad = [(n, why) for n, d in items if (why := check(n, d, idx))]
    for n, why in bad:
        print('rom_guard: %s: %s' % (n, why), file=sys.stderr)
    if not idx:
        print('rom_guard: SOM_ROM not set, byte comparison against the ROM skipped', file=sys.stderr)
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
