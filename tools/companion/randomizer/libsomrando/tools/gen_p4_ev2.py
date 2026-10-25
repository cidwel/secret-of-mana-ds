#!/usr/bin/env python3
"""Generates src/sr_p4_ev2_{supporting_events,supporting_map_changes,misc_dialogue}.c from the upstream C# of Moppu/SecretOfManaRandomizer v1.49.

  gen_p4_ev2.py <upstream SoMRandomizer folder> <out dir>

Line oriented translation of the process() bodies of OpenWorldSupportingEvents, OpenWorldSupportingMapChanges and MiscDialogue.  Only the
statement shapes those files use are accepted; anything else stops the generator with the offending line.  The few statements that need
more than a mechanical rewrite (string concatenation, dictionaries, the reindeer goal) are listed in OVERRIDES.  Original tool of this
repository; the generated code is derived from the upstream files (Author: Moppleton), LGPL-2.1+."""
import os
import re
import sys

SRC = sys.argv[1]
OUT = sys.argv[2]
EV = os.path.join(SRC, "processing/openworld/events")


class Stop(Exception):
    pass


def read(path):
    return open(path, encoding="utf-8-sig").read()


# ------------------------------------------------------------------------------------------------------------- constants
FLAGS = {n: int(v, 16) for n, v in re.findall(r"public const byte (\w+) = (0x[0-9A-Fa-f]+);", read(SRC + "/processing/common/EventFlags.cs"))}
CMDS = {n: int(v, 16) for n, v in re.findall(r"public static EventCommandEnum (\w+) = new EventCommandEnum\((0x[0-9A-Fa-f]+)\);",
                                              read(SRC + "/processing/common/EventCommandEnum.cs"))}
MAPS = {n: int(v) for n, v in re.findall(r"public const int (MAPNUM_\w+) = (\d+);", read(SRC + "/processing/common/SomVanillaValues.cs"))}
_m = re.search(r"lostContinentElementOrbFlags\s*=\s*new byte\[\] \{([^}]*)\}", read(SRC + "/processing/common/EventFlags.cs"))
LOST_FLAGS = [FLAGS[x.strip()] for x in _m.group(1).split(",") if x.strip()]
_m = re.search(r"XMAS_EXTRA_FLAGS = new byte\[\] \{([^}]*)\}", read(SRC + "/processing/common/EventFlags.cs"))
GOALS = {"GOAL_MTR": "manatree", "GOAL_GIFTMODE": "xmas2020", "GOAL_REINDEER": "xmas2020b", "GOAL_MANABEAST": "manabeast"}
KEYS = {"OpenWorldClassSelection.ANY_MAGIC_EXISTS": "anyMagicExists", "OpenWorldClassSelection.BOY_CLASS": "boyClass",
        "OpenWorldClassSelection.GIRL_CLASS": "girlClass", "OpenWorldClassSelection.SPRITE_CLASS": "spriteClass",
        "OpenWorldGoalProcessor.GOAL_SHORT_NAME": "openWorldGoalShortName",
        "OpenWorldMtrSeedNumSelection.MANA_SEEDS_REQUIRED": "manaSeedsRequired",
        "OpenWorldSettings.PROPERTYNAME_LOGIC_MODE": "opLogic", "OpenWorldSettings.PROPERTYNAME_FLAMMIE_DRUM_IN_LOGIC": "opFlammieDrumInLogic"}


def wrap_text(text):
    """VanillaEventUtil.wordWrapText"""
    if "\n" in text:
        return text
    width, out = 0, ""
    for token in text.split(" "):
        if len(token) + width < 27:
            out += token + " "
            width += len(token) + 1
        else:
            out += "\n" + token + " "
            width = len(token) + 1
    return out


def cs_unescape(s):
    return re.sub(r"\\(.)", lambda m: {"n": "\n", "t": "\t", '"': '"', "\\": "\\", "'": "'"}[m.group(1)], s)


