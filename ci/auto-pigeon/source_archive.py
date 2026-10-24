#!/usr/bin/env python3
"""Write the complete corresponding source of one release, and record that it rebuilt from itself.

    python3 ci/auto-pigeon/source_archive.py --plan plan.json --out <dist>
    python3 ci/auto-pigeon/source_archive.py --mark-rebuilt <build-report.json> --plan <plan in the archive> --out <dist>

The archive is `git archive` of the planned commit (this repository has no submodules) with
`ci/auto-pigeon/release-plan.json` added, so `build.py` can build it without git: same version,
banner and SOURCE_DATE_EPOCH. Members are sorted, owned by 0/0 and dated at the commit time, and the
gzip header carries no name and that time, so the same commit gives the same bytes.

The q3map2 libraries are not vendored: each target's build-info.json names the distribution
package (and version) its libraries came from, which is how their source is obtained.
"""

import argparse
import gzip
import io
import json
import pathlib
import sys
import subprocess
import tarfile

from common import REPO, Failure, load_json, log, sha256_file, write_json


def write_archive(plan, out):
    stem = plan["source_asset"][:-len(".tar.gz")]
    log("+ git archive --format=tar --prefix=%s/ %s" % (stem, plan["commit"]))
    data = subprocess.run(["git", "archive", "--format=tar", "--prefix=%s/" % stem, plan["commit"]], cwd=str(REPO),
                          stdout=subprocess.PIPE, check=True).stdout
    members = []
    with tarfile.open(fileobj=io.BytesIO(data)) as src:
        for m in src.getmembers():
            members.append((m, src.extractfile(m).read() if m.isfile() else None))
    plan_bytes = (json.dumps(dict(plan, publish=False), indent=2, sort_keys=True) + "\n").encode()
    extra = tarfile.TarInfo("%s/ci/auto-pigeon/release-plan.json" % stem)
    extra.size = len(plan_bytes)
    members.append((extra, plan_bytes))
    members.sort(key=lambda item: item[0].name)
    buffer = io.BytesIO()
    with tarfile.open(fileobj=buffer, mode="w", format=tarfile.PAX_FORMAT) as tar:
        for info, payload in members:
            info.mtime, info.uid, info.gid, info.uname, info.gname = plan["commit_time"], 0, 0, "", ""
            info.pax_headers = {}
            if info.isdir():
                info.mode = 0o755
            elif info.isfile():
                info.mode = 0o755 if info.mode & 0o111 else 0o644
            tar.addfile(info, io.BytesIO(payload) if payload is not None else None)
    path = pathlib.Path(out) / plan["source_asset"]
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "wb") as f:
        with gzip.GzipFile(filename="", mode="wb", fileobj=f, mtime=plan["commit_time"], compresslevel=9) as gz:
            gz.write(buffer.getvalue())
    files = sum(1 for info, _ in members if info.isfile())
    record = {"schema": "auto-pigeon-q3map2-source/1", "asset": path.name, "commit": plan["commit"], "version": plan["version"],
              "sha256": sha256_file(path), "size": path.stat().st_size, "files": files, "rebuild_verified": False}
    write_json(pathlib.Path(out) / (stem + ".source.json"), record)
    log("wrote %s  %d bytes  %d files  sha256 %s" % (path.name, record["size"], files, record["sha256"]))


def mark_rebuilt(plan, build_report, out):
    stem = plan["source_asset"][:-len(".tar.gz")]
    record_path = pathlib.Path(out) / (stem + ".source.json")
    record = load_json(record_path)
    report = load_json(build_report)
    if report["commit"] != plan["commit"] or record["commit"] != plan["commit"]:
        raise Failure("the rebuild is not of the archived commit")
    if not report["regress"] or report["regress"]["exit"] != 0:
        raise Failure("the program rebuilt from the source archive did not pass its regressions")
    record.update(rebuild_verified=True, rebuilt_on=report["platform"], rebuilt_program_sha256=report["program_sha256"],
                  rebuilt_regress=report["regress"]["counts"], rebuilt_toolchain=report["toolchain"])
    write_json(record_path, record)
    log("source archive rebuilt and passed: q3map2 sha256 %s" % report["program_sha256"])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--plan", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--mark-rebuilt")
    args = ap.parse_args()
    plan = load_json(args.plan)
    if args.mark_rebuilt:
        mark_rebuilt(plan, args.mark_rebuilt, args.out)
    else:
        write_archive(plan, args.out)


if __name__ == "__main__":
    try:
        main()
    except Failure as e:
        sys.exit("source_archive: " + str(e))
