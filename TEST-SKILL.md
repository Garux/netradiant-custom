---
name: auto-pigeon-testing
description: Plan and execute bounded, change-driven tests for Auto-Pigeon applications, contracts, compiler forks and distributed workers. Use before selecting tests, diagnosing defects, resuming matrices, reviewing test evidence or qualifying a release. Preserve browser and native-engine acceptance while reusing valid builds and retaining unsuccessful results honestly.
---

# Auto-Pigeon testing

Policy revision: 2026-10-22. Treat this file as the repository's portable testing skill.

## 1. Establish the task and the evidence

Read the current prompt, FINAL handoff, AGENTS.md and the routed owner documentation. Read DESIGN and UI-DESIGN/UX-DESIGN before changing UX. Pin the task-start commit and dirty-path inventory in each owner. Inspect the changed functions and their callers; identify shared Q1/Q2/Q3 behavior. Use existing test commands, fixtures, compiler contracts, browser drivers and result records. Do not introduce another testing framework or reread every historical report.

Write a short test plan BEFORE expensive execution: changed behavior; affected interfaces/games/platforms; exact selected test/case IDs; existing evidence to reuse; invalidation reasons; negative controls; time/memory budget; required final browser/engine observations; and escalation conditions. Update the plan when scope changes. Report counts for compile, reread, reuse, retained failure and pending work. Do not start a full matrix merely because its command is available.

Choose pre-authorized interfaces from the prompt and existing suites: editor tool/public geometry operations, export/import/conversion, application HTTP/CLI, profile resolution and job execution, compiler CLI/BSP readers, scheduler/result contracts, and native game loading/collision. Explain an interface's limits. Ordinary test selection within this scope needs no extra user confirmation. Ask only when a real product requirement, access or destructive operation is unresolved.

## 2. Use three levels of verification

| Level | Trigger | Work |
| --- | --- | --- |
| Repair loop | A hypothesis or coherent code change | Smallest red-capable reproducer, relevant unit/integration tests, original failure and independent negative controls. Prefer BSP-only for drawing/collision diagnosis. |
| Affected feature acceptance | Candidate behavior works | Selected dependent cases in relevant games, save/reopen and export routes, required live browser flow, bounded normal VIS/LIGHT and exact engine inspection for repaired geometry. |
| Integration/release qualification | Frozen release candidate or documented shared-boundary change | Required full owner suites once for that candidate, required platform CI, representative end-to-end acceptance, and a broader matrix only where the changed behavior warrants it. |

Expand when a focused test exposes an unexpected regression, shared code affects another game, contract/export semantics change, a platform-sensitive compiler change needs native coverage, or the explicit release gate requires it. State the reason and select the next smallest useful set. Do not downgrade mandatory acceptance, turn fast VIS into final VIS, or let a small test replace an unobserved user journey.

## 3. Diagnose with a tight loop

Capture the original input, symptom, hashes, exact command and failed assertion. Use a small deterministic reproducer that exercises that symptom; keep the original scenario for final confirmation. Minimize only while preserving the defect, bounded by a declared attempt/time budget. Compare candidates against independent expected shape, material, UV, occupancy and collision. Test one falsifiable hypothesis at a time. Record why a proposed fix is rejected; do not repeat a rejected candidate unchanged.

Use the installed Matt Pocock diagnosing-bugs skill when useful: construct and tighten the feedback loop, isolate the cause, then apply a narrow regression. Use tdd at the approved interfaces, with a red-before-green behavior test for meaningful repairs. Avoid tautological tests or mocks that implement the same geometry assumption as the product. A real performance regression may need an expensive reproducer; preserve it and use the cheapest valid diagnostic proxy during iteration.

At final review use the installed code-review skill when applicable: task-start commit is the review base; the canonical task prompt and its accepted updates are the specification. Review changed code against both repository standards and task requirements. Do not require a new GitHub issue or rerun setup merely because the external skill describes that workflow. Follow its applicable review procedure within existing agent permissions. Load only relevant installed skill files; discover their actual location rather than assuming all agents use one plugin path. Do not reinstall skills, copy the entire upstream pack, replace CLAUDE/AGENTS, or adopt its issue tracker/branch workflow as part of testing.

## 4. Keep builds separate from readings and verdicts

