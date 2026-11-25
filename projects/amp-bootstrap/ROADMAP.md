# Roadmap — amp-bootstrap

M0 host prerequisites; M1 stage0; M2 toolchain acquisition; M3 self-host verification.

## Milestone exit gates
Each milestone: working C++26 implementation with pinned managed compiler; passing `*_test.cpp` tests; negative and integration fixtures; measured coverage; sanitizer jobs where supported; benchmark/profiling evidence for performance-sensitive code; docs and contract update. Blocked work remains marked blocked rather than completed.

## Integration dependency
Coordinate with root `docs/ROADMAP.md` and freeze shared interfaces before cross-module implementation.


## v1.3 mandatory bootstrap update
Implement and test the root `Makefile` stages `boost-fetch`, `boost-bootstrap`, `boost-build`, `bootstrap`, and `self-host`. Boost must be compiled from source, not merely downloaded as headers. The default is latest stable Boost, with exact tag/commit pinned for releases. Add negative tests for missing tools, bad commit, interrupted downloads, incompatible compiler ABI and absent bootstrap source. See `docs/BOOTSTRAP.md` and `docs/BOOST-POLICY.md`.