def c_string(s):
    o = ""
    for ch in s:
        if ch == "\n":
            o += "\\n"
        elif ch == "\t":
            o += "\\t"
        elif ch == '"':
            o += '\\"'
        elif ch == "\\":
            o += "\\\\"
        elif ord(ch) < 32 or ord(ch) > 126:
            raise Stop("non-ASCII/control char in string: %r" % s)
        else:
            o += ch
    return '"' + o + '"'


STR = r'"(?:[^"\\]|\\.)*"'


# ------------------------------------------------------------------------------------------------------------ utilities
def split_args(s):
    """splits on top-level commas (parens, braces, brackets, strings)"""
    out, depth, cur, i = [], 0, "", 0
    while i < len(s):
        ch = s[i]
        if ch == '"':
            m = re.match(STR, s[i:])
            cur += m.group(0)
            i += len(m.group(0))
            continue
        if ch in "({[":
            depth += 1
        elif ch in ")}]":
            depth -= 1
        if ch == "," and depth == 0:
            out.append(cur.strip())
            cur = ""
        else:
            cur += ch
        i += 1
    if cur.strip():
        out.append(cur.strip())
    return out


def match_close(s, i, open_ch, close_ch):
    depth = 0
    while i < len(s):
        ch = s[i]
        if ch == '"':
            i += len(re.match(STR, s[i:]).group(0))
            continue
        if ch == open_ch:
            depth += 1
        elif ch == close_ch:
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise Stop("unbalanced: " + s)


def peek_expr(e):
    """outRom[x] -> sr_peek(c, x) (nested)"""
    while True:
        i = e.find("outRom[")
        if i < 0:
            return e
        j = match_close(e, i + 6, "[", "]")
        e = e[:i] + "sr_peek(c, " + peek_expr(e[i + 7:j]) + ")" + e[j + 1:]


def tr_expr(e):
    e = e.strip()
    e = re.sub(r"VanillaEventUtil\.wordWrapText\((%s)\)" % STR, lambda m: c_string(wrap_text(cs_unescape(m.group(1)[1:-1]))), e)
    e = re.sub(r"EventCommandEnum\.(\w+)\.Value", lambda m: "0x%02X" % CMDS[m.group(1)], e)
    e = re.sub(r"EventFlags\.(\w+)", lambda m: "0x%02X" % FLAGS[m.group(1)], e)
    e = re.sub(r"\b(MAPNUM_\w+)\b", lambda m: str(MAPS[m.group(1)]), e)
    e = re.sub(r"\(byte\)", "(uint8_t)", e)
    e = re.sub(r"EventScript\.GetJumpCmd\(", "JC(", e)
    e = re.sub(r"\((\w+) == OpenWorldGoalProcessor\.(\w+)\)", lambda m: '!strcmp(%s, "%s")' % (m.group(1), GOALS[m.group(2)]), e)
    e = re.sub(r"(\w+) == OpenWorldGoalProcessor\.(\w+)", lambda m: '!strcmp(%s, "%s")' % (m.group(1), GOALS[m.group(2)]), e)
    e = e.replace("VanillaMapUtil.getTriggerOffset(outRom, ", "sr_p4_ev2_trigger_offset(c, ")
    e = re.sub(r"\b(get\w+|set\w+)\(outRom, ", lambda m: "obj_" + m.group(1) + "(c, ", e)
    e = peek_expr(e)
    return e


def lit_int(e):
    e = tr_expr(e)
    return int(eval(e, {}, {})) if re.fullmatch(r"[0-9xXa-fA-F+\-*<>() ]+", e) else None


