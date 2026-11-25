#include <cassert>
#include <chrono>
#include <cstddef>
#include <expected>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

import cap.toolchain.host;
import cap.core.process;
import cap.core.diagnostic;

namespace {
/// Scripted process adapter recording calls and returning queued replies.
struct Runner final : cap::core::IProcessRunner {
  /// Recorded process specifications.
  mutable std::vector<cap::core::ProcessSpec> calls;
  /// Scripted replies in process-call order.
  mutable std::vector<cap::core::Result<cap::core::ProcessResult>> replies;
  /// Record a call and return its corresponding scripted reply.
  /// @param[in] spec const cap::core::ProcessSpec& — Process request to
  /// capture.
  /// @return cap::core::Result<cap::core::ProcessResult> — Queued reply.
  /// @throws std::out_of_range — If reply count is shorter than call count;
  ///         allocation exceptions may escape while recording.
  cap::core::Result<cap::core::ProcessResult>
  run(const cap::core::ProcessSpec &spec) const override {
    calls.push_back(spec);
    return replies.at(calls.size() - 1);
  }
};
/// Create a successful process reply with owned output text.
/// @param[in] output std::string — Standard output to report.
/// @return cap::core::ProcessResult — Exit-zero response.
/// @throws std::exception — If storing output allocates and fails.
auto success(std::string output) {
  return cap::core::ProcessResult{0, std::move(output), {}, false};
}
/// Fixed explicit host tools and supported target for resolver tests.
const cap::toolchain::HostToolchain host{
    {"/home/linuxbrew/.linuxbrew/bin/gcc-16",
     "/home/linuxbrew/.linuxbrew/bin/g++-16", "/usr/bin/ar"},
    "x86_64-pc-linux-gnu"};
} // namespace

/// Test SDK resolution, ordered duplicate operands, errors, and a real link.
/// @param[in] argc int — Process argument count; test requires exactly two.
/// @param[in] argv char** — Process arguments; argv[1] is the output executable
///            path used by the real-link check.
/// @return int — Zero when assertions pass; assertion failure terminates.
/// @throws std::exception — Test setup and local process operations may throw.
int main(int argc, char **argv) {
  using cap::toolchain::resolve_system_libraries;
  static_assert(std::is_same_v<decltype(resolve_system_libraries(
                                   host, {}, std::declval<const Runner &>())),
                               cap::core::Result<std::vector<std::string>>>);
  Runner runner;
  runner.replies = {success("/lib/libm.so\n"), success("libz.so\n"),
                    success("/lib/libz.a\n"), success("/lib/libm.so\n")};
  auto result = resolve_system_libraries(host, {"m", "z", "m"}, runner);
  assert(result &&
         (*result == std::vector<std::string>{"/lib/libm.so", "/lib/libz.a",
                                              "/lib/libm.so"}));
  /// Expected driver query order, including .a fallback and repeated input.
  const std::vector<std::string> options = {"libm.so", "libz.so", "libz.a",
                                            "libm.so"};
  for (std::size_t i = 0; i < options.size(); ++i) {
    assert((runner.calls[i].argv ==
            std::vector<std::string>{host.tools.cpp_compiler,
                                     "-print-file-name=" + options[i]}));
    assert(runner.calls[i].env_allowlist.empty());
    assert(runner.calls[i].timeout.count() == 10000);
  }
  Runner empty;
  assert(resolve_system_libraries(host, {}, empty));
  for (const char *bad : {"", "-x", "a/b", "a b", "x\n", "a."})
    assert(!resolve_system_libraries(host, {"valid", bad}, empty));
  assert(!resolve_system_libraries(host, {std::string("x\0y", 3)}, empty));
  auto invalid = host;
  invalid.tools.cpp_compiler = "g++";
  assert(!resolve_system_libraries(invalid, {}, empty));
  invalid.tools.cpp_compiler = std::string("/g\0x", 4);
  assert(!resolve_system_libraries(invalid, {}, empty));
  invalid = host;
  invalid.host_triple = "aarch64-linux-gnu";
  assert(!resolve_system_libraries(invalid, {}, empty));
  assert(empty.calls.empty());

  Runner missing;
  missing.replies = {success("libx.so\n"), success("libx.a\n")};
  auto absent = resolve_system_libraries(host, {"x"}, missing);
  assert(!absent &&
         absent.error().code == cap::core::ErrorCode::missing_resource);
  for (const std::string &output :
       std::vector<std::string>{"", "\n", "/x", "/x\nmore\n", "relative\n",
                                "libother.so\n", std::string("/x\0y\n", 5)}) {
    Runner malformed;
    malformed.replies = {success(output)};
    assert(!resolve_system_libraries(host, {"x"}, malformed));
    assert(malformed.calls.size() == 1);
  }
  Runner timeout;
  timeout.replies = {cap::core::ProcessResult{-1, {}, {}, true}};
  auto timed = resolve_system_libraries(host, {"x"}, timeout);
  assert(!timed && timed.error().code == cap::core::ErrorCode::transient_io);
  Runner exit;
  exit.replies = {cap::core::ProcessResult{1, {}, {}, false}};
  assert(!resolve_system_libraries(host, {"x"}, exit));
  Runner failed;
  cap::core::Diagnostic error;
  error.code = cap::core::ErrorCode::missing_resource;
  error.message = "failed";
  failed.replies = {std::unexpected(error)};
  auto failure = resolve_system_libraries(host, {"x"}, failed);
  assert(!failure && failure.error().code == error.code);

  assert(argc == 2);
  cap::core::LocalProcessRunner local;
  auto probed = cap::toolchain::probe_host_toolchain(host.tools, local);
  assert(probed);
  auto real = resolve_system_libraries(*probed, {"m"}, local);
  assert(real && real->size() == 1);
  const std::string source =
      "#include <cmath>\nint main(int argc, char**) { "
      "volatile double x = argc; return std::cos(x) > 2; }\n";
  cap::core::ProcessSpec compile;
  compile.argv = {host.tools.cpp_compiler,
                  "-x",
                  "c++",
                  "-",
                  "-x",
                  "none",
                  (*real)[0],
                  "-o",
                  argv[1]};
  compile.env_allowlist = {"PATH"};
  for (char c : source)
    compile.stdin_bytes.push_back(static_cast<std::byte>(c));
  compile.timeout = std::chrono::seconds(10);
  auto linked = local.run(compile);
  assert(linked && !linked->timed_out && linked->exit_code == 0);
  cap::core::ProcessSpec execute;
  execute.argv = {argv[1]};
  execute.timeout = std::chrono::seconds(10);
  auto ran = local.run(execute);
  assert(ran && !ran->timed_out && ran->exit_code == 0);
}
