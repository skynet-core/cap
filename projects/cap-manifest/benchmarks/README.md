# Local parser baseline

From an empty scratch directory, set `R` to the worktree's absolute path:

```sh
CXX=/home/linuxbrew/.linuxbrew/bin/g++-16
$CXX -std=c++26 -O2 -pg -fmodules -x c++ -c "$R/projects/cap-core/diagnostic.cppm" -o diagnostic.o
$CXX -std=c++26 -O2 -pg -fmodules -x c++ -c "$R/projects/cap-manifest/manifest.cppm" -o manifest.o
$CXX -std=c++26 -O2 -pg -fmodules "$R/projects/cap-manifest/benchmarks/parser_bench.cpp" diagnostic.o manifest.o -o parser_bench
./parser_bench "$R/projects/cap-compdb/fixtures/workspace/app/cap.toml"
gprof ./parser_bench gmon.out > gprof.txt
```

The input is read once before timing. One 10,000-parse warmup precedes five
10,000-parse samples; the program prints the median and range. On the local
Linux host with GCC 16.2, a profiling-instrumented run reported 3.279
microseconds per parse median, range 3.248–3.333, and 120,000 binary targets.
`gprof` recorded six 10-ms samples; the saved report is at
`.bootstrap/manifest-reader-terra/profile/gprof.txt` in the main workspace.
This instrumented baseline is not a regression comparison and is too short
for a reliable fine-grained profile. The default review threshold is a
sustained >10% median slowdown or memory increase.
