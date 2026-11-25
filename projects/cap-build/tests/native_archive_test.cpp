#include <cassert>
#include <expected>
#include <span>
#include <string>
#include <vector>

import cap.build.native_binary;
import cap.build.actions;
import cap.build.modules;
import cap.core.diagnostic;
import cap.core.filesystem;
import cap.core.process;

namespace {
/// Injected process-runner fake that records requests and can fail one call.
class Runner final : public cap::core::IProcessRunner {
public:
  /// mutable std::vector<cap::core::ProcessSpec> — Captured direct-process
  /// requests in execution order; modified by run().
  mutable std::vector<cap::core::ProcessSpec> calls;
  /// int — One-based invocation index to fail, or -1 for no injected failure.
  int fail_at = -1;

  /// Records a process and returns injected success or failure.
  /// @param[in] spec const cap::core::ProcessSpec& — Direct process request.
  /// @return cap::core::Result<cap::core::ProcessResult> — Zero-exit result,
  ///         or configured nonzero-exit result.
  /// @throws std::bad_alloc If recording the request cannot allocate.
  /// @throws std::length_error If the recording exceeds its size limit.
  cap::core::Result<cap::core::ProcessResult>
  run(const cap::core::ProcessSpec &spec) const override {
    calls.push_back(spec);
    return static_cast<int>(calls.size()) == fail_at
               ? cap::core::ProcessResult{1, {}, "injected failure", false}
               : cap::core::ProcessResult{0, {}, {}, false};
  }
};

/// Injected filesystem fake; rejects report reads because this C-only case
/// must not request module scan reports.
class Files final : public cap::core::IFileSystem {
public:
  /// mutable int — Number of atomic writes requested by the executor.
  mutable int writes = 0;
  /// Rejects an unexpected report read.
  /// @param[in] path const std::string& — Unexpected report path.
  /// @return cap::core::Result<std::vector<std::byte>> — Internal-error
  ///         diagnostic containing the unexpected path.
  /// @throws std::bad_alloc If constructing the diagnostic cannot allocate.
  /// @throws std::length_error If the diagnostic exceeds its size limit.
  cap::core::Result<std::vector<std::byte>>
  read(const std::string &path) const override {
    cap::core::Diagnostic d;
    d.code = cap::core::ErrorCode::internal;
    d.message = path;
    return std::unexpected(std::move(d));
  }
  /// Supplies the unused write operation for the fake.
  /// @param[in] path const std::string& — Unused output path.
  /// @param[in] bytes std::span<const std::byte> — Unused output bytes.
  /// @return cap::core::Status — Success; this test never calls the method.
  /// @note Does not throw.
  cap::core::Status
  write_atomic(const std::string &path,
               std::span<const std::byte> bytes) const override {
    (void)path;
    (void)bytes;
    ++writes;
    return {};
  }
  /// Supplies the unused enumeration operation for the fake.
  /// @param[in] path const std::string& — Unused directory path.
  /// @return cap::core::Result<std::vector<std::string>> — Empty listing;
  ///         this test never calls the method.
  /// @note Does not throw.
  cap::core::Result<std::vector<std::string>>
  enumerate(const std::string &path) const override {
    (void)path;
    return std::vector<std::string>{};
  }
  /// Supplies the unused canonicalization operation for the fake.
  /// @param[in] path const std::string& — Unused path.
  /// @return cap::core::Result<std::string> — Empty path; this test never
  ///         calls the method.
  /// @note Does not throw.
  cap::core::Result<std::string>
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
  /// @return cap::core::Status — Success for every requested metadata check.
  /// @note Does not throw.
  const auto verify = [](const std::string &path) -> cap::core::Status {
    (void)path;
    return {};
  };
  Files files;
  cap::build::NativeArchiveRequest request{"/tmp/cap-archive-test",
                                           "/usr/bin/ar",
                                           "/tmp/cap-archive-test/libsample.a",
                                           {{"/usr/bin/gcc",
                                             "/tmp/cap-archive-test",
                                             "/tmp/source.c",
                                             "/tmp/cap-archive-test/source.o",
                                             "c17",
                                             {},
                                             false}}};
  {
    Runner runner;
    auto built = cap::build::run_native_archive(request, runner, files, verify);
    assert(built && built->archive == request.archive);
    assert(built->compile_actions.size() == 1);
    assert(runner.calls.size() == 2);
    assert(runner.calls.front().argv ==
           built->compile_actions.front().arguments);
    assert((runner.calls.back().argv ==
            std::vector<std::string>{"/usr/bin/ar", "rcs", request.archive,
                                     "/tmp/cap-archive-test/source.o"}));
  }
  {
    Runner runner;
    Files no_mapper_files;
    auto c_only = request;
    c_only.workspace_modules = cap::build::ModuleMap{};
    const auto expected = cap::build::plan_compile(c_only.compiles.front());
    assert(expected);
    auto built =
        cap::build::run_native_archive(c_only, runner, no_mapper_files, verify);
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
        cap::build::ModuleMap{{"bad name", "relative.gcm"}};
    const auto expected = cap::build::plan_compile(c_only.compiles.front());
    assert(expected);
    auto built =
        cap::build::run_native_archive(c_only, runner, no_mapper_files, verify);
    assert(built && no_mapper_files.writes == 0);
    assert(runner.calls.size() == 2);
    assert(expected && built &&
           built->compile_actions.front().arguments == expected->arguments);
  }
  {
    Runner runner;
    runner.fail_at = 2;
    auto failed =
        cap::build::run_native_archive(request, runner, files, verify);
    assert(!failed && runner.calls.size() == 2);
    assert(failed.error().message.find("injected failure") !=
           std::string::npos);
  }
  {
    Runner runner;
    auto invalid = request;
    invalid.archive = "/tmp/cap-archive-test/source.o";
    assert(!cap::build::run_native_archive(invalid, runner, files, verify));
    assert(runner.calls.empty());
  }
  {
    Runner runner;
    auto invalid = request;
    invalid.archiver = "ar";
    assert(!cap::build::run_native_archive(invalid, runner, files, verify));
    assert(runner.calls.empty());
  }
  {
    Runner runner;
    auto invalid = request;
    invalid.compiles.front().source = "/tmp/source.cpp";
    invalid.compiles.front().output = "/tmp/cap-archive-test/provider.o";
    invalid.compiles.front().cxx_modules = true;
    invalid.workspace_modules =
        cap::build::ModuleMap{{"external", invalid.archive}};
    assert(!cap::build::run_native_archive(invalid, runner, files, verify));
    assert(runner.calls.empty());
    assert(files.writes == 0);
  }
  {
    Runner runner;
    auto invalid = request;
    invalid.compiles.front().source = "/tmp/source.cpp";
    invalid.compiles.front().output = "/tmp/cap-archive-test/provider.o";
    invalid.compiles.front().cxx_modules = true;
    invalid.workspace_modules =
        cap::build::ModuleMap{{"bad name", "/tmp/external.gcm"}};
    assert(!cap::build::run_native_archive(invalid, runner, files, verify));
    assert(runner.calls.empty());
    assert(files.writes == 0);
  }
  {
    Runner runner;
    auto invalid = request;
    invalid.compiles.front().source = "/tmp/source.cpp";
    invalid.compiles.front().output = "/tmp/cap-archive-test/.cap-module-map";
    invalid.compiles.front().cxx_modules = true;
    invalid.workspace_modules = cap::build::ModuleMap{};
    assert(!cap::build::run_native_archive(invalid, runner, files, verify));
    assert(runner.calls.empty());
    assert(files.writes == 0);
  }
}
