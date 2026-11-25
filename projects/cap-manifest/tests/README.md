# M1-B01 local reader checks

## M1-B02 source selection

Compile `cap.core.diagnostic`, `cap.core.filesystem`, `cap.manifest`, then
`cap.manifest.sources`, and link those objects with `sources_test.cpp` in an
exclusive module-cache directory. Pass an existing empty writable scratch
directory and the read-only `projects/cap-compdb/fixtures/workspace` directory
as the two test arguments. The test creates files and symlinks in scratch.

GCC 16 in C++26 mode with `-fmodules -Wall -Wextra -Werror` passed locally.
Clang 23 in C++2c mode with ASan+UBSan and source-based coverage also passed;
`sources.cppm` measured 204/212 lines (96.23%) and 167/194 branches (86.08%).
Coverage includes injected filesystem failure and alias rejection tests. These
results do not establish performance, race resistance, other platforms, or
complete static analysis.

Tests use `*_test.cpp` translation units; see root `docs/QUALITY.md`.

Run from an exclusive scratch directory, with `R` set to the absolute worktree
path and `CXX=/home/linuxbrew/.linuxbrew/bin/g++-16`:

```sh
$CXX -std=c++26 -fmodules -x c++ -c "$R/projects/cap-core/diagnostic.cppm" -o diagnostic.o
$CXX -std=c++26 -fmodules -Wall -Wextra -x c++ -c "$R/projects/cap-manifest/manifest.cppm" -o manifest.o
$CXX -std=c++26 -fmodules -Wall -Wextra "$R/projects/cap-manifest/tests/manifest_test.cpp" diagnostic.o manifest.o -o manifest_test
./manifest_test "$R/projects/cap-compdb/fixtures/workspace"
```

The tests read the actual compdb fixture files, plus direct positive and
negative strings. Full TOML, semantic-version validation, path containment and
source discovery are outside this reader. Coverage, sanitizers, parser
performance and clice results are separate mandatory evidence; this file is
not a claim that those gates have passed.

Local Clang source-based instrumentation for this candidate reported 321/340
lines (94.41%) and 233/272 branches (85.66%) in `manifest.cppm`. The profile
data is in `.bootstrap/manifest-reader-terra/clang-coverage/` in the main
workspace; this measurement does not include fuzz or integration coverage.
