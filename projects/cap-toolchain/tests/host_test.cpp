#include <cassert>
#include <cstddef>
#include <expected>
#include <string>
#include <utility>
#include <vector>

import cap.toolchain.host;
import cap.core.process;
import cap.core.diagnostic;

namespace {
/// Scripted process adapter recording calls and returning queued replies.
struct Runner final : cap::core::IProcessRunner {
  /// Mutable call history required by the const interface method.
  mutable std::vector<cap::core::ProcessSpec> calls;
  /// Replies returned in call order.
  mutable std::vector<cap::core::Result<cap::core::ProcessResult>> replies;
  /// Index of the next queued reply.
  mutable std::size_t next = 0;

  /// Record a process specification and return its queued result.
  /// @param[in] spec const cap::core::ProcessSpec& — Process request to
  /// capture.
  /// @return cap::core::Result<cap::core::ProcessResult> — Next scripted reply.
  /// @throws std::out_of_range — If no reply remains; allocation exceptions
  ///         may escape while recording the call.
  cap::core::Result<cap::core::ProcessResult>
  run(const cap::core::ProcessSpec &spec) const override {
    calls.push_back(spec);
    return replies.at(next++);
  }
};

/// Explicit absolute tool paths used by deterministic probe tests.
cap::toolchain::HostTools tools{"/usr/bin/gcc", "/usr/bin/g++", "/usr/bin/ar"};
/// Create a successful scripted process result.
/// @param[in] out std::string — Standard output to own in the result.
/// @return cap::core::Result<cap::core::ProcessResult> — Exit-zero result.
/// @throws std::exception — If result storage allocation fails.
cap::core::Result<cap::core::ProcessResult> success(std::string out) {
  return cap::core::ProcessResult{0, std::move(out), {}, false};
}
/// Create a nonzero-exit scripted process result.
/// @param[in] code int — Exit status to report.
/// @param[in] err std::string — Optional standard error text.
/// @return cap::core::Result<cap::core::ProcessResult> — Result with status.
/// @throws std::exception — If result storage allocation fails.
cap::core::Result<cap::core::ProcessResult> exited(int code,
                                                   std::string err = {}) {
  return cap::core::ProcessResult{code, {}, std::move(err), false};
}
} // namespace

/// Exercise host probing contracts and run the explicit local GCC smoke.
/// @return int — Zero when all assertions pass; assertion failure terminates.
/// @throws std::exception — Test setup and local process operations may throw.
int main() {
  Runner runner;
  runner.replies = {success("x86_64-pc-linux-gnu\n"),
                    success("x86_64-pc-linux-gnu\n"), success("GNU ar\n")};
  auto probed = cap::toolchain::probe_host_toolchain(tools, runner);
  assert(probed && probed->host_triple == "x86_64-pc-linux-gnu");
  assert(runner.calls.size() == 3);
  assert((runner.calls[0].argv ==
          std::vector<std::string>{"/usr/bin/gcc", "-dumpmachine"}));
  assert((runner.calls[1].argv ==
          std::vector<std::string>{"/usr/bin/g++", "-dumpmachine"}));
  assert((runner.calls[2].argv ==
          std::vector<std::string>{"/usr/bin/ar", "--version"}));
  for (const auto &call : runner.calls) {
    assert(call.env_allowlist.empty());
    assert(call.timeout.count() == 10000);
  }

  Runner mismatched;
  mismatched.replies = {success("x86_64-linux-gnu\n"),
                        success("x86_64-pc-linux-gnu\n")};
  auto mismatch_result =
      cap::toolchain::probe_host_toolchain(tools, mismatched);
  assert(!mismatch_result);
  assert(mismatch_result.error().message.find("targets differ") !=
         std::string::npos);

  Runner malformed;
  malformed.replies = {success("x86_64-linux-gnu\nextra\n")};
  assert(!cap::toolchain::probe_host_toolchain(tools, malformed));

  Runner failed;
  cap::core::Diagnostic runner_error;
  runner_error.code = cap::core::ErrorCode::missing_resource;
  runner_error.message = "executable missing";
  failed.replies = {std::unexpected(runner_error)};
  auto failed_result = cap::toolchain::probe_host_toolchain(tools, failed);
  assert(!failed_result);
  assert(failed_result.error().code == cap::core::ErrorCode::missing_resource);
  assert(failed_result.error().message ==
         "C compiler target probe /usr/bin/gcc: executable missing");

  Runner timed_out;
  timed_out.replies = {cap::core::ProcessResult{-1, {}, {}, true}};
  auto timeout_result = cap::toolchain::probe_host_toolchain(tools, timed_out);
  assert(!timeout_result);
  assert(timeout_result.error().code == cap::core::ErrorCode::transient_io);
  assert(timeout_result.error().message.find("timed out") != std::string::npos);

  Runner archive_failed;
  archive_failed.replies = {success("x86_64-linux-gnu\n"),
                            success("x86_64-linux-gnu\n"),
                            exited(1, "missing ar")};
  assert(!cap::toolchain::probe_host_toolchain(tools, archive_failed));

  Runner archive_no_version;
  archive_no_version.replies = {success("x86_64-linux-gnu\n"),
                                success("x86_64-linux-gnu\n"), success("")};
  assert(!cap::toolchain::probe_host_toolchain(tools, archive_no_version));

  Runner invalid;
  assert(!cap::toolchain::probe_host_toolchain(
      {"gcc", "/usr/bin/g++", "/usr/bin/ar"}, invalid));
  assert(invalid.calls.empty());
  assert(!cap::toolchain::probe_host_toolchain(
      {"/usr/bin/gcc", "", "/usr/bin/ar"}, invalid));
  assert(!cap::toolchain::probe_host_toolchain(
      {"/usr/bin/gcc", "/usr/bin/g++", "relative/ar"}, invalid));
  assert(!cap::toolchain::probe_host_toolchain(
      {std::string("/usr/bin/gcc\0bad", 17), "/usr/bin/g++", "/usr/bin/ar"},
      invalid));
  assert(invalid.calls.empty());

  cap::core::LocalProcessRunner local;
  const cap::toolchain::HostTools local_tools{
      "/home/linuxbrew/.linuxbrew/bin/gcc-16",
      "/home/linuxbrew/.linuxbrew/bin/g++-16", "/usr/bin/ar"};
  auto real = cap::toolchain::probe_host_toolchain(local_tools, local);
  assert(real && real->host_triple == "x86_64-pc-linux-gnu");
}