def cmd_bytes(e):
    """a two byte command (GetJumpCmd(n) or new byte[] {a, b}) as 'a, b'"""
    e = e.strip()
    m = re.fullmatch(r"EventScript\.GetJumpCmd\((.*)\)", e)
    if m:
        n = lit_int(m.group(1))
        return "0x%02X, 0x%02X" % (0x10 + (n >> 8), n & 0xFF)
    m = re.fullmatch(r"new byte\[\] \{(.*)\}", e)
    if m:
        a = split_args(m.group(1))
        return ", ".join("0x%02X" % lit_int(x) for x in a)
    raise Stop("command expression " + e)


# --------------------------------------------------------------------------------------------------------------- overrides
# regex of the (joined) statement -> replacement C (list of lines), or 'SKIP_UNTIL:<regex of the stopping statement>' handled by the caller
OVERRIDES_EVENTS = [
    (r"newEvent2c1\.AddDialogueBox\(\"Today's orb element is:.*",
     ["{", "  char msg[64];", "  snprintf(msg, sizeof msg, \"Today's orb element is:\\n%s!\", sr_p4_ev2_orb_element_name(sr_p4_ev2_crystal_orb(c, 35)));",
      "  sr_p4_ev2_dialogue_box(c, newEvent2c1, msg);", "}"]),
    (r"newEvent4d7\.AddDialogueBox\(\"Return with \" \+ manaSeedsRequired.*",
     ["{", "  char msg[96];", "  snprintf(msg, sizeof msg, \"Return with %d of the\\nMana seeds to complete\\nthe run!\", manaSeedsRequired);",
      "  sr_p4_ev2_dialogue_box(c, newEvent4d7, msg);", "}"]),
    (r"Dictionary<int, byte> crystalOrbColorMap = .*", []),
]
# skip from the first statement to (excluding) the statement matching the second
SKIPS_EVENTS = [
    (r"Dictionary<int, int> spellGrantValues = .*", r"newEvent42d\.End\(\);",
     ["{", "  const char *classes[3] = {boyClass, girlClass, spriteClass};",
      "  for (int ch = 1; ch <= 3; ch++) {   /* the C# dictionary keys enumerate in insertion order 1, 2, 3 */",
      "    int v = 0;", "    if (!strcmp(classes[ch - 1], \"OGgirl\")) v = 0x38;", "    else if (!strcmp(classes[ch - 1], \"OGsprite\")) v = 0x07;",
      "    if (v) {", "      sr_bytes_push(c, newEvent42d, 0x%02X);" % CMDS["SET_CHARACTER_ATTRIBUTE"], "      sr_bytes_push(c, newEvent42d, ch);",
      "      sr_bytes_push(c, newEvent42d, 0xCD);", "      sr_bytes_push(c, newEvent42d, v);", "    }", "  }", "}"]),
]


