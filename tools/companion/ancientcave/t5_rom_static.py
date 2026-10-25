#!/usr/bin/env python3
"""T5: ROM-side assumptions of the companion direct layer on the generated Ancient Cave ROMs (static, no emulator).

    t5_rom_static.py ROM [ROM...]

Per ROM: the nine routine signatures that tools/companion/direct/som_direct.c checks (SIGS), the spell table (42 rows of 64 bytes at
0x102AD0, target class at +8 and +15 used by somd_spell_class / somd_spell_mp_cost) compared with the vanilla US ROM, the free
stub area, and the size/header facts.  Prints one JSON object per ROM.
"""
import hashlib
import json
import pathlib
import sys

VANILLA = pathlib.Path('/home/cidwel/projects/rommancer/roms/snes/Secret of Mana (USA).smc')
SIGS = [(0xC00036, [0x4C, 0x1E, 0x38]), (0xC0001B, [0x4C, 0x03, 0x38]), (0xC0002A, [0x4C, 0x12, 0x38]), (0xC0E9F4, [0x20, 0x19, 0xEA, 0x6B]),
        (0xC188C7, [0xE2, 0x20, 0x86, 0x14]), (0xC1DA1E, [0x20, 0xC7, 0x88, 0x6B]), (0xC078C3, [0x08, 0xE2, 0x30, 0xA0]),
        (0xC0B079, [0x24, 0xEC, 0xD0, 0xFC]), (0xC0C0A8, [0xC2, 0x30, 0x48])]
SPELL_ROW = 0x102AD0


def off(a):
    return ((a >> 16) - 0xC0) * 0x10000 + (a & 0xFFFF)


def load(p):
    b = pathlib.Path(p).read_bytes()
    return b[512:] if len(b) % 0x8000 == 512 else b


def names(r):
    """Name block as som_ui_state.c decode_names reads it: 335 word pointers at 0xA0800, strings from 0xA0000 (letters and digits only)."""
    out = []
    for n in range(335):
        o = 0xA0000 + (r[0xA0800 + 2 * n] | r[0xA0800 + 2 * n + 1] << 8)
        t = ''
        while o < len(r) and r[o] and len(t) < 30:
            g = r[o]
            t += ' ' if g == 0x80 else chr(97 + g - 0x81) if 0x81 <= g <= 0x9A else chr(65 + g - 0x9B) if 0x9B <= g <= 0xB4 else chr(48 + g - 0xB5) if 0xB5 <= g <= 0xBE else '?'
            o += 1
        out.append(t.rstrip())
    return out


def main():
    van = load(VANILLA)
    for p in sys.argv[1:]:
        r = load(p)
        sig = [bytes(r[off(a):off(a) + len(b)]) == bytes(b) for a, b in SIGS]
        rows = []
        for i in range(42):
            a, b = r[SPELL_ROW + 64 * i:SPELL_ROW + 64 * i + 64], van[SPELL_ROW + 64 * i:SPELL_ROW + 64 * i + 64]
            if a != b:
                rows.append(dict(id=i, bytes_differ=[k for k in range(64) if a[k] != b[k]], class_vanilla=b[8], class_rom=a[8], mp_vanilla=b[15], mp_rom=a[15]))
        nv, nr = names(van), names(r)
        diff = {i: (nv[i], nr[i]) for i in range(335) if nv[i] != nr[i]}
        print(json.dumps(dict(rom=pathlib.Path(p).name, size=len(r), sha256=hashlib.sha256(r).hexdigest()[:16], signatures_ok=sum(sig), signatures_total=len(sig),
                              all_signatures=all(sig), spell_rows_changed=rows, spell_class_bytes_equal=all(r[SPELL_ROW + 64 * i + 8] == van[SPELL_ROW + 64 * i + 8] for i in range(42)),
                              spell_cost_bytes_equal=all(r[SPELL_ROW + 64 * i + 15] == van[SPELL_ROW + 64 * i + 15] for i in range(42)),
                              title=bytes(r[0xFFC0:0xFFD5]).decode('ascii', 'replace'), names_empty=sum(1 for t in nr if not t), names_changed=len(diff), name_probes=[nr[0x32], nr[0xBA], nr[0xCF]],
                              item6_item7=[nr[0xBA + 6], nr[0xBA + 7]], names_changed_sample=dict(list(diff.items())[:8]))))


if __name__ == '__main__':
    main()
