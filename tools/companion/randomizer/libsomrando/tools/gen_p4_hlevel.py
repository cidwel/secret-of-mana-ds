#!/usr/bin/env python3
"""Generates src/sr_p4_hk_level_gen.c / .h: the code-emitting part of EnemiesAtYourLevel.cs and the whole of CustomTopOfScreenText.cs and
MinMaxLevel.cs (Moppu/SecretOfManaRandomizer v1.49), translated line by line.  Original tool of this repository; LGPL-2.1+.

  gen_p4_hlevel.py <upstream SoMRandomizer folder> <out dir>

EnemiesAtYourLevel.process() is split in two: everything up to "enemy level message" (stat tables) is written by hand in
src/sr_p4_hk_level.c; from `List<byte> enemyLevelupMessage` to the end of the class it is translated here (a state struct carries the
locals of the hand-written part).  The translator only accepts the statement shapes of that region and stops at anything else.
"""
import os
import re
import sys

UP = sys.argv[1] if len(sys.argv) > 1 else "."
OUT = sys.argv[2] if len(sys.argv) > 2 else "."
OW = os.path.join(UP, "processing/hacks/openworld")

# ------------------------------------------------------------------------------------------------ constants
def load_ram():
    t = open(os.path.join(UP, "processing/common/CustomRamOffsets.cs"), encoding="utf-8-sig").read()
    vals = {}
    for name, expr in re.findall(r"public static int (\w+)\s*=\s*([^;]+);", t):
        expr = re.sub(r"//.*", "", expr)
        vals[name] = eval(expr, {}, dict(vals))
    return vals


def load_flags():
    t = open(os.path.join(UP, "processing/common/EventFlags.cs"), encoding="utf-8-sig").read()
    return {n: int(v, 0) for n, v in re.findall(r"public const byte (\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+);", t)}


RAM = load_ram()
FLAGS = load_flags()
USED_RAM = set()
USED_FLAGS = set()

BYTE_LISTS = {"enemyLevelupMessage", "orig5629", "orig477e"}      # struct sr_bytes
BYTE_ARRAYS = {"replacedCode", "nonBossBosses"}                     # pointer + NAME_n
INT_LISTS = {"sourceOffsets", "sixteenBitOffsets", "altLoaderStats"}   # struct sr_p4_hlevel_ints
USHORT_LISTS = {"ramLocations"}
HELPERS = {}      # name -> signature info, filled by the signature pass


class Manual(Exception):
    pass


def split_args(s):
    out, depth, cur, in_str = [], 0, "", False
    for ch in s:
        if ch == '"':
            in_str = not in_str
        if not in_str:
            if ch in "([{":
                depth += 1
            elif ch in ")]}":
                depth -= 1
            elif ch == "," and depth == 0:
                out.append(cur.strip())
                cur = ""
                continue
        cur += ch
    if cur.strip():
        out.append(cur.strip())
    return out


def conv(e):
    e = e.replace("context.workingOffset", "c->working_offset")
    e = re.sub(r"\(byte\)", "(uint8_t)", e)

    def ram(m):
        USED_RAM.add(m.group(1))
        return "EYL_" + m.group(1)
    e = re.sub(r"CustomRamOffsets\.(\w+)", ram, e)

    def flag(m):
        USED_FLAGS.add(m.group(1))
        return "EYL_EVF_" + m.group(1)
    e = re.sub(r"EventFlags\.(\w+)", flag, e)
    e = re.sub(r"\b(replacedCode)\.Length", r"\1_n", e)
    e = re.sub(r"\b(\w+)\.Count\b", lambda m: m.group(1) + ".len" if m.group(1) in BYTE_LISTS else m.group(1) + ".n", e)
    e = re.sub(r"\b(\w+)\.Contains\((\w+)\)", r"sr_p4_hlevel_has(&\1, \2)", e)
    e = re.sub(r'(\w+) == "(\w+)"', r'!strcmp(\1, "\2")', e)
    e = re.sub(r"\borigRom\[", "c->orig[", e)
    return e


def conv_sig(params):
    out = ["struct sr_ctx *c"]
    for p in split_args(params):
        m = re.fullmatch(r"(.+?)\s+(\w+)", p)
        t, n = m.group(1), m.group(2)
        if n in ("outRom", "context"):
            continue
        if t == "byte[]":
            out += ["const uint8_t *%s" % n, "int %s_n" % n]
        elif t == "string":
            out.append("const char *%s" % n)
        elif t == "List<ushort>":
            out.append("struct sr_p4_hlevel_ushorts %s" % n)
        elif t == "byte":
            out.append("uint8_t %s" % n)
        elif t in ("int", "double", "bool"):
            out.append(("int " if t == "bool" else t + " ") + n)
        else:
            raise Manual("param type " + p)
    return ", ".join(out)


