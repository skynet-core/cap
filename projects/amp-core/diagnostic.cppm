// amp-core foundational diagnostics (M0 slice).
// Stable error taxonomy and canonical single-line encoding; see CONTRACTS.md.
module;

#include <cstddef>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>
#include <vector>

export module amp.core.diagnostic;

export namespace amp::core {

enum class Severity : std::uint8_t {
  info = 0,
  warning = 1,
  error = 2,
  fatal = 3
};

// Stable error taxonomy. Hyphenated names are part of the persisted contract;
// never reuse a retired name.
enum class ErrorCode : std::uint8_t {
  invalid_input = 1,
  missing_resource = 2,
  unsupported_capability = 3,
  integrity_failure = 4,
  cancellation = 5,
  transient_io = 6,
  internal = 7,
};

constexpr std::string_view severity_name(Severity severity) {
  switch (severity) {
  case Severity::info:
    return "info";
  case Severity::warning:
    return "warning";
  case Severity::error:
    return "error";
  case Severity::fatal:
    return "fatal";
  }
  return "unknown";
}

constexpr std::string_view error_code_name(ErrorCode code) {
  switch (code) {
  case ErrorCode::invalid_input:
    return "invalid-input";
  case ErrorCode::missing_resource:
    return "missing-resource";
  case ErrorCode::unsupported_capability:
    return "unsupported-capability";
  case ErrorCode::integrity_failure:
    return "integrity-failure";
  case ErrorCode::cancellation:
    return "cancellation";
  case ErrorCode::transient_io:
    return "transient-io";
  case ErrorCode::internal:
    return "internal";
  }
  return "unknown";
}

// Owned source location; zero means unspecified.
struct Span {
  std::string file;
  int line = 0;
  int column = 0;
};

// Structured diagnostic with causal chain. Value type; causes are owned.
struct Diagnostic {
  ErrorCode code = ErrorCode::internal;
  Severity severity = Severity::error;
  std::string message;
  Span span;
  std::vector<Diagnostic> causes; // first entry is the root cause
  std::string remediation;

  // Canonical form:
  //   {severity} code={name}[ file={path}:{line}:{column}] message={text}
  //                  [ causes={name,...}][ fix={text}]
  // Optional segments appear only when present; output is deterministic.
  std::string format() const;
};

namespace detail {
inline void append_field(std::string &out, std::string_view field) {
  constexpr char hex[] = "0123456789ABCDEF";
  for (const unsigned char byte : field) {
    switch (byte) {
    case '\\':
      out += "\\\\";
      break;
    case '\0':
      out += "\\0";
      break;
    case '\a':
      out += "\\a";
      break;
    case '\b':
      out += "\\b";
      break;
    case '\t':
      out += "\\t";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\v':
      out += "\\v";
      break;
    case '\f':
      out += "\\f";
      break;
    case '\r':
      out += "\\r";
      break;
    default:
      if (byte < 0x20 || byte == 0x7f) {
        out += "\\x";
        out += hex[byte >> 4];
        out += hex[byte & 0x0f];
      } else {
        out += static_cast<char>(byte);
      }
    }
  }
}

inline std::string format_diagnostic(const Diagnostic &diagnostic) {
  std::string out;
  out.reserve(diagnostic.message.size() + 64);
  out += severity_name(diagnostic.severity);
  out += " code=";
  out += error_code_name(diagnostic.code);
  if (!diagnostic.span.file.empty()) {
    out += " file=";
    append_field(out, diagnostic.span.file);
    out += ':';
    out += std::to_string(diagnostic.span.line);
    out += ':';
    out += std::to_string(diagnostic.span.column);
  }
  out += " message=";
  append_field(out, diagnostic.message);
  if (!diagnostic.causes.empty()) {
    out += " causes=";
    for (std::size_t i = 0; i < diagnostic.causes.size(); ++i) {
      if (i != 0)
        out += ',';
      out += error_code_name(diagnostic.causes[i].code);
    }
  }
  if (!diagnostic.remediation.empty()) {
    out += " fix=";
    append_field(out, diagnostic.remediation);
  }
  return out;
}
} // namespace detail

inline std::string Diagnostic::format() const {
  return detail::format_diagnostic(*this);
}

template <class T> using Result = std::expected<T, Diagnostic>;
using Status = std::expected<void, Diagnostic>;
} // namespace amp::core
