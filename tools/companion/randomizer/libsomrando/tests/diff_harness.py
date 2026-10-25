#!/usr/bin/env python3
"""Differential test harness: C# generator (oracle) against libsomrando, byte for byte.

  diff_harness.py [--seeds N] [--random N] [--jobs N] [--report FILE] [--hashes FILE] [--skip-matrix]

Needs the oracle (NativeAOT build of the C# headless generator, `som-dualscreen setup-randomizer` / build_native.sh exe-aot), the C command
line (`build.sh lib`) and the user's ROM (SOM_ROM, as the other companion tests).  Generated ROMs stay under the state directory; only
SHA-256 hashes are written to the optional hashes file.  Sections:

  options   `sr_cli --list-options` equals `somrando --list-options` (keys, order, defaults) for the vanilla randomizer
  baseline  the supported baseline configuration over many seeds (ASCII, digits, leading zeros, empty, non-ASCII)
  matrix    every option flipped to every legal value from the baseline: supported ones must be identical to the oracle, the others must
            be refused with an "unsupported option" error and no output file
  random    random combinations of supported values over random seeds
  api       cancel, progress numbering, headered ROM, bad ROM, bad request errors
  presets   the 17 menu presets: supported or not, and how much of what the oracle applies exists in C; every supported preset is compared
            over --preset-seeds seeds (default 20), with per-preset generation times of both generators
  ancientcave   (sections ac-*, see --ac-seeds) Ancient Cave: option list equality, baseline over many seeds, every option at every legal value, pairs,
            random subsets, API behaviour (progress numbering with the floor steps, cancel between floors, refusals, no partial file); the
            5 presets are part of presets / preset-seeds.  --ac-biomes test --oracle .../oracle-dbg/bin/somrando drives all of it with the
            test floor generator (debug builds of both sides) while the real floor generators are not ported
  features  each phase-2 feature alone (3 seeds, every legal value), every pair of supported features, random subsets and all supported at once
            (unsupported ones must be refused without a file)

A failing comparison prints the first differing offset, the number of differing bytes and the C hacks whose write ranges cover the offset.
Original work of this repository (LGPL-2.1+).
"""
import argparse
import concurrent.futures
import hashlib
import json
import os
import pathlib
import random
import re
import subprocess
import sys
import threading
import time

HERE = pathlib.Path(__file__).resolve().parent
LIB = HERE.parent
COMPANION = LIB.parent.parent
STATE = pathlib.Path(os.environ.get("SOMRANDO_PORT_STATE", "/home/cidwel/.local/state/som-decomp/randoport-20261018"))
CLI_STATE = pathlib.Path(os.environ.get("SOM_CLI_STATE", "/home/cidwel/.local/state/som-decomp/companion-cli-20261017"))
ORACLE = pathlib.Path(os.environ.get("SOMRANDO_ORACLE", str(CLI_STATE / "aot/out/somrando")))
CLI = pathlib.Path(os.environ.get("SOMRANDO_CLI", str(STATE / "build/lib/sr_cli")))
ROM = pathlib.Path(os.environ.get("SOM_ROM", "/home/cidwel/projects/rommancer/roms/snes/Secret of Mana (USA).smc"))
OUT = STATE / "diff"
# acBiomeTypes override for every ancientcave case that does not set it itself (development: "test", with the instrumented oracle)
AC_BIOMES = None
PRESET_MODES = None   # --preset-modes: restrict the presets / preset-seeds sections
INTERNAL = {"common version", "common buildDate", "common mode", "common currentGenerateProgress"}

# Options of the vanilla randomizer for which the C reproduces the C# at this stage.  The harness does not trust this table: it only
# builds a configuration that is known to be supported; the matrix proves the rest.
BASELINE_NO = ("ovrchrgFix leavePlayersBehind seReduction funSpeedup rainbowRabites longStatus fasterChests magicRebalance mpAtLevel "
               "fasterMagicLevel fasterWeaponLevel fasterMessages moreItems noBsVampires bossDeathFix speciesDamage elemDamage statusGlow "
               "mode7Fix noGigasSplit whipFix offscreenTarget buyMultiple manaMagicFix noSummonDespawn confusionFix mechRiderDeathFix "
               "adjustEnemyPos showEvades nameEntryFix showMinimap cupAtZeroHp candyHealouts magicRopeDeathFix "
               "rEnemies rBosses rWeapons rMusic rAutosave rDialogue").split()
BASELINE = {k: "no" for k in BASELINE_NO}
BASELINE.update({"characterColors": "none", "rExp": "normal", "rGold": "normal"})
SEEDS = ["1", "2", "12345", "99999999", "0042", "42", "hello", "Secret of Mana", "", "é", "日本語", "seed with spaces", "-7", "x" * 40, "2147483647",
         "0"]

lock_ids = iter(range(10 ** 9))
RUN = str(int(time.time()))   # fresh names for the few files that must not exist before a call
_slots = threading.local()
_slot_counter = iter(range(10 ** 6))
_slot_lock = threading.Lock()


def slot():
    """A per-thread file slot: ROM outputs are overwritten in place (4 MiB each), never accumulated."""
    if not hasattr(_slots, "id"):
        with _slot_lock:
            _slots.id = next(_slot_counter)
    return _slots.id


def sha(path):
    return hashlib.sha256(pathlib.Path(path).read_bytes()).hexdigest()


def set_args(settings):
    a = []
    for k, v in settings.items():
        a += ["--set", "%s=%s" % (k, v)]
    return a


def _ac(mode, settings):
    if mode == "ancientcave" and AC_BIOMES and "acBiomeTypes" not in settings:
        settings = dict(settings)
        settings["acBiomeTypes"] = AC_BIOMES
    return settings


def run_oracle(mode, seed, settings, dst, work, difficulty=None, rom=ROM):
    settings = _ac(mode, settings)
    work.mkdir(parents=True, exist_ok=True)
    cmd = [str(ORACLE), "--mode", mode, "--seed", seed, "--src", str(rom), "--dst", str(dst), "--workdir", str(work)] + set_args(settings)
    if difficulty:
        cmd += ["--difficulty", difficulty]
    return subprocess.run(cmd, capture_output=True, text=True)


def run_c(mode, seed, settings, dst, rom=ROM, extra=(), env=None, difficulty=None):
    settings = _ac(mode, settings)
    cmd = [str(CLI), "--mode", mode, "--seed", seed, "--src", str(rom), "--dst", str(dst)] + set_args(settings) + list(extra)
    if difficulty:
        cmd += ["--difficulty", difficulty]
    e = dict(os.environ)
    if env:
        e.update(env)
    return subprocess.run(cmd, capture_output=True, text=True, env=e)


