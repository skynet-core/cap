# M0 milestone review bundle

Prepared under the v1.5 milestone review policy: milestone criteria,
candidate SHA, scoped diff and sanitized CI evidence. No credentials, no
repository tooling, no unrelated files. Reviewer must return a structured
`approve` or `reject` **for this exact SHA**.

## Candidate

- SHA: `8dfe9015ea477b638ede40c87184322009887fbc` (branch `main`, local-only repo)
- Scoped diff: `git diff cb9ae10..8dfe9015ea477b638ede40c87184322009887fbc`
  (baseline spec commit `cb9ae10` to candidate; 25 files, +1976/−19)
- Commits in range:
  - `f04f1e7` feat(amp-core): implement M0 diagnostics contract (T01)
  - `64c4a15` build: ignore .bootstrap outputs and untrack committed test binary
  - `418ec60` build: pin Boost.Build toolset to the same g++-16 as stage-0
  - `191722b` feat(amp-core): implement M0/T02 filesystem and process contracts
  - `1eb1542` test(amp-core): add sanitize and coverage jobs with fault-injection tests
  - `8dfe901` feat(amp-core): complete M0 primitives and stage-0/1 bootstrap

## Milestone criteria (docs/RELEASE-PLAN-v1.4.md, M0)

1. Explicit verified Boost version+commit — **met**. `make boost-fetch`
   requires `BOOST_VERSION=1.92.0` + full 40-hex `BOOST_COMMIT`, verifies the
   upstream tag, records 172 submodule SHAs
   (`.bootstrap/boost-submodules.txt`) and the install digests
   (`.bootstrap/boost-install-digest.txt`).
2. Preseed/offline validation — **met for the pinned checkout**. Reuse path
   re-verifies revision/tag/submodules before building; fetch requires
   network, which is documented in `docs/BOOTSTRAP.md`.
3. C++26 feature probes — **partial, documented**. `make prerequisites`
   probes compiler acceptance of `-std=c++26`; the separate reflection
   feature probe is explicitly pending (`Makefile:67` NOTE,
   `docs/BOOTSTRAP.md:7` "once written") and no amp source uses reflection.
4. Stage-1 executable — **met**. Stage-0 driver (`bootstrap/main.cpp`)
   compiles stage-1 (`bootstrap/stage1.cpp`) with the pinned g++-16 against
   the installed Boost, without any external build system; stage-1 self-checks
   Boost provenance (`boost-version.txt`) and the amp-core SHA-256
   empty-input vector, then emits deterministic status JSON.
5. Stage comparison design — **met as design**. `make self-host` stub
   documents stage-2 as a later milestone; no stage-2 claims made.

Additional amp-core M0 scope (projects/amp-core/TASKS.md): T01 diagnostics
contract + golden fixture, T02 filesystem/process adapters, T06 coverage
job + waiver, T07 sanitizer jobs — done; T03 test coverage in progress
(digest block-boundary vectors and primitive behavioral tests added);
T05 cancellation/integrity wiring and T08 benchmarks explicitly deferred.

## CI evidence (all runs 2026-11-20, host toolchain g++-16 / clang++ 23.1.5)

### make core-test (5 suites)

```
amp-core diagnostics: all checks passed
amp-core filesystem: all checks passed
amp-core process: all checks passed
amp-core digest: all checks passed
amp-core primitives: all checks passed
```

### make sanitize

```
sanitize: ASan+UBSan+LeakSanitizer (all suites) and TSan (non-process suites) passed.
sanitize: MSan rejected (no fully instrumented dependencies); TSan+process_test rejected (uninstrumented children). Not run, not claimed.
```

(UBSan initially caught a null-pointer `memcpy` on empty-input digests in
`digest.hpp`; fixed with a size guard, re-run green.)

### make core-coverage

```
lines.......: 84.5% (321 of 380 lines)
functions...: 100.0% (20 of 20 functions)
branches....: 71.7% (137 of 191 branches)
coverage gate: lines=84.5% branches=71.7% (baseline >=79% / >=66%)
```

Per header: digest 100% lines (>=95% critical-algorithm threshold met),
stop_token 100%, clock 100%, logger 100%, diagnostic 95.5%,
filesystem 78.6%, process 75.0% (waived paths documented in
`projects/amp-core/COVERAGE-WAIVER.md`). Baseline gate intentionally
unchanged until T05 fault injection.

### make bootstrap (BOOST_VERSION=1.92.0 BOOST_COMMIT=afdfa32505af73e3d208144b3f623f0096cb62b6)

```
bootstrap: stage-1 executable ready at <repo>/.bootstrap/bin/amp-stage1
{"stage":1,"amp_core":"ok","boost":"1_92","self_check":"passed"}
```

Stage-1 was additionally executed manually in a clean environment
(`env -i PATH=...`) to prove the binary needs no ambient state.

## Known limitations (not claimed as done)

- Reflection C++26 feature probe not yet implemented (no amp source needs it).
- StopToken not yet wired into `IProcessRunner` (T05); process-tree
  termination and isolation adapters unimplemented (v1.4 addendum).
- Benchmarks/profile baselines for hot paths not recorded (T08);
  digest has a documented shortcut note instead (fixed-size padding buffer
  is adequate at stage-0).
- Coverage raw policy gate (85/75) deferred to T05; no-regression baseline
  in force (79/66).
- MSan and TSan+process_test rejected as unsupported and documented.
- Stage-2 self-hosting not implemented (design stub only).
