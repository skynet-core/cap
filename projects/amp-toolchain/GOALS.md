# Goals — amp-toolchain

## Mission
Managed compiler and SDK lifecycle.

## In scope
Signed catalog; platform-specific bundles; digest verification; download resume; install/verify/uninstall; compiler capabilities.

## Non-goals
No external build-system delegation; no undocumented cross-module behavior; no unverified production readiness. Ownership is limited to this subsystem; shared interfaces require review.

## Success criteria
1. Public contract documented with success, error and cancellation semantics.
2. Unit, integration and negative tests cover: Tampered archive, zip-slip, interrupted downloads, unsupported sanitizer/target, offline install, concurrent installs.
3. Meets root QUALITY.md coverage and sanitizer requirements; benchmark and profiling evidence for hot paths.
4. Deterministic behavior on pinned toolchain with explicit platform limitations.
