# API and class design — amp-toolchain

**Purpose:** Download, verify, install, and select compilers and SDKs.

**Status:** proposed interfaces. No signatures below should be considered an implemented or ABI-stable API.

## Component and class inventory

### Implemented local host probe

The named module `amp.toolchain.host` exports owned `HostTools { c_compiler, cpp_compiler, archiver }`, `HostToolchain { tools, host_triple }`, and `probe_host_toolchain(const HostTools&, const amp::core::IProcessRunner&) -> amp::core::Result<HostToolchain>`. Strings are absolute executable paths. Caller must supply trusted executable paths; the probe executes each supplied tool directly. It uses empty environment allowlists and accepts only identical exact `x86_64-linux-gnu` or `x86_64-pc-linux-gnu` output from the two GCC drivers. It invokes the archiver with `--version`, checking successful exit and nonempty output. Each call is bounded to 10 seconds. Any process/timeout/status/output failure returns a contextual structured diagnostic. It performs no installs or other filesystem/network operations. Tests are `tests/host_test.cpp`; the separately run local smoke uses explicit gcc-16/g++-16 and `/usr/bin/ar`.

`amp.toolchain.host` also exports `resolve_system_libraries(const HostToolchain&, const std::vector<std::string>&, const amp::core::IProcessRunner&) -> amp::core::Result<std::vector<std::string>>`. The caller supplies a verified host toolchain and trusted C++ driver. Results own their ordered paths. The function has no shared state; concurrent use requires a thread-safe runner. Calls are bounded to 10 seconds each, without cancellation. Invalid input, unsupported target, process errors, malformed output and missing libraries return diagnostics. It does not check file contents; GCC may return a linker script. See `tests/system_libraries_test.cpp`; no persistent format or hot-path performance guarantee is introduced.

| Type | Kind | Planned methods / fields |
|---|---|---|
| `ToolchainCatalog` | interface | `query(host,target,requirements)->Releases` |
| `ToolchainResolver` | service | `resolve(request)->Result<ToolchainIdentity>` |
| `ToolchainInstaller` | service | `install(identity)->Result<InstalledToolchain>` |
| `ToolchainStore` | service | `get(digest), activate(digest), gc()` |
| `CapabilityProbe` | interface | `probe(compiler,target)->Capabilities` |
| `CompilerBackend` | interface | `compile(request), link(request), scan_modules(request)` |
| `TargetDescriptor` | value | `triple, sysroot, abi, runtimes, capabilities` |

## Interface and lifecycle contract

- Public request values are immutable or owned; avoid borrowed views that outlive a call.
- Fallible methods return `std::expected<T, Diagnostic>` or a documented error domain.
- Inject filesystem, process, clock, network and store dependencies through narrow interfaces (DIP).
- Interfaces represent one responsibility; concrete adapters implement platform-specific behavior (SRP/ISP).
- No implicit shell commands or untracked mutable process-global state.
- Cancellation, timeouts, resource cleanup, and concurrency safety must be specified for I/O APIs.
- Serialization formats and persisted identifiers are versioned; deterministic ordering is mandatory.

## Invariants

Immutable verified bundles; target-aware capabilities; licensed SDK constraints.

## Verification requirements

- **Unit / property tests:** digest mismatch, zip-slip, atomic activation, unsupported sanitizer, interrupted install.
- **Benchmarks:** download/cache hit and compiler startup.
- **Coverage:** >=85% line, >=75% branch where available; >=95% line for critical algorithms.
- **Profiling:** capture baseline, flamegraph or equivalent sample evidence, and regression comparison for hot paths.
- **Sanitizers:** ASan/UBSan, TSan for concurrency, MSan on supported instrumented toolchain; document unsupported combinations.
- **SOLID review:** explain each new public type responsibility, interface segregation, and substitution tests.

## Definition of done

Every public type has an implementation, compile-time API test, negative-path tests, ownership and thread-safety documentation, a contract fixture, and performance evidence where relevant. An inventory entry is not evidence that code exists.

## v1.4 proposed additions (not implemented)
- `ToolchainIdentity` includes compiler binary digest, stdlib/SDK identity and capability probe results.
