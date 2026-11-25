# amp — engineering specification v1.4

**Awesome Modern Packager**: a native C/C++ package manager and build engine in development using C++26. The current CLI builds standalone binaries and static libraries, and local workspaces with static-library dependencies.

## Frozen decisions

- No CMake, Meson, Autotools, Ninja, Bazel or other build backend in AMP core; GNU Make is bootstrap-only for AMP. Manifest dependencies may use independently installed [library import plugins](docs/ADR/0010-manifest-library-import-plugins.md) to prepare third-party libraries separately from native builds (planned).
- Managed downloadable toolchains; versioned catalog and immutable verified installs. Apple and Microsoft SDK licensing must be respected.
- TOML manifests and lockfiles; arbitrary project directory layouts; explicit executable entry points and source sets.
- `*_test.cpp` translation units (not inline tests); Go-style `_linux`, `_windows`, `_darwin`, `_freebsd`, and architecture suffixes.
- Minimize preprocessor use; optional features may map to preprocessor definitions. Existing C/C++ ecosystem and header guards are not forbidden.
- Workspace-wide multi-package graph and lockfile; ignored `.amp/config.local.toml` for local overrides.
- Cargo-style external executable plugins, optional structured protocol later; never load arbitrary C++ ABI plugins.
- SOLID, C++26, tests, coverage, benchmarks, profiling, sanitizer checks and security review are mandatory.
- Bootstrap uses a system C++26 compiler, Git, GNU Make, and Boost built from source using upstream bootstrap.sh/b2; amp builds never delegate to external build systems.

## Reading order

1. `docs/ARCHITECTURE.md`
2. `docs/FEATURE-CATALOG.md`
3. `docs/SCHEMAS.md`
4. `docs/BOOTSTRAP.md`
5. `docs/QUALITY.md`
6. `docs/ROADMAP.md`
7. Each `projects/<module>/` directory (DESIGN, GOALS, CONTRACTS, ROADMAP, TASKS, AGENTS)

There are 18 subsystem specifications. Interfaces and TOML examples are **proposals requiring conformance tests**; toolchain versions are not claims of availability.

Contributors: see the [C++ source documentation rules and local reference generation](docs/API-DESIGN-RULES.md#source-documentation).

AMP's own code and documentation are licensed under [Apache-2.0](LICENSE);
see [NOTICE](NOTICE) for attribution. Third-party components retain their own
licenses.

Use `make bootstrap-dev` with an existing verified source-built Boost install
to build the development CLI. See the [local bootstrap instructions](docs/BOOTSTRAP.md#local-development-cli).
Workspace builds support local static dependencies and declared SDK libraries.
On the supported Linux/GCC toolchain, C++ named-module imports can cross
local static-library dependencies. Only modules from the selected target's
dependency closure are visible. This does not support header units or
reuse of compiler module interfaces across incompatible toolchains.

## Editor / LSP compatibility
`amp-compdb` is the 19th module. It exports exact native compile actions to clangd-compatible `compile_commands.json`, with explicit configuration activation. See `projects/amp-compdb/`.

## v1.2 additions
- 20 planned modules including `amp-vscode` (TypeScript extension).
- Every module has an `API.md` enumerating planned classes/interfaces, responsibilities, contracts and quality gates.
- See `docs/COMPONENT-INVENTORY.md`, `docs/API-DESIGN-RULES.md`, and `docs/VSCODE-EXTENSION.md`.
- This is a design specification, **not a complete implementation or finalized method-by-method API**.

## v1.3 bootstrap and Boost
`Makefile` includes `boost-fetch`, `boost-bootstrap`, `boost-build`, and `bootstrap`. The v1.3 policy used latest stable Boost by default; v1.4 supersedes this with explicit pinned version and verified commit for every Boost build. See `docs/BOOST-POLICY.md`. The M0 bootstrap produces the stage-1 status driver; it is distinct from `bootstrap-dev`.

## v1.4 engineering revision (specification only)
This bundle adds normative scheduler, isolation, agent coordination and observability contracts, a staged release plan and future remote execution design. It strengthens bootstrap reproducibility: **Boost version and full commit must be explicitly provided**, rather than silently resolving `latest`. Obtain and authenticate a Boost pin before running Boost targets. Read `docs/RELEASE-PLAN-v1.4.md`, `docs/SCHEDULER.md`, `docs/BUILD-ISOLATION.md`, `docs/AGENT-COORDINATION.md`, `docs/BUILD-OBSERVABILITY.md`, and `docs/REMOTE-EXECUTION.md`. The original twenty-module scope remains intact; `amp-worker` and `amp-remote` are future proposals only.

## Agent development workflow

The development workflow assigns intake, planning and independent milestone review to Sol. Up to three Terra and five Luna workers share the seven ownership lanes and work against frozen acceptance criteria. Qwen may author code and tests only when verified provider usage limits block both primary models. Astra audits final delivery, and production requires human approval. Commit and push code changes to `origin` topic branches. The [Linux build and test workflow](.github/workflows/ci.yml) checks pushes and pull requests in addition to [local verification](docs/QUALITY.md#merge-and-release-gates). See the [agent workflow](docs/AGENT-WORKFLOW.md) and [harness configuration](docs/HARNESS-CONFIGURATION.md).
