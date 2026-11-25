# API and class design — amp-manifest

**Purpose:** TOML project schema and source conventions.

**Status:** proposed interfaces. No signatures below should be considered an implemented or ABI-stable API.

## M1-B01 planned pure reader

`amp::manifest::parse_manifest(std::string_view bytes, std::string_view source)`
returns `amp::core::Result<Manifest>`; both arguments are borrowed only during
the call. The value types own all strings and vectors:

* `Package`: name, version, optional license and publish.
* `Language`: optional c and cpp standards.
* `Binary`: name, entry, sources, exclude, include_dirs, depends_on,
  system_libraries.
* `Library`: name, type (default `static`), sources, headers,
  public_include_dirs, system_libraries.
* `Workspace`: members, default_members, exclude, optional resolver.
* `PathDependency`: name and path.
* `Manifest`: optional package, library and workspace; language; ordered bins
  and dependencies.

The pure call is reentrant and uses no filesystem, process, network or
cancellation. Invalid input returns an owned `invalid_input` diagnostic with
source and one-based line/column; allocation failure is not converted. This is
the v1 subset in CONTRACTS.md, not a full TOML implementation. Behavioral and
compile tests and parser benchmark/coverage evidence are required. This
boundary is independent of the planned filesystem-based `ManifestLoader`.

## Component and class inventory

## M1-B02 source selection

`amp.manifest.sources` exports `select_sources(const std::string &canonical_root,
const Binary|Library &, const core::IFileSystem &) -> core::Result<std::vector<std::string>>`.
Arguments are borrowed for the call; the sorted, package-relative result is
owned. Calls are reentrant subject to the injected filesystem. See CONTRACTS.md
for the supported Linux x86_64 grammar, errors and filesystem limitations.

| Type | Kind | Planned methods / fields |
|---|---|---|
| `ManifestLoader` | service | `load(path)->Result<Manifest>` |
| `ManifestValidator` | service | `validate(Manifest)->Diagnostics` |
| `PackageManifest` | value | `package, language, targets, dependencies, features, profiles` |
| `TargetSpec` | variant | `BinaryTarget, LibraryTarget, TestTarget` |
| `SourceSelector` | service | `select(globs,target,host)->Result<SourceSet>` |
| `PlatformSuffixParser` | service | `classify(filename)->PlatformCondition` |
| `OptionSchema` | value | `boolean, enum, integer, string and constraints` |

## Interface and lifecycle contract

- Public request values are immutable or owned; avoid borrowed views that outlive a call.
- Fallible methods return `std::expected<T, Diagnostic>` or a documented error domain.
- Inject filesystem, process, clock, network and store dependencies through narrow interfaces (DIP).
- Interfaces represent one responsibility; concrete adapters implement platform-specific behavior (SRP/ISP).
- No implicit shell commands or untracked mutable process-global state.
- Cancellation, timeouts, resource cleanup, and concurrency safety must be specified for I/O APIs.
- Serialization formats and persisted identifiers are versioned; deterministic ordering is mandatory.

## Invariants

Arbitrary layout; explicit entry; _test.cpp excluded from production; deterministic globs.

## Verification requirements

- **Unit / property tests:** ambiguous suffix, excluded test, duplicate ownership, symlinks, unknown TOML fields.
- **Benchmarks:** glob enumeration 100k files.
- **Coverage:** >=85% line, >=75% branch where available; >=95% line for critical algorithms.
- **Profiling:** capture baseline, flamegraph or equivalent sample evidence, and regression comparison for hot paths.
- **Sanitizers:** ASan/UBSan, TSan for concurrency, MSan on supported instrumented toolchain; document unsupported combinations.
- **SOLID review:** explain each new public type responsibility, interface segregation, and substitution tests.

## Definition of done

Every public type has an implementation, compile-time API test, negative-path tests, ownership and thread-safety documentation, a contract fixture, and performance evidence where relevant. An inventory entry is not evidence that code exists.
