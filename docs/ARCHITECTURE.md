# Architecture

## Layering and allowed dependency direction

`amp-cli` orchestrates `amp-workspace`, `amp-manifest`, `amp-toolchain`, `amp-resolver`, `amp-lock`, `amp-build`, `amp-test`, `amp-quality`, `amp-registry`, `amp-plugin`. All depend on `amp-core`; only schema/metadata users depend on `amp-meta`. `amp-build` consumes immutable resolved plans and compiler capability descriptors; it does not perform network resolution. `amp-cache` provides content-addressed artifact storage to build and sources. `amp-sources` fetches package inputs; `amp-registry` handles registry transport/publishing. `amp-plugin-sdk` defines an external protocol independent of amp internal ABI. `amp-bootstrap` is an isolated stage-0 implementation. `amp-registry-server` deploys independently.

## Pipeline

Parse TOML -> validate workspace and target ownership -> resolve features/dependencies and local override state -> resolve and verify toolchain -> discover platform-eligible sources -> scan includes/modules -> freeze action DAG -> cache lookup -> parallel execution -> validate outputs -> materialize targets and compile_commands.json -> run tests/quality actions.

AMP's native C++ implementation uses C++20 named modules in C++26 mode. See [ADR-0007](ADR/0007-native-cxx-modules.md) for source boundaries, migration and verification.

## Core invariants

- Build plan immutable once execution begins; all action inputs/outputs declared.
- Paths are canonicalized relative to manifest; glob order stable; source ownership unambiguous.
- Every compile/link action fingerprint includes toolchain digest, target triple, normalized arguments, relevant environment, discovered inputs, dependency digests, profile, and feature configuration.
- Source files may be located anywhere; entry point identifies main translation unit but does not imply whole source set.
- `*_test.cpp` excluded from normal targets, included only in test builds; platform suffix selection applied before scheduling.
- Lockfile is deterministic and committed for applications; local overrides do not silently rewrite committed lock.
- Toolchains installed atomically, immutable, verified, with explicit host/target capability tables.
- Plugins are external commands; built-in commands take priority; PATH discovery disabled unless opted in.
- Network access, code generation, subprocess execution and credentials are explicitly modeled.

## SOLID application

S: parser, validator, resolver, planner, executor and publisher are separate responsibilities. O: source/toolchain backends implement narrow interfaces. L: backend substitutes must obey capability and error contracts. I: no god interfaces; separate resolve/fetch/install/compile/link operations. D: orchestration depends on abstractions, injected via constructors; concrete IO at composition root.

## Major risks

C++26 reflection compiler maturity; module dependency scanning portability; native build parity for complex C/C++ projects; signed toolchain distribution; SDK licensing; secure plugin execution; local overrides vs lock reproducibility; multi-target compilation databases. Track with ADRs and dedicated fixtures.

## Language-server interoperability
`amp-compdb` is a first-class core module, depending on a read-only compile-action projection from `amp-build`. It emits exact JSON compilation database commands for clangd; never implements its own LSP server or rebuilds flags independently. Configuration identity includes toolchain/profile/target/features/overrides/tests; active editor database switches explicitly. See `projects/amp-compdb/`.

## Component-level API inventory
See `COMPONENT-INVENTORY.md` and `projects/<module>/API.md` for every planned public class/interface, responsibilities, methods, invariants and verification requirements. All future types must be inventoried.

## VS Code integration
`amp-vscode` is a TypeScript extension consuming amp CLI and amp-compdb; it is not a native C++ module and does not implement LSP.


## v1.3 Boost/bootstrap policy
GNU Make is an explicit amp bootstrap exception. Root Makefile downloads latest stable Boost sources, bootstraps upstream b2, and compiles selected Boost libraries. Release/CI builds pin and verify exact Boost revision. No CMake/Meson/Ninja backend for amp packages. See `BOOTSTRAP.md` and `BOOST-POLICY.md`.

## v1.4 execution architecture (normative)
`amp-build` freezes a resource-annotated action DAG and schedules actions with CPU/memory admission, cancellation and fairness (`SCHEDULER.md`). `amp-core` provides process-tree termination and platform isolation adapters. `amp-cache` implements versioned fingerprints, verified CAS and atomic publication (`BUILD-ISOLATION.md`). Trace and cache explanation events are emitted by build/cache and displayed by CLI (`BUILD-OBSERVABILITY.md`). Strict isolation is required before remote execution claims; audit/off modes are explicitly non-hermetic. `amp-compdb` continues to consume the same immutable action projection. A future optional `ampd` daemon may cache metadata but is not authoritative; CLI must work daemonless. Remote caching/workers are future extensions (`REMOTE-EXECUTION.md`) and are not required to bootstrap AMP.
