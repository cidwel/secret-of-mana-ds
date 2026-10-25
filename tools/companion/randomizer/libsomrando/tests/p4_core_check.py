#!/usr/bin/env python3
"""Open World search check: runs the instrumented oracle (SOM_CORE_DUMP hooks in OpenWorldRandomizer) and tests/p4_core_replay on the state the
oracle dumped at the entry of the search, then compares the two dumps line by line (prizes, locations, placements in Dictionary order, score,
collection cycles, iteration count, functional random state before and after).  Original work of this repository (LGPL-2.1+).

  p4_core_check.py --seeds a,b,c [--set k=v]... [--tag T]
"""
import argparse
import os
import pathlib
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor

STATE = pathlib.Path(os.environ.get("SOMRANDO_PORT_STATE", "."))
ORACLE_DLL = os.environ.get("SOMRANDO_ORACLE_CORE", str(STATE.parent / "oracle/out/somrando.dll"))
REPLAY = os.environ.get("SOMRANDO_REPLAY", str(STATE / "build/lib/p4_core_replay"))
ROM = os.environ.get("SOM_ROM", "/home/cidwel/projects/rommancer/roms/snes/Secret of Mana (USA).smc")
OUT = STATE / "t"


def one(seed, sets, tag):
    OUT.mkdir(parents=True, exist_ok=True)
    safe = "".join(ch if ch.isalnum() else "_" for ch in seed) or "empty"
    base = "%s_%s" % (tag, safe)
    od, cd = OUT / (base + ".o.dump"), OUT / (base + ".c.dump")
    od.write_text("")
    work = OUT / ("wd_" + base)
    work.mkdir(exist_ok=True)
    sa = []
    for s in sets:
        sa += ["--set", s]
    env = dict(os.environ, SOM_CORE_DUMP=str(od), DOTNET_CLI_TELEMETRY_OPTOUT="1")
    ro = subprocess.run(["dotnet", ORACLE_DLL, "--mode", "open", "--seed", seed, "--src", ROM, "--dst", str(OUT / (base + ".sfc")), "--workdir", str(work)] + sa,
                        capture_output=True, text=True, env=env)
    otext = od.read_text()
    if "rf-start" not in otext:
        return seed, "oracle failed before the search (rc=%d): %s" % (ro.returncode, (ro.stdout + ro.stderr).strip()[-200:]), False
    rc = subprocess.run([REPLAY, str(od), str(cd)] + sets, capture_output=True, text=True)
    if "iter " not in otext:
        # the C# threw "Open world generation failed!": the C must fail the same way
        ok = rc.returncode != 0 and "Open world generation failed!" in rc.stdout + rc.stderr
        return seed, "oracle search found no solution; c %s" % ("fails too" if ok else "DID NOT FAIL: " + (rc.stdout + rc.stderr)[-200:]), ok
    # the injector that follows the search cannot run on the replayed state: only a failure before the final dump counts
    if rc.returncode != 0 and "rf-end" not in (cd.read_text() if cd.exists() else ""):
        return seed, "c failed: " + (rc.stdout + rc.stderr).strip()[-300:], False
    o = [l for l in otext.splitlines() if not l.startswith("work ") and l]
    c = [l for l in cd.read_text().splitlines() if l]
    for i in range(max(len(o), len(c))):
        a = o[i] if i < len(o) else "<missing>"
        b = c[i] if i < len(c) else "<missing>"
        if a != b:
            return seed, "line %d differs\n  oracle: %s\n  c     : %s" % (i + 1, a[:400], b[:400]), False
    return seed, "identical (%d lines)" % len(o), True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seeds", default="12345")
    ap.add_argument("--set", action="append", default=[])
    ap.add_argument("--tag", default="x")
    ap.add_argument("--jobs", type=int, default=4)
    a = ap.parse_args()
    seeds = a.seeds.split(",") if a.seeds != "" else [""]
    bad = 0
    with ThreadPoolExecutor(a.jobs) as ex:
        for seed, msg, ok in ex.map(lambda s: one(s, a.set, a.tag), seeds):
            print("%-14s %s" % (repr(seed), msg))
            bad += 0 if ok else 1
    sys.exit(1 if bad else 0)


main()
