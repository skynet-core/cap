# Workspace tests

`workspace_test.cpp` exercises workspace loading and target selection.
`dependencies_test.cpp` exercises local static-library dependency resolution.
Both take a nonexistent scratch path as their first argument and the
absolute path to `projects/cap-compdb/fixtures/workspace` as their second.

With the pinned GCC 16.2.0 compiler, compile the diagnostic, filesystem,
manifest, and workspace named modules in dependency order using
`-std=c++26 -fmodules -Wall -Wextra -Werror`. For the dependency test, also
compile `dependencies.cppm` after `workspace.cppm`. Link each test separately
against its module objects, and run it from a disposable build directory.

The historical M1 component checks passed with GCC 16.2.0 and with Clang
23.1.6 ASan/UBSan. They measured 90.95% line / 82.91% branch coverage for
workspace loading and 156/156 lines / 74/76 branches for dependency
resolution. These are component measurements, not CI or exact-commit
evidence. Clice lint passed; the historical LSP check timed out.
