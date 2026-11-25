# Roadmap — cap-lock

M0 schema; M1 deterministic writer; M2 frozen/locked; M3 migration.

## Milestone exit gates
Each milestone: working C++26 implementation with pinned managed compiler; passing `*_test.cpp` tests; negative and integration fixtures; measured coverage; sanitizer jobs where supported; benchmark/profiling evidence for performance-sensitive code; docs and contract update. Blocked work remains marked blocked rather than completed.

## Integration dependency
Coordinate with root `docs/ROADMAP.md` and freeze shared interfaces before cross-module implementation.
