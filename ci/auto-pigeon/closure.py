"""What does a program need at run time, and does the unpacked toolchain supply all of it?

One implementation for package.py (which decides what to ship) and accept.py (which decides
whether what was shipped is enough), so the two cannot disagree. ELF is read with readelf/ldd,
Mach-O with otool, PE with the small parser below (no Visual Studio needed on the accepting host).
"""

import os
import pathlib
import re
import struct

from common import Failure, run

# The Linux baseline: glibc and the GCC runtime are the host's, everything else is shipped.
LINUX_SYSTEM = {
    "libc.so.6", "libm.so.6", "libdl.so.2", "libpthread.so.0", "librt.so.1", "libstdc++.so.6", "libgcc_s.so.1",
    "ld-linux-x86-64.so.2", "ld-linux-aarch64.so.1", "linux-vdso.so.1",
}
# Never acceptable from the host on Windows: these are ours to ship app-locally (the MinGW runtime and every
# library q3map2 links).
WINDOWS_NEVER_SYSTEM = re.compile(r"^(libstdc\+\+|libgcc|libwinpthread|libglib|libxml2|libpng|libjpeg|libassimp|zlib|libiconv|libintl|libpcre|liblzma|libzstd|libminizip|libbz2|libdraco|libpugixml)", re.I)
WINDOWS_API_SET = re.compile(r"^(api-ms-win-|ext-ms-)", re.I)
MACOS_SYSTEM_PREFIXES = ("/usr/lib/", "/System/Library/")


# ---------------------------------------------------------------- PE

class PE:
    def __init__(self, path):
        self.path = str(path)
        with open(path, "rb") as f:
            self.data = f.read()
        d = self.data
        if d[:2] != b"MZ":
            raise Failure("%s is not a PE file" % path)
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        if d[pe:pe + 4] != b"PE\0\0":
            raise Failure("%s has no PE signature" % path)
        self.machine, nsections = struct.unpack_from("<HH", d, pe + 4)
        opt_size = struct.unpack_from("<H", d, pe + 20)[0]
        opt = pe + 24
        magic = struct.unpack_from("<H", d, opt)[0]
        self.pe32plus = magic == 0x20B
        self.dirs = opt + (112 if self.pe32plus else 96)
        self.sections = []
        table = opt + opt_size
        for i in range(nsections):
            vsize, vaddr, rsize, rptr = struct.unpack_from("<IIII", d, table + 40 * i + 8)
            self.sections.append((vaddr, max(vsize, rsize), rptr))

    @property
    def architecture(self):
        return {0x8664: "amd64", 0x14C: "x86", 0xAA64: "arm64"}.get(self.machine, hex(self.machine))

    def _off(self, rva):
        for vaddr, size, rptr in self.sections:
            if vaddr <= rva < vaddr + size:
                return rva - vaddr + rptr
        raise Failure("%s: RVA %#x is in no section" % (self.path, rva))

    def _str(self, rva):
        o = self._off(rva)
        return self.data[o:self.data.index(b"\0", o)].decode("ascii", "replace")

    def _dir(self, index):
        return struct.unpack_from("<II", self.data, self.dirs + 8 * index)

    def imports(self):
        """{dll name: [imported symbol names]} for the ordinary and the delay-load import tables."""
        result = {}
        rva, size = self._dir(1)
        if rva:
            o = self._off(rva)
            while True:
                oft, _, _, name, ft = struct.unpack_from("<IIIII", self.data, o)
                if not (oft or name or ft):
                    break
                result.setdefault(self._str(name), []).extend(self._thunks(oft or ft))
                o += 20
        rva, size = self._dir(13)
        if rva:
            o = self._off(rva)
            while True:
                fields = struct.unpack_from("<IIIIIIII", self.data, o)
                if not any(fields):
                    break
                result.setdefault(self._str(fields[1]), []).extend(self._thunks(fields[4]))
                o += 32
        return result

    def _thunks(self, rva):
        names = []
        if not rva:
            return names
        o = self._off(rva)
        width, fmt, flag = (8, "<Q", 1 << 63) if self.pe32plus else (4, "<I", 1 << 31)
        while True:
            value = struct.unpack_from(fmt, self.data, o)[0]
            if not value:
                break
            names.append("#%d" % (value & 0xFFFF) if value & flag else self._str((value & 0x7FFFFFFF) + 2))
            o += width
        return names

    def exports(self):
        rva, size = self._dir(0)
        if not rva:
            return set()
        o = self._off(rva)
        count, names = struct.unpack_from("<I", self.data, o + 24)[0], struct.unpack_from("<I", self.data, o + 32)[0]
        table = self._off(names)
        return {self._str(struct.unpack_from("<I", self.data, table + 4 * i)[0]) for i in range(count)}


