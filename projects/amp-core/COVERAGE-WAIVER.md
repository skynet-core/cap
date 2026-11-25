# Coverage and sanitizer waiver — amp-core (M0)

Per `docs/QUALITY.md`: exclusions and waivers state owner, reason, expiry and
mitigation. Raw policy gates (85% line / 75% branch) apply to amp-core once
the excluded paths below become testable.

## Recorded baseline (2026-11-20)

Toolchain: g++-16 instrumented (`-O1 -fprofile-arcs -ftest-coverage`),
lcov 2.6 capture with `--gcov-tool gcov-16 --filter branch`, extracted to
`projects/amp-core/include/*`. HTML report: upstream lcov-2.6 `genhtml`
(the brew genhtml bottle on this host references an `LCOV::Util` module the
formula never installs; the Makefile uses the upstream script with
`.bootstrap/perl-lib` for DateTime/TimeDate deps). `make core-coverage`
enforces no-regression:

The clock migration also includes `projects/amp-core/clock.cppm` in capture.
Capture-time path selection retains the production-header scope and excludes
external and test sources before merging GCC coverage records.

- lines: 79.7% (232 of 291) → gate >= 79%
- branches: 66.9% (109 of 163) → gate >= 66%

Post-M0-completion measurement (2026-11-20, digest + primitives added):
84.5% lines (321/380), 71.7% branches (137/191); `digest.hpp` and
`stop_token.hpp` at 100% lines. Gate values intentionally unchanged until
T05 fault injection, per the expiry conditions below.

Stop-token module migration measurement (2026-11-20): 82.1% lines
(380/463), 68.9% branches (213/309); `clock.cppm` 3/3 and
`stop_token.cppm` 4/4 lines. The earlier recorded totals differ in scope,
so they do not establish a like-for-like regression.

Digest and logger module migration measurement (2026-11-20): 82.3% lines
(387/470), 68.9% branches (213/309); `digest.cppm` 85/85 lines.
The critical digest gate now checks the production module source; the
existing header and module capture scope and baseline floors remain in force.

Filesystem module migration measurement (2026-11-20): 82.3% lines
(387/470), 68.9% branches (213/309); `filesystem.cppm` 103/132 lines.
The trace includes the module source; the historical floors remain unchanged.

Process module migration measurement (2026-11-20): 82.1% lines
(386/470), 68.9% branches (213/309); digest remains 85/85 lines.
The one-line difference is the `ProcessSpec::argv` declaration, which gcov
no longer marks covered after migration from the included header.

## Excluded (uncovered) paths

1. **Child-side fork/exec block** in `LocalProcessRunner::run`
   (`process.cppm`): executed on every passing process test, but gcov cannot
   record it — `execve` replaces the process image before any coverage
   flush. Mitigation: the environment-filter logic was extracted to the pure
   `detail::build_envp` and is exercised through the runner; behavior is proven by
   `process_test.cpp` (stdout capture, exit codes, empty/filtered env, cwd,
   timeout, stdin).
2. **Kernel-fault branches** (untriggerable without fault injection):
   - `filesystem.cppm`: `exists()` access-error, short read, `ssize_max`
     payload guard, write `EINTR` retry, temp write/fsync/close failure,
     atomic rename failure.
   - `process.cppm`: pipe/fork/waitpid failure, child dup2/chdir failure,
     exec-report write failure, stdin write error.
   - `diagnostic.hpp`: two unreachable post-switch fallback returns.
   Mitigation: defensive error mapping only; no observable behavior depends
   on these lines.
3. **Rejected sanitizer jobs** (documented, never claimed, per
   `docs/QUALITY.md` "reject unsupported sanitizer combinations"):
   - MSan: no fully instrumented dependencies on this host.
   - TSan + `process_test`: child processes are uninstrumented; running it
     would not validate the child and could produce misleading reports.

## Expiry / upgrade conditions

- Expiry: implementation of amp-core T05 (cancellation, integrity,
  atomicity, fault injection).
- Upgrade: seccomp- or preload-based syscall-failure injection covers class
  2; a pre-exec coverage flush or no-exec child variant covers class 1.
  Then raise `AMP_COV_BASELINE_LINE/BRANCH` to the policy 85/75.

## Owner

Lane A (amp-core). Baseline and waiver changes require milestone review per
`docs/LOCAL-AGENT-OPERATIONS.md`.
