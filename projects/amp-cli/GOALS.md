# Goals — amp-cli

## Mission
Unified user-facing commands.

## In scope
new/init/add/remove/build/run/test/bench/profile/check/clean/update/fetch/publish/toolchain/plugin/cache/override.

## Non-goals
No external build-system delegation; no undocumented cross-module behavior; no unverified production readiness. Ownership is limited to this subsystem; shared interfaces require review.

## Success criteria
1. Public contract documented with success, error and cancellation semantics.
2. Unit, integration and negative tests cover: Help snapshots, argument parsing, ambiguous package, signal forwarding, color/no-color.
3. Meets root QUALITY.md coverage and sanitizer requirements; benchmark and profiling evidence for hot paths.
4. Deterministic behavior on pinned toolchain with explicit platform limitations.
