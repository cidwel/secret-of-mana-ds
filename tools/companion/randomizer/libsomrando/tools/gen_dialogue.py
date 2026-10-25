#!/usr/bin/env python3
"""Generates src/sr_p2_dialogue_gen.c from FasterDialogueEvents.cs (processShortening + setAttribute).

usage: gen_dialogue.py <upstream SoMRandomizer folder> <out .c>

Every statement of the C# method is translated one to one, in source order (the order matters: replacementEvents insertion order and
the r.Next() draws).  Anything the translator does not recognise aborts the run.
"""
import re
import sys

up, out = sys.argv[1], sys.argv[2]
src = open(up + "/processing/vanillarando/FasterDialogueEvents.cs", encoding="utf-8-sig").read()
flags_src = open(up + "/processing/common/EventFlags.cs", encoding="utf-8-sig").read()
cmds_src = open(up + "/processing/common/EventCommandEnum.cs", encoding="utf-8-sig").read()
vals_src = open(up + "/processing/common/SomVanillaValues.cs", encoding="utf-8-sig").read()

FLAGS = dict(re.findall(r"public const byte ([A-Z_0-9]+) = (0x[0-9A-Fa-f]+);", flags_src))
CMDS = dict(re.findall(r"public static EventCommandEnum ([A-Z_0-9]+) = new EventCommandEnum\((0x[0-9A-Fa-f]+)\);", cmds_src))
MAPS = dict(re.findall(r"public const int (MAPNUM_[A-Z_0-9]+) = (\d+);", vals_src))

start = src.index("public void processShortening")
body_start = src.index("{", start) + 1
end = src.index("private void setAttribute")
body = src[body_start:end]
# drop the closing brace of processShortening
body = body[:body.rindex("}")]

used_flags, used_cmds = set(), set()


def strip_comment(line):
    in_s = False
    i = 0
    while i < len(line):
        ch = line[i]
        if in_s:
            if ch == "\\":
                i += 2
                continue
            if ch == '"':
                in_s = False
        else:
            if ch == '"':
                in_s = True
            elif line.startswith("//", i):
                return line[:i]
        i += 1
    return line


STR = r'"(?:[^"\\]|\\.)*"'


def cstr(lit):
    inner = lit[1:-1]
    for m in re.finditer(r"\\(.)", inner):
        if m.group(1) not in "n\"'":
            sys.exit("unexpected escape " + lit)
    if any(ord(ch) > 126 for ch in inner):
        sys.exit("non-ascii string " + lit)
    return lit


def flag(tok):
    tok = tok.strip()
    m = re.fullmatch(r"EventFlags\.([A-Z_0-9]+)", tok)
    if m:
        used_flags.add(m.group(1))
        return "SRDLG_F_" + m.group(1)
    if re.fullmatch(r"0x[0-9A-Fa-f]+|\d+", tok):
        return tok
    sys.exit("flag? " + tok)


def num(tok):
    tok = tok.strip()
    if re.fullmatch(r"0x[0-9A-Fa-f]+|\d+", tok):
        return tok
    sys.exit("num? " + tok)


def text_expr(expr, ctx):
    """Returns (is_dynamic, c expression yielding const char *)."""
    parts = [p.strip() for p in re.findall(STR + r"|[^+]+", expr) if p.strip() not in ("", "+")]
    # re-split properly on top level '+'
    parts, cur, depth, in_s, i = [], "", 0, False, 0
    while i < len(expr):
        ch = expr[i]
        if in_s:
            cur += ch
            if ch == "\\":
                cur += expr[i + 1]
                i += 1
            elif ch == '"':
                in_s = False
        elif ch == '"':
            in_s = True
            cur += ch
        elif ch in "([":
            depth += 1
            cur += ch
        elif ch in ")]":
            depth -= 1
            cur += ch
        elif ch == "+" and depth == 0:
            parts.append(cur.strip())
            cur = ""
        else:
            cur += ch
        i += 1
    parts.append(cur.strip())
    fmt, args, dyn = "", [], False
    for p in parts:
        if re.fullmatch(STR, p):
            fmt += cstr(p)[1:-1].replace("%", "%%")
        elif p == "VanillaEventUtil.BOY_NAME_INDICATOR":
            fmt += "(boy)"
        elif re.fullmatch(r"[A-Za-z_0-9]+", p) and p in ctx["strvars"]:
            fmt += "%s"
            args.append(p)
            dyn = True
        else:
            m = re.fullmatch(r"ElementSwaps\.elementNames\[newElementMapping\[(\d+)\]\]", p)
            if not m:
                sys.exit("text part? " + p)
            fmt += "%s"
            args.append("sr_w5_dlg_element_names[new_element_mapping[%s]]" % m.group(1))
            dyn = True
    if not dyn:
        return '"%s"' % fmt
    return 'sr_w5_dlg_fmt(c, "%s", %s)' % (fmt, ", ".join(args))


