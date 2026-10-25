#!/usr/bin/env python3
"""Mechanical C# -> C translation of the straight-line byte-patching hacks of Moppu/SecretOfManaRandomizer v1.49.

  transpile_simple.py <upstream SoMRandomizer folder> <out.c> <out.h> ClassName...

Only the statement shapes that these hacks use are accepted: a gate `if(!settings.getBool(CommonSettings.X)) { return false; }`,
`outRom[expr] = expr;`, `outRom[context.workingOffset++] = expr;`, `ensureSpaceInBank`, `for` loops over ints, `return true/false`, and comments.
Anything else stops the translation of that class with the offending line (it is then ported by hand).  The generated file is reviewed
and committed; comments of the C# are kept.  Derived from the upstream files named in the output; LGPL-2.1+.  Original tool of this repository.
"""
import glob
import os
import re
import sys

HEADER = '''/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Straight-line hacks translated mechanically from the C# by tools/transpile_simple.py (then reviewed).  Author of every upstream hack:
 * Moppleton.  Each function names its upstream file; comments are those of the C#.  Translated to C 2026-10-18.
 */
#include <string.h>

#include "sr_core.h"

#define E(v) sr_emit(c, (v))
#define P(off, v) sr_poke(c, (off), (v))
#define SB(key) sr_settings_get_bool(c->settings, (key))

'''


class Manual(Exception):
    pass


def snake(name):
    return re.sub(r"(?<!^)(?=[A-Z])", "_", name).lower().replace("__", "_")


def load_keys(src):
    t = open(os.path.join(src, "config/settings/CommonSettings.cs"), encoding="utf-8-sig").read()
    return dict(re.findall(r'public const string (PROPERTYNAME_\w+) = "(\w+)";', t))


def find_process_body(text):
    m = re.search(r"protected override bool process\([^)]*\)\s*\{", text)
    if not m:
        raise Manual("no process()")
    i, depth = m.end(), 1
    while depth:
        ch = text[i]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
        i += 1
    return text[m.end():i - 1]


def translate(cls, path, keys):
    text = open(path, encoding="utf-8-sig").read()
    body = find_process_body(text)
    out = []
    gate_key = None
    # normalise: put braces of the gate on one line
    body = re.sub(r"if\s*\(\s*!\s*settings\.getBool\(CommonSettings\.(PROPERTYNAME_\w+)\)\s*\)\s*\{\s*return false;\s*\}",
                  lambda m: "GATE:" + keys[m.group(1)] + ";", body)
    in_block = False
    for raw in body.split("\n"):
        line = raw.strip()
        if not line:
            continue
        if in_block or line.startswith("/*"):
            # block comments are copied; a stray "/*" inside one would be a nested comment in C
            head = line[:2] if line.startswith("/*") else ""
            rest = line[2:] if head else line
            out.append("  " + head + rest.replace("/*", "/ *"))
            in_block = "*/" not in line
            continue
        if line.startswith("//"):
            out.append("  " + line)
            continue
        code, _, cm = line.partition("//")
        code = code.strip()
        cm = ("  //" + cm) if cm else ""
        if not code:
            continue
        if code.startswith("GATE:"):
            if gate_key is not None or out and any(not l.strip().startswith(("//", "/*", "*")) for l in out):
                raise Manual("gate is not the first statement: " + code)
            gate_key = code[5:-1]
            out.append('  if (!SB("%s")) return 0;%s' % (gate_key, cm))
            continue
        m = re.fullmatch(r"outRom\[context\.workingOffset\+\+\]\s*=\s*(.+);", code)
        if m:
            if "workingOffset" in m.group(1):
                raise Manual("value reads workingOffset: " + code)
            out.append("  E(%s);%s" % (conv(m.group(1)), cm))
            continue
        m = re.fullmatch(r"outRom\[(.+?)\]\s*=\s*(.+);", code)
        if m:
            out.append("  P(%s, %s);%s" % (conv(m.group(1)), conv(m.group(2)), cm))
            continue
        m = re.fullmatch(r"CodeGenerationUtils\.ensureSpaceInBank\(ref context\.workingOffset,\s*(.+)\);", code)
        if m:
            out.append("  sr_ensure_space_in_bank(&c->working_offset, %s);%s" % (conv(m.group(1)), cm))
            continue
        m = re.fullmatch(r"for\s*\(\s*int (\w+)\s*=\s*(\w+)\s*;\s*\1\s*(<|<=)\s*(\w+)\s*;\s*\1\+\+\s*\)", code)
        if m:
            out.append("  for (int %s = %s; %s %s %s; %s++)" % (m.group(1), m.group(2), m.group(1), m.group(3), m.group(4), m.group(1)))
            continue
        if code in ("{", "}"):
            out.append("  " + code)
            continue
        if code == "return true;":
            out.append("  return 1;")
            continue
        if code == "return false;":
            out.append("  return 0;")
            continue
        raise Manual("unsupported statement: " + code)
    nocomment = re.sub(r"/\*.*?\*/|//[^\n]*", "", text, flags=re.S)
    name = re.search(r'getName\(\).*?return\s*"([^"]*)"', nocomment, re.S).group(1)
    rel = path.split("SoMRandomizer/")[-1]
    fn = "sr_hack_" + snake(cls)
    code = "/* %s - upstream %s */\nint %s(struct sr_ctx *c) {\n%s\n}\n" % (name, rel, fn, "\n".join(out))
    return fn, gate_key, code


def conv(expr):
    e = expr.strip()
    e = e.replace("context.workingOffset", "c->working_offset")
    e = re.sub(r"\(byte\)", "(uint8_t)", e)
    if re.search(r"[A-Za-z_]\w*\(", re.sub(r"\(uint8_t\)", "", e)) or "[" in e or "." in e:
        raise Manual("expression: " + expr)
    return e


def main():
    if len(sys.argv) < 5:
        print(__doc__)
        return 2
    src, out_c, out_h = sys.argv[1:4]
    classes = sys.argv[4:]
    keys = load_keys(src)
    files = {os.path.basename(f)[:-3]: f for f in glob.glob(os.path.join(src, "processing/**/*.cs"), recursive=True)}
    code_parts, protos, done, manual = [], [], [], []
    for cls in classes:
        try:
            fn, gate, code = translate(cls, files[cls], keys)
        except Manual as e:
            manual.append((cls, str(e)))
            continue
        code_parts.append(code)
        protos.append("int %s(struct sr_ctx *c);" % fn)
        done.append((cls, fn, gate))
    open(out_c, "w").write(HEADER + "\n".join(code_parts))
    open(out_h, "w").write("/* generated by tools/transpile_simple.py; see sr_hacks_simple.c */\n#ifndef SR_HACKS_SIMPLE_H\n#define SR_HACKS_SIMPLE_H\nstruct sr_ctx;\n" +
                           "\n".join(protos) + "\n#endif\n")
    for cls, fn, gate in done:
        print("OK     %-28s %-40s gate=%s" % (cls, fn, gate))
    for cls, why in manual:
        print("MANUAL %-28s %s" % (cls, why))
    return 0


if __name__ == "__main__":
    sys.exit(main())
