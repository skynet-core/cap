module;

#include <chrono>
#include <expected>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module cap.toolchain.host;

import cap.core.diagnostic;
import cap.core.process;

export namespace cap::toolchain {

/// Owned paths to caller-selected host executables.
/// The caller supplies trusted absolute paths; this value does not establish
/// executable identity or retain a process-adapter lifetime.
struct HostTools {
  /// Trusted absolute path to the C compiler executable.
  std::string c_compiler;
  /// Trusted absolute path to the C++ compiler executable.
  std::string cpp_compiler;
  /// Trusted absolute path to the archiver executable.
  std::string archiver;
};

/// Host tools whose C and C++ drivers reported the same supported triple.
/// The strings are owned copies; this value does not authenticate the tools.
struct HostToolchain {
  /// Owned executable paths supplied for probing.
  HostTools tools;
  /// Exact supported target string reported by both compiler drivers.
  std::string host_triple;
};

/// Probe explicit host tools without filesystem or network discovery.
/// @param[in] tools const HostTools& — Trusted absolute, NUL-free executable
///            paths, copied into the result on success.
/// @param[in] runner const cap::core::IProcessRunner& — Borrowed adapter,
///            which must outlive the call; it is not retained.
/// @return cap::core::Result<HostToolchain> — Validated tools and common
///         supported target, or a structured diagnostic. All paths are checked
///         before any process invocation.
/// @throws std::bad_alloc — If owned strings or diagnostics cannot allocate.
/// @throws std::length_error — If an owned string exceeds its maximum size.
/// @throws Any exception from IProcessRunner::run — If the injected adapter
///         throws; adapter exceptions are not caught.
cap::core::Result<HostToolchain>
probe_host_toolchain(const HostTools &tools,
                     const cap::core::IProcessRunner &runner);

/// Resolve ordered logical SDK library names using the selected C++ driver.
/// @param[in] host const HostToolchain& — Host with selected trusted C++
///            driver and supported triple; neither is re-probed here.
/// @param[in] names const std::vector<std::string>& — Ordered names matching
///            ASCII `[A-Za-z0-9_+][A-Za-z0-9_+-]*`; hyphens may occur anywhere
///            except first. Empty is valid after host/driver validation and
///            causes no process invocation.
/// @param[in] runner const cap::core::IProcessRunner& — Borrowed adapter,
///            which must outlive the call and is not retained.
/// @return cap::core::Result<std::vector<std::string>> — Absolute operands in
///         input order, preserving duplicates. `.so` is queried before `.a`;
///         `.a` is queried only if `.so` echoes its candidate. Driver output
///         acceptance does not verify file existence, inspect a linker script,
///         or execute a library. Failure returns no partial vector.
/// @throws std::bad_alloc — If owned strings, vectors, or diagnostics cannot
///         allocate.
/// @throws std::length_error — If owned strings or vectors exceed maximum size.
/// @throws Any exception from IProcessRunner::run — If the injected adapter
///         throws; adapter exceptions are not caught.
cap::core::Result<std::vector<std::string>>
resolve_system_libraries(const HostToolchain &host,
                         const std::vector<std::string> &names,
                         const cap::core::IProcessRunner &runner);

} // namespace cap::toolchain

