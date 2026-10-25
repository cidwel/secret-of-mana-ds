#!/usr/bin/env python3
"""Generates src/sr_p4_ow_core_loc_body.inc and src/sr_p4_ow_core_prz_body.inc from the upstream Open World sources.

  gen_p4_core.py UPSTREAM_SOMRANDOMIZER_DIR OUT_SRC_DIR

The statements of OpenWorldLocations.getForSelectedOptions (the location table) and of OpenWorldPrizes.getForSelectedOptions (the prize
event data) are translated line by line into C with the helpers of src/sr_p4_ow_core_int.h; any line the translator does not know is a hard error,
so a change of the upstream tables can never be dropped silently.  Original work of this repository (LGPL-2.1 or later).
"""
import os
import re
import sys

UP = sys.argv[1]
OUT = sys.argv[2]


def read(rel):
    with open(os.path.join(UP, rel), encoding="utf-8-sig") as f:
        return f.read().split("\n")


def split_top(s, sep):
    """Splits s at the separator character outside strings and brackets."""
    parts, depth, cur, instr, i = [], 0, "", False, 0
    while i < len(s):
        ch = s[i]
        if instr:
            cur += ch
            if ch == "\\":
                cur += s[i + 1]
                i += 1
            elif ch == '"':
                instr = False
        elif ch == '"':
            instr = True
            cur += ch
        elif ch in "({[":
            depth += 1
            cur += ch
        elif ch in ")}]":
            depth -= 1
            cur += ch
        elif ch == sep and depth == 0:
            parts.append(cur)
            cur = ""
        else:
            cur += ch
        i += 1
    parts.append(cur)
    return [p.strip() for p in parts]


def strip_comment(line):
    out, instr, i = "", False, 0
    while i < len(line):
        ch = line[i]
        if instr:
            out += ch
            if ch == "\\":
                out += line[i + 1]
                i += 1
            elif ch == '"':
                instr = False
        elif ch == '"':
            instr = True
            out += ch
        elif line.startswith("//", i):
            break
        else:
            out += ch
        i += 1
    return out.rstrip()


# ----------------------------------------------------------------------------------------------------------- constants
consts = {}   # name -> python value (int, str)


def load_consts():
    for rel in ("processing/common/SomVanillaValues.cs", "processing/common/EventFlags.cs", "processing/openworld/PlandoProperties.cs",
                "processing/openworld/randomization/OpenWorldLocations.cs", "processing/common/ElementSwaps.cs",
                "processing/openworld/OpenWorldGoalProcessor.cs", "processing/common/VanillaEventUtil.cs"):
        for line in read(rel):
            m = re.match(r'\s*public const (?:int|byte) (\w+) = (0x[0-9A-Fa-f]+|\d+);', line)
            if m:
                consts[m.group(1)] = int(m.group(2), 0)
            m = re.match(r'\s*public const string (\w+) = "([^"]*)";', line)
            if m:
                consts[m.group(1)] = m.group(2)
            m = re.match(r'\s*public const string (\w+) = (\w+) \+ "([^"]*)";', line)
            if m and isinstance(consts.get(m.group(2)), str):
                consts[m.group(1)] = consts[m.group(2)] + m.group(3)
    for line in read("processing/common/ElementSwaps.cs"):
        m = re.match(r'\s*public const int (ORBMAP_\w+) = (\w+);', line)
        if m and m.group(2) in consts:
            consts[m.group(1)] = consts[m.group(2)]
    for line in read("processing/common/EventCommandEnum.cs"):
        m = re.match(r'\s*public static EventCommandEnum (\w+) = new EventCommandEnum\((0x[0-9A-Fa-f]+)\);', line)
        if m:
            consts["EventCommandEnum." + m.group(1)] = int(m.group(2), 16)


load_consts()