lines = body.split("\n")
o = []
ctx = {"strvars": set()}
created = {}      # variable -> line index of creation
registered = {}   # event id -> variable
used_before_reg = []
pending_arr = None
ev_ids = []

i = 0
while i < len(lines):
    raw = lines[i]
    i += 1
    full_comment = raw.strip().startswith("//")
    line = strip_comment(raw).strip()
    if not line:
        if full_comment:
            t = raw.strip()[2:].strip()
            m = re.match(r"(x[0-9a-fA-F]+\b.*)", t)
            if m and len(m.group(1)) < 110:
                o.append("  /* " + m.group(1).replace("*/", "* /") + " */")
        continue
    if line in ("{", "}"):
        o.append("  " + line)
        continue
    # string arrays
    m = re.fullmatch(r"string\[\] (\w+) = new string\[\]\s*\{", line) or re.fullmatch(r"string\[\] (\w+) = new string\[\]\{", line)
    if m:
        name = m.group(1)
        items = []
        while True:
            l2 = strip_comment(lines[i]).strip()
            i += 1
            if l2.startswith("}"):
                break
            if not l2:
                continue
            mm = re.fullmatch(r"(" + STR + r"),?", l2)
            if not mm:
                sys.exit("array item? " + l2)
            items.append(cstr(mm.group(1)))
        o.append("  static const char *const %s[] = {" % name)
        for it in items:
            o.append("    %s," % it)
        o.append("  };")
        continue
    m = re.fullmatch(r"string (\w+) = (\w+)\[r\.Next\(\) % \2\.Length\];", line)
    if m:
        o.append("  const char *%s = %s[(unsigned)sr_random_next(r) %% (sizeof %s / sizeof %s[0])];" % (m.group(1), m.group(2), m.group(2), m.group(2)))
        ctx["strvars"].add(m.group(1))
        continue
    if line.startswith("List<List<byte>> "):
        continue
    m = re.fullmatch(r"EventScript (\w+) = new EventScript\(\);", line)
    if m:
        created[m.group(1)] = len(o)
        o.append("  struct sr_bytes *%s = NULL;" % m.group(1))
        continue
    m = re.fullmatch(r"context\.replacementEvents\[(0x[0-9A-Fa-f]+)\] = (\w+);", line)
    if m:
        eid = int(m.group(1), 16)
        if eid in registered:
            sys.exit("duplicate event id %x" % eid)
        registered[eid] = m.group(2)
        ev_ids.append(eid)
        o.append("  %s = sr_event_new(c, %s);" % (m.group(2), m.group(1)))
        continue
    m = re.fullmatch(r"(\w+)\.(\w+)\((.*)\);", line)
    if m and m.group(1).startswith("newEvent"):
        v, meth, args = m.groups()
        if v not in registered.values():
            sys.exit("use before registration: " + line)
        e = v
        if meth == "Add":
            mm = re.fullmatch(r"EventCommandEnum\.([A-Z_0-9]+)\.Value", args.strip())
            if mm:
                used_cmds.add(mm.group(1))
                o.append("  PB(%s, SRDLG_C_%s);" % (e, mm.group(1)))
            else:
                n = num(args)
                if int(n, 0) > 255:
                    sys.exit("byte overflow " + line)
                o.append("  PB(%s, %s);" % (e, n))
        elif meth == "IncrFlag":
            o.append("  sr_ev_incr_flag(c, %s, %s);" % (e, flag(args)))
        elif meth == "DecrFlag":
            o.append("  sr_ev_decr_flag(c, %s, %s);" % (e, flag(args)))
        elif meth == "SetFlag":
            a, b = [x.strip() for x in args.split(",")]
            o.append("  sr_ev_set_flag(c, %s, %s, %s);" % (e, flag(a), num(b)))
        elif meth in ("Jsr", "Jump", "Door", "Sleep"):
            fn = {"Jsr": "sr_ev_jsr", "Jump": "sr_ev_jump", "Door": "sr_ev_door", "Sleep": "sr_ev_sleep"}[meth]
            o.append("  %s(c, %s, %s);" % (fn, e, num(args)))
        elif meth == "End":
            o.append("  sr_ev_end(c, %s);" % e)
        elif meth == "Return":
            o.append("  PB(%s, 0x02);" % e)
        elif meth == "OpenDialogueBox":
            o.append("  PB(%s, 0x50);" % e)
        elif meth == "CloseDialogueBox":
            o.append("  PB(%s, 0x51);" % e)
        elif meth == "Logic":
            mm = re.fullmatch(r"(.+?),\s*(\w+),\s*(\w+),\s*EventScript\.Get(Jump|Jsr|Incr)Cmd\((.+)\)", args.strip())
            if not mm:
                sys.exit("logic? " + line)
            f, lo, hi, kind, t = mm.groups()
            if kind == "Incr":
                t = flag(t)
            else:
                t = num(t)
            o.append("  sr_w5_dlg_logic(c, %s, %s, %s, %s, SRDLG_CMD_%s, %s);" % (e, flag(f), num(lo), num(hi), kind.upper(), t))
        elif meth == "AddDialogueBox":
            o.append("  sr_w5_dlg_box(c, %s, %s);" % (e, text_expr(args, ctx)))
        elif meth == "AddDialogue":
            o.append("  sr_w5_dlg_dialogue(c, %s, %s, 0);" % (e, text_expr(args, ctx)))
        elif meth == "AddAutoTextDialogueBox":
            mm = re.fullmatch(r"(.+),\s*(0x[0-9A-Fa-f]+|\d+)", args.strip())
            o.append("  sr_w5_dlg_auto_box(c, %s, %s, %s);" % (e, text_expr(mm.group(1), ctx), mm.group(2)))
        else:
            sys.exit("method? " + line)
        continue
    m = re.fullmatch(r"setAttribute\((\d+), newElementMapping, (\w+)\);", line)
    if m:
        o.append("  sr_w5_dlg_set_attribute(c, %s, new_element_mapping, %s);" % (m.group(2), m.group(1)))
        continue
    m = re.fullmatch(r"int (\w+) = (MAPNUM_[A-Z_0-9]+);", line)
    if m:
        o.append("  int %s = %s;   /* %s */" % (m.group(1), MAPS[m.group(2)], m.group(2)))
        continue
    m = re.fullmatch(r"int mapObjOffset = VanillaMapUtil\.getObjectOffset\(origRom, (\w+)\);", line)
    if m:
        o.append("  int mapObjOffset = sr_vanilla_object_offset(c->orig, %s);" % m.group(1))
        continue
    m = re.fullmatch(r"mapObjOffset = VanillaMapUtil\.getObjectOffset\(origRom, (\w+)\);", line)
    if m:
        o.append("  mapObjOffset = sr_vanilla_object_offset(c->orig, %s);" % m.group(1))
        continue
    if line == "mapObjOffset += 8;":
        o.append("  mapObjOffset += 8;   /* skip header */")
        continue
    m = re.fullmatch(r"rom\[(mapObjOffset \+ 8 \* (?:\d+|i) \+ \d+)\] (\+?=) (0x[0-9A-Fa-f]+|\d+);", line)
    if m:
        off, op, v = m.groups()
        if op == "=":
            o.append("  P(%s, %s);" % (off, v))
        else:
            o.append("  P(%s, sr_peek(c, %s) + %s);" % (off, off, v))
        continue
    m = re.fullmatch(r"rom\[(0x[0-9A-Fa-f]+)\] = (0x[0-9A-Fa-f]+);", line)
    if m:
        o.append("  P(%s, %s);" % m.groups())
        continue
    m = re.fullmatch(r"for ?\(int i ?= ?(\d+); i < (\d+); i\+\+\)", line)
    if m:
        o.append("  for (int i = %s; i < %s; i++)" % m.groups())
        continue
    sys.exit("untranslated: " + repr(line))

