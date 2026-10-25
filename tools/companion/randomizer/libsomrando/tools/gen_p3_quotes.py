#!/usr/bin/env python3
"""Generates data/sr_quotes_mitch.c and data/sr_quotes_demetri.c from the upstream quote classes.

usage: gen_p3_quotes.py UPSTREAM_ANCIENTCAVE_DIR OUT_DATA_DIR

The quote text is third-party material (not LGPL); the generated files are the only place it may live (see data/README.quotes.md).
Part of libsomrando, LGPL-2.1 or later (this script only; not the generated text).
"""
import os
import sys


def parse_pool(src):
    """Returns the list of strings of the first C# string[] initializer: literals joined by '+', entries separated by ','."""
    start = src.index("{", src.index("string[]"))
    i = start + 1
    entries, cur, have = [], [], False
    while i < len(src):
        ch = src[i]
        if ch == '"':
            i += 1
            buf = []
            while src[i] != '"':
                if src[i] == "\\":
                    i += 1
                    esc = {"n": "\n", "t": "\t", '"': '"', "\\": "\\", "'": "'", "r": "\r", "0": "\0"}
                    if src[i] not in esc:
                        raise ValueError("unsupported escape \\%s" % src[i])
                    buf.append(esc[src[i]])
                else:
                    buf.append(src[i])
                i += 1
            cur.append("".join(buf))
            have = True
            i += 1
        elif src.startswith("//", i):
            i = src.index("\n", i)
        elif ch == ",":
            if have:
                entries.append("".join(cur))
            cur, have = [], False
            i += 1
        elif ch == "}":
            if have:
                entries.append("".join(cur))
            return entries
        else:
            i += 1
    raise ValueError("unterminated initializer")


def c_literal(s):
    out = []
    for b in s.encode("utf-8"):
        ch = chr(b)
        if ch == "\n":
            out.append("\\n")
        elif ch == '"':
            out.append('\\"')
        elif ch == "\\":
            out.append("\\\\")
        elif ch == "?":
            out.append("\\?")   # no trigraphs
        elif 32 <= b < 127:
            out.append(ch)
        else:
            out.append("\\%03o" % b)
    return '"' + "".join(out) + '"'


def emit(path, ident, entries, source):
    with open(path, "w", encoding="ascii") as f:
        f.write("/* Quote pool '%s' generated from %s by tools/gen_p3_quotes.py.\n" % (ident, source))
        f.write(" * THIRD-PARTY TEXT, not covered by the LGPL (see README.quotes.md).  Do not edit by hand. */\n")
        f.write('#include "sr_p3_quotes.h"\n\n')
        f.write("const char *const sr_quotes_%s[] = {\n" % ident)
        for e in entries:
            f.write("  %s,\n" % c_literal(e))
        f.write("};\n")
        f.write("const int sr_quotes_%s_count = %d;\n" % (ident, len(entries)))
        if ident == "mitch":
            f.write("const int sr_quotes_are_upstream = 1;\n")


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    src_dir, out_dir = sys.argv[1], sys.argv[2]
    for ident, name in (("mitch", "MitchHedbergQuotes.cs"), ("demetri", "DemetriMartinQuotes.cs")):
        with open(os.path.join(src_dir, name), encoding="utf-8-sig") as f:
            entries = parse_pool(f.read())
        emit(os.path.join(out_dir, "sr_quotes_%s.c" % ident), ident, entries, name)
        print("%s: %d quotes" % (ident, len(entries)))


if __name__ == "__main__":
    main()
