module;

#include <expected>
#include <string>
#include <vector>

export module cap.compdb;

import cap.build.actions;
import cap.core.diagnostic;

export namespace cap::compdb {
/// Serializes the compile-action projection as a deterministic JSON array.
/// @param[in] actions const std::vector<cap::build::Action>& — Borrowed
/// read-only actions; the vector and its contents need remain valid only
/// through this call and are not retained.
/// @return cap::core::Result<std::string> — The JSON bytes on success;
/// `invalid_input` when a compile action lacks a primary value, when its
/// directory, primary input, or primary output is empty or not an absolute
/// path, or when any serialized path or argument contains NUL or malformed
/// UTF-8. argv[0] must additionally be nonempty and absolute; later arguments
/// may be empty or relative. Non-compile actions are ignored and an empty
/// projection yields `[]`.
/// @throws std::bad_alloc — If building the result or a diagnostic exhausts
/// available memory.
/// @throws std::length_error — If an output string operation exceeds its
/// maximum representable size.
/// @note Preserves action and argument order, including empty arguments after
/// argv[0]. Performs no file I/O and does not regenerate compiler arguments.
auto to_json(const std::vector<cap::build::Action> &actions)
    -> cap::core::Result<std::string>;
} // namespace cap::compdb

namespace cap::compdb {
namespace {
/// Validates bytes as UTF-8, rejecting overlong, surrogate, truncated, and
/// out-of-range encodings.
/// @param[in] value const std::string& — Borrowed bytes to validate.
/// @return bool — True exactly when `value` is well-formed UTF-8, including
/// the empty byte sequence.
/// @note Does not throw.
bool valid_utf8(const std::string &value);

/// Checks the lexical path requirements for serialized primary paths.
/// @param[in] path const std::string& — Borrowed path bytes to validate.
/// @return bool — True when nonempty, absolute (leading `/`), NUL-free, and
/// valid UTF-8; this does not check filesystem existence or normalization.
/// @note Does not throw.
bool valid_path(const std::string &path) {
  return !path.empty() && path.front() == '/' &&
         path.find('\0') == std::string::npos && valid_utf8(path);
}

bool valid_utf8(const std::string &value) {
  for (std::size_t i = 0; i < value.size();) {
    const auto lead = static_cast<unsigned char>(value[i]);
    if (lead <= 0x7f) {
      ++i;
      continue;
    }
    std::size_t count;
    unsigned char second_min = 0x80, second_max = 0xbf;
    if (lead >= 0xc2 && lead <= 0xdf) {
      count = 2;
    } else if (lead >= 0xe0 && lead <= 0xef) {
      count = 3;
      if (lead == 0xe0)
        second_min = 0xa0;
      if (lead == 0xed)
        second_max = 0x9f;
    } else if (lead >= 0xf0 && lead <= 0xf4) {
      count = 4;
      if (lead == 0xf0)
        second_min = 0x90;
      if (lead == 0xf4)
        second_max = 0x8f;
    } else {
      return false;
    }
    if (i + count > value.size())
      return false;
    const auto second = static_cast<unsigned char>(value[i + 1]);
    if (second < second_min || second > second_max)
      return false;
    for (std::size_t j = 2; j < count; ++j) {
      const auto continuation = static_cast<unsigned char>(value[i + j]);
      if (continuation < 0x80 || continuation > 0xbf)
        return false;
    }
    i += count;
  }
  return true;
}

/// Appends one JSON string literal to an existing output buffer.
/// @param[in,out] out std::string& — Owned by the caller; receives an opening
/// quote, escaped `value` bytes, and a closing quote. Existing contents remain
/// as a prefix.
/// @param[in] value const std::string& — Borrowed UTF-8 string bytes to encode;
/// control bytes are escaped, while other bytes are copied unchanged.
/// @return void — Appends to `out`; no separate value is returned.
/// @throws std::bad_alloc — If appending any encoded byte exhausts memory.
/// @throws std::length_error — If an append would exceed `out.max_size()`.
/// @pre `value` is valid UTF-8 and contains no NUL, as guaranteed by the
/// caller's validation.
/// @pre `out` and `value` do not refer to overlapping storage.
/// @note If an append throws, `out` may retain a partial suffix; no rollback
/// to its original size is provided.
void quote(std::string &out, const std::string &value) {
  /// Hexadecimal digits used for JSON `\u00XX` escapes, indexed by nibble.
  constexpr char hex[] = "0123456789abcdef";
  out += '"';
  for (const unsigned char c : value) {
    switch (c) {
    case '"':
      out += "\\\"";
      break;
    case '\\':
      out += "\\\\";
      break;
    case '\b':
      out += "\\b";
      break;
    case '\f':
      out += "\\f";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      if (c < 0x20) {
        out += "\\u00";
        out += hex[c >> 4];
        out += hex[c & 0xf];
      } else {
        out += static_cast<char>(c);
      }
    }
  }
  out += '"';
}

/// Creates the common malformed-projection diagnostic result.
/// @return cap::core::Result<std::string> — An unexpected diagnostic with
/// code `invalid_input` and message `malformed compile action projection`.
/// @throws std::bad_alloc — If constructing the diagnostic message exhausts
/// available memory.
/// @throws std::length_error — If constructing its string exceeds the
/// implementation's maximum string size.
auto invalid() -> cap::core::Result<std::string> {
  cap::core::Diagnostic error;
  error.code = cap::core::ErrorCode::invalid_input;
  error.message = "malformed compile action projection";
  return std::unexpected(std::move(error));
}
} // namespace

auto to_json(const std::vector<cap::build::Action> &actions)
    -> cap::core::Result<std::string> {
  std::string out = "[";
  bool first = true;
  for (const auto &action : actions) {
    if (action.kind != cap::build::ActionKind::compile)
      continue;
    if (!valid_path(action.directory) || action.inputs.empty() ||
        !valid_path(action.inputs.front()) || action.outputs.empty() ||
        !valid_path(action.outputs.front()) || action.arguments.empty() ||
        !valid_path(action.arguments.front()))
      return invalid();
    for (const auto &arg : action.arguments)
      if (arg.find('\0') != std::string::npos || !valid_utf8(arg))
        return invalid();
    if (!first)
      out += ',';
    first = false;
    out += "{\"directory\":";
    quote(out, action.directory);
    out += ",\"file\":";
    quote(out, action.inputs.front());
    out += ",\"arguments\":[";
    for (std::size_t i = 0; i < action.arguments.size(); ++i) {
      if (i)
        out += ',';
      quote(out, action.arguments[i]);
    }
    out += "],\"output\":";
    quote(out, action.outputs.front());
    out += '}';
  }
  out += ']';
  return out;
}
} // namespace cap::compdb
