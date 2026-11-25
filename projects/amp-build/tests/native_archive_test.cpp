#include <cassert>
#include <expected>
#include <span>
#include <string>
#include <vector>

import amp.build.native_binary;
import amp.build.actions;
import amp.build.modules;
import amp.core.diagnostic;
import amp.core.filesystem;
import amp.core.process;

namespace {
/// Injected process-runner fake that records requests and can fail one call.
class Runner final : public amp::core::IProcessRunner {
public:
  /// mutable std::vector<amp::core::ProcessSpec> — Captured direct-process
  /// requests in execution order; modified by run().
  mutable std::vector<amp::core::ProcessSpec> calls;
  /// int — One-based invocation index to fail, or -1 for no injected failure.
  int fail_at = -1;

  /// Records a process and returns injected success or failure.
  /// @param[in] spec const amp::core::ProcessSpec& — Direct process request.
  /// @return amp::core::Result<amp::core::ProcessResult> — Zero-exit result,
  ///         or configured nonzero-exit result.
  /// @throws std::bad_alloc If recording the request cannot allocate.
  /// @throws std::length_error If the recording exceeds its size limit.
  amp::core::Result<amp::core::ProcessResult>
  run(const amp::core::ProcessSpec &spec) const override {
    calls.push_back(spec);
    return static_cast<int>(calls.size()) == fail_at
               ? amp::core::ProcessResult{1, {}, "injected failure", false}
               : amp::core::ProcessResult{0, {}, {}, false};
  }
};

/// Injected filesystem fake; rejects report reads because this C-only case
/// must not request module scan reports.
class Files final : public amp::core::IFileSystem {
public:
  /// mutable int — Number of atomic writes requested by the executor.
  mutable int writes = 0;
  /// Rejects an unexpected report read.
  /// @param[in] path const std::string& — Unexpected report path.
  /// @return amp::core::Result<std::vector<std::byte>> — Internal-error
  ///         diagnostic containing the unexpected path.
  /// @throws std::bad_alloc If constructing the diagnostic cannot allocate.
  /// @throws std::length_error If the diagnostic exceeds its size limit.
  amp::core::Result<std::vector<std::byte>>
  read(const std::string &path) const override {
    amp::core::Diagnostic d;
    d.code = amp::core::ErrorCode::internal;
    d.message = path;
    return std::unexpected(std::move(d));
  }
  /// Supplies the unused write operation for the fake.
  /// @param[in] path const std::string& — Unused output path.
  /// @param[in] bytes std::span<const std::byte> — Unused output bytes.
  /// @return amp::core::Status — Success; this test never calls the method.
  /// @note Does not throw.
  amp::core::Status
  write_atomic(const std::string &path,
               std::span<const std::byte> bytes) const override {
    (void)path;
    (void)bytes;
    ++writes;
    return {};
  }
  /// Supplies the unused enumeration operation for the fake.
  /// @param[in] path const std::string& — Unused directory path.
  /// @return amp::core::Result<std::vector<std::string>> — Empty listing;
  ///         this test never calls the method.
  /// @note Does not throw.
  amp::core::Result<std::vector<std::string>>
  enumerate(const std::string &path) const override {
    (void)path;
    return std::vector<std::string>{};
  }
  /// Supplies the unused canonicalization operation for the fake.
  /// @param[in] path const std::string& — Unused path.
  /// @return amp::core::Result<std::string> — Empty path; this test never
  ///         calls the method.
  /// @note Does not throw.
  amp::core::Result<std::string>
  canonicalize(const std::string &path) const override {
    (void)path;
    return std::string{};
  }
};
} // namespace

