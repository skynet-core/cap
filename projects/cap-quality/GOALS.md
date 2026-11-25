# Goals — cap-quality

## Mission
Sanitizers, coverage, benchmarks, profiling.

## In scope
ASan/UBSan/TSan/MSan capability checks; coverage; perf/PGO; benchmark runner; baseline reports.

## Non-goals
No external build-system delegation; no undocumented cross-module behavior; no unverified production readiness. Ownership is limited to this subsystem; shared interfaces require review.

## Success criteria
1. Public contract documented with success, error and cancellation semantics.
2. Unit, integration and negative tests cover: ASan and TSan incompatible, MSan instrumented deps, benchmark regressions, PGO stages.
3. Meets root QUALITY.md coverage and sanitizer requirements; benchmark and profiling evidence for hot paths.
4. Deterministic behavior on pinned toolchain with explicit platform limitations.
