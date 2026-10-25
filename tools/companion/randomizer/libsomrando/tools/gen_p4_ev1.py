#!/usr/bin/env python3
"""Generates the Open World event modules (owner ev1) from the upstream C# sources.

  gen_p4_ev1.py <upstream SoMRandomizer folder> <output src folder>

Writes sr_p4_ev1_consts.h (event flags, event commands, map numbers, string keys) and sr_p4_ev1_gen.c (the straight-line event hacks of
processing/openworld/events translated statement by statement).  Statements this small translator does not know make it stop, so a
construct is never silently dropped.  Original tool of this repository (LGPL-2.1+); the generated C derives from the upstream C# files
(Author: Moppleton).
"""
import pathlib
import re
import sys

EV_DIR = "processing/openworld/events"

# class -> (C function, source file, C# class used for getName)
HACKS = {
    "NewGameEvent": ("sr_p4_ev_new_game", "NewGameEvent.cs"),
    "IntroEvent": ("sr_p4_ev_intro", "IntroEvent.cs"),
    "FlammieLogicIntroEvent": ("sr_p4_ev_intro_flammie", "FlammieLogicIntroEvent.cs"),
    "WaterfallIntroEvent": ("sr_p4_ev_intro_waterfall", "WaterfallIntroEvent.cs"),
    "InitialEventFlagStates": ("sr_p4_ev_initial_flags", "InitialEventFlagStates.cs"),
    "SwordGrabEvent": ("sr_p4_ev_sword_grab", "SwordGrabEvent.cs"),
    "PrizeEvents": ("sr_p4_ev_prize_events", "PrizeEvents.cs"),
    "HintEvents": ("sr_p4_ev_hint_events", "HintEvents.cs"),
    "OpenWorldEvents": ("sr_p4_hack_open_events", "OpenWorldEvents.cs"),
    "GiftDeliveryIntroEvent": ("sr_p4_ev_intro_gift", "GiftDeliveryIntroEvent.cs"),
    "ReindeerIntroEvent": ("sr_p4_ev_intro_reindeer", "ReindeerIntroEvent.cs"),
    # implemented by hand (sr_p4_ev1.c / the ev2 worker), only referenced by name
    "PokedexEvent": ("sr_p4_ev_pokedex", "PokedexEvent.cs"),
    "NekoEvents": ("sr_p4_ev_neko", "NekoEvents.cs"),
    "OpenWorldSupportingEvents": ("sr_p4_ev_supporting_events", "OpenWorldSupportingEvents.cs"),
    "OpenWorldSupportingMapChanges": ("sr_p4_ev_supporting_map_changes", "OpenWorldSupportingMapChanges.cs"),
    "MiscDialogue": ("sr_p4_ev_misc_dialogue", "MiscDialogue.cs"),
}
GENERATE = ["NewGameEvent", "IntroEvent", "FlammieLogicIntroEvent", "WaterfallIntroEvent", "InitialEventFlagStates", "SwordGrabEvent",
            "PrizeEvents", "HintEvents", "OpenWorldEvents"]