# ------------------------------------------------------------------------------------------------------------ expressions
def tr_str(e, locals_):
    """C# string expression (literals, identifiers, + concatenation, ternary) -> C expression of type const char *."""
    e = e.strip()
    q = split_top(e, "?")
    if len(q) == 2:
        rest = q[1]
        # the colon of a ternary: the first top-level colon
        idx = None
        depth = 0
        instr = False
        for i, ch in enumerate(rest):
            if instr:
                if ch == '"':
                    instr = False
            elif ch == '"':
                instr = True
            elif ch in "({[":
                depth += 1
            elif ch in ")}]":
                depth -= 1
            elif ch == ":" and depth == 0:
                idx = i
                break
        a, b = rest[:idx], rest[idx + 1:]
        return "(%s) ? %s : %s" % (tr_cond(q[0], locals_), tr_str(a, locals_), tr_str(b, locals_))
    parts = split_top(e, "+")
    out = []
    for p in parts:
        if p.startswith('"'):
            out.append(p)
        elif re.fullmatch(r"\w+", p) and isinstance(consts.get(p), str):
            out.append('"%s"' % consts[p])
        elif re.fullmatch(r"[A-Za-z_]\w*\.\w+", p) and isinstance(consts.get(p.split(".")[1]), str):
            out.append('"%s"' % consts[p.split(".")[1]])
        elif re.fullmatch(r"[A-Za-z_]\w*", p):
            out.append(p)
        else:
            raise SystemExit("cannot translate string expression part: %r in %r" % (p, e))
    if len(out) == 1:
        return out[0]
    return "ow_cat(c, %s, NULL)" % ", ".join(out)


def tr_cond(e, locals_):
    e = e.strip()
    for sep, cop in (("||", " || "), ("&&", " && ")):
        parts = []
        depth, cur, i, instr = 0, "", 0, False
        while i < len(e):
            ch = e[i]
            if instr:
                cur += ch
                if ch == '"':
                    instr = False
            elif ch == '"':
                instr = True
                cur += ch
            elif ch == "(":
                depth += 1
                cur += ch
            elif ch == ")":
                depth -= 1
                cur += ch
            elif depth == 0 and e.startswith(sep, i):
                parts.append(cur)
                cur = ""
                i += 1
            else:
                cur += ch
            i += 1
        parts.append(cur)
        if len(parts) > 1:
            return cop.join("(%s)" % tr_cond(p, locals_) for p in parts)
    m = re.fullmatch(r'(.+?)\s*(==|!=)\s*(.+)', e)
    if m:
        l, op, r = m.groups()
        lstr = l.strip().startswith('"') or l.strip() in locals_.get("strs", ()) or isinstance(consts.get(l.strip().split(".")[-1]), str)
        rstr = r.strip().startswith('"') or r.strip() in locals_.get("strs", ()) or isinstance(consts.get(r.strip().split(".")[-1]), str)
        if lstr or rstr:
            return "%sstrcmp(%s, %s)" % ("!" if op == "==" else "", tr_str(l, locals_), tr_str(r, locals_))
        return "%s %s %s" % (tr_num(l), op, tr_num(r))
    m = re.fullmatch(r'(\w+)\.Contains\((\w+)\)', e)
    if m:
        if m.group(1) == "weaponIndexes":
            return "ow_ilist_contains(&weaponIndexes, %s)" % m.group(2)
        if m.group(1) == "startingItems":
            return 'ow_sl_contains(&startingItems, "%s")' % consts[m.group(2)]
    if re.fullmatch(r"!?\w+", e):
        return e
    raise SystemExit("cannot translate condition: %r" % e)


def tr_num(e):
    """integer/byte expression -> C expression."""
    e = e.strip()
    def sub_enum(m):
        return "0x%02X" % consts["EventCommandEnum." + m.group(1)]
    e = re.sub(r"EventCommandEnum\.(\w+)\.Value", sub_enum, e)
    e = re.sub(r"EventFlags\.(\w+)", lambda m: "0x%02X" % consts[m.group(1)], e)
    e = e.replace("(byte)", "(uint8_t)")
    e = re.sub(r"spellGrantValues\[key\]", "sg_val[ki]", e)
    return e


def tr_double(e):
    return e.strip()


def strlit_args(args, n):
    if len(args) != n:
        raise SystemExit("expected %d arguments, got %r" % (n, args))


