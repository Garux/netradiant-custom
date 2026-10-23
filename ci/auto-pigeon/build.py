#!/usr/bin/env python3
"""Build q3map2 (and nothing else) for one target, natively, from a clean copy of the source, then run its regressions.

    python3 ci/auto-pigeon/build.py --platform linux-amd64 --plan plan.json --work <dir> --report build-report.json [--jobs 3]

The recipe is upstream's own Makefile (the one its CI and AppImage use), asked for the single target
`install/q3map2.<machine>`: no editor, no plugins, no gamepacks. What differs per platform is only
where the libraries q3map2 links (glib, libxml2, libpng, libjpeg, zlib, assimp) come from:

  linux-*     Ubuntu 24.04's -dev packages, GCC 14 (`gcc-14`/`g++-14`): the base needs GCC >= 14
  windows-*   MSYS2 MINGW64 packages and its GCC (run this script with the MINGW64 Python, inside that
              shell), upstream's msys2-Makefile.conf
  macos-*     Homebrew bottles, Apple clang; libxml2 is the SDK's (macOS ships it)

SOURCE_DATE_EPOCH is the commit time, so the one __DATE__/__TIME__ in q3map2 (`main.cpp`'s
NetRadiant line) is fixed by the commit, not by the clock. The banner is the plan's.
"""

import argparse
import os
import pathlib
import platform as _platform
import re
import shutil
import sys
import time

from common import HERE, PLATFORMS, REPO, Failure, exe, load_json, log, require_native, run, sha256_file, write_json

MACHINE = {"linux-amd64": "x86_64", "linux-arm64": "aarch64", "macos-amd64": "x86_64", "macos-arm64": "arm64",
           "windows-amd64": "exe"}


def tracked_tree(dest):
    """Copy the files git tracks (or, in an unpacked source archive, every file) into dest."""
    if (REPO / ".git").exists():
        files = run(["git", "ls-files", "-z"], cwd=REPO, capture=True)[1].split("\0")
        modified = [line[3:] for line in run(["git", "status", "--porcelain"], cwd=REPO, capture=True)[1].splitlines()
                    if not line.startswith("??")]
    else:
        files = [str(p.relative_to(REPO)) for p in REPO.rglob("*") if p.is_file()]
        modified = []
    for rel in files:
        if not rel:
            continue
        src = REPO / rel
        if src.is_symlink() or not src.is_file():
            continue
        out = dest / rel
        out.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(src, out)
    return modified


def brew_prefix(formula=None):
    return run(["brew", "--prefix"] + ([formula] if formula else []), capture=True)[1].strip()


def make_argv(platform, plan, jobs):
    argv = ["make", "-j%d" % jobs, "BUILD=release", "DEPENDENCIES_CHECK=off", "Q3MAP_VERSION=" + plan["banner"]]
    if platform.startswith("linux"):
        argv += ["CC=gcc-14", "CXX=g++-14", "CXXFLAGS=-Wno-deprecated-copy"]
    elif platform.startswith("windows"):
        argv += ["MAKEFILE_CONF=msys2-Makefile.conf"]
    else:
        sdk = run(["xcrun", "--show-sdk-path"], capture=True)[1].strip()
        jpeg = brew_prefix("jpeg-turbo")
        argv += ["OS=Darwin", "CC=clang", "CXX=clang++", "MACLIBDIR=%s/lib" % brew_prefix(),
                 "CPPFLAGS_XML=-I%s/usr/include/libxml2" % sdk, "LIBS_XML=-lxml2",
                 "CPPFLAGS_JPEG=-I%s/include" % jpeg, "LIBS_JPEG=-L%s/lib -ljpeg" % jpeg]
    return argv + ["install/q3map2.%s" % MACHINE[platform]]


