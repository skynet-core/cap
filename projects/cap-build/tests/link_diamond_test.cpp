#include <cassert>
#include <chrono>
#include <filesystem>
#include <string>
#include <vector>

import cap.build.actions;
import cap.core.process;

namespace fs = std::filesystem;

/// Exercise planned diamond archive ordering and negative real-link cases.
/// @param[in] argc int — Argument count, required to be five.
/// @param[in] argv char ** — Program name, C driver, archiver, fixture path
/// and absolute zlib SDK operand; tools and fixture must be available.
/// @return int — Zero when assertions pass; assertion failure aborts.
/// @throws std::bad_alloc If allocation fails.
/// @throws std::length_error If a string or vector exceeds its maximum size.
/// @throws std::filesystem::filesystem_error If filesystem operations fail.
int main(int argc, char **argv) {
  assert(argc == 5);
  /// const std::string — C driver, archiver and zlib SDK operand, respectively.
  const std::string compiler = argv[1], ar = argv[2], sdk = argv[4];
  /// const fs::path — Directory containing the diamond sources.
  const fs::path fixture = argv[3];
  /// const fs::path — Unique temporary work directory.
  const auto work =
      fs::temp_directory_path() /
      ("cap-link-diamond-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  assert(fs::create_directory(work));
  cap::core::LocalProcessRunner runner;
  /// Execute direct argv in the temporary work directory.
  /// @param[in] args const std::vector<std::string> & — Executable and argv.
  /// @return cap::core::ProcessResult — Child outcome; runner failure or
  /// timeout aborts via assertion.
  /// @throws std::bad_alloc If allocation fails.
  /// @throws std::length_error If a string or vector exceeds its maximum size.
  /// @throws std::filesystem::filesystem_error If path conversion fails.
  const auto execute = [&](const std::vector<std::string> &args) {
    cap::core::ProcessSpec spec;
    spec.argv = args;
    spec.cwd = work.string();
    spec.env_allowlist = {"PATH"};
    spec.timeout = std::chrono::seconds(20);
    auto result = runner.run(spec);
    assert(result && !result->timed_out);
    return *result;
  };
  /// Form a path inside the temporary work directory.
  /// @param[in] name const std::string & — Path component to append.
  /// @return std::string — Resulting path; no failure result.
  /// @throws std::bad_alloc If allocation fails.
  /// @throws std::length_error If a string exceeds its maximum size.
  /// @throws std::filesystem::filesystem_error If path conversion fails.
  const auto path = [&](const std::string &name) {
    return (work / name).string();
  };
  for (const auto &name :
       {"main", "left", "right", "leaf_left", "leaf_right"}) {
    auto action = cap::build::plan_compile(
        {compiler,
         work.string(),
         (fixture / (std::string(name) + ".c")).string(),
         path(std::string(name) + ".o"),
         "c23",
         {},
         false});
    assert(action && execute(action->arguments).exit_code == 0);
  }
  for (const auto &[name, objects] :
       std::vector<std::pair<std::string, std::vector<std::string>>>{
           {"left", {path("left.o")}},
           {"right", {path("right.o")}},
           {"leaf", {path("leaf_left.o"), path("leaf_right.o")}}}) {
    auto action = cap::build::plan_archive(
        {ar, work.string(), path("lib" + name + ".a"), objects});
    assert(action && execute(action->arguments).exit_code == 0);
  }
  auto inputs =
      cap::build::plan_link_inputs({"left", "right"},
                                   {{"left", path("libleft.a"), {"leaf"}, {}},
                                    {"right", path("libright.a"), {"leaf"}, {}},
                                    {"leaf", path("libleaf.a"), {}, {sdk}}},
                                   {});
  assert(inputs);
  auto link = cap::build::plan_link({compiler,
                                     work.string(),
                                     path("consumer"),
                                     {path("main.o")},
                                     inputs->ordered_archives,
                                     inputs->system_libraries});
  assert(link);
  /// const std::vector<std::string> — Expected ordered link operands.
  const std::vector<std::string> expected{path("main.o"), path("libleft.a"),
                                          path("libright.a"), path("libleaf.a"),
                                          sdk};
  assert(link->inputs == expected);
  std::vector<std::string> expected_argv{compiler, "-o", path("consumer")};
  expected_argv.insert(expected_argv.end(), expected.begin(), expected.end());
  assert(link->arguments == expected_argv);
  assert(execute(link->arguments).exit_code == 0);
  auto run = execute({path("consumer")});
  assert(run.exit_code == 0 && run.stdout_text == "diamond: 42\n");
  auto wrong = *link;
  std::swap(wrong.arguments[5], wrong.arguments[6]);
  /// const cap::core::ProcessResult — Link result with the leaf too early.
  const auto bad = execute(wrong.arguments);
  assert(bad.exit_code != 0 &&
         bad.stderr_text.find("leaf_right") != std::string::npos);
  wrong = *link;
  wrong.arguments.pop_back();
  /// const cap::core::ProcessResult — Link result with the SDK operand missing.
  const auto missing = execute(wrong.arguments);
  assert(missing.exit_code != 0 &&
         missing.stderr_text.find("zlibVersion") != std::string::npos);
  fs::remove_all(work);
}