class Gen:
    def __init__(self, name, body, overrides, skips, special):
        self.name, self.body, self.overrides, self.skips, self.special = name, body, overrides, skips, special
        self.out = []
        self.ind = 1
        self.evars = set()
        self.pending_ev = {}
        self.after_brace = []
        self.arrays = {}
        self.skip_until = None

    def emit(self, line):
        self.out.append("  " * self.ind + line if line else "")

    def comment(self, text):
        text = text.strip().replace("*/", "* /").replace("??", "? ?")
        if text:
            self.emit("/* " + text + " */")

    # ------------------------------------------------------------------ statements
    def stmt(self, s, cm):
        for pat, until, lines in self.skips:
            if re.fullmatch(pat, s):
                self.skip_until = until
                for ln in lines:
                    self.emit(ln)
                return
        for pat, lines in self.overrides:
            if re.fullmatch(pat, s):
                for ln in lines:
                    self.emit(ln)
                return
        if self.special and self.special(self, s, cm):
            return
        if s == "{":
            if self.out and re.search(r"^\s*(\}\s*)?(if|else if|for|else)\b", self.out[-1]) and not re.search(r"[;{]$", self.out[-1].rstrip()) and "/*" not in self.out[-1]:
                self.out[-1] += " {"
            else:
                self.emit("{")
            self.ind += 1
            for ln in self.after_brace:
                self.emit(ln)
            self.after_brace = []
            return
        if s == "}":
            self.ind -= 1
            self.emit("}")
            return
        m = re.fullmatch(r"EventScript (\w+) = new EventScript\(\);", s)
        if m:
            self.pending_ev[m.group(1)] = True
            return
        m = re.fullmatch(r"context\.replacementEvents\[(\w+)\] = (\w+);", s)
        if m:
            v = m.group(2)
            if v not in self.pending_ev:
                raise Stop(s)
            del self.pending_ev[v]
            self.evars.add(v)
            self.emit("struct sr_bytes *%s = sr_event_new(c, %s);%s" % (v, tr_expr(m.group(1)), "   /* " + cm.strip() + " */" if cm.strip() else ""))
            return
        m = re.fullmatch(r"(\w+)\.(\w+)\((.*)\);", s)
        if m and m.group(1) in self.evars:
            self.event_call(m.group(1), m.group(2), m.group(3), cm)
            return
        m = re.fullmatch(r"outRom\[(.*?)\] (\|?)= (.*);", s)
        if m:
            off, op, val = tr_expr(m.group(1)), m.group(2), tr_expr(m.group(3))
            if op:
                self.emit("sr_poke(c, %s, sr_peek(c, %s) | (%s));%s" % (off, off, val, self.tc(cm)))
            else:
                self.emit("sr_poke(c, %s, %s);%s" % (off, val, self.tc(cm)))
            return
        m = re.fullmatch(r"(setObject\w+|getObject\w+)\(outRom, (.*)\);", s)
        if m:
            self.emit("obj_%s(c, %s);%s" % (m.group(1), tr_expr(m.group(2)), self.tc(cm)))
            return
        m = re.fullmatch(r"(\w+) \+= (.*);", s)
        if m:
            self.emit("%s += %s;%s" % (m.group(1), tr_expr(m.group(2)), self.tc(cm)))
            return
        if s == "return true;":
            self.emit("return 1;")
            return
        m = re.fullmatch(r"(bool|int|string) (\w+) = (.*);", s)
        if m:
            ty, name, rhs = m.groups()
            cty = {"bool": "int", "int": "int", "string": "const char *"}[ty]
            self.emit("%s %s = %s;%s" % (cty, name, self.decl_rhs(rhs), self.tc(cm)))
            return
        m = re.fullmatch(r"int\[\] (\w+) = new int\[\]\s*\{(.*)\};", s)
        if m:
            vals = [tr_expr(x) for x in split_args(m.group(2))]
            self.arrays[m.group(1)] = vals
            self.emit("const int %s[] = {%s};" % (m.group(1), ", ".join(vals)))
            return
        m = re.fullmatch(r"foreach \(int (\w+) in (\w+)\)", s)
        if m and m.group(2) in self.arrays:
            v, arr = m.groups()
            self.emit("for (size_t %s_i = 0; %s_i < sizeof %s / sizeof *%s; %s_i++)" % (v, v, arr, arr, v))
            self.after_brace.append("int %s = %s[%s_i];" % (v, arr, v))
            return
        m = re.fullmatch(r"foreach \(byte (\w+) in EventFlags\.lostContinentElementOrbFlags\)", s)
        if m:
            v = m.group(1)
            self.emit("for (size_t %s_i = 0; %s_i < sizeof lost_continent_orb_flags; %s_i++)" % (v, v, v))
            self.after_brace.append("int %s = lost_continent_orb_flags[%s_i];" % (v, v))
            return
        m = re.fullmatch(r"for ?\(int (\w+) ?= ?(\d+); ?\1 ?< ?(\w+); ?\1\+\+\)", s)
        if m:
            self.emit("for (int %s = %s; %s < %s; %s++)" % (m.group(1), m.group(2), m.group(1), m.group(3), m.group(1)))
            return
        m = re.fullmatch(r"(if|else if) ?\((.*)\)", s)
        if m:
            hdr = "%s (%s)%s" % (m.group(1), self.cond(m.group(2)), self.tc(cm))
            if m.group(1) == "else if" and self.out and self.out[-1].strip() == "}":
                self.out[-1] += " " + hdr
            else:
                self.emit(hdr)
            return
        if s == "else":
            if self.out and self.out[-1].strip() == "}":
                self.out[-1] += " else"
            else:
                self.emit("else")
            return
        raise Stop("unsupported statement: " + s)

    def tc(self, cm):
        cm = cm.strip().replace("*/", "* /").replace("??", "? ?")
        return "   /* %s */" % cm if cm else ""

    def decl_rhs(self, rhs):
        for k, v in KEYS.items():
            rhs = rhs.replace(k, c_string(v))
        m = re.fullmatch(r"context\.workingData\.getBool\((.*)\)", rhs)
        if m:
            return "sr_p4_ev2_work_bool(c, %s)" % m.group(1)
        m = re.fullmatch(r"context\.workingData\.getInt\((.*)\)", rhs)
        if m:
            return "sr_p4_ev2_work_int(c, %s)" % m.group(1)
        m = re.fullmatch(r"context\.workingData\.get\((.*)\)", rhs)
        if m:
            return "sr_p4_ev2_work_str(c, %s)" % m.group(1)
        m = re.fullmatch(r"settings\.getBool\((.*)\)", rhs)
        if m:
            return "sr_settings_get_bool(c->settings, %s)" % m.group(1)
        m = re.fullmatch(r"settings\.get\((.*)\) == (\".*\")", rhs)
        if m:
            return "!strcmp(sr_settings_get(c->settings, %s), %s)" % (m.group(1), m.group(2))
        return tr_expr(rhs)

    def cond(self, c):
        for k, v in KEYS.items():
            c = c.replace(k, c_string(v))
        return tr_expr(c)

    def event_call(self, v, meth, args, cm):
        a = split_args(args)
        t = self.tc(cm)
        P = lambda x: self.emit("sr_bytes_push(c, %s, %s);%s" % (v, x, t))
        if meth in ("Jump", "Jsr", "Door", "Sleep", "SetFlag", "IncrFlag", "DecrFlag", "End"):
            fn = {"Jump": "sr_ev_jump", "Jsr": "sr_ev_jsr", "Door": "sr_ev_door", "Sleep": "sr_ev_sleep", "SetFlag": "sr_ev_set_flag",
                  "IncrFlag": "sr_ev_incr_flag", "DecrFlag": "sr_ev_decr_flag", "End": "sr_ev_end"}[meth]
            self.emit("%s(c, %s%s);%s" % (fn, v, "".join(", " + tr_expr(x) for x in a), t))
        elif meth == "Add":
            if len(a) != 1:
                raise Stop("Add with %d args" % len(a))
            P(tr_expr(a[0]))
        elif meth == "Return":
            P("EV_RETURN")
        elif meth == "OpenDialogueBox":
            P("EV_OPEN_DIALOGUE")
        elif meth == "CloseDialogueBox":
            P("EV_CLOSE_DIALOGUE")
        elif meth in ("AddDialogueBox", "AddDialogue"):
            fn = "sr_p4_ev2_dialogue_box" if meth == "AddDialogueBox" else "sr_p4_ev2_dialogue"
            self.emit("%s(c, %s, %s);%s" % (fn, v, self.str_arg(a[0]), t))
        elif meth == "Logic":
            self.emit("sr_ev_logic(c, %s, %s, %s, %s, %s);%s" % (v, tr_expr(a[0]), tr_expr(a[1]), tr_expr(a[2]), tr_expr(a[3]), t))
        elif meth in ("AddDialogueBoxWithChoices", "AddDialogueWithChoices"):
            text = self.str_arg(a[0])
            xs = [lit_int(x) for x in split_args(a[1][a[1].index("{") + 1:a[1].rindex("}")])]
            labels = split_args(a[2][a[2].index("{") + 1:a[2].rindex("}")])
            cmds = split_args(a[3][a[3].index("{") + 1:a[3].rindex("}")])
            # "new byte[][] { new byte[] { 0x1F, 0x06 }, ... }": the outer split above works because split_args honours braces
            self.emit("{")
            self.ind += 1
            self.emit("static const uint8_t xs[] = {%s};" % ", ".join("0x%02X" % x for x in xs))
            self.emit("static const char *const labels[] = {%s};" % ", ".join(self.str_arg(x) for x in labels))
            self.emit("static const uint8_t cmds[][2] = {%s};" % ", ".join("{%s}" % cmd_bytes(x) for x in cmds))
            self.emit("static const uint8_t cancel[2] = {%s};" % cmd_bytes(a[4]))
            self.emit("sr_p4_ev2_choices(c, %s, %d, %s, %d, xs, labels, cmds, cancel);" % (v, 1 if meth == "AddDialogueBoxWithChoices" else 0, text, len(xs)))
            self.ind -= 1
            self.emit("}")
        else:
            raise Stop("unsupported EventScript method " + meth)

    def str_arg(self, e):
        e = tr_expr(e)
        if re.fullmatch(r"[A-Za-z_]\w*", e):
            return e
        if not re.fullmatch(STR, e):
            raise Stop("not a string literal: " + e)
        return e

    # ------------------------------------------------------------------ driver
    def run(self):
        buf, comment = "", ""
        for raw in self.body.split("\n"):
            line = raw.strip()
            if not line:
                continue
            if line.startswith("//"):
                if not buf and self.skip_until is None:
                    self.comment(line[2:])
                continue
            line, cm = split_comment(line)
            if not line:
                continue
            buf = (buf + " " + line).strip() if buf else line
            comment = (comment + " " + cm).strip() if cm else comment
            if not complete(buf):
                continue
            s, cmt = buf, comment
            buf, comment = "", ""
            if self.skip_until is not None:
                if re.fullmatch(self.skip_until, s):
                    self.skip_until = None
                else:
                    continue
            self.stmt(s, cmt)
        if buf:
            raise Stop("dangling: " + buf)
        return self.out


