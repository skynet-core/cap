#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <span>
#include <string>
#include <vector>

import amp.build.native_binary;
import amp.build.actions;
import amp.build.modules;
import amp.compdb;
import amp.core.diagnostic;
import amp.core.filesystem;
import amp.core.process;

namespace {
/// Records and delegates direct process requests to the local runner.
class RecordingRunner final : public amp::core::IProcessRunner {
public:
  /// mutable std::vector<amp::core::ProcessSpec> — Actual subprocess requests.
  mutable std::vector<amp::core::ProcessSpec> calls;
  /// amp::core::LocalProcessRunner — Direct process implementation.
  amp::core::LocalProcessRunner local;

  /// Executes a direct request after recording it.
  /// @param[in] spec const amp::core::ProcessSpec& — Request to execute.
  /// @return amp::core::Result<amp::core::ProcessResult> — Child result or
  ///         process diagnostic.
  /// @throws std::bad_alloc If recording the process cannot allocate.
  /// @throws std::length_error If the recording exceeds its size limit.
  /// @note Exceptions from the injected local runner propagate unchanged.
  /// @note An explicitly set TMPDIR is allowlisted for compiler scratch files.
  amp::core::Result<amp::core::ProcessResult>
  run(const amp::core::ProcessSpec &spec) const override {
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

/// Verifies a path is a regular nonsymlink file without reading it.
/// @param[in] path const std::string& — Artifact path to inspect.
/// @return amp::core::Status — Success for a regular nonsymlink file, else
///         a missing-resource diagnostic.
/// @throws std::bad_alloc If diagnostic construction cannot allocate.
/// @throws std::length_error If the diagnostic exceeds its size limit.
/// @throws std::filesystem::filesystem_error If path conversion throws.
amp::core::Status check_regular(const std::string &path) {
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  if (ec || !std::filesystem::is_regular_file(status) ||
      std::filesystem::is_symlink(status)) {
    amp::core::Diagnostic error;
    error.code = amp::core::ErrorCode::missing_resource;
    error.message = path + ": expected regular nonsymlink file";
    return std::unexpected(std::move(error));
  }
  return {};
}

/// Reserves a fresh private child directory.
/// @param[in] parent const std::string& — Existing scratch directory.
/// @param[in] name const std::string& — New child directory name.
/// @return std::string — Absolute created child path.
/// @throws std::bad_alloc If path storage cannot allocate.
/// @throws std::length_error If path storage exceeds its size limit.
/// @throws std::filesystem::filesystem_error If directory creation fails.
std::string fresh_directory(const std::string &parent,
                            const std::string &name) {
  const auto path = parent + "/" + name;
  if (!std::filesystem::create_directory(path))
    throw std::filesystem::filesystem_error(
        "cannot create fresh directory", path,
        std::make_error_code(std::errc::file_exists));
  return path;
}

/// Writes one small generated module source for the transitive-BMI case.
/// @param[in] path const std::string& — Fresh absolute source path.
/// @param[in] contents const std::string& — Complete source text.
/// @return amp::core::Status — Success or a transient write diagnostic.
/// @throws std::bad_alloc If path, content, or diagnostic storage cannot
/// allocate.
/// @throws std::length_error If path, content, or diagnostic storage exceeds
///         its size limit.
amp::core::Status write_source(const std::string &path,
                               const std::string &contents) {
  std::ofstream file(path, std::ios::binary);
  if (!file || !file.write(contents.data(),
                           static_cast<std::streamsize>(contents.size()))) {
    amp::core::Diagnostic error;
    error.code = amp::core::ErrorCode::transient_io;
    error.message = path + ": source fixture write failed";
    return std::unexpected(std::move(error));
  }
  return {};
}
} // namespace

/// Verifies real archive-to-binary BMI handoff, compdb and clangd behavior.
/// @param[in] argc int — Must be seven including the program name.
/// @param[in] argv char** — Absolute CC, CXX, AR, CLANGD, workspace fixture
///            root, and fresh scratch directory, in that order.
/// @return int — Zero on success; nonzero identifies setup/build/negative/
///         consumer/editor failure.
/// @throws std::bad_alloc If test state cannot allocate.
/// @throws std::length_error If test state exceeds its size limit.
/// @throws std::filesystem::filesystem_error If filesystem setup fails.
/// @note Exceptions from the filesystem and process adapters propagate.
int main(int argc, char **argv) {
  assert(argc == 7);
  if (argc != 7)
    return 2;
  for (int i = 1; i < argc; ++i)
    if (argv[i][0] != '/')
      return 2;
  const std::string cc = argv[1], cxx = argv[2], ar = argv[3], clangd = argv[4],
                    fixture = argv[5], scratch = argv[6];
  if (!std::filesystem::is_directory(fixture) ||
      !std::filesystem::create_directory(scratch))
    return 2;
  const auto library = fresh_directory(scratch, "provider cwd");
  const auto app = fresh_directory(scratch, "consumer cwd");
  const auto empty_app = fresh_directory(scratch, "empty-map cwd");
  const auto invalid_app = fresh_directory(scratch, "invalid-map cwd");
  const auto collision_app = fresh_directory(scratch, "collision-map cwd");
  const auto rejected_provider =
      fresh_directory(scratch, "rejected-provider cwd");
  const auto bmi_collision_provider =
      fresh_directory(scratch, "BMI collision provider cwd");
  const auto leaf_dir = fresh_directory(scratch, "transitive leaf cwd");
  const auto middle_dir = fresh_directory(scratch, "transitive middle cwd");
  const auto transitive_app = fresh_directory(scratch, "transitive app cwd");
  const auto fixture_provider = fixture + "/lib/src/fixture.cpp";
  const auto fixture_consumer = fixture + "/app/src space/second.cpp";
  amp::core::LocalFileSystem filesystem;
  RecordingRunner runner;

  amp::build::NativeArchiveRequest archive_request;
  archive_request.directory = library;
  archive_request.archiver = ar;
  archive_request.archive = library + "/libfixture.a";
  archive_request.compiles.push_back({cxx,
                                      library,
                                      fixture_provider,
                                      library + "/fixture.o",
                                      "c++26",
                                      {},
                                      true});
  archive_request.workspace_modules = amp::build::ModuleMap{};
  auto archive = amp::build::run_native_archive(archive_request, runner,
                                                filesystem, check_regular);
  if (!archive) {
    std::cerr << archive.error().message << '\n';
    return 3;
  }
  assert(archive->provided_modules.size() == 1);
  const auto provided = archive->provided_modules.find("fixture");
  assert(provided != archive->provided_modules.end());
  if (provided == archive->provided_modules.end())
    return 3;
  assert(std::filesystem::is_regular_file(provided->second));
  assert(archive->compile_actions.size() == 1);
  assert(archive->compile_actions.front().outputs.front() ==
         archive_request.compiles.front().output);
  assert(archive->compile_actions.front().outputs.back() == provided->second);

  amp::build::NativeArchiveRequest rejected_archive = archive_request;
  rejected_archive.directory = rejected_provider;
  rejected_archive.archive = rejected_provider + "/libfixture.a";
  rejected_archive.compiles.front().directory = rejected_provider;
  rejected_archive.compiles.front().output = rejected_provider + "/fixture.o";
  /// Injects failure only for a produced BMI output.
  /// @param[in] path const std::string& — Output path to check.
  /// @return amp::core::Status — Regular-file check result or injected
  ///         missing-resource diagnostic for a BMI.
  /// @throws std::bad_alloc If diagnostic construction cannot allocate.
  /// @throws std::length_error If the diagnostic exceeds its size limit.
  const auto reject_bmi = [](const std::string &path) -> amp::core::Status {
    if (!path.ends_with(".gcm"))
      return check_regular(path);
    amp::core::Diagnostic error;
    error.code = amp::core::ErrorCode::missing_resource;
    error.message = "injected BMI verification failure";
    return std::unexpected(std::move(error));
  };
  const auto before_rejected_provider = runner.calls.size();
  auto rejected = amp::build::run_native_archive(rejected_archive, runner,
                                                 filesystem, reject_bmi);
  assert(!rejected && runner.calls.size() == before_rejected_provider + 2);
  assert(runner.calls.back().argv.front() == cxx);

  amp::build::NativeArchiveRequest bmi_collision_request = archive_request;
  bmi_collision_request.directory = bmi_collision_provider;
  bmi_collision_request.archive = bmi_collision_provider + "/libfixture.a";
  bmi_collision_request.compiles.front().directory = bmi_collision_provider;
  bmi_collision_request.compiles.front().output =
      bmi_collision_provider + "/.amp-bmi-0.gcm";
  const auto before_bmi_collision = runner.calls.size();
  auto bmi_collision = amp::build::run_native_archive(
      bmi_collision_request, runner, filesystem, check_regular);
  assert(!bmi_collision && runner.calls.size() == before_bmi_collision + 1);
  assert(!std::filesystem::exists(bmi_collision_provider + "/.amp-module-map"));

  amp::build::NativeBinaryRequest binary_request;
  binary_request.directory = app;
  binary_request.link_driver = cxx;
  binary_request.executable = app + "/second";
  binary_request.compiles.push_back(
      {cxx, app, fixture_consumer, app + "/second.o", "c++26", {}, true});
  binary_request.link_inputs.ordered_archives = {archive->archive};
  binary_request.workspace_modules = archive->provided_modules;
  auto binary = amp::build::run_native_binary(binary_request, runner,
                                              filesystem, check_regular);
  if (!binary) {
    std::cerr << binary.error().message << '\n';
    return 4;
  }
  assert(binary->compile_actions.size() == 1);
  assert(binary->compile_actions.front().arguments.back().starts_with(
      "-fmodule-mapper="));
  assert(std::find(binary->compile_actions.front().inputs.begin(),
                   binary->compile_actions.front().inputs.end(),
                   provided->second) !=
         binary->compile_actions.front().inputs.end());

  const auto executed = runner.run(
      {{binary->executable}, app, {"PATH"}, {}, std::chrono::seconds(30)});
  assert(executed && executed->exit_code == 0 &&
         executed->stdout_text == "second: 42\n");
  if (!executed || executed->exit_code != 0 ||
      executed->stdout_text != "second: 42\n")
    return 6;

  const auto leaf_source = scratch + "/transitive_leaf.cppm";
  const auto middle_source = scratch + "/transitive_middle.cppm";
  const auto transitive_consumer_source = scratch + "/transitive_main.cpp";
  assert(write_source(leaf_source, "export module transitive_leaf;\n"
                                   "export int leaf_value() { return 42; }\n"));
  assert(write_source(middle_source,
                      "export module transitive_middle;\n"
                      "import transitive_leaf;\n"
                      "export int middle_value() { return leaf_value(); }\n"));
  const std::string transitive_consumer_text =
      "#include <cstdio>\n"
      "import transitive_middle;\n"
      "int main() { std::printf(\"second: %d\\n\", middle_value()); }\n";
  assert(transitive_consumer_text.find("import transitive_leaf") ==
         std::string::npos);
  assert(write_source(transitive_consumer_source, transitive_consumer_text));

  amp::build::NativeArchiveRequest leaf_request;
  leaf_request.directory = leaf_dir;
  leaf_request.archiver = ar;
  leaf_request.archive = leaf_dir + "/libleaf.a";
  leaf_request.compiles.push_back(
      {cxx, leaf_dir, leaf_source, leaf_dir + "/leaf.o", "c++26", {}, true});
  leaf_request.workspace_modules = amp::build::ModuleMap{};
  auto leaf_archive = amp::build::run_native_archive(leaf_request, runner,
                                                     filesystem, check_regular);
  assert(leaf_archive &&
         leaf_archive->provided_modules.contains("transitive_leaf"));
  if (!leaf_archive)
    return 8;

  amp::build::NativeArchiveRequest middle_request;
  middle_request.directory = middle_dir;
  middle_request.archiver = ar;
  middle_request.archive = middle_dir + "/libmiddle.a";
  middle_request.compiles.push_back({cxx,
                                     middle_dir,
                                     middle_source,
                                     middle_dir + "/middle.o",
                                     "c++26",
                                     {},
                                     true});
  middle_request.workspace_modules = leaf_archive->provided_modules;
  auto middle_archive = amp::build::run_native_archive(
      middle_request, runner, filesystem, check_regular);
  assert(middle_archive &&
         middle_archive->provided_modules.contains("transitive_middle"));
  if (!middle_archive)
    return 8;

  amp::build::ModuleMap transitive_modules = middle_archive->provided_modules;
  transitive_modules.insert(leaf_archive->provided_modules.begin(),
                            leaf_archive->provided_modules.end());
  amp::build::NativeBinaryRequest transitive_request;
  transitive_request.directory = transitive_app;
  transitive_request.link_driver = cxx;
  transitive_request.executable = transitive_app + "/second";
  transitive_request.compiles.push_back({cxx,
                                         transitive_app,
                                         transitive_consumer_source,
                                         transitive_app + "/second.o",
                                         "c++26",
                                         {},
                                         true});
  transitive_request.link_inputs.ordered_archives = {middle_archive->archive,
                                                     leaf_archive->archive};
  transitive_request.workspace_modules = transitive_modules;
  auto transitive_binary = amp::build::run_native_binary(
      transitive_request, runner, filesystem, check_regular);
  assert(transitive_binary && transitive_binary->compile_actions.size() == 1);
  if (!transitive_binary)
    return 8;
  const auto &transitive_action = transitive_binary->compile_actions.front();
  assert(transitive_action.inputs.front() == transitive_consumer_source);
  assert(std::find(transitive_action.inputs.begin(),
                   transitive_action.inputs.end(),
                   middle_archive->provided_modules.at("transitive_middle")) !=
         transitive_action.inputs.end());
  assert(std::find(transitive_action.inputs.begin(),
                   transitive_action.inputs.end(),
                   leaf_archive->provided_modules.at("transitive_leaf")) !=
         transitive_action.inputs.end());
  const auto transitive_run = runner.run({{transitive_binary->executable},
                                          transitive_app,
                                          {"PATH"},
                                          {},
                                          std::chrono::seconds(30)});
  assert(transitive_run && transitive_run->exit_code == 0 &&
         transitive_run->stdout_text == "second: 42\n");
  if (!transitive_run || transitive_run->exit_code != 0 ||
      transitive_run->stdout_text != "second: 42\n")
    return 8;

  std::vector<amp::build::Action> actual_actions =
      leaf_archive->compile_actions;
  actual_actions.insert(actual_actions.end(),
                        middle_archive->compile_actions.begin(),
                        middle_archive->compile_actions.end());
  actual_actions.insert(actual_actions.end(), archive->compile_actions.begin(),
                        archive->compile_actions.end());
  actual_actions.insert(actual_actions.end(), binary->compile_actions.begin(),
                        binary->compile_actions.end());
  actual_actions.insert(actual_actions.end(),
                        transitive_binary->compile_actions.begin(),
                        transitive_binary->compile_actions.end());
  const auto database = amp::compdb::to_json(actual_actions);
  assert(database);
  if (!database)
    return 5;
  const auto database_path = scratch + "/compile_commands.json";
  const auto database_bytes =
      std::as_bytes(std::span(database->data(), database->size()));
  auto database_write = filesystem.write_atomic(database_path, database_bytes);
  assert(database_write);
  if (!database_write)
    return 5;
  for (const auto &action : actual_actions) {
    const auto executed_action = std::find_if(
        runner.calls.begin(), runner.calls.end(),
        /// Matches an exported compile action to the exact child request.
        /// @param[in] call const amp::core::ProcessSpec& — Recorded request.
        /// @return bool — True when argv and cwd match exactly.
        [&](const amp::core::ProcessSpec &call) {
          return call.argv == action.arguments && call.cwd == action.directory;
        });
    assert(executed_action != runner.calls.end());
    if (executed_action == runner.calls.end())
      return 5;
  }

  amp::build::NativeBinaryRequest empty_request = binary_request;
  empty_request.directory = empty_app;
  empty_request.executable = empty_app + "/second";
  empty_request.compiles.front().directory = empty_app;
  empty_request.compiles.front().output = empty_app + "/second.o";
  empty_request.workspace_modules = amp::build::ModuleMap{};
  const auto before_empty = runner.calls.size();
  auto empty = amp::build::run_native_binary(empty_request, runner, filesystem,
                                             check_regular);
  assert(!empty && runner.calls.size() == before_empty + 1);
  assert(!std::filesystem::exists(empty_app + "/.amp-module-map"));

  amp::build::NativeBinaryRequest invalid_request = empty_request;
  invalid_request.directory = invalid_app;
  invalid_request.executable = invalid_app + "/second";
  invalid_request.compiles.front().directory = invalid_app;
  invalid_request.compiles.front().output = invalid_app + "/second.o";
  invalid_request.workspace_modules =
      amp::build::ModuleMap{{"fixture", scratch + "/missing.gcm"}};
  const auto before_invalid = runner.calls.size();
  auto invalid = amp::build::run_native_binary(invalid_request, runner,
                                               filesystem, check_regular);
  assert(!invalid && runner.calls.size() == before_invalid);

  auto collision_request = binary_request;
  collision_request.directory = collision_app;
  collision_request.executable = collision_app + "/second";
  collision_request.compiles.front().directory = collision_app;
  collision_request.compiles.front().output = collision_app + "/second.o";
  collision_request.link_inputs.ordered_archives = {collision_app +
                                                    "/.amp-module-map"};
  const auto before_collision = runner.calls.size();
  auto collision = amp::build::run_native_binary(collision_request, runner,
                                                 filesystem, check_regular);
  assert(!collision && runner.calls.size() == before_collision);
  assert(!std::filesystem::exists(collision_app + "/.amp-module-map"));

  amp::core::ProcessSpec editor{{clangd, "--check=" + fixture_consumer,
                                 "--compile-commands-dir=" + scratch,
                                 "--query-driver=" + cc + "," + cxx,
                                 "--experimental-modules-support"},
                                app,
                                {"PATH"},
                                {},
                                std::chrono::minutes(1)};
  const auto editor_result = runner.run(editor);
  const std::string editor_output =
      editor_result ? editor_result->stdout_text + editor_result->stderr_text
                    : std::string{};
  assert(editor_result && editor_result->exit_code == 0 &&
         editor_output.find("All checks completed, 0 errors") !=
             std::string::npos);
  if (!editor_result || editor_result->exit_code != 0 ||
      editor_output.find("All checks completed, 0 errors") == std::string::npos)
    return 7;
  std::cout << executed->stdout_text;
  return 0;
}
