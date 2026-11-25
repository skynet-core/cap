# Agent instructions — cap-bootstrap

Own `projects/cap-bootstrap` only unless an interface change is explicitly approved. Read root AGENTS.md, ARCHITECTURE.md, QUALITY.md and this module's DESIGN/CONTRACTS before coding. Mandatory C++26, SOLID, separate `*_test.cpp`, coverage, sanitizers, benchmark/profile evidence. Do not introduce external build-system compatibility. Follow root task workflow. Specific invariants: No CMake/Meson/Make/Ninja; bootstrap must state required host compiler/runtime/network; reproducible pinned input.

## API completeness rule
Keep API.md synchronized with every introduced, renamed, or removed public type; add compile-time and behavioral tests for each.


## v1.3 mandatory bootstrap update
Implement and test the root `Makefile` stages `boost-fetch`, `boost-bootstrap`, `boost-build`, `bootstrap`, and `self-host`. Boost must be compiled from source, not merely downloaded as headers. The default is latest stable Boost, with exact tag/commit pinned for releases. Add negative tests for missing tools, bad commit, interrupted downloads, incompatible compiler ABI and absent bootstrap source. See `docs/BOOTSTRAP.md` and `docs/BOOST-POLICY.md`.
