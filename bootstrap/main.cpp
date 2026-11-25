// cap stage-0 bootstrap driver (M0).
// Compiles the stage-1 cap executable with the pinned system compiler and
// the installed Boost, then verifies it. Never invokes CMake/Meson/Ninja.
// Dogfoods cap-core: process execution and filesystem access go through
// the cap-core adapters, not raw syscalls.
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include <boost/program_options.hpp>

import cap.core.diagnostic;
import cap.core.process;
import cap.bootstrap.path_io;
import cap.bootstrap.stage_status;

namespace {

using boost::program_options::value;
using cap::core::Diagnostic;
using cap::core::ErrorCode;
using cap::core::LocalProcessRunner;
using cap::core::ProcessSpec;

std::string find_cxx() {
  if (const char *cxx = std::getenv("CXX"))
    return cxx;
  return "c++";
}

Diagnostic missing(const std::string &path, const std::string &what) {
  Diagnostic d;
  d.code = ErrorCode::missing_resource;
  d.message = path + ": " + what;
  return d;
}

} // namespace

int main(int argc, char **argv) {
  boost::program_options::options_description desc(
      "cap stage-0 bootstrap driver");
  desc.add_options()("boost-prefix", value<std::string>(),
                     "installed Boost prefix")(
      "source-root", value<std::string>(), "cap source root")(
      "output-dir", value<std::string>(), "directory for the stage-1 binary")(
      "cxx", value<std::string>()->default_value(find_cxx()), "C++ compiler");
  boost::program_options::variables_map vm;
  boost::program_options::store(
      boost::program_options::parse_command_line(argc, argv, desc), vm);
  boost::program_options::notify(vm);

  const auto fail = [](const Diagnostic &d) -> int {
    std::printf("bootstrap: %s\n", d.format().c_str());
    return 1;
  };

  const std::string prefix = vm.at("boost-prefix").as<std::string>();
  const std::string root = vm.at("source-root").as<std::string>();
  const std::string out_dir = vm.at("output-dir").as<std::string>();
  const std::string cxx = vm.at("cxx").as<std::string>();

  // Check that the installed version file is readable.
  const auto canonical_prefix = cap::bootstrap::canonicalize(prefix);
  if (!canonical_prefix)
    return fail(missing(prefix, "Boost prefix is not usable"));
  const auto boost_version =
      (std::filesystem::path(*canonical_prefix) / "boost-version.txt").string();
  const auto version_file = cap::bootstrap::check_readable(boost_version);
  if (!version_file) {
    return fail(Diagnostic{ErrorCode::integrity_failure,
                           cap::core::Severity::error,
                           "installed Boost lacks a readable boost-version.txt",
                           {},
                           {},
                           "rerun: make boost with the verified pin"});
  }

  const std::string stage1_src = root + "/bootstrap/stage1.cpp";
  if (!cap::bootstrap::check_readable(stage1_src))
    return fail(missing(stage1_src, "stage-1 source missing"));
  const auto canonical_root = cap::bootstrap::canonicalize(root);
  const auto canonical_out = cap::bootstrap::canonicalize(out_dir);
  if (!canonical_root || !canonical_out)
    return fail(
        missing(!canonical_root ? root : out_dir, "directory is not usable"));
  const auto module_template = *canonical_out + "/.cap-modules-XXXXXX";
  std::vector<char> module_dir(module_template.begin(), module_template.end());
  module_dir.push_back('\0');
  if (::mkdtemp(module_dir.data()) == nullptr)
    return fail(
        missing(*canonical_out, "cannot create stage-1 module directory"));

  // Compile the module before stage-1 with the pinned compiler.
  ProcessSpec build;
  build.cwd = module_dir.data();
  // The child environment is built from this allowlist (cap-core contract:
  // empty allowlist means empty environment). PATH lets g++ locate the
  // linker; the compiler prefix itself is pinned on the command line.
  build.env_allowlist = {"PATH"};
  for (const std::string name : {"diagnostic", "digest", "filesystem"}) {
    build.argv = {
        cxx,       "-std=c++26",
        "-O2",     "-Wall",
        "-Wextra", "-fmodules",
        "-x",      "c++",
        "-c",      *canonical_root + "/projects/cap-core/" + name + ".cppm",
        "-o",      name + ".o"};
    auto module = LocalProcessRunner().run(build);
    if (!module)
      return fail(module.error());
    if (module->exit_code != 0) {
      std::printf("%s\n", module->stderr_text.c_str());
      return fail(Diagnostic{ErrorCode::internal,
                             cap::core::Severity::error,
                             "stage-1 module compile exited " +
                                 std::to_string(module->exit_code),
                             {},
                             {},
                             ""});
    }
  }
  for (const std::string name : {"path_io.cppm", "path_io.cpp"}) {
    build.argv = {cxx,       "-std=c++26",
                  "-O2",     "-Wall",
                  "-Wextra", "-fmodules",
                  "-x",      "c++",
                  "-c",      *canonical_root + "/bootstrap/" + name,
                  "-o",      name + ".o"};
    auto module = LocalProcessRunner().run(build);
    if (!module)
      return fail(module.error());
    if (module->exit_code != 0) {
      std::printf("%s\n", module->stderr_text.c_str());
      return fail(Diagnostic{ErrorCode::internal,
                             cap::core::Severity::error,
                             "stage-1 module compile exited " +
                                 std::to_string(module->exit_code),
                             {},
                             {},
                             ""});
    }
  }
  build.argv = {cxx,
                "-std=c++26",
                "-O2",
                "-Wall",
                "-Wextra",
                "-fmodules",
                "-I" + *canonical_root + "/projects/cap-core/include",
                "-I" + *canonical_prefix + "/include",
                *canonical_root + "/bootstrap/stage1.cpp",
                "diagnostic.o",
                "digest.o",
                "filesystem.o",
                "path_io.cppm.o",
                "path_io.cpp.o",
                "-L" + *canonical_prefix + "/lib",
                "-lboost_filesystem",
                "-lboost_program_options",
                "-lboost_json",
                "-o",
                *canonical_out + "/cap-stage1"};
  auto built = LocalProcessRunner().run(build);
  if (!built) {
    return fail(Diagnostic{ErrorCode::transient_io,
                           cap::core::Severity::error,
                           "stage-1 compile failed",
                           {},
                           {built.error()},
                           ""});
  }
  if (built->exit_code != 0) {
    Diagnostic d;
    d.code = ErrorCode::internal;
    d.message = "stage-1 compile exited " + std::to_string(built->exit_code);
    std::printf("%s\n", built->stderr_text.c_str());
    return fail(d);
  }

  // Verify stage-1: it must report its status JSON and pass its self-check.
  ProcessSpec run;
  run.argv = {out_dir + "/cap-stage1", "--boost-prefix", *canonical_prefix};
  auto status = LocalProcessRunner().run(run);
  if (!status) {
    return fail(Diagnostic{ErrorCode::integrity_failure,
                           cap::core::Severity::error,
                           "stage-1 verification failed",
                           {},
                           {status.error()},
                           ""});
  }
  if (status->exit_code != 0) {
    Diagnostic d{ErrorCode::integrity_failure,
                 cap::core::Severity::error,
                 "stage-1 verification failed: exit " +
                     std::to_string(status->exit_code),
                 {},
                 {},
                 ""};
    return fail(d);
  }
  if (!cap::bootstrap::valid_stage_status(status->stdout_text)) {
    return fail(Diagnostic{ErrorCode::integrity_failure,
                           cap::core::Severity::error,
                           "stage-1 output is not the status JSON",
                           {},
                           {},
                           ""});
  }

  std::printf("bootstrap: stage-1 executable ready at %s/cap-stage1\n",
              out_dir.c_str());
  std::printf("%s\n", status->stdout_text.c_str());
  return 0;
}
