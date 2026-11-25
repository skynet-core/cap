# Standalone multiple-binary fixture

Build using the local `cap build` with explicit C, C++ and archive tool paths.
Run the reported artifacts separately: `first` prints `first: 1`, and `second`
prints `second: 42`. The second target needs its named-module provider and C
source in `src space/`. `--bin second` must build only those three sources.
