# Parallel agent coordination protocol (normative, v1.4)

**Status:** documented workflow; no task-claim service or `cap dev` command is implemented. Existing seven lanes in ROADMAP.md remain ownership defaults.

## Stage 1: contract freeze
Before concurrent implementation, identify cross-module interfaces, stable schema IDs, ownership and versioned error codes. The owning lane signs off on `CONTRACTS.md` and `API.md` changes and provides compile-time mock interfaces and golden fixtures. Breaking changes require an ADR, impacted-owner approval, migration note and fixture updates. The runner records lane sign-off separately from the independent milestone verdict.

## Stage 2: parallel execution
Each task must include a stable ID, owner/lane, dependency IDs, allowed paths, base commit, contract and acceptance revisions, input/output artifacts, tests, risks, estimated effort and evidence links. Workers, including Qwen when admitted under the fallback policy, work in isolated Git worktrees with one active owner per task. Sol proposes assignments and the runner enforces claims transactionally. A claim is advisory without the runner. Edits to another module's public contract require its owner's sign-off. Use mock adapters to unblock implementation without introducing undocumented coupling.

## Stage 3: integration gates
Before staging tasks, check changed-file ownership and contract version, run targeted unit and contract tests and security, format and static analysis checks, and collect sanitizer results where supported and benchmark evidence for hot paths. Stage in dependency order. Milestone promotion requires cross-module fixtures, reproducibility evidence, documented limitations, passing acceptance checks and an independent read-only Sol approval for the exact commit. Record unrun checks as `not run`.

## Proposed future CLI (not implemented)
`cap dev tasks --ready`, `cap dev claim <task-id>`, `cap dev contracts check`, `cap dev integration status`. These are optional coordination tools, not dependencies of the native build engine. Local Git commits and their verification records are authoritative during local development. Avoid storing secrets or personal access tokens in task metadata.

## Lane constraints
A core/bootstrap; B manifest/meta; C workspace/toolchain; D build/cache; E resolver/sources/lock; F test/quality; G registry/plugin/CLI. `cap-compdb` follows build action contracts; `cap-vscode` waits for stable CLI JSON and compdb interfaces. Cross-lane blockers are resolved via documented contracts, not direct private header imports.

## Operational profile
See [Local Agent Operations](LOCAL-AGENT-OPERATIONS.md) for worker limits across the seven lanes, frozen acceptance, review and production approval. The [harness configuration](HARNESS-CONFIGURATION.md) describes the local client and benchmark examples. Runner enforcement is pending.