def first_diff(a, b):
    da, db = pathlib.Path(a).read_bytes(), pathlib.Path(b).read_bytes()
    if len(da) != len(db):
        return {"size": [len(da), len(db)]}
    first, count = None, 0
    for i in range(len(da)):
        if da[i] != db[i]:
            if first is None:
                first = i
            count += 1
    return {"first": first, "count": count}


def attribute(mode, seed, settings, offset, difficulty=None):
    """Which C hacks write `offset` (from SOMRANDO_TRACE)."""
    tr = OUT / ("trace_%d.jsonl" % slot())
    out = OUT / ("trace_%d.sfc" % slot())
    run_c(mode, seed, settings, out, env={"SOMRANDO_TRACE": str(tr)}, difficulty=difficulty)
    hits = []
    try:
        for line in tr.read_text().splitlines():
            j = json.loads(line)
            for s, n in j["ranges"]:
                if s <= offset < s + n:
                    hits.append(j["name"])
    except Exception:
        pass
    return hits or ["no C hack writes it (a hack the C does not run, or a stub)"]


class Results:
    def __init__(self):
        self.cases = []
        self.fail = []

    def add(self, section, name, status, **kw):
        rec = dict(section=section, name=name, status=status, **kw)
        self.cases.append(rec)
        if status not in ("identical", "refused-ok", "ok"):
            self.fail.append(rec)
        return rec


def compare_case(res, section, name, mode, seed, settings, difficulty=None):
    tag = slot()
    o = OUT / ("o_%d.sfc" % tag)
    c = OUT / ("c_%d.sfc" % tag)
    for f in (o, c):
        if f.exists():
            f.write_bytes(b"")   # truncate, so a failed run cannot leave the previous ROM behind
    ro = run_oracle(mode, seed, settings, o, OUT / ("work_%d" % tag), difficulty=difficulty)
    rc = run_c(mode, seed, settings, c, difficulty=difficulty)
    rec = dict(mode=mode, seed=seed, settings=settings, difficulty=difficulty)
    if ro.returncode != 0:
        # the oracle itself rejected it: the C must not produce a ROM either
        if rc.returncode == 0:
            return res.add(section, name, "MISMATCH", detail="oracle failed (%s) but C produced a ROM" % ro.stderr.strip(), **rec)
        return res.add(section, name, "ok", detail="both fail", **rec)
    if rc.returncode != 0:
        return res.add(section, name, "C-ERROR", detail=rc.stderr.strip(), rc=rc.returncode, **rec)
    ho, hc = sha(o), sha(c)
    if ho == hc:
        return res.add(section, name, "identical", sha256=ho, **rec)
    d = first_diff(o, c)
    who = attribute(mode, seed, settings, d["first"], difficulty) if d.get("first") is not None else []
    keep = next(lock_ids)
    o.rename(OUT / ("fail_%s_%d_oracle.sfc" % (RUN, keep)))
    c.rename(OUT / ("fail_%s_%d_c.sfc" % (RUN, keep)))
    return res.add(section, name, "MISMATCH", oracle_sha256=ho, c_sha256=hc, diff=d, hacks=who, kept="fail_%s_%d_*.sfc" % (RUN, keep), **rec)


def section_options(res):
    ro = subprocess.run([str(ORACLE), "--mode", "rando", "--list-options"], capture_output=True, text=True).stdout.splitlines()
    ro = [l for l in ro if l.split("=")[0] not in INTERNAL]
    rc = subprocess.run([str(CLI), "--mode", "rando", "--list-options"], capture_output=True, text=True).stdout.splitlines()
    res.add("options", "list-options", "identical" if ro == rc else "MISMATCH", count=len(rc),
            detail="" if ro == rc else {"oracle_only": [l for l in ro if l not in rc], "c_only": [l for l in rc if l not in ro]})


def section_options_mode(res, mode):
    ro = subprocess.run([str(ORACLE), "--mode", mode, "--list-options"], capture_output=True, text=True).stdout.splitlines()
    ro = [l for l in ro if l.split("=")[0] not in INTERNAL]
    rc = subprocess.run([str(CLI), "--mode", mode, "--list-options"], capture_output=True, text=True).stdout.splitlines()
    res.add("options-" + mode, "list-options", "identical" if ro == rc and ro else "MISMATCH", count=len(rc),
            detail="" if ro == rc else {"oracle_only": [l for l in ro if l not in rc], "c_only": [l for l in rc if l not in ro], "same_set_other_order": sorted(ro) == sorted(rc)})


def legal_values(mode="rando"):
    out = []
    for line in subprocess.run([str(CLI), "--mode", mode, "--list-legal"], capture_output=True, text=True).stdout.splitlines():
        m = re.match(r"(\S+) (\S+)=(\S*) \[(.*)\]$", line)
        out.append((m.group(2), m.group(3), m.group(4).split("|")))
    return out


def section_baseline(res, jobs, nseeds):
    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        list(ex.map(lambda s: compare_case(res, "baseline", "seed=%r" % s, "rando", s, dict(BASELINE)), SEEDS[:nseeds]))


def section_matrix(res, jobs, opts):
    jobsl = []
    for key, default, values in opts:
        for v in values:
            for seed in ("1", "424242"):
                st = dict(BASELINE)
                st[key] = v
                jobsl.append((key, v, seed, st))

    def one(j):
        key, v, seed, st = j
        name = "%s=%s seed=%s" % (key, v, seed)
        sup = run_c("rando", seed, st, OUT / "none.sfc", extra=["--supports"])
        if sup.returncode == 0:
            return compare_case(res, "matrix", name, "rando", seed, st)
        c = OUT / ("refused_%s_%d.sfc" % (RUN, next(lock_ids)))
        r = run_c("rando", seed, st, c)
        if r.returncode == 15 and not c.exists() and "unsupported option" in r.stderr:
            return res.add("matrix", name, "refused-ok", message=sup.stdout.strip())
        return res.add("matrix", name, "NOT-REFUSED", detail="rc=%d %s" % (r.returncode, r.stderr.strip()))

    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        list(ex.map(one, jobsl))


def supported_values(res, opts):
    """option -> values that the matrix found supported (from the C's own --supports answers)."""
    sup = {}
    for r in res.cases:
        if r["section"] == "matrix" and r["status"] in ("identical", "ok"):
            m = re.match(r"(\S+)=(\S*) seed", r["name"])
            sup.setdefault(m.group(1), set()).add(m.group(2))
    return {k: sorted(v) for k, v in sup.items()}


