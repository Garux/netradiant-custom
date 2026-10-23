#!/usr/bin/env python3
"""Stage one built q3map2 as a relocatable tool directory and write its ZIP.

    python3 ci/auto-pigeon/package.py --platform linux-amd64 --plan plan.json --build-report build-report.json \
        --out <dist dir> [--dll-dir <MINGW64 bin, Windows only>]

The archive is `<stem>/bin/q3map2[.exe]` with every library it loads beside it, `licenses/`, a
README, `build-info.json` and `MANIFEST.json`. Libraries are not globbed: each one is there because
the program, or a library it ships, asks for it by name, and it is copied once from the place the
build linked it from. The program finds them through its own run path (`$ORIGIN` on Linux,
`@loader_path` on macOS) or, on Windows, because they are beside it. Each library's licence comes
from the package it was installed from, with the package name and version recorded so its source
can be fetched from that distribution.
"""

import argparse
import json
import os
import pathlib
import re
import shutil
import stat
import sys
import zipfile

import closure
from common import (HERE, LABELS, PLATFORMS, REPO, Failure, exe, load_json, log, require_native, run, sha256_file,
                    write_json)

REPO_LICENSES = {"LICENSE.txt": "LICENSE", "GPL.txt": "GPL", "LGPL.txt": "LGPL"}
MACOS_SYSTEM_PREFIXES = closure.MACOS_SYSTEM_PREFIXES


# ---------------------------------------------------------------- collecting the libraries

def linux_libraries(program):
    """{needed name: resolved path} for the program's whole load closure, as the dynamic loader resolves it."""
    out = run(["ldd", program], capture=True)[1]
    found = {}
    for line in out.splitlines():
        m = re.match(r"\s*(\S+) => (\S+) \(", line)
        if m and m.group(1) not in closure.LINUX_SYSTEM:
            found[m.group(1)] = os.path.realpath(m.group(2))
        elif "not found" in line:
            raise Failure("the built program cannot resolve %s" % line.strip())
    return found


def windows_libraries(program, dll_dir):
    dll_dir = pathlib.Path(dll_dir)
    beside = {p.name.lower(): p for p in dll_dir.iterdir() if p.suffix.lower() == ".dll"}
    found, todo = {}, [pathlib.Path(program)]
    while todo:
        current = todo.pop()
        for name in closure.PE(current).imports():
            low = name.lower()
            if low in found or closure.WINDOWS_API_SET.match(low):
                continue
            if low in beside:
                found[low] = str(beside[low])
                todo.append(beside[low])
            elif closure.WINDOWS_NEVER_SYSTEM.match(low):
                raise Failure("%s needs %s, which %s does not hold" % (current.name, name, dll_dir))
            elif not (pathlib.Path(os.environ.get("SystemRoot", r"C:\Windows")) / "System32" / name).is_file():
                raise Failure("%s needs %s, which is neither in %s nor a Windows system library" % (current.name, name, dll_dir))
    return {pathlib.Path(p).name: p for p in found.values()}


def macos_libraries(program):
    found, todo = {}, [str(program)]
    while todo:
        current = todo.pop()
        for lib in closure.macho_info(current)["libraries"]:
            if lib.startswith(MACOS_SYSTEM_PREFIXES) or lib.startswith(("@rpath/", "@loader_path/", "@executable_path/")):
                if lib.startswith("@") and pathlib.PurePath(lib).name not in found:
                    raise Failure("%s names %s by run path; this build expects absolute Homebrew install names" % (current, lib))
                continue
            name = pathlib.PurePath(lib).name
            if name not in found:
                found[name] = os.path.realpath(lib)
                todo.append(found[name])
    return found


def fix_macho(path, names, is_program):
    """Install names -> @rpath/<name>, one @loader_path run path, then an ad hoc signature (arm64 refuses unsigned)."""
    info = closure.macho_info(path)
    for lib in info["libraries"]:
        name = pathlib.PurePath(lib).name
        if name in names and not lib.startswith("@rpath/"):
            run(["install_name_tool", "-change", lib, "@rpath/" + name, path])
    if not is_program:
        run(["install_name_tool", "-id", "@rpath/" + path.name, path])
    for rpath in info["rpaths"]:
        run(["install_name_tool", "-delete_rpath", rpath, path])
    run(["install_name_tool", "-add_rpath", "@loader_path", path])
    run(["codesign", "--force", "--sign", "-", path])


# ---------------------------------------------------------------- where each library came from

