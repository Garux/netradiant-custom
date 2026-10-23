#!/usr/bin/env python3
"""Publish ONE prerelease from archives that were already built, tested and accepted natively.

    python3 ci/auto-pigeon/publish.py --plan plan.json --artifacts <dir> --repo owner/name \
        --run-url <url> --out <dir> [--dry-run]

This never compiles, edits an archive or downloads a toolchain. For each target it reads the
package record and the native acceptance report, and admits the target only when both agree with
the plan and with the bytes on disk. Every REQUIRED target must be admitted or nothing is
published; an OPTIONAL target that did not pass is left out and named as not published. A release
is created only for the tag the plan computed, which the workflow was started by.

A rerun never replaces anything: the tag must point at the planned commit, an existing asset must be
byte-identical, and a difference stops the job with an explanation. `decide()` and `reconcile()` are
pure so selftest.py can run them on fixtures (a missing required target, a rerun with identical
and with differing bytes).
"""

import argparse
import json
import os
import pathlib
import sys
import tempfile

from common import (ACCEPTANCE_STEPS, LABELS, PLATFORMS, REQUIRED_PLATFORMS, Failure, load_json, log,
                    run, sha256_file, write_json)


def find(root, name):
    hits = sorted(pathlib.Path(root).rglob(name))
    return hits[0] if hits else None


def evaluate(platform, plan, artifacts):
    """(admitted record | None, reason when not admitted). Raises Failure when records contradict the bytes."""
    asset = plan["assets"][platform]
    stem = asset[:-len(".zip")]
    package_path, zip_path = find(artifacts, stem + ".package.json"), find(artifacts, asset)
    report_path = find(artifacts, "acceptance-%s.json" % platform)
    if not package_path or not zip_path:
        return None, "no archive reached this job: its build job did not finish (build, regressions, packaging or upload)"
    if not report_path:
        return None, "the archive was built but its native acceptance wrote no report"
    package, report = load_json(package_path), load_json(report_path)
    if report.get("verdict") != "pass":
        return None, "native acceptance failed: %s" % report.get("failure", "see the run")
    digest = sha256_file(zip_path)
    for label, got, want in (
            ("package commit", package["commit"], plan["commit"]), ("package banner", package["banner"], plan["banner"]),
            ("acceptance commit", report["commit"], plan["commit"]), ("acceptance banner", report["banner"], plan["banner"]),
            ("accepted archive digest", report["archive_sha256"], digest), ("packaged archive digest", package["sha256"], digest),
            ("acceptance platform", report["platform"], platform)):
        if got != want:
            raise Failure("%s: %s is %r, expected %r" % (platform, label, got, want))
    passed = [s for s in report["steps"] if s["status"] == "pass"]
    if len(passed) != ACCEPTANCE_STEPS or len(report["steps"]) != ACCEPTANCE_STEPS:
        raise Failure("%s: the acceptance report says pass with %d of %d steps passed" % (platform, len(passed), ACCEPTANCE_STEPS))
    regress = report["regress"]
    return {"platform": platform, "name": asset, "path": str(zip_path), "size": zip_path.stat().st_size, "sha256": digest,
            "kind": "toolchain", "program_sha256": package["program_sha256"],
            "acceptance": {"verdict": "pass", "steps_passed": len(passed)},
            "regressions": {"required_passed": regress["required"]["passed"], "required_cases": regress["required"]["cases"],
                            "known_failures_still_failing": regress["known_failures"]["still_failing"],
                            "cases": regress["cases"]}}, None


def decide(plan, artifacts):
    """(admitted, missing): every required target admitted, optional ones admitted or named with a reason."""
    admitted, missing = [], []
    for platform in PLATFORMS:
        record, reason = evaluate(platform, plan, artifacts)
        if record:
            admitted.append(record)
        elif platform in REQUIRED_PLATFORMS:
            raise Failure("required target %s cannot be published: %s. Nothing is published: a release with a "
                          "required target missing would read as all-target success." % (platform, reason))
        else:
            missing.append((platform, reason))
    return admitted, missing


def reconcile(uploads, remote_digest):
    """uploads: [Path]. remote_digest(name) -> sha256 of the published asset, or None. Returns the paths still to
    upload; raises Failure on any published asset whose bytes differ (a published asset is never replaced)."""
    todo = []
    for p in uploads:
        theirs = remote_digest(p.name)
        if theirs is None:
            todo.append(p)
        elif theirs != sha256_file(p):
            raise Failure("asset %s is already published with sha256 %s; this run produced %s. The published asset is "
                          "kept and nothing was uploaded: a second build of one commit that differs is a finding, not a "
                          "licence to replace a published binary." % (p.name, theirs, sha256_file(p)))
    return todo