def pe_closure(bin_dir, system_root=None):
    """Check every .exe and .dll of bin_dir. Returns a report; raises Failure on a gap."""
    if not pathlib.Path(system_root or os.environ.get("SystemRoot", r"C:\Windows")).is_dir():
        raise Failure("no Windows system directory: the PE closure is only judged on Windows")
    system_root = pathlib.Path(system_root or os.environ.get("SystemRoot", r"C:\Windows"))
    files = {p.name.lower(): p for p in sorted(bin_dir.iterdir()) if p.suffix.lower() in (".exe", ".dll")}
    report, problems = {}, []
    for name, path in files.items():
        pe = PE(path)
        if pe.architecture not in ("amd64",):
            problems.append("%s is %s, not amd64" % (path.name, pe.architecture))
        entry = {"architecture": pe.architecture, "app_local": [], "system": [], "api_sets": []}
        for dll, symbols in sorted(pe.imports().items()):
            low = dll.lower()
            if low in files:
                entry["app_local"].append(dll)
                missing = sorted(set(s for s in symbols if not s.startswith("#")) - PE(files[low]).exports())
                if missing:
                    problems.append("%s imports %d symbol(s) that the shipped %s does not export, e.g. %s" % (
                        path.name, len(missing), dll, missing[0]))
            elif WINDOWS_API_SET.match(low):
                entry["api_sets"].append(dll)
            elif WINDOWS_NEVER_SYSTEM.match(low):
                problems.append("%s needs %s, which is not in the archive and must never come from the host" % (path.name, dll))
            elif (system_root / "System32" / dll).is_file():
                entry["system"].append(dll)
            else:
                problems.append("%s needs %s, which is neither shipped nor a Windows system library" % (path.name, dll))
        report[path.name] = entry
    if problems:
        raise Failure("Windows runtime closure is incomplete:\n  " + "\n  ".join(problems))
    return report


# ---------------------------------------------------------------- ELF

def elf_dynamic(path):
    out = run(["readelf", "-d", "-W", path], capture=True)[1]
    needed = re.findall(r"\(NEEDED\)\s+Shared library: \[([^\]]+)\]", out)
    runpath = re.findall(r"\((?:RPATH|RUNPATH)\)\s+Library (?:rpath|runpath): \[([^\]]*)\]", out)
    soname = re.findall(r"\(SONAME\)\s+Library soname: \[([^\]]+)\]", out)
    return needed, runpath, (soname[0] if soname else None)


def elf_symbol_baseline(paths):
    best = {}
    for path in paths:
        out = run(["readelf", "-V", "-W", path], capture=True)[1]
        for family, version in re.findall(r"\b(GLIBC|GLIBCXX|CXXABI)_(\d+(?:\.\d+)+)\b", out):
            key = tuple(int(x) for x in version.split("."))
            if family not in best or key > best[family][0]:
                best[family] = (key, version)
    return {family: value[1] for family, value in sorted(best.items())}


def elf_closure(bin_dir, programs):
    files = {p.name: p for p in sorted(bin_dir.iterdir()) if p.is_file()}
    for p in bin_dir.iterdir():
        if p.is_symlink():
            raise Failure("%s is a symbolic link; the archive ships real files under their SONAME" % p.name)
    report, problems = {}, []
    todo, seen = list(programs), set()
    while todo:
        name = todo.pop()
        if name in seen:
            continue
        seen.add(name)
        needed, runpath, soname = elf_dynamic(files[name])
        entry = {"needed": needed, "runpath": runpath, "soname": soname, "bundled": [], "system": []}
        if name in programs and runpath != ["$ORIGIN"]:
            problems.append("%s has run path %r, expected exactly $ORIGIN" % (name, runpath))
        if soname and soname != name:
            problems.append("%s is shipped under a name that is not its SONAME %s" % (name, soname))
        for lib in needed:
            if lib in files:
                entry["bundled"].append(lib)
                todo.append(lib)
            elif lib in LINUX_SYSTEM:
                entry["system"].append(lib)
            else:
                problems.append("%s needs %s, which is neither shipped nor part of the glibc/GCC runtime baseline" % (name, lib))
        report[name] = entry
    # Now ask the dynamic loader itself, with nothing but the archive on its search path.
    env = {k: v for k, v in os.environ.items() if k not in ("LD_LIBRARY_PATH", "LD_PRELOAD")}
    for name in programs:
        out = run(["ldd", "-r", files[name]], capture=True, env=env, check=False)[1]
        resolved = {}
        for line in out.splitlines():
            if "not found" in line or "undefined symbol" in line:
                problems.append("%s: %s" % (name, line.strip()))
            m = re.match(r"\s*(\S+) => (\S+) \(", line)
            if m:
                resolved[m.group(1)] = m.group(2)
        for lib, where in resolved.items():
            inside = pathlib.Path(where).resolve().parent == bin_dir.resolve()
            if lib in files and not inside:
                problems.append("%s loads %s from %s instead of the archive" % (name, lib, where))
            if lib not in files and lib not in LINUX_SYSTEM:
                problems.append("%s loads %s from the host (%s)" % (name, lib, where))
        report[name]["resolved"] = resolved
    unused = sorted(n for n in files if n not in seen)
    if problems:
        raise Failure("Linux runtime closure is incomplete:\n  " + "\n  ".join(problems))
    return {"files": report, "not_reached_from_programs": unused,
            "symbol_baseline": elf_symbol_baseline([files[n] for n in sorted(seen)])}


