#!/usr/bin/env python3
"""Floor-level check of the Ancient Cave generators against the instrumented C# oracle.

  ac_check.py --biome cave --seeds 1,2,3 [--length long] [--difficulty casual] [--keep]

For every seed the instrumented oracle (a build of the upstream sources with dump hooks, see docs) generates an Ancient Cave ROM with only
the given biome enabled, and writes one line per generated map: layer hashes, header, objects, triggers, door dictionaries, RNG draw count.
Each floor is then replayed by tests/ac_replay.c from the RNG state the oracle dumped before it, and the C lines are compared with the oracle
lines.  The first differing field of the first differing line is printed.  Original work of this repository (LGPL-2.1+).
"""
import argparse
import concurrent.futures
import os
import pathlib
import subprocess
import sys

STATE = pathlib.Path(os.environ.get("SOMRANDO_PORT_STATE", "/home/cidwel/.local/state/som-decomp/randoport-20261018"))
DBG = pathlib.Path(os.environ.get("SOMRANDO_ORACLE_DBG", "/home/cidwel/.local/state/som-decomp/randoport-20261018/oracle-dbg/bin/somrando"))
REPLAY = pathlib.Path(os.environ.get("SOMRANDO_REPLAY", str(pathlib.Path(os.environ.get("SOMRANDO_BUILD", STATE / "build")) / "lib/ac_replay")))
ROM = os.environ.get("SOM_ROM", "/home/cidwel/projects/rommancer/roms/snes/Secret of Mana (USA).smc")
SKIP = ("state ", "statec ", "work ")
TYPES = {"test": "Test", "forest": "Forest", "ruins": "Ruins", "island": "Island", "cave": "Cave", "manafortinterior": "Manafort Interior"}


def floors(lines):
    """Splits an oracle dump into per-floor line lists."""
    out, cur = [], None
    for ln in lines:
        if ln.startswith("floor "):
            cur = []
            out.append(cur)
        if cur is not None:
            cur.append(ln)
    return out


def first_diff(a, b):
    ta, tb = a.split(" "), b.split(" ")
    for i in range(max(len(ta), len(tb))):
        x = ta[i] if i < len(ta) else "<none>"
        y = tb[i] if i < len(tb) else "<none>"
        if x != y:
            def cut(s):
                return s if len(s) < 300 else s[:300] + "..."
            return "field %d: oracle %s | C %s" % (i, cut(x), cut(y))
    return "identical tokens"


def run_oracle(seed, biome, length, difficulty, tmp, extra):
    dump = tmp / ("oracle_%s_%s.txt" % (biome, seed))
    dump.write_text("")
    env = dict(os.environ, SOM_AC_DUMP=str(dump), DOTNET_CLI_TELEMETRY_OPTOUT="1", DOTNET_NOLOGO="1")
    cmd = [str(DBG), "--mode", "ancientcave", "--seed", seed, "--src", ROM, "--dst", str(tmp / ("o_%s_%s.sfc" % (biome, seed))),
           "--workdir", str(tmp / "w"), "--difficulty", difficulty, "--set", "acBiomeTypes=" + biome, "--set", "acLength=" + length] + extra
    r = subprocess.run(cmd, env=env, capture_output=True, text=True)
    if "done" not in r.stdout:
        return None, (r.stdout + r.stderr).strip()[-300:]
    return dump.read_text().splitlines(), None


def check_floor(args):
    seed, biome, fl, idx, tmp, dump_path = args
    cdump = tmp / ("c_%s_%s_%d.txt" % (biome, seed, idx))
    cdump.write_text("")
    r = subprocess.run([str(REPLAY), ROM, str(dump_path), str(idx), str(cdump)], capture_output=True, text=True)
    cl = cdump.read_text().splitlines()
    want = [x for x in fl if not x.startswith(SKIP)]
    if r.returncode != 0:
        return seed, idx, "C failed: " + (r.stderr.strip() or "?")[-200:]
    for i in range(max(len(want), len(cl))):
        a = want[i] if i < len(want) else "<none>"
        b = cl[i] if i < len(cl) else "<none>"
        if a != b:
            return seed, idx, "line %d (%s): %s" % (i, a.split(" ")[0], first_diff(a, b))
    return seed, idx, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--biome", required=True, choices=sorted(TYPES))
    ap.add_argument("--seeds", default="1,2,3")
    ap.add_argument("--length", default="long")
    ap.add_argument("--difficulty", default="casual")
    ap.add_argument("--jobs", type=int, default=4)
    ap.add_argument("--set", action="append", default=[], help="extra key=value passed to the oracle")
    ap.add_argument("--first-only", action="store_true", help="stop at the first failing floor of each seed")
    a = ap.parse_args()
    tmp = pathlib.Path(os.environ.get("TMPDIR", STATE)) / "ac_check"
    tmp.mkdir(parents=True, exist_ok=True)
    (tmp / "w").mkdir(exist_ok=True)
    extra = []
    for kv in a.set:
        extra += ["--set", kv]
    total = bad = 0
    for seed in a.seeds.split(","):
        lines, err = run_oracle(seed, a.biome, a.length, a.difficulty, tmp, extra)
        if lines is None:
            print("seed %s: oracle failed: %s" % (seed, err))
            bad += 1
            continue
        fls = floors(lines)
        dump_path = tmp / ("oracle_%s_%s.txt" % (a.biome, seed))
        jobs = [(seed, a.biome, fl, i, tmp, dump_path) for i, fl in enumerate(fls)]
        with concurrent.futures.ThreadPoolExecutor(a.jobs) as ex:
            results = list(ex.map(check_floor, jobs))
        nbad = 0
        for s, i, msg in results:
            total += 1
            if msg:
                nbad += 1
                bad += 1
                if nbad <= 3:
                    print("seed %s floor %d: %s" % (s, i, msg))
        print("seed %s: %d floors, %d differ" % (seed, len(fls), nbad))
    print("TOTAL floors %d, differing %d" % (total, bad))
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
