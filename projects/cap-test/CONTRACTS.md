# Contracts — cap-test

## Public boundary
Responsibility: Go-style source-file tests. Define narrow request/result interfaces in `include/cap/test/`. Interfaces are specifications, not compiled headers yet.

## Required contract fields
- Request: typed input identity, configuration, target/host context as applicable, cancellation token, provenance.
- Result: typed value or structured diagnostic with code, location, causal chain, remediation.
- Determinism: stable ordering and canonical encoding for persisted/hashed output.
- Ownership: caller owns request; returned artifacts immutable or move-owned; no dangling filesystem or string views.
- IO: injected adapter for filesystem/process/network; no hidden global mutable state.
- Errors: invalid input, missing resource, unsupported capability, integrity failure, cancellation, transient IO.
- Compatibility: version all serialized protocols and files; migrations require ADR and golden fixtures.

## Module-specific invariant
Tests excluded from production; platform suffix applied; dev dependencies isolated; no inline-test macros.

## Contract tests
Linux-only tests, multiple binaries, fixture linking, filter, cross-target runner, failure reporting.

## Acceptance
Every public API has documented pre/postconditions, unit and integration tests, benchmark for hot paths and explicit failure behavior before implementation is considered complete.

## Exhaustive planned type inventory
See [API.md](API.md). Every type there requires a public contract and verification before merge.
