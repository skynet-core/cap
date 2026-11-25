# Goals — amp-core

## Mission
Foundational primitives.

## In scope
Diagnostic/result types; filesystem paths; cryptographic digests; cancellation; process execution; logging; environment capture.

## Non-goals
No external build-system delegation; no undocumented cross-module behavior; no unverified production readiness. Ownership is limited to this subsystem; shared interfaces require review.

## Success criteria
1. Public contract documented with success, error and cancellation semantics.
2. Unit, integration and negative tests cover: Path normalization, atomic writes, process exit/signal handling, timeout and cancellation, error propagation.
3. Meets root QUALITY.md coverage and sanitizer requirements; benchmark and profiling evidence for hot paths.
4. Deterministic behavior on pinned toolchain with explicit platform limitations.
