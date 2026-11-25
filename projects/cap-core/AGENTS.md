# Agent instructions — cap-core

Own `projects/cap-core` only unless an interface change is explicitly approved. Read root AGENTS.md, ARCHITECTURE.md, QUALITY.md and this module's DESIGN/CONTRACTS before coding. Mandatory C++26, SOLID, separate `*_test.cpp`, coverage, sanitizers, benchmark/profile evidence. Do not introduce external build-system compatibility. Follow root task workflow. Specific invariants: No dependency on other cap modules; stable error taxonomy; subprocess arguments never shell-expanded.

## API completeness rule
Keep API.md synchronized with every introduced, renamed, or removed public type; add compile-time and behavioral tests for each.
