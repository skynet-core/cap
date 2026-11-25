# Contracts — cap-compdb

## Dependencies
Required: `cap-core` (paths, errors, atomic filesystem), `cap-build` (read-only planned compile action projection). Optional: `cap-workspace` for member metadata; `cap-cli` invokes this module, never reverse.

## M1 frozen exporter slice
The `cap.compdb` named module exports `cap::compdb::to_json` over read-only
`cap::build::Action` values. It emits only compile actions using exactly
`directory`, `inputs.front()`, ordered `arguments`, and `outputs.front()`.
Actions retain input order; argv is never reordered. Missing primary values,
empty argv, invalid argv[0], NULs, or invalid UTF-8 in serialized values return
`invalid_input`, not partial JSON. Empty argv elements after argv[0] are
preserved. Output is standard JSON with exactly
`directory`, `file`, `arguments`, and `output`. This slice performs no file IO,
activation, plan selection, or argument regeneration.

## Conceptual C++26 interfaces
```cpp
namespace cap::compdb {
struct ConfigurationId { std::string digest; };
struct Request {
    ConfigurationId configuration;
    std::filesystem::path workspace_root;
    bool include_tests = false;
    std::optional<std::string> package;
};
struct Entry {
    std::filesystem::path directory;
    std::filesystem::path file;
    std::vector<std::string> arguments;
    std::optional<std::filesystem::path> output;
};
struct ExportResult {
    std::filesystem::path database_path;
    std::size_t entry_count;
    std::vector<Diagnostic> warnings;
};
class CompileActionView {
public:
    virtual ~CompileActionView() = default;
    virtual auto compile_commands(const ConfigurationId&)
      const -> Result<std::vector<Entry>> = 0;
};
class Exporter {
public:
    virtual ~Exporter() = default;
    virtual auto generate(const Request&, const CompileActionView&)
      -> Result<ExportResult> = 0;
    virtual auto activate(const ConfigurationId&, const std::filesystem::path& workspace)
      -> Result<void> = 0;
};
}
```
Illustrative API; `Result` and `Diagnostic` are shared core types. Implementations must preserve action argument order, including response-file semantics (materialize or preserve valid response files).

## CLI contract
`cap compdb generate [--workspace] [--package NAME] [--target TRIPLE] [--profile NAME] [--features LIST] [--include-tests]`; `cap compdb activate --configuration ID`; `cap compdb list`; `cap compdb check`. `cap build/check/test` generate/update the matching configuration's database by default; only explicit activation updates the workspace-root active file. `cap compdb check` validates freshness, path existence where required, and schema, not compilation success.

## JSON example
```json
[{"directory":"/work/app","file":"/work/app/src/main.cpp","arguments":["/home/user/.cap/toolchains/llvm/bin/clang++","-std=c++26","-c","/work/app/src/main.cpp","-o","/work/app/.cap/build/main.o"],"output":"/work/app/.cap/build/main.o"}]
```

## M0 reference fixture

The local fixture is a proposed-v1 workspace containing a static named-module
library and an app with two executables. Its Linux, Windows, and `_test.cpp`
sources exercise selection; a source directory contains a space. The reference
JSON uses `@ROOT@` and `@BUILD@` placeholders and ordered `arguments`, not a
shell command. The local check substitutes absolute roots, rejects any changed
argument or order, and verifies the selected Linux test-enabled translation
units against the commands it actually executes. It compiles the module before
its importers, archives the library, and runs both executables and the test.
This is fixture evidence only: it neither implements an exporter nor verifies an
cap-build plan or freezes a cross-module interface. Windows is selection-only.
The reference run is pinned to the documented local GCC and archive drivers.

## Exhaustive planned type inventory
See [API.md](API.md). Every type there requires a public contract and verification before merge.
