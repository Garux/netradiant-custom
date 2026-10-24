#!/usr/bin/env python3
"""Accept one candidate ZIP on its own native host, from the EXTRACTED archive only.

    python3 ci/auto-pigeon/accept.py --platform linux-amd64 --archive <zip> --package-json <json> \
        --plan plan.json --work <empty dir> --report acceptance.json

Nothing here reads a build tree or needs a compiler. The archive is checked against its digest
and manifest, unpacked into a directory with spaces, moved, and its q3map2 is run by absolute path
from an unrelated working directory with the operating system's own search path only. Then every
regression case runs against those extracted bytes, with NORMAL -vis and -light where a case
declares them. The verdict is `pass` only when all ACCEPTANCE_STEPS steps passed; the exit status
says the same.
"""

import argparse
import os
import pathlib
import shutil
import stat
import sys
import zipfile

import closure
from common import (ACCEPTANCE_STEPS, HERE, PLATFORMS, REPO, Failure, clean_env, exe, load_json, log, require_native,
                    run, sha256_file, write_json)

REQUIRED_MEMBERS = ("README.md", "build-info.json", "MANIFEST.json", "licenses/LICENSE.txt", "licenses/GPL.txt", "licenses/LGPL.txt")


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--platform", required=True, choices=PLATFORMS)
    ap.add_argument("--archive", required=True)
    ap.add_argument("--package-json", required=True)
    ap.add_argument("--plan", required=True)
    ap.add_argument("--work", required=True)
    ap.add_argument("--report", required=True)
    args = ap.parse_args()
    platform = args.platform
    plan, package = load_json(args.plan), load_json(args.package_json)
    archive = pathlib.Path(args.archive).resolve()
    work = pathlib.Path(args.work).resolve()
    steps = []
    report = {"schema": "auto-pigeon-q3map2-acceptance/1", "platform": platform, "commit": plan["commit"],
              "version": plan["version"], "banner": plan["banner"], "archive": archive.name, "steps": steps}
    state = {}

    def step(name, fn):
        try:
            detail = fn()
            steps.append({"step": name, "status": "pass", "detail": detail})
            log("PASS  %s" % name)
        except Exception as e:  # noqa: BLE001 - every failure is recorded, then the run stops
            steps.append({"step": name, "status": "fail", "detail": str(e)})
            log("FAIL  %s: %s" % (name, e))
            raise

    def s1_identity():
        require_native(platform)
        if archive.name != plan["assets"][platform]:
            raise Failure("archive %s is not the planned %s" % (archive.name, plan["assets"][platform]))
        digest = sha256_file(archive)
        if digest != package["sha256"]:
            raise Failure("archive sha256 %s, package record %s" % (digest, package["sha256"]))
        for key in ("commit", "version", "banner", "platform"):
            if package[key] != (plan[key] if key != "platform" else platform):
                raise Failure("package %s=%r disagrees with the plan" % (key, package[key]))
        report["archive_sha256"] = digest
        return {"sha256": digest, "size": archive.stat().st_size}

    def s2_unpack_and_move():
        if work.exists() and any(work.iterdir()):
            raise Failure("%s is not empty" % work)
        first = work / "unpacked here first"
        with zipfile.ZipFile(archive) as z:
            names = z.namelist()
            stem = plan["assets"][platform][:-len(".zip")]
            bad = [n for n in names if not n.startswith(stem + "/") or ".." in n.split("/")]
            if bad:
                raise Failure("members outside %s/: %s" % (stem, bad[:3]))
            z.extractall(first)
            for info in z.infolist():
                mode = (info.external_attr >> 16) & 0o777
                if mode and not info.is_dir():
                    os.chmod(first / info.filename, mode)
        final = work / "moved tool dir" / stem
        final.parent.mkdir(parents=True)
        shutil.move(str(first / stem), str(final))
        state["root"] = final
        return {"root": "<work>/moved tool dir/%s" % stem, "members": len(names)}

    def s3_manifest():
        root = state["root"]
        manifest = load_json(root / "MANIFEST.json")
        listed = {f["path"]: f for f in manifest["files"]}
        present = {p.relative_to(root).as_posix() for p in root.rglob("*") if p.is_file()} - {"MANIFEST.json"}
        if set(listed) != present:
            raise Failure("manifest/archive mismatch: missing %s, unlisted %s" % (sorted(set(listed) - present)[:3],
                                                                                   sorted(present - set(listed))[:3]))
        for rel, f in listed.items():
            p = root / rel
            if p.stat().st_size != f["size"] or sha256_file(p) != f["sha256"]:
                raise Failure("%s does not match the manifest" % rel)
        if sha256_file(root / "MANIFEST.json") != package["manifest_sha256"]:
            raise Failure("MANIFEST.json is not the one the package recorded")
        return {"files": len(listed)}

    def s4_members():
        root = state["root"]
        missing = [m for m in REQUIRED_MEMBERS if not (root / m).is_file()]
        program = root / "bin" / exe(platform)
        if not program.is_file():
            missing.append("bin/" + exe(platform))
        if missing:
            raise Failure("missing members: %s" % missing)
        if not platform.startswith("windows") and not os.stat(program).st_mode & stat.S_IXUSR:
            raise Failure("bin/q3map2 is not executable after extraction")
        state["program"] = program
        return {"program": "bin/" + exe(platform)}

    def s5_closure():
        return {"kind": closure.check(platform, state["root"] / "bin", [exe(platform)])["kind"]}

    def s6_banner():
        outside = work / "an unrelated directory"
        outside.mkdir()
        code, out, _ = run([state["program"], "-help"], cwd=outside, env=clean_env(platform), check=False, capture=True)
        first = out.splitlines()[0].strip() if out else ""
        if first != plan["banner"]:
            raise Failure("q3map2 -help printed %r first (exit %s), expected %r" % (first, code, plan["banner"]))
        if "-bsp" not in out or "-light" not in out:
            raise Failure("the usage text does not name the -bsp and -light stages")
        return {"first_line": first, "exit": code}

    def s7_build_info():
        info = load_json(state["root"] / "build-info.json")
        for label, got, want in (("commit", info["fork"]["commit"], plan["commit"]), ("banner", info["version_banner"], plan["banner"]),
                                 ("target", info["target"], platform), ("program", info["program"]["sha256"], sha256_file(state["program"])),
                                 ("upstream base", info["upstream"]["base_commit"], plan["upstream"]["base_commit"])):
            if got != want:
                raise Failure("build-info %s is %r, expected %r" % (label, got, want))
        return {"patch_series": len(info["patch_series"])}

    def s8_regressions():
        regress_report = work / "regress-report.json"
        env = clean_env(platform)
        code, out, seconds = run([sys.executable, HERE / "regress.py", "--q3map2", state["program"], "--work",
                                  work / "regression work", "--full", "--report", regress_report], env=env, check=False, capture=True)
        sys.stdout.write(out[-6000:])
        r = load_json(regress_report)
        state["regress"] = r
        report["regress"] = {"exit": code, "seconds": seconds, "counts": r["counts"], "required": r["required"],
                             "known_failures": r["known_failures"], "q3map2_sha256": r["q3map2_sha256"],
                             "cases": {c["id"]: c["status"] for c in r["cases"]}}
        if code != 0:
            raise Failure("regressions on the extracted q3map2: %s" % r["counts"])
        return report["regress"]["counts"]

    def s9_tested_is_shipped():
        if state["regress"]["q3map2_sha256"] != sha256_file(state["program"]) or package["program_sha256"] != sha256_file(state["program"]):
            raise Failure("the regressions ran another q3map2 than the archive ships")
        return {"program_sha256": package["program_sha256"]}

    try:
        for name, fn in (("archive identity", s1_identity), ("unpack into a path with spaces and move", s2_unpack_and_move),
                         ("manifest", s3_manifest), ("required members", s4_members), ("runtime closure", s5_closure),
                         ("banner from a clean environment", s6_banner), ("build-info agrees with the plan", s7_build_info),
                         ("regressions on the extracted bytes (BSP, NORMAL VIS, LIGHT)", s8_regressions),
                         ("tested bytes are shipped bytes", s9_tested_is_shipped)):
            step(name, fn)
        report["verdict"] = "pass" if len(steps) == ACCEPTANCE_STEPS else "fail"
    except Exception as e:  # noqa: BLE001
        report["verdict"] = "fail"
        report["failure"] = str(e)
    write_json(args.report, report)
    log("acceptance: %s (%d of %d steps)" % (report["verdict"], sum(s["status"] == "pass" for s in steps), ACCEPTANCE_STEPS))
    sys.exit(0 if report["verdict"] == "pass" else 1)


if __name__ == "__main__":
    main()
