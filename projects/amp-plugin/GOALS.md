# Goals — amp-plugin

## Mission
Cargo-style executable plugins.

## In scope
amp-* discovery; installed command registry; plugin manifest; install/update/uninstall; workspace plugin locks.

## Non-goals
No external build-system delegation; no undocumented cross-module behavior; no unverified production readiness. Ownership is limited to this subsystem; shared interfaces require review.

## Success criteria
1. Public contract documented with success, error and cancellation semantics.
2. Unit, integration and negative tests cover: Name collision, exit propagation, malformed manifest, version conflict, malicious executable, workspace lock.
3. Meets root QUALITY.md coverage and sanitizer requirements; benchmark and profiling evidence for hot paths.
4. Deterministic behavior on pinned toolchain with explicit platform limitations.
