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

namespace fs = std::filesystem;

namespace {
/// Execute the supplied direct argv in the isolated work directory.
/// @param[in] runner cap::core::LocalProcessRunner& — Direct process runner.
/// @param[in] directory const std::string& — Existing private working path.
/// @param[in] arguments std::vector<std::string> — Owned executable and args.
/// @return cap::core::ProcessResult — Process result; callers inspect exit
///         status and stdout as required by the fixture.
/// @throws std::bad_alloc If process state cannot allocate.
/// @throws std::length_error If process state exceeds its limit.
/// @note Asserts if the runner returns an error or times out; runner
///       exceptions propagate unchanged.
auto run(cap::core::LocalProcessRunner &runner, const std::string &directory,
         std::vector<std::string> arguments) -> cap::core::ProcessResult {
  cap::core::ProcessSpec spec{
      std::move(arguments), directory, {"PATH"}, {}, std::chrono::minutes(2)};
  auto result = runner.run(spec);
  assert(result && !result->timed_out);
  return *result;
}

/// Create an archive from the given planned C object paths.
/// @param[in] runner cap::core::LocalProcessRunner& — Direct process runner.
/// @param[in] compiler const std::string& — Trusted absolute C compiler.
/// @param[in] archiver const std::string& — Trusted absolute archiver.
/// @param[in] directory const std::string& — Existing private work path.
/// @param[in] fixture const std::string& — Existing link-diamond fixture root.
/// @param[in] names std::vector<std::string> — Fixture source stems in archive
///            member order.
/// @param[in] archive_name const std::string& — Direct-child archive filename.
/// @return std::string — Created absolute archive path.
/// @throws std::bad_alloc If owned plan state cannot allocate.
/// @throws std::length_error If plan state exceeds its limit.
/// @note Asserts if planning or a process fails; process-runner exceptions
///       propagate unchanged.
auto make_archive(cap::core::LocalProcessRunner &runner,
                  const std::string &compiler, const std::string &archiver,
                  const std::string &directory, const std::string &fixture,
                  const std::vector<std::string> &names,
                  const std::string &archive_name) -> std::string {
  std::vector<std::string> objects;
  for (const auto &name : names) {
    const auto object = directory + "/" + name + ".o";
    auto action = cap::build::plan_compile({compiler,
                                            directory,
                                            fixture + "/" + name + ".c",
                                            object,
                                            "c17",
                                            {},
                                            false});
    assert(action);
    assert(run(runner, directory, action->arguments).exit_code == 0);
    objects.push_back(object);
  }
  const auto archive = directory + "/" + archive_name;
  auto action =
      cap::build::plan_archive({archiver, directory, archive, objects});
  assert(action);
  assert(run(runner, directory, action->arguments).exit_code == 0);
  return archive;
}
} // namespace

/// Build a real archive diamond through the native binary executor and prove
/// incorrect archive order and omission of the SDK operand fail at link time.
/// @param[in] argc int — Must be seven including the program name.
/// @param[in] argv char** — Program, C driver, C++ driver, archiver, fixture,
///            trusted SDK library path, and existing scratch root.
/// @return int — Zero on success, nonzero on invalid arguments, setup, link,
///         or runtime failure.
/// @throws std::bad_alloc If test state cannot allocate.
/// @throws std::length_error If test state exceeds its limit.
/// @throws std::filesystem::filesystem_error If filesystem operations throw.
int main(int argc, char **argv) {
  assert(argc == 7);
  if (argc != 7)
    return 2;
  const std::string cc = argv[1], cxx = argv[2], ar = argv[3],
                    fixture = argv[4], sdk = argv[5], scratch = argv[6];
  const auto root =
      scratch + "/native-link-diamond-" +
      std::to_string(
          std::chrono::steady_clock::now().time_since_epoch().count());
  const auto archive_dir = root + "/archives";
  std::error_code ec;
  if (!fs::create_directories(archive_dir, ec) || ec)
    return 2;

  cap::core::LocalProcessRunner runner;
  const auto left =
      make_archive(runner, cc, ar, archive_dir, fixture, {"left"}, "libleft.a");
  const auto right = make_archive(runner, cc, ar, archive_dir, fixture,
                                  {"right"}, "libright.a");
  const auto leaf = make_archive(runner, cc, ar, archive_dir, fixture,
                                 {"leaf_left", "leaf_right"}, "libleaf.a");
  cap::core::LocalFileSystem filesystem;

  /// Verify a generated path is a regular non-symlink file without reading it.
  /// @param[in] path const std::string& — Output path to inspect.
  /// @return cap::core::Status — Success or missing_resource diagnostic.
  /// @throws std::bad_alloc If diagnostic storage cannot allocate.
  /// @throws std::length_error If diagnostic storage exceeds its limit.
  const auto check_output = [](const std::string &path) -> cap::core::Status {
    std::error_code status_error;
    const auto status = fs::symlink_status(path, status_error);
    if (status_error || !fs::is_regular_file(status) ||
        fs::is_symlink(status)) {
      cap::core::Diagnostic diagnostic;
      diagnostic.code = cap::core::ErrorCode::missing_resource;
      diagnostic.message = path + ": expected regular non-symlink output";
      return std::unexpected(std::move(diagnostic));
    }
    return {};
  };

  /// Build one binary with the supplied ordered terminal link operands.
  /// @param[in] name const std::string& — Distinct private subdirectory name.
  /// @param[in] archives std::vector<std::string> — Ordered absolute archives.
  /// @param[in] sdk_operands std::vector<std::string> — Ordered SDK operands.
  /// @return cap::core::Result<cap::build::NativeBinaryResult> — Executor
  ///         result, including process or validation failure.
  /// @throws std::bad_alloc If request or result storage cannot allocate.
  /// @throws std::length_error If request or result exceeds its limit.
  /// @note Exceptions from injected executor adapters propagate unchanged.
  const auto build = [&](const std::string &name,
                         std::vector<std::string> archives,
                         std::vector<std::string> sdk_operands)
      -> cap::core::Result<cap::build::NativeBinaryResult> {
    const auto directory = root + "/" + name;
    std::error_code create_error;
    if (!fs::create_directory(directory, create_error) || create_error) {
      cap::core::Diagnostic diagnostic;
      diagnostic.code = cap::core::ErrorCode::invalid_input;
      diagnostic.message = "could not create binary work directory";
      return std::unexpected(std::move(diagnostic));
    }
    cap::build::NativeBinaryRequest request{
        directory,
        cxx,
        directory + "/diamond",
        {{cc,
          directory,
          fixture + "/main.c",
          directory + "/main.o",
          "c17",
          {},
          false}},
        {std::move(archives), std::move(sdk_operands)}};
    return cap::build::run_native_binary(request, runner, filesystem,
                                         check_output);
  };

  auto correct = build("correct", {left, right, leaf}, {sdk});
  assert(correct);
  if (!correct)
    return 3;
  auto executed = run(runner, root + "/correct", {correct->executable});
  assert(executed.exit_code == 0 && executed.stdout_text == "diamond: 42\n");
  if (executed.exit_code != 0 || executed.stdout_text != "diamond: 42\n")
    return 4;

  auto premature = build("premature", {left, leaf, right}, {sdk});
  assert(!premature);
  if (premature ||
      premature.error().message.find("leaf_right") == std::string::npos)
    return 5;
  auto missing_sdk = build("missing-sdk", {left, right, leaf}, {});
  assert(!missing_sdk);
  if (missing_sdk ||
      missing_sdk.error().message.find("zlibVersion") == std::string::npos)
    return 6;

  std::cout << executed.stdout_text;
  return 0;
}
