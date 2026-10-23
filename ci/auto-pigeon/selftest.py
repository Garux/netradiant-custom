#!/usr/bin/env python3
"""Cheap checks of the release scripts themselves; the policy job runs them on every push, before anything is built.

    python3 ci/auto-pigeon/selftest.py

Covers: every script compiles; the authored regression files are what generate.py writes; the
known-failure record names real cases; the workflow's pinning, permissions and gating; and the
publication decision on fixtures — all targets present, an optional target absent, a required
target absent or failed (nothing published), and a rerun meeting identical and differing bytes.
"""

import json
import pathlib
import py_compile
import re
import subprocess
import sys
import tempfile

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import publish  # noqa: E402
from common import (ACCEPTANCE_STEPS, OPTIONAL_PLATFORMS, PLATFORMS, REQUIRED_PLATFORMS, TAG_PREFIX, Failure,  # noqa: E402
                    write_json)

REPO = HERE.parent.parent
WORKFLOW = REPO / ".github" / "workflows" / "auto-pigeon-release.yml"


def check(condition, message):
    if not condition:
        sys.exit("selftest: " + message)


def fixture_artifacts(root, plan, platforms, failing=()):
    for platform in platforms:
        stem = plan["assets"][platform][:-len(".zip")]
        d = root / ("candidate-" + platform)
        d.mkdir(parents=True)
        z = d / plan["assets"][platform]
        z.write_bytes(("archive of %s" % platform).encode())
        digest = publish.sha256_file(z)
        write_json(d / (stem + ".package.json"), {"commit": plan["commit"], "banner": plan["banner"], "sha256": digest,
                                                   "program_sha256": "0" * 64})
        verdict = "fail" if platform in failing else "pass"
        steps = [{"status": "pass"}] * ACCEPTANCE_STEPS if verdict == "pass" else [{"status": "fail"}]
        write_json(root / ("acceptance-" + platform) / ("acceptance-%s.json" % platform),
                   {"verdict": verdict, "failure": "fixture failure", "commit": plan["commit"], "banner": plan["banner"],
                    "archive_sha256": digest, "platform": platform, "steps": steps,
                    "regress": {"required": {"passed": 35, "cases": 35}, "known_failures": {"still_failing": 2}, "cases": {}}})


def publication_fixtures():
    plan = {"commit": "c" * 40, "banner": "2.5.17n-ap.1+gcccccccc.upstream.82161339", "tag": TAG_PREFIX + "2.5.17n-ap.1",
            "version": "2.5.17n-ap.1", "upstream": {"repository": "u", "base_commit": "b" * 40, "base_tag": "upstream-base/bbbbbbbb"},
            "fork_repository": "f", "patch_series": [{"commit": "c" * 40, "subject": "q3map2: a fix", "compiler": True}],
            "assets": {p: "auto-pigeon-q3map2-2.5.17n-ap.1-%s.zip" % p for p in PLATFORMS}}
    with tempfile.TemporaryDirectory() as tmp:
        tmp = pathlib.Path(tmp)
        fixture_artifacts(tmp / "all", plan, PLATFORMS)
        admitted, missing = publish.decide(plan, tmp / "all")
        check(len(admitted) == len(PLATFORMS) and not missing, "every target present must all be admitted")

        fixture_artifacts(tmp / "no-optional", plan, REQUIRED_PLATFORMS)
        admitted, missing = publish.decide(plan, tmp / "no-optional")
        check([m[0] for m in missing] == list(OPTIONAL_PLATFORMS) and len(admitted) == len(REQUIRED_PLATFORMS),
              "an absent optional target is named as not published, the required ones admitted")
        manifest, notes = publish.manifest_and_notes(plan, admitted, missing, {"name": "s.tar.gz", "path": "x", "kind": "source",
                                                                               "rebuilt_and_tested_from_the_archive": True},
                                                     "run", {"R04.m4.p08_nearby": {"mechanism": "M4", "failing": "9 points", "owner": "NEW_340"}})
        check(manifest["targets"]["linux-arm64"]["status"] == "not_published" and not manifest["targets"]["linux-arm64"]["required"],
              "the manifest must mark the missing optional target not_published")
        check(all(manifest["targets"][p]["status"] == "published" and manifest["targets"][p]["required"] for p in REQUIRED_PLATFORMS),
              "the manifest must mark every required target published and required")
        check("R04.m4.p08_nearby" in notes and "still FAIL" in notes, "the notes must list the known failures as failing")

        for name, platforms, failing in (("no-macos-arm64", [p for p in PLATFORMS if p != "macos-arm64"], ()),
                                         ("failed-windows", PLATFORMS, ("windows-amd64",))):
            fixture_artifacts(tmp / name, plan, platforms, failing)
            try:
                publish.decide(plan, tmp / name)
            except Failure as e:
                check("required target" in str(e), "the refusal must name the required target")
            else:
                sys.exit("selftest: %s must refuse publication" % name)

        linux = {"built_program_sha256": "1" * 64, "program_sha256": "2" * 64}
        check(publish.rebuilt_matches(linux, {"rebuilt_program_sha256": "1" * 64}) is True,
              "a source rebuild is compared with the build before packaging, not with the patched shipped file")
        check(publish.rebuilt_matches(linux, {"rebuilt_program_sha256": "2" * 64}) is False, "a different rebuild is not equal")
        check(publish.rebuilt_matches({"program_sha256": "2" * 64}, {"rebuilt_program_sha256": "2" * 64}) is None,
              "without the pre-packaging digest the answer is unknown, never a guess")

        a, b = tmp / "a.zip", tmp / "b.zip"
        a.write_bytes(b"same")
        b.write_bytes(b"new")
        digest_a = publish.sha256_file(a)
        todo = publish.reconcile([a, b], lambda n: digest_a if n == "a.zip" else None)
        check(todo == [b], "a rerun uploads only what is not published yet")
        try:
            publish.reconcile([a], lambda n: "0" * 64)
        except Failure as e:
            check("kept" in str(e), "the refusal must say the published asset is kept")
        else:
            sys.exit("selftest: differing published bytes must stop a rerun")


