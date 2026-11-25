# Contracts — amp-toolchain

## Public boundary
Responsibility: Managed compiler and SDK lifecycle. Define narrow request/result interfaces in `include/amp/toolchain/`. Interfaces are specifications, not compiled headers yet.

### M1 local host probe (`amp.toolchain.host`)
`HostTools` owns absolute executable paths for C compiler, C++ compiler, and archiver. `HostToolchain` owns those tools and their verified common host triple. `probe_host_toolchain(tools, runner)` is pure orchestration over an injected `amp::core::IProcessRunner`: direct argv only, empty environment allowlist, bounded 10-second invocations, no filesystem mutation/network/PATH lookup/install. It queries both GCC drivers with `-dumpmachine`, requiring exactly one newline-terminated line equal to `x86_64-linux-gnu` or `x86_64-pc-linux-gnu` and equal between drivers. It validates tool paths as absolute and NUL-free. The archiver is checked using `--version` (success status and nonempty output); GNU ar wrapper dependencies are outside this probe. Failures return structured diagnostics with tool/operation context. This is host identification only, not cross-compilation, authenticity, SDK resolution, or an action environment contract.

### M1 local system libraries (`amp.toolchain.host`)
`resolve_system_libraries(host, names, runner)` requires the caller's verified supported host and trusted absolute NUL-free C++ driver. It validates all names against ASCII `[A-Za-z0-9_+][A-Za-z0-9_+-]*` before execution. For each name, in order and including duplicates, it queries the driver directly with `-print-file-name=lib<name>.so`; only an exact echoed candidate triggers the corresponding `.a` query. Each invocation has an empty environment and 10-second timeout. Output must be exactly one LF-terminated line containing either the exact candidate or an absolute NUL-free path. Returned path bytes are unchanged and may identify linker scripts. Failure returns a structured diagnostic and no partial result; an empty list launches no process. No PATH discovery, shell, cross-target resolution or file inspection occurs. The injected runner governs concurrency; no global state is used.

## Required contract fields
- Request: typed input identity, configuration, target/host context as applicable, cancellation token, provenance.
- Result: typed value or structured diagnostic with code, location, causal chain, remediation.
- Determinism: stable ordering and canonical encoding for persisted/hashed output.
- Ownership: caller owns request; returned artifacts immutable or move-owned; no dangling filesystem or string views.
- IO: injected adapter for filesystem/process/network; no hidden global mutable state.
- Errors: invalid input, missing resource, unsupported capability, integrity failure, cancellation, transient IO.
- Compatibility: version all serialized protocols and files; migrations require ADR and golden fixtures.

## Module-specific invariant
Immutable installs; no PATH fallback; toolchain host/target identity; licensed SDK handling; atomic extraction.

## Contract tests
Tampered archive, zip-slip, interrupted downloads, unsupported sanitizer/target, offline install, concurrent installs.

## Acceptance
Every public API has documented pre/postconditions, unit and integration tests, benchmark for hot paths and explicit failure behavior before implementation is considered complete.

## Exhaustive planned type inventory
See [API.md](API.md). Every type there requires a public contract and verification before merge.

## v1.4 normative additions
Provide stable compiler/SDK identity for action fingerprints; feature probes are compile-and-run or compile-only as appropriate, not just flag acceptance.
