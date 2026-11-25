# Goals — amp-workspace

## Mission
Multi-package workspace planning.

## In scope
Virtual roots; member discovery; default members; shared dependencies; target ownership; package selection.

## Non-goals
No external build-system delegation; no undocumented cross-module behavior; no unverified production readiness. Ownership is limited to this subsystem; shared interfaces require review.

## Success criteria
1. Public contract documented with success, error and cancellation semantics.
2. Unit, integration and negative tests cover: Nested member conflicts, duplicate names, target cycles, workspace=true inheritance, glob exclusions.
3. Meets root QUALITY.md coverage and sanitizer requirements; benchmark and profiling evidence for hot paths.
4. Deterministic behavior on pinned toolchain with explicit platform limitations.