Reuse compiler outputs by actual inputs: source MAP bytes; all effective WAD/shader/texture/model and game-root dependencies; compiler executable and relevant library hashes; effective argv/options; format; architecture and relevant build/runtime environment; and upstream stage inputs. Use transitive input identities, not a whole-repository head or task ID as a substitute. Keep real route/case provenance separate from the physical build identity.

Key a reading/verdict by its BSP/log hashes, source/expected geometry and probe identities, reader implementation/policy and required acceptance contract. A changed reader or moved probe invalidates readings, NOT unchanged compiler outputs. Read the saved BSP using the new reader. A warning-parser change rereads retained logs. A changed exporter first exports again; if the actual MAP/assets match, reuse the build while recording both exporter observations.

Reconcile native and embedded-AUE routes independently. Reuse an expensive build only after proving actual compiler input and asset identity. Semantic similarity alone does not prove byte-equivalent compiler behavior. Keep differing inputs separate. Preserve logs/working-directory/path semantics; do not normalize away a meaningful WAD search path or option to manufacture equality. Cross-architecture outputs remain separate unless a specific supported evidence contract proves reuse legitimate; one host's execution never establishes another architecture's native support.

## 5. Resume individual stages safely

Keep immutable outputs and exact dependencies for generation/export, leaktest/QBSP, VIS, LIGHT, readings and runtime observations. Preserve pre-VIS and pre-LIGHT BSP snapshots: these programs mutate their inputs. Resume VIS against an intact verified QBSP/PRT snapshot; resume LIGHT against its valid VIS snapshot. Copy cached inputs into a fresh attempt workspace so one job cannot mutate another's evidence. Reuse required sidecars too. Missing, altered or incomplete files invalidate dependent stages and must not become a stale pass.

Choose invalidation from dependencies: changed QBSP invalidates that QBSP and downstream outputs; changed LIGHT need not rerun unchanged QBSP/VIS; changed probes need not rerun any compiler. Changed resource budget affects timeout/performance evidence, not automatically a completed deterministic correctness result. Preserve both old and new evidence identities.

## 6. Retain unsuccessful outcomes without automatic repetition

For identical relevant inputs, retain PASS, genuine FAIL, timeout/performance limit, safe refusal, unsupported capability and engine-limit findings as their real outcomes. Resuming is not an instruction to retry them. A cached FAIL still fails the required acceptance; a retained timeout stays incomplete/unsuccessful. Do not rename a failure as skipped success or remove it from the denominator.

Retry only with an explicit recorded reason: relevant code/input/tool/budget changed, resource admission is now demonstrably adequate, a lost worker is safely recovered, an identified transient failure cleared, or the operator requested reproduction. Create a new attempt linked to the original. Retry resource-blocked/interrupted work selectively after conditions change; never blindly retry every INCOMPLETE row. Bound any transient retry and retain both attempts. Do not repeatedly pay a 1,800-second VIS budget for an unchanged known timeout.

## 7. Protect interactivity and measure fairly

Use the existing shared fleet inventory, ownership leases and CPU/RAM/disk admission for compilers, readers and services. Leave OS, browser and operator headroom. Dispatch useful independent work to eligible hosts; do not equate maximum CPU utilization with minimum completion time. Backfill small admissible work while a large job waits. Respect architecture and native capability. A host marked BETA is excluded from general testing until the operator explicitly returns it to TEST.

Reserve quiet capacity for timing-sensitive unit/browser suites. Do not run the full frontend suite beside saturating compiler jobs and then repeatedly retry five-second timeouts. If a genuine contention failure occurs, preserve it and retry the affected set once under controlled conditions; expand only if the remaining evidence requires it. Do not raise test timeouts or disable assertions to disguise contention.

Measure queue, staging/build setup, compiler, reader, browser/engine and CI time separately. Performance comparisons need matching cases/options/hardware/power state and comparable contention. Historical throttled timings are not the baseline for a serviced laptop. A timeout under heavy contention is not proof of an intrinsic compiler algorithm limit; record the conditions and keep acceptance unsuccessful until qualified.

Run memory-heavy readers in owned bounded subprocesses with structured results and process-tree accounting. Terminate the reader process/group when necessary and report resource_blocked. Never inject asynchronous MemoryError into shared Python threads as a cancellation mechanism, deliberately exhaust host RAM, disable thermal protections or use swap as extra admitted RAM.

## 8. Collect every process and every required result

