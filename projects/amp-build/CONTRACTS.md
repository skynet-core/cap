# Contracts — amp-build

## Public boundary
Responsibility: Native build DAG and execution. Define narrow request/result interfaces in `include/amp/build/`. Interfaces are specifications, not compiled headers yet.

### M1 frozen action planner contract
`amp.build.actions` owns `Action{kind,directory,arguments,inputs,outputs}` and
pure `plan_compile`, `plan_archive`, and `plan_link` functions. Requests own their
strings/vectors. The planner is side-effect free: executable paths and all other
paths are trusted caller inputs after caller workspace validation, but this
boundary still rejects empty, NUL-containing, or non-absolute paths and argv
elements. No caller raw flags, shell, filesystem, process, compiler probing, or
implicit backend. Compile actions have exactly one primary source and output;
the compile database reads `directory`, primary `file`, `arguments`, and primary
`output` from that same action, never by regenerating arguments. Standards,
language inference, module switches, archive order, and link ordering are
specified in [API.md](API.md). Tests: `tests/actions_test.cpp`.
The Linux argv execution smoke is `tests/actions_smoke_test.cpp`; it consumes
only checked-in fixture sources and explicit tool paths, using the injected
`IProcessRunner` (no shell). It resolves `m` through the GCC driver used for the
planned C link, preserves the absolute SDK operand in argv and inputs, then runs
the consumer. Run it from the repository root after building
`amp.core.diagnostic`, `amp.core.process`, `amp.build.actions`, and
`amp.toolchain.host` named modules:

```sh
<actions-smoke-test> \
  /home/linuxbrew/.linuxbrew/bin/g++-16 \
  /home/linuxbrew/.linuxbrew/bin/gcc-16 \
  /usr/bin/ar \
  "$PWD/projects/amp-build/tests/fixtures/actions-smoke"
```

The tool arguments above are explicit trusted local paths, not values read from
a manifest or workspace. The smoke test submits each planned action's
`arguments` unchanged to `IProcessRunner`; only `PATH` is allowlisted for the
GCC driver's internal linker lookup.

### M1 GCC module scan
`amp.build.modules` accepts only canonical C++ module-mode compile actions.
Its projection keeps the original driver's cwd, preprocessing options and
source, replacing compile/output switches with GCC 16 `-M` P1689 reporting
switches and an absolute caller-owned report path. It has no filesystem or
process access and does not alter compile actions or compdb rows. The JSON
parser accepts version/revision 0/0, rejects duplicate decoded keys at every
JSON object depth, requires one matching primary output, and returns named
module provides/requires; header units and unknown schema shapes fail closed.
Ordering validates unit/object identities, resolves imports only among the
supplied units, rejects duplicate providers, missing providers and cycles, and
uses a lexical ready set for deterministic ties. Ordinary TUs may provide
nothing. Tests are in `tests/modules_test.cpp`; the GCC 16 provider/importer
and empty-map integration is `tests/module_mapper_gcc_smoke_test.cpp`. The
smoke exports the actual GCC compile actions through `amp.compdb::to_json`,
checks the same database with clangd's experimental module support and GCC
query driver, then links and runs the two objects. The native executor owns
scan execution and target-scoped dependency visibility.
Run it from the repository root after compiling the diagnostic, process,
actions, modules and compdb named modules:

```sh
<module-mapper-gcc-smoke-test> \
  /home/linuxbrew/.linuxbrew/bin/g++-16 \
  /home/linuxbrew/.linuxbrew/bin/clangd \
  "$PWD/projects/amp-compdb/fixtures/workspace" \
  "/absolute/fresh/scratch directory"
```

`ModuleMap` is the single caller-approved source of external named-module
bindings: logical name to absolute lexically canonical `.gcm` path. The map
does not establish that a BMI exists; the caller verifies provider outputs.
Its pure formatter validates every binding and emits GCC mapper rows in
logical-name order, preserving spaces and ending nonempty output with a
newline; NUL/control bytes, relative/lexically unnormalized paths and invalid
names fail with `invalid_input`.
The two-argument ordering overload treats valid external providers as already
built, without producing compile actions. A local/external provider name
collision and any import absent from both sets fail. The original one-argument
ordering behavior is unchanged. The native executor consumes the approved map
only when C++ actions are present; C-only targets do not validate or write an
unused map.

