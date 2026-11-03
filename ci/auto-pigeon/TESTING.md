# Agent-local qualification and light CI

Operator decision: 2026-11-02. This fork-specific policy controls WHERE the verification described by
the shared `TEST-SKILL.md` runs; older wording that required every regression and the sanitizers in CI
is superseded for this repository. It does not change release or tag immutability, and it is not a
shared-policy change: the other repositories' `TEST-SKILL.md` copies are untouched.

## Before the agent wraps up

Use small red-capable reproducers while repairing a defect. Freeze a coherent candidate, then run once,
locally, what the change affects:

| Change | Local qualification |
| --- | --- |
| documentation only | `selftest.py`; nothing is built |
| CI scripts or workflow | `selftest.py`, and a real build through the changed path |
| compiler, build or dependency | full regressions, sanitizers, full archive acceptance, source rebuild |

`build.py` defaults to full qualification. With task-owned paths outside the checkout:

```sh
python3 ci/auto-pigeon/selftest.py
python3 ci/auto-pigeon/plan.py --sha HEAD --out ../plan.json
python3 ci/auto-pigeon/build.py --platform linux-amd64 --plan ../plan.json --work "../q3 build" --report ../build-report.json
python3 ci/auto-pigeon/build.py --platform linux-amd64 --sanitize --plan ../plan.json --work "../q3 asan" --report ../asan-report.json
python3 ci/auto-pigeon/package.py --platform linux-amd64 --plan ../plan.json --build-report ../build-report.json --out ../dist
python3 ci/auto-pigeon/accept.py --platform linux-amd64 --archive ../dist/<asset>.zip --package-json ../dist/<asset>.package.json \
    --plan ../plan.json --work "../q3 accept" --report ../acceptance.json
```

For the source archive, write it with `source_archive.py`, unpack it into a fresh directory, run the
`build.py` above from there with `--plan ci/auto-pigeon/release-plan.json`, and record that with
`source_archive.py --mark-rebuilt <that report>`. Only a full report can mark it; a smoke report is
refused.

The sanitizer build is `-O1 -fsanitize=address,undefined` and halts on the first report. It covers the
default allocation paths: q3map2 allocates with the C++ allocators, so ASan sees them, and windings use
`winding_t`, a standard container. Say so if a repair depends on a path this does not reach.

Record the exact candidate, compiler and dependency identities, commands, exit codes, executed, failed,
known-failure and skipped counts, durations and durable report paths in the canonical handoff.
Distinguish fresh runs from reused evidence. A changed relevant input invalidates what depends on it; a
prose-only commit does not by itself invalidate an unchanged compiler binary. When one stage fails, fix
the cause and repeat the affected stages, keeping the failed attempt. Missing required local evidence
stays incomplete even when CI is green.

A Linux run does not establish Windows, macOS or arm64. Native coverage a platform-sensitive change needs
runs on an available native host; report unavailable coverage as exactly that. CI builds and smoke-checks
those targets on a release tag, which is real but is not a full regression run on them.

## What automatic CI does

- `selftest.py`: script, generator, workflow and publication contracts, including the negative controls in
  `test_ci_policy.py`, on every push and pull request.
- A change to the compiler, build or tests (`scope.py`): the Linux build and the smoke check.
- A release tag, or `workflow_dispatch` with `all_targets`: native builds of every declared target,
  packaging, architecture and dependency closure, manifests and provenance, then, from the extracted archive
  on a fresh runner, integrity, relocation, the clean-environment banner and the smoke check; the source
  archive is written and is not rebuilt.

The smoke check is `SMOKE_CASES` in `common.py`, run by `regress.py --smoke`: `R01.room`, a tiny sealed room
through BSP, NORMAL VIS and LIGHT with its solid, open and drawn probes; and `R07.open.1`, a genuine opening
that must be reported as a leak and write a pointfile. Each tool invocation has `SMOKE_TOOL_SECONDS` (60 s),
and the acceptance job has ten minutes. Toolchain compilation is separate from that budget.

CI runs no full regression set, sanitizer, broad geometry sweep or source rebuild, on any event. Do not put
those back under another job or event name; `selftest.py` refuses the known spellings. A heavier CI diagnostic
needs an explicit operator request.

`common.validated_regress` is the one gate, used by build, packaging, acceptance, publication and source
rebuild marking. It refuses a missing report, an empty case set, a partial or unlabelled run, a skipped or
timed-out stage, a smoke leak verdict the wrong way round, and swapped smoke cases. Reports, the archive's
README, the manifest and the release notes label `smoke` and `full`, and notes state what did not run. Never
describe a smoke pass as a full-regression or sanitizer pass. Local handoff evidence is not uploaded to or
attested by the release workflow. `known-failures.json` still means what it meant: a smoke run does not
execute those cases, and the notes say so.