Assign each background job an owner, PID/process group, logical row, attempt ID, output directory, next check and deadline. Capture the primary child exit code before date/printf/cleanup, including across SSH/tee. Poll in minutes, harvest completed jobs promptly, and retain independent useful work. Do not end the session while owned jobs are uncollected unless a supported checkpoint transfers explicit ownership.

Write the expected-item set before dispatch and terminal records atomically. Reconcile missing, duplicate, unexpected, cancelled and unreadable records. A wrapper exit 0 with three of five required rows is unsuccessful. Distinguish not admitted, resource_blocked, interrupted, host lost, signal and confirmed OOM; SIGKILL alone is not an OOM diagnosis. Keep timed-out/failed/pending rows visible even when other work can continue.

## 9. Validate products live at the right boundary

For UX and user flows, use headed Chrome through the supported driver, actual controls and task-owned state. Exercise the affected interactions, failure/recovery and save/reopen behavior. A console store mutation, API-only bypass or screenshot of a different map does not establish the requested browser behavior. A Linux observation does not satisfy native Windows/macOS interaction acceptance.

For repaired geometry, keep the original failing case and required negative controls. Final repaired positives need the correct pinned compiler's successful BSP, NORMAL VIS and LIGHT, surface/material/UV and solid/bore checks, and inspection of the exact changed region in the selected unmodified engine. Confirm map/BSP identity, view/player coordinates, drawn surfaces, lighting and collision. Engine startup alone is insufficient. Keep expected leaks, missing surfaces, missing collision and blocked bores distinct. Require a player walk only where the intended passage fits that game's player box.

Reuse a prior browser/engine observation only when all behavior-relevant identities and assets match and no changed UI/runtime path invalidates it; state exactly what was reused and why. If interactive product code or the delivered executable changed, replay the affected live journey. Documentation-only changes do not require launching an engine. Never deform the requested shape, shrink a pipe, silently reduce tessellation, cap a bore, change game engines or repair the operator's map to obtain a pass.

## 10. Gate releases once, on the actual candidate

Run meaningful focused checks first. Schedule required broad suites after the coherent candidate is frozen, not after every prose/checkpoint commit. Reuse the same candidate's existing native CI evidence when its required identity is unchanged. Required all-target release checks remain mandatory; diagnostic caches cannot replace unperformed native platform checks. Batch coherent authorized AUCOM pushes and monitor the exact head's workflows to terminal completion. Preserve release/tag immutability and existing push permissions.

Do not silently ignore unrelated baseline failures. Identify exact IDs and compare with the documented baseline; distinguish a new regression, a known failure and incomplete execution. Re-run baseline failures only for relevant changes, uncertainty or an explicit baseline-repair task. A required full gate that remains red stays red; report whether it blocks this candidate. Do not expand a bounded compiler task into repairing every unrelated historical runner failure.

## 11. Finish with bounded evidence

Stop expanding when the declared acceptance is satisfied and required gates have terminal results. Escalate only for a new failure, relevant change or unresolved requirement. At the context checkpoint, commit coherent work, collect/stop or explicitly transfer owned jobs, and update the SAME canonical handoff with exact remaining commands. Do not invent successor prompts to clear a queue or rerun completed predecessors because their files remain visible.

Report changed behavior; fresh/reused/reread counts; genuine passes/failures/timeouts/incomplete/not-run states; exact tool/input/reader identities; phase time/resource totals; browser/engine evidence; CI/release identity; and remaining owner. Include omitted tests and the reason they were unaffected. Keep replayable original failures and small summaries; do not create repeated full mapper/database archives or retain redundant multi-gigabyte outputs solely as proof that a test ran. Never delete the only original failure or active evidence to save space.

## 12. Maintain one policy with synchronized copies

Maintain the canonical text at AUT skills/auto-pigeon-testing/SKILL.md. Distribute identical tracked TEST-SKILL.md copies to maintained project repositories, with a short AGENTS.md pointer requiring it before test selection/resumption. Replace superseded blanket testing prose in the existing owner modules rather than append conflicting instructions. Preserve documented required release gates and repository ownership/permission boundaries. Use the approved synchronization/check helper for later changes; do not edit one copy and leave the others stale.

Keep this shared file generic and safe for public repositories: no credentials, host IPs, private map/account names, corpus locations or task logs. Store fleet configuration and run evidence separately. If an unavailable, archived or dirty repository cannot be updated safely, name it in the installation ledger; do not silently call distribution complete. Installing this skill does not grant permission to push another repository or change its product code.
