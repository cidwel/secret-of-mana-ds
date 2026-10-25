#!/usr/bin/env python3
"""Per-hack comparison of the C port against the instrumented oracle (private copy with SOM_HACK_LOG hooks, see tests/README in the port notes).

  hack_cmp.py --mode open --seed 12345 [--set k=v ...] [--difficulty d] [--dump]

Both sides write one line per hack: index, name, success, workingOffset, FNV-1a of the ROM, hash of the functional and cosmetic random
states.  The first differing line names the hack that diverged (ROM, offset or random draws).  With --dump the ROM after the hack, from
both sides, is compared and the first differing ranges are printed.  Original work of this repository (LGPL-2.1+).
"""
import argparse
import os
import pathlib
import subprocess
import sys

STATE = pathlib.Path(os.environ.get("SOMRANDO_PORT_STATE", "/home/cidwel/.local/state/som-decomp/randoport-20261018"))
ORACLE_DLL = pathlib.Path(os.environ.get("SOMRANDO_ORACLE_HD", "/home/cidwel/.local/state/som-decomp/randoport-20261018/oracle-hd/bin/somrando.dll"))
CLI = pathlib.Path(os.environ.get("SOMRANDO_CLI", str(STATE / "build/lib/sr_cli")))
ROM = os.environ.get("SOM_ROM", "/home/cidwel/projects/rommancer/roms/snes/Secret of Mana (USA).smc")
OUT = pathlib.Path(os.environ.get("SOMRANDO_HC_OUT", str(STATE / "hc")))


SKIP = ""


def common_names():
    out = subprocess.run([str(CLI), "--list-hacks"], capture_output=True, text=True).stdout.splitlines()
    return [l.split(" ", 2)[2].split("|", 1)[1] for l in out if l.split(" ", 2)[1] == "common"]


def run(mode, seed, sets, diff, dump_idx=None, tag="x"):
    OUT.mkdir(parents=True, exist_ok=True)
    work = OUT / ("wd_" + tag)
    work.mkdir(exist_ok=True)
    lo, lc = OUT / ("o_%s.log" % tag), OUT / ("c_%s.log" % tag)
    for p in (lo, lc):
        if p.exists():
            p.write_text("")
    eo = dict(os.environ, SOM_HACK_LOG=str(lo), DOTNET_CLI_TELEMETRY_OPTOUT="1")
    ec = dict(os.environ, SOMRANDO_HACKLOG=str(lc))
    if dump_idx is not None:
        eo.update(SOM_HACK_DUMP_DIR=str(OUT), SOM_HACK_DUMP_IDX=str(dump_idx))
        ec.update(SOMRANDO_HACKLOG_DUMP_DIR=str(OUT), SOMRANDO_HACKLOG_DUMP_IDX=str(dump_idx))
    if SKIP:
        eo["SOM_HACK_SKIP"] = SKIP
        ec["SOMRANDO_SKIP_NAMES"] = SKIP
    ec["SOMRANDO_P4_DEV"] = "1"
    sa = []
    for s in sets:
        sa += ["--set", s]
    da = ["--difficulty", diff] if diff else []
    ro = subprocess.run(["dotnet", str(ORACLE_DLL), "--mode", mode, "--seed", seed, "--src", ROM, "--dst", str(OUT / ("o_%s.sfc" % tag)), "--workdir", str(work)] + sa + da,
                        capture_output=True, text=True, env=eo)
    rc = subprocess.run([str(CLI), "--mode", mode, "--seed", seed, "--src", ROM, "--dst", str(OUT / ("c_%s.sfc" % tag))] + sa + da, capture_output=True, text=True, env=ec)
    return ro, rc, lo, lc


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--mode", default="open")
    ap.add_argument("--seed", default="12345")
    ap.add_argument("--set", action="append", default=[])
    ap.add_argument("--difficulty", default=None)
    ap.add_argument("--tag", default="x")
    ap.add_argument("--dump", action="store_true")
    ap.add_argument("--skip", default="", help="hack names ('|' separated) skipped on both sides (getName() of the C# class)")
    ap.add_argument("--skip-common", action="store_true", help="skip the 65 common hacks on both sides (isolates the mode hacks)")
    a = ap.parse_args()
    global SKIP
    SKIP = a.skip
    if a.skip_common:
        SKIP = "|".join(filter(None, [SKIP] + common_names()))
    ro, rc, lo, lc = run(a.mode, a.seed, a.set, a.difficulty, tag=a.tag)
    print("oracle rc=%d  c rc=%d" % (ro.returncode, rc.returncode))
    if ro.returncode:
        print("oracle:", (ro.stdout + ro.stderr).strip()[-300:])
    if rc.returncode:
        print("c:", (rc.stdout + rc.stderr).strip()[-300:])
    LO = [l.rstrip("\n").split("\t") for l in lo.read_text().splitlines() if l]
    LC = [l.rstrip("\n").split("\t") for l in lc.read_text().splitlines() if l] if lc.exists() else []
    print("hacks: oracle %d, c %d" % (len(LO), len(LC)))
    bad = None
    for i in range(max(len(LO), len(LC))):
        x = LO[i] if i < len(LO) else None
        y = LC[i] if i < len(LC) else None
        strip = lambda r: r and r[:2] + r[3:]   # the success flag is informational: C hacks do not all return the C# value
        if strip(x) != strip(y):
            bad = i
            what = []
            if x and y:
                for k, n in enumerate(["idx", "name", "success", "workingOffset", "rom", "rf", "rc"]):
                    if k != 2 and x[k] != y[k]:
                        what.append("%s: oracle %s c %s" % (n, x[k], y[k]))
            print("FIRST DIFFERENCE at hack %d: oracle=%s c=%s" % (i, x and x[1], y and y[1]))
            print("   " + "; ".join(what))
            break
    if bad is None:
        same = (OUT / ("o_%s.sfc" % a.tag)).exists() and (OUT / ("o_%s.sfc" % a.tag)).read_bytes() == (OUT / ("c_%s.sfc" % a.tag)).read_bytes() if rc.returncode == 0 and ro.returncode == 0 else None
        print("all hacks identical; final ROM identical: %s" % same)
        return 0 if same else 1
    if a.dump and LO and LC and bad < min(len(LO), len(LC)):
        run(a.mode, a.seed, a.set, a.difficulty, dump_idx=bad, tag=a.tag)
        po, pc = OUT / ("o%d.rom" % bad), OUT / ("c%d.rom" % bad)
        if po.exists() and pc.exists():
            do, dc = po.read_bytes(), pc.read_bytes()
            ranges, i, n = [], 0, len(do)
            while i < n:
                if do[i] != dc[i]:
                    j = i
                    while j < n and do[j] != dc[j]:
                        j += 1
                    ranges.append((i, j - i))
                    i = j
                else:
                    i += 1
            print("differing ranges after hack %d: %d (showing 20)" % (bad, len(ranges)))
            for o, l in ranges[:20]:
                print("  0x%06X len %d  oracle %s  c %s" % (o, l, do[o:o + min(l, 12)].hex(), dc[o:o + min(l, 12)].hex()))
    return 1


if __name__ == "__main__":
    sys.exit(main())
