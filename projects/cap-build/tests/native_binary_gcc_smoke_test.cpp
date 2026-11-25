#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

import cap.build.native_binary;
import cap.core.diagnostic;
import cap.core.filesystem;
import cap.core.process;

namespace {
/// Records executor requests and runs them locally for native smoke coverage.
class RecordingRunner final : public cap::core::IProcessRunner {
public:
  /// mutable std::vector<cap::core::ProcessSpec> — Requests actually executed.
  mutable std::vector<cap::core::ProcessSpec> calls;
  /// cap::core::LocalProcessRunner — Direct local process adapter.
  cap::core::LocalProcessRunner local;

  /// Executes the request, optionally forwarding an operator-set TMPDIR.
  /// @param[in] spec const cap::core::ProcessSpec& — Planned direct invocation.
  /// @return cap::core::Result<cap::core::ProcessResult> — Child result or
  ///         process diagnostic.
  /// @throws std::bad_alloc If recording or environment storage cannot
  /// allocate.
  /// @throws std::length_error If request storage exceeds its size limit.
  /// @note Exceptions from the local runner propagate unchanged.
  cap::core::Result<cap::core::ProcessResult>
  run(const cap::core::ProcessSpec &spec) const override {
    auto executed = spec;
    if (const char *tmpdir = std::getenv("TMPDIR");
        tmpdir != nullptr && *tmpdir != '\0' &&
        std::find(executed.env_allowlist.begin(), executed.env_allowlist.end(),
                  "TMPDIR") == executed.env_allowlist.end())
      executed.env_allowlist.push_back("TMPDIR");
    calls.push_back(executed);
    return local.run(executed);
  }
};
} // namespace

int main(int argc, char **argv) {
  assert(argc == 5);
  const std::string cc = argv[1], cxx = argv[2], directory = argv[3],
                    fixture = argv[4];
  std::error_code create_error;
  const bool created =
      std::filesystem::create_directory(directory, create_error);
  assert(created && !create_error);
  if (!created || create_error)
    return 2;
  cap::core::LocalFileSystem filesystem;
  RecordingRunner runner;
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
  cap::build::NativeBinaryRequest request{directory,
                                          cxx,
                                          directory + "/native-app",
                                          {{cc,
                                            directory,
                                            fixture + "/value.c",
                                            directory + "/value.o",
                                            "c17",
                                            {},
                                            false},
                                           {cxx,
                                            directory,
                                            fixture + "/answer.cppm",
                                            directory + "/answer.o",
                                            "c++26",
                                            {},
                                            true},
                                           {cxx,
                                            directory,
                                            fixture + "/main.cpp",
                                            directory + "/main.o",
                                            "c++26",
                                            {},
                                            true}}};
  auto built =
      cap::build::run_native_binary(request, runner, filesystem, check_output);
  if (!built)
    std::cerr << built.error().message << '\n';
  assert(built && built->compile_actions.size() == 3);
  assert(built->compile_actions[0].outputs.front() == directory + "/answer.o");
  assert(built->compile_actions[1].outputs.front() == directory + "/main.o");
  const std::vector<std::string> expected_link{cxx,
                                               "-o",
                                               directory + "/native-app",
                                               directory + "/value.o",
                                               directory + "/answer.o",
                                               directory + "/main.o"};
  assert(runner.calls.size() == 6); // two scans, three compiles, one link
  for (const auto &action : built->compile_actions) {
    const auto found =
        std::ranges::find_if(runner.calls, [&](const auto &call) {
          return call.argv == action.arguments;
        });
    assert(found != runner.calls.end());
    assert(found->cwd == directory);
    assert(found->timeout == std::chrono::minutes(2));
  }
  assert(runner.calls.back().argv == expected_link);
  assert(runner.calls.back().cwd == directory);
  cap::core::ProcessSpec execute{
      {built->executable}, directory, {"PATH"}, {}, std::chrono::seconds(30)};
  auto result = runner.run(execute);
  assert(result && !result->timed_out && result->exit_code == 0);
  assert(result->stdout_text == "native binary: 42\n");
  std::cout << result->stdout_text;
}
