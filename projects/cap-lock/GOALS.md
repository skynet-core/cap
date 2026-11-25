# Goals — cap-lock

## Mission
TOML lockfiles and frozen mode.

## In scope
Canonical package IDs; dependency edges; checksum; feature resolution; lockfile diff; local effective state.

## Non-goals
No external build-system delegation; no undocumented cross-module behavior; no unverified production readiness. Ownership is limited to this subsystem; shared interfaces require review.

## Success criteria
1. Public contract documented with success, error and cancellation semantics.
2. Unit, integration and negative tests cover: Round trip, lock merge, concurrent writes, source replacement, frozen missing cache.
3. Meets root QUALITY.md coverage and sanitizer requirements; benchmark and profiling evidence for hot paths.
4. Deterministic behavior on pinned toolchain with explicit platform limitations.