# ---------------------------------------------------------------- Mach-O

def macho_info(path):
    libs_out = run(["otool", "-L", path], capture=True)[1].splitlines()[1:]
    libs = [line.strip().split(" (compatibility")[0] for line in libs_out if line.strip()]
    load = run(["otool", "-l", path], capture=True)[1]
    rpaths = re.findall(r"cmd LC_RPATH\n\s+cmdsize \d+\n\s+path (\S+)", load)
    ident = re.findall(r"cmd LC_ID_DYLIB\n\s+cmdsize \d+\n\s+name (\S+)", load)
    minos = re.findall(r"cmd LC_BUILD_VERSION\n(?:.*\n){0,3}?\s+minos (\S+)", load) or \
        re.findall(r"cmd LC_VERSION_MIN_MACOSX\n\s+cmdsize \d+\n\s+version (\S+)", load)
    archs = run(["lipo", "-archs", path], capture=True)[1].split()
    return {"libraries": libs, "rpaths": rpaths, "id": ident[0] if ident else None,
            "minos": minos[0] if minos else None, "archs": archs}


def macho_closure(bin_dir, programs, arch):
    want = {"amd64": "x86_64", "arm64": "arm64"}[arch]
    files = {p.name: p for p in sorted(bin_dir.iterdir()) if p.is_file()}
    for p in bin_dir.iterdir():
        if p.is_symlink():
            raise Failure("%s is a symbolic link; the archive ships real files" % p.name)
    report, problems = {}, []
    todo, seen = list(programs), set()
    while todo:
        name = todo.pop()
        if name in seen:
            continue
        seen.add(name)
        info = macho_info(files[name])
        if want not in info["archs"]:
            problems.append("%s holds %s, not %s" % (name, info["archs"], want))
        for rpath in info["rpaths"]:
            if not rpath.startswith(("@loader_path", "@executable_path")):
                problems.append("%s carries the build-host run path %s" % (name, rpath))
        if name in programs and not any(r.rstrip("/") in ("@loader_path", "@executable_path") for r in info["rpaths"]):
            problems.append("%s has no @loader_path run path" % name)
        info["bundled"], info["system"] = [], []
        for lib in info["libraries"]:
            if lib == info["id"]:
                continue
            if lib.startswith(MACOS_SYSTEM_PREFIXES):
                info["system"].append(lib)
            elif lib.startswith(("@rpath/", "@loader_path/", "@executable_path/")) and lib.split("/", 1)[1] in files:
                info["bundled"].append(lib)
                todo.append(lib.split("/", 1)[1])
            else:
                problems.append("%s needs %s, which is neither shipped beside it nor a macOS system library" % (name, lib))
        report[name] = info
    if problems:
        raise Failure("macOS runtime closure is incomplete:\n  " + "\n  ".join(problems))
    versions = [tuple(int(x) for x in i["minos"].split(".")) for i in report.values() if i["minos"]]
    return {"files": report, "not_reached_from_programs": sorted(n for n in files if n not in seen),
            "minimum_macos": ".".join(str(x) for x in max(versions)) if versions else None}


def check(platform, bin_dir, programs):
    """programs: file names (with .exe on Windows). Returns the closure report or raises Failure."""
    bin_dir = pathlib.Path(bin_dir)
    if platform.startswith("windows"):
        return {"kind": "pe", "files": pe_closure(bin_dir)}
    if platform.startswith("linux"):
        return dict(kind="elf", **elf_closure(bin_dir, programs))
    return dict(kind="mach-o", **macho_closure(bin_dir, programs, platform.split("-")[1]))


def needed_names(platform, path):
    """The library names one file asks for, in the form they must have inside bin/."""
    if platform.startswith("windows"):
        return sorted(PE(path).imports())
    if platform.startswith("linux"):
        return elf_dynamic(path)[0]
    info = macho_info(path)
    return [lib.split("/", 1)[1] for lib in info["libraries"]
            if lib != info["id"] and lib.startswith(("@rpath/", "@loader_path/", "@executable_path/"))]