def split_comment(line):
    i = 0
    while i < len(line):
        if line[i] == '"':
            i += len(re.match(STR, line[i:]).group(0))
            continue
        if line.startswith("//", i):
            return line[:i].rstrip(), line[i + 2:].strip()
        i += 1
    return line, ""


def complete(buf):
    nostr = re.sub(STR, '""', buf)
    if buf in ("{", "}", "else"):
        return True
    bal = nostr.count("(") == nostr.count(")") and nostr.count("[") == nostr.count("]")
    if not bal:
        return False
    if re.fullmatch(r"(if|else if|for|foreach) ?\(.*\)", nostr):
        return True
    return nostr.endswith(";") and nostr.count("{") == nostr.count("}")


def process_body(text):
    m = re.search(r"protected override bool process\([^)]*\)\s*\{", text)
    i, depth = m.end(), 1
    while depth:
        ch = text[i]
        if ch == '"':
            i += len(re.match(STR, text[i:]).group(0))
            continue
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
        i += 1
    return text[m.end():i - 1]


HEADER = '''/* libsomrando: C port of Moppu/SecretOfManaRandomizer v1.49 (commit 1404b7b).  LGPL-2.1 or later, see sr_core.h and COPYING.LIB.
 *
 * Derived from %s (Author: Moppleton).  Translated to C 2026-10-18.
 * Generated by tools/gen_p4_ev2.py from the upstream file (then reviewed); do not edit by hand.
 */
#include <stdio.h>
#include <string.h>

#include "sr_p4.h"
#include "sr_p4_ev2.h"

'''