### M1 resolved link inputs
`LinkRequest.system_libraries` contains ordered absolute, NUL-free SDK operands
from `amp.toolchain.host::resolve_system_libraries`, not names or flags. The
planner copies each operand unchanged into argv and records it unchanged in
`Action.inputs`, after objects and archives and in the same order, including
duplicates. A path may be a linker script and is never inspected here.

`plan_link_inputs` consumes ordered direct archive IDs, caller-owned archive
facts (`id`, absolute output, ordered direct prerequisite IDs, ordered resolved
system operands), and the binary's ordered resolved SDK operands. It validates
all fact identities, output paths and SDK operands, but only traverses and
validates references in the reachable closure. Depth-first discovery follows
declared order; emission selects the earliest-discovered ready archive, where
ready means all reachable dependents have been emitted. Each archive is emitted
once before its prerequisites, even when a prerequisite is also a direct root.
Requirements concatenate as
binary first, then archive requirements in emitted archive order, without
deduplication. Reachable cycles and invalid references fail with
`invalid_input`; unrelated cycles do not affect the closure. This pure planner
does not execute, archive/link, discover tools or resolve libraries. Tests:
`tests/link_inputs_test.cpp`.
`tests/link_diamond_test.cpp` takes explicit trusted GCC, ar, fixture directory,
and absolute libz SDK operand arguments. It executes planned compile, archive
and link argv, checks argv and inputs, runs the consumer, and verifies that
both premature leaf placement and an omitted SDK operand fail the real link.

### M1 standalone native binary executor
`amp.build.native_binary` owns one binary only. The caller supplies an exclusive,
canonical private build directory, explicit trusted absolute compiler and C++
link-driver paths, ordered `CompileRequest`s, and direct-child outputs. Compile
cwd equals the private directory. The engine validates paths and collisions
(including `compile_commands.json` and `gcm.cache`) before any process starts.
It uses `plan_compile`, `plan_module_scan`, `parse_module_scan`,
`order_module_units`, and `plan_link`; it never rebuilds argv independently.
All scan and link plans are validated before any process. C++ units are scanned
before compilation, then compile actions run serially in module dependency
order. Link inputs stay in caller request order: module dependency order governs
compile scheduling, not link argument ordering. The filesystem seam reads only
scan reports; an injected metadata-only callback verifies regular non-symlink
reports and outputs. No produced executable is run. Each action has a
two-minute timeout. `ProcessSpec` allowlists the environment variable name
`PATH`; it does not set its value or restrict tools the compiler may discover.
The caller/runner must supply a trusted `PATH` suitable for compiler child
tools (for example, the local GCC linker lookup). No fixed value or child-tool
restriction is enforced by this engine. This is non-hermetic local execution,
not a sandbox. First failure stops the sequence and reports process stderr,
exit status, or timeout. Tests: `tests/native_binary_test.cpp`,
`tests/native_binary_gcc_smoke_test.cpp`, and
`tests/native_binary_link_gcc_smoke_test.cpp`.

`NativeBinaryRequest.link_inputs` is a default-empty, owned `LinkInputs`
value. Its ordered archive paths and SDK operands must be absolute paths;
`plan_link` validates them before any process starts. The executor passes both
vectors unchanged to the terminal action and rejects collisions with object,
executable, and generated scan-report paths before any process starts.
Repeated SDK operands remain repeated. Archive ordering, resolution, and
filesystem inspection remain caller responsibilities. The real-link smoke
prepares the existing C diamond archives with explicit `plan_compile` and
`plan_archive` actions, then invokes `run_native_binary` and verifies runtime
output plus actual failures for premature shared-archive order and a missing
SDK operand.

### M1 standalone native static archive executor
`amp.build.native_binary` also exports `NativeArchiveRequest`, containing the
private directory, explicit trusted absolute archiver, direct-child archive
output, and ordered compile requests; `NativeArchiveResult` contains that
verified archive path and the exact compile actions executed. `run_native_archive`
uses the same injected process runner, report filesystem and metadata-only
output checker as `run_native_binary`. It validates all compile and terminal
archive plans, paths and collisions before the first subprocess, scans and
orders module-enabled C++ units before compiling, then executes serially and
verifies every output. Its final argv comes exclusively from `plan_archive`.
The archive is never read or executed by the engine. Processes are bounded to
two minutes; first failure returns its diagnostic and stops subsequent work.
Tests: `tests/native_archive_test.cpp` and
`tests/native_archive_gcc_smoke_test.cpp`; the latter must link and run an
independent consumer against the produced archive.