def section_random(res, jobs, n, opts, sup):
    rng = random.Random(20261018)
    jl = []
    for i in range(n):
        st = dict(BASELINE)
        for key, default, values in opts:
            if key in sup and rng.random() < 0.5:
                st[key] = rng.choice(sup[key])
        seed = rng.choice(SEEDS[:8] + [str(rng.randrange(1, 10 ** 8)) for _ in range(8)])
        jl.append((i, seed, st))
    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        list(ex.map(lambda j: compare_case(res, "random", "random #%d" % j[0], "rando", j[1], j[2]), jl))


def section_api(res):
    st = dict(BASELINE)
    # progress numbering and totals
    p = run_c("rando", "1", st, OUT / ("api_progress_%s.sfc" % RUN), extra=["--progress"])
    steps = [l for l in p.stdout.splitlines() if l.startswith("progress ")]
    nums = [int(l.split()[1].split("/")[0]) for l in steps]
    totals = {l.split()[1].split("/")[1] for l in steps}
    ok = nums == list(range(1, len(nums) + 1)) and len(totals) == 1 and int(next(iter(totals))) == len(nums) and p.returncode == 0
    res.add("api", "progress steps 1..N, constant total", "ok" if ok else "FAIL", steps=len(nums), total=sorted(totals))
    # cancel: no output file, code 3
    dst = OUT / ("api_cancel_%s.sfc" % RUN)
    r = run_c("rando", "1", st, dst, extra=["--cancel-at", "7"])
    res.add("api", "cancel at step 7 -> ERR_CANCELLED, no file", "ok" if r.returncode == 13 and not dst.exists() else "FAIL", rc=r.returncode)
    # headered ROM equals the plain one
    data = ROM.read_bytes()
    if len(data) == 2 * 1024 * 1024:
        h = OUT / "api_headered.smc"
        h.write_bytes(b"\x00" * 512 + data)
        a, b = OUT / ("api_h_out_%s.sfc" % RUN), OUT / ("api_plain_out_%s.sfc" % RUN)
        run_c("rando", "5", st, a, rom=h)
        run_c("rando", "5", st, b)
        res.add("api", "512 byte copier header accepted", "ok" if a.exists() and b.exists() and sha(a) == sha(b) else "FAIL")
        # bad ROMs
        short = OUT / "api_short.smc"
        short.write_bytes(data[:1024 * 1024])
        r = run_c("rando", "1", st, OUT / ("api_bad1_%s.sfc" % RUN), rom=short)
        res.add("api", "1 MiB file -> ERR_BAD_ROM", "ok" if r.returncode == 14 and "unexpected size" in r.stderr else "FAIL", rc=r.returncode, err=r.stderr.strip())
        for name, off, val, text in (("title", 0xFFC0, 0x58, "Name of game"), ("region", 0xFFD9, 0x00, "US ROM"), ("version", 0xFFDB, 0x01, "version 1.0")):
            bad = bytearray(data)
            bad[off] = val
            f = OUT / ("api_bad_%s.smc" % name)
            f.write_bytes(bytes(bad))
            r = run_c("rando", "1", st, OUT / ("api_bad_%s_%s.sfc" % (name, RUN)), rom=f)
            res.add("api", "bad %s -> ERR_BAD_ROM" % name, "ok" if r.returncode == 14 and text in r.stderr else "FAIL", rc=r.returncode, err=r.stderr.strip())
    r = run_c("rando", "1", st, ROM)
    res.add("api", "src == dst refused", "ok" if r.returncode == 12 else "FAIL", rc=r.returncode)
    r = run_c("rando", "1", st, OUT / ("api_missing_%s.sfc" % RUN), rom=OUT / "does_not_exist.smc")
    res.add("api", "missing source -> ERR_FAILED", "ok" if r.returncode == 12 else "FAIL", rc=r.returncode, err=r.stderr.strip())
    for name, mode, settings in (("open plando", "open", {"plandoSettings": "x"}), ("ancientcave illegal acLength", "ancientcave", {"acLength": "huge"}), ("unknown mode", "nope", {}),
                                 ("unknown option", "rando", {"noSuchOption": "yes"}), ("internal option", "rando", {"mode": "open"}),
                                 ("illegal enum value", "rando", {"rExp": "quadruple"}), ("illegal bool value", "rando", {"rOrbs": "maybe"})):
        d = OUT / ("api_unsupported_%s_%d.sfc" % (RUN, next(lock_ids)))
        r = run_c(mode, "1", settings, d)
        res.add("api", name + " -> ERR_UNSUPPORTED, no file", "ok" if r.returncode == 15 and not d.exists() else "FAIL", rc=r.returncode, err=r.stderr.strip())


def section_presets(res, jobs):
    presets = json.loads((COMPANION / "randomizer/presets.json").read_text())["presets"]
    hacks = {}
    for line in subprocess.run([str(CLI), "--list-hacks"], capture_output=True, text=True).stdout.splitlines():
        m = re.match(r"\d+ (\S+) (\S+)\|(.*)$", line)
        hacks.setdefault(m.group(3), set()).add(m.group(2))
    ported = {n for n, s in hacks.items() if s & {"ported", "partial"}}
    cov = []

    def one(p):
        work = OUT / ("preset_work_%s" % p["id"])
        dst = OUT / ("preset_%s.sfc" % p["id"])
        set_ = {k: v for k, v in p["set"].items()}
        ro = run_oracle(p["mode"], "12345", set_, dst, work, difficulty=p.get("difficulty"))
        applied = set()
        for lg in work.glob("log_*.txt"):
            for l in lg.read_text(errors="replace").splitlines():
                m = re.search(r"Adding: \[(.*?)\]", l)
                if m:
                    applied.add(m.group(1))
        sup = run_c(p["mode"], "12345", set_, OUT / "none.sfc", extra=["--supports"], difficulty=p.get("difficulty"))
        rec = {"id": p["id"], "mode": p["mode"], "oracle_ok": ro.returncode == 0, "oracle_hacks": len(applied),
               "c_hacks_available": len(applied & ported), "missing_hacks": sorted(applied - ported),
               "supported": sup.returncode == 0, "why": sup.stdout.strip()}
        if sup.returncode == 0:
            cmpres = compare_case(res, "presets", p["id"], p["mode"], "12345", set_, p.get("difficulty"))
            rec["identical"] = cmpres["status"] == "identical"
        cov.append(rec)

    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        list(ex.map(one, presets))
    cov.sort(key=lambda r: [p["id"] for p in presets].index(r["id"]))
    res.add("presets", "coverage", "ok", presets=cov)
    return cov


FEATURE_KEYS = ["rWeapons", "magicRebalance", "rainbowRabites", "characterColors", "leavePlayersBehind", "funSpeedup", "statusGlow",
                "elemDamage", "offscreenTarget", "buyMultiple", "adjustEnemyPos", "showMinimap", "rDialogue", "rBosses", "rEnemies",
                "rMusic", "muteMusic", "rAutosave", "rDifficulty", "rHoliday", "aggEnemies"]