def main():
    jobs = [
        ("OpenWorldSupportingEvents", "supporting_events", "sr_p4_ev_supporting_events", "processing/openworld/events/OpenWorldSupportingEvents.cs",
         OVERRIDES_EVENTS, SKIPS_EVENTS),
        ("OpenWorldSupportingMapChanges", "supporting_map_changes", "sr_p4_ev_supporting_map_changes",
         "processing/openworld/events/OpenWorldSupportingMapChanges.cs", [], []),
        ("MiscDialogue", "misc_dialogue", "sr_p4_ev_misc_dialogue", "processing/openworld/events/MiscDialogue.cs", [], []),
    ]
    for cls, short, fn, rel, ov, sk in jobs:
        text = read(os.path.join(SRC, rel))
        body = process_body(text)
        special = None
        pre = []
        if cls == "OpenWorldSupportingMapChanges":
            body = prep_map_changes(body)
        if cls == "MiscDialogue":
            body, ov = prep_misc(body)
        g = Gen(cls, body, ov, sk, special)
        lines = g.run()
        out = HEADER % rel
        if cls == "OpenWorldSupportingMapChanges":
            out = out.replace('#include "sr_p4_ev2.h"\n', '#include "sr_p4_ev1.h"\n#include "sr_p4_ev2.h"\n')
        if cls == "OpenWorldSupportingEvents":
            out += "static const uint8_t lost_continent_orb_flags[] = {%s};\n\n" % ", ".join("0x%02X" % x for x in LOST_FLAGS)
        out += "int %s(struct sr_ctx *c) {\n" % fn
        out += "\n".join(lines) + "\n}\n"
        with open(os.path.join(OUT, "sr_p4_ev2_%s.c" % short), "w") as f:
            f.write(out)


