# Roadmap — cap-build

M0 single binary; M1 static lib and multi-bin; M2 incremental DAG; M3 modules; M4 cross-target.

## Milestone exit gates
Each milestone: working C++26 implementation with pinned managed compiler; passing `*_test.cpp` tests; negative and integration fixtures; measured coverage; sanitizer jobs where supported; benchmark/profiling evidence for performance-sensitive code; docs and contract update. Blocked work remains marked blocked rather than completed.

## Integration dependency
Coordinate with root `docs/ROADMAP.md` and freeze shared interfaces before cross-module implementation.
