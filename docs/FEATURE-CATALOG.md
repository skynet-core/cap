# Complete planned feature catalog

Status: planned, not implemented. Each item must have acceptance tests and a documented owner.

## Project authoring
- TOML manifest and versioned validation; arbitrary layouts; explicit `[[bin]] entry`, `sources`, `exclude`, `include-dirs`, link dependencies; static/shared/header-only libraries; multi-bin packages; examples and benchmarks; source glob determinism.
- Go-like OS and architecture filename suffixes (`_windows`, `_linux`, `_freebsd`, `_darwin`, `_amd64`, `_arm64`) and `_test.cpp`; manifest target conditions; preserve legacy filenames by explicit conditions.
- C and C++ standards independently selectable; C++20+ modules first class; compiler-specific BMI invalidation; generated sources through explicit declared actions only.
- Optional additive features with preprocessor definitions; typed project options (bool/enum/int/string) for configuration not suited to additive features; generated config header optional.
- Build profiles dev/release/bench/profile/custom; optimization, debug symbols, LTO, strip, frame pointers, PGO, target-specific flags and validation.

- Target-level declarations for system-library linkage and runtime-loaded shared libraries. See [ADR-0008](ADR/0008-system-linkage-and-standalone-install.md).
- Opt-in `build.lua` generators with declared inputs and outputs. See [ADR-0009](ADR/0009-lua-build-scripts.md).

## Workspaces and dependencies
- Virtual and package workspace roots, members/default-members/exclude, workspace inheritance, package selection, shared resolution, target dependency DAG, single committed workspace lock.
- Registry, Git (commit/tag/branch resolved to immutable revision), path, workspace, optional, target-scoped, dev-only dependencies; semantic version solver, feature unification and conflict explanations.
- Committed patches and ignored `.amp/config.local.toml` overrides; CLI overrides; precedence CLI > local > committed patch > normal; effective local resolution recorded separately; publishing disallows uncommitted local substitutions.
- TOML lockfiles with canonical package IDs, checksums, dependency edges; `--locked`, `--frozen`, offline; toolchain lock and workspace plugin lock separate.

## Toolchains and execution
- Download/verify/install/update/remove managed LLVM/Clang, GCC where distributable, WASI SDK and Emscripten; explicit compiler/linker/stdlib/sysroot/runtime components; platform SDK licensing-aware integration; no implicit PATH fallback.
- Host/target triples, cross-compilation, target capability negotiation, WebAssembly/WASI execution via runner, Windows/macOS/Linux/FreeBSD platform handling as supported.
- Native action DAG, parallel execution, incremental compilation, compiler dependency scans, C++ modules, deterministic linking, compilation database per workspace/configuration, global source/artifact CAS and GC.

## Testing and performance
- `*_test.cpp` discovery, platform filtering, dev dependencies, framework-neutral runner and adapters for Google Test/Catch2 v3, test filters, parallel tests, cross-target runners, reports.
- ASan, UBSan, TSan, MSan, leak sanitizer where supported, coverage, benchmarking, sampling profiling, PGO, frame pointers, baseline comparison, host capability matrix.

## Distribution and extensibility
- `amp install --standalone` installs a relocatable `bin/` and `lib/` tree for a compatible Unix target, starting with Linux/ELF. CLI and environment overrides select the root.
- Registry index, package fetch/publish/yank, checksums, auth, private registries and Git, provenance and signatures; registry server as separate deployment.
- Cargo-style `amp-*` executable command plugins; install/list/search/update/uninstall, versioned manifest, workspace plugin locking, Git/path distribution, PATH opt-in, built-in precedence; optional future structured JSON protocol and read-only observers; no arbitrary build-graph mutation.
- Manifest-declared CMake, Meson and Autotools library dependencies prepared by explicit, independently installed [import plugins](ADR/0010-manifest-library-import-plugins.md). Native builds use verified local results and never invoke the foreign build tools.

## CLI
`new`, `init`, `add`, `remove`, `build`, `run`, `install`, `check`, `test`, `bench`, `profile`, `clean`, `update`, `fetch`, `publish`, `login`, `toolchain`, `cache`, `override`, `plugin`, `graph`, `metadata`, `doctor`.

## Explicit non-goals
No built-in CMake/Meson/Autotools importer or compatibility backend; no implicit third-party build-script execution; no mandatory repository layout; no inline test macro facility; no arbitrary dynamically loaded C++ plugin ABI; no guarantee that all toolchains/SDKs can be redistributed; no universal sanitizer support.

## LSP and compilation database (mandatory)
Standard `compile_commands.json` export, managed compiler absolute paths, multi-project workspace aggregation, platform filtering, `_test.cpp` opt-in, profile/feature/target variants, generated paths and C++ module limitations, explicit active configuration, automatic export on build/check/test and no-build `amp compdb generate`. Owned by `amp-compdb`.

## Editor integration
First-class clangd-compatible compilation database generation and official VS Code extension with build/run/test/debug/profile, target selection, multi-root workspaces and managed toolchain status.


## v1.3 Boost/bootstrap policy
GNU Make is an explicit amp bootstrap exception. Root Makefile downloads latest stable Boost sources, bootstraps upstream b2, and compiles selected Boost libraries. Release/CI builds pin and verify exact Boost revision. No CMake/Meson/Ninja backend for amp packages. See `BOOTSTRAP.md` and `BOOST-POLICY.md`.
