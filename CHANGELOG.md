# Engineering specification changes

## v1.4 (2026-11-19)
- Preserved v1.3 scaffold and 20 existing module specifications.
- Added six design documents: scheduler, isolation, agent coordination, observability, remote execution and milestone plan.
- Strengthened architecture, quality, roadmap, traceability, bootstrap and agent guidance.
- Added module API/contract/task addenda for build, cache, core, CLI, bootstrap, toolchain and quality.
- Changed Makefile from moving latest Boost default to mandatory explicit pinned version and full commit; verify reused checkout and record submodules. No verified upstream pin bundled.
- Deferred remote workers, daemon, web trace viewer and AI agent CLI to future implementation.
- No production implementation, runtime tests, Boost download or build executed.

## v1.5 — local agent development operations
- Added normative local agent workflow, bounded retries, review policy and benchmark acceptance metrics.
- Added sample OpenCode and Codex CLI local-vLLM configurations; Codex requires Responses API support.
- Bundled experimental benchmark harness and serving example.
- Preserved 20-module specification and v1.4 Boost/bootstrap policy; no runtime implementation claimed.

## M0 implementation (2026-11-19)
- Baseline commit of the v1.5 spec precedes all code changes.
- Pinned system compiler to Homebrew GCC 16 (`g++-16`) as the default C++26 toolchain for bootstrap and quality targets (override via `CXX=`).
- Implemented amp-core T01 slice: stable error taxonomy, structured `Diagnostic` (span, causal chain, remediation), canonical single-line `format()`, `Result<T>`/`Status` aliases — header-only, no new dependencies.
- Added contract fixture `tests/golden/diagnostic_format.txt` and assert-based `tests/diagnostic_test.cpp`; verified with `make core-test` (all checks passed).
- Implemented amp-core T02 slice (2026-11-20): `IFileSystem`/`LocalFileSystem`
  (read, atomic temp+fsync+rename write, sorted enumerate, canonicalize) and
  `IProcessRunner`/`LocalProcessRunner` (direct execve, never a shell; cwd;
  env allowlist; stdin; timeout kill; exec-failure reporting). POSIX-only
  stage-0 adapters with documented limitations; verified by
  `tests/filesystem_test.cpp` and `tests/process_test.cpp`.
- Sanitizer and coverage jobs wired into the bootstrap Makefile (2026-11-20):
  `make sanitize` (pinned clang-23: ASan+UBSan+LeakSanitizer on all suites,
  TSan on non-process suites; MSan and TSan+process_test rejected as
  unsupported, documented, never claimed) and `make core-coverage`
  (g++-16 instrumented, lcov 2.6 + upstream genhtml, HTML report,
  no-regression gate vs recorded baseline 79.7% line / 66.9% branch;
  raw 85/75 policy gate deferred to T05 fault injection per
  `projects/amp-core/COVERAGE-WAIVER.md`). Fault-injection tests added
  (permission denials, exec EACCES, RLIMIT_NPROC fork exhaustion);
  env-allowlist logic extracted to testable `detail::build_envp`.
- Boost 1.92.0 pin verified and built (2026-11-20): upstream tag `boost-1.92.0`
  -> commit `afdfa32505af73e3d208144b3f623f0096cb62b6`; 172/172 submodules at
  recorded SHAs (`.bootstrap/boost-submodules.txt`). `make boost` pins
  Boost.Build's toolset to the same g++-16 via generated `user-config.jam` +
  `BOOST_BUILD_USER_CONFIG` — b2 ignores CC/CXX env vars and had silently
  selected the system gcc-15. Install digests in `.bootstrap/boost-install-digest.txt`;
  C++26 ABI link probe against filesystem/program_options/json passes.
- M0 completed (2026-11-20): amp-core primitives finished — `Digest` (SHA-256
  per FIPS 180-4, FIPS vectors incl. block-boundary lengths, 64-hex parse with
  negative cases, 100% line coverage), `StopToken`, `IClock`/`SystemClock`,
  `ILogger`/`CaptureLogger`; `digest_test.cpp` and `primitives_test.cpp`
  added to `make core-test`/`sanitize`/`core-coverage`. Stage-0 bootstrap
  driver (`bootstrap/main.cpp`) and stage-1 artifact (`bootstrap/stage1.cpp`)
  implemented: the pinned compiler compiles stage-1 against the installed
  Boost without any external build system, and stage-1 self-checks Boost
  provenance and the amp-core empty-input digest vector. `make bootstrap`
  (BOOST_VERSION=1.92.0 BOOST_COMMIT=afdfa32505af73e3d208144b3f623f0096cb62b6)
  produces and verifies `.bootstrap/bin/amp-stage1`. Sanitizers re-run green
  (UBSan caught a null-pointer memcpy on empty digests, fixed with a size
  guard); coverage gate green at 84.5% lines / 71.7% branches
  (baseline 79/66 unchanged until T05). API.md, TASKS.md and
  COVERAGE-WAIVER.md updated; all five amp-core test suites pass.
