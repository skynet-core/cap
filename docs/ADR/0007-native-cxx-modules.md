# ADR-0007 — Named C++ modules for AMP

Status: accepted requirement; migration pending.

AMP-owned native C++ uses the named module facility introduced in C++20. The compiler language mode remains C++26. Export internal APIs from named module interfaces and consume them with `import`; don't introduce new header-based internal APIs. Executable entry points and `*_test.cpp` files can be ordinary translation units that import modules. Third-party and platform headers, and any documented external C ABI boundary, can remain headers.

Migrate the existing header-based code in dependency order, starting with `amp-core`. Keep the public behavior covered by existing fixtures and update module contracts before switching consumers. This requirement is for AMP's own source; packages built by AMP still choose their own standards and may use headers.

Before landing the migration, prove that the pinned toolchain can compile a named module and link a separate importer from a clean build. Verify that bootstrap and the native build order module interfaces before importers, that exact compile commands still work for clice and the compilation database, and that tests, coverage and supported sanitizers pass. Do not introduce an external build backend or claim module support from `-std=c++26` flag acceptance alone.
