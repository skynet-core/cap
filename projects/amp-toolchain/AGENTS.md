# Agent instructions — amp-toolchain

Own `projects/amp-toolchain` only unless an interface change is explicitly approved. Read root AGENTS.md, ARCHITECTURE.md, QUALITY.md and this module's DESIGN/CONTRACTS before coding. Mandatory C++26, SOLID, separate `*_test.cpp`, coverage, sanitizers, benchmark/profile evidence. Do not introduce external build-system compatibility. Follow root task workflow. Specific invariants: Immutable installs; no PATH fallback; toolchain host/target identity; licensed SDK handling; atomic extraction.

## API completeness rule
Keep API.md synchronized with every introduced, renamed, or removed public type; add compile-time and behavioral tests for each.