def conv_call(name, argstr):
    args = []
    for a in split_args(argstr):
        if a in ("outRom", "context"):
            continue
        m = re.fullmatch(r"new byte\[\]\s*\{(.*)\}", a)
        if m:
            n = len(split_args(m.group(1)))
            args += ["(const uint8_t[]){%s}" % m.group(1).strip(), str(n)]
        else:
            args.append(conv(a))
    return "%s(c, %s)" % (name, ", ".join(args))


# ------------------------------------------------------------------------------------------------ line translator
def translate(lines, helpers_decl):
    out = []
    pending = []          # declarations to insert after the next "{"
    in_block = False
    for raw in lines:
        s = raw.strip()
        if not s:
            out.append("")
            continue
        if in_block or s.startswith("/*"):
            out.append("  " + s.replace("/*", "/ *") if not s.startswith("/*") else "  /*" + s[2:].replace("/*", "/ *"))
            in_block = "*/" not in s
            continue
        if s.startswith("//"):
            out.append("  " + s)
            continue
        code, _, cm = s.partition("//")
        code = code.strip()
        cm = ("  //" + cm.replace("*/", "* /")) if cm else ""
        if not code:
            continue
        m = re.fullmatch(r"outRom\[context\.workingOffset\+\+\]\s*=\s*(.+);", code)
        if m:
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
        m = re.fullmatch(r"foreach \((\w+) (\w+) in (\w+)\)", code)
        if m:
            t, v, lst = m.groups()
            if lst in BYTE_LISTS:
                out.append("  for (size_t %s_i = 0; %s_i < %s.len; %s_i++)%s" % (v, v, lst, v, cm))
                pending.append("uint8_t %s = %s.p[%s_i];" % (v, lst, v))
            elif lst in BYTE_ARRAYS:
                out.append("  for (int %s_i = 0; %s_i < %s_n; %s_i++)%s" % (v, v, lst, v, cm))
                pending.append("uint8_t %s = %s[%s_i];" % (v, lst, v))
            elif lst in INT_LISTS:
                out.append("  for (int %s_i = 0; %s_i < %s.n; %s_i++)%s" % (v, v, lst, v, cm))
                pending.append("int %s = %s.v[%s_i];" % (v, lst, v))
            elif lst in USHORT_LISTS:
                out.append("  for (int %s_i = 0; %s_i < %s.n; %s_i++)%s" % (v, v, lst, v, cm))
                pending.append("uint16_t %s = %s.v[%s_i];" % (v, lst, v))
            else:
                raise Manual("foreach: " + code)
            continue
        if code == "{":
            out.append("  {")
            for p in pending:
                out.append("    " + p)
            pending = []
            continue
        if code == "}":
            out.append("  }")
            continue
        if pending:
            raise Manual("foreach body without brace")
        m = re.fullmatch(r"(if|else if)\s*\((.*)\)", code)
        if m:
            out.append("  %s (%s)%s" % (m.group(1), conv(m.group(2)), cm))
            continue
        if code == "else":
            out.append("  else" + cm)
            continue
        m = re.fullmatch(r"for \(int (\w+) = (\w+); \1 (<|<=) (\w+); \1\+\+\)", code)
        if m:
            out.append("  for (int %s = %s; %s %s %s; %s++)%s" % (m.group(1), m.group(2), m.group(1), m.group(3), m.group(4), m.group(1), cm))
            continue
        # byte lists
        m = re.fullmatch(r"List<byte> (\w+) = new List<byte>\(\);", code)
        if m:
            out.append("  struct sr_bytes %s;" % m.group(1))
            out.append("  memset(&%s, 0, sizeof %s);" % (m.group(1), m.group(1)))
            continue
        m = re.fullmatch(r'List<byte> (\w+) = VanillaEventUtil\.getBytes\(("[^"]*")\);', code)
        if m:
            out.append("  struct sr_bytes %s;" % m.group(1))
            out.append("  memset(&%s, 0, sizeof %s);" % (m.group(1), m.group(1)))
            out.append("  sr_event_text_bytes(c, &%s, %s);" % (m.group(1), m.group(2)))
            continue
        m = re.fullmatch(r"(\w+)\.Add\((.+)\);", code)
        if m and m.group(1) in BYTE_LISTS:
            out.append("  sr_bytes_push(c, &%s, %s);%s" % (m.group(1), conv(m.group(2)), cm))
            continue
        m = re.fullmatch(r"byte\[\] (\w+) = new byte\[\]\s*\{(.*)\};", code)
        if m:
            n = len(split_args(m.group(2)))
            out.append("  static const uint8_t %s[] = {%s};" % (m.group(1), m.group(2).strip()))
            out.append("  const int %s_n = %d;" % (m.group(1), n))
            continue
        # double casts
        m = re.fullmatch(r"byte (divisor|incrementAmount) = \(byte\)(\w+);", code)
        if m:
            out.append("  uint8_t %s = (uint8_t)(int)%s;%s" % (m.group(1), m.group(2), cm))
            continue
        m = re.fullmatch(r"(int|byte|bool|double) (\w+) = (.+);", code)
        if m:
            t = {"byte": "uint8_t", "bool": "int"}.get(m.group(1), m.group(1))
            out.append("  %s %s = %s;%s" % (t, m.group(2), conv(m.group(3)), cm))
            continue
        m = re.fullmatch(r"(\w+)\((.*)\);", code)
        if m and m.group(1) in helpers_decl:
            out.append("  %s;%s" % (conv_call(m.group(1), m.group(2)), cm))
            continue
        if re.fullmatch(r"Logging\.log\(.*\);", code):
            continue
        if code == "return true;":
            out.append("  return 1;")
            continue
        raise Manual("unsupported statement: " + code)
    return out