PRESET_SEEDS = ["1", "2", "12345", "99999999", "42", "7", "31337", "1000000", "555", "2718281", "8675309", "4242", "100", "999", "65535",
                "hello", "Secret of Mana", "", "\u00e9", "\u65e5\u672c\u8a9e"]


def feature_settings(opts):
    """[(label, {key: value})] for every non-baseline legal value of the phase-2 options."""
    out = []
    for key, default, values in opts:
        if key not in FEATURE_KEYS:
            continue
        for v in values:
            if v == BASELINE.get(key, default) or (key == "rDifficulty" and v == "100"):
                continue
            out.append(("%s=%s" % (key, v), {key: v}))
    return out


def is_supported(settings, seed="1", mode="rando", difficulty=None):
    return run_c(mode, seed, settings, OUT / "none.sfc", extra=["--supports"], difficulty=difficulty).returncode == 0


def check_refused(res, section, name, mode, seed, settings, difficulty=None):
    c = OUT / ("refused_%s_%d.sfc" % (RUN, next(lock_ids)))
    r = run_c(mode, seed, settings, c, difficulty=difficulty)
    if r.returncode == 15 and not c.exists() and "unsupported option" in r.stderr:
        return res.add(section, name, "refused-ok", message=r.stderr.strip()[:200])
    return res.add(section, name, "NOT-REFUSED", detail="rc=%d %s" % (r.returncode, r.stderr.strip()))


def section_features(res, jobs, opts, nrandom):
    feats = feature_settings(opts)
    sup = []
    for label, s in feats:
        st = dict(BASELINE)
        st.update(s)
        if is_supported(st):
            sup.append((label, s))
    jl = []
    for label, s in feats:
        for seed in ("1", "424242", "hello"):
            st = dict(BASELINE)
            st.update(s)
            jl.append(("alone %s seed=%s" % (label, seed), seed, st))
    rng = random.Random(1049)
    by_key = {}
    for label, s in sup:
        by_key.setdefault(next(iter(s)), []).append(s)
    keys = sorted(by_key)
    for i, a in enumerate(keys):
        for b in keys[i + 1:]:
            st = dict(BASELINE)
            st.update(rng.choice(by_key[a]))
            st.update(rng.choice(by_key[b]))
            jl.append(("pair %s+%s" % (a, b), rng.choice(PRESET_SEEDS[:15]), st))
    for i in range(nrandom):
        k = rng.randrange(3, max(4, len(keys) + 1))
        st = dict(BASELINE)
        for key in rng.sample(keys, min(k, len(keys))):
            st.update(rng.choice(by_key[key]))
        for seed in ("5", "888888", "random-%d" % i):
            jl.append(("subset #%d (%d features) seed=%s" % (i, k, seed), seed, st))
    for seed in ("1", "2", "3", "12345", "99999999"):
        st = dict(BASELINE)
        for key in keys:
            st.update(by_key[key][-1])
        jl.append(("all supported seed=%s" % seed, seed, st))

    def one(j):
        name, seed, st = j
        if is_supported(st, seed):
            return compare_case(res, "features", name, "rando", seed, st)
        return check_refused(res, "features", name, "rando", seed, st)

    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        list(ex.map(one, jl))
    res.add("features", "supported features", "ok", features=[l for l, _ in sup], unsupported=[l for l, _ in feats if l not in [x for x, _ in sup]])


def section_preset_seeds(res, jobs, nseeds, cov):
    """Every supported preset over nseeds seeds (oracle and C), with timings; unsupported presets must be refused without a file."""
    presets = json.loads((COMPANION / "randomizer/presets.json").read_text())["presets"]
    if PRESET_MODES:
        presets = [p for p in presets if p["mode"] in PRESET_MODES]
    seeds = PRESET_SEEDS[:nseeds] if nseeds <= len(PRESET_SEEDS) else PRESET_SEEDS + [str(1000 + 7 * i) for i in range(nseeds - len(PRESET_SEEDS))]
    timing = {}
    jl = [(p, s) for p in presets for s in seeds]

    def one(j):
        p, seed = j
        set_ = dict(p["set"])
        if not is_supported(set_, seed, p["mode"], p.get("difficulty")):
            if seed == seeds[0]:
                check_refused(res, "preset-seeds", "%s (unsupported)" % p["id"], p["mode"], seed, set_, p.get("difficulty"))
            return
        tag = slot()
        o, c = OUT / ("po_%d.sfc" % tag), OUT / ("pc_%d.sfc" % tag)
        for f in (o, c):
            if f.exists():
                f.write_bytes(b"")
        t0 = time.time()
        ro = run_oracle(p["mode"], seed, set_, o, OUT / ("work_%d" % tag), difficulty=p.get("difficulty"))
        t1 = time.time()
        rc = run_c(p["mode"], seed, set_, c, difficulty=p.get("difficulty"))
        t2 = time.time()
        name = "%s seed=%r" % (p["id"], seed)
        rec = dict(mode=p["mode"], seed=seed, settings=set_, difficulty=p.get("difficulty"))
        if ro.returncode != 0 or rc.returncode != 0:
            res.add("preset-seeds", name, "C-ERROR" if rc.returncode else "MISMATCH", detail="oracle rc=%d C rc=%d %s" % (ro.returncode, rc.returncode, rc.stderr.strip()), **rec)
            return
        timing.setdefault(p["id"], []).append((t1 - t0, t2 - t1))
        if sha(o) == sha(c):
            res.add("preset-seeds", name, "identical", sha256=sha(o), **rec)
        else:
            d = first_diff(o, c)
            who = attribute(p["mode"], seed, set_, d["first"], p.get("difficulty")) if d.get("first") is not None else []
            keep = next(lock_ids)
            o.rename(OUT / ("fail_%s_%d_oracle.sfc" % (RUN, keep)))
            c.rename(OUT / ("fail_%s_%d_c.sfc" % (RUN, keep)))
            res.add("preset-seeds", name, "MISMATCH", diff=d, hacks=who, kept="fail_%s_%d_*.sfc" % (RUN, keep), **rec)

    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        list(ex.map(one, jl))
    rows = []
    for pid, v in timing.items():
        rows.append({"id": pid, "n": len(v), "oracle_mean_s": sum(a for a, _ in v) / len(v), "c_mean_s": sum(b for _, b in v) / len(v), "c_max_s": max(b for _, b in v)})
    res.add("preset-seeds", "timing (wall clock incl. process start, jobs run in parallel: indicative only)", "ok", timing=rows)