def workflow_checks():
    text = WORKFLOW.read_text(encoding="utf-8")
    for use in re.findall(r"uses:\s*(\S+)", text):
        check(re.fullmatch(r"[\w.-]+/[\w.-]+@[0-9a-f]{40}", use) is not None, "action %s is not pinned to a commit" % use)
    check(re.search(r"^\s*pull_request_target", text, re.M) is None, "pull_request_target must never be used")
    check(len(re.findall(r"^\s+contents: write\s*$", text, re.M)) == 1, "exactly one job may write repository contents")
    for platform in REQUIRED_PLATFORMS:
        rows = re.findall(r"\{ *platform: *%s,[^}]*\}" % platform, text)
        check(rows and all("candidate: false" in r for r in rows), "%s must be a required (candidate: false) matrix row" % platform)
    for platform in OPTIONAL_PLATFORMS:
        rows = re.findall(r"\{ *platform: *%s,[^}]*\}" % platform, text)
        check(rows and all("candidate: true" in r for r in rows), "%s must be an optional (candidate: true) matrix row" % platform)
    publish_job = text.split("\n  publish:", 1)[1]
    check(re.search(r"needs: \[plan, build, accept, source, sanitizers\]", publish_job) is not None,
          "publish must need plan, build, accept, source and the sanitizer job")
    check("startsWith(github.ref, 'refs/tags/" in publish_job, "publish must run only for a pushed tag")
    others = [p for p in (REPO / ".github" / "workflows").glob("*.y*ml") if p.name != WORKFLOW.name]
    for other in others:
        body = other.read_text(encoding="utf-8")
        # upstream's own workflows stay as upstream wrote them; none may run for our branch or our tags
        check(re.search(r"branches:\s*\[[^\]]*\bmain\b", body) is None and re.search(r"^\s+tags:", body, re.M) is None,
              "%s (upstream's) must not run for main or for a tag, so the release path stays one" % other.name)


def main():
    for script in sorted(HERE.glob("*.py")):
        py_compile.compile(str(script), doraise=True)
    py_compile.compile(str(REPO / "regression_tests" / "auto-pigeon" / "generate.py"), doraise=True)
    subprocess.run([sys.executable, str(REPO / "regression_tests" / "auto-pigeon" / "generate.py"), "--check"], check=True)
    cases = {c["id"] for c in json.loads((REPO / "regression_tests" / "auto-pigeon" / "cases.json").read_text())["cases"]}
    known = json.loads((REPO / "regression_tests" / "auto-pigeon" / "known-failures.json").read_text())["cases"]
    check(set(known) <= cases, "known-failures.json names unknown cases: %s" % sorted(set(known) - cases))
    for cid, k in known.items():
        check(all(k.get(f) for f in ("mechanism", "owner", "failing", "observed_on")), "%s lacks a field of its record" % cid)
    check(not TAG_PREFIX.startswith("auto-pigeon-ericw"), "the tag namespace must not be EricW's")
    check(set(REQUIRED_PLATFORMS) | set(OPTIONAL_PLATFORMS) == set(PLATFORMS) and not set(REQUIRED_PLATFORMS) & set(OPTIONAL_PLATFORMS),
          "required and optional targets must partition the platforms")
    workflow_checks()
    publication_fixtures()
    print("selftest: ok")


if __name__ == "__main__":
    main()