def toolchain(platform, env):
    def first_line(argv):
        code, out, _ = run(argv, env=env, check=False, capture=True)
        return out.strip().splitlines()[0] if code == 0 and out.strip() else None

    info = {"make": first_line(["make", "--version"])}
    if platform.startswith("linux"):
        info["compiler"] = first_line(["g++-14", "--version"])
        info["dependencies"] = {}
        for pkg in ("libglib2.0-dev", "libxml2-dev", "libpng-dev", "libjpeg-turbo8-dev", "libassimp-dev", "zlib1g-dev"):
            code, out, _ = run(["dpkg-query", "-W", "-f=${Version}", pkg], check=False, capture=True)
            info["dependencies"][pkg] = out.strip() if code == 0 else None
        info["os"] = first_line(["sh", "-c", ". /etc/os-release && echo $PRETTY_NAME"])
    elif platform.startswith("windows"):
        info["compiler"] = first_line(["g++", "--version"])
        code, out, _ = run(["pacman", "-Q"], env=env, check=False, capture=True)
        wanted = ("glib2", "libxml2", "libpng", "libjpeg-turbo", "assimp", "zlib", "gcc")
        info["dependencies"] = {line.split()[0]: line.split()[1] for line in out.splitlines()
                                if any(line.split()[0] == "mingw-w64-x86_64-" + w for w in wanted)}
    else:
        info["compiler"] = first_line(["clang++", "--version"])
        info["sdk"] = first_line(["xcrun", "--show-sdk-version"])
        info["dependencies"] = {}
        for formula in ("glib", "libpng", "jpeg-turbo", "assimp"):
            code, out, _ = run(["brew", "list", "--versions", formula], check=False, capture=True)
            info["dependencies"][formula] = out.strip() if code == 0 else None
        info["os"] = first_line(["sw_vers", "-productVersion"])
    return info


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--platform", required=True, choices=PLATFORMS)
    ap.add_argument("--plan", required=True)
    ap.add_argument("--work", required=True)
    ap.add_argument("--report", required=True)
    ap.add_argument("--jobs", type=int, default=3)
    ap.add_argument("--no-regress", action="store_true", help="build only (a local experiment; never releasable)")
    args = ap.parse_args()
    platform = args.platform
    require_native(platform)
    plan = load_json(args.plan)
    work = pathlib.Path(args.work).resolve()
    src = work / "src"
    if src.exists():
        shutil.rmtree(src)
    src.mkdir(parents=True)
    modified = tracked_tree(src)
    env = dict(os.environ, SOURCE_DATE_EPOCH=str(plan["commit_time"]))
    start = time.monotonic()
    run(make_argv(platform, plan, args.jobs), cwd=src, env=env)
    seconds = round(time.monotonic() - start, 1)
    program = src / "install" / ("q3map2.exe" if platform.startswith("windows") else "q3map2.%s" % MACHINE[platform])
    if not program.is_file():
        raise Failure("make did not produce %s" % program)
    code, out, _ = run([program, "-help"], check=False, capture=True)
    first = out.splitlines()[0].strip() if out else ""
    if first != plan["banner"]:
        raise Failure("the built q3map2 says %r on its first line, the plan's banner is %r" % (first, plan["banner"]))
    report = {
        "schema": "auto-pigeon-q3map2-build-report/1", "platform": platform, "commit": plan["commit"],
        "version": plan["version"], "banner": plan["banner"], "host": {"machine": _platform.machine(), "system": _platform.system()},
        "make": make_argv(platform, plan, args.jobs), "build_seconds": seconds, "source_date_epoch": plan["commit_time"],
        "toolchain": toolchain(platform, env), "program": str(program), "program_sha256": sha256_file(program),
        "program_size": program.stat().st_size, "tracked_files_modified": modified, "regress": None,
    }
    if not args.no_regress:
        regress_report = work / "regress-report.json"
        code, _, rs = run([sys.executable, HERE / "regress.py", "--q3map2", program, "--work", work / "regress work",
                           "--report", regress_report], check=False)
        r = load_json(regress_report)
        report["regress"] = {"exit": code, "seconds": rs, "counts": r["counts"], "required": r["required"],
                             "known_failures": r["known_failures"], "q3map2_sha256": r["q3map2_sha256"]}
        if code != 0:
            write_json(args.report, report)
            raise Failure("the regressions failed on the build tree's q3map2: %s" % r["counts"])
    write_json(args.report, report)
    log("built %s  %d bytes  sha256 %s  in %.1fs" % (program.name, report["program_size"], report["program_sha256"], seconds))


if __name__ == "__main__":
    try:
        main()
    except Failure as e:
        sys.exit("build: " + str(e))
