#!/usr/bin/env python3
"""Decide how much a workflow run must do, from what started it and which files changed.

    python3 ci/auto-pigeon/scope.py --event <name> --ref <GITHUB_REF> [--before <sha>] [--base <sha>] [--all-targets true|false]

Prints and writes to $GITHUB_OUTPUT `scope=`:

  release   a pushed release tag, or a manual run asking for every target: every declared target is
            built, tested and accepted natively, and the source archive rebuilt (a manual run never publishes)
  source    a change that can alter the compiler, its build or its tests: the Linux build and every
            regression case
  docs      anything else (prose, the editor, gamepacks): the policy checks only, nothing is built

An unknown range (a new branch, a force push) counts as `source`: when in doubt, build.
"""

import argparse
import os
import re
import subprocess

from common import REPO

SOURCE = re.compile(r"^(tools/quake3/|libs/|include/|Makefile$|Makefile\.conf$|msys2-Makefile\.conf$|regression_tests/|"
                    r"ci/auto-pigeon/|\.github/workflows/auto-pigeon-release\.yml$)")


def changed(base):
    out = subprocess.run(["git", "diff", "--name-only", base, "HEAD"], cwd=str(REPO), stdout=subprocess.PIPE, check=False)
    return None if out.returncode != 0 else [line for line in out.stdout.decode().splitlines() if line]


def scope(event, ref, before, base, all_targets):
    if ref.startswith("refs/tags/") or (event == "workflow_dispatch" and all_targets):
        return "release", "release tag or all-target request"
    start = base if event == "pull_request" else before
    if not start or set(start) == {"0"}:
        return "source", "no previous commit to compare with"
    files = changed(start)
    if files is None:
        return "source", "the previous commit %s is not in this clone" % start
    hits = [f for f in files if SOURCE.match(f)]
    return ("source", "%d compiler/build/test file(s), e.g. %s" % (len(hits), hits[0])) if hits else \
        ("docs", "%d file(s), none of them compiler, build or test" % len(files))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--event", required=True)
    ap.add_argument("--ref", required=True)
    ap.add_argument("--before", default="")
    ap.add_argument("--base", default="")
    ap.add_argument("--all-targets", default="false")
    args = ap.parse_args()
    value, why = scope(args.event, args.ref, args.before, args.base, args.all_targets == "true")
    print("scope=%s (%s)" % (value, why))
    out = os.environ.get("GITHUB_OUTPUT")
    if out:
        with open(out, "a", encoding="utf-8") as f:
            f.write("scope=%s\n" % value)


if __name__ == "__main__":
    main()