def prep_map_changes(body):
    """The reindeer goal block is refused instead of ported (needs ReindeerIntroEvent's replacement table)."""
    i = body.index("if (goal == OpenWorldGoalProcessor.GOAL_REINDEER)")
    j = match_close(body, body.index("{", i), "{", "}")
    body = body[:i] + "REINDEER_REFUSED();\n" + body[j + 1:]
    return body


def prep_misc(body):
    """MiscDialogue: the watts text pool and the random picks."""
    ov = []
    m = re.search(r"List<string> allWattsDialogue = new string\[\]\s*\{(.*?)\}\.ToList\(\);", body, re.S)
    items = split_args(m.group(1))
    lit = ", ".join(tr_expr(x) for x in items)
    body = body[:m.start()] + "WATTS_POOL(%s);" % lit.replace("\n", " ") + body[m.end():]
    body = re.sub(r"string\[\] (\w+) = new string\[\]\s*\{(.*?)\};", lambda m: "STRARRAY %s = {%s};" % (m.group(1), m.group(2)), body, flags=re.S)
    body = re.sub(r"string (\w+) = pickRandomDialogue\(r, new string\[\]\s*\{(.*?)\}\s*\);", lambda m: "PICK_NEW %s = {%s};" % (m.group(1), m.group(2)), body, flags=re.S)
    body = re.sub(r"string (\w+) = pickRandomDialogue\(r, (\w+)\);", lambda m: "PICK_OLD %s = %s;" % (m.group(1), m.group(2)), body)
    body = re.sub(r"List<byte> \w+ = VanillaEventUtil\.getBytes\(.*?\);", "", body)
    body = re.sub(r"Random r = context\.randomFunctional;", "", body)
    return body, ov


