#include <cassert>
#include <expected>
#include <span>
#include <string>
#include <utility>
#include <vector>

import amp.build.native_binary;
import amp.core.diagnostic;
import amp.core.filesystem;
import amp.core.process;

namespace {
class Runner final : public amp::core::IProcessRunner {
public:
  mutable std::vector<amp::core::ProcessSpec> calls;
  int fail_at = -1;
  int timeout_at = -1;
  int error_at = -1;
  amp::core::Result<amp::core::ProcessResult>
  run(const amp::core::ProcessSpec &spec) const override {
    calls.push_back(spec);
    if (static_cast<int>(calls.size()) == error_at) {
      amp::core::Diagnostic d;
      d.code = amp::core::ErrorCode::transient_io;
      d.message = "injected runner error";
      return std::unexpected(std::move(d));
    }
    if (static_cast<int>(calls.size()) == timeout_at)
      return amp::core::ProcessResult{0, {}, "injected timeout", true};
    if (static_cast<int>(calls.size()) == fail_at)
      return amp::core::ProcessResult{1, {}, "injected failure", false};
    return amp::core::ProcessResult{0, {}, {}, false};
  }
};

class Files final : public amp::core::IFileSystem {
public:
  enum class Mode { normal, read_error, malformed, missing_provider };
  explicit Files(Mode mode = Mode::normal) : mode(mode) {}
  Mode mode;

  amp::core::Result<std::vector<std::byte>>
  read(const std::string &path) const override {
    if (mode == Mode::read_error) {
      amp::core::Diagnostic d;
      d.code = amp::core::ErrorCode::missing_resource;
      d.message = "injected missing report";
      return std::unexpected(std::move(d));
    }
    const std::string report =
        mode == Mode::malformed ? "{bad"
        : path.ends_with("provider.o.p1689.json")
            ? R"({"version":0,"revision":0,"rules":[{"primary-output":"/tmp/amp-private/provider.o","provides":[{"logical-name":"math"}]}]})"
        : mode == Mode::missing_provider
            ? R"({"version":0,"revision":0,"rules":[{"primary-output":"/tmp/amp-private/importer.o","requires":[{"logical-name":"absent"}]}]})"
            : R"({"version":0,"revision":0,"rules":[{"primary-output":"/tmp/amp-private/importer.o","requires":[{"logical-name":"math"}]}]})";
    std::vector<std::byte> bytes;
    for (const char c : report)
      bytes.push_back(static_cast<std::byte>(c));
    return bytes;
  }
  amp::core::Status write_atomic(const std::string &,
                                 std::span<const std::byte>) const override {
    assert(false);
    return {};
  }
  amp::core::Result<std::vector<std::string>>
  enumerate(const std::string &) const override {
    assert(false);
    return std::vector<std::string>{};
  }
  amp::core::Result<std::string>
  canonicalize(const std::string &) const override {
    assert(false);
    return std::string{};
  }
};

amp::build::NativeBinaryRequest request() {
  return {"/tmp/amp-private",
          "/usr/bin/g++",
          "/tmp/amp-private/app",
          {{"/usr/bin/gcc",
            "/tmp/amp-private",
            "/tmp/input.c",
            "/tmp/amp-private/c.o",
            "c17",
            {},
            false},
           {"/usr/bin/g++",
            "/tmp/amp-private",
            "/tmp/provider.cppm",
            "/tmp/amp-private/provider.o",
            "c++26",
            {},
            true},
           {"/usr/bin/g++",
            "/tmp/amp-private",
            "/tmp/importer.cpp",
            "/tmp/amp-private/importer.o",
            "c++26",
            {},
            true}}};
}
} // namespace

