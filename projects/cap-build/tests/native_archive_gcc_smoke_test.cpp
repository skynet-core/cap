#include <cassert>
#include <chrono>
#include <expected>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

import cap.build.actions;
import cap.build.native_binary;
import cap.core.diagnostic;
import cap.core.filesystem;
import cap.core.process;

namespace {
/// Direct-runner test adapter recording each invocation before delegation.
class RecordingRunner final : public cap::core::IProcessRunner {
public:
  /// mutable std::vector<cap::core::ProcessSpec> — Engine and consumer
  /// process requests, recorded in invocation order.
  mutable std::vector<cap::core::ProcessSpec> calls;
  /// cap::core::LocalProcessRunner — Executes the explicit tools directly.
  cap::core::LocalProcessRunner local;

  /// Records and executes one direct process request.
  /// @param[in] spec const cap::core::ProcessSpec& — Direct process argv,
  ///            cwd, environment allowlist and timeout.
  /// @return cap::core::Result<cap::core::ProcessResult> — Local process
  ///         result or runner diagnostic.
  /// @throws std::bad_alloc If recording the process cannot allocate.
  /// @throws std::length_error If the recording exceeds its size limit.
  /// @note Exceptions from the local runner propagate unchanged.
  cap::core::Result<cap::core::ProcessResult>
  run(const cap::core::ProcessSpec &spec) const override {
    calls.push_back(spec);
    return local.run(spec);
  }
};
} // namespace

/// Builds a mixed C/module archive, then separately links and runs its
/// consumer.
/// @param[in] argc int — Argument count; must be six including the program
///            name (asserted, then checked for non-asserting builds).
/// @param[in] argv char** — Six NUL-terminated argument strings: program
///            name, trusted C compiler, trusted C++ compiler, trusted
///            archiver, exclusive new private cwd, and fixture root.
/// @return int — Zero when archive and independent consumer pass; 2 for
///         argument/directory setup failure, 3 for archive failure, and 4–6
///         for consumer compile/link/run failure.
/// @throws std::bad_alloc If test state cannot be allocated.
/// @throws std::length_error If test state exceeds a size limit.
/// @throws std::filesystem::filesystem_error If filesystem operations throw.
/// @note Exceptions from the file/process adapters propagate unchanged.
int main(int argc, char **argv) {
  assert(argc == 6);
  if (argc != 6)
    return 2;
  const std::string cc = argv[1], cxx = argv[2], ar = argv[3],
                    directory = argv[4], fixture = argv[5],
                    source_dir = fixture + "/src space";
  std::error_code create_error;
  const bool created =
      std::filesystem::create_directory(directory, create_error);
  assert(created && !create_error);
  if (!created || create_error)
    return 2;
  cap::core::LocalFileSystem filesystem;
  RecordingRunner runner;
  /// Checks generated artifacts without opening or reading their contents.
  /// @param[in] path const std::string& — Output path to inspect without
  ///            reading its contents.
  /// @return cap::core::Status — Success for a regular non-symlink file;
  ///         otherwise missing_resource diagnostic.
  /// @throws std::bad_alloc If path or diagnostic construction cannot allocate.
  /// @throws std::length_error If the diagnostic exceeds its limit.
  /// @throws std::filesystem::filesystem_error If path construction fails.
  const auto check_output = [](const std::string &path) -> cap::core::Status {
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(path, ec);
    if (ec || !std::filesystem::is_regular_file(status) ||
        std::filesystem::is_symlink(status)) {
      cap::core::Diagnostic d;
      d.code = cap::core::ErrorCode::missing_resource;
      d.message = path + ": expected regular non-symlink output";
      return std::unexpected(std::move(d));
    }
    return {};
  };
  cap::build::NativeArchiveRequest request{directory,
                                           ar,
                                           directory + "/libstandalone.a",
                                           {{cc,
                                             directory,
                                             source_dir + "/c_value.c",
                                             directory + "/c_value.o",
                                             "c17",
                                             {},
                                             false},
                                            {cxx,
                                             directory,
                                             source_dir + "/value.cppm",
                                             directory + "/value.o",
                                             "c++26",
                                             {},
                                             true}}};
  auto archived =
      cap::build::run_native_archive(request, runner, filesystem, check_output);
  if (!archived)
    std::cerr << archived.error().message << '\n';
  assert(archived && archived->compile_actions.size() == 2);
  if (!archived)
    return 3;
  assert(archived->compile_actions.front().outputs.front() ==
         directory + "/value.o");
  assert(archived->compile_actions.back().outputs.front() ==
         directory + "/c_value.o");
  assert(runner.calls.size() == 4); // scan, two compiles, archive
  assert(runner.calls.back().argv ==
         (std::vector<std::string>{ar, "rcs", request.archive,
                                   directory + "/c_value.o",
                                   directory + "/value.o"}));

  const cap::build::CompileRequest consumer{
      cxx, directory, source_dir + "/main.cpp", directory + "/main.o", "c++26",
      {},  true};
  auto compile = cap::build::plan_compile(consumer);
  assert(compile);
  cap::core::ProcessSpec compile_spec{
      compile->arguments, directory, {"PATH"}, {}, std::chrono::minutes(2)};
  auto compiled = runner.run(compile_spec);
  assert(compiled && compiled->exit_code == 0);
  if (!compiled || compiled->exit_code != 0)
    return 4;
  const auto executable = directory + "/consumer";
  cap::core::ProcessSpec link_spec{
      {cxx, "-o", executable, directory + "/main.o", request.archive},
      directory,
      {"PATH"},
      {},
      std::chrono::minutes(2)};
  auto linked = runner.run(link_spec);
  assert(linked && linked->exit_code == 0);
  if (!linked || linked->exit_code != 0)
    return 5;
  cap::core::ProcessSpec execute_spec{
      {executable}, directory, {"PATH"}, {}, std::chrono::seconds(30)};
  auto executed = runner.run(execute_spec);
  assert(executed && executed->exit_code == 0 &&
         executed->stdout_text == "standalone: 42\n");
  if (!executed || executed->exit_code != 0 ||
      executed->stdout_text != "standalone: 42\n")
    return 6;
  std::cout << executed->stdout_text;
}
