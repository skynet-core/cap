# API and class design — cap-bootstrap

**Purpose:** GNU Make self-hosting bootstrap with upstream Boost source build.

**Status:** proposed interfaces. No signatures below should be considered an implemented or ABI-stable API.

The stage-0 implementation privately imports `cap.bootstrap.stage_status` to validate stage-1 JSON; its narrow boolean validator is not a public API. See [CONTRACTS.md](CONTRACTS.md).

## Component and class inventory

| Type | Kind | Planned methods / fields |
|---|---|---|
| `BootstrapConfig` | value | `toolchain_url, digest, trust_root, sources, output` |
| `BootstrapDriver` | service | `run(config)->Result<Stage1Binary>` |
| `ToolchainFetcher` | service | `fetch_verified(url,digest)->Result<Path>` |
| `DirectCompiler` | service | `compile_units(inputs,flags), link(objects,output)` |
| `BootstrapVerifier` | service | `compare_stage_outputs(stage1,stage2)` against the pinned fixture and evidence in [ADR-0003](../../docs/ADR/0003-bootstrap.md#stage-comparison-design) |

## Interface and lifecycle contract

- Public request values are immutable or owned; avoid borrowed views that outlive a call.
- Fallible methods return `std::expected<T, Diagnostic>` or a documented error domain.
- Inject filesystem, process, clock, network and store dependencies through narrow interfaces (DIP).
- Interfaces represent one responsibility; concrete adapters implement platform-specific behavior (SRP/ISP).
- No implicit shell commands or untracked mutable process-global state.
- Cancellation, timeouts, resource cleanup, and concurrency safety must be specified for I/O APIs.
- Serialization formats and persisted identifiers are versioned; deterministic ordering is mandatory.

## Invariants

GNU Make orchestrates cap bootstrap; upstream Boost bootstrap.sh/b2 may build Boost only. No CMake/Meson/Ninja or third-party backends for cap projects. Record Boost version, commit, compiler and ABI; offline mode requires preseeded verified sources.

## Verification requirements

- **Unit / property tests:** tampered archives, missing compiler, offline bundles, reproducible stage-2, clean host.
- **Benchmarks:** cold bootstrap time, archive extraction speed.
- **Coverage:** >=85% line, >=75% branch where available; >=95% line for critical algorithms.
- **Profiling:** capture baseline, flamegraph or equivalent sample evidence, and regression comparison for hot paths.
- **Sanitizers:** ASan/UBSan, TSan for concurrency, MSan on supported instrumented toolchain; document unsupported combinations.
- **SOLID review:** explain each new public type responsibility, interface segregation, and substitution tests.

## Definition of done

Every public type has an implementation, compile-time API test, negative-path tests, ownership and thread-safety documentation, a contract fixture, and performance evidence where relevant. An inventory entry is not evidence that code exists.


## v1.3 mandatory bootstrap update
Implement and test the root `Makefile` stages `boost-fetch`, `boost-bootstrap`, `boost-build`, `bootstrap`, and `self-host`. Boost must be compiled from source, not merely downloaded as headers. The default is latest stable Boost, with exact tag/commit pinned for releases. Add negative tests for missing tools, bad commit, interrupted downloads, incompatible compiler ABI and absent bootstrap source. See `docs/BOOTSTRAP.md` and `docs/BOOST-POLICY.md`.

## v1.4 proposed bootstrap interface additions (not implemented)
- `BootstrapInputLock` (value): verified Boost tag/full commit, submodule revisions, compiler identity, selected components.
- `BootstrapVerifier` (service): validate provenance and stage identity before reuse.
