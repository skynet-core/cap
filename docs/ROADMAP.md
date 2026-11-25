# Integrated roadmap and dependencies

All milestones require the quality gates in QUALITY.md. Sequence is dependency-based, not calendar promises.

## Phase 0 — Foundation / bootstrap
`cap-core`, `cap-bootstrap`: pinned C++26 compiler feasibility, stage-0 direct compiler bootstrap, signed/digested bundle acquisition, diagnostics/process/filesystem, test harness. Exit: stage-1 executable produced without external build system.

## Phase 1 — Manifest, workspace, toolchains, CLI
`cap-meta`, `cap-manifest`, `cap-workspace`, `cap-toolchain`, `cap-cli`: TOML parser, arbitrary-layout targets, Go-style source/test selection, managed LLVM install, workspace discovery. Exit: workspace with library and two binaries compiles through pinned managed Clang.

## Phase 2 — Native build and cache
`cap-build`, `cap-cache`: immutable DAG, incremental C/C++ compile, parallel scheduler, compile_commands.json, module scanning, action CAS. Exit: rebuild invalidates only affected nodes; C++ modules smoke passes.

## Phase 3 — Dependencies and lockfiles
`cap-resolver`, `cap-sources`, `cap-lock`: SemVer, registry/Git/path/workspace, optional/dev features, committed patches and local overrides, deterministic TOML locks. Exit: frozen offline workspace rebuild with verified inputs.

## Phase 4 — Tests and quality
`cap-test`, `cap-quality`: *_test.cpp, Google Test/Catch2 adapters, cross-target runner, ASan/UBSan/TSan/MSan capability matrix, coverage, benchmarks, profiler and PGO. Exit: locally verified coverage and performance gates.

## Phase 5 — Distribution and plugins
`cap-registry`, `cap-registry-server`, `cap-plugin`, `cap-plugin-sdk`: authenticated publishing, private registry, external command plugin installation, workspace plugin locks, structured protocol and [manifest-declared library imports](ADR/0010-manifest-library-import-plugins.md). Exit: publish/install/test private package; independently installed `cap-hello` command works; local CMake, Meson and Autotools library imports link into a native CAP executable without running foreign tools during `cap build`.

## Phase 6 — Hardening / release
Linux/macOS/Windows/FreeBSD coverage where supported, WASI and Emscripten, security/fuzz tests, offline bootstrap, stage-2 self-hosting, migration docs, release signing, benchmark baselines. Exit: reproducible supported-host release with documented exceptions.

## Parallel agent lanes
Lane A core/bootstrap; B manifest/meta; C workspace/toolchain; D build/cache; E resolver/sources/lock; F tests/quality; G registry/plugin/CLI. Freeze shared CONTRACTS before parallel implementation; integrate at each phase exit.

## Phase 2A — LSP compilation database compatibility
`cap-compdb` consumes immutable compile actions from `cap-build`; emits standard clangd-compatible compile_commands.json without compiling, supports explicit active configuration and per-target variants. Exit: clangd indexes multi-package fixture with platform-filtered and test sources; measured export performance and coverage gates pass.

## Phase 7 — VS Code developer integration
`cap-vscode` TypeScript extension: workspace trust, cap tasks, multi-root package/target/profile selection, managed toolchain status, clangd compilation database activation, Test Explorer, debugging, sanitizers, profiling, marketplace packaging. Requires stable cap-cli JSON contracts, cap-compdb and cap-test. Exit: extension-host tests pass on supported VS Code hosts and clangd indexes multi-project fixture.


## v1.3 Boost/bootstrap policy
GNU Make is an explicit cap bootstrap exception. Root Makefile downloads latest stable Boost sources, bootstraps upstream b2, and compiles selected Boost libraries. Release/CI builds pin and verify exact Boost revision. No CMake/Meson/Ninja backend for cap packages. See `BOOTSTRAP.md` and `BOOST-POLICY.md`.

## v1.4 sequencing amendment (supersedes milestone ordering where inconsistent)
See `RELEASE-PLAN-v1.4.md` for M0–M6 vertical slices. Contract freeze and mock-backed parallel work precede agent lane implementation; integrate by dependency gates. Memory-aware scheduling, build explanations, trace schema and action isolation contracts must be designed with Phase 2, even when remote execution is deferred. Remote cache follows local CAS and verified action keys; remote workers follow strict sandbox and threat-model gates. `capd`, remote workers and a web trace viewer are optional later enhancements, not first-release blockers.

## Development infrastructure track

Before unattended parallel implementation, establish disposable execution environments, task-specific worktrees, frozen acceptance and a runner with transactional claims. Use M0 as the first integration fixture for the [agent workflow](LOCAL-AGENT-OPERATIONS.md). Verify the worker and revision limits, invalid model bindings, failed checks and stale verdicts. Exercise the production approval gate with a test target before permitting real publication. Measure independently accepted throughput at progressively higher concurrency.
