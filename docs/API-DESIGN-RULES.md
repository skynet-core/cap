# API completeness and SOLID policy

This specification inventories **planned** types; it does not claim to list every future private implementation class. All new classes and interfaces must be added to the owning module's `API.md` before implementation, with: responsibility; public signatures and pre/postconditions; ownership/lifetime; thread-safety; cancellation; errors; versioning; dependency direction; tests; benchmark/profiling obligations.

## Source documentation

- Use Doxygen-compatible `///` comments immediately before the declaration
  in the `.cppm` interface or, for implementation-only symbols, where they
  are declared or defined. Document each class and struct (including private
  types), each public member, each named constant (including enum values),
  and every free function and method, public or private. Include explicitly
  declared constructors, destructors, operators, templates and test helpers.
  Document local lambdas at their definitions with the same callable rules.
- For each callable, explain its purpose and preconditions, document every
  parameter by name with its C++ type and input/output direction (`@param[in]`,
  `@param[out]`, or `@param[in,out]`), and document template parameters with
  `@tparam`. State the return type and the meaning of each outcome (`@return`);
  for `void`, constructors and destructors, state that there is no returned
  value and document effects or output parameters instead.
- List error conditions and their returned values or diagnostic codes.
  Use `@throws` for each exception type the implementation lets escape and
  describe its trigger. Say when a call has no failure result or does not
  throw if applicable. Do not describe a function as nonthrowing merely
  because it normally reports failures through `std::expected`.
- Document ownership, lifetime, units, side effects and state invariants
  where they matter. Prefer one declaration comment over copying a comment
  onto its out-of-line definition. The owning module's `API.md` still records
  module boundaries, contracts and verification; it does not replace
  comments that editor hover and reference generators can read.

New and changed declarations must comply before merge. Backfill existing
CAP-owned C++ as a separate work item; the full documentation requirement
is not complete until that pass is verified.

```cpp
/// Converts a nonnegative count to bytes, rejecting values above the limit.
/// @param[in] count int — Number of bytes requested.
/// @return std::expected<std::size_t, Error> — Count on success;
///         Error::out_of_range if count is negative or exceeds the limit.
/// @note Does not throw.
std::expected<std::size_t, Error> checked_size(int count) noexcept;
```

To generate a local contributor reference, use the actual Clang-compatible
compilation database and LLVM's optional `clang-doc` tool (not a build
dependency):

```sh
clang-doc -p "$BUILD_DIR" --doxygen --format=json --output="$DOC_DIR" \
  "$REPO/projects/cap-core/diagnostic.cppm"
```

Pass the desired translation units explicitly, using absolute paths. Leave
`--public` off to include private declarations that the tool supports.
Keep generated reference files outside the source tree. Use the same
compilation database with clangd to check hover text on a representative API
use, and inspect generator output rather than treating a zero exit as
complete documentation. Report any module or symbol the installed tools
cannot display; writing its source comment is still required.

LLVM builds that ship the Markdown templates can use `--format=md` to
generate a human-readable reference. JSON generation does not require
those templates. Private methods and member comments may be absent from
`clang-doc` output even without `--public`; inspect source or IDE hover for
anything the generator omits.

## SOLID gates
- SRP: one reason to change per type; avoid god services.
- OCP: extend via explicit adapters without modifying unrelated modules.
- LSP: contract tests run against every implementation of an interface.
- ISP: avoid interfaces combining unrelated filesystem/network/build operations.
- DIP: core policy depends on interfaces, not platform adapters.

## Class-level template
For each planned class: document construction/injection, state machine, method signatures and guarantees, invariant, error table, concurrency contract, test matrix, benchmark budget, and observable metrics.

## Gates
No merge without applicable API inventory and source documentation updates, passing tests, coverage and sanitizer evidence, and performance comparison for hot paths. Use C++26 for native cap code. The VS Code extension uses TypeScript as required by its host runtime.
