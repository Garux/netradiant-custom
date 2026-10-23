# Auto-Pigeon q3map2 — the maintained fork

This repository, [auto-pigeon/q3map2](https://github.com/auto-pigeon/q3map2), is the Auto-Pigeon
maintained fork of [Garux/netradiant-custom](https://github.com/Garux/netradiant-custom). It exists to
build, test and release **one program: the Quake III map compiler `q3map2`**, with a small number of
named compiler fixes on top of upstream. The NetRadiant editor, its plugins and the gamepacks below are
upstream's and are never part of a release. Everything after the next horizontal rule is upstream's
README, unchanged.

- `main` is the maintenance branch. `master` mirrors upstream. The tag `upstream-base/<sha8>` marks the
  upstream commit `main` is based on (currently `upstream-base/82161339`, upstream `master` of
  2026-09-01), so our patch series is always `git log upstream-base/82161339..main`.
- Releases are prereleases named `auto-pigeon-q3map2-v<Q3MAP_VERSION>-ap.<N>`, `N` being the commit
  count. `q3map2` prints `<version>+g<commit>.upstream.<base>`, which names both our commit and the
  upstream one. Assets are never replaced.

## What this fork changes in the compiler

| Commit | Change | Regression case (fails without it) |
| --- | --- | --- |
| `q3map2: clip a crossing face at the epsilon it was classified with (P2)` | `BuildFaceTree_r` clipped a face it had classified as crossing (more than `ON_EPSILON` = 0.1 on both sides) with `CLIP_EPSILON*2` = 0.2, so a part between 0.1 and 0.2 wide vanished, the whole face went to one side, and a leaf straddling the brush turned opaque and filled room air. It now clips at `ON_EPSILON`. | `R03.cross.0148`, `R06.p4.p08_base` |
| `q3map2: keep a split brush fragment of any positive volume (P1)` | `SplitBrush` discarded a split half smaller than 1 cubic unit: thin rods and walls lost their collision. Only a non-positive volume is discarded now; `-bsp -microvolume <F>` restores a threshold (upstream's was 1). | `R02.rod.split` |

| `q3map2: append BSP entities one by one so libc++ builds it` | A build fix with no change in behaviour: libc++ (Apple clang) cannot compile a vector range insert of `entity_t`. | the macOS builds |
| `q3map2: give the macOS worker threads an 8 MiB stack` | `std::thread` gets 512 KiB on macOS and `-light` needs more than 1 MiB: every `-light` died with SIGBUS. macOS workers are pthreads with 8 MiB (Linux threads already get 8 MiB, Windows ones 4 MiB). | `R01`, `R02`, `R09` with `--full` on macOS |

P1 and P2 belong together: with P1 alone the rod of `R02` is solid but the air above it is filled.

Measured requirements of the released programs: Linux (x86-64 and arm64) glibc 2.38 and `GLIBCXX_3.4.32`
or newer (built on Ubuntu 24.04 with GCC 14); Windows 10 or later (MSYS2 MinGW-w64 GCC); macOS 15.0 or
newer (Apple clang 17 with Homebrew's libraries, which target the build runner's macOS). Each archive's
README states its own measurement.

## Known defects (still failing, kept red)

`regression_tests/auto-pigeon/known-failures.json` is the record; a release lists the same cases.

- `R04.m4.p08_nearby` (M4): a face nearly parallel to a neighbouring face is discarded as lying on its
  plane; 9 drawn points of a pipe bore are missing. No accepted repair yet.
- `R08.slit.0125`: a 0.125-unit opening to the void is closed; the map is reported sealed although
  its source leaks (every Q3Map2 build measured does this for openings of 0.25 or less).

Erratum, release `2.5.17n-ap.3017`: its `release-manifest.json` says
`rebuilt_program_equals_linux_amd64_release: false`. That compared the source rebuild with the shipped
`bin/q3map2`, whose run path packaging rewrites; compared before packaging, the rebuild from the source
archive is byte-identical (sha256 `d4dfbf88…`, the run's `source.json` against the linux-amd64
`build-report.json`). Later releases compare like with like.

## Build `q3map2` only

Upstream's Makefile, asked for the one target. The base needs GCC 14 or newer on Linux.

```sh
# Ubuntu 24.04 (amd64 or arm64)
sudo apt-get install gcc-14 g++-14 make pkg-config libglib2.0-dev libxml2-dev libpng-dev libjpeg-dev libassimp-dev zlib1g-dev
make -j4 BUILD=release DEPENDENCIES_CHECK=off CC=gcc-14 CXX=g++-14 CXXFLAGS=-Wno-deprecated-copy install/q3map2.$(uname -m)
./install/q3map2.$(uname -m) -help
```

```sh
# Windows: an MSYS2 MINGW64 shell
pacman -S --needed git make mingw-w64-x86_64-gcc mingw-w64-x86_64-pkgconf mingw-w64-x86_64-python mingw-w64-x86_64-glib2 \
  mingw-w64-x86_64-libxml2 mingw-w64-x86_64-libpng mingw-w64-x86_64-libjpeg-turbo mingw-w64-x86_64-zlib mingw-w64-x86_64-assimp
make -j4 MAKEFILE_CONF=msys2-Makefile.conf BUILD=release DEPENDENCIES_CHECK=off install/q3map2.exe
```

```sh
# macOS (Homebrew; libxml2 is the SDK's)
brew install glib libpng jpeg-turbo assimp pkgconf
make -j4 OS=Darwin BUILD=release DEPENDENCIES_CHECK=off CC=clang CXX=clang++ MACLIBDIR="$(brew --prefix)/lib" \
  CPPFLAGS_XML="-I$(xcrun --show-sdk-path)/usr/include/libxml2" LIBS_XML=-lxml2 \
  CPPFLAGS_JPEG="-I$(brew --prefix jpeg-turbo)/include" LIBS_JPEG="-L$(brew --prefix jpeg-turbo)/lib -ljpeg" \
  install/q3map2.$(uname -m)
```

The release scripts do the same with the plan's version banner, from a clean copy of the tree, and
then package exactly the libraries `q3map2` loads:

```sh
python3 ci/auto-pigeon/plan.py --sha HEAD --out /tmp/plan.json
python3 ci/auto-pigeon/build.py --platform linux-amd64 --plan /tmp/plan.json --work "/tmp/q3 build" --report /tmp/build-report.json
python3 ci/auto-pigeon/package.py --platform linux-amd64 --plan /tmp/plan.json --build-report /tmp/build-report.json --out /tmp/dist
```

## Run the regression cases

`regression_tests/auto-pigeon/` extends upstream's `regression_tests/q3map2/` (maps plus a README per
directory) with assertions a script checks: which points of each source map are solid, open or drawn
(with shader and texture coordinates), the leak verdict and patch control points. Upstream's own maps
are compile-only cases.

```sh
python3 ci/auto-pigeon/regress.py --q3map2 ./install/q3map2.x86_64 --work "/tmp/q3 regress" --report /tmp/regress.json
python3 ci/auto-pigeon/regress.py --q3map2 ./install/q3map2.x86_64 --work "/tmp/q3 full" --full   # + NORMAL -vis and -light
python3 regression_tests/auto-pigeon/generate.py --check   # the authored maps and cases.json are what generate.py writes
```

The exit status is 0 only when every required case passes and every known failure still fails.

## CI and releases

`.github/workflows/auto-pigeon-release.yml` is the one path. A documentation-only push runs the policy
checks (`ci/auto-pigeon/selftest.py`); a change to the compiler, its build or its tests adds the Linux
build and every regression case; pushing the release tag builds, tests and accepts **linux-amd64,
windows-amd64, macos-amd64 and macos-arm64** (required) and **linux-arm64** (optional) on native
runners, rebuilds the source archive from itself, and publishes only after every required job passed.

```sh
tag=$(python3 ci/auto-pigeon/plan.py --sha HEAD --print-tag)   # the ONE tag this commit may be released as
git tag "$tag" && git push origin "$tag"
gh workflow run auto-pigeon-release.yml -f all_targets=true    # the whole matrix, without publishing
```

## Updating from upstream without losing our fixes

```sh
git fetch upstream
git push origin upstream/master:master                 # keep the mirror
git checkout main && git merge --no-ff upstream/master  # never rebase or force-push main
base=$(git rev-parse --short=8 upstream/master); git tag "upstream-base/$base" upstream/master
# rebuild, then run every regression case; first on upstream alone, to see whether it now fixes a case itself:
python3 ci/auto-pigeon/regress.py --q3map2 <upstream-only q3map2> --work "/tmp/upstream only"
python3 ci/auto-pigeon/regress.py --q3map2 ./install/q3map2.x86_64 --work "/tmp/merged"
git push origin main "upstream-base/$base"
```

If upstream alone now passes a case one of our commits repairs, revert our commit instead of stacking a
second fix, and keep the regression case. A conflict in a patched function is resolved by re-applying
the named change and proving its case red-then-green again.

## Licences

`q3map2` is GPL-2.0-or-later (its source headers); the tree also carries BSD- and LGPL-licensed parts,
as `LICENSE` explains. Each release ships the source archive of its exact commit, the licence texts,
and, per bundled library, its licence and the distribution package and version it came from.

---

NetRadiant-custom
=================

The open-source, cross-platform level editor for id Tech based games.

NetRadiant-custom is a fork of NetRadiant (GtkRadiant 1.4&rarr;massive rewrite&rarr;1.5&rarr;NetRadiant&rarr;this)

---
![screenshot](/../readme_files/radDarkShot.png?raw=true)
---

## Downloads

Ready-to-use packages are available in the [Releases section](/../../releases).

## Supported games

Main focus is on Quake, Quake3 and Quake Live.

Though other normally supported games should work too. Releases include configs for the following games: Alien Arena, Darkplaces, Doom 3, Doombringer, Wolfenstein: Enemy Territory, Heretic II, Half-Life, Jedi Knight Jedi Academy, Jedi Knight II: Jedi Outcast, Kingpin, Neverball, Nexuiz, Open Arena, Project::OSiRiON, Prey, Quake II, Q3Rally, Quake 4, Quetoo, Smokin' Guns, Soldier of Fortune II - Double Helix, Star Trek Voyager : Elite Force, Tremulous, Turtle Arena, UFO:Alien Invasion, Unreal Arena, Unvanquished, Urban Terror, Warfork, Warsow, Return To Castle Wolfenstein, World of Padman, Xonotic, ZEQ2 Lite.

## Features

Development is focused on smoothing and tweaking editing process.

#### Random feature highlights

* WASD camera binds
* Fully supported editing in 3D view (brush and entity creation, all manipulating tools)
* Uniform merge algorithm, merging selected brushes, components and clipper points
* Free and robust vertex editing, also providing abilities to remove and insert vertices
* UV Tool (edits texture alignment of selected face or patch)
* Autocaulk
* Model browser
* Brush faces extrusion
* Left mouse button click tunnel selector, paint selector
* Numerous mouse shortcuts (see help->General->Mouse Shortcuts)
* Focus camera on selected (Tab)
* Snapped modes of manipulators
* Draggable renderable transform origin for manipulators
* Quick vertices drag / brush faces shear shortcut
* Simple shader editor
* Texture painting by drag
* Seamless brush face<->face, patch<->face texture paste
* Customizable keyboard shortcuts
* Customizable GUI themes, fonts
* MeshTex plugin
* Patch thicken
* All patch prefabs are created aligned to active projection
* Filters toolbar with extra functions on right mouse button click
* Viewports zoom in to mouse pointer
* \'all Supported formats\' default option in open dialogs
* Opening *.map, sent via cmd line (can assign *.map files in OS to be opened with radiant)
* Texture browser: show alpha transparency option
* Texture browser: search in directories and tags trees
* Texture browser: search in currently shown textures
* CSG Tool (aka shell modifier)
* Working region compilations (build a map with region enabled = compile regioned part only)
* QE tool in a component mode: perform drag w/o hitting any handle too
* Map info dialog: + Total patches, Ingame entities, Group entities, Ingame group entities counts
* Connected entities selector/walker
* Build->customize: list available build variables
* 50x faster light radius rendering
* Light power is adjustable by mouse drag
* Anisotropic textures filtering
* Optional MSAA in viewports
* New very fast entity names rendering system
* Support \'stupid quake bug\'
* Arbitrary texture projections for brushes and curves
* Fully working texture lock, supporting any affine transformation
* Texture locking during vertex and edge manipulations
* Brush resize (QE tool): reduce selected faces amount to most wanted ones
* Support brush formats, as toggleable preference: Axial projection, Brush primitives, Valve 220
* Autodetect brush type on map opening
* Automatic AP, BP and Valve220 brush types conversion on map Import and Paste
* New bbox styled manipulator, allowing any affine transform (move, rotate, scale, skew)
* rendering of Q3 shader based skyboxes
* Incredible number of fixes and options


#### Q3Map2:

* q3map_remapshader remaps anything fine, on all stages
* Automatic map packager (complete Q3 support)
* Report full / full pk3 path on file syntax errors
* Allowed simultaneous samples+filter use, makes sense
* -brightness 0..alot, def 1: mimics q3map_lightmapBrightness globally
* -contrast -255..255, def 0: lighting contrast
* -saturation light option
* -bouncecolorratio 0..1 (ratio of colorizing light sample by texture)
* -nolm - no lightmaps
* -novertex works, (0..1) sets globally
* -vertexscale
* New area lights backsplash algorithm (utilizing area lights instead of point ones)
* -backsplash (float)scale (float)distance: adjust area lights globally (real area lights have no backsplash)
* New slightly less careful, but much faster lightmaps packing algorithm (allocating... process)
* -extlmhacksize zero effort external lightmaps for Q3
* Valve220 mapformat autodetection and support
* Consistent brush content deduction with mixed face parameters
* Model shaders paths deduction
* Fixed model autoclip, added 20 new clipping modes
* Support negative misc_model scale
* Assimp model loading library (40+ formats)
* -json bsp export/import
* -mergebsp injects one bsp to another
* No shaderlist.txt mode: load all shaders

###### see changelog-custom.txt for more

## [COMPILING](/COMPILING)
