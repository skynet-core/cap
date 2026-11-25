# Goals — amp-build

## Mission
Native build DAG and execution.

## In scope
Compile/link/archive actions; module scanning; target source graph; scheduler; compiler backend; compilation database.

## Non-goals
No external build-system delegation; no undocumented cross-module behavior; no unverified production readiness. Ownership is limited to this subsystem; shared interfaces require review.

## Success criteria
1. Public contract documented with success, error and cancellation semantics.
2. Unit, integration and negative tests cover: Incremental header edits, module BMI invalidation, linker order, parallel cancellation, cyclic graph, platform filtering.
3. Meets root QUALITY.md coverage and sanitizer requirements; benchmark and profiling evidence for hot paths.
4. Deterministic behavior on pinned toolchain with explicit platform limitations.