def main():
    src = open(os.path.join(OW, "EnemiesAtYourLevel.cs"), encoding="utf-8-sig").read().split("\n")
    # tail of process(): from the enemy-level message to the end of process()
    start = next(i for i, l in enumerate(src) if "List<byte> enemyLevelupMessage" in l)
    ret = next(i for i in range(start, len(src)) if src[i].strip() == "return true;")
    end_proc = ret + 1                  # the "}" of process()
    # helpers: private void NAME(...) { ... }
    sigs = []
    for i in range(end_proc, len(src)):
        m = re.match(r"\s*private void (\w+)\((.*)\)\s*$", src[i])
        if m:
            sigs.append((i, m.group(1), m.group(2)))
    helpers = {n for _, n, _ in sigs}
    body_tail = translate(src[start:ret + 1], helpers)
    parts = []
    for k, (i, n, params) in enumerate(sigs):
        j = sigs[k + 1][0] if k + 1 < len(sigs) else len(src)
        # body = lines after the signature up to the closing brace at 8 spaces
        k2 = i + 1
        end = next(x for x in range(k2, len(src)) if src[x] == "        }")
        parts.append((n, params, translate(src[k2 + 1:end], helpers)))

    h = []
    h.append("/* libsomrando: generated by tools/gen_p4_hlevel.py from CustomTopOfScreenText.cs, MinMaxLevel.cs and EnemiesAtYourLevel.cs\n"
             " * (Moppu/SecretOfManaRandomizer v1.49, Author: Moppleton).  LGPL-2.1 or later.  Translated to C 2026-10-18. */")
    h.append("#ifndef SR_P4_HK_LEVEL_GEN_H\n#define SR_P4_HK_LEVEL_GEN_H\n#include <stdint.h>\n#include \"sr_core.h\"\n")
    h.append("struct sr_p4_hlevel_ints { int n; int v[24]; };\nstruct sr_p4_hlevel_ushorts { int n; uint16_t v[2]; };")
    h.append("static inline int sr_p4_hlevel_has(const struct sr_p4_hlevel_ints *l, int x) {\n  for (int i = 0; i < l->n; i++)\n    if (l->v[i] == x) return 1;\n  return 0;\n}\n")
    h.append("/* locals of EnemiesAtYourLevel.process() that the generated tail reads */")
    h.append("struct sr_p4_hlevel_state {\n  const char *enemyLevelSource;\n  double bossIncrementValue, timerIncrementValue;\n"
             "  int screenTextSubrLocation, minLevelTableLoc, maxLevelTableLoc, vanillaManaBeast, pauseTimerInMenu;\n  uint8_t noFutureLevel;\n"
             "  struct sr_p4_hlevel_ints sourceOffsets, sixteenBitOffsets, altLoaderStats;\n"
             "  int valueTableLocations[261], classLocations[261];\n  struct sr_p4_hlevel_ushorts ramLocations[261];\n};")
    h.append("int sr_p4_hlevel_emit_tail(struct sr_ctx *c, const struct sr_p4_hlevel_state *st);   /* process() after the stat tables */")
    h.append("int sr_p4_hlevel_custom_top_text(struct sr_ctx *c);   /* CustomTopOfScreenText.process() */")
    h.append("#endif")

    g = []
    g.append("/* libsomrando: generated by tools/gen_p4_hlevel.py (do not edit); see sr_p4_hk_level_gen.h. */")
    g.append('#include <string.h>\n\n#include "sr_p4_hk_level_gen.h"\n\n/* the C# helpers carry parameters and locals they never read */\n#pragma GCC diagnostic ignored "-Wunused-parameter"\n#pragma GCC diagnostic ignored "-Wunused-variable"\n')
    g.append("#define E(v) sr_emit(c, (v))\n#define P(off, v) sr_poke(c, (off), (v))\n")
    cons = []
    # constants are emitted after translation so that only used names appear
    # helper prototypes
    for n, params, _ in parts:
        g.append("static void %s(%s);" % (n, conv_sig(params)))
    g.append("")
    g.append("int sr_p4_hlevel_emit_tail(struct sr_ctx *c, const struct sr_p4_hlevel_state *st) {")
    g.append("  const char *enemyLevelSource = st->enemyLevelSource;\n  const double bossIncrementValue = st->bossIncrementValue, timerIncrementValue = st->timerIncrementValue;\n"
             "  const int screenTextSubrLocation = st->screenTextSubrLocation, minLevelTableLoc = st->minLevelTableLoc, maxLevelTableLoc = st->maxLevelTableLoc;\n"
             "  const int vanillaManaBeast = st->vanillaManaBeast, pauseTimerInMenu = st->pauseTimerInMenu;\n  const uint8_t noFutureLevel = st->noFutureLevel;\n"
             "  const struct sr_p4_hlevel_ints sourceOffsets = st->sourceOffsets, sixteenBitOffsets = st->sixteenBitOffsets, altLoaderStats = st->altLoaderStats;\n"
             "  const int *valueTableLocations = st->valueTableLocations, *classLocations = st->classLocations;\n"
             "  const struct sr_p4_hlevel_ushorts *ramLocations = st->ramLocations;\n"
             "  const int hasDebugLogger = 0; (void)hasDebugLogger;\n  (void)bossIncrementValue; (void)timerIncrementValue; (void)screenTextSubrLocation; (void)noFutureLevel; (void)pauseTimerInMenu;")
    g += body_tail
    g.append("}\n")
    for n, params, body in parts:
        g.append("static void %s(%s) {" % (n, conv_sig(params)))
        g += body
        g.append("}\n")
    body_s = "\n".join(g)

    defs = []
    for k in sorted(USED_RAM):
        defs.append("#define EYL_%s 0x%X" % (k, RAM[k]))
    for k in sorted(USED_FLAGS):
        defs.append("#define EYL_EVF_%s 0x%X" % (k, FLAGS[k]))
    body_s = body_s.replace("#define P(off, v) sr_poke(c, (off), (v))\n", "#define P(off, v) sr_poke(c, (off), (v))\n" + "\n".join(defs) + "\n", 1)

    # CustomTopOfScreenText: straight line, run through the same translator
    ct = open(os.path.join(OW, "CustomTopOfScreenText.cs"), encoding="utf-8-sig").read().split("\n")
    a = next(i for i, l in enumerate(ct) if "int subrOffset = context.workingOffset;" in l)
    b = max(i for i, l in enumerate(ct) if l.strip() == "return true;")
    ctl = translate(ct[a + 2:b + 1], set())
    ctl = [l for l in ctl]
    body_s += "\n/* CustomTopOfScreenText.process() */\nint sr_p4_hlevel_custom_top_text(struct sr_ctx *c) {\n  int subrOffset = c->working_offset;\n  sr_work_set_int(c, \"customTopOfScreenTextOffset\", subrOffset);\n" + "\n".join(ctl) + "\n}\n"
    # the translation of CustomTopOfScreenText needs its own constants
    extra = [k for k in USED_RAM]
    for k in sorted(USED_RAM):
        if "#define EYL_%s " % k not in body_s:
            body_s = body_s.replace("#define P(off, v)", "#define EYL_%s 0x%X\n#define P(off, v)" % (k, RAM[k]), 1)
    # MinMaxLevel: the area table, in source order (a later entry for the same map wins)
    mm = open(os.path.join(OW, "MinMaxLevel.cs"), encoding="utf-8-sig").read()
    sv = open(os.path.join(UP, "processing/common/SomVanillaValues.cs"), encoding="utf-8-sig").read()
    maps = {n: int(v, 0) for n, v in re.findall(r"public const int (MAPNUM_\w+)\s*=\s*(0x[0-9A-Fa-f]+|\d+);", sv)}
    ents = re.findall(r"minLevels\[(MAPNUM_\w+)\]\s*=\s*(\d+);", mm)
    body_s += "\n/* MinMaxLevel area table: {map number, level}, in source order */\nconst int sr_p4_hlevel_min_levels[][2] = {\n"
    body_s += "".join("  {%d, %d},   /* %s */\n" % (maps[n], int(v), n) for n, v in ents)
    body_s += "};\nconst int sr_p4_hlevel_min_levels_n = %d;\n" % len(ents)
    h.insert(-1, "extern const int sr_p4_hlevel_min_levels[][2];\nextern const int sr_p4_hlevel_min_levels_n;")
    open(os.path.join(OUT, "sr_p4_hk_level_gen.h"), "w").write("\n".join(h) + "\n")
    open(os.path.join(OUT, "sr_p4_hk_level_gen.c"), "w").write(body_s)
    return 0


if __name__ == "__main__":
    sys.exit(main())
