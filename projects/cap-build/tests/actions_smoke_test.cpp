#include <cassert>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

import cap.build.actions;
import cap.core.process;
import cap.toolchain.host;

namespace fs = std::filesystem;

namespace {
/// Execute planned argv with a bounded timeout and print process failures.
/// @param[in] action const cap::build::Action & — Planned argv and cwd.
/// @param[in] runner const cap::core::IProcessRunner & — Process adapter.
/// @return bool — True for a successful exit without timeout; false otherwise.
/// @throws std::bad_alloc If allocation fails.
/// @throws std::length_error If a string or vector exceeds its maximum size.
/// @note Exceptions from the injected runner and configured streams propagate.
bool run(const cap::build::Action &action,
         const cap::core::IProcessRunner &runner) {
  cap::core::ProcessSpec process;
  process.argv = action.arguments;
  process.cwd = action.directory;
  process.env_allowlist = {"PATH"};
  process.timeout = std::chrono::seconds(20);
  auto result = runner.run(process);
  if (!result) {
    std::cerr << result.error().format() << '\n';
    return false;
  }
  if (result->exit_code != 0 || result->timed_out)
    std::cerr << result->stderr_text;
  return !result->timed_out && result->exit_code == 0;
}

/// Execute a built program with a bounded timeout.
/// @param[in] path const std::string & — Executable path.
/// @param[in] directory const std::string & — Working directory.
/// @param[in] runner const cap::core::IProcessRunner & — Process adapter.
/// @return bool — True for a successful exit without timeout; false otherwise.
/// @throws std::bad_alloc If allocation fails.
/// @throws std::length_error If a string or vector exceeds its maximum size.
/// @note Exceptions from the injected runner propagate.
bool run_program(const std::string &path, const std::string &directory,
                 const cap::core::IProcessRunner &runner) {
  cap::core::ProcessSpec process;
  process.argv = {path};
  process.cwd = directory;
  process.env_allowlist = {"PATH"};
  process.timeout = std::chrono::seconds(10);
  auto result = runner.run(process);
  return result && !result->timed_out && result->exit_code == 0;
}
} // namespace

