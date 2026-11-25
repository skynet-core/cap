# Goals — cap-sources

## Mission
Dependency source adapters.

## In scope
Registry archives; Git commit fetch; local path and workspace; credentials; source integrity.

## Non-goals
No external build-system delegation; no undocumented cross-module behavior; no unverified production readiness. Ownership is limited to this subsystem; shared interfaces require review.

## Success criteria
1. Public contract documented with success, error and cancellation semantics.
2. Unit, integration and negative tests cover: Git branch pin, private SSH, checksum mismatch, offline fetch, source alias collisions.
3. Meets root QUALITY.md coverage and sanitizer requirements; benchmark and profiling evidence for hot paths.
4. Deterministic behavior on pinned toolchain with explicit platform limitations.
