# ADR-0001 Native-only builds and external executable plugins
Status: accepted.
Decision: amp never delegates native build actions to CMake, Meson, Make, Ninja, Bazel or other build systems. Third-party libraries can use native recipes or verified artifacts prepared by independently installed import plugins. See [ADR-0010](0010-manifest-library-import-plugins.md) for the planned manifest and execution boundary; importers are not core build backends.
