# AGENTS.md — the Auto-Pigeon fork of NetRadiant-custom's q3map2

This checkout is `auto-pigeon-q3map2`, alias **Q3TOOLS**: the Auto-Pigeon maintained fork of
[Garux/netradiant-custom](https://github.com/Garux/netradiant-custom), published as
[auto-pigeon/q3map2](https://github.com/auto-pigeon/q3map2). Only the `q3map2` command-line compiler
is built, tested and released from here; the NetRadiant editor, gamepacks and the other tools stay
upstream's and are never part of a release.

Everything outside `ci/auto-pigeon/`, `.github/workflows/auto-pigeon-release.yml`,
`regression_tests/auto-pigeon/`, this file, `CLAUDE.md`, `TEST-SKILL.md`, `.agent-repo.json`, the
Auto-Pigeon section at the top of `README.md` and the compiler commits that section lists is
upstream's tree. `master` mirrors upstream; `main` is the maintenance branch; the immutable tag
`upstream-base/<sha8>` marks the upstream commit `main` is based on, so the patch series is always
`git log upstream-base/<sha8>..main`.

The workspace root (`../AGENTS.md`) and the shared prompt/handoff workflow bind here as in every
sibling. This repository has no prompt queue of its own: a prompt filed in another queue names it as
a mutation target, and its handoff is written by that queue's tooling.

## Rules that bind every task here

- **Read `TEST-SKILL.md` before selecting, resuming or qualifying tests** — the shared Auto-Pigeon
  testing skill, an identical copy of the canonical one. It is the testing policy here; this
  repository has no rule that runs the whole platform matrix after every commit. The split is:
  a documentation-only change runs the CI policy checks (`ci/auto-pigeon/selftest.py`) and builds
  nothing; a compiler/build/dependency change runs, locally, the Linux build, every regression in
  `regression_tests/auto-pigeon/` and the sanitizers. Automatic CI builds and smoke-checks only.
- **Read `ci/auto-pigeon/TESTING.md` with it.** It places the verification for this fork (operator
  decision, 2026-11-02, superseding older wording): full regressions, sanitizers, geometry sweeps and
  the source rebuild are local wrap-up work; CI runs two bounded smoke cases. A smoke pass does not
  establish full-regression or sanitizer coverage.
- **A published release is immutable.** Never replace a release's assets, move a tag or rewrite
  history. The version and tag are read off `ci/auto-pigeon/plan.py` for the exact commit, never
  guessed; a correction is a new commit and a new tag.
- **Do not touch upstream.** `upstream` is Garux's repository. Updating from it is a merge into
  `main` (README, *Updating from upstream*), never a rebase of our commits and never a force push.
- **A compiler change is narrow, named and tested first.** Each one has a case under
  `regression_tests/auto-pigeon/` that fails on the code before it, a commit of its own, and a line
  in the README's fork section. A geometry assertion states what the source map describes (solid,
  open, drawn), never what an earlier compiler emitted.
- **A known defect stays red.** `regression_tests/auto-pigeon/known-failures.json` lists each case
  that is expected to fail, with its owner and the identity it was observed on. The runner reports
  it as `known_failure`, never as a pass, and a release report counts it separately. A known
  failure that starts passing is reported as `unexpected_pass` and fails the run until the record
  is updated deliberately.
- **Build and run output stays out of the checkout** — a scratch or task-owned directory elsewhere.
- **This is GPL software and stays a separate executable.** No source from here is copied into an
  Auto-Pigeon product repository; products invoke the released `q3map2` program.
- **Nothing private enters this public repository**: no host names or addresses, accounts,
  credentials, private corpus maps, retail game data or machine-specific paths. Fixtures here are
  authored for this repository.

## Where things are

| What | Where |
| --- | --- |
| the compiler | `tools/quake3/q3map2/`, `tools/quake3/common/` |
| upstream's regression maps (visual, manual) | `regression_tests/q3map2/` |
| our regression cases, their maps and assertions | `regression_tests/auto-pigeon/` |
| run every regression case against one `q3map2` | `ci/auto-pigeon/regress.py` |
| the IBSP 46 reader the assertions use | `ci/auto-pigeon/ibsp.py` |
| release plan, version, tag and banner | `ci/auto-pigeon/plan.py` |
| build one target natively (q3map2 only) | `ci/auto-pigeon/build.py` |
| package, accept, source archive, publish | `ci/auto-pigeon/package.py`, `accept.py`, `source_archive.py`, `publish.py` |
| the CI scripts' own self-test and its negative controls | `ci/auto-pigeon/selftest.py`, `test_ci_policy.py` |
| where each test runs: local qualification, light CI | `ci/auto-pigeon/TESTING.md` |
| the one workflow | `.github/workflows/auto-pigeon-release.yml` |

<!-- trail:start -->
## Trail — repo context graph

This repo is indexed in `.trail/cache/`: small linked markdown nodes that explain each
system and carry exact file:line spans, kept in sync with the code through git.

For ANY task here — understanding how something works, finding where code lives,
or scoping a change — get context from the graph before grepping or opening
source files. Re-ask freely (it's cheap) and reuse literal identifiers you
already have (symbol, error string, file name) as the query. New to this repo?
Run `trail map` first — a token-budgeted orientation (dir clusters, hubs,
hotspots), no LLM, no key.

- Run `trail ask "<your question>" --source` → ranked nodes with the relevant
  code spans inlined (each hit's ≤8-line crux by default; `--full` for whole
  definitions when the crux isn't enough). Match the tool to the task shape:
  for understanding or editing, the top node IS the answer — cite its
  `covers:` file:line spans and edit straight from `--source`. For
  exhaustive tasks ("every occurrence / every caller of this pattern"), ranked
  results are top-N, not complete — run `trail grep "<literal>"` instead
  (exhaustive over indexed files, grouped by enclosing symbol), falling back
  to raw `grep -rn` only for unindexed files.
- `trail skeleton <file>` → every definition's signature + span, ~10× cheaper
  than reading the file; use it to skim an API surface.
- `trail callers <symbol>` gives precomputed, exact edges — who calls this.
  Add `--direction out` for what it calls, or `--depth N` to walk
  transitively for the full blast radius. For structural questions, skip
  ranking and use this directly.
- Or browse: `.trail/cache/INDEX.md` lists every node; follow the links.
- Monorepos and folders of multiple repos rank fairly across sub-projects —
  hits carry `[scope/]` labels naming which one they're from. Narrow with
  `trail ask "<task>" --in <scope>/` once you know where you're working.

If a returned span is truncated ("+N more lines"), open the file at that exact
range before finalizing. Only open source files when a node genuinely lacks a
needed detail, and then at the exact file:line the node points to — never
re-read whole files.

After big code changes, refresh the graph with `trail build` (deterministic,
no API key, $0).
<!-- trail:end -->