namespace cap::toolchain {
namespace {
using cap::core::Diagnostic;
using cap::core::ErrorCode;
using cap::core::ProcessResult;
using cap::core::ProcessSpec;

/// Construct a diagnostic with the supplied code and owned message.
/// @param[in] code ErrorCode — Structured failure classification.
/// @param[in] message std::string — Diagnostic text, moved into the result.
/// @return Diagnostic — Owned diagnostic value.
/// @throws std::bad_alloc — If storing the owned message cannot allocate.
/// @throws std::length_error — If the message exceeds the string maximum size.
Diagnostic diagnostic(ErrorCode code, std::string message) {
  Diagnostic value;
  value.code = code;
  value.message = std::move(message);
  return value;
}

/// Run one explicit executable with bounded, empty-allowlist process settings.
/// @param[in] runner const cap::core::IProcessRunner& — Borrowed process
/// adapter.
/// @param[in] path const std::string& — Absolute, NUL-free executable path.
/// @param[in] args std::vector<std::string> — Arguments excluding argv[0].
/// @param[in] operation const char* — Static diagnostic operation label.
/// @return cap::core::Result<ProcessResult> — Process result or contextual
///         validation, runner, timeout, or nonzero-exit diagnostic.
/// @throws std::bad_alloc — If argv or diagnostic construction cannot allocate.
/// @throws std::length_error — If argv or diagnostic strings exceed maximum
/// size.
/// @throws Any exception from IProcessRunner::run — If the adapter throws.
cap::core::Result<ProcessResult> run(const cap::core::IProcessRunner &runner,
                                     const std::string &path,
                                     std::vector<std::string> args,
                                     const char *operation) {
  if (path.empty() || path.front() != '/' ||
      path.find('\0') != std::string::npos)
    return std::unexpected(diagnostic(
        ErrorCode::invalid_input,
        std::string(operation) + " executable must be an absolute path"));
  args.insert(args.begin(), path);
  ProcessSpec spec;
  spec.argv = std::move(args);
  spec.timeout = std::chrono::seconds(10);
  auto result = runner.run(spec);
  if (!result) {
    auto error = result.error();
    error.message = std::string(operation) + " " + path + ": " + error.message;
    return std::unexpected(std::move(error));
  }
  if (result->timed_out || result->exit_code != 0) {
    return std::unexpected(diagnostic(
        result->timed_out ? ErrorCode::transient_io
                          : ErrorCode::unsupported_capability,
        std::string(operation) + " " + path +
            (result->timed_out ? " timed out"
                               : " failed with exit code " +
                                     std::to_string(result->exit_code))));
  }
  return result;
}

/// Check whether a target exactly matches a supported local Linux triple.
/// @param[in] triple std::string_view — Target triple to compare.
/// @return bool — True for either supported x86-64 Linux spelling.
/// @note Does not throw.
bool supported(std::string_view triple) {
  return triple == "x86_64-linux-gnu" || triple == "x86_64-pc-linux-gnu";
}

/// Query and validate one compiler's single-line target output.
/// @param[in] runner const cap::core::IProcessRunner& — Borrowed process
/// adapter.
/// @param[in] compiler const std::string& — Explicit compiler executable.
/// @param[in] label const char* — Static diagnostic label.
/// @return cap::core::Result<std::string> — Supported target or a diagnostic.
/// @throws std::bad_alloc — If output or diagnostic strings cannot allocate.
/// @throws std::length_error — If output or diagnostic strings exceed maximum
/// size.
/// @throws Any exception from IProcessRunner::run — If the adapter throws.
cap::core::Result<std::string> target(const cap::core::IProcessRunner &runner,
                                      const std::string &compiler,
                                      const char *label) {
  auto result = run(runner, compiler, {"-dumpmachine"}, label);
  if (!result)
    return std::unexpected(result.error());
  const auto &output = result->stdout_text;
  if (output.size() < 2 || output.back() != '\n' ||
      output.find('\n') != output.size() - 1 ||
      output.find('\r') != std::string::npos)
    return std::unexpected(
        diagnostic(ErrorCode::unsupported_capability,
                   std::string(label) + " " + compiler +
                       " returned non-single-line target output"));
  std::string value = output.substr(0, output.size() - 1);
  if (!supported(value))
    return std::unexpected(diagnostic(ErrorCode::unsupported_capability,
                                      std::string(label) + " " + compiler +
                                          " reports unsupported host target " +
                                          value));
  return value;
}
} // namespace

cap::core::Result<HostToolchain>
probe_host_toolchain(const HostTools &tools,
                     const cap::core::IProcessRunner &runner) {
  for (const auto &[path, label] :
       {std::pair{&tools.c_compiler, "C compiler"},
        std::pair{&tools.cpp_compiler, "C++ compiler"},
        std::pair{&tools.archiver, "archiver"}}) {
    if (path->empty() || path->front() != '/' ||
        path->find('\0') != std::string::npos)
      return std::unexpected(diagnostic(
          ErrorCode::invalid_input,
          std::string(label) + " executable must be an absolute path"));
  }
  auto c = target(runner, tools.c_compiler, "C compiler target probe");
  if (!c)
    return std::unexpected(c.error());
  auto cpp = target(runner, tools.cpp_compiler, "C++ compiler target probe");
  if (!cpp)
    return std::unexpected(cpp.error());
  if (*c != *cpp)
    return std::unexpected(diagnostic(
        ErrorCode::unsupported_capability,
        "C and C++ compiler host targets differ: " + *c + " vs " + *cpp));
  auto ar =
      run(runner, tools.archiver, {"--version"}, "archiver version check");
  if (!ar)
    return std::unexpected(ar.error());
  if (ar->stdout_text.empty() && ar->stderr_text.empty())
    return std::unexpected(diagnostic(ErrorCode::unsupported_capability,
                                      "archiver " + tools.archiver +
                                          " returned no version output"));
  return HostToolchain{tools, *c};
}

cap::core::Result<std::vector<std::string>>
resolve_system_libraries(const HostToolchain &host,
                         const std::vector<std::string> &names,
                         const cap::core::IProcessRunner &runner) {
  const auto &driver = host.tools.cpp_compiler;
  if (driver.empty() || driver.front() != '/' ||
      driver.find('\0') != std::string::npos)
    return std::unexpected(diagnostic(ErrorCode::invalid_input,
                                      "C++ compiler must be an absolute path"));
  if (!supported(host.host_triple))
    return std::unexpected(
        diagnostic(ErrorCode::unsupported_capability,
                   "unsupported host target " + host.host_triple));
  /// Accept characters allowed after a name's required non-hyphen prefix.
  const auto valid_char = [](char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '+';
  };
  for (const auto &name : names) {
    if (name.empty() || !valid_char(name.front()))
      return std::unexpected(
          diagnostic(ErrorCode::invalid_input, "invalid system library name"));
    for (char c : name)
      if (!valid_char(c) && c != '-')
        return std::unexpected(diagnostic(ErrorCode::invalid_input,
                                          "invalid system library name"));
  }
  std::vector<std::string> paths;
  for (const auto &name : names) {
    bool found = false;
    for (const char *suffix : {".so", ".a"}) {
      const auto candidate = "lib" + name + suffix;
      auto result = run(runner, driver, {"-print-file-name=" + candidate},
                        "system library lookup");
      if (!result)
        return std::unexpected(result.error());
      const auto &output = result->stdout_text;
      if (output.size() < 2 || output.back() != '\n' ||
          output.find('\n') != output.size() - 1 ||
          output.find('\0') != std::string::npos)
        return std::unexpected(
            diagnostic(ErrorCode::unsupported_capability,
                       "malformed lookup output for " + candidate));
      auto value = output.substr(0, output.size() - 1);
      if (value == candidate)
        continue;
      if (value.front() != '/')
        return std::unexpected(
            diagnostic(ErrorCode::unsupported_capability,
                       "malformed lookup output for " + candidate));
      paths.push_back(std::move(value));
      found = true;
      break;
    }
    if (!found)
      return std::unexpected(diagnostic(ErrorCode::missing_resource,
                                        "system library not found: " + name));
  }
  return paths;
}
} // namespace cap::toolchain
