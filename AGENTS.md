# Agent policy (mandatory)

Read README and architecture, feature catalog, schema and quality docs before changing interfaces. Own only assigned project paths; request review for cross-project contracts. Implement in C++26. Apply SOLID: single responsibility, interfaces for replaceable infrastructure, explicit dependency injection, composition over inheritance, narrow APIs. Do not add CMake/Meson/Make/Ninja or other external build-system dependencies. Every code change requires tests; performance-sensitive changes require reproducible benchmark and profile evidence. Run sanitizer and coverage jobs on supported targets; document unsupported jobs rather than pretending success. Never claim a stub or unrun test is complete. Use separate *_test.cpp files. Prefer filename/manifest platform selection over platform preprocessor switches. Keep local override files and secrets untracked. No unreviewed network/process execution. Record architectural changes in ADRs and update module CONTRACTS before code. No undocumented interface changes. Maintain deterministic output and structured diagnostics.

## Definition of done

CAP-owned native C++ uses **C++20 named modules**, compiled in **C++26 mode**. New internal APIs use `export` and `import`, not public headers. Existing header-based code is migration work, not an exception for new APIs. Follow [ADR-0007](docs/ADR/0007-native-cxx-modules.md).

Compiles with pinned C++26 toolchain; tests pass; coverage thresholds met or waiver documented; benchmark baseline recorded for hot paths; sanitizer jobs pass where supported; public API and schema docs updated; security and failure cases reviewed; no unexpected source/build-system dependencies; acceptance criteria verified with logs.

Compilation database compatibility is mandatory: preserve exact compile commands in `cap-build` and run `cap-compdb` regression tests when altering compile flags, targets, profiles, features or toolchains.

Every added/modified public class or interface MUST update its module API.md and corresponding tests. Consult docs/API-DESIGN-RULES.md.

Document every CAP-owned C++ class/struct, public member, named constant, free function and method (public or private) in source using the [documentation contract](docs/API-DESIGN-RULES.md#source-documentation). Document input/output types and semantics, error conditions and any exceptions that escape. Verify IDE hover and local reference generation for the documented surface; module API.md alone does not satisfy this requirement.

## v1.3 non-negotiable Boost/bootstrap rules
Use latest stable Boost in development; pin verified exact revision for CI/release. Root GNU Makefile must download Boost, run Boost bootstrap.sh, and compile Boost via b2. GNU Make and Boost.Build are bootstrap-only; cap package builds use cap native DAG exclusively. All cap production C++ remains C++26. Never claim bootstrap/test success without running it.

## v1.4 superseding requirements
`docs/AGENT-COORDINATION.md`, `docs/SCHEDULER.md`, `docs/BUILD-ISOLATION.md`, and `docs/BUILD-OBSERVABILITY.md` are normative. Freeze cross-module contracts before parallel work; use isolated branches and ownership checks. For all Boost fetch/build invocations, require explicit `BOOST_VERSION` and full verified `BOOST_COMMIT` (the v1.3 latest-by-default rule above is superseded). Do not claim hermeticity, remote caching, distributed compilation, a running daemon, or agent command implementation until demonstrated by tests.

## Agent development workflow
`tools/agent-harness/ai-workflow.yaml` and `docs/LOCAL-AGENT-OPERATIONS.md` define the development workflow. Sol owns intake, planning and independent milestone review. Terra and Luna are the only regular authors of code and tests; Qwen may author them only as a fallback when verified provider usage limits block both. Astra audits the final candidate. Freeze the request and acceptance criteria before dispatch, and enforce the configured worker and revision limits even for fallback work. Internal milestone promotion requires Sol approval of the exact locally verified commit. Production additionally requires Astra's final audit and human approval for the exact release inputs. No other model fallback is allowed.

Commit each scoped change on its assigned `origin` topic branch and open a task PR to `milestone/<name>`. The operator may rebase-merge a green task PR into its milestone. Open a separate milestone PR to `develop`; the operator must leave it for human review and merge. A merged milestone can produce a prerelease for manual verification, not a stable release. See the [branch policy](docs/LOCAL-AGENT-OPERATIONS.md#branch-policy). Keep credentials, private logs, and machine-local files out of Git. Terra, Luna and fallback Qwen must format their code and pass every applicable [static check](docs/QUALITY.md#pre-review-static-checks) before submission. Verify the same checks locally on the exact commit before sending it to reviewers.