def provenance(platform, path):
    """{'package', 'version', 'license', 'license_files': [paths], 'source'} for one bundled library."""
    if platform.startswith("linux"):
        code, out, _ = run(["dpkg", "-S", path], check=False, capture=True)
        if code != 0:
            code, out, _ = run(["dpkg", "-S", path.replace("/usr/lib/", "/lib/", 1)], check=False, capture=True)
        if code != 0:
            raise Failure("no Debian package owns %s" % path)
        pkg = out.split(":", 1)[0].strip()
        version = run(["dpkg-query", "-W", "-f=${Version}", pkg], capture=True)[1].strip()
        source = run(["dpkg-query", "-W", "-f=${source:Package}", pkg], capture=True)[1].strip() or pkg
        doc = pathlib.Path("/usr/share/doc") / pkg / "copyright"
        return {"package": pkg, "version": version, "license_files": [str(doc)] if doc.is_file() else [],
                "source": "Ubuntu source package %s %s (apt-get source %s=%s)" % (source, version, source, version)}
    if platform.startswith("windows"):
        unix = run(["cygpath", "-u", path], capture=True)[1].strip()
        out = run(["pacman", "-Qo", unix], capture=True)[1]
        m = re.search(r"is owned by (\S+) (\S+)", out)
        if not m:
            raise Failure("no MSYS2 package owns %s" % path)
        pkg, version = m.group(1), m.group(2)
        info = run(["pacman", "-Qi", pkg], capture=True)[1]
        lic = re.search(r"^Licenses\s*:\s*(.+)$", info, re.M)
        base = re.search(r"^Base Package\s*:\s*(\S+)$", info, re.M)
        root = pathlib.Path(run(["cygpath", "-w", "/mingw64/share/licenses"], capture=True)[1].strip())
        short = (base.group(1) if base else pkg).replace("mingw-w64-", "")
        files = []
        for candidate in (root / short, root / pkg.replace("mingw-w64-x86_64-", "")):
            if candidate.is_dir():
                files += [str(p) for p in sorted(candidate.rglob("*")) if p.is_file()]
        return {"package": pkg, "version": version, "license": lic.group(1).strip() if lic else None, "license_files": files,
                "source": "MSYS2 package %s %s (https://github.com/msys2/MINGW-packages, %s)" % (pkg, version, base.group(1) if base else pkg)}
    m = re.search(r"/Cellar/([^/]+)/([^/]+)/", path)
    if not m:
        raise Failure("%s is not inside a Homebrew Cellar" % path)
    formula, version = m.group(1), m.group(2)
    keg = pathlib.Path(path[:m.end()])
    licence_name = re.compile(r"(LICEN[CS]E|COPYING|COPYRIGHT|NOTICE)", re.I)
    files = [str(p) for p in sorted(keg.iterdir()) if p.is_file() and licence_name.match(p.name)]
    for docs in (keg / "share" / "doc", keg / "share" / "licenses"):
        if not files and docs.is_dir():
            files = [str(p) for p in sorted(docs.rglob("*")) if p.is_file() and licence_name.match(p.name)][:8]
    info = json.loads(run(["brew", "info", "--json=v2", formula], capture=True)[1])["formulae"][0]
    licence = info.get("license")
    source_url = ((info.get("urls") or {}).get("stable") or {}).get("url")
    return {"package": formula, "version": version, "license": licence, "license_files": files, "source_url": source_url,
            "source": "Homebrew formula %s %s, built from %s (https://formulae.brew.sh/formula/%s)" % (formula, version, source_url, formula)}


# ---------------------------------------------------------------- the archive text

def requirements(platform, report):
    if platform.startswith("linux"):
        b = report.get("symbol_baseline", {})
        arch = "x86-64" if platform.endswith("amd64") else "arm64 (aarch64)"
        return ("%s Linux with glibc **%s** or newer and a libstdc++ providing `GLIBCXX_%s` (measured from the shipped files; "
                "built on Ubuntu 24.04 with GCC 14). Every other library is in `bin/` and is found through the program's "
                "`$ORIGIN` run path; `LD_LIBRARY_PATH` is not needed." % (arch, b.get("GLIBC", "?"), b.get("GLIBCXX", "?")))
    if platform.startswith("windows"):
        return ("64-bit Windows 10 or later. The MinGW-w64 runtime and every library q3map2 loads are in `bin\\`; nothing "
                "has to be installed.")
    arch = "Intel (x86_64)" if platform == "macos-amd64" else "Apple silicon (arm64)"
    return ("%s Mac running macOS **%s** or newer (the highest minimum any shipped file declares, measured with `otool`). "
            "Not signed or notarised: after downloading with a browser, clear the quarantine flag with "
            "`xattr -dr com.apple.quarantine <folder>`." % (arch, report.get("minimum_macos") or "unknown"))