/// Exercises archive planning, terminal argv, output checks and failure stop.
/// @return int — Zero when all assertions pass; aborts on an assertion
/// failure.
/// @throws std::bad_alloc If fixture setup or recording cannot allocate.
/// @throws std::length_error If a fixture or recording exceeds its size limit.
int main() {
  /// Injected metadata verifier that accepts every requested output.
  /// @param[in] path const std::string& — Path accepted by the verifier.
  /// @return amp::core::Status — Success for every requested metadata check.
  /// @note Does not throw.
  const auto verify = [](const std::string &path) -> amp::core::Status {
    (void)path;
    return {};
  };
  Files files;
  amp::build::NativeArchiveRequest request{"/tmp/amp-archive-test",
                                           "/usr/bin/ar",
                                           "/tmp/amp-archive-test/libsample.a",
                                           {{"/usr/bin/gcc",
                                             "/tmp/amp-archive-test",
                                             "/tmp/source.c",
                                             "/tmp/amp-archive-test/source.o",
                                             "c17",
                                             {},
                                             false}}};
  {
    Runner runner;
    auto built = amp::build::run_native_archive(request, runner, files, verify);
    assert(built && built->archive == request.archive);
    assert(built->compile_actions.size() == 1);
    assert(runner.calls.size() == 2);
    assert(runner.calls.front().argv ==
           built->compile_actions.front().arguments);
    assert((runner.calls.back().argv ==
            std::vector<std::string>{"/usr/bin/ar", "rcs", request.archive,
                                     "/tmp/amp-archive-test/source.o"}));
  }
  {
    Runner runner;
    Files no_mapper_files;
    auto c_only = request;
    c_only.workspace_modules = amp::build::ModuleMap{};
    const auto expected = amp::build::plan_compile(c_only.compiles.front());
    assert(expected);
    auto built =
        amp::build::run_native_archive(c_only, runner, no_mapper_files, verify);
    assert(built && built->provided_modules.empty());
    assert(no_mapper_files.writes == 0);
    assert(runner.calls.size() == 2);
    assert(runner.calls.front().argv == expected->arguments);
    assert(built->compile_actions.front().arguments == expected->arguments);
    assert(built->compile_actions.front().inputs.size() == 1);
    assert(built->compile_actions.front().outputs.size() == 1);
  }
  {
    Runner runner;
    Files no_mapper_files;
    auto c_only = request;
    c_only.workspace_modules =
        amp::build::ModuleMap{{"bad name", "relative.gcm"}};
    const auto expected = amp::build::plan_compile(c_only.compiles.front());
    assert(expected);
    auto built =
        amp::build::run_native_archive(c_only, runner, no_mapper_files, verify);
    assert(built && no_mapper_files.writes == 0);
    assert(runner.calls.size() == 2);
    assert(expected && built &&
           built->compile_actions.front().arguments == expected->arguments);
  }
  {
    Runner runner;
    runner.fail_at = 2;
    auto failed =
        amp::build::run_native_archive(request, runner, files, verify);
    assert(!failed && runner.calls.size() == 2);
    assert(failed.error().message.find("injected failure") !=
           std::string::npos);
  }
  {
    Runner runner;
    auto invalid = request;
    invalid.archive = "/tmp/amp-archive-test/source.o";
    assert(!amp::build::run_native_archive(invalid, runner, files, verify));
    assert(runner.calls.empty());
  }
  {
    Runner runner;
    auto invalid = request;
    invalid.archiver = "ar";
    assert(!amp::build::run_native_archive(invalid, runner, files, verify));
    assert(runner.calls.empty());
  }
  {
    Runner runner;
    auto invalid = request;
    invalid.compiles.front().source = "/tmp/source.cpp";
    invalid.compiles.front().output = "/tmp/amp-archive-test/provider.o";
    invalid.compiles.front().cxx_modules = true;
    invalid.workspace_modules =
        amp::build::ModuleMap{{"external", invalid.archive}};
    assert(!amp::build::run_native_archive(invalid, runner, files, verify));
    assert(runner.calls.empty());
    assert(files.writes == 0);
  }
  {
    Runner runner;
    auto invalid = request;
    invalid.compiles.front().source = "/tmp/source.cpp";
    invalid.compiles.front().output = "/tmp/amp-archive-test/provider.o";
    invalid.compiles.front().cxx_modules = true;
    invalid.workspace_modules =
        amp::build::ModuleMap{{"bad name", "/tmp/external.gcm"}};
    assert(!amp::build::run_native_archive(invalid, runner, files, verify));
    assert(runner.calls.empty());
    assert(files.writes == 0);
  }
  {
    Runner runner;
    auto invalid = request;
    invalid.compiles.front().source = "/tmp/source.cpp";
    invalid.compiles.front().output = "/tmp/amp-archive-test/.amp-module-map";
    invalid.compiles.front().cxx_modules = true;
    invalid.workspace_modules = amp::build::ModuleMap{};
    assert(!amp::build::run_native_archive(invalid, runner, files, verify));
    assert(runner.calls.empty());
    assert(files.writes == 0);
  }
}
