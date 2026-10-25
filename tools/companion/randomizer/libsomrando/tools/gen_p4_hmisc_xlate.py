#!/usr/bin/env python3
"""Line converter used once to translate the straight-line code-emitting stretches of the hmisc hacks (output is hand-reviewed).

  gen_p4_hmisc_xlate.py <file.cs> <first line> <last line>

outRom[context.workingOffset++] = v -> E(v);  outRom[i] = v -> P(i, v);  `byte[] a = new byte[] {..};` -> static const uint8_t; comments kept.
Original tool of this repository (LGPL-2.1+).
"""
import re
import sys


def conv(e):
    e = e.replace("context.workingOffset", "c->working_offset")
    return re.sub(r"\(byte\)", "(uint8_t)", e)


def main():
    lines = open(sys.argv[1], encoding="utf-8-sig").read().split("\n")
    for raw in lines[int(sys.argv[2]) - 1:int(sys.argv[3])]:
        l = raw.rstrip()
        ind = len(l) - len(l.lstrip())
        pad = "  " * max(1, (ind - 8) // 4 + 1)
        s = l.strip()
        if not s:
            print()
            continue
        m = re.fullmatch(r"outRom\[context\.workingOffset\+\+\]\s*=\s*(.+?);(\s*//.*)?", s)
        if m:
            print("%sE(%s);%s" % (pad, conv(m.group(1)), m.group(2) or ""))
            continue
        m = re.fullmatch(r"outRom\[(.+?)\]\s*=\s*(.+?);(\s*//.*)?", s)
        if m:
            print("%sP(%s, %s);%s" % (pad, conv(m.group(1)), conv(m.group(2)), m.group(3) or ""))
            continue
        m = re.fullmatch(r"byte\[\] (\w+) = new byte\[\] \{(.*)\};(\s*//.*)?", s)
        if m:
            print("%sstatic const uint8_t %s[] = {%s};%s" % (pad, m.group(1), m.group(2), m.group(3) or ""))
            continue
        print(pad + conv(s))


if __name__ == "__main__":
    main()
