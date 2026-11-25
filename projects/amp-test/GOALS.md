# Goals — amp-test

## Mission
Go-style source-file tests.

## In scope
*_test.cpp discovery; per-target test build; Google Test and Catch2 adapters; filtering; runner; reports.

## Non-goals
No external build-system delegation; no undocumented cross-module behavior; no unverified production readiness. Ownership is limited to this subsystem; shared interfaces require review.

## Success criteria
1. Public contract documented with success, error and cancellation semantics.
2. Unit, integration and negative tests cover: Linux-only tests, multiple binaries, fixture linking, filter, cross-target runner, failure reporting.
3. Meets root QUALITY.md coverage and sanitizer requirements; benchmark and profiling evidence for hot paths.
4. Deterministic behavior on pinned toolchain with explicit platform limitations.
