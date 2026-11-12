#!/usr/bin/env python3
"""Freeze one release plan: the exact commit, its version, tag, banner, upstream base and asset names.

    python3 ci/auto-pigeon/plan.py --sha <full sha> --out plan.json [--ref refs/tags/<tag>]
    python3 ci/auto-pigeon/plan.py --sha HEAD --print-tag        # the ONE tag this commit may be released as

This is the only place a version is computed. Every build, test, manifest and the source archive
read the plan and nothing recomputes it.

  version  <Q3MAP_VERSION>-ap.<N>, N = the number of commits reachable from the commit (strictly
           increasing along main, and never EricW's `1.<N>` namespace). Q3MAP_VERSION is upstream's,
           read from its Makefile.
  tag      auto-pigeon-q3map2-v<version>
  banner   <version>+g<commit8>.upstream.<base8>: what `q3map2` prints as its version and after
           `Q3Map (ydnar) - v`, naming our commit and the upstream commit it is based on.
  base     the newest `upstream-base/<sha8>` tag that is an ancestor of the commit.

A plan publishes only when the workflow was started by pushing exactly the tag this commit computes
to (`--ref`). A tag that names another version is refused here, so a guessed tag cannot release.
"""

import argparse
import os
import re
import sys

from common import (ASSET_PREFIX, BASE_TAG_PREFIX, FORK_REPOSITORY, OPTIONAL_PLATFORMS, PLATFORMS, PROGRAM, REPO,
                    REQUIRED_PLATFORMS, TAG_PREFIX, UPSTREAM_REPOSITORY, Failure, log, run, write_json)


def git(*args):
    return run(["git", *args], cwd=REPO, capture=True)[1].strip()


def upstream_q3map_version(full):
    makefile = git("show", "%s:Makefile" % full)
    m = re.search(r"^Q3MAP_VERSION\s*=\s*(\S+)\s*$", makefile, re.M)
    if not m:
        raise Failure("upstream's Makefile has no Q3MAP_VERSION line")
    return m.group(1)


def make_plan(sha, ref=None):
    full = git("rev-parse", "--verify", sha + "^{commit}")
    if not re.fullmatch(r"[0-9a-f]{40}", full):
        raise Failure("not a full commit id: %r" % full)
    if git("rev-parse", "--is-shallow-repository") != "false":
        raise Failure("shallow checkout: the version is the commit count, so full history is required")
    count = int(git("rev-list", "--count", full))
    base_tag = git("describe", "--tags", "--abbrev=0", "--match", BASE_TAG_PREFIX + "*", full)
    base = git("rev-parse", base_tag + "^{commit}")
    if not base_tag.startswith(BASE_TAG_PREFIX) or base_tag[len(BASE_TAG_PREFIX):] != base[:8]:
        raise Failure("base tag %s does not name its own commit %s" % (base_tag, base[:8]))
    q3 = upstream_q3map_version(full)
    version = "%s-ap.%d" % (q3, count)
    tag = TAG_PREFIX + version
    banner = "%s+g%s.upstream.%s" % (version, full[:8], base[:8])
    series = []
    for line in git("log", "--first-parent", "--format=%H %s", "%s..%s" % (base, full)).splitlines():
        commit, subject = line.split(" ", 1)
        touched = git("diff-tree", "--no-commit-id", "--name-only", "-r", commit).splitlines()
        series.append({"commit": commit, "subject": subject,
                       "compiler": any(p.startswith(("tools/", "libs/", "include/")) or p.startswith("Makefile") for p in touched)})
    series.reverse()
    publish = False
    if ref:
        if ref.startswith("refs/tags/"):
            pushed = ref[len("refs/tags/"):]
            if pushed != tag:
                raise Failure("the pushed tag %s is not the tag this commit is released as (%s); nothing is published "
                              "under a guessed name" % (pushed, tag))
            publish = True
    return {
        "schema": "auto-pigeon-q3map2-plan/1",
        "commit": full,
        "commit_time": int(git("show", "-s", "--format=%ct", full)),
        "commit_count": count,
        "q3map_version": q3,
        "version": version,
        "tag": tag,
        "banner": banner,
        "upstream": {"repository": UPSTREAM_REPOSITORY, "base_tag": base_tag, "base_commit": base},
        "fork_repository": FORK_REPOSITORY,
        "patch_series": series,
        "platforms": list(PLATFORMS),
        "required_platforms": list(REQUIRED_PLATFORMS),
        "optional_platforms": list(OPTIONAL_PLATFORMS),
        "program": PROGRAM,
        "assets": {p: "%s-%s-%s.zip" % (ASSET_PREFIX, version, p) for p in PLATFORMS},
        "source_asset": "%s-%s-source.tar.gz" % (ASSET_PREFIX, version),
        "publish": publish,
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--sha", required=True)
    ap.add_argument("--out")
    ap.add_argument("--ref", default=None, help="the ref that started the workflow (GITHUB_REF)")
    ap.add_argument("--print-tag", action="store_true")
    args = ap.parse_args()
    plan = make_plan(args.sha, args.ref)
    if args.print_tag:
        print(plan["tag"])
        return
    if not args.out:
        sys.exit("plan: --out is required")
    write_json(args.out, plan)
    for key in ("commit", "version", "tag", "banner", "publish"):
        log("%s=%s" % (key, str(plan[key]).lower() if isinstance(plan[key], bool) else plan[key]))
    out = os.environ.get("GITHUB_OUTPUT")
    if out:
        with open(out, "a", encoding="utf-8") as f:
            for key in ("commit", "version", "tag", "banner"):
                f.write("%s=%s\n" % (key, plan[key]))
            f.write("publish=%s\n" % ("true" if plan["publish"] else "false"))


if __name__ == "__main__":
    try:
        main()
    except Failure as e:
        sys.exit("plan: " + str(e))
