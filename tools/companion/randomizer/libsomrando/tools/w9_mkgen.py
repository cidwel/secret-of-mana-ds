#!/usr/bin/env python3
"""Assembles C functions from the converted C# classes (w8 working tool).  usage: mkgen.py Class snake_name cfunc_name"""
import re
import sys

P = "/home/cidwel/.local/state/som-decomp/companion-cli-20261017/repo-moppu/SecretOfManaRandomizer/SoMRandomizer/processing/"


def read(path):
    return open(path, encoding="utf-8-sig").read()


def match_brace(s, i):
    depth = 0
    n = len(s)
    while i < n:
        ch = s[i]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return i
        elif ch == "/" and s[i:i + 2] == "//":
            i = s.index("\n", i)
            continue
        elif ch == '"':
            i += 1
            while s[i] != '"':
                if s[i] == "\\":
                    i += 1
                i += 1
        i += 1
    raise ValueError("unbalanced")


def split_methods(text):
    """Return list of (header, body) for the methods of the first class."""
    ci = text.index("public class")
    ob = text.index("{", ci)
    cb = match_brace(text, ob)
    body = text[ob + 1:cb]
    res = []
    i = 0
    pat = re.compile(r"^\s*(?:(?:private|public|protected)\s+)(?:static\s+)?(?:override\s+)?([\w<>\[\], ]+?)\s+(\w+)\s*\(([^)]*)\)\s*\n?\s*\{", re.M)
    while True:
        m = pat.search(body, i)
        if not m:
            break
        ob2 = body.index("{", m.end() - 1)
        cb2 = match_brace(body, ob2)
        res.append((m.group(1), m.group(2), m.group(3), body[ob2 + 1:cb2]))
        i = cb2 + 1
    return res


def snake(n):
    return re.sub(r"(?<!^)(?=[A-Z])", "_", n).lower()


def conv_params(params):
    ps = []
    depth = 0
    cur = ""
    for ch in params:
        if ch == "<": depth += 1
        if ch == ">": depth -= 1
        if ch == "," and depth == 0:
            ps.append(cur.strip()); cur = ""
        else:
            cur += ch
    if cur.strip(): ps.append(cur.strip())
    ps = [p for p in ps if p]
    out = ["struct sr_ctx *c"]
    for p in ps:
        t, n = p.rsplit(" ", 1)
        if t in ("byte[]",) and n in ("outRom", "origRom"):
            continue
        if t == "RandoContext" or t == "RandoSettings" or (t == "string" and n == "seed"):
            continue
        if t == "byte[]":
            out.append("const uint8_t *%s, int %s_len" % (snake(n), snake(n)))
        elif t in ("int", "byte", "ushort", "bool"):
            out.append("int %s" % n)
        elif t == "double":
            out.append("double %s" % n)
        else:
            out.append("/*REVIEW %s*/ %s" % (t, n))
    return ", ".join(out)


def camel_to_snake_ident(m):
    return snake(m.group(0))


def conv_body(body, locals_arrays):
    lines = body.split("\n")
    out = []
    for line in lines:
        l = line.rstrip()
        m = re.match(r"^(\s*)outRom\[context\.workingOffset\+\+\] = (.*);(\s*//.*)?$", l)
        if m:
            out.append("%sE(%s);%s" % (m.group(1), m.group(2), m.group(3) or ""))
            continue
        m = re.match(r"^(\s*)outRom\[(.*?)\] = (.*);(\s*//.*)?$", l)
        if m:
            out.append("%sP(%s, %s);%s" % (m.group(1), m.group(2), m.group(3), m.group(4) or ""))
            continue
        out.append(l)
    s = "\n".join(out)
    s = s.replace("context.workingOffset", "c->working_offset")
    s = s.replace("(byte)", "(uint8_t)")
    s = s.replace("(ushort)", "(uint16_t)")
    s = re.sub(r"context\.workingData\.setInt\(", "sr_work_set_int(c, ", s)
    s = re.sub(r"context\.workingData\.getInt\(", "w8_wget_int(c, ", s)
    # arrays declared as locals
    def decl(m):
        name = m.group(1)
        locals_arrays.add(name)
        return "static const uint8_t %s[] = {" % name
    s = re.sub(r"byte\[\] (\w+) = new byte\[\]\s*\{", decl, s)
    # inline byte arrays as call args -> BYTES(...)
    s = re.sub(r"new byte\[\]\s*\{([^}]*)\}", lambda m: "BYTES(%s)" % m.group(1).strip(), s)
    # foreach over bytes
    def fe(m):
        var, arr = m.group(1), m.group(2)
        if arr in locals_arrays:
            return "for (size_t %s_i = 0; %s_i < sizeof %s; %s_i++) { const uint8_t %s = %s[%s_i];" % (var, var, arr, var, var, arr, var)
        return "for (int %s_i = 0; %s_i < %s_len; %s_i++) { const uint8_t %s = %s[%s_i];" % (var, var, snake(arr), var, var, snake(arr), var)
    s = re.sub(r"foreach\s*\(\s*byte (\w+) in (\w+)\s*\)\s*\n?\s*\{", fe, s)
    s = re.sub(r"(\w+)\.Length", lambda m: "(int)sizeof %s" % m.group(1) if m.group(1) in locals_arrays else "%s_len" % snake(m.group(1)), s)
    return s


def gen(cls_path, cfunc, helper_prefix, process_only=False):
    text = read(P + cls_path)
    methods = split_methods(text)
    out = []
    names = {}
    for ret, name, params, body in methods:
        if name == "getName":
            continue
        if name == "process":
            names[name] = cfunc
        else:
            names[name] = "%s_%s" % (helper_prefix, snake(name))
    for ret, name, params, body in methods:
        if name == "getName":
            continue
        la = set()
        cb = conv_body(body, la)
        # camelCase locals -> keep (valid C)
        for n, cn in names.items():
            cb = re.sub(r"\b%s\(outRom, context, " % n, "%s(c, " % cn, cb)
            cb = re.sub(r"\b%s\(outRom, context\)" % n, "%s(c)" % cn, cb)
        cb = cb.replace("return true;", "return 1;").replace("return false;", "return 0;")
        if name == "process":
            sig = "int %s(struct sr_ctx *c)" % cfunc
            ctype = ""
        else:
            ctype = "static "
            rt = {"void": "void", "int": "int", "bool": "int", "double": "double"}.get(ret, ret)
            sig = "%s%s %s(%s)" % (ctype, rt, names[name], conv_params(params))
        out.append((name, "%s {%s}\n" % (sig, cb)))
    # helpers first so they are declared before use
    res = [t for n, t in out if n != "process"] + [t for n, t in out if n == "process"]
    return "\n".join(res)


if __name__ == "__main__":
    print(gen(sys.argv[1], sys.argv[2], sys.argv[3]))