def manifest_and_notes(plan, admitted, missing, source, run_url, known):
    manifest = {
        "schema": "auto-pigeon-q3map2-release-manifest/1", "tag": plan["tag"], "version": plan["version"],
        "commit": plan["commit"], "version_banner": plan["banner"], "upstream": plan["upstream"],
        "fork": plan["fork_repository"], "patch_series": plan["patch_series"], "prerelease": True, "signed": False,
        "built_by": run_url,
        "qualification": "the declared regression cases and native archive acceptance of each published target; NOT a "
                         "claim that every map compiles: known failures are listed and still fail",
        "known_failures": known,
        "assets": [{k: v for k, v in a.items() if k != "path"} for a in admitted + [source]],
        "targets": dict({a["platform"]: {"status": "published", "required": a["platform"] in REQUIRED_PLATFORMS,
                                         "asset": a["name"], "sha256": a["sha256"], "size": a["size"]} for a in admitted},
                        **{p: {"status": "not_published", "required": False, "reason": r} for p, r in missing}),
    }
    lines = [
        "**Prerelease of `q3map2` from the Auto-Pigeon maintained fork of NetRadiant-custom.** Not an upstream release: "
        "upstream is [Garux/netradiant-custom](%s) and has not reviewed these changes." % plan["upstream"]["repository"],
        "",
        "- Commit: [`%s`](%s/commit/%s)" % (plan["commit"], plan["fork_repository"], plan["commit"]),
        "- Upstream base: `%s` (`%s`)" % (plan["upstream"]["base_commit"], plan["upstream"]["base_tag"]),
        "- `q3map2` prints `%s`" % plan["banner"],
        "",
        "### What this fork changes in the compiler",
        "",
    ]
    lines += ["- %s (`%s`)" % (c["subject"], c["commit"][:8]) for c in plan["patch_series"] if c["compiler"]]
    lines += [
        "",
        "Each change has a regression case under `regression_tests/auto-pigeon/` that fails without it.",
        "",
        "### Qualification boundary",
        "",
        "Every published archive passed, on a native runner of its own platform and from the extracted ZIP, all required "
        "regression cases (BSP; NORMAL VIS and LIGHT where declared). **These cases still FAIL and are not fixed in this "
        "release:**",
        "",
    ]
    lines += ["- `%s` (%s): %s. Owner: %s." % (cid, k["mechanism"], k["failing"], k["owner"]) for cid, k in sorted(known.items())]
    lines += ["", "### Downloads", "", "| Target | Archive | Required regressions | Known failures (still failing) | Native acceptance |",
              "| --- | --- | --- | --- | --- |"]
    for a in admitted:
        r = a["regressions"]
        lines.append("| %s | `%s` | %d of %d passed | %d | %d of %d steps, %s |" % (
            LABELS[a["platform"]], a["name"], r["required_passed"], r["required_cases"], r["known_failures_still_failing"],
            a["acceptance"]["steps_passed"], ACCEPTANCE_STEPS, "required target" if a["platform"] in REQUIRED_PLATFORMS else "optional target"))
    for platform, reason in missing:
        lines.append("| %s | **not published** | | | optional target: %s |" % (LABELS[platform], reason))
    lines += [
        "",
        "`%s` is the complete corresponding source (this commit plus `ci/auto-pigeon/release-plan.json`); it was rebuilt "
        "and its regressions re-run from the unpacked archive alone%s. Each archive's `build-info.json` names the "
        "distribution package and version of every bundled library, which is where their source comes from." % (
            source["name"], "" if source["rebuilt_and_tested_from_the_archive"] else " (**NOT verified in this run**)"),
        "",
        "### Verify",
        "",
        "```sh\nsha256sum -c SHA256SUMS --ignore-missing        # Linux\nshasum -a 256 -c SHA256SUMS --ignore-missing    # macOS\n```",
        "",
        "A SHA-256 is an integrity check, not a signature. `release-manifest.json` records every target, asset, digest and result.",
        "",
        "Built, tested, accepted and published by %s." % run_url,
    ]
    return manifest, "\n".join(lines) + "\n"