def section_mode(res, jobs, mode, diffs, ncombos):
    """Non-vanilla modes (bossrush, chaos): every option alone at every non-default legal value (2 seeds x difficulties), then random
    combinations; supported cases must equal the oracle, unsupported ones must be refused without a file."""
    opts = [(k, d, v) for k, d, v in legal_values(mode) if k not in ("mode", "version", "buildDate", "currentGenerateProgress")]
    jl = []
    for key, default, values in opts:
        for v in values:
            if v == default:
                continue
            for diff in diffs:
                for seed in ("11", "hello"):
                    jl.append(("%s %s=%s seed=%s %s" % (mode, key, v, seed, diff), seed, {key: v}, diff))
    rng = random.Random(77 + len(mode))
    for i in range(ncombos):
        st = {}
        for key, default, values in opts:
            if rng.random() < 0.3:
                st[key] = rng.choice(values)
        jl.append(("%s combo #%d" % (mode, i), rng.choice(PRESET_SEEDS), st, rng.choice(diffs)))
    for diff in diffs:
        jl.append(("%s defaults %s" % (mode, diff), "12345", {}, diff))

    def one(j):
        name, seed, st, diff = j
        if is_supported(st, seed, mode, diff):
            return compare_case(res, "mode-" + mode, name, mode, seed, st, diff)
        return check_refused(res, "mode-" + mode, name, mode, seed, st, diff)

    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        list(ex.map(one, jl))



# ----------------------------------------------------------------------------------------------------------------------- Open World
OPEN_INTERNAL = ("mode", "version", "buildDate", "currentGenerateProgress", "plandoSettings")


