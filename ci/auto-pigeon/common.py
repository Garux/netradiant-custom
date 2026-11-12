"""Shared helpers of the Auto-Pigeon q3map2 release scripts. Standard library only."""

import hashlib
import json
import os
import pathlib
import subprocess
import sys
import time

HERE = pathlib.Path(__file__).resolve().parent
REPO = HERE.parent.parent
# Every target a release declares. The first four are required: a release is published only when all of
# them built, passed their regressions and were accepted natively. linux-arm64 is OPTIONAL: built and
# tested on a native arm64 runner, published when it passes, and never required.
PLATFORMS = ("linux-amd64", "windows-amd64", "macos-amd64", "macos-arm64", "linux-arm64")
REQUIRED_PLATFORMS = ("linux-amd64", "windows-amd64", "macos-amd64", "macos-arm64")
OPTIONAL_PLATFORMS = ("linux-arm64",)
LABELS = {"linux-amd64": "Linux x86-64", "windows-amd64": "Windows x86-64", "macos-amd64": "macOS Intel (x86-64)",
          "macos-arm64": "macOS Apple silicon (arm64)", "linux-arm64": "Linux arm64"}
PROGRAM = "q3map2"
ASSET_PREFIX = "auto-pigeon-q3map2"
TAG_PREFIX = "auto-pigeon-q3map2-v"
BASE_TAG_PREFIX = "upstream-base/"
UPSTREAM_REPOSITORY = "https://github.com/Garux/netradiant-custom"
FORK_REPOSITORY = "https://github.com/auto-pigeon/q3map2"
# The steps accept.py runs. publish.py refuses a report with any other number: a report that stopped
# early, or one written by an older acceptance that asked fewer questions, is not a pass.
ACCEPTANCE_STEPS = 9
# What a CI run may stand on (ci/auto-pigeon/TESTING.md): one tiny sealed room through BSP, NORMAL VIS and LIGHT, and
# one genuine leak that must be reported. Every tool invocation of a smoke run has SMOKE_TOOL_SECONDS. Default local
# qualification stays `full`: every declared case, BSP plus the stages each case declares.
SMOKE_POSITIVE = "R01.room"
SMOKE_NEGATIVE = "R07.open.1"
SMOKE_CASES = {SMOKE_POSITIVE: ("bsp", "vis", "light"), SMOKE_NEGATIVE: ("bsp",)}
SMOKE_TOOL_SECONDS = 60
TEST_MODES = ("full", "smoke")


class Failure(Exception):
    """A gate that did not hold. Never caught to turn a failure into a pass."""


def regress_summary(report, exit_code, seconds):
    """What build, acceptance, packaging and publication keep of a regress.py report."""
    return {"mode": report.get("mode"), "exit": exit_code, "seconds": seconds, "counts": report["counts"], "required": report["required"],
            "known_failures": report["known_failures"], "q3map2_sha256": report["q3map2_sha256"], "expected": report.get("expected"),
            "cases": {c["id"]: {"status": c["status"], "leak": c.get("leak"),
                                "stages": {name: stage["exit"] for name, stage in c["stages"].items()}} for c in report["cases"]}}


def validated_regress(summary):
    """The one answer to "did this regression run qualify anything?". Returns its mode, or raises Failure.

    A missing report, an empty case set, a skipped or timed-out stage, a missing output and a partial run are never
    passes: each of them leaves a summary this refuses."""
    if not summary:
        raise Failure("there is no regression report")
    mode = summary.get("mode")
    if mode not in TEST_MODES:
        raise Failure("only a full or an explicit smoke regression run qualifies a build, not %r" % (mode,))
    if summary.get("exit") != 0:
        raise Failure("the regressions exited %s: %s" % (summary.get("exit"), summary.get("counts")))
    cases = summary.get("cases") or {}
    if not cases:
        raise Failure("the regression report holds no cases")
    if sorted(cases) != sorted(summary.get("expected") or []):
        raise Failure("the regression report's cases are not the ones it was asked to run")
    if mode == "smoke":
        if set(cases) != set(SMOKE_CASES):
            raise Failure("a smoke report must hold exactly %s, not %s" % (sorted(SMOKE_CASES), sorted(cases)))
        for cid, stages in SMOKE_CASES.items():
            case = cases[cid]
            if case["status"] != "pass" or set(case["stages"]) != set(stages) or any(code != 0 for code in case["stages"].values()):
                raise Failure("smoke case %s did not pass all of %s: %s" % (cid, stages, case))
        if cases[SMOKE_POSITIVE]["leak"] is not False or cases[SMOKE_NEGATIVE]["leak"] is not True:
            raise Failure("the smoke leak verdicts are wrong: a sealed room must seal and the genuine opening must leak")
        return mode
    required = summary["required"]
    if required["cases"] <= 0 or required["passed"] != required["cases"] or required.get("failed"):
        raise Failure("the required regressions did not all pass: %s" % required)
    bad = {cid: c["status"] for cid, c in cases.items() if c["status"] not in ("pass", "known_failure")}
    if bad:
        raise Failure("regression cases are neither passing nor known failures: %s" % bad)
    return mode


