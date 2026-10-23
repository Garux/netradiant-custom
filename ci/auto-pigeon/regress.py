#!/usr/bin/env python3
"""Compile every case of regression_tests/auto-pigeon with one q3map2 and ask each BSP its assertions.

    python3 ci/auto-pigeon/regress.py --q3map2 <executable> --work <empty dir> [--report report.json]
        [--case ID ...] [--full] [--threads 2] [--timeout 900]

`--full` adds NORMAL -vis and -light to the cases that declare those stages (positive, sealed maps);
without it every case runs the BSP stage only. Standard library only; the maps, shaders and
assertions are regression_tests/auto-pigeon/cases.json (written by its generate.py).

Each case ends in exactly one state:

  pass             compiled, the leak verdict and every probe hold
  fail             a probe or the leak verdict is wrong, or a stage failed or timed out
  known_failure    `fail`, for a case regression_tests/auto-pigeon/known-failures.json names
  unexpected_pass  `pass`, for a case known-failures.json names: the record is out of date

The exit status is 0 only when no case is `fail` or `unexpected_pass`. A known failure is never
counted as a pass and the report keeps it in its own column.
"""

import argparse
import json
import os
import pathlib
import shutil
import struct
import subprocess
import sys
import time

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

from ibsp import CONTENTS, Ibsp  # noqa: E402

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parent.parent
CASES_DIR = REPO / "regression_tests" / "auto-pigeon"
LEAK_MARKERS = ("******* leaked *******", "MAP LEAKED")
# texture images of the regression game root: name -> (width, height, two colours)
IMAGES = {"apreg/checker": (64, 64), "apreg/wide": (64, 32), "apreg/patch": (64, 64), "apreg/water": (64, 64),
          "apreg/clip": (64, 64), "common/caulk": (64, 64), "AUPCHK1": (32, 32)}
ST_TOLERANCE = 0.005


