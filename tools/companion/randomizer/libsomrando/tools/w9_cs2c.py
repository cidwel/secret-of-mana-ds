#!/usr/bin/env python3
"""First-pass C# -> C line converter for the straight-line code generating hacks (w8 working tool; output is hand-reviewed)."""
import re
import sys


def conv(src, fname_snake):
    s = src
    s = s.lstrip("﻿")
    # drop usings / namespace wrappers handled by caller; keep body lines
    out = []
    for line in s.split("\n"):
        l = line.rstrip()
        # outRom[context.workingOffset++] = X;
        m = re.match(r"^(\s*)outRom\[context\.workingOffset\+\+\] = (.*);(\s*//.*)?$", l)
        if m:
            ind, expr, cm = m.group(1), m.group(2), m.group(3) or ""
            if "workingOffset" in expr:
                out.append(ind + "/* REVIEW: self-referencing emit */ E(%s);%s" % (expr, cm))
            else:
                out.append("%sE(%s);%s" % (ind, expr, cm))
            continue
        m = re.match(r"^(\s*)outRom\[(.*?)\] = (.*);(\s*//.*)?$", l)
        if m:
            ind, idx, expr, cm = m.group(1), m.group(2), m.group(3), m.group(4) or ""
            out.append("%sP(%s, %s);%s" % (ind, idx, expr, cm))
            continue
        out.append(l)
    s = "\n".join(out)
    s = s.replace("context.workingOffset", "c->working_offset")
    s = re.sub(r"\(byte\)", "(uint8_t)", s)
    s = re.sub(r"\(ushort\)", "(uint16_t)", s)
    s = re.sub(r"context\.workingData\.setInt\(", "sr_work_set_int(c, ", s)
    s = re.sub(r"context\.workingData\.getInt\(", "w8_wget_int(c, ", s)
    s = re.sub(r"\bnew byte\[\] \{", "(const uint8_t[]){", s)
    return s


if __name__ == "__main__":
    print(conv(open(sys.argv[1], encoding="utf-8-sig").read(), "x"))