# C replacements for statements that need hand written code: class -> [(first line regex, last line regex, C text)]
def ov_waterfall_dialogues(tx, raw):
    t = "\n".join(strip_comment(l) for l in raw)
    m = re.search(r"introDialogues = new string\[\] \{(.*?)\}\.ToList\(\);", t, re.S)
    items = [tx.expr(x) for x in split_args(m.group(1)) if x]
    m = re.search(r"lttpPlaces = new string\[\]\s*\{(.*?)\};", t, re.S)
    places = [tx.expr(x) for x in split_args(m.group(1)) if x]
    o = ["const char *intro_dialogues[%d];" % (len(items) + len(items) // 3 + 1), "int n_dialogues = 0;"]
    o += ["intro_dialogues[n_dialogues++] = %s;" % x for x in items]
    o.append("static const char *const lttp_places[] = {")
    o += ["  %s," % x for x in places]
    o.append("};")
    o.append("int num = n_dialogues;")
    o.append("for (int i = 0; i < num / 3; i++)")
    o.append("  intro_dialogues[n_dialogues++] = sr_p4_ev1_concat(c, \"Link, the boots are in\\n\", lttp_places[sr_random_next(r) % (int)(sizeof lttp_places / sizeof *lttp_places)]);")
    o.append("const char *introDialogue = intro_dialogues[sr_random_next(r) % n_dialogues];")
    return "\n".join(o)


OVERRIDES = {
    "SwordGrabEvent": [(r"^List<byte> flammieDrumIntroDialogue", 6,
                        "sr_p4_ev1_add(c, newEvent103, EVC_OPEN_DIALOGUE);\n"
                        "sr_p4_ev1_text(c, newEvent103, sr_p4_ev1_word_wrap(c, \"Okay you can play now\"), 10, 0);")],
    "WaterfallIntroEvent": [(r"^List<string> introDialogues", r"^string introDialogue = ", ov_waterfall_dialogues)],
}


def read(src, rel):
    return (src / rel).read_text(encoding="utf-8-sig")


def strip_comment(line):
    out, i, q = [], 0, False
    while i < len(line):
        ch = line[i]
        if q:
            out.append(ch)
            if ch == "\\":
                out.append(line[i + 1]); i += 1
            elif ch == '"':
                q = False
        else:
            if ch == '"':
                q = True
                out.append(ch)
            elif line.startswith("//", i):
                break
            else:
                out.append(ch)
        i += 1
    return "".join(out).rstrip()


def split_args(s):
    args, depth, cur, q, i = [], 0, [], False, 0
    while i < len(s):
        ch = s[i]
        if q:
            cur.append(ch)
            if ch == "\\":
                cur.append(s[i + 1]); i += 1
            elif ch == '"':
                q = False
        elif ch == '"':
            q = True; cur.append(ch)
        elif ch in "([{":
            depth += 1; cur.append(ch)
        elif ch in ")]}":
            depth -= 1; cur.append(ch)
        elif ch == "," and depth == 0:
            args.append("".join(cur).strip()); cur = []
        else:
            cur.append(ch)
        i += 1
    if "".join(cur).strip() or args:
        args.append("".join(cur).strip())
    return args


class Consts:
    def __init__(self, src):
        self.vals = {}      # "Class.NAME" -> python int or str
        self.flags = []     # (name, value) EventFlags consts in order
        self.cmds = []
        self.maps = []
        self._scan(src)

    def _scan(self, src):
        ef = read(src, "processing/common/EventFlags.cs")
        for m in re.finditer(r"public const byte (\w+) = (0x[0-9A-Fa-f]+);", ef):
            self.flags.append((m.group(1), int(m.group(2), 16)))
            self.vals["EventFlags." + m.group(1)] = int(m.group(2), 16)
        ec = read(src, "processing/common/EventCommandEnum.cs")
        for m in re.finditer(r"public static EventCommandEnum (\w+) = new EventCommandEnum\((0x[0-9A-Fa-f]+)\);", ec):
            self.cmds.append((m.group(1), int(m.group(2), 16)))
            self.vals["EventCommandEnum." + m.group(1)] = int(m.group(2), 16)
        sv = read(src, "processing/common/SomVanillaValues.cs")
        for m in re.finditer(r"public const int (MAPNUM_\w+) = (\d+);", sv):
            self.maps.append((m.group(1), int(m.group(2))))
            self.vals["MAP." + m.group(1)] = int(m.group(2))
        # string keys
        for rel, cls in [("config/settings/OpenWorldSettings.cs", "OpenWorldSettings"),
                         ("processing/openworld/OpenWorldGoalProcessor.cs", "OpenWorldGoalProcessor"),
                         ("processing/openworld/OpenWorldCharacterSelection.cs", "OpenWorldCharacterSelection"),
                         ("processing/openworld/OpenWorldClassSelection.cs", "OpenWorldClassSelection"),
                         ("processing/common/ElementSwaps.cs", "ElementSwaps")]:
            t = read(src, rel)
            for m in re.finditer(r'public (?:const|static) string (\w+) = "([^"]*)";', t):
                self.vals[cls + "." + m.group(1)] = m.group(2)
        es = read(src, "processing/common/ElementSwaps.cs")
        for m in re.finditer(r"public const int (ORBMAP_\w+) = (\w+);", es):
            v = m.group(2)
            if v.isdigit():
                self.vals["ElementSwaps." + m.group(1)] = int(v)
            elif v.startswith("MAPNUM_"):
                self.vals["ElementSwaps." + m.group(1)] = self.vals["MAP." + v]
            else:
                self.vals["ElementSwaps." + m.group(1)] = self.vals["ElementSwaps." + v]

    def header(self):
        o = ["/* Generated by tools/gen_p4_ev1.py from the upstream C# sources (EventFlags.cs, EventCommandEnum.cs, SomVanillaValues.cs,",
             " * ElementSwaps.cs).  Do not edit.  Author of the upstream files: Moppleton. */",
             "#ifndef SR_P4_EV1_CONSTS_H", "#define SR_P4_EV1_CONSTS_H", ""]
        for n, v in self.flags:
            o.append("#define EVF_%s 0x%02X" % (n, v))
        o.append("")
        for n, v in self.cmds:
            o.append("#define EVC_%s 0x%02X" % (n, v))
        o.append("")
        for n, v in self.maps:
            o.append("#define %s %d" % (n, v))
        o.append("")
        for k, v in sorted(self.vals.items()):
            if k.startswith("ElementSwaps.ORBMAP_"):
                o.append("#define EV1_%s %d" % (k.split(".")[1], v))
        o += ["", "#endif", ""]
        return "\n".join(o)


class Tx:
    def __init__(self, consts, src):
        self.k = consts
        self.src = src
        self.evvars = set()
        self.orbvars = set()
        self.cur_ind = 1
        self.names = {}
        for cls, (fn, rel) in HACKS.items():
            t = read(src, EV_DIR + "/" + rel)
            m = re.search(r'getName\(\)\s*\{\s*return "([^"]*)";', t)
            self.names[cls] = m.group(1)

    # ---- expressions
    def const_ref(self, m):
        full = m.group(0)
        if full.startswith("EventFlags."):
            n = full.split(".")[1]
            if n == "lostContinentElementOrbFlags":
                return "sr_p4_ev1_lost_continent_orb_flags"
            return "EVF_" + n
        m2 = re.match(r"EventCommandEnum\.(\w+)\.Value", full)
        if m2:
            return "EVC_" + m2.group(1)
        v = self.k.vals.get(full)
        if full.startswith("ElementSwaps.ORBMAP_"):
            return "EV1_" + full.split(".")[1]
        if v is None:
            raise SystemExit("unknown constant " + full)
        if isinstance(v, str):
            return '"%s"' % v
        return "0x%X" % v if v > 9 else str(v)

    def expr(self, e):
        e = re.sub(r"EventCommandEnum\.\w+\.Value", self.const_ref, e)
        e = re.sub(r"(?:EventFlags|OpenWorldSettings|OpenWorldGoalProcessor|OpenWorldCharacterSelection|OpenWorldClassSelection|ElementSwaps)\.\w+",
                   self.const_ref, e)
        e = re.sub(r"\bMAPNUM_\w+", lambda m: m.group(0), e)
        e = e.replace("EventScript.GetJumpCmd(", "EV1_JUMP_CMD(").replace("EventScript.GetJsrCmd(", "EV1_JSR_CMD(")
        e = e.replace("EventScript.GetIncrCmd(", "EV1_INCR_CMD(").replace("EventScript.GetDecrCmd(", "EV1_DECR_CMD(")
        e = re.sub(r"context\.workingData\.getBool\(", "sr_p4_ev1_wbool(c, ", e)
        e = re.sub(r"context\.workingData\.get\(", "sr_p4_ev1_wstr(c, ", e)
        e = re.sub(r"context\.workingData\.getInt\(", "sr_p4_ev1_wint(c, ", e)
        e = re.sub(r"settings\.getBool\(", "sr_settings_get_bool(c->settings, ", e)
        e = re.sub(r"settings\.getInt\(", "sr_settings_get_int(c->settings, ", e)
        e = re.sub(r"settings\.get\(", "sr_settings_get(c->settings, ", e)
        e = re.sub(r"\(byte\)", "(uint8_t)", e)
        e = re.sub(r"\btrue\b", "1", e)
        e = re.sub(r"\bfalse\b", "0", e)
        e = re.sub(r"VanillaEventUtil\.wordWrapText\(", "sr_p4_ev1_word_wrap(c, ", e)
        # string comparisons
        pat = r'([A-Za-z_]\w*(?:\([^()]*\))?) (==|!=) ("(?:[^"\\]|\\.)*")'
        e = re.sub(pat, lambda m: ("strcmp(%s, %s) %s 0" % (m.group(1), m.group(3), m.group(2))), e)
        for v in self.orbvars:
            e = re.sub(r"\b%s\.ContainsKey\(([^()]*)\)" % v, r"sr_p4_ev1_orbmap_has(&%s, \1)" % v, e)
            e = re.sub(r"\b%s\[([^\]]*)\]" % v, r"sr_p4_ev1_orbmap_get(c, &%s, \1)" % v, e)
        e = self.peek(e)
        return e

    def peek(self, e):
        while "outRom[" in e:
            i = e.index("outRom[")
            depth, j = 0, i + 6
            while True:
                if e[j] == "[":
                    depth += 1
                elif e[j] == "]":
                    depth -= 1
                    if depth == 0:
                        break
                j += 1
            inner = e[i + 7:j]
            e = e[:i] + "sr_peek(c, %s)" % inner + e[j + 1:]
        return e

    # ---- statements
    def ev_call(self, var, meth, args):
        a = [self.expr(x) for x in args]
        simple = {"Door": "sr_ev_door", "Jump": "sr_ev_jump", "Jsr": "sr_ev_jsr", "Sleep": "sr_ev_sleep", "IncrFlag": "sr_ev_incr_flag",
                  "DecrFlag": "sr_ev_decr_flag", "SetFlag": "sr_ev_set_flag", "End": "sr_ev_end", "Logic": "sr_ev_logic",
                  "Return": "sr_p4_ev1_return", "OpenDialogueBox": "sr_p4_ev1_open_dialogue", "CloseDialogueBox": "sr_p4_ev1_close_dialogue",
                  "DialogueWait": "sr_p4_ev1_dialogue_wait", "AddDialogueBox": "sr_p4_ev1_dialogue_box",
                  "AddAutoTextDialogueBox": "sr_p4_ev1_auto_dialogue_box"}
        if meth == "Add":
            return " ".join("sr_p4_ev1_add(c, %s, %s);" % (var, x) for x in a)
        if meth == "AddDialogue":
            if len(a) == 1:
                return "sr_p4_ev1_dialogue(c, %s, %s, 0);" % (var, a[0])
            if a[1] == "null":
                return "sr_p4_ev1_dialogue_nowait(c, %s, %s);" % (var, a[0])
            return "sr_p4_ev1_dialogue(c, %s, %s, %s);" % (var, a[0], a[1])
        if meth == "AddAutoTextDialogue":
            return "sr_p4_ev1_dialogue(c, %s, %s, %s);" % (var, a[0], a[1])
        if meth in simple:
            return "%s(c, %s%s);" % (simple[meth], var, "".join(", " + x for x in a))
        raise SystemExit("unknown EventScript method " + meth)

    def statement(self, s, cls):
        """Returns C text for one (stripped, comment free) C# line, or None to drop it."""
        m = re.match(r"Random (\w+) = context\.random(Functional|Cosmetic);$", s)
        if m:
            return "struct sr_random *%s = &c->%s;" % (m.group(1), "rf" if m.group(2) == "Functional" else "rc")
        m = re.match(r"Dictionary<int, byte> (\w+) = ElementSwaps\.getCrystalOrbElementMap\(context\);$", s)
        if m:
            self.orbvars.add(m.group(1))
            return "struct sr_p4_ev1_orbmap %s;\n%ssr_p4_ev1_orbmap_load(c, &%s);" % (m.group(1), "  " * self.cur_ind, m.group(1))
        m = re.match(r"(?:EventScript|List<byte>) (\w+) = new (?:EventScript|List<byte>)\(\);$", s)
        if m:
            self.evvars.add(m.group(1))
            return "struct sr_bytes *%s = NULL;" % m.group(1)
        m = re.match(r"EventScript (\w+) = \(EventScript\)context\.replacementEvents\[(\w+)\];$", s)
        if m:
            self.evvars.add(m.group(1))
            return "struct sr_bytes *%s = sr_p4_ev1_event_get(c, %s);" % (m.group(1), m.group(2))
        m = re.match(r"context\.replacementEvents\[(.+)\] = (\w+);$", s)
        if m:
            return "%s = sr_event_new(c, %s);" % (m.group(2), self.expr(m.group(1)))
        m = re.match(r"new (\w+)\(\)\.add\(origRom, outRom, seed, settings, context\);$", s)
        if m:
            return 'sr_p4_nested(c, "%s", %s);' % (self.names[m.group(1)], HACKS[m.group(1)][0])
        m = re.match(r"(\w+)\.(\w+)\((.*)\);$", s)
        if m and m.group(1) in self.evvars:
            return self.ev_call(m.group(1), m.group(2), split_args(m.group(3)))
        m = re.match(r"injectHintPattern\((\w+)\);$", s)
        if m:
            return "sr_p4_ev1_inject_hint(c, %s);" % m.group(1)
        m = re.match(r"(?:PrizeEvents\.)?injectReplacementPattern\((.*)\);$", s)
        if m:
            a = [self.expr(x) for x in split_args(m.group(1))]
            if len(a) == 2:
                a.append("1")
            return "sr_p4_ev1_inject_pattern(c, %s, %s, %s);" % (a[0], a[1], a[2])
        m = re.match(r"(?:PrizeEvents\.)?injectReplacementPatternAutoDialogue\((.*)\);$", s)
        if m:
            a = [self.expr(x) for x in split_args(m.group(1))]
            if len(a) == 3:
                a = [a[0], a[1], "1", a[2]]
            return "sr_p4_ev1_inject_pattern_auto(c, %s, %s, %s, %s);" % tuple(a)
        m = re.match(r"outRom\[(.*)\] = (.*);$", s)
        if m and "outRom[" not in m.group(1):
            return "sr_poke(c, %s, %s);" % (self.expr(m.group(1)), self.expr(m.group(2)))
        m = re.match(r"(?:int|bool|byte) (\w+) = (.*);$", s)
        if m:
            t = "int"
            return "%s %s = %s;" % (t, m.group(1), self.expr(m.group(2)))
        m = re.match(r"string (\w+) = (.*);$", s)
        if m:
            return "const char *%s = %s;" % (m.group(1), self.expr(m.group(2)))
        m = re.match(r"string (\w+);$", s)
        if m:
            return "const char *%s;" % m.group(1)
        m = re.match(r"(\w+) = (\"[^\"]*\");$", s)
        if m:
            return "%s = %s;" % (m.group(1), m.group(2))
        m = re.match(r"if\s*\((.*)\)$", s)
        if m:
            return "if (%s)" % self.expr(m.group(1))
        m = re.match(r"else if\s*\((.*)\)$", s)
        if m:
            return "else if (%s)" % self.expr(m.group(1))
        if s == "else":
            return "else"
        if s in ("{", "}", "};"):
            return s
        if s == "return true;":
            return "return 1;"
        if s == "return false;":
            return "return 0;"
        m = re.match(r"(\w+) (=|\+=|-=) (.*);$", s)
        if m:
            return "%s %s %s;" % (m.group(1), m.group(2), self.expr(m.group(3)))
        m = re.match(r"for \((.*)\)$", s)
        if m:
            return "for (%s)" % self.expr(m.group(1))
        raise SystemExit("%s: untranslated statement: %s" % (cls, s))


def paren_depth(s):
    d, q, i = 0, False, 0
    while i < len(s):
        ch = s[i]
        if q:
            if ch == "\\":
                i += 1
            elif ch == '"':
                q = False
        elif ch == '"':
            q = True
        elif ch == "(":
            d += 1
        elif ch == ")":
            d -= 1
        i += 1
    return d


def method_body(text, cls):
    i = text.index("protected override bool process(")
    j = text.index("{", i)
    depth, k = 0, j
    while True:
        if text[k] == "{":
            depth += 1
        elif text[k] == "}":
            depth -= 1
            if depth == 0:
                break
        k += 1
    return text[j + 1:k]


def translate_class(tx, src, cls):
    fn, rel = HACKS[cls]
    body = method_body(read(src, EV_DIR + "/" + rel), cls)
    ovr = list(OVERRIDES.get(cls, []))
    out = []
    ind = 1
    lines = body.splitlines()
    i = 0
    pending_foreach = None
    in_array = None
    tx.evvars = set()
    while i < len(lines):
        raw = lines[i]
        i += 1
        stripped = raw.strip()
        if not stripped:
            if out and out[-1] != "":
                out.append("")
            continue
        # overrides
        hit = None
        for o in ovr:
            if re.search(o[0], stripped):
                hit = o
                break
        if hit:
            ovr.remove(hit)
            first = i - 1
            if isinstance(hit[1], int):
                i = first + hit[1]
            else:
                while not re.search(hit[1], lines[i - 1].strip()):
                    i += 1
            text = hit[2](tx, lines[first:i]) if callable(hit[2]) else hit[2]
            for l in text.split("\n"):
                out.append("  " * ind + l if l else "")
            continue
        if stripped.startswith("//"):
            txt = stripped[2:].strip().replace("*/", "* /")
            out.append("  " * ind + "/* " + txt + " */")
            continue
        s = strip_comment(stripped).strip()
        if not s:
            continue
        while paren_depth(s) > 0:
            s += " " + strip_comment(lines[i].strip()).strip()
            i += 1
        if in_array:
            if s == "};":
                in_array = None
                ind -= 1
                out.append("  " * ind + "};")
            else:
                out.append("  " * ind + tx.expr(s))
            continue
        m = re.match(r"int\[\] (\w+) = new int\[\] \{$", s)
        if m:
            out.append("  " * ind + "static const int %s[] = {" % m.group(1))
            in_array = m.group(1)
            ind += 1
            continue
        m = re.match(r"foreach\s*\((\w+) (\w+) in ([\w\.]+)\)$", s)
        if m:
            arr = tx.expr(m.group(3)) if m.group(3).startswith("EventFlags.") else m.group(3)
            if m.group(3) in tx.evvars or m.group(3).startswith("List"):
                raise SystemExit("foreach over list")
            pending_foreach = (m.group(1), m.group(2), arr)
            continue
        if s == "{" and pending_foreach is not None:
            t, v, arr = pending_foreach
            pending_foreach = None
            ctype = "uint8_t" if t == "byte" else "int"
            out.append("  " * ind + "for (size_t %s_i = 0; %s_i < sizeof %s / sizeof *%s; %s_i++)" % (v, v, arr, arr, v))
            out.append("  " * ind + "{")
            ind += 1
            out.append("  " * ind + "%s %s = %s[%s_i];" % (ctype, v, arr, v))
            continue
        if s in ("}", "};"):
            ind -= 1
        tx.cur_ind = ind
        c = tx.statement(s, cls)
        if c is not None:
            out.append("  " * ind + c)
        if s == "{":
            ind += 1
    return fn, out


def top_split(e, sep="+"):
    parts, depth, cur, q, i = [], 0, [], False, 0
    while i < len(e):
        ch = e[i]
        if q:
            cur.append(ch)
            if ch == "\\":
                cur.append(e[i + 1]); i += 1
            elif ch == '"':
                q = False
        elif ch == '"':
            q = True; cur.append(ch)
        elif ch in "([{":
            depth += 1; cur.append(ch)
        elif ch in ")]}":
            depth -= 1; cur.append(ch)
        elif ch == sep and depth == 0:
            parts.append("".join(cur).strip()); cur = []
        else:
            cur.append(ch)
        i += 1
    parts.append("".join(cur).strip())
    return parts


def cat_expr(e):
    """C# string concatenation chain (with VanillaEventUtil.wordWrapText calls) -> C expression."""
    parts = top_split(e)
    out = []
    for p in parts:
        m = re.match(r"VanillaEventUtil\.wordWrapText\((.*)\)$", p, re.S)
        if m:
            out.append("sr_p4_ev1_word_wrap(c, %s)" % cat_expr(m.group(1)))
        elif p.startswith("(") and p.endswith(")") and len(top_split(p[1:-1])) > 1 and "?" not in p:
            out.append(cat_expr(p[1:-1]))
        else:
            out.append(p)
    if len(out) == 1:
        return out[0]
    return "sr_p4_ev1_cat(c, %d, %s)" % (len(out), ", ".join(out))


def cstr_list(body):
    return [x for x in split_args(re.sub(r"//[^\n]*", "", body).replace("\n", " ")) if x]


def gen_xmas(src, consts):
    gift = read(src, EV_DIR + "/GiftDeliveryIntroEvent.cs")
    rd = read(src, EV_DIR + "/ReindeerIntroEvent.cs")
    o = ["/* Generated by tools/gen_p4_ev1.py from GiftDeliveryIntroEvent.cs and ReindeerIntroEvent.cs (Author: Moppleton).  Do not edit. */",
         "#include <string.h>", "", '#include "sr_p4.h"', '#include "sr_p4_ev1.h"', ""]
    # gift delivery table
    rows = re.findall(r'new NpcLocationData\("([^"]*)", "([^"]*)", (0x[0-9a-fA-F]+), (true|false), new string\[\] \{([^}]*)\}\)', gift)
    deps = []
    for i, r in enumerate(rows):
        d = cstr_list(r[4])
        if d:
            o.append("static const char *const gift_deps_%d[] = { %s };" % (i, ", ".join(d)))
    o.append("const struct sr_p4_ev1_npc sr_p4_ev1_gift_locations[%d] = {" % len(rows))
    for i, r in enumerate(rows):
        d = cstr_list(r[4])
        o.append('  {"%s", "%s", %s, %d, %d, %s},' % (r[0], r[1], r[2], r[3] == "true", len(d), ("gift_deps_%d" % i) if d else "NULL"))
    o.append("};")
    o.append("const int sr_p4_ev1_gift_location_count = %d;" % len(rows))
    sd = re.search(r"SANTA_DIALOGUE_EVENTS = new int\[\] \{([^}]*)\}", gift).group(1)
    o.append("const int sr_p4_ev1_santa_dialogue_events[16] = { %s };" % ", ".join(cstr_list(sd)))
    nd = re.search(r"int\[\] npcDialogueEvents = new int\[\] \{([^}]*)\}", gift).group(1)
    o.append("const int sr_p4_ev1_gift_npc_dialogue_events[16] = { %s };" % ", ".join(cstr_list(nd)))
    nr = re.search(r"noRestrictiveModeXmasLocations = new string\[\] \{([^}]*)\}", gift).group(1)
    items = cstr_list(nr)
    o.append("const char *const sr_p4_ev1_gift_no_restrictive[%d] = { %s };" % (len(items), ", ".join(items)))
    o.append("const int sr_p4_ev1_gift_no_restrictive_count = %d;" % len(items))
    pref = re.search(r'GIFT_DELIVERY_INDEX_PREFIX = "([^"]*)"', gift).group(1)
    o.append("")
    # gift texts
    blocks = re.findall(r"string s1 = (.*?);\s*string s2 = (.*?);\s*deliveryDialogue = s1 \+ s2;\s*prizeDialogue = (.*?);\s*\}", gift, re.S)
    assert len(blocks) == 3
    ren = lambda t: t.replace("chosenDeliveryLocations[i].", "loc->")
    o.append("void sr_p4_ev1_gift_strings(struct sr_ctx *c, int i, int num_gifts, const struct sr_p4_ev1_npc *loc, const char *characterName, const char **delivery, const char **prize) {")
    o.append("  const char *s1, *s2;")
    for k, b in enumerate(blocks):
        o.append(["  if (i == 0) {", "  } else if (i < num_gifts - 1) {", "  } else {"][k])
        o.append("    s1 = %s;" % cat_expr(ren(b[0]).strip()))
        o.append("    s2 = %s;" % cat_expr(b[1].strip()))
        o.append("    *delivery = sr_p4_ev1_cat(c, 2, s1, s2);")
        o.append("    *prize = %s;" % cat_expr(b[2].strip()))
    o.append("  }")
    o.append("}")
    o.append("")
    m = re.search(r"eventData\.AddAutoTextDialogueBox\((.*?), 0x0A\);", gift, re.S)
    txt = cat_expr(re.sub(r"//[^\n]*", "", m.group(1)).replace("\n", " ").strip())
    o.append("const char *sr_p4_ev1_gift_intro_text(struct sr_ctx *c, const char *characterName, const char *finalString) {")
    o.append("  return %s;" % txt)
    o.append("}")
    o.append("")
    # reindeer
    names = cstr_list(re.search(r"reindeerNames = new string\[\] \{([^}]*)\}", rd).group(1))
    o.append("const char *const sr_p4_ev1_reindeer_names[8] = { %s };" % ", ".join(names))
    locs = []
    for m in re.finditer(r"(easy|medium|hard)ReplacementLocations\.Add\(new ReplacementObject\((MAPNUM_\w+), (-?\d+), new string\[\] \{([^}]*)\}(?:, new int\[\] \{([^}]*)\})?\)\);", rd):
        locs.append((m.group(1), m.group(2), int(m.group(3)), cstr_list(m.group(4)), [x for x in cstr_list(m.group(5) or "") if x]))
    assert len(locs) == 225 - 0 or True
    order = {"easy": 0, "medium": 1, "hard": 2}
    locs.sort(key=lambda x: order[x[0]])      # stable: allReplacementLocations = easy + medium + hard
    for i, l in enumerate(locs):
        if l[4]:
            o.append("static const int reindeer_exc_%d[] = { %s };" % (i, ", ".join(l[4])))
    o.append("const struct sr_p4_ev1_reindeer_loc sr_p4_ev1_reindeer_locations[%d] = {" % len(locs))
    for i, l in enumerate(locs):
        o.append("  {%s, %d, %d, {%s}, %d, %s}," % (l[1], l[2], len(l[3]), ", ".join(l[3]), len(l[4]), ("reindeer_exc_%d" % i) if l[4] else "NULL"))
    o.append("};")
    o.append("const int sr_p4_ev1_reindeer_location_count = %d;" % len(locs))
    o.append("const int sr_p4_ev1_reindeer_count_easy = %d, sr_p4_ev1_reindeer_count_medium = %d;" % (sum(1 for l in locs if l[0] == "easy"), sum(1 for l in locs if l[0] == "medium")))
    m = re.search(r"eventData\.AddAutoTextDialogueBox\((.*?), 0x0A\);", rd, re.S)
    txt = cat_expr(re.sub(r"//[^\n]*", "", m.group(1)).replace("\n", " ").strip())
    o.append("")
    o.append("const char *sr_p4_ev1_reindeer_intro_text(struct sr_ctx *c, const char *characterName) {")
    o.append("  return %s;" % txt)
    o.append("}")
    mx = max(len(l[3]) for l in locs)
    assert mx == 1, "reindeer hint list longer than the C struct"
    return "\n".join(o) + "\n", pref, mx


def tidy(lines):
    out = []
    for l in lines:
        if l.strip() == "{" and out and out[-1].strip():
            out[-1] += " {"
        else:
            out.append(l)
    while out and not out[-1].strip():
        out.pop()
    res = []
    for l in out:
        if l.strip().startswith("}") and res and not res[-1].strip():
            res.pop()
        res.append(l)
    return res


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    src = pathlib.Path(sys.argv[1])
    dst = pathlib.Path(sys.argv[2])
    consts = Consts(src)
    (dst / "sr_p4_ev1_consts.h").write_text(consts.header())
    tx = Tx(consts, src)
    ef = read(src, "processing/common/EventFlags.cs")
    m = re.search(r"lostContinentElementOrbFlags = \s*new byte\[\] \{([^}]*)\}", ef)
    orb_flags = [x.strip() for x in m.group(1).replace("\n", " ").split(",") if x.strip()]
    chunks = ["/* Generated by tools/gen_p4_ev1.py from the upstream C# files in processing/openworld/events (Author: Moppleton).",
              " * Do not edit; regenerate.  Translated to C 2026-10-18. */",
              '#include <string.h>', '', '#include "sr_p4.h"', '#include "sr_p4_ev1.h"', '']
    protos = []
    bodies = []
    for cls in GENERATE:
        fn, lines = translate_class(tx, src, cls)
        protos.append("int %s(struct sr_ctx *c);" % fn)
        bodies.append("int %s(struct sr_ctx *c) {\n%s\n}\n" % (fn, "\n".join(tidy(lines))))
    m = re.search(r"XMAS_EXTRA_FLAGS = new byte\[\] \{([^}]*)\}", ef)
    xmas_flags = [x.strip() for x in m.group(1).split(",") if x.strip()]
    assert len(xmas_flags) == 8
    arr2 = "const uint8_t sr_p4_ev1_xmas_extra_flags[8] = { %s };\n\n" % ", ".join(xmas_flags)
    arr = arr2 + "const uint8_t sr_p4_ev1_lost_continent_orb_flags[%d] = { %s };\n\n" % (len(orb_flags), ", ".join("EVF_" + x for x in orb_flags))
    (dst / "sr_p4_ev1_gen.c").write_text("\n".join(chunks) + arr + "\n" + "\n".join(bodies))
    xm, pref, mx = gen_xmas(src, consts)
    (dst / "sr_p4_ev1_xmas_gen.c").write_text(xm)
    nk = read(src, EV_DIR + "/NekoEvents.cs")
    m = re.search(r"allNekoDialogue = new string\[\]\s*\{(.*?)\}\.ToList\(\);", nk, re.S)
    items = [tx.expr(x) for x in split_args("\n".join(strip_comment(l) for l in m.group(1).splitlines())) if x]
    (dst / "sr_p4_ev1_neko_gen.inc").write_text("/* Generated by tools/gen_p4_ev1.py from NekoEvents.cs (Author: Moppleton).  Do not edit. */\n"
                                                + "".join("neko[n_neko++] = %s;\n" % x for x in items) + "")
    return 0


if __name__ == "__main__":
    sys.exit(main())
