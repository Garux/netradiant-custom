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


class Failure(Exception):
    """A gate that did not hold. Never caught to turn a failure into a pass."""


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
    """Run a native command, wait for it, and return (exit, output, seconds). check=True raises on non-zero."""
    argv = [str(a) for a in argv]
    log("+ " + " ".join(('"%s"' % a) if " " in a else a for a in argv) + (("   (cwd %s)" % cwd) if cwd else ""))
    start = time.monotonic()
    proc = subprocess.run(argv, cwd=str(cwd) if cwd else None, env=env,
                          stdout=subprocess.PIPE if capture else None, stderr=subprocess.STDOUT if capture else None,
                          timeout=timeout)
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
