# Design — cap-lock

## Responsibilities
Canonical package IDs; dependency edges; checksum; feature resolution; lockfile diff; local effective state.

## Invariants
Stable ordering; atomic rewrite; --locked rejects edits; --frozen forbids network; toolchain separate lock.

## Design approach
Use narrow, dependency-injected C++26 interfaces. Separate pure domain operations from filesystem, network and process adapters. Use `std::expected`-style structured errors, RAII, explicit cancellation and stable identities. Define input validation at the boundary; retain provenance for diagnostics. Do not expose implementation-specific Boost types in public cross-module APIs unless approved. Keep parsing, planning and execution separate. Respect root architecture and SOLID policy.

## Data flow
Caller supplies validated request -> module validates its own preconditions -> pure transformation or injected backend operation -> typed result and diagnostics -> caller persists/acts. Failure must not leave partially committed state; operations modifying disk use atomic staging when possible.

## Integration
Depends on `cap-core` for diagnostics/process/path/digests; other dependencies must be explicitly declared in CONTRACTS and reviewed. No implicit call to external build systems. Instrument metrics and trace IDs for profileable operations.

## Failure and security design
Handle malformed input, unsupported host/target capability, cancellation, filesystem races, resource exhaustion and inconsistent cached metadata. Never log secrets. Treat downloaded archives, manifest paths and plugin executables as untrusted until validated.

## Verification design
Unit, integration and negative fixtures: Round trip, lock merge, concurrent writes, source replacement, frozen missing cache. Use deterministic fixtures, concurrency/fault injection when relevant. Report line/branch coverage, sanitizer capability matrix, benchmark baseline and profiling trace.

## Open questions
List unimplemented capabilities, compiler/toolchain compatibility gaps and public contract changes in TASKS.md; do not silently guess semantics.
