# Goals — cap-meta

## Mission
C++26 reflection and metadata.

## In scope
Reflection-assisted TOML mapping; typed schema descriptors; CLI option metadata; validation metadata.

## Non-goals
No external build-system delegation; no undocumented cross-module behavior; no unverified production readiness. Ownership is limited to this subsystem; shared interfaces require review.

## Success criteria
1. Public contract documented with success, error and cancellation semantics.
2. Unit, integration and negative tests cover: Optional fields, enum mappings, unknown fields, invalid values, nested structures, compiler conformance.
3. Meets root QUALITY.md coverage and sanitizer requirements; benchmark and profiling evidence for hot paths.
4. Deterministic behavior on pinned toolchain with explicit platform limitations.
