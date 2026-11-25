#include <cassert>
#include <chrono>
#include <expected>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

import amp.build.actions;
import amp.build.native_binary;
import amp.core.diagnostic;
import amp.core.filesystem;
import amp.core.process;

namespace {
/// Direct-runner test adapter recording each invocation before delegation.
class RecordingRunner final : public amp::core::IProcessRunner {
public:
  /// mutable std::vector<amp::core::ProcessSpec> — Engine and consumer
  /// process requests, recorded in invocation order.
  mutable std::vector<amp::core::ProcessSpec> calls;
  /// amp::core::LocalProcessRunner — Executes the explicit tools directly.
  amp::core::LocalProcessRunner local;

  /// Records and executes one direct process request.
  /// @param[in] spec const amp::core::ProcessSpec& — Direct process argv,
  ///            cwd, environment allowlist and timeout.
  /// @return amp::core::Result<amp::core::ProcessResult> — Local process
  ///         result or runner diagnostic.
  /// @throws std::bad_alloc If recording the process cannot allocate.
  /// @throws std::length_error If the recording exceeds its size limit.
  /// @note Exceptions from the local runner propagate unchanged.
  amp::core::Result<amp::core::ProcessResult>
  run(const amp::core::ProcessSpec &spec) const override {
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
  amp::core::LocalFileSystem filesystem;
  RecordingRunner runner;
  /// Checks generated artifacts without opening or reading their contents.
  /// @param[in] path const std::string& — Output path to inspect without
  ///            reading its contents.
  /// @return amp::core::Status — Success for a regular non-symlink file;
  ///         otherwise missing_resource diagnostic.
  /// @throws std::bad_alloc If path or diagnostic construction cannot allocate.
  /// @throws std::length_error If the diagnostic exceeds its limit.
  /// @throws std::filesystem::filesystem_error If path construction fails.
  const auto check_output = [](const std::string &path) -> amp::core::Status {
    std::error_code ec;
    const auto status = std::filesystem::symlink_status(path, ec);
    if (ec || !std::filesystem::is_regular_file(status) ||
        std::filesystem::is_symlink(status)) {
      amp::core::Diagnostic d;
      d.code = amp::core::ErrorCode::missing_resource;
      d.message = path + ": expected regular non-symlink output";
      return std::unexpected(std::move(d));
    }
    return {};
  };
  amp::build::NativeArchiveRequest request{directory,
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
      amp::build::run_native_archive(request, runner, filesystem, check_output);
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

  const amp::build::CompileRequest consumer{
      cxx, directory, source_dir + "/main.cpp", directory + "/main.o", "c++26",
      {},  true};
  auto compile = amp::build::plan_compile(consumer);
  assert(compile);
  amp::core::ProcessSpec compile_spec{
      compile->arguments, directory, {"PATH"}, {}, std::chrono::minutes(2)};
  auto compiled = runner.run(compile_spec);
  assert(compiled && compiled->exit_code == 0);
  if (!compiled || compiled->exit_code != 0)
    return 4;
  const auto executable = directory + "/consumer";
  amp::core::ProcessSpec link_spec{
      {cxx, "-o", executable, directory + "/main.o", request.archive},
      directory,
      {"PATH"},
      {},
      std::chrono::minutes(2)};
  auto linked = runner.run(link_spec);
  assert(linked && linked->exit_code == 0);
  if (!linked || linked->exit_code != 0)
    return 5;
  amp::core::ProcessSpec execute_spec{
      {executable}, directory, {"PATH"}, {}, std::chrono::seconds(30)};
  auto executed = runner.run(execute_spec);
  assert(executed && executed->exit_code == 0 &&
         executed->stdout_text == "standalone: 42\n");
  if (!executed || executed->exit_code != 0 ||
      executed->stdout_text != "standalone: 42\n")
    return 6;
  std::cout << executed->stdout_text;
}
