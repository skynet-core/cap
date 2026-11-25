# Task backlog — amp-core

- [x] T01: Freeze API and error contract; create contract fixtures. (Implemented: `Diagnostic`/`Result`/`Status` in `include/amp/core/`; golden fixture `tests/golden/diagnostic_format.txt`; verified via `make core-test`.)
- [x] T02: Implement minimal vertical slice with C++26 and injected IO. (Implemented: `IFileSystem`/`LocalFileSystem` and `IProcessRunner`/`LocalProcessRunner` in `include/amp/core/`; verified via `make core-test` filesystem/process suites. Process-tree termination and isolation adapters remain unimplemented per v1.4 addendum.)
- [ ] T03: Add positive, negative and boundary unit tests in `*_test.cpp` files. (Progress 2026-11-20: `digest_test.cpp` FIPS vectors incl. 55/56/57-byte block boundaries, hex parse positive/negative cases; `primitives_test.cpp` StopToken sticky semantics, IClock monotonicity, ILogger canonical capture. Remaining: test-case inventory items below.)
- [ ] T04: Add cross-module integration and regression fixtures.
- [ ] T05: Implement cancellation, integrity and atomicity behavior where applicable.
- [ ] T06: Add local coverage checks (85% line/75% branch; 95% critical algorithms) and a waiver process. (Coverage job + waiver exist: `make core-coverage`, `COVERAGE-WAIVER.md`; raw 85/75 gate deferred to T05 fault injection.)
- [x] T07: Run ASan/UBSan; TSan for concurrency; MSan only on compatible fully instrumented setup. (2026-11-20: `make sanitize` on pinned clang-23 — ASan+UBSan+LeakSanitizer all suites, TSan diagnostic+filesystem; MSan and TSan+process_test rejected as unsupported and documented, not claimed.)
- [ ] T08: Add repeatable benchmarks and profiler baseline for hot paths.
- [ ] T09: Update DESIGN, CONTRACTS, ADRs, docs and evidence for release review.

## Test case inventory
- [ ] Path normalization
- [ ] atomic writes
- [ ] process exit/signal handling
- [ ] timeout and cancellation
- [ ] error propagation

### M0/core/T03a — Atomic replacement and child signal regression tests

- **Status** — Proposed for runner intake. No worker claim or milestone verdict exists.
- **Owner and role** — Lane A, one Luna worker. Use the Luna binding from the current [workflow profile](../../tools/agent-harness/ai-workflow.yaml).
- **Base and dependencies** — Planning baseline `caf85a347a31da297cd984c7f62437ccc3b14fd5`. Depends on the existing T01 and T02 filesystem and process contracts at that revision. The runner records the exact admitted base, frozen request and acceptance revisions before dispatch.
- **Allowed paths** — `projects/amp-core/tests/filesystem_test.cpp` and `projects/amp-core/tests/process_test.cpp`. Add assertions under the existing API and preserve the existing fixtures and checks. If a fixture reveals a contract gap, return it to Sol for a separate task.

#### Proposed acceptance criteria

1. An atomic write over an existing file replaces the full payload. Use a shorter replacement to verify that the original trailing bytes are gone. The target directory contains only the expected entries after replacement.
2. A child terminated by `SIGTERM` yields a successful `ProcessResult` with `exit_code == -SIGTERM` and `timed_out == false`. Use a deterministic child fixture with no external services.
3. The exact candidate passes `make core-test`, the supported `make sanitize` jobs, and `make core-coverage` with the existing M0 coverage waiver. Attach compiler identity, command exit statuses and logs. Keep existing capability exclusions and their actual statuses visible.

This slice adds tests for implemented behavior. Cancellation wiring and process-tree termination are tracked in T05 and T10.

## Definition of done
All applicable quality gates in root docs/QUALITY.md met; no unsupported completion claims.

## v1.4 additional tasks (unimplemented)
- [ ] T10: Implement and test the v1.4 API/contract additions with failure fixtures and evidence.
