# cap-core local performance baseline

`core_bench.cpp` benchmarks the existing digest and process APIs on Linux.
The digest input is a fixed 1 MiB repetition of `cap-core benchmark payload\n`,
checked against its SHA-256 known answer before timing and on every sample.
Process latency uses direct argv `["/usr/bin/true"]`; each child must return
status 0 and not time out. Both operations have two warmups and nine samples.
Reported dispersion is median absolute deviation (MAD). The harness introduces
no runtime API and needs no dependency beyond the core modules and C++ library.

## Reproduce

Run from the repository root with the pinned compiler. Keep all module caches,
compiler temporaries and binaries in a unique local-filesystem scratch
directory:

```sh
set -eu
repo=$PWD
mkdir -p "$repo/.bootstrap"
taskdir=$(mktemp -d "$repo/.bootstrap/cap-perf.XXXXXXXX")
export TMPDIR=$taskdir/tmp
export BOOTSTRAP_ROOT=$taskdir/bootstrap
mkdir -p "$TMPDIR" "$BOOTSTRAP_ROOT/modules/bench" "$BOOTSTRAP_ROOT/bin"
cxx=/home/linuxbrew/.linuxbrew/bin/g++-16
cd "$BOOTSTRAP_ROOT/modules/bench"
for module in diagnostic digest process; do
  "$cxx" -std=c++26 -O2 -Wall -Wextra -Werror -fmodules -x c++ -c \
    "$repo/projects/cap-core/$module.cppm" -o "$module.o"
done
"$cxx" -std=c++26 -O2 -Wall -Wextra -Werror -fmodules \
  -I"$repo/projects/cap-core/include" \
  "$repo/projects/cap-core/benchmarks/core_bench.cpp" \
  diagnostic.o digest.o process.o -o "$BOOTSTRAP_ROOT/bin/core_bench"
"$BOOTSTRAP_ROOT/bin/core_bench"
```

For the profile, repeat the same module and consumer commands, replacing every
`-O2` with `-O2 -pg`, in a distinct build directory:

```sh
profile=$BOOTSTRAP_ROOT/profile
mkdir -p "$profile/modules" "$profile/bin" "$profile/run"
cd "$profile/modules"
for module in diagnostic digest process; do
  "$cxx" -std=c++26 -O2 -pg -Wall -Wextra -Werror -fmodules -x c++ -c \
    "$repo/projects/cap-core/$module.cppm" -o "$module.o"
done
"$cxx" -std=c++26 -O2 -pg -Wall -Wextra -Werror -fmodules \
  -I"$repo/projects/cap-core/include" \
  "$repo/projects/cap-core/benchmarks/core_bench.cpp" \
  diagnostic.o digest.o process.o -o "$profile/bin/core_bench"
cd "$profile/run"
"$profile/bin/core_bench"
test -s gmon.out
/home/linuxbrew/.linuxbrew/bin/gprof -b \
  "$profile/bin/core_bench" gmon.out
```

`gprof` combines periodic program-counter sampling with compiler-instrumented
call counts. Its timings are not comparable with the uninstrumented baseline;
waiting inside kernel process creation and child execution is not attributed as
user-space function time.

## Baseline and profile (2026-11-20)

- Base revision: `b231ad9fe17953a80a954dfe9eabe6e1bd9076fe`.
- Host: Ubuntu 26.04.1, Linux `7.0.0-43-generic`, x86_64; AMD Ryzen 9 5980HS,
  16 logical CPUs.
- Compiler: `/home/linuxbrew/.linuxbrew/bin/g++-16`, GCC 16.2.0; C++26,
  `-O2 -Wall -Wextra -Werror`. Profile build adds `-pg`.
- Three uninstrumented runs (actual output; run-to-run variation is a reminder
  that a single sample set is a local baseline, not a universal performance
  claim). The first two runs preserve the original measurements; their digest
  MAD values used the old approximation and are superseded by the corrected
  per-sample-throughput MAD in the third run:

  ```text
  digest: payload_bytes=1048576 warmups=2 samples=9 median_MiB_s=327.608 MAD_MiB_s=4.09269
  process: executable=/usr/bin/true argv_count=1 warmups=2 samples=9 median_ms=0.794171 MAD_ms=0.022
  digest: payload_bytes=1048576 warmups=2 samples=9 median_MiB_s=309.547 MAD_MiB_s=6.26389
  process: executable=/usr/bin/true argv_count=1 warmups=2 samples=9 median_ms=0.818965 MAD_ms=0.019555
  digest: payload_bytes=1048576 warmups=2 samples=9 median_MiB_s=288.212 MAD_MiB_s=1.36397
  process: executable=/usr/bin/true argv_count=1 warmups=2 samples=9 median_ms=0.905289 MAD_ms=0.040368
  ```

- Nonempty `gmon.out` in both profile runs. The first `gprof -b` reported
  0.02 seconds sampled; the correction rerun reported 0.04 seconds, 12 calls to
  `cap::core::detail::sha256` at 3.33 ms/call and 100% of sampled time.
  `LocalProcessRunner::run` had 12 calls but rounded to 0.00 user CPU seconds.
  This low-resolution call-graph instrumentation identifies SHA-256 as the
  measured user-space hot path; it does not profile kernel/child spawn cost.
- `perf_event_paranoid=4`; Linux `perf` sampling is unavailable here.
- Review budget: investigate a sustained median slowdown above 10%, accounting
  for dispersion, per `docs/QUALITY.md`. This is a local baseline, not a
  before/after optimization comparison.

## Verification

- `make core-test` (GCC 16.2.0, C++26, task-local `BOOTSTRAP_ROOT` and
  `TMPDIR`): exit 0; diagnostics, filesystem, process, digest, and primitives
  suites passed.
- `make sanitize` (pinned Clang 23.1.6, task-local outputs): exit 0; ASan,
  UBSan, and leak checks passed for all suites; TSan passed the configured
  diagnostic and filesystem suites. As documented by the target, TSan process,
  digest and primitives tests and MSan were not run.
- `/home/linuxbrew/.linuxbrew/bin/clang-format --dry-run --Werror
  projects/cap-core/benchmarks/core_bench.cpp`: exit 0 (clang-format 23.1.6).
- `/opt/clice/bin/clice lint --workspace projects/cap-core`: exit 0,
  `Linted 4 translation units: 0 findings` (clice
  `1.0.0+g92d26ad`). The correction rerun used a temporary compilation
  database with the documented GCC C++26 module compile commands for
  `diagnostic.cppm`, `digest.cppm`, `process.cppm` and the benchmark consumer.
  Its `clice.toml` routed cache/PCH and PCM output to task-local scratch and was
  removed after checking. The previous check used actual Clang module commands
  and also reported zero findings.
- `/opt/clice/bin/clice serve` over stdio with the GCC-command database:
  completed `textDocument/diagnostic` for `core_bench.cpp`, zero
  warnings/errors, server exit 0. The previous Clang-command LSP check also
  completed with zero diagnostics.
- `git diff --check`: exit 0. No public API changed.
- `make core-coverage` with `GENHTML` and `LCOV_PERL_LIB` pointing to the
  local lcov 2.6 and Perl installations,
  with task-local `BOOTSTRAP_ROOT` and `TMPDIR`: exit 0; 82.1% lines,
  68.9% branches, and digest gate 85/85 lines. No network access.
- Linux `perf` sampling is unavailable as noted above.
