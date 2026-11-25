# Goals — amp-plugin-sdk

## Mission
Stable external command protocol.

## In scope
Protocol negotiation; argument forwarding; optional JSON messages; structured diagnostics; C++ SDK.

## Non-goals
No external build-system delegation; no undocumented cross-module behavior; no unverified production readiness. Ownership is limited to this subsystem; shared interfaces require review.

## Success criteria
1. Public contract documented with success, error and cancellation semantics.
2. Unit, integration and negative tests cover: Version mismatch, malformed JSON, stderr separation, cancellation, binary paths with spaces.
3. Meets root QUALITY.md coverage and sanitizer requirements; benchmark and profiling evidence for hot paths.
4. Deterministic behavior on pinned toolchain with explicit platform limitations.