# ------------------------------------------------------------------------------------------------------------- locations
def gen_locations():
    lines = read("processing/openworld/randomization/OpenWorldLocations.cs")
    # statements of getForSelectedOptions from the first allLocations.Add to "return allLocations;"
    start = next(i for i, l in enumerate(lines) if "allLocations.Add(new PrizeLocation(\"mech rider 3" in l)
    end = next(i for i, l in enumerate(lines) if "return allLocations;" in l)
    out = ["/* Generated by tools/gen_p4_core.py from OpenWorldLocations.getForSelectedOptions: do not edit. */"]
    strs = {"_earthPalaceElement", "_firePalaceElement1", "_firePalaceElement2", "_firePalaceElement3", "_lunaPalaceElement",
            "_matangoCaveElement", "_upperLandElement", "goal", "ep", "lp", "fp1", "fp2", "fp3", "mf"}
    loc = {"strs": strs}

    def tr_arr(e):
        e = e.strip()
        m = re.fullmatch(r"new string\[\] \{(.*)\}", e)
        if m:
            inner = m.group(1).strip().rstrip(",")
            items = [x for x in split_top(inner, ",") if x] if inner else []
            return "ow_sa(c, %sNULL)" % "".join(tr_str(x, loc) + ", " for x in items)
        m = re.fullmatch(r"(\w+)\.ToArray\(\)", e)
        if m:
            return "ow_sl_arr(c, &%s)" % m.group(1)
        raise SystemExit("cannot translate array: %r" % e)

    i = start
    while i < end:
        raw = strip_comment(lines[i])
        s = raw.strip()
        i += 1
        if not s:
            continue
        if s in ("{", "}", "else"):
            out.append(s if s != "else" else "else")
            continue
        m = re.fullmatch(r"if \((.*)\)", s)
        if m:
            out.append("if (%s)" % tr_cond(m.group(1), loc))
            continue
        m = re.fullmatch(r"allLocations\.Add\(new PrizeLocation\((.*)\)\);", s)
        if m:
            a = split_top(m.group(1), ",")
            if len(a) == 9:
                name, mp, ob, ev, idx, opts, hints, locked, reach = a
            elif len(a) == 7:
                name, ev, idx, opts, hints, locked, reach = a
                mp, ob = "-1", "-1"
            elif len(a) == 6:
                name, ev, idx, opts, locked, reach = a
                hints = "new string[] { }"
                mp, ob = "-1", "-1"
            else:
                raise SystemExit("PrizeLocation with %d args: %r" % (len(a), s))
            mpc = "SR_P4_%s" % mp if mp.startswith("MAPNUM_") else mp
            out.append("ow_loc_add(c, L, %s, %s, %s, %s, %s, %s, %s, %s, %s);" % (name, mpc, ob, ev, idx, tr_arr(opts), tr_arr(hints), tr_arr(locked), reach))
            continue
        m = re.fullmatch(r"List<string> (\w+) = new string\[\] \{(.*)\}\.ToList\(\);", s) or re.fullmatch(r"List<string> (\w+) = new List<string> ?\{(.*)\};", s)
        if m:
            inner = m.group(2).strip().rstrip(",")
            items = [x for x in split_top(inner, ",") if x] if inner else []
            out.append("struct ow_slist %s = {0};" % m.group(1))
            for it in items:
                out.append("ow_sl_add(c, &%s, %s);" % (m.group(1), tr_str(it, loc)))
            continue
        m = re.fullmatch(r"List<string> (\w+) = new List<string>\(\);", s)
        if m:
            out.append("struct ow_slist %s = {0};" % m.group(1))
            continue
        m = re.fullmatch(r"(\w+)\.Add\((.*)\);", s)
        if m:
            out.append("ow_sl_add(c, &%s, %s);" % (m.group(1), tr_str(m.group(2), loc)))
            continue
        m = re.fullmatch(r"string (\w+) = (.*);", s)
        if m:
            out.append("const char *%s = %s;" % (m.group(1), tr_str(m.group(2), loc)))
            continue
        raise SystemExit("cannot translate location line %d: %r" % (i, s))
    return "\n".join(out) + "\n"


