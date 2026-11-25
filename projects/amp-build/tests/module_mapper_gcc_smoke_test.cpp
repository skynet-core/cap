#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

import amp.build.actions;
import amp.build.modules;
import amp.compdb;
import amp.core.process;

namespace {

/// Runs one direct process with a private working directory.
/// @param[in] runner const amp::core::LocalProcessRunner& — Direct process
/// runner.
/// @param[in] argv const std::vector<std::string>& — Exact direct-exec argv,
/// including the absolute executable at index zero.
/// @param[in] cwd const std::filesystem::path& — Process working directory.
/// @param[in] success bool — Whether zero exit status is required.
/// @param[in] expected_failure std::string_view — Required stderr text on
/// failure, or empty to accept any diagnostic.
/// @return amp::core::ProcessResult — Captured child output and status.
/// Asserts the expected outcome; may propagate `std::bad_alloc`,
/// `std::length_error`, or `std::filesystem::filesystem_error`.
amp::core::ProcessResult
run_process(const amp::core::LocalProcessRunner &runner,
            const std::vector<std::string> &argv,
            const std::filesystem::path &cwd, bool success,
            std::string_view expected_failure = {}) {
  auto result =
      runner.run({argv, cwd.string(), {"PATH"}, {}, std::chrono::minutes(2)});
  assert(result && !result->timed_out);
  assert((result->exit_code == 0) == success);
  if (!success) {
    assert(!result->stderr_text.empty());
    if (!expected_failure.empty())
      assert(result->stderr_text.find(expected_failure) != std::string::npos);
  }
  return std::move(*result);
}

} // namespace

/// Exercises the real GCC mapper, compiler-action database, clangd and binary.
/// @param[in] argc int — Must be five, including argv[0].
/// @param[in] argv char** — Absolute GCC and clangd paths, fixture root, and
/// fresh absolute scratch directory.
/// @return int — Zero after successful GCC provider/importer compilation,
/// empty-map rejection, same-database clangd check, and `second: 42` execution.
/// May propagate `std::bad_alloc`, `std::length_error`,
/// `std::filesystem::filesystem_error`, or `std::ios_base::failure`.
int main(int argc, char **argv) {
  assert(argc == 5);
  const std::filesystem::path gcc(argv[1]);
  const std::filesystem::path clangd(argv[2]);
  const std::filesystem::path fixture(argv[3]);
  const std::filesystem::path scratch(argv[4]);
  assert(gcc.is_absolute() && clangd.is_absolute() && fixture.is_absolute() &&
         scratch.is_absolute());
  assert(!std::filesystem::exists(scratch));

  const auto provider_source = fixture / "lib/src/fixture.cpp";
  const auto importer_source = fixture / "app/src space/second.cpp";
  assert(std::filesystem::is_regular_file(provider_source));
  assert(std::filesystem::is_regular_file(importer_source));
  const auto provider_dir = scratch / "provider work";
  const auto importer_dir = scratch / "importer work";
  const auto empty_dir = scratch / "empty work";
  const auto link_dir = scratch / "link work";
  std::filesystem::create_directories(provider_dir);
  std::filesystem::create_directories(importer_dir);
  std::filesystem::create_directories(empty_dir);
  std::filesystem::create_directories(link_dir);

  const auto gcc_path = gcc.string();
  const auto provider_object = (provider_dir / "fixture.o").string();
  const auto provider_bmi = (provider_dir / "gcm.cache/fixture.gcm").string();
  const auto provider_argv = std::vector<std::string>{
      gcc_path, "-std=c++26",   "-fmodules", "-c", provider_source.string(),
      "-o",     provider_object};
  run_process(amp::core::LocalProcessRunner{}, provider_argv, provider_dir,
              true);
  assert(std::filesystem::is_regular_file(provider_bmi));

  const auto mapper =
      amp::build::format_module_map({{"fixture", provider_bmi}});
  assert(mapper && *mapper == "fixture " + provider_bmi + "\n");
  const auto mapper_path = importer_dir / "module.map";
  {
    std::ofstream file(mapper_path);
    file.exceptions(std::ios::badbit | std::ios::failbit);
    file << *mapper;
  }
  const auto importer_object = (importer_dir / "second.o").string();
  const auto importer_argv = std::vector<std::string>{
      gcc_path,    "-std=c++26",
      "-fmodules", "-fmodule-mapper=" + mapper_path.string(),
      "-c",        importer_source.string(),
      "-o",        importer_object};
  run_process(amp::core::LocalProcessRunner{}, importer_argv, importer_dir,
              true);
  assert(std::filesystem::is_regular_file(importer_object));

  const auto empty_map = empty_dir / "module.map";
  {
    std::ofstream file(empty_map);
    file.exceptions(std::ios::badbit | std::ios::failbit);
  }
  auto missing_mapper_argv = importer_argv;
  missing_mapper_argv[3] = "-fmodule-mapper=" + empty_map.string();
  missing_mapper_argv.back() = (empty_dir / "second.o").string();
  run_process(amp::core::LocalProcessRunner{}, missing_mapper_argv, empty_dir,
              false, "no such module");

  const amp::build::Action provider_action{amp::build::ActionKind::compile,
                                           provider_dir.string(),
                                           provider_argv,
                                           {provider_source.string()},
                                           {provider_object}};
  const amp::build::Action importer_action{amp::build::ActionKind::compile,
                                           importer_dir.string(),
                                           importer_argv,
                                           {importer_source.string()},
                                           {importer_object}};
  const auto database_json =
      amp::compdb::to_json({provider_action, importer_action});
  assert(database_json);
  const auto database = scratch / "compile_commands.json";
  {
    std::ofstream file(database);
    file.exceptions(std::ios::badbit | std::ios::failbit);
    file << *database_json << '\n';
  }

  const auto executable = (link_dir / "second").string();
  run_process(amp::core::LocalProcessRunner{},
              {gcc_path, provider_object, importer_object, "-o", executable},
              link_dir, true);
  const auto program = run_process(amp::core::LocalProcessRunner{},
                                   {executable}, link_dir, true);
  assert(program.stdout_text == "second: 42\n");

  const auto clangd_result =
      run_process(amp::core::LocalProcessRunner{},
                  {clangd.string(), "--experimental-modules-support",
                   "--query-driver=" + gcc_path,
                   "--compile-commands-dir=" + scratch.string(),
                   "--check=" + importer_source.string()},
                  scratch, true);
  assert(clangd_result.stderr_text.find("All checks completed, 0 errors") !=
         std::string::npos);
  std::cout << "compile database: " << database << '\n';
}
