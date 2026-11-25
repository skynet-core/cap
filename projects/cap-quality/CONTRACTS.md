# Contracts — cap-quality

## Public boundary
Responsibility: Sanitizers, coverage, benchmarks, profiling. Define narrow request/result interfaces in `include/cap/quality/`. Interfaces are specifications, not compiled headers yet.

## Required contract fields
- Request: typed input identity, configuration, target/host context as applicable, cancellation token, provenance.
- Result: typed value or structured diagnostic with code, location, causal chain, remediation.
- Determinism: stable ordering and canonical encoding for persisted/hashed output.
- Ownership: caller owns request; returned artifacts immutable or move-owned; no dangling filesystem or string views.
- IO: injected adapter for filesystem/process/network; no hidden global mutable state.
- Errors: invalid input, missing resource, unsupported capability, integrity failure, cancellation, transient IO.
- Compatibility: version all serialized protocols and files; migrations require ADR and golden fixtures.

## Module-specific invariant
Sanitizer flags applied at compile and link; unsupported combinations rejected; profile modes reproducible.

## Contract tests
ASan and TSan incompatible, MSan instrumented deps, benchmark regressions, PGO stages.

## Acceptance
Every public API has documented pre/postconditions, unit and integration tests, benchmark for hot paths and explicit failure behavior before implementation is considered complete.

## Exhaustive planned type inventory
See [API.md](API.md). Every type there requires a public contract and verification before merge.

## v1.4 normative additions
Gate memory-aware scheduler, isolation negative fixtures, redaction, cache-miss explanation and reproducible bootstrap in local verification on supported hosts.