def usage(platform, stem):
    if platform.startswith("windows"):
        return ("```powershell\n$q3 = \"C:\\Tools\\%s\\bin\\q3map2.exe\"\n"
                "& $q3 -bsp -game quake3 -fs_basepath \"C:\\Games\\Quake III Arena\" -meta \"C:\\Maps\\my map.map\"\n"
                "& $q3 -vis -game quake3 -fs_basepath \"C:\\Games\\Quake III Arena\" \"C:\\Maps\\my map.map\"\n"
                "& $q3 -light -game quake3 -fs_basepath \"C:\\Games\\Quake III Arena\" -fast \"C:\\Maps\\my map.map\"\n```" % stem)
    return ("```sh\nq3=\"$HOME/tools/%s/bin/q3map2\"\n"
            "\"$q3\" -bsp -game quake3 -fs_basepath \"$HOME/games/quake3\" -meta \"$HOME/maps/my map.map\"\n"
            "\"$q3\" -vis -game quake3 -fs_basepath \"$HOME/games/quake3\" \"$HOME/maps/my map.map\"\n"
            "\"$q3\" -light -game quake3 -fs_basepath \"$HOME/games/quake3\" -fast \"$HOME/maps/my map.map\"\n```" % stem)


README = """# Auto-Pigeon q3map2 @VERSION@ — @LABEL@

The Quake III map compiler `q3map2` from the Auto-Pigeon maintained fork of NetRadiant-custom,
built for @LABEL@ from commit `@COMMIT@` of @FORK@ (tag `@TAG@`).
It prints `@BANNER@` as its version.

This is not an upstream release. Upstream is @UPSTREAM@; this build is upstream commit
`@BASE@` plus the Auto-Pigeon changes listed in `build-info.json` (`patch_series`), which upstream
has not reviewed. Only `q3map2` is here: no editor, no gamepacks, no game data.

## Requirements

@REQUIREMENTS@

## Use

Unpack anywhere (a path with spaces is fine) and run `bin/@PROGRAM@` by its path. It needs your own
game installation through `-fs_basepath`; none is included.

@USAGE@

## Known defects in this release

Qualified against its regression cases (`regression_tests/auto-pigeon` in the source), with these
cases still FAILING and owned by later work — this is not a claim that every map compiles correctly:

@KNOWN@

## Licences

`q3map2` is free software under the GNU General Public License, version 2 or (at your option) any
later version (its source headers; the tree's `LICENSE`, `GPL` and `LGPL` are in `licenses/`). The
complete corresponding source is `@SOURCE_ASSET@`, published beside this archive. Each bundled
library's licence is in `licenses/` under its package name; `build-info.json` names the package,
version and where its source comes from. SHA-256 digests are integrity checks, not signatures.
"""


def licences_dir_notice(pkg, entry, licence, bundled_text):
    lines = ["%s %s (Homebrew formula), shipped as %s." % (pkg, entry["version"], ", ".join(entry["libraries"])), "",
             "Licence (as the formula declares it, SPDX): %s" % licence,
             ("The full text of that licence is %s in this directory." % bundled_text) if bundled_text else
             "The formula's package carries no licence file; the licence text is in the source below.",
             "", "Corresponding source: %s" % entry.get("source_url"), "Formula: https://formulae.brew.sh/formula/%s" % pkg]
    return "\n".join(lines) + "\n"


