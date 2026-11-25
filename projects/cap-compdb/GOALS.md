# Goals — cap-compdb

## Required behavior
- Produce Clang JSON Compilation Database compatible `compile_commands.json` for C/C++ translation units using the **exact planned compile invocation**.
- Support arbitrary project layouts, multiple packages and targets, managed toolchains, feature sets, platform suffix selection, profiles and optional test sources.
- Generate without compiling/linking; permit dependency scanning/planning as necessary and report unavailable generated inputs.
- Generate stable per-configuration databases and expose one explicitly selected active workspace database.
- Integrate with `cap build`, `cap check`, `cap test`, and `cap compdb` commands; preserve editor stability.

## Non-goals
- Implementing an LSP server, replacing clangd, synthesizing fake header compile commands, or promising module BMI availability from JSON alone.

## Acceptance
- clangd successfully indexes a workspace library, two binaries, target-specific sources and tests using generated database.
- Command fidelity is tested against actual build plan; stale configuration detection works; JSON passes schema and parser checks.
- Meets repository quality gates (coverage, sanitizers, profiling, benchmarks, SOLID, C++26).
