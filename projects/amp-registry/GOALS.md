# Goals — amp-registry

## Mission
Registry client and publishing.

## In scope
HTTP index; package download; publish/yank; credential helpers; signatures and provenance.

## Non-goals
No external build-system delegation; no undocumented cross-module behavior; no unverified production readiness. Ownership is limited to this subsystem; shared interfaces require review.

## Success criteria
1. Public contract documented with success, error and cancellation semantics.
2. Unit, integration and negative tests cover: Auth failure, token redaction, upload retry, immutable release, malicious archive.
3. Meets root QUALITY.md coverage and sanitizer requirements; benchmark and profiling evidence for hot paths.
4. Deterministic behavior on pinned toolchain with explicit platform limitations.