# --------------------------------------------------------------------------------------------------------------- prizes
def gen_prizes():
    lines = read("processing/openworld/randomization/OpenWorldPrizes.cs")
    start = next(i for i, l in enumerate(lines) if l.strip() == "if (anyCharsAdded)" and i > 200)
    end = next(i for i, l in enumerate(lines) if "List<PrizeLocation> filteredLocations" in l)
    out = ["/* Generated by tools/gen_p4_core.py from OpenWorldPrizes.getForSelectedOptions: do not edit. */"]
    loc = {"strs": {"goal", "boyClass", "girlClass", "spriteClass", "startingChar"}}
    pending = None
    i = start
    while i < end:
        raw = strip_comment(lines[i])
        s = raw.strip()
        i += 1
        if not s:
            continue
        if s in ("{", "}", "else"):
            out.append(s)
            if s == "{" and pending:
                out.append(pending)
                pending = None
            continue
        m = re.fullmatch(r"else if \((.*)\)", s)
        if m:
            out.append("else if (%s)" % tr_cond(m.group(1), loc))
            continue
        m = re.fullmatch(r"if \((.*)\)", s)
        if m:
            out.append("if (%s)" % tr_cond(m.group(1), loc))
            continue
        m = re.fullmatch(r"foreach \(byte flag in altStarterWeaponFlags\)", s)
        if m:
            out.append("for (size_t fi = 0; fi < altStarterWeaponFlags.len; fi++)")
            pending = "uint8_t flag = altStarterWeaponFlags.p[fi];"
            continue
        if s == "foreach (int key in spellGrantValues.Keys)":
            out.append("for (int ki = 0; ki < sg_n; ki++)")
            pending = "int key = sg_key[ki];"
            continue
        if s == "int val = spellGrantValues[key];":
            out.append("int val = sg_val[ki];")
            continue
        m = re.fullmatch(r"val = (0x[0-9A-F]+);", s)
        if m:
            out.append("val = %s;" % m.group(1))
            continue
        if s == "Dictionary<int, int> spellGrantValues = new Dictionary<int, int>();":
            out.append("int sg_key[3], sg_val[3], sg_n = 0;")
            continue
        m = re.fullmatch(r"spellGrantValues\[(\d)\] = (0x[0-9A-F]+);", s)
        if m:
            out.append("ow_sg_set(sg_key, sg_val, &sg_n, %s, %s);" % (m.group(1), m.group(2)))
            continue
        m = re.fullmatch(r"List<byte> (\w+) = new List<byte>\(\);", s)
        if m:
            out.append("struct sr_bytes %s = {0};" % m.group(1))
            continue
        if s == "List<byte> goldEventFlags = new List<byte>(EventFlags.OPENWORLD_GOLD_FLAGS);":
            out.append("int n_gold_flags = 0; uint8_t gold_flags[64]; ow_gold_flags_init(gold_flags, &n_gold_flags);")
            continue
        if s == "byte flammieDrumFlag = goldEventFlags[0];":
            out.append("uint8_t flammieDrumFlag = gold_flags[0];")
            continue
        if s == "goldEventFlags.RemoveAt(0);":
            out.append("for (int gi = 1; gi < n_gold_flags; gi++) { gold_flags[gi - 1] = gold_flags[gi]; }\nn_gold_flags--;")
            continue
        m = re.fullmatch(r"double seedScore = (.*);", s)
        if m:
            out.append("double seedScore = %s;" % m.group(1))
            continue
        m = re.fullmatch(r"seedScore = (.*);", s)
        if m:
            out.append("seedScore = %s;" % m.group(1))
            continue
        m = re.fullmatch(r"(\w+)\.Add\((.*)\);", s)
        if m and m.group(1) not in ("allPrizes",):
            out.append("sr_bytes_push(c, &%s, %s);" % (m.group(1), tr_num(m.group(2))))
            continue
        m = re.fullmatch(r"(\w+)\.AddRange\(getRandomGiftText\(r, prizePrefixes, (.*)\)\);", s)
        if m:
            out.append("ow_gift_text(c, &%s, %s);" % (m.group(1), tr_str(m.group(2), loc)))
            continue
        m = re.fullmatch(r"(\w+)\.AddRange\((\w+)\.ToArray\(\)\);", s)
        if m:
            out.append("sr_bytes_push_n(c, &%s, %s.p, %s.len);" % (m.group(1), m.group(2), m.group(2)))
            continue
        m = re.fullmatch(r"PrizeItem girlSpriteStarterWeapon = new PrizeItem\((.*)\);", s)
        if m:
            a = split_top(m.group(1), ",")
            out.append("PZ_ADD(%s, %s, &%s, %s, %s, %s);" % (tr_str(a[0], loc), tr_str(a[1], loc), "girlSpriteStarterWeaponEventData", tr_str(a[3], loc), tr_num(a[4]), tr_double(a[5])))
            continue
        if s == "allPrizes.Add(girlSpriteStarterWeapon);":
            continue
        m = re.fullmatch(r"allPrizes\.Add\(new PrizeItem\((.*)\)\);", s)
        if m:
            a = split_top(m.group(1), ",")
            mm = re.fullmatch(r"(\w+)\.ToArray\(\)", a[2])
            if not mm:
                raise SystemExit("PrizeItem data is not a ToArray: %r" % s)
            out.append("PZ_ADD(%s, %s, &%s, %s, %s, %s);" % (tr_str(a[0], loc), tr_str(a[1], loc), mm.group(1), tr_str(a[3], loc), tr_num(a[4]), tr_double(a[5])))
            continue
        raise SystemExit("cannot translate prize line %d: %r" % (i, s))
    return "\n".join(out) + "\n"