def write_zip(root, zip_path, timestamp):
    """Fixed member order, times and modes. The program bytes are whatever the compiler wrote."""
    import time
    date = time.gmtime(max(timestamp, 315532800))[:6]
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as z:
        def add_dir(rel):
            info = zipfile.ZipInfo(rel + "/", date)
            info.create_system = 3
            info.external_attr = ((stat.S_IFDIR | 0o755) << 16) | 0x10
            z.writestr(info, b"")
        add_dir(root.name)
        for p in sorted(root.rglob("*")):
            rel = root.name + "/" + p.relative_to(root).as_posix()
            if p.is_symlink():
                raise Failure("refusing to archive the symbolic link %s" % rel)
            if p.is_dir():
                add_dir(rel)
                continue
            mode = 0o755 if p.parent.name == "bin" and p.name in ("q3map2", "q3map2.exe") else 0o644
            info = zipfile.ZipInfo(rel, date)
            info.create_system = 3
            info.compress_type = zipfile.ZIP_DEFLATED
            info.external_attr = (stat.S_IFREG | mode) << 16
            z.writestr(info, p.read_bytes(), compresslevel=9)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--platform", required=True, choices=PLATFORMS)
    ap.add_argument("--plan", required=True)
    ap.add_argument("--build-report", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--dll-dir", help="Windows: the MINGW64 bin directory the build linked against")
    args = ap.parse_args()
    platform = args.platform
    require_native(platform)
    plan, report = load_json(args.plan), load_json(args.build_report)
    for key in ("commit", "version", "banner"):
        if report[key] != plan[key]:
            raise Failure("build report %s=%r does not match the plan's %r" % (key, report[key], plan[key]))
    if report["platform"] != platform:
        raise Failure("the build report is for %s" % report["platform"])
    if report["tracked_files_modified"]:
        raise Failure("the build is from a modified tree: %s" % report["tracked_files_modified"])
    if not report["regress"] or report["regress"]["exit"] != 0:
        raise Failure("the build's regressions did not pass; not packaging it")
    out = pathlib.Path(args.out).resolve()
    stem = plan["assets"][platform][:-len(".zip")]
    root = out / stem
    if root.exists():
        shutil.rmtree(root)
    bin_dir = root / "bin"
    bin_dir.mkdir(parents=True)
    program = bin_dir / exe(platform)
    shutil.copyfile(report["program"], program)
    os.chmod(program, 0o755)

    if platform.startswith("linux"):
        found = linux_libraries(report["program"])
    elif platform.startswith("windows"):
        if not args.dll_dir:
            raise Failure("--dll-dir is required on Windows")
        found = windows_libraries(report["program"], args.dll_dir)
    else:
        found = macos_libraries(report["program"])
    libraries = []
    for name, path in sorted(found.items()):
        shutil.copyfile(path, bin_dir / name)
        os.chmod(bin_dir / name, 0o644)
        libraries.append({"file": name, "source_sha256": sha256_file(path), "from": path})
    if platform.startswith("linux"):
        for p in [program] + [bin_dir / lib["file"] for lib in libraries]:
            run(["patchelf", "--set-rpath", "$ORIGIN", p])
    elif platform.startswith("macos"):
        names = set(found)
        fix_macho(program, names, True)
        for lib in libraries:
            os.chmod(bin_dir / lib["file"], 0o644 | stat.S_IWUSR)
            fix_macho(bin_dir / lib["file"], names, False)

    licenses = root / "licenses"
    licenses.mkdir()
    for name, rel in REPO_LICENSES.items():
        shutil.copyfile(REPO / rel, licenses / name)
    packages = {}
    for lib in libraries:
        prov = provenance(platform, lib["from"])
        lib["package"], lib["package_version"] = prov["package"], prov["version"]
        entry = packages.setdefault(prov["package"], dict(prov, libraries=[]))
        entry["libraries"].append(lib["file"])
    for pkg, entry in sorted(packages.items()):
        copied = []
        for i, path in enumerate(entry["license_files"]):
            target = licenses / ("%s%s.txt" % (pkg, "" if i == 0 else "-%s" % pathlib.Path(path).name))
            shutil.copyfile(path, target)
            copied.append(target.name)
        if not copied:
            # The keg carries no licence file (Homebrew's glib does not): write the formula's declared licence and its
            # exact source, and name the licence text this archive already holds when it is that text.
            licence = entry.get("license")
            if not licence or not platform.startswith("macos"):
                raise Failure("no licence text for the package %s (libraries %s)" % (pkg, entry["libraries"]))
            text_for = {"LGPL-2.1-or-later": "LGPL.txt", "LGPL-2.1-only": "LGPL.txt", "GPL-2.0-or-later": "GPL.txt"}
            notice = licences_dir_notice(pkg, entry, licence, text_for.get(licence))
            (licenses / ("%s.txt" % pkg)).write_text(notice, encoding="utf-8", newline="\n")
            copied.append("%s.txt" % pkg)
        entry["license_files"] = copied
    for lib in libraries:
        lib.pop("from")

    closure_report = closure.check(platform, bin_dir, [program.name])
    if closure_report.get("not_reached_from_programs"):
        raise Failure("bin/ holds files the program does not reach: %s" % closure_report["not_reached_from_programs"])

    known = load_json(REPO / "regression_tests" / "auto-pigeon" / "known-failures.json")["cases"]
    known_text = "\n".join("- `%s` (%s): %s. Owner: %s." % (cid, k["mechanism"], k["failing"], k["owner"])
                           for cid, k in sorted(known.items())) or "- none recorded"
    text = README
    for key, value in {"VERSION": plan["version"], "LABEL": LABELS[platform], "COMMIT": plan["commit"],
                       "FORK": plan["fork_repository"], "TAG": plan["tag"], "BANNER": plan["banner"],
                       "UPSTREAM": plan["upstream"]["repository"], "BASE": plan["upstream"]["base_commit"],
                       "REQUIREMENTS": requirements(platform, closure_report), "PROGRAM": program.name,
                       "USAGE": usage(platform, stem), "KNOWN": known_text, "SOURCE_ASSET": plan["source_asset"]}.items():
        text = text.replace("@%s@" % key, value)
    (root / "README.md").write_text(text, encoding="utf-8", newline="\n")

    build_info = {
        "schema": "auto-pigeon-q3map2-build-info/1", "product": "q3map2 from the Auto-Pigeon fork of NetRadiant-custom",
        "release": {"version": plan["version"], "tag": plan["tag"], "asset": plan["assets"][platform]},
        "fork": {"repository": plan["fork_repository"], "commit": plan["commit"], "commit_count": plan["commit_count"]},
        "upstream": plan["upstream"], "patch_series": plan["patch_series"], "version_banner": plan["banner"],
        "target": platform, "signed": False, "program": {"file": "bin/" + program.name, "sha256": sha256_file(program),
                                                        "built_sha256": report["program_sha256"]},
        "build": {k: report[k] for k in ("make", "build_seconds", "source_date_epoch", "toolchain")},
        "regressions_on_the_build_tree": report["regress"],
        "known_failures": known,
        "runtime_libraries": libraries, "library_packages": packages, "runtime_closure": closure_report,
        "modified_after_build": ("run path set to $ORIGIN with patchelf" if platform.startswith("linux") else
                                 "install names -> @rpath, run path @loader_path, ad hoc signature" if platform.startswith("macos")
                                 else "none"),
    }
    scrub_from = str(bin_dir)
    build_info = json_scrub(build_info, scrub_from)
    write_json(root / "build-info.json", build_info)

    files = []
    for p in sorted(root.rglob("*")):
        if p.is_file():
            rel = p.relative_to(root).as_posix()
            files.append({"path": rel, "size": p.stat().st_size, "sha256": sha256_file(p),
                          "mode": "0755" if rel == "bin/" + program.name else "0644"})
    write_json(root / "MANIFEST.json", {"schema": "auto-pigeon-q3map2-archive-manifest/1", "root": stem, "platform": platform,
                                        "commit": plan["commit"], "version": plan["version"], "banner": plan["banner"],
                                        "program": "bin/" + program.name, "note": "every file of the archive except this one",
                                        "files": files})
    zip_path = out / plan["assets"][platform]
    write_zip(root, zip_path, plan["commit_time"])
    package = {"schema": "auto-pigeon-q3map2-package/1", "asset": zip_path.name, "size": zip_path.stat().st_size,
               "sha256": sha256_file(zip_path), "platform": platform, "commit": plan["commit"], "version": plan["version"],
               "banner": plan["banner"], "program_sha256": sha256_file(program), "built_program_sha256": report["program_sha256"],
               "manifest_sha256": sha256_file(root / "MANIFEST.json"),
               "members": len(files) + 1, "regress": report["regress"]}
    write_json(out / (stem + ".package.json"), package)
    shutil.copyfile(args.build_report, out / (stem + ".build-report.json"))
    log("packaged %s  %d bytes  sha256 %s  (%d libraries)" % (zip_path.name, package["size"], package["sha256"], len(libraries)))


def json_scrub(obj, prefix):
    """No build-host path in a published file: the staged bin directory becomes <archive>/bin."""
    if isinstance(obj, dict):
        return {k: json_scrub(v, prefix) for k, v in obj.items()}
    if isinstance(obj, list):
        return [json_scrub(v, prefix) for v in obj]
    if isinstance(obj, str):
        return obj.replace(prefix, "<archive>/bin")
    return obj


if __name__ == "__main__":
    try:
        main()
    except Failure as e:
        sys.exit("package: " + str(e))
