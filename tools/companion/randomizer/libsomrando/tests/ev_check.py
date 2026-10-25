#!/usr/bin/env python3
"""Compares the C event maker (ev_replay) with the instrumented oracle for many settings/seed combinations.

usage: ev_check.py --oracle BIN --replay BIN --rom ROM --work DIR [--seeds a,b,c] [--random N] [--full] [--jobs 4]
Original work of this repository (LGPL-2.1+).
"""
import argparse, itertools, os, random, subprocess, sys, threading
from concurrent.futures import ThreadPoolExecutor

LENGTHS = ["short", "medium", "long"]
BOSS = ["every", "everyfew", "final"]
DIALOGUE = ["mitch", "demetri"]
FILTER = ["no", "yes"]
CHARS = list(itertools.product(["yes", "no"], repeat=3))
DIFF = ["casual", "hard", "reallyhard"]


def run_one(args, idx, seed, combo):
    length, boss, dlg, flt, (boy, girl, sprite), diff = combo
    sets = ["acBiomeTypes=test", "acLength=" + length, "BossFrequency=" + boss, "acDialogue=" + dlg, "acFilter=" + flt,
            "acBoy=" + boy, "acGirl=" + girl, "acSprite=" + sprite]
    wd = os.path.join(args.work, "t%d" % threading.get_ident())
    os.makedirs(wd, exist_ok=True)
    od, cd = os.path.join(wd, "o.txt"), os.path.join(wd, "c.txt")
    for p in (od, cd):
        if os.path.exists(p):
            os.replace(p, p + ".old")
    cmd = [args.oracle, "--mode", "ancientcave", "--seed", seed, "--src", args.rom, "--dst", os.path.join(wd, "o.smc"), "--workdir", wd,
           "--difficulty", diff]
    for s in sets:
        cmd += ["--set", s]
    env = dict(os.environ, SOM_AC_DUMP=od)
    r = subprocess.run(cmd, env=env, capture_output=True, text=True, cwd=wd)
    desc = "seed=%r %s" % (seed, " ".join(sets[1:] + ["diff=" + diff]))
    if not os.path.exists(od):
        return desc, "oracle failed: " + (r.stderr or r.stdout)[-300:]
    r2 = subprocess.run([args.replay, od, cd, diff] + sets[1:], capture_output=True, text=True)
    if r2.returncode != 0:
        return desc, "replay failed: " + r2.stderr[-300:]
    keep = lambda l: l.startswith(("ev ", "npc ", "evrng", "evrom"))
    ol = [l.rstrip("\n") for l in open(od) if keep(l)]
    cl = [l.rstrip("\n") for l in open(cd) if keep(l)]
    ow = sorted(l.rstrip("\n") for l in open(od) if l.startswith("wd "))
    cw = sorted(l.rstrip("\n") for l in open(cd) if l.startswith("wd "))
    if ol != cl:
        for i, (a, b) in enumerate(itertools.zip_longest(ol, cl)):
            if a != b:
                return desc, "line %d differs:\n  oracle: %s\n  C:      %s" % (i, (a or "<none>")[:400], (b or "<none>")[:400])
    if ow != cw:
        for a, b in itertools.zip_longest(ow, cw):
            if a != b:
                return desc, "workingData differs:\n  oracle: %s\n  C:      %s" % (a, b)
    return desc, None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--oracle", required=True)
    ap.add_argument("--replay", required=True)
    ap.add_argument("--rom", required=True)
    ap.add_argument("--work", required=True)
    ap.add_argument("--seeds", default="1,2,3")
    ap.add_argument("--random", type=int, default=0)
    ap.add_argument("--full", action="store_true")
    ap.add_argument("--jobs", type=int, default=4)
    args = ap.parse_args()
    seeds = args.seeds.split(",")
    allc = list(itertools.product(LENGTHS, BOSS, DIALOGUE, FILTER, CHARS, DIFF))
    jobs = []
    if args.full:
        jobs += [(seeds[0], c) for c in allc]
    rnd = random.Random(12345)
    for _ in range(args.random):
        jobs.append((rnd.choice(seeds), rnd.choice(allc)))
    bad = 0
    with ThreadPoolExecutor(args.jobs) as ex:
        futs = [ex.submit(run_one, args, i, s, c) for i, (s, c) in enumerate(jobs)]
    for f in futs:
        desc, err = f.result()
        if err:
            bad += 1
            print("DIFF", desc, "\n", err)
    print("%d runs, %d differing" % (len(jobs), bad))
    sys.exit(1 if bad else 0)


if __name__ == "__main__":
    main()