def open_option_values():
    """[(key, default, [non-default values])] of mode open; numeric options get a few representative values on their step grid."""
    out = []
    for line in subprocess.run([str(CLI), "--mode", "open", "--list-legal"], capture_output=True, text=True).stdout.splitlines():
        m = re.match(r"(\S+) (\S+)=(\S*) \[(.*)\]$", line)
        key, default, legal = m.group(2), m.group(3), m.group(4).split("|")
        if key in OPEN_INTERNAL:
            continue
        if len(legal) == 3 and re.match(r"^[0-9.]+$", legal[0] + legal[1] + legal[2]) and key not in ("opMinSeeds", "opMaxSeeds"):
            lo, hi, st = float(legal[0]), float(legal[1]), float(legal[2])
            pts = {lo, hi, lo + st, (lo + hi) / 2 // st * st, float(default) + st, float(default) - st}
            vals = []
            for x in sorted(p for p in pts if lo <= p <= hi):
                t = ("%g" % x)
                if t != default and t not in vals:
                    vals.append(t)
        else:
            vals = [v for v in legal if v != default]
        out.append((key, default, vals))
    return out


def open_cases(res, jobs, cases, section):
    def one(j):
        name, seed, st = j
        if is_supported(st, seed, "open"):
            return compare_case(res, section, name, "open", seed, st)
        return check_refused(res, section, name, "open", seed, st)

    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        list(ex.map(one, cases))


def section_open(res, jobs, nseeds, nrandom, only=None):
    presets = [p for p in json.loads((COMPANION / "randomizer/presets.json").read_text())["presets"] if p["mode"] == "open"]
    opts = open_option_values()
    rng = random.Random(2026101804)
    seeds = PRESET_SEEDS[:nseeds] if nseeds <= len(PRESET_SEEDS) else PRESET_SEEDS + [str(1000 + 7 * i) for i in range(nseeds - len(PRESET_SEEDS))]
    ro = subprocess.run([str(ORACLE), "--mode", "open", "--list-options"], capture_output=True, text=True).stdout.splitlines()
    ro = [l for l in ro if l.split("=")[0] not in INTERNAL]
    rc = subprocess.run([str(CLI), "--mode", "open", "--list-options"], capture_output=True, text=True).stdout.splitlines()
    res.add("open-options", "list-options", "identical" if ro == rc else "MISMATCH", count=len(rc),
            detail="" if ro == rc else {"oracle_only": [l for l in ro if l not in rc], "c_only": [l for l in rc if l not in ro]})
    want = lambda k: only is None or k in only
    if want("open-baseline"):
        open_cases(res, jobs, [("defaults seed=%r" % sd, sd, {}) for sd in seeds], "open-baseline")
    if want("open-matrix"):
        jl = []
        for key, default, vals in opts:
            for v in vals:
                for sd in ("11", "hello"):
                    jl.append(("%s=%s seed=%s" % (key, v, sd), sd, {key: v}))
        open_cases(res, jobs, jl, "open-matrix")
    if want("open-presetflip"):
        jl = []
        for p in presets:
            for key, default, vals in opts:
                for v in vals:
                    if p["set"].get(key, default) == v:
                        continue
                    st = dict(p["set"])
                    st[key] = v
                    jl.append(("%s + %s=%s" % (p["id"], key, v), "7", st))
        open_cases(res, jobs, jl, "open-presetflip")
    if want("open-random"):
        sup = {}
        for r in res.cases:
            if r["section"] == "open-matrix" and r["status"] == "identical":
                k, v = r["name"].split(" ")[0].split("=", 1)
                sup.setdefault(k, set()).add(v)
        pool = [(k, sorted(v)) for k, v in sorted(sup.items())] or [(k, vals) for k, _, vals in opts]
        jl = []
        for i in range(nrandom):
            st = {}
            for k, vals in pool:
                if rng.random() < 0.3:
                    st[k] = rng.choice(vals)
            jl.append(("random #%d" % i, rng.choice(PRESET_SEEDS), st))
        for i in range(nrandom // 3):
            st = {}
            for k, _, vals in opts:
                if rng.random() < 0.3:
                    st[k] = rng.choice(vals)
            jl.append(("unfiltered random #%d" % i, rng.choice(PRESET_SEEDS), st))
        open_cases(res, jobs, jl, "open-random")
    if want("open-pairs"):
        keys = [(k, vals) for k, _, vals in opts if vals]
        jl = []
        for i, (a, va) in enumerate(keys):
            for b, vb in keys[i + 1:]:
                jl.append(("pair %s+%s" % (a, b), rng.choice(PRESET_SEEDS[:15]), {a: rng.choice(va), b: rng.choice(vb)}))
        open_cases(res, jobs, jl, "open-pairs")
    if want("open-api"):
        section_open_api(res)


def section_open_api(res):
    """progress numbering, cancel at several steps and refusals for mode open; a cancelled or refused run leaves no file."""
    st = {}
    pr = run_c("open", "5", st, OUT / "api_open.sfc", extra=["--progress"])
    steps = [l for l in pr.stdout.splitlines() if l.startswith("progress ")]
    ok = pr.returncode == 0 and steps and steps[-1].split()[1].split("/")[0] == steps[-1].split()[1].split("/")[1]
    res.add("open-api", "progress numbering", "ok" if ok else "FAIL", detail="%d steps, rc=%d" % (len(steps), pr.returncode))
    total = int(steps[-1].split()[1].split("/")[1]) if steps else 0
    for at in (1, 20, total // 2, total - 3, total):
        c = OUT / ("api_open_cancel_%d.sfc" % at)
        if c.exists():
            c.write_bytes(b"")
        r = run_c("open", "5", st, c, extra=["--cancel-at", str(at)])
        gone = (not c.exists()) or c.stat().st_size == 0
        res.add("open-api", "cancel at step %d" % at, "ok" if r.returncode == 13 and gone else "FAIL", detail="rc=%d" % r.returncode)

# ----------------------------------------------------------------------------------------------------------------------- Ancient Cave
AC_MODE_KEYS = ("acBoy", "acGirl", "acSprite", "acFilter", "acMusic", "acLength", "BossFrequency", "acDialogue", "acDifficulty", "acBiomeTypes")
AC_DIFFS = ["casual", "hard", "reallyhard"]


def ac_options():
    """[(key, default, [non-default legal values])] for the options of mode ancientcave (common ones included)."""
    out = []
    for k, d, v in legal_values("ancientcave"):
        if k in ("mode", "version", "buildDate", "currentGenerateProgress"):
            continue
        vals = [x for x in v if x != d and not (k == "acBiomeTypes" and x == "test" and AC_BIOMES != "test")]
        out.append((k, d, vals))
    return out


def ac_case(res, jobs, cases, section="ac"):
    """cases: [(name, seed, settings, difficulty)]: supported ones are compared with the oracle, the others must be refused without a file."""
    def one(j):
        name, seed, st, diff = j
        if is_supported(st, seed, "ancientcave", diff):
            return compare_case(res, section, name, "ancientcave", seed, st, diff)
        return check_refused(res, section, name, "ancientcave", seed, st, diff)

    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        list(ex.map(one, cases))


def section_ac_baseline(res, jobs, nseeds):
    cases = []
    for i, seed in enumerate(SEEDS[:nseeds] if nseeds <= len(SEEDS) else SEEDS + [str(5000 + 17 * k) for k in range(nseeds - len(SEEDS))]):
        length = ("medium", "short", "long")[i % 3]
        diff = AC_DIFFS[(i // 3) % 3]
        cases.append(("baseline seed=%r %s %s" % (seed, length, diff), seed, {"acLength": length}, diff))
    ac_case(res, jobs, cases, "ac-baseline")


def section_ac_matrix(res, jobs, opts, seeds=("11", "hello"), diffs=AC_DIFFS):
    cases = []
    for key, default, values in opts:
        for v in values:
            for diff in diffs:
                for seed in seeds:
                    cases.append(("%s=%s seed=%s %s" % (key, v, seed, diff), seed, {key: v}, diff))
    for diff in diffs:
        cases.append(("defaults %s" % diff, "12345", {}, diff))
    ac_case(res, jobs, cases, "ac-matrix")


def ac_supported_options(opts):
    """Options reduced to the values that are supported on their own (so the random families spend their time comparing, not refusing)."""
    out = []
    for key, default, values in opts:
        out.append((key, default, [v for v in values if is_supported({key: v}, "1", "ancientcave", "casual")]))
    return out


def section_ac_pairs(res, jobs, opts, n_random):
    opts = ac_supported_options(opts)
    rng = random.Random(4949)
    ac_opts = [o for o in opts if o[0] in AC_MODE_KEYS and o[2]]
    cases = []
    for i, (ka, da, va) in enumerate(ac_opts):
        for kb, db, vb in ac_opts[i + 1:]:
            cases.append(("pair %s+%s" % (ka, kb), rng.choice(PRESET_SEEDS[:15]), {ka: rng.choice(va), kb: rng.choice(vb)}, rng.choice(AC_DIFFS)))
    allo = [o for o in opts if o[2]]
    for i in range(n_random):
        (ka, da, va), (kb, db, vb) = rng.sample(allo, 2)
        cases.append(("random pair #%d %s+%s" % (i, ka, kb), rng.choice(PRESET_SEEDS[:15]), {ka: rng.choice(va), kb: rng.choice(vb)}, rng.choice(AC_DIFFS)))
    ac_case(res, jobs, cases, "ac-pairs")


def section_ac_random(res, jobs, opts, n):
    opts = ac_supported_options(opts)
    rng = random.Random(20261019)
    cases = []
    for i in range(n):
        st = {}
        p = rng.choice((0.1, 0.3, 0.5))
        for key, default, values in opts:
            if values and rng.random() < p:
                st[key] = rng.choice(values)
        cases.append(("random subset #%d (%d options)" % (i, len(st)), rng.choice(PRESET_SEEDS + [str(rng.randrange(1, 10 ** 8))]), st, rng.choice(AC_DIFFS)))
    ac_case(res, jobs, cases, "ac-random")


def progress_steps(mode, seed, st, difficulty=None, extra=()):
    r = run_c(mode, seed, st, OUT / ("ac_progress_%s_%d.sfc" % (RUN, next(lock_ids))), extra=["--progress"] + list(extra), difficulty=difficulty)
    steps = [l for l in r.stdout.splitlines() if l.startswith("progress ")]
    return r, steps


def section_ac_api(res):
    # progress numbering: 65 common hacks + 20 Ancient Cave hacks + 3 post-processing steps + one step per floor
    for length, floors in (("short", 8), ("medium", 16), ("long", 24)):
        st = {"acLength": length}
        if not is_supported(st, "1", "ancientcave", "casual"):
            res.add("ac-api", "progress %s" % length, "refused-ok", detail="not supported yet")
            continue
        r, steps = progress_steps("ancientcave", "1", st, "casual")
        nums = [int(l.split()[1].split("/")[0]) for l in steps]
        totals = {l.split()[1].split("/")[1] for l in steps}
        want = 65 + 20 + 3 + floors
        ok = (r.returncode == 0 and nums == list(range(1, len(nums) + 1)) and totals == {str(len(nums))} and len(nums) == want
              and sum(1 for l in steps if "Ancient Cave floor" in l) == floors)
        res.add("ac-api", "progress %s: steps 1..%d, constant total, %d floor steps" % (length, want, floors), "ok" if ok else "FAIL", steps=len(nums), total=sorted(totals))
        # cancel before the first floor, in the middle of the floors and at the very last step: ERR_CANCELLED, no file, no temporary file
        for at in (1, 65 + 10 + 1, 65 + 10 + floors // 2, want - 1, want):
            dst = OUT / ("ac_cancel_%s_%d.sfc" % (RUN, next(lock_ids)))
            c = run_c("ancientcave", "1", st, dst, extra=["--cancel-at", str(at)], difficulty="casual")
            leftovers = [x.name for x in OUT.glob(dst.name + "*")]
            res.add("ac-api", "cancel at step %d of %d (%s) -> ERR_CANCELLED, nothing written" % (at, want, length),
                    "ok" if c.returncode == 13 and not leftovers else "FAIL", rc=c.returncode, leftovers=leftovers)
    # a cancelled or failed generation leaves an existing destination untouched
    st = {"acLength": "short"}
    if is_supported(st, "1", "ancientcave", "casual"):
        dst = OUT / ("ac_keep_%s.sfc" % RUN)
        dst.write_bytes(b"previous content")
        c = run_c("ancientcave", "1", st, dst, extra=["--cancel-at", "90"], difficulty="casual")
        res.add("ac-api", "cancelled run keeps the existing destination", "ok" if c.returncode == 13 and dst.read_bytes() == b"previous content" else "FAIL")
        dst2 = OUT / ("ac_replace_%s.sfc" % RUN)
        dst2.write_bytes(b"previous content")
        c = run_c("ancientcave", "1", st, dst2, difficulty="casual")
        res.add("ac-api", "successful run replaces the destination atomically", "ok" if c.returncode == 0 and dst2.stat().st_size == 4 * 1024 * 1024
                and not list(OUT.glob(dst2.name + ".part")) else "FAIL")
    # refusals happen up front: rc 15, no file, no temporary file, fast
    refused = [("difficulty custom", {}, "custom"), ("difficulty Casual", {}, "Casual"), ("difficulty nope", {}, "nope"),
               ("illegal acLength", {"acLength": "huge"}, "casual"), ("empty biome list", {"acBiomeTypes": ""}, "casual"),
               ("unknown biome", {"acBiomeTypes": "swamp"}, "casual"), ("biome list with a space", {"acBiomeTypes": "forest, ruins"}, "casual"),
               ("characterColors custom", {"characterColors": "custom"}, "casual"),
               ("rando option in ancientcave", {"rEnemies": "yes"}, "casual")]
    for name, st, diff in refused:
        dst = OUT / ("ac_refused_%s_%d.sfc" % (RUN, next(lock_ids)))
        t0 = time.time()
        r = run_c("ancientcave", "1", st, dst, difficulty=diff)
        dt = time.time() - t0
        leftovers = [x.name for x in OUT.glob(dst.name + "*")]
        res.add("ac-api", "refused up front: %s" % name, "ok" if r.returncode == 15 and not leftovers and dt < 1.0 and "unsupported option" in r.stderr else "FAIL",
                rc=r.returncode, seconds=round(dt, 3), leftovers=leftovers, err=r.stderr.strip()[:160])
    # supports() agrees with generate(): refused <=> rc 15, supported => rc 0 (checked over a mixed list of requests)
    rng = random.Random(77)
    opts = ac_options()
    for i in range(12):
        st = {}
        for key, default, values in opts:
            if values and rng.random() < 0.15:
                st[key] = rng.choice(values)
        diff = rng.choice(AC_DIFFS + ["custom"])
        sup = run_c("ancientcave", "3", st, OUT / "none.sfc", extra=["--supports"], difficulty=diff)
        dst = OUT / ("ac_cons_%s_%d.sfc" % (RUN, next(lock_ids)))
        gen = run_c("ancientcave", "3", st, dst, difficulty=diff)
        ok = (sup.returncode == 0 and gen.returncode == 0 and dst.exists()) or (sup.returncode != 0 and gen.returncode == 15 and not dst.exists())
        res.add("ac-api", "supports/generate consistent #%d" % i, "ok" if ok else "FAIL", supports=sup.returncode, generate=gen.returncode, settings=st, difficulty=diff)
    # unwritable destination is reported before any work
    if is_supported({}, "1", "ancientcave", "casual"):
        r = run_c("ancientcave", "1", {}, OUT / "no_such_dir" / "x.sfc", difficulty="casual")
        res.add("ac-api", "unwritable destination -> ERR_FAILED", "ok" if r.returncode == 12 and "destination" in r.stderr else "FAIL", rc=r.returncode, err=r.stderr.strip())


def section_ac(res, a, only=None):
    """All Ancient Cave sections (names: ac-options ac-baseline ac-matrix ac-pairs ac-random ac-api)."""
    def want(n):
        return only is None or n in only or "ac" in only

    opts = ac_options()
    if want("ac-options"):
        section_options_mode(res, "ancientcave")
    if want("ac-baseline"):
        section_ac_baseline(res, a.jobs, a.ac_seeds)
    if want("ac-matrix"):
        section_ac_matrix(res, a.jobs, opts, seeds=("11", "hello")[:a.ac_matrix_seeds], diffs=AC_DIFFS[:a.ac_matrix_diffs])
    if want("ac-pairs"):
        section_ac_pairs(res, a.jobs, opts, a.ac_random)
    if want("ac-random"):
        section_ac_random(res, a.jobs, opts, a.ac_random)
    if want("ac-api"):
        section_ac_api(res)

def verify_hashes(path, jobs):
    """C only: re-generate every recorded configuration and compare with the recorded SHA-256 (no oracle, no dotnet needed)."""
    rows = json.loads(pathlib.Path(path).read_text())["cases"]
    bad = []

    def one(r):
        tag = slot()
        c = OUT / ("vh_%d.sfc" % tag)
        if c.exists():
            c.write_bytes(b"")
        run_c(r["mode"], r["seed"], r["settings"], c, difficulty=r.get("difficulty"), env={"SOMRANDO_FAKE_DATE": "10-18"})   # Nesoberi picks by calendar date
        if not c.exists() or c.stat().st_size == 0 or sha(c) != r["sha256"]:
            bad.append(r["name"])

    with concurrent.futures.ThreadPoolExecutor(jobs) as ex:
        list(ex.map(one, rows))
    print("verify-hashes: %d recorded, %d differ %s" % (len(rows), len(bad), bad[:10]))
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seeds", type=int, default=len(SEEDS))
    ap.add_argument("--random", type=int, default=150)
    ap.add_argument("--jobs", type=int, default=6)
    ap.add_argument("--report", default=str(OUT / "report.json"))
    ap.add_argument("--hashes", default=None, help="write config + SHA-256 pairs (no ROM bytes) to this JSON file")
    ap.add_argument("--skip-matrix", action="store_true")
    ap.add_argument("--preset-seeds", type=int, default=20, help="seeds per supported preset in the preset-seeds section")
    ap.add_argument("--feature-subsets", type=int, default=30, help="random feature subsets in the features section")
    ap.add_argument("--only", default=None, help="comma separated sections to run: features,preset-seeds,presets,mode-bossrush,mode-chaos,ac,ac-options,ac-baseline,ac-matrix,ac-pairs,ac-random,ac-api,open,open-baseline,open-matrix,open-presetflip,open-random,open-pairs,open-api (skips the rest)")
    ap.add_argument("--ac-biomes", default=None, help="acBiomeTypes override for every ancientcave case (development: 'test', with --oracle .../oracle-dbg/bin/somrando)")
    ap.add_argument("--oracle", default=None, help="path of the C# generator (default: SOMRANDO_ORACLE or the release build)")
    ap.add_argument("--open-seeds", type=int, default=12, help="seeds of the Open World baseline")
    ap.add_argument("--open-random", type=int, default=60, help="random option combinations of the Open World family")
    ap.add_argument("--ac-seeds", type=int, default=12, help="seeds of the Ancient Cave baseline")
    ap.add_argument("--ac-matrix-seeds", type=int, default=2, help="seeds per option value in the Ancient Cave matrix (1 or 2)")
    ap.add_argument("--ac-matrix-diffs", type=int, default=3, help="difficulties per option value in the Ancient Cave matrix (1 to 3)")
    ap.add_argument("--ac-random", type=int, default=30, help="random pairs and random subsets of the Ancient Cave family")
    ap.add_argument("--preset-hashes", default=None, help="write the SHA-256 of every identical preset-seeds case (the regression file tests/regression_hashes.json)")
    ap.add_argument("--verify-hashes", default=None, help="C only: check recorded SHA-256 values (see --hashes) and exit")
    ap.add_argument("--preset-modes", default=None, help="comma separated modes: restrict the preset-seeds section (e.g. open)")
    a = ap.parse_args()
    global ORACLE, AC_BIOMES, PRESET_MODES
    PRESET_MODES = a.preset_modes.split(",") if a.preset_modes else None
    if a.oracle:
        ORACLE = pathlib.Path(a.oracle)
    AC_BIOMES = a.ac_biomes
    OUT.mkdir(parents=True, exist_ok=True)
    for need in (ORACLE, CLI, ROM):
        if not need.exists():
            print("missing: %s" % need)
            return 2
    if a.verify_hashes:
        return verify_hashes(a.verify_hashes, a.jobs)
    res = Results()
    opts = legal_values()
    only = set(a.only.split(",")) if a.only else None
    if only:
        if "features" in only:
            section_features(res, a.jobs, opts, a.feature_subsets)
        for m in ("bossrush", "chaos"):
            if "mode-" + m in only:
                section_mode(res, a.jobs, m, ["casual", "hard", "reallyhard"], a.feature_subsets * 4)
        if any(x == "ac" or x.startswith("ac-") for x in only):
            section_ac(res, a, only)
        oo = {x for x in only if x.startswith("open-")}
        if oo or "open" in only:
            section_open(res, a.jobs, a.open_seeds, a.open_random, None if "open" in only else oo)
        cov = section_presets(res, a.jobs) if "presets" in only else []
        if "preset-seeds" in only:
            section_preset_seeds(res, a.jobs, a.preset_seeds, None)
        return finish(res, a, cov)
    section_options(res)
    section_baseline(res, a.jobs, a.seeds)
    if not a.skip_matrix:
        section_matrix(res, a.jobs, opts)
        sup = supported_values(res, opts)
        section_random(res, a.jobs, a.random, opts, sup)
    section_api(res)
    section_features(res, a.jobs, opts, a.feature_subsets)
    for m in ("bossrush", "chaos"):
        section_mode(res, a.jobs, m, ["casual", "hard", "reallyhard"], a.feature_subsets * 4)
    section_ac(res, a)
    section_open(res, a.jobs, a.open_seeds, a.open_random)
    cov = section_presets(res, a.jobs)
    section_preset_seeds(res, a.jobs, a.preset_seeds, cov)
    return finish(res, a, cov)


def finish(res, a, cov):
    by = {}
    for r in res.cases:
        by.setdefault(r["section"], {}).setdefault(r["status"], 0)
        by[r["section"]][r["status"]] += 1
    print("sections:", json.dumps(by))
    for r in res.fail:
        print("FAIL", r["section"], r["name"], json.dumps({k: v for k, v in r.items() if k not in ("section", "name", "settings")})[:600])
    for r in res.cases:
        if r["name"].startswith("timing"):
            for row in r["timing"]:
                print("  time %-22s n=%-3d oracle %.3fs  C %.3fs (max %.3fs)" % (row["id"], row["n"], row["oracle_mean_s"], row["c_mean_s"], row["c_max_s"]))
    print("presets:")
    for r in cov:
        print("  %-22s supported=%-5s oracle hacks=%-3d available in C=%-3d  %s" % (r["id"], r["supported"], r["oracle_hacks"], r["c_hacks_available"], r["why"][:90]))
    pathlib.Path(a.report).write_text(json.dumps({"cases": res.cases, "summary": by}, indent=1))
    if a.hashes:
        rows = [{"section": r["section"], "name": r["name"], "mode": r["mode"], "seed": r["seed"], "settings": r["settings"], "sha256": r["sha256"]}
                for r in res.cases if r["status"] == "identical" and r["section"] in ("baseline", "random", "matrix", "presets", "features", "preset-seeds")]
        pathlib.Path(a.hashes).write_text(json.dumps({"generator": "1.49", "note": "SHA-256 of the 4 MiB output of the C# generator, equal to libsomrando's; no ROM data",
                                                      "cases": rows}, indent=0, sort_keys=True) + "\n")
    if a.preset_hashes:
        rows = [{"name": r["name"], "mode": r["mode"], "seed": r["seed"], "settings": r["settings"], "difficulty": r.get("difficulty"), "sha256": r["sha256"]}
                for r in res.cases if r["status"] == "identical" and r["section"] == "preset-seeds" and not (AC_BIOMES and r["mode"] == "ancientcave")]
        pathlib.Path(a.preset_hashes).write_text(json.dumps({"generator": "1.49", "note": "SHA-256 of the 4 MiB ROM from the unmodified US v1.0 ROM; equal for the C# generator and libsomrando; no ROM data",
                                                             "cases": rows}, indent=0, sort_keys=True) + "\n")
    print("failures: %d of %d cases; report %s" % (len(res.fail), len(res.cases), a.report))
    return 1 if res.fail else 0


if __name__ == "__main__":
    sys.exit(main())
