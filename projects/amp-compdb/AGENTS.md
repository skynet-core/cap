# Agent instructions — amp-compdb

Read repository `AGENTS.md`, `docs/ARCHITECTURE.md`, `docs/QUALITY.md`, and this module's DESIGN/CONTRACTS/GOALS before coding. Use C++26 and SOLID. Only edit this module and approved amp-build/CLI contract integration points. Do not invoke external build systems. Preserve exact build-plan commands; never guess flags. Add `_test.cpp` unit tests, integration tests with clangd where available, coverage, local sanitizer checks, benchmarks and profiling evidence. Ask for contract review before modifying shared types. Never silently switch an editor's active configuration.

## API completeness rule
Keep API.md synchronized with every introduced, renamed, or removed public type; add compile-time and behavioral tests for each.