/// Compile, archive, link and run the fixture and resolved math SDK consumer.
/// @param[in] argc int — Argument count, required to be five.
/// @param[in] argv char ** — Program name, C++ driver, C driver, archiver and
/// fixture directory; tools and fixture must be available.
/// @return int — Zero when assertions pass; assertion failure aborts.
/// @throws std::bad_alloc If allocation fails.
/// @throws std::length_error If a string or vector exceeds its maximum size.
/// @throws std::filesystem::filesystem_error If filesystem operations fail.
/// @note Exceptions from process adapters and configured streams propagate.
int main(int argc, char **argv) {
  assert(argc == 5);
  /// const std::string — C++ compiler path from the test invocation.
  const std::string compiler = argv[1];
  /// const std::string — C compiler path from the test invocation.
  const std::string c_compiler = argv[2];
  /// const std::string — Archiver path from the test invocation.
  const std::string archiver = argv[3];
  /// const fs::path — Fixture directory supplied by the test invocation.
  const fs::path fixture = argv[4];
  assert(fs::is_regular_file(fixture / "provider.cpp"));
  assert(fs::is_regular_file(fixture / "first.cpp"));
  assert(fs::is_regular_file(fixture / "second.cpp"));
  assert(fs::is_regular_file(fixture / "platform_linux.c"));
  assert(fs::is_regular_file(fixture / "math_provider.c"));
  assert(fs::is_regular_file(fixture / "math_consumer.c"));

  fs::path work;
  for (unsigned attempt = 0; attempt != 100; ++attempt) {
    work = fs::temp_directory_path() /
           ("cap-actions-smoke-" + std::to_string(attempt) + "-" +
            std::to_string(
                std::chrono::steady_clock::now().time_since_epoch().count()));
    if (fs::create_directory(work))
      break;
  }
  assert(fs::is_directory(work));
  fs::create_directories(work / "gcm.cache");

  cap::core::LocalProcessRunner runner;
  /// Plan an object compilation using the fixture and temporary directory.
  /// @param[in] source const std::string & — Fixture-relative source name.
  /// @param[in] name const std::string & — Output object name.
  /// @param[in] tool const std::string & — Absolute compiler path.
  /// @param[in] standard const std::string & — Language standard.
  /// @param[in] module_mode bool — Whether to enable C++ modules.
  /// @return cap::build::Action — Planned action; a failed plan aborts.
  /// @throws std::bad_alloc If allocation fails.
  /// @throws std::length_error If a string or vector exceeds its maximum size.
  /// @throws std::filesystem::filesystem_error If path conversion fails.
  const auto object = [&](const std::string &source, const std::string &name,
                          const std::string &tool, const std::string &standard,
                          bool module_mode) {
    auto action = cap::build::plan_compile({tool,
                                            work.string(),
                                            (fixture / source).string(),
                                            (work / name).string(),
                                            standard,
                                            {},
                                            module_mode});
    assert(action);
    return *action;
  };
  /// const cap::build::Action — Module provider compilation.
  const auto provider =
      object("provider.cpp", "provider.o", compiler, "c++26", true);
  /// const cap::build::Action — First C++ consumer compilation.
  const auto first = object("first.cpp", "first.o", compiler, "c++26", true);
  /// const cap::build::Action — Second C++ consumer compilation.
  const auto second = object("second.cpp", "second.o", compiler, "c++26", true);
  /// const cap::build::Action — Platform C source compilation.
  const auto platform =
      object("platform_linux.c", "platform.o", c_compiler, "c23", false);
  /// const cap::build::Action — Math provider C source compilation.
  const auto math_provider =
      object("math_provider.c", "math_provider.o", c_compiler, "c23", false);
  /// const cap::build::Action — Math consumer C source compilation.
  const auto math_consumer =
      object("math_consumer.c", "math_consumer.o", c_compiler, "c23", false);
  assert(run(provider, runner));
  assert(run(first, runner));
  assert(run(second, runner));
  assert(run(platform, runner));
  assert(run(math_provider, runner));
  assert(run(math_consumer, runner));

  auto archive = cap::build::plan_archive(
      {archiver,
       work.string(),
       (work / "libfixture.a").string(),
       {(work / "provider.o").string(), (work / "platform.o").string()}});
  assert(archive && run(*archive, runner));
  auto math_archive =
      cap::build::plan_archive({archiver,
                                work.string(),
                                (work / "libmath_provider.a").string(),
                                {(work / "math_provider.o").string()}});
  assert(math_archive && run(*math_archive, runner));
  for (const auto &name : {"first", "second"}) {
    auto link =
        cap::build::plan_link({compiler,
                               work.string(),
                               (work / name).string(),
                               {(work / (std::string(name) + ".o")).string()},
                               {(work / "libfixture.a").string()},
                               {}});
    assert(link && run(*link, runner));
    assert(run_program((work / name).string(), work.string(), runner));
  }
  /// const cap::core::Result<cap::toolchain::HostToolchain> — Probed tools.
  const auto host = cap::toolchain::probe_host_toolchain(
      {c_compiler, compiler, archiver}, runner);
  assert(host);
  /// const cap::core::Result<std::vector<std::string>> — Resolved math operand.
  const auto system_libraries =
      cap::toolchain::resolve_system_libraries(*host, {"m"}, runner);
  assert(system_libraries && system_libraries->size() == 1 &&
         system_libraries->front().front() == '/');
  auto math_link =
      cap::build::plan_link({compiler,
                             work.string(),
                             (work / "math_consumer").string(),
                             {(work / "math_consumer.o").string()},
                             {(work / "libmath_provider.a").string()},
                             *system_libraries});
  assert(math_link);
  assert(math_link->arguments.back() == system_libraries->front());
  assert(math_link->inputs.back() == system_libraries->front());
  assert(run(*math_link, runner));
  assert(run_program((work / "math_consumer").string(), work.string(), runner));
  fs::remove_all(work);
}