def gen_lists():
    """flammieRequiredLocations / upperLandOrbRequiredLocations (OpenWorldPrizes) and the gold/orb flag tables (EventFlags)."""
    text = "\n".join(read("processing/openworld/randomization/OpenWorldPrizes.cs"))
    out = ["/* Generated by tools/gen_p4_core.py: static lists of OpenWorldPrizes and EventFlags used by the Open World search. */"]
    for cs, cn in (("flammieRequiredLocations", "ow_flammie_required"), ("upperLandOrbRequiredLocations", "ow_upperland_required")):
        m = re.search(cs + r" = new string\[\] \{(.*?)\}\.ToList\(\);", text, re.S)
        items = re.findall(r'"([^"]*)"', m.group(1))
        out.append("static const char *const %s[] = {%s};" % (cn, ", ".join('"%s"' % x for x in items)))
        out.append("#define %s_N %d" % (cn.upper(), len(items)))
    ftext = "\n".join(read("processing/common/EventFlags.cs"))
    for cs, cn in (("OPENWORLD_GOLD_FLAGS", "ow_gold_flags"), ("OPENWORLD_ORB_FLAGS", "ow_orb_flags")):
        m = re.search(cs + r" = new byte\[\] \{(.*?)\};", ftext, re.S)
        body = re.sub(r"//[^\n]*", "", m.group(1))
        vals = re.findall(r"0x[0-9A-Fa-f]+", body)
        out.append("static const uint8_t %s[] = {%s};" % (cn, ", ".join(vals)))
        out.append("#define %s_N %d" % (cn.upper(), len(vals)))
    return "\n".join(out) + "\n"


def main():
    # MAPNUM_* constants used by the table, as C macros
    used = sorted(set(re.findall(r"MAPNUM_\w+", "\n".join(read("processing/openworld/randomization/OpenWorldLocations.cs")))))
    hdr = ["/* Generated by tools/gen_p4_core.py: the MAPNUM_* constants (SomVanillaValues) that OpenWorldLocations uses. */"]
    for n in used:
        hdr.append("#define SR_P4_%s %d" % (n, consts[n]))
    orb = {k: v for k, v in consts.items() if k.startswith("ORBMAP_")}
    for k, v in sorted(orb.items()):
        hdr.append("#define SR_P4_%s %d" % (k, v))
    with open(os.path.join(OUT, "sr_p4_ow_core_map.inc"), "w") as f:
        f.write("\n".join(hdr) + "\n")
    with open(os.path.join(OUT, "sr_p4_ow_core_lists.inc"), "w") as f:
        f.write(gen_lists())
    with open(os.path.join(OUT, "sr_p4_ow_core_loc_body.inc"), "w") as f:
        f.write(gen_locations())
    with open(os.path.join(OUT, "sr_p4_ow_core_prz_body.inc"), "w") as f:
        f.write(gen_prizes())


main()
