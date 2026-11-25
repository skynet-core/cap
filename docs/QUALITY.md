# Mandatory engineering quality standard

## Code quality
C++26 mandatory for amp implementation. SOLID mandatory and reviewed at every public boundary. Strong types, explicit ownership, RAII, `std::expected`-style errors, no unchecked exceptions across module boundaries, cancellation-aware process/network APIs, structured diagnostics. Prefer Boost.Asio/Process/Graph/JSON when appropriate; TOML parser such as toml++; SQLite for cache metadata. C++26 reflection only behind `amp-meta` and proven compiling fixtures. All nontrivial APIs require contracts, invariants and negative tests.

## C++ Core Guidelines review

Review AMP-owned C++, including bootstrap code, tests and fixtures, against the
[C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines),
starting with [F: Functions](https://isocpp.github.io/CppCoreGuidelines/CppCoreGuidelines#s-functions).
Check that each function performs one logical operation (F.2–F.3), parameter
types express ownership, lifetime and direction (F.16–F.21, F.24), and no
result or escaping lambda refers to an object that does not outlive it
(F.43, F.53). Review exception behavior (F.6), invariants, initialization and
resource ownership in the connected code. Preserve the repository's contracts
for C++26 named modules, structured errors and exact compile-command capture.

New and changed C++ requires a review on the exact candidate commit. Record
the scope, findings by rule and location, and the reason for any intentional
deviation, such as an unavoidable platform ABI boundary. The reviewer must
accept deviations; use the waiver process below where a gate cannot be met.
Audit unchanged sources by ownership lane and close or explicitly waive
findings before claiming whole-tree conformance. Static checks support this
review but cannot establish conformance on their own; no new build
dependency is required.

## Pre-review static checks

Terra, Luna and fallback Qwen must format changed source and test files and pass all applicable static checks before submitting a patch. The [workflow profile](../tools/agent-harness/ai-workflow.yaml) lists absolute tool paths and command prefixes. Use those binaries, not versions resolved via `PATH`.

| Changed code | Required checks |
|---|---|
| C++ sources and headers | Run `/opt/clice/bin/clice serve` through an LSP client with the candidate's actual compile commands for affected build configurations. Check changed sources and a consuming translation unit for each changed header. Capture a completed diagnostic result with zero warnings or errors; a running server or silence is not a pass. Run `clice lint` from the same installation against the affected build configurations and `clang-format --dry-run --Werror` on changed files. |
| Bash and shell scripts, including extensionless scripts | Run `shellcheck` for the script's declared dialect and `shfmt -d` on changed scripts. |
| Python | Run `ruff check`, `ruff format --check`, `isort --check-only`, and `pyrefly check --min-severity warn` on the changed files. |

Pass means no formatting differences, warnings or lint/type errors in the checked scope. Record the exact commit, tool versions, commands, checked files or build configurations, exit statuses and diagnostics. A missing tool or compile command, incomplete diagnostics, stale evidence or a failed or skipped applicable check blocks review. Record non-applicability only when the change does not touch that language; workers must not suppress findings or weaken check settings to pass. These checks do not replace tests, sanitizers or the separate compilation database compatibility gate below.

## Coverage gates (initial policy)
- Each production module: >=85% line and >=75% branch coverage where instrumented coverage is supported.
- Resolver, lockfile, source selection, security-sensitive extraction and action fingerprinting: >=95% line coverage.
- Coverage must include unit and integration suites; exclusions must be documented and reviewed; generated code and external vendored code excluded explicitly.
- Coverage does not replace property-based, fuzz, fault-injection or integration tests.

## Testing matrix
Unit tests `*_test.cpp`; contract tests for each backend; end-to-end fixture with workspace library + two bins + platform files + test files; deterministic golden TOML; fuzz TOML parser and archive extraction; fault injection for network, disk and process failures; local cross-platform checks on Linux/macOS/Windows and FreeBSD where feasible; cross-target WASI smoke; registry/private Git auth fixtures without real secrets.

## Sanitizers
ASan+UBSan on supported Clang configurations; TSan in separate jobs for concurrency; MSan only with fully instrumented compatible runtime/dependencies; leak checks where available. Reject unsupported sanitizer combinations; never claim universal support.

## Benchmark and profiling policy
Each performance-sensitive module must define named benchmarks, fixed input datasets, warmup, repetition, baseline, environment/toolchain metadata, median and dispersion, regression budget and profiler output. Default regression review threshold: >10% sustained median slowdown or >10% memory growth, subject to variance and approved exceptions. Track resolver graph sizes, scheduler throughput, cache hit/miss latency, TOML parsing, source scanning, toolchain download/extract and registry operations. Profile before optimization and archive before/after evidence.

## Merge and release gates
Run the applicable checks locally on the exact candidate commit: build with the pinned C++26 toolchain, unit/integration/golden tests, coverage, sanitizers on supported targets, format/static analysis, license/security checks, bootstrap-from-clean-host smoke, self-host stage comparison, frozen/offline build smoke, documentation/API contract updates. Also require the [Linux build and test job](../.github/workflows/ci.yml) on the exact candidate SHA. It checks the compiler/module probe, source-built Boost and stage-1 bootstrap, core unit tests, the development CLI, and the four native build modes. It does not test `make self-host` or replace local sanitizer, coverage, or editor gates. Record the commit, toolchain, commands, exit statuses, logs and CI URL; document unsupported configurations without marking them passed. All regressions need a test. No fabricated benchmarks or green checks. Waivers must state owner, reason, expiry and mitigation.

## LSP compatibility gate
Every change to compiler command planning or target filtering must pass amp-compdb golden JSON tests and clangd workspace smoke tests (where supported). Benchmark export speed for large workspaces and profile refresh latency.

For new or changed C++ declarations, review the [source documentation contract](API-DESIGN-RULES.md#source-documentation) before merging. On documentation changes, capture a representative clangd hover and a local generated-reference sample (including an implementation-only or private symbol when supported). Record unsupported tooling separately. Lint, formatting and a passing compiler do not verify that comments are present or accurate.

## Class-level verification
Every public class/interface must be inventoried in its module API.md with method contracts, ownership, error behavior, tests and performance expectations. SOLID review mandatory. The VS Code TypeScript extension uses extension-host tests, coverage, activation benchmarks and workspace-trust security tests.


## v1.3 Boost/bootstrap policy
GNU Make is an explicit amp bootstrap exception. Root Makefile downloads latest stable Boost sources, bootstraps upstream b2, and compiles selected Boost libraries. Release/CI builds pin and verify exact Boost revision. No CMake/Meson/Ninja backend for amp packages. See `BOOTSTRAP.md` and `BOOST-POLICY.md`.

## v1.4 additional mandatory verification
Scheduler: memory/CPU admission, starvation, process-tree cancellation, 10k-node benchmark and TSan stress. Isolation: undeclared read, environment leak, network attempt, generated-file race, SDK/toolchain change, symlink traversal, corruption and interrupted publication. Agent workflow: ownership checks, versioned contract fixtures and integration evidence. Observability: trace schema goldens, redaction, stable cache-miss reasons and measured overhead. Offline/preseeded Boost bootstrap must verify pinned provenance; an unverified checkout must fail. Remote execution is optional and must pass separate authentication, worker compatibility and integrity gates before release claims.
