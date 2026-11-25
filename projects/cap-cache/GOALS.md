# Goals — cap-cache

## Mission
Content-addressed source and artifact cache.

## In scope
CAS; action fingerprint; SQLite metadata; garbage collection; artifact integrity; workspace compdb index.

## Non-goals
No external build-system delegation; no undocumented cross-module behavior; no unverified production readiness. Ownership is limited to this subsystem; shared interfaces require review.

## Success criteria
1. Public contract documented with success, error and cancellation semantics.
2. Unit, integration and negative tests cover: Cache hit/miss, corruption, race, GC during build, env invalidation, module cache isolation.
3. Meets root QUALITY.md coverage and sanitizer requirements; benchmark and profiling evidence for hot paths.
4. Deterministic behavior on pinned toolchain with explicit platform limitations.
