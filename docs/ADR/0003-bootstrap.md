# ADR-0003 Direct compiler bootstrap
Status: accepted.
Decision: minimal stage-0 source compiled directly by a host compiler or supplied as a verified prebuilt; stage-0 downloads/verifies pinned managed C++26 toolchain and directly compiles stage-1; stage-1 self-hosts. No external build system. Consequences: documented host prerequisite and bootstrap dependency snapshots.

## Stage comparison design

Compare stage 1 and stage 2 by running the same native build fixture with both executables. Do not compare the two AMP executables byte for byte: the bootstrap and self-hosted builds can have different inputs.

Use one pinned compiler, dependency set, target, profile and fixture revision for both runs. Give each run a clean build directory and an empty cache. The fixture builds a named C++ module and an executable that imports it, then runs the executable. Require successful build and execution in both stages, identical fixture stdout, and matching sets of compile actions with the same ordered argument vectors. Normalize only the separately assigned source and output roots in path-valued arguments and compilation database fields. Keep the compiler identity, flags, module relationships and relative paths in the comparison.

Record both stage executable digests, the input identities, both unmodified compilation databases, commands, statuses and stdout. Missing evidence, a failed build or run, or a comparison mismatch fails the stage check. Add fixtures that deliberately change a compile flag and a runtime result and show that each difference is rejected. Stage comparison is a planned self-host check; it is not implemented by stage 1's bootstrap status response.