def gh(args, check=True):
    return run(["gh", *args], capture=True, check=check)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--plan", required=True)
    ap.add_argument("--artifacts", required=True)
    ap.add_argument("--repo", required=True)
    ap.add_argument("--run-url", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--known-failures", default=str(pathlib.Path(__file__).resolve().parents[2] / "regression_tests" / "auto-pigeon" / "known-failures.json"))
    ap.add_argument("--dry-run", action="store_true", help="decide and write the manifest, notes and sums; touch nothing remote")
    args = ap.parse_args()
    plan = load_json(args.plan)
    artifacts, out = pathlib.Path(args.artifacts).resolve(), pathlib.Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    if not args.dry_run:
        if not plan["publish"]:
            raise Failure("this run's plan does not publish (it was not started by pushing the planned tag)")
        if os.environ.get("GITHUB_REF") != "refs/tags/" + plan["tag"]:
            raise Failure("releases are published from the planned tag only, not from %r" % os.environ.get("GITHUB_REF"))
    admitted, missing = decide(plan, artifacts)
    source_path = find(artifacts, plan["source_asset"])
    source_meta = find(artifacts, plan["source_asset"][:-len(".tar.gz")] + ".source.json")
    if not source_path or not source_meta:
        raise Failure("the source archive is missing")
    meta = load_json(source_meta)
    if meta["commit"] != plan["commit"] or meta["sha256"] != sha256_file(source_path):
        raise Failure("the source archive does not match its record or the plan")
    if not meta.get("rebuild_verified"):
        raise Failure("the source archive was not rebuilt and tested from itself")
    linux = next((a for a in admitted if a["platform"] == "linux-amd64"), None)
    source = {"name": source_path.name, "path": str(source_path), "size": source_path.stat().st_size, "sha256": meta["sha256"],
              "kind": "source", "files": meta["files"], "rebuilt_and_tested_from_the_archive": True,
              "rebuilt_program_sha256": meta["rebuilt_program_sha256"],
              "rebuilt_program_equals_linux_amd64_release": bool(linux and linux["program_sha256"] == meta["rebuilt_program_sha256"])}
    known = load_json(args.known_failures)["cases"]
    manifest, notes = manifest_and_notes(plan, admitted, missing, source, args.run_url, known)
    manifest_path = out / "release-manifest.json"
    write_json(manifest_path, manifest)
    uploads = [pathlib.Path(a["path"]) for a in admitted] + [source_path, manifest_path]
    sums_path = out / "SHA256SUMS"
    with open(sums_path, "w", encoding="utf-8", newline="\n") as f:
        for p in uploads:
            f.write("%s  %s\n" % (sha256_file(p), p.name))
    uploads.append(sums_path)
    notes_path = out / "release-notes.md"
    notes_path.write_text(notes, encoding="utf-8", newline="\n")
    log("targets: %s" % json.dumps({k: v["status"] for k, v in manifest["targets"].items()}, sort_keys=True))
    if args.dry_run:
        log("dry run: nothing was published")
        return

    repo, tag = args.repo, plan["tag"]
    _, ref, _ = gh(["api", "repos/%s/git/ref/tags/%s" % (repo, tag), "--jq", ".object.sha"])
    if ref.strip() != plan["commit"]:
        raise Failure("tag %s points at %s, not at the planned %s; refusing" % (tag, ref.strip(), plan["commit"]))
    code, existing, _ = gh(["release", "view", tag, "--repo", repo, "--json", "assets,isDraft"], check=False)
    if code == 0:
        info = json.loads(existing)
        remote = {a["name"] for a in info["assets"]}
        log("release %s already exists with %d asset(s); comparing, never replacing" % (tag, len(remote)))
        with tempfile.TemporaryDirectory() as tmp:
            def remote_digest(name):
                if name not in remote:
                    return None
                gh(["release", "download", tag, "--repo", repo, "--pattern", name, "--dir", tmp, "--clobber"])
                return sha256_file(pathlib.Path(tmp) / name)
            todo = reconcile(uploads, remote_digest)
        if todo:
            gh(["release", "upload", tag, "--repo", repo, *[str(p) for p in todo]])
    else:
        gh(["release", "create", tag, "--repo", repo, "--verify-tag", "--draft", "--prerelease",
            "--title", "Auto-Pigeon q3map2 %s (prerelease)" % plan["version"], "--notes-file", str(notes_path),
            *[str(p) for p in uploads]])
    gh(["release", "edit", tag, "--repo", repo, "--draft=false", "--prerelease"])

    _, view, _ = gh(["release", "view", tag, "--repo", repo, "--json", "assets,isDraft,isPrerelease,url"])
    info = json.loads(view)
    if info["isDraft"] or not info["isPrerelease"]:
        raise Failure("the release is not a published prerelease: %s" % view)
    with tempfile.TemporaryDirectory() as tmp:
        gh(["release", "download", tag, "--repo", repo, "--dir", tmp])
        published = []
        for p in uploads:
            got = sha256_file(pathlib.Path(tmp) / p.name)
            if got != sha256_file(p):
                raise Failure("the published %s does not have the digest that was uploaded" % p.name)
            published.append({"name": p.name, "sha256": got, "size": p.stat().st_size,
                              "url": "https://github.com/%s/releases/download/%s/%s" % (repo, tag, p.name)})
    write_json(out / "publish-report.json", {"release_url": info["url"], "tag": tag, "commit": plan["commit"],
                                             "assets": published, "targets": manifest["targets"]})
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as f:
            f.write("## Published %s\n\n%s\n\n" % (tag, info["url"]))
            for a in published:
                f.write("- `%s`  `%s`\n" % (a["sha256"], a["name"]))
    log("published %s with %d assets" % (info["url"], len(published)))


if __name__ == "__main__":
    try:
        main()
    except Failure as e:
        sys.exit("publish: " + str(e))