def misc_special(g, s, cm):
    m = re.fullmatch(r"WATTS_POOL\((.*)\);", s)
    if m:
        items = split_args(m.group(1))
        g.emit("const char *watts[%d] = {%s};" % (len(items), ", ".join(items)))
        g.emit("int n_watts = %d;" % len(items))
        return True
    m = re.fullmatch(r"int (\w+) = r\.Next\(\) % allWattsDialogue\.Count;", s)
    if m:
        g.emit("int %s = (int)(sr_random_next(&c->rf) %% n_watts);" % m.group(1))
        return True
    m = re.fullmatch(r"allWattsDialogue\.RemoveAt\((\w+)\);", s)
    if m:
        g.emit("sr_p4_ev2_remove_at(watts, &n_watts, %s);" % m.group(1))
        return True
    m = re.fullmatch(r"(\w+)\.AddDialogueBox\(allWattsDialogue\[(\w+)\]\);", s)
    if m:
        g.emit("sr_p4_ev2_dialogue_box(c, %s, watts[%s]);" % m.groups())
        return True
    m = re.fullmatch(r"if \(\(r\.Next\(\) % 3\) != 0\)", s)
    if m:
        g.emit("if ((sr_random_next(&c->rf) % 3) != 0)")
        return True
    m = re.fullmatch(r"STRARRAY (\w+) = \{(.*)\};", s)
    if m:
        items = [g.str_arg(x) for x in split_args(m.group(2))]
        g.emit("const char *const %s[%d] = {%s};" % (m.group(1), len(items), ", ".join(items)))
        g.emit("const int n_%s = %d;" % (m.group(1), len(items)))
        return True
    m = re.fullmatch(r"PICK_NEW (\w+) = \{(.*)\};", s)
    if m:
        items = [g.str_arg(x) for x in split_args(m.group(2))]
        g.emit("const char *%s;" % m.group(1))
        g.emit("{")
        g.emit("  static const char *const pool[%d] = {%s};" % (len(items), ", ".join(items)))
        g.emit("  %s = pool[sr_random_next(&c->rf) %% %d];" % (m.group(1), len(items)))
        g.emit("}")
        return True
    m = re.fullmatch(r"PICK_OLD (\w+) = (\w+);", s)
    if m:
        g.emit("const char *%s = %s[sr_random_next(&c->rf) %% n_%s];" % (m.group(1), m.group(2), m.group(2)))
        return True
    return False


_orig_run = Gen.run


def patched_main():
    main()


if __name__ == "__main__":
    # attach the per-class special handlers
    _orig_init = Gen.__init__

    def init(self, name, body, overrides, skips, special):
        _orig_init(self, name, body, overrides, skips, misc_special if name == "MiscDialogue" else map_special if name == "OpenWorldSupportingMapChanges" else None)

    def map_special(g, s, cm):
        if s == "REINDEER_REFUSED();":
            xf = ", ".join("0x%02X" % int(x, 16) for x in re.findall(r"0x[0-9A-Fa-f]+", _m.group(1)))
            for ln in [
                'if (!strcmp(goal, "xmas2020b")) {',
                '  static const uint8_t xmas_extra_flags[8] = {%s};' % xf,
                '  /* expects ReindeerIntroEvent to have run before this, to determine these ids */',
                '  for (int i = 0; i < 8; i++) {',
                '    char key[32];',
                '    int reindeerMapNum, objNum, reindeerMapObjOffset;',
                '    snprintf(key, sizeof key, "reindeerLocation%d", i);',
                '    sr_p4_ev1_reindeer_location(c, sr_p4_ev2_work_int(c, key), &reindeerMapNum, &objNum);',
                '    reindeerMapObjOffset = sr_vanilla_object_offset(c->out, reindeerMapNum) + 8;   /* skip header */',
                '    sr_poke(c, reindeerMapObjOffset + 8 * objNum + 0, xmas_extra_flags[i]);',
                '    sr_poke(c, reindeerMapObjOffset + 8 * objNum + 1, 0x00);',
                '  }',
                '}']:
                g.emit(ln)
            return True
        return False
    Gen.__init__ = init
    try:
        main()
    except Stop as e:
        print("STOP:", e)
        sys.exit(1)