int main() {
  Files files;
  const auto verified = [](const std::string &) -> amp::core::Status {
    return {};
  };
  {
    Runner runner;
    auto invalid = request();
    invalid.compiles.back().output = "/tmp/amp-private/provider.o.p1689.json";
    assert(!amp::build::run_native_binary(invalid, runner, files, verified));
    assert(runner.calls.empty());
  }
  {
    Runner runner;
    auto with_links = request();
    with_links.link_inputs.ordered_archives = {"/tmp/libfirst.a",
                                               "/tmp/libleaf.a"};
    with_links.link_inputs.system_libraries = {"/sdk/libz.so", "/sdk/libz.so"};
    auto result =
        amp::build::run_native_binary(with_links, runner, files, verified);
    assert(result);
    const auto &argv = runner.calls.back().argv;
    assert(argv[argv.size() - 4] == "/tmp/libfirst.a");
    assert(argv[argv.size() - 3] == "/tmp/libleaf.a");
    assert(argv[argv.size() - 2] == "/sdk/libz.so");
    assert(argv[argv.size() - 1] == "/sdk/libz.so");
  }
  for (const auto &bad_link_input :
       {std::string("/tmp/amp-private/app"), std::string("relative")}) {
    Runner runner;
    auto invalid = request();
    invalid.link_inputs.ordered_archives = {bad_link_input};
    assert(!amp::build::run_native_binary(invalid, runner, files, verified));
    assert(runner.calls.empty());
  }
  for (const auto &collision :
       {std::pair{std::string("/tmp/amp-private/app"), false},
        std::pair{std::string("/tmp/amp-private/c.o"), false},
        std::pair{std::string("/tmp/amp-private/provider.o.p1689.json"), false},
        std::pair{std::string("/tmp/amp-private/app"), true},
        std::pair{std::string("/tmp/amp-private/c.o"), true},
        std::pair{std::string("/tmp/amp-private/provider.o.p1689.json"),
                  true}}) {
    Runner runner;
    auto invalid = request();
    auto &paths = collision.second ? invalid.link_inputs.system_libraries
                                   : invalid.link_inputs.ordered_archives;
    paths.push_back(collision.first);
    assert(!amp::build::run_native_binary(invalid, runner, files, verified));
    assert(runner.calls.empty());
  }
  for (const auto &bad_executable :
       {"/tmp/other/app", "/tmp/amp-private/nested/app", "/tmp/amp-private/"}) {
    Runner runner;
    auto invalid = request();
    invalid.executable = bad_executable;
    assert(!amp::build::run_native_binary(invalid, runner, files, verified));
    assert(runner.calls.empty());
  }
  for (const auto &bad_name :
       {".", "..", "gcm.cache", "compile_commands.json"}) {
    Runner runner;
    auto invalid = request();
    invalid.executable = std::string("/tmp/amp-private/") + bad_name;
    assert(!amp::build::run_native_binary(invalid, runner, files, verified));
    assert(runner.calls.empty());
  }
  {
    Runner runner;
    auto invalid = request();
    invalid.executable = "/tmp/amp-private/app";
    invalid.executable.push_back('\0');
    invalid.executable += "evil";
    assert(!amp::build::run_native_binary(invalid, runner, files, verified));
    assert(runner.calls.empty());
  }
  {
    Runner runner;
    auto invalid = request();
    invalid.compiles.back().directory = "/tmp/other";
    assert(!amp::build::run_native_binary(invalid, runner, files, verified));
    assert(runner.calls.empty());
  }
  {
    Runner runner;
    auto invalid = request();
    invalid.compiles.back().cxx_modules = false;
    assert(!amp::build::run_native_binary(invalid, runner, files, verified));
    assert(runner.calls.empty());
  }
  {
    Runner runner;
    auto invalid = request();
    invalid.compiles.back().standard = "c++42";
    assert(!amp::build::run_native_binary(invalid, runner, files, verified));
    assert(runner.calls.empty());
  }
  {
    Runner runner;
    auto invalid = request();
    invalid.executable = "/tmp/amp-private/input.c";
    invalid.compiles.front().source = invalid.executable;
    auto result =
        amp::build::run_native_binary(invalid, runner, files, verified);
    assert(!result &&
           result.error().message.find("collides") != std::string::npos);
    assert(runner.calls.empty());
  }
  {
    Runner runner;
    auto invalid = request();
    invalid.compiles.back().source = invalid.compiles.front().source;
    assert(!amp::build::run_native_binary(invalid, runner, files, verified));
    assert(runner.calls.empty());
  }
  {
    Runner runner;
    auto invalid = request();
    invalid.compiles.back().output = invalid.compiles.front().output;
    assert(!amp::build::run_native_binary(invalid, runner, files, verified));
    assert(runner.calls.empty());
  }
  {
    Runner runner;
    runner.fail_at = 1;
    auto result =
        amp::build::run_native_binary(request(), runner, files, verified);
    assert(!result && runner.calls.size() == 1);
    assert(runner.calls.front().argv.front() == "/usr/bin/g++");
    assert(runner.calls.front().timeout.count() > 0);
  }
  {
    Runner runner;
    runner.error_at = 1;
    auto result =
        amp::build::run_native_binary(request(), runner, files, verified);
    assert(!result && runner.calls.size() == 1);
    assert(result.error().message == "injected runner error");
  }
  {
    Runner runner;
    runner.timeout_at = 1;
    auto result =
        amp::build::run_native_binary(request(), runner, files, verified);
    assert(!result && runner.calls.size() == 1);
    assert(result.error().message.find("timed out") != std::string::npos);
  }
  for (const auto mode : {Files::Mode::read_error, Files::Mode::malformed}) {
    Runner runner;
    Files failed_files(mode);
    auto result = amp::build::run_native_binary(request(), runner, failed_files,
                                                verified);
    assert(!result && runner.calls.size() == 1);
  }
  {
    Runner runner;
    Files invalid_graph(Files::Mode::missing_provider);
    auto result = amp::build::run_native_binary(request(), runner,
                                                invalid_graph, verified);
    assert(!result && runner.calls.size() == 2);
    assert(result.error().message.find("missing module provider") !=
           std::string::npos);
  }
  {
    Runner runner;
    runner.fail_at = 3;
    auto result =
        amp::build::run_native_binary(request(), runner, files, verified);
    assert(!result && runner.calls.size() == 3);
    assert(runner.calls.back().argv[1] == "-std=c++26");
    assert(result.error().message.find("injected failure") !=
           std::string::npos);
  }
  {
    Runner runner;
    runner.error_at = 3;
    auto result =
        amp::build::run_native_binary(request(), runner, files, verified);
    assert(!result && runner.calls.size() == 3);
    assert(result.error().message == "injected runner error");
  }
  {
    Runner runner;
    runner.timeout_at = 3;
    auto result =
        amp::build::run_native_binary(request(), runner, files, verified);
    assert(!result && runner.calls.size() == 3);
    assert(result.error().message.find("timed out") != std::string::npos);
  }
  {
    Runner runner;
    int output_checks = 0;
    const auto missing_output =
        [&](const std::string &path) -> amp::core::Status {
      if (path.ends_with(".json"))
        return {};
      ++output_checks;
      amp::core::Diagnostic d;
      d.code = amp::core::ErrorCode::missing_resource;
      d.message = "injected absent output";
      return std::unexpected(std::move(d));
    };
    auto result =
        amp::build::run_native_binary(request(), runner, files, missing_output);
    assert(!result && output_checks == 1);
    assert(runner.calls.size() == 3); // two scans, then one compile only
    assert(runner.calls.back().argv.size() > 1);
    assert(runner.calls.back().argv[1] == "-std=c++26");
  }
  {
    Runner runner;
    const auto missing_executable =
        [](const std::string &path) -> amp::core::Status {
      if (!path.ends_with("/app"))
        return {};
      amp::core::Diagnostic d;
      d.code = amp::core::ErrorCode::missing_resource;
      d.message = "injected missing executable";
      return std::unexpected(std::move(d));
    };
    auto result = amp::build::run_native_binary(request(), runner, files,
                                                missing_executable);
    assert(!result && runner.calls.size() == 6);
    assert(runner.calls.back().argv[1] == "-o");
    assert(result.error().message == "injected missing executable");
  }
  {
    Runner runner;
    runner.fail_at = 6;
    auto result =
        amp::build::run_native_binary(request(), runner, files, verified);
    assert(!result && runner.calls.size() == 6);
    assert(result.error().message.find("injected failure") !=
           std::string::npos);
  }
}