def sha256_file(path):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def exe(platform, name=PROGRAM):
    return name + ".exe" if platform.startswith("windows") else name


def load_json(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def write_json(path, data):
    path = pathlib.Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, indent=2, sort_keys=True)
        f.write("\n")


def log(msg):
    """Progress goes to stderr, so a script's stdout carries only its answer (`plan.py --print-tag`)."""
    print(msg, file=sys.stderr, flush=True)


def run(argv, cwd=None, env=None, check=True, capture=False, timeout=None):
    """Run a native command, wait for it, and return (exit, output, seconds). check=True raises on non-zero.
    A timeout is a Failure, whatever `check` says: a program that never answered did not pass anything."""
    argv = [str(a) for a in argv]
    log("+ " + " ".join(('"%s"' % a) if " " in a else a for a in argv) + (("   (cwd %s)" % cwd) if cwd else ""))
    start = time.monotonic()
    try:
        proc = subprocess.run(argv, cwd=str(cwd) if cwd else None, env=env,
                              stdout=subprocess.PIPE if capture else None, stderr=subprocess.STDOUT if capture else None,
                              timeout=timeout)
    except subprocess.TimeoutExpired:
        raise Failure("timed out after %s s: %s" % (timeout, " ".join(argv))) from None
    seconds = round(time.monotonic() - start, 2)
    out = proc.stdout.decode("utf-8", "replace") if capture else ""
    if check and proc.returncode != 0:
        if capture:
            sys.stdout.write(out[-8000:])
        raise Failure("command failed with exit %d: %s" % (proc.returncode, " ".join(argv)))
    return proc.returncode, out, seconds


def host_platform():
    import platform as _p

    machine = _p.machine().lower()
    arch = {"x86_64": "amd64", "amd64": "amd64", "arm64": "arm64", "aarch64": "arm64"}.get(machine, machine)
    system = {"Linux": "linux", "Windows": "windows", "Darwin": "macos"}.get(_p.system(), _p.system().lower())
    if system.startswith(("mingw", "msys", "cygwin")):
        system = "windows"
    return "%s-%s" % (system, arch)


def require_native(platform):
    """A target is only ever built and accepted on its own operating system and CPU."""
    host = host_platform()
    if host != platform:
        raise Failure("this host is %s; refusing to stand in for %s" % (host, platform))
    if platform.startswith("macos"):
        code, out, _ = run(["sysctl", "-n", "sysctl.proc_translated"], check=False, capture=True)
        if code == 0 and out.strip() == "1":
            raise Failure("this process is translated by Rosetta; that is not a native %s host" % platform)
    return host


def clean_env(platform, extra=None):
    """The environment an end user's shell would have: the system's own search path and nothing a build added."""
    keep = ("HOME", "USER", "LANG", "TMPDIR", "TEMP", "TMP", "SystemRoot", "SYSTEMROOT", "windir", "USERPROFILE",
            "COMSPEC", "PATHEXT", "NUMBER_OF_PROCESSORS", "PROCESSOR_ARCHITECTURE", "LOCALAPPDATA", "APPDATA")
    env = {k: v for k, v in os.environ.items() if k in keep}
    if platform.startswith("windows"):
        root = os.environ.get("SystemRoot", r"C:\Windows")
        env["PATH"] = root + r"\System32;" + root
    else:
        env["PATH"] = "/usr/bin:/bin:/usr/sbin:/sbin"
    env.update(extra or {})
    return env