### Opt-in cross-target module handoff
`NativeBinaryRequest::workspace_modules` and
`NativeArchiveRequest::workspace_modules` are appended optional `ModuleMap`
inputs. Disengaged preserves standalone argv and filesystem behavior exactly;
engaged empty explicitly enables workspace mapping. External BMI paths must be
regular files before any compiler process. Missing imports and local/external
provider conflicts fail closed.

Opt-in execution formats the external and local provider bindings with
`format_module_map`, atomically writes a private `.amp-module-map`, and adds
`-fmodule-mapper=<path>` to each executed compile action. Local BMIs use
deterministic direct-child `.amp-bmi-<ordinal>.gcm` paths, and mapper/BMI
metadata is recorded after each action's primary source/object entries.
Collisions are rejected before compilation. Every local BMI is verified as a
regular file before success; archive results expose only those verified local
bindings. This GCC-specific local handoff is not a sandbox or portable cache
contract.
The mapper is written only through the injected `IFileSystem::write_atomic`
and verified through the metadata callback. An engaged map on a C-only target
does not write a mapper or alter C compile argv.
If a C++ action reaches any external provider directly or through local
providers, its compile metadata includes every approved external BMI path
once, in deterministic order, in addition to the required local BMI paths.

The real cross-target engine smoke is
`tests/native_workspace_modules_gcc_smoke_test.cpp`. Its positional arguments
are absolute `CC CXX AR CLANGD WORKSPACE_FIXTURE FRESH_SCRATCH`; it builds
the provider archive and consumer in separate target directories, then writes
one exact `compile_commands.json` to `FRESH_SCRATCH/compile_commands.json`
using `amp.compdb::to_json` over the returned executed compile actions. The
same database is checked with clangd, experimental module support and an
explicit compiler query-driver.

### Scoped C++ Core Guidelines review
The handoff follows F.2/F.3 by keeping validation, mapper preparation and
per-action output verification in separately named private functions;
`run_native` coordinates the existing scan/compile/terminal sequence.
F.16 uses values for booleans and const references for owned collections and
adapters. F.17/F.20 avoid an output-reference parameter: the private executor
returns `NativeExecutionResult`. F.18/F.19 need no move-from or forwarding
parameter API. F.21 returns the related compile actions and provider map in
that result value. F.43 returns owned values and does not expose references to
local state. F.53 lambdas capture only for immediate synchronous standard
algorithm calls; none escape or outlive their local inputs.

## Required contract fields
- Request: typed input identity, configuration, target/host context as applicable, cancellation token, provenance.
- Result: typed value or structured diagnostic with code, location, causal chain, remediation.
- Determinism: stable ordering and canonical encoding for persisted/hashed output.
- Ownership: caller owns request; returned artifacts immutable or move-owned; no dangling filesystem or string views.
- IO: injected adapter for filesystem/process/network; no hidden global mutable state.
- Errors: invalid input, missing resource, unsupported capability, integrity failure, cancellation, transient IO.
- Compatibility: version all serialized protocols and files; migrations require ADR and golden fixtures.

## Module-specific invariant
Immutable planned graph; declared inputs/outputs; separate test actions; no external build-system invocation.

Resolve declared system libraries through the selected target toolchain or SDK. Preserve declared ordering, propagate static library requirements to their final link consumers, and record resolved inputs in link actions and fingerprints. For standalone installation, collect the target's transitive runtime shared-library graph as specified in [ADR-0008](../../docs/ADR/0008-system-linkage-and-standalone-install.md).

Run authorized `build.lua` declarations as isolated native generator actions. Publish only verified outputs and complete generators before scanning dependent compile units per [ADR-0009](../../docs/ADR/0009-lua-build-scripts.md).

## Contract tests
Incremental header edits, module BMI invalidation, linker order, parallel cancellation, cyclic graph, platform filtering.

## Acceptance
Every public API has documented pre/postconditions, unit and integration tests, benchmark for hot paths and explicit failure behavior before implementation is considered complete.

## Exhaustive planned type inventory
See [API.md](API.md). Every type there requires a public contract and verification before merge.

## v1.4 normative additions
Schedule with CPU/memory admission, resource classes, bounded fairness, cancellation of child process trees and no partial output publication (`../../docs/SCHEDULER.md`). Strict/audit/off isolation must be explicit; strict mode fails if unsupported, audit/off must not claim hermeticity (`../../docs/BUILD-ISOLATION.md`). Emit stable action lifecycle events and cache-miss explanations (`../../docs/BUILD-OBSERVABILITY.md`). Include tests for memory starvation, generator races, process cancellation and undeclared reads.