# every creation must be followed (before any other use) by its registration; checked textually
for idx, l in enumerate(o):
    m = re.match(r"  struct sr_bytes \*(newEvent\w+) = NULL;", l)
    if m:
        nxt = o[idx + 1]
        if nxt.strip() != "%s = sr_event_new(c, %s);" % (m.group(1), hex(next(k for k, v in registered.items() if v == m.group(1)))) and \
           not re.fullmatch(r"\s+%s = sr_event_new\(c, 0x[0-9A-Fa-f]+\);" % m.group(1), nxt):
            sys.exit("creation not followed by registration: " + l)

hdr = """/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * GENERATED by tools/gen_dialogue.py from processing/vanillarando/FasterDialogueEvents.cs (Author: Moppleton): processShortening
 * translated statement by statement.  Do not edit by hand; regenerate.
 */
#include "sr_core.h"
#include "sr_p2_dialogue_gen.h"

#define PB(e, v) sr_bytes_push(c, (e), (v))
#define P(off, v) sr_poke(c, (off), (v))

"""
for n in sorted(used_flags):
    hdr += "#define SRDLG_F_%s %s\n" % (n, FLAGS[n])
for n in sorted(used_cmds):
    hdr += "#define SRDLG_C_%s %s\n" % (n, CMDS[n])
hdr += """
void sr_w5_dlg_shortening(struct sr_ctx *c, const int *new_element_mapping) {
  struct sr_random *r = &c->rf;
"""
open(out, "w").write(hdr + "\n".join(o) + "\n}\n")
print("events registered:", len(registered), "flags", len(used_flags), "cmds", len(used_cmds), "lines", len(o))
