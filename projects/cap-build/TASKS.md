# Task backlog — cap-build

- [ ] T01: Freeze API and error contract; create contract fixtures.
- [ ] T02: Implement minimal vertical slice with C++26 and injected IO.
- [ ] T03: Add positive, negative and boundary unit tests in `*_test.cpp` files.
- [ ] T04: Add cross-module integration and regression fixtures.
- [ ] T05: Implement cancellation, integrity and atomicity behavior where applicable.
- [ ] T06: Add local coverage checks (85% line/75% branch; 95% critical algorithms) and a waiver process.
- [ ] T07: Run ASan/UBSan; TSan for concurrency; MSan only on compatible fully instrumented setup.
- [ ] T08: Add repeatable benchmarks and profiler baseline for hot paths.
- [ ] T09: Update DESIGN, CONTRACTS, ADRs, docs and evidence for release review.

## Test case inventory
- [ ] Incremental header edits
- [ ] module BMI invalidation
- [ ] linker order
- [ ] parallel cancellation
- [ ] cyclic graph
- [ ] platform filtering

## Definition of done
All applicable quality gates in root docs/QUALITY.md met; no unsupported completion claims.

## v1.4 additional tasks (unimplemented)
- [ ] T10: Freeze scheduler resource and cancellation contracts; test fairness and memory admission.
- [ ] T11: Add strict/audit/off isolation capability matrix and undeclared-input fixtures.
- [ ] T12: Add versioned action trace and cache-miss reason integration.
- [ ] T13: Benchmark 10k-action scheduler and incremental no-op, including trace overhead.