def tga(width, height, cell=8):
    """An uncompressed 24-bit checker; 8-pixel cells so a wrong st shows as a visibly shifted pattern."""
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, width, height, 24, 0)
    rows = bytearray()
    for y in range(height):
        for x in range(width):
            rows += b"\xd0\xd0\xd0" if ((x // cell) + (y // cell)) % 2 else b"\x40\x80\x40"
    return header + bytes(rows)


def make_gameroot(root):
    base = root / "baseq3"
    shutil.copytree(CASES_DIR / "gameroot" / "baseq3" / "scripts", base / "scripts")
    for name, (w, h) in IMAGES.items():
        path = base / "textures" / (name + ".tga")
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(tga(w, h))
    return root


def run_stage(argv, cwd, timeout, log_path):
    start = time.monotonic()
    try:
        proc = subprocess.run([str(a) for a in argv], cwd=str(cwd), stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                              timeout=timeout)
        out, code = proc.stdout.decode("utf-8", "replace"), proc.returncode
    except subprocess.TimeoutExpired as e:
        out, code = (e.stdout or b"").decode("utf-8", "replace") + "\n[regress] TIMEOUT\n", "timeout"
    log_path.write_text(out, encoding="utf-8")
    return code, out, round(time.monotonic() - start, 2)


def near_modulo_one(got, want):
    d = got - want
    return abs(d - round(d)) <= ST_TOLERANCE


def check_probe(bsp, probe):
    kind = probe["kind"]
    if kind in ("solid", "open", "contents"):
        c = bsp.contents_at(tuple(probe["point"]))
        detail = {"leaf": c["leaf"], "opaque": c["opaque"], "contents": c["contents"], "brushes": len(c["brushes"])}
        if kind == "solid":
            ok = bool(c["contents"] & CONTENTS["solid"])
        elif kind == "open":
            ok = not c["opaque"] and not (c["contents"] & (CONTENTS["solid"] | CONTENTS["playerclip"]))
        else:
            ok = all(c["contents"] & CONTENTS[n] for n in probe["has"]) and not any(c["contents"] & CONTENTS[n] for n in probe["lacks"])
        return ok, detail
    if kind in ("drawn", "not_drawn"):
        hit = bsp.drawn_at(tuple(probe["point"]), tuple(probe["normal"]))
        if kind == "not_drawn":
            return hit is None, {"drawn_by": hit}
        if hit is None:
            return False, {"drawn_by": None}
        ok = True
        if "shader" in probe and hit["shader"].lower() != probe["shader"].lower():
            ok = False
        if "st" in probe and not all(near_modulo_one(g, w) for g, w in zip(hit["st"], probe["st"])):
            ok = False
        return ok, {"drawn_by": {"surface": hit["surface"], "shader": hit["shader"], "st": [round(v, 5) for v in hit["st"]]}}
    if kind == "patch":
        patches = bsp.patches()
        if len(patches) <= probe["index"]:
            return False, {"patches": len(patches)}
        p = patches[probe["index"]]
        problems = []
        if p["shader"].lower() != probe["shader"].lower() or (p["width"], p["height"]) != (probe["width"], probe["height"]):
            problems.append("shader/size %s %dx%d" % (p["shader"], p["width"], p["height"]))
        if len(p["points"]) != len(probe["points"]):
            problems.append("%d control points" % len(p["points"]))
        else:
            for i, ((xyz, st), want) in enumerate(zip(p["points"], probe["points"])):
                if any(abs(a - b) > 0.001 for a, b in zip(xyz, want[:3])) or not all(near_modulo_one(a, b) for a, b in zip(st, want[3:])):
                    problems.append("point %d is %s %s" % (i, [round(v, 4) for v in xyz], [round(v, 4) for v in st]))
        return not problems, {"problems": problems[:4]}
    raise ValueError("unknown probe kind %r" % kind)


def run_case(case, q3map2, work, threads, timeout, full, gameroot):
    cid = case["id"]
    case_dir = work / "cases" / cid
    case_dir.mkdir(parents=True)
    source = (CASES_DIR / case["map"]).resolve()
    map_path = case_dir / source.name
    shutil.copyfile(source, map_path)
    if case.get("gameroot"):
        root = work / "gameroots" / cid
        shutil.copytree((CASES_DIR / case["gameroot"]).resolve(), root / "baseq3",
                        ignore=shutil.ignore_patterns("maps"))
    else:
        root = gameroot
    common = [q3map2, "-game", "quake3", "-fs_basepath", root, "-fs_homepath", case_dir / "home", "-threads", threads]
    result = {"id": cid, "mechanism": case["mechanism"], "title": case["title"], "stages": {}, "probes": []}
    bsp_path = map_path.with_suffix(".bsp")
    code, out, seconds = run_stage(common[:1] + ["-bsp"] + common[1:] + ["-meta", map_path], case_dir, timeout, case_dir / "bsp.log")
    result["stages"]["bsp"] = {"exit": code, "seconds": seconds}
    leaked = any(m in out for m in LEAK_MARKERS)
    result["leak"] = leaked
    failures = []
    if code != 0 or not bsp_path.is_file():
        failures.append("bsp stage exit %s%s" % (code, "" if bsp_path.is_file() else ", no BSP"))
    else:
        expect = case["expect"]["leak"]
        if expect is not None and leaked != expect:
            failures.append("leak verdict %s, the source %s" % ("leaked" if leaked else "sealed", "leaks" if expect else "is sealed"))
        snapshot = case_dir / "after-bsp.bsp"
        shutil.copyfile(bsp_path, snapshot)
        bsp = Ibsp(snapshot)
        result["surfaces"] = bsp.surface_counts()
        for i, probe in enumerate(case["probes"]):
            ok, detail = check_probe(bsp, probe)
            result["probes"].append({"n": i, "kind": probe["kind"], "why": probe["why"], "ok": ok, "detail": detail})
            if not ok:
                failures.append("probe %d (%s: %s)" % (i, probe["kind"], probe["why"]))
        if full and "vis" in case["stages"] and not leaked:
            code, out, seconds = run_stage(common[:1] + ["-vis", "-saveprt"] + common[1:] + [map_path], case_dir, timeout,
                                           case_dir / "vis.log")
            result["stages"]["vis"] = {"exit": code, "seconds": seconds}
            if code != 0:
                failures.append("vis stage exit %s" % code)
            elif "light" in case["stages"]:
                code, out, seconds = run_stage(common[:1] + ["-light", "-filter", "-samples", "2", "-bounce", "1"] + common[1:]
                                               + [map_path], case_dir, timeout, case_dir / "light.log")
                result["stages"]["light"] = {"exit": code, "seconds": seconds}
                lit = Ibsp(bsp_path) if code == 0 and bsp_path.is_file() else None
                if code != 0:
                    failures.append("light stage exit %s" % code)
                elif not lit.lumps["visibility"] or not lit.lumps["lightmaps"]:
                    failures.append("the lit BSP has %d visibility and %d lightmap bytes" % (len(lit.lumps["visibility"]),
                                                                                           len(lit.lumps["lightmaps"])))
                else:
                    result["lit"] = {"visibility_bytes": len(lit.lumps["visibility"]), "lightmap_bytes": len(lit.lumps["lightmaps"])}
    result["status"] = "fail" if failures else "pass"
    result["failures"] = failures
    return result


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--q3map2", required=True)
    ap.add_argument("--work", required=True)
    ap.add_argument("--report")
    ap.add_argument("--case", action="append", default=[])
    ap.add_argument("--full", action="store_true", help="add NORMAL -vis and -light where a case declares them")
    ap.add_argument("--threads", default="2")
    ap.add_argument("--timeout", type=int, default=900)
    args = ap.parse_args()
    q3map2 = pathlib.Path(args.q3map2).resolve()
    work = pathlib.Path(args.work).resolve()
    if work.exists() and any(work.iterdir()):
        sys.exit("regress: %s is not empty" % work)
    work.mkdir(parents=True, exist_ok=True)
    document = json.loads((CASES_DIR / "cases.json").read_text(encoding="utf-8"))
    known = json.loads((CASES_DIR / "known-failures.json").read_text(encoding="utf-8"))["cases"]
    cases = [c for c in document["cases"] if not args.case or c["id"] in args.case]
    missing = sorted(set(args.case) - {c["id"] for c in cases})
    if missing:
        sys.exit("regress: no such case: %s" % ", ".join(missing))
    unknown = sorted(set(known) - {c["id"] for c in document["cases"]})
    if unknown:
        sys.exit("regress: known-failures.json names cases that do not exist: %s" % ", ".join(unknown))
    gameroot = make_gameroot(work / "gameroot")
    banner = subprocess.run([str(q3map2), "-help"], stdout=subprocess.PIPE, stderr=subprocess.STDOUT).stdout.decode("utf-8", "replace")
    results = []
    for case in cases:
        r = run_case(case, q3map2, work, args.threads, args.timeout, args.full, gameroot)
        if case["id"] in known:
            r["known_failure"] = known[case["id"]]
            r["status"] = "known_failure" if r["status"] == "fail" else "unexpected_pass"
        results.append(r)
        print("%-18s %-16s %6.1fs  %s" % (r["status"], r["id"], sum(s["seconds"] for s in r["stages"].values()),
                                         "; ".join(r["failures"][:3])), flush=True)
    counts = {}
    for r in results:
        counts[r["status"]] = counts.get(r["status"], 0) + 1
    import hashlib
    report = {"schema": "auto-pigeon-q3map2-regress-report/1", "q3map2_sha256": hashlib.sha256(q3map2.read_bytes()).hexdigest(),
              "version_line": banner.splitlines()[0] if banner else "", "full": args.full, "counts": counts,
              "required": {"cases": sum(1 for r in results if "known_failure" not in r),
                           "passed": counts.get("pass", 0), "failed": counts.get("fail", 0)},
              "known_failures": {"cases": sum(1 for r in results if "known_failure" in r),
                                 "still_failing": counts.get("known_failure", 0), "unexpected_pass": counts.get("unexpected_pass", 0)},
              "cases": results}
    if args.report:
        pathlib.Path(args.report).parent.mkdir(parents=True, exist_ok=True)
        pathlib.Path(args.report).write_text(json.dumps(report, indent=1, sort_keys=True) + "\n", encoding="utf-8")
    print("regress: %s" % json.dumps(counts, sort_keys=True))
    sys.exit(1 if counts.get("fail") or counts.get("unexpected_pass") else 0)


if __name__ == "__main__":
    main()
