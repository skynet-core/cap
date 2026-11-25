# Goals — amp-resolver

## Mission
Dependency and feature solving.

## In scope
SemVer constraints; PubGrub-like conflict explanation; additive features; patches; local overrides; dev scopes.

## Non-goals
No external build-system delegation; no undocumented cross-module behavior; no unverified production readiness. Ownership is limited to this subsystem; shared interfaces require review.

## Success criteria
1. Public contract documented with success, error and cancellation semantics.
2. Unit, integration and negative tests cover: Conflicts, diamond deps, optional feature activation, dev-only isolation, overrides of multiple sources.
3. Meets root QUALITY.md coverage and sanitizer requirements; benchmark and profiling evidence for hot paths.
4. Deterministic behavior on pinned toolchain with explicit platform limitations.
