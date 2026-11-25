# Goals — amp-bootstrap

## Mission
External-build-system-free bootstrap.

## In scope
Auditable stage-0 C++ source; pinned compiler acquisition; hash/signature verification; stage-1 build; self-host.

## Non-goals
No external build-system delegation; no undocumented cross-module behavior; no unverified production readiness. Ownership is limited to this subsystem; shared interfaces require review.

## Success criteria
1. Public contract documented with success, error and cancellation semantics.
2. Unit, integration and negative tests cover: Fresh host, offline with preseeded bundle, tamper rejection, stage-1/2 consistency, cross-host smoke.
3. Meets root QUALITY.md coverage and sanitizer requirements; benchmark and profiling evidence for hot paths.
4. Deterministic behavior on pinned toolchain with explicit platform limitations.


## v1.3 mandatory bootstrap update
Implement and test the root `Makefile` stages `boost-fetch`, `boost-bootstrap`, `boost-build`, `bootstrap`, and `self-host`. Boost must be compiled from source, not merely downloaded as headers. The default is latest stable Boost, with exact tag/commit pinned for releases. Add negative tests for missing tools, bad commit, interrupted downloads, incompatible compiler ABI and absent bootstrap source. See `docs/BOOTSTRAP.md` and `docs/BOOST-POLICY.md`.
