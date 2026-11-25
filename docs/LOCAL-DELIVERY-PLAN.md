# Local AMP delivery plan

## Scope and gates

Deliver a working native `amp` for binary, multi-binary, library and
multi-project workspace builds, then complete the non-CI tasks in the
[integrated roadmap](ROADMAP.md) in [M0–M6 order](RELEASE-PLAN-v1.4.md), followed
by editor integration. Keep worktrees, builds, tests and evidence local. Do not
configure Git remotes, CI or remote runners. Where future distribution features
need a service, use local fixtures and do not publish or deploy to production.
AMP-owned C++ interfaces are C++20 named modules built in C++26 mode.

Verify every milestone locally on its exact SHA under [the quality
policy](QUALITY.md), including the independent review and final audit gates.
Record passed, failed, unsupported and unrun checks separately. Do not count a
stub or a test run on another commit as completion.

## First usable build: acceptance

Use temporary build roots and an explicit pinned host compiler/toolchain,
without a network dependency or delegation to another build system.

| Fixture | Observable result |
| --- | --- |
| A package with one `[[bin]]` | `amp build` produces an executable that runs and prints the expected text. |
| A package with two `[[bin]]` entries | Both outputs build in one invocation and run independently. Selecting one builds only that target and its prerequisites. |
| A `[lib]` package | A static archive is published, and a separate consumer links and runs against it. |
| A virtual `[workspace]` with an app and a library member | Building the app resolves and builds its local library dependency first. Building the workspace produces the expected member outputs; package selection and default members are tested. |
| C and C++ sources, including a named module and a separate importer | The native plan supplies the correct compiler and flags; the module provider is built before the importer, which links and runs. |
| Declared system library on a target and on a static library | The target SDK/toolchain resolves it; link order is preserved and the static requirement reaches its consumer. Invalid or unresolved libraries fail. |
| Exact compilation database | Compare `arguments`, working directory and outputs with the executed compile actions, including paths with spaces and the module fixture. Run the local clangd smoke where supported. |

Negative fixtures cover bad manifests, duplicate targets or workspace members,
missing sources or dependencies, dependency cycles, escaping paths and failed
compiler/linker actions. Fail before reporting an artifact or build as
successful. Unsupported manifest/build options must fail explicitly.

## Implementation order

1. Finish M0 bootstrap evidence and independent local review.
2. Freeze the local manifest, workspace, toolchain, native plan, CLI and compdb
   contracts before implementation. Land the parser and its positive/negative
   fixtures first; connect workspace discovery and local dependency selection
   to the planner next.
3. Implement a serial native compile/archive/link plan and exact compdb export.
   Validate and run the first usable build fixtures above. Do not claim
   incremental builds, parallel scheduling, or isolation from this slice.
4. Complete the remaining milestone and editor requirements against their own
   local acceptance fixtures. The initial static-library slice does not
   substitute for the planned shared/header-only library modes, dependency
   sources, installation, or other distribution features.
