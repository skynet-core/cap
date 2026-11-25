module;
#include <algorithm>
#include <cstddef>
#include <expected>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module cap.manifest;
import cap.core.diagnostic;

export namespace cap::manifest {
/// Owned package metadata parsed from a `[package]` table.
struct Package {
  /// Package name; must satisfy the reader's ASCII name grammar.
  std::string name, version;
  /// Optional license string, absent when the key is omitted.
  std::optional<std::string> license;
  /// Optional publication flag, absent when the key is omitted.
  std::optional<bool> publish;
};
/// Optional language-standard selections from `[language]`.
struct Language {
  /// Optional C language standard spelling.
  std::optional<std::string> c, cpp;
};
/// One ordered `[[bin]]` target and its declared paths and dependencies.
struct Binary {
  /// Unique target name and required entry path, preserved verbatim.
  std::string name, entry;
  /// Ordered source patterns, exclusions, include directories, binary
  /// dependencies, and logical system-library names.
  std::vector<std::string> sources, exclude, include_dirs, depends_on,
      system_libraries;
};
/// Static library declaration and its ordered paths and link requirements.
struct Library {
  /// Library name and type; type defaults to and currently only accepts
  /// `"static"`.
  std::string name, type = "static";
  /// Ordered source paths, header paths, public include directories, and
  /// logical system-library names.
  std::vector<std::string> sources, headers, public_include_dirs,
      system_libraries;
};
/// Workspace membership and default-selection metadata.
struct Workspace {
  /// Ordered workspace members, default members, and exclusions.
  std::vector<std::string> members, default_members, exclude;
  /// Optional resolver spelling, retained without interpretation.
  std::optional<std::string> resolver;
};
/// A named local path dependency, in manifest declaration order.
struct PathDependency {
  /// Dependency name and declared relative path, preserved verbatim.
  std::string name, path;
};
/// Fully owned result of parsing the supported manifest subset.
struct Manifest {
  /// Optional package metadata.
  std::optional<Package> package;
  /// Language settings, empty when no language keys are declared.
  Language language;
  /// Optional static library declaration.
  std::optional<Library> library;
  /// Optional workspace declaration.
  std::optional<Workspace> workspace;
  /// Binary targets in source declaration order.
  std::vector<Binary> bins;
  /// Local path dependencies in source declaration order.
  std::vector<PathDependency> dependencies;
};
/// Parses manifest bytes without filesystem or process access.
/// @param[in] bytes std::string_view — Borrowed manifest text, valid for this
/// call; only the documented cap manifest subset is accepted.
/// @param[in] source std::string_view — Borrowed source identity copied into
/// any diagnostic.
/// @return core::Result<Manifest> — Owned parsed values, or an
/// invalid_input diagnostic with source and one-based line and column.
/// @throws std::bad_alloc, std::length_error — Allocation or container/string
/// size failure; these are not converted to diagnostics.
core::Result<Manifest> parse_manifest(std::string_view bytes,
                                      std::string_view source);
} // namespace cap::manifest

namespace cap::manifest {
namespace {
/// Stateful cursor and semantic validator for the supported manifest subset.
class Reader {
  /// Borrowed manifest text; remains valid for the Reader's lifetime.
  std::string_view text, source;
  /// Current byte offset used for parsing and diagnostic locations.
  std::size_t pos = 0;
  /// First package-scoped table offset, used to locate orphaned package data.
  std::optional<std::size_t> package_section;
  /// Owned result accumulated in declaration order.
  Manifest result;
  /// Current table name, empty for root keys.
  std::string section;
  /// Names already declared at document, field, and binary-target scopes.
  std::set<std::string> tables, fields, bin_names;
  /// Whether the required root `manifest-version=1` was read.
  bool version = false;

  /// Throws an invalid-input diagnostic at the current byte position.
  /// @param[in] message std::string — Diagnostic message, moved into the
  /// exception.
  /// @return void — No value; always throws core::Diagnostic.
  /// @throws core::Diagnostic — Always, with invalid_input and the supplied
  /// source plus one-based line and column.
  /// @throws std::bad_alloc, std::length_error — While constructing the
  /// location or diagnostic.
  [[noreturn]] void fail(std::string message) const {
    int line = 1, column = 1;
    for (std::size_t i = 0; i < pos && i < text.size(); ++i) {
      if (text[i] == '\n') {
        ++line;
        column = 1;
      } else {
        ++column;
      }
    }
    throw core::Diagnostic{core::ErrorCode::invalid_input,
                           core::Severity::error,
                           std::move(message),
                           {std::string(source), line, column},
                           {},
                           {}};
  }
  /// Reports whether the cursor has consumed all input.
  /// @return bool — True exactly when pos equals text.size(); does not throw.
  bool end() const { return pos == text.size(); }
  /// Returns the current byte or NUL at end of input.
  /// @return char — Current byte, or '\0' when end() is true; does not throw.
  char peek() const { return end() ? '\0' : text[pos]; }
  /// Consumes the requested byte only when it is present.
  /// @param[in] c char — Byte to match.
  /// @return bool — True and advances pos on a match; otherwise false without
  /// advancing; does not throw.
  bool take(char c) {
    if (peek() != c || end())
      return false;
    ++pos;
    return true;
  }
  /// Requires and consumes one byte.
  /// @param[in] c char — Required byte.
  /// @return void — No value; advances pos on success.
  /// @throws core::Diagnostic — invalid_input when the byte is absent.
  /// @throws std::bad_alloc, std::length_error — While constructing a failure
  /// message or diagnostic.
  void expect(char c) {
    if (!take(c))
      fail(std::string("expected '") + c + "'");
  }
  /// Consumes horizontal spaces and tabs.
  /// @return void — No value; does not throw.
  void space() {
    while (peek() == ' ' || peek() == '\t')
      ++pos;
  }
  /// Consumes a comment from `#` through but not including its line ending.
  /// @return void — No value; does not throw.
  void comment() {
    if (take('#'))
      while (!end() && peek() != '\n' && peek() != '\r')
        ++pos;
  }
  /// Consumes LF or CRLF, diagnosing a bare CR.
  /// @return bool — True when a line ending was consumed, otherwise false.
  /// @throws core::Diagnostic — invalid_input for CR not followed by LF.
  /// @throws std::bad_alloc, std::length_error — While creating a diagnostic.
  bool newline() {
    if (take('\n'))
      return true;
    if (take('\r')) {
      expect('\n');
      return true;
    }
    return false;
  }
  /// Consumes whitespace, comments, and line endings inside arrays.
  /// @return void — No value; does not throw.
  void array_space() {
    for (;;) {
      space();
      comment();
      if (!newline())
        break;
    }
  }
  /// Tests whether a byte belongs to the accepted bare-key/name alphabet.
  /// @param[in] c char — Byte to test.
  /// @return bool — True for ASCII letters, digits, underscore, or hyphen.
  /// Does not throw.
  static bool keychar(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-';
  }
  /// Reads a nonempty bare key from the current position.
  /// @return std::string — Owned key text.
  /// @throws core::Diagnostic — invalid_input with `expected bare key` when
  /// no key byte is present.
  /// @throws std::bad_alloc, std::length_error — While owning the key or
  /// constructing a diagnostic.
  std::string key() {
    auto start = pos;
    while (keychar(peek()) && !end())
      ++pos;
    if (start == pos)
      fail("expected bare key");
    return std::string(text.substr(start, pos - start));
  }
  /// Reads a double-quoted single-line string with the supported escapes.
  /// @return std::string — Owned decoded string.
  /// @throws core::Diagnostic — invalid_input for missing quote, unfinished or
  /// unsupported escape, or control byte; message identifies the failure.
  /// @throws std::bad_alloc, std::length_error — While building the result or
  /// diagnostic.
  std::string string() {
    expect('"');
    std::string out;
    while (!end() && peek() != '"') {
      char c = text[pos++];
      if (c == '\\') {
        if (end())
          fail("unfinished escape");
        switch (text[pos++]) {
        case '"':
          out += '"';
          break;
        case '\\':
          out += '\\';
          break;
        case 'n':
          out += '\n';
          break;
        case 'r':
          out += '\r';
          break;
        case 't':
          out += '\t';
          break;
        case 'b':
          out += '\b';
          break;
        case 'f':
          out += '\f';
          break;
        default:
          fail("unsupported escape");
        }
      } else {
        if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f)
          fail("control byte in string");
        out += c;
      }
    }
    expect('"');
    return out;
  }
  /// Reads an array of strings, allowing comments and multiline whitespace.
  /// @return std::vector<std::string> — Owned strings in source order.
  /// @throws core::Diagnostic — invalid_input for malformed delimiters or
  /// invalid array elements, with the parser's specific message.
  /// @throws std::bad_alloc, std::length_error — While growing values or
  /// constructing a diagnostic.
  std::vector<std::string> array() {
    std::vector<std::string> out;
    expect('[');
    array_space();
    while (!take(']')) {
      out.push_back(string());
      array_space();
      if (take(']'))
        break;
      expect(',');
      array_space();
    }
    return out;
  }
  /// Reads a schema boolean literal.
  /// @return bool — The parsed true or false value.
  /// @throws core::Diagnostic — invalid_input with `expected boolean` when
  /// neither literal begins at pos.
  bool boolean() {
    if (text.substr(pos, 4) == "true") {
      pos += 4;
      return true;
    }
    if (text.substr(pos, 5) == "false") {
      pos += 5;
      return false;
    }
    fail("expected boolean");
  }
  /// Validates a package, target, or system-library identifier.
  /// @param[in] s const std::string& — Name to validate.
  /// @return void — No value; does not modify s.
  /// @throws core::Diagnostic — invalid_input with `invalid name` unless the
  /// name begins with an ASCII letter/digit and contains only letters,
  /// digits, underscore, or hyphen.
  /// @throws std::bad_alloc, std::length_error — While constructing a
  /// diagnostic.
  void name(const std::string &s) {
    if (s.empty() || !keychar(s.front()) || s.front() == '-' ||
        s.front() == '_' || !std::all_of(s.begin(), s.end(), keychar))
      fail("invalid name");
  }
  /// Requires a semantic condition to hold.
  /// @param[in] condition bool — Whether the required condition holds.
  /// @param[in] what const char* — Text appended to the missing-field error.
  /// @return void — No value.
  /// @throws core::Diagnostic — invalid_input with `missing ` plus what when
  /// condition is false.
  /// @throws std::bad_alloc, std::length_error — While constructing the
  /// failure message or diagnostic.
  void required(bool condition, const char *what) {
    if (!condition)
      fail(std::string("missing ") + what);
  }
  /// Validates required fields and values in the current table.
  /// @return void — No value; leaves the accumulated result unchanged except
  /// for validation bookkeeping.
  /// @throws core::Diagnostic — invalid_input for missing/invalid required
  /// values, unsupported library type, or duplicate binary name.
  /// @throws std::bad_alloc, std::length_error — From validation bookkeeping
  /// or diagnostic construction.
  void finish_section() {
    if (section == "package") {
      required(fields.contains("name") && fields.contains("version"),
               "package name or version");
      name(result.package->name);
      // shortcut: version syntax is only checked for emptiness, upgrade when
      // semver policy is frozen.
      if (result.package->version.empty())
        fail("empty package version");
    } else if (section == "lib") {
      required(fields.contains("name") && fields.contains("sources"),
               "library name or sources");
      name(result.library->name);
      if (result.library->type != "static")
        fail("unsupported library type");
      for (const auto &item : result.library->system_libraries)
        name(item);
    } else if (section == "workspace") {
      required(fields.contains("members"), "workspace members");
    } else if (section == "bin") {
      required(fields.contains("name") && fields.contains("entry"),
               "binary name or entry");
      auto &b = result.bins.back();
      name(b.name);
      if (b.entry.empty())
        fail("empty binary entry");
      for (const auto &item : b.system_libraries)
        name(item);
      if (!bin_names.insert(b.name).second)
        fail("duplicate binary name");
    }
  }
  /// Parses a table header and initializes its corresponding result value.
  /// @return void — No value; advances the cursor and resets current fields.
  /// @throws core::Diagnostic — invalid_input for malformed, unknown,
  /// unsupported, or duplicate tables.
  /// @throws std::bad_alloc, std::length_error — While storing table names,
  /// section state, result values, or diagnostics.
  void header() {
    finish_section();
    auto start = pos;
    expect('[');
    bool bin = take('[');
    auto s = key();
    expect(']');
    if (bin)
      expect(']');
    if (bin && s != "bin")
      fail("unsupported array table");
    if (!bin && s != "package" && s != "language" && s != "workspace" &&
        s != "dependencies" && s != "lib")
      fail("unknown table");
    if (!bin && !tables.insert(s).second)
      fail("duplicate table");
    if ((bin || s == "lib" || s == "dependencies" || s == "language") &&
        !package_section)
      package_section = start;
    section = s;
    fields.clear();
    if (s == "package")
      result.package.emplace();
    else if (s == "workspace")
      result.workspace.emplace();
    else if (s == "lib")
      result.library.emplace();
    else if (s == "bin")
      result.bins.emplace_back();
  }
  /// Parses and assigns one root or table field, rejecting unsupported keys.
  /// @return void — No value; advances the cursor and updates the owned result.
  /// @throws core::Diagnostic — invalid_input for duplicate keys, malformed
  /// values, invalid names, wrong types, or unknown keys.
  /// @throws std::bad_alloc, std::length_error — While parsing or storing
  /// strings, arrays, dependencies, and diagnostics.
  void field() {
    auto k = key();
    if (!fields.insert(k).second)
      fail("duplicate key");
    space();
    expect('=');
    space();
    if (section.empty()) {
      if (k != "manifest-version")
        fail("unknown root key");
      expect('1');
      version = true;
    } else if (section == "package") {
      auto &p = *result.package;
      if (k == "name")
        p.name = string();
      else if (k == "version")
        p.version = string();
      else if (k == "license")
        p.license = string();
      else if (k == "publish")
        p.publish = boolean();
      else
        fail("unknown package key");
    } else if (section == "language") {
      if (k == "c")
        result.language.c = string();
      else if (k == "cpp")
        result.language.cpp = string();
      else
        fail("unknown language key");
    } else if (section == "workspace") {
      auto &w = *result.workspace;
      if (k == "members")
        w.members = array();
      else if (k == "default-members")
        w.default_members = array();
      else if (k == "exclude")
        w.exclude = array();
      else if (k == "resolver")
        w.resolver = string();
      else
        fail("unknown workspace key");
    } else if (section == "dependencies") {
      name(k);
      expect('{');
      space();
      if (key() != "path")
        fail("expected dependency path");
      space();
      expect('=');
      space();
      auto path = string();
      space();
      expect('}');
      if (path.empty())
        fail("empty dependency path");
      result.dependencies.push_back({std::move(k), std::move(path)});
    } else if (section == "lib") {
      auto &l = *result.library;
      if (k == "name")
        l.name = string();
      else if (k == "type")
        l.type = string();
      else if (k == "sources")
        l.sources = array();
      else if (k == "headers")
        l.headers = array();
      else if (k == "public-include-dirs")
        l.public_include_dirs = array();
      else if (k == "system-libraries")
        l.system_libraries = array();
      else
        fail("unknown library key");
    } else {
      auto &b = result.bins.back();
      if (k == "name")
        b.name = string();
      else if (k == "entry")
        b.entry = string();
      else if (k == "sources")
        b.sources = array();
      else if (k == "exclude")
        b.exclude = array();
      else if (k == "include-dirs")
        b.include_dirs = array();
      else if (k == "depends-on")
        b.depends_on = array();
      else if (k == "system-libraries")
        b.system_libraries = array();
      else
        fail("unknown binary key");
    }
  }

public:
  /// Creates a reader borrowing input text and source identity for parsing.
  /// @param[in] bytes std::string_view — Manifest text, borrowed until parse
  /// completes.
  /// @param[in] identity std::string_view — Diagnostic source identity,
  /// borrowed until parse completes.
  /// @return void — No value; initializes an empty parse state.
  /// @throws std::bad_alloc, std::length_error — If member initialization
  /// requires allocation (the views themselves do not).
  Reader(std::string_view bytes, std::string_view identity)
      : text(bytes), source(identity) {}
  /// Parses the complete document and validates cross-table requirements.
  /// @return Manifest — Fully owned parsed values in declaration order.
  /// @throws core::Diagnostic — invalid_input for malformed input or missing
  /// required version/package/workspace declarations.
  /// @throws std::bad_alloc, std::length_error — Allocation/size failures
  /// escape without diagnostic conversion.
  Manifest parse() {
    for (pos = 0; pos < text.size(); ++pos) {
      unsigned char c = static_cast<unsigned char>(text[pos]);
      if ((c < 0x20 && c != '\n' && c != '\r' && c != '\t') || c == 0x7f)
        fail("invalid control byte");
    }
    pos = 0;
    while (!end()) {
      space();
      comment();
      if (newline())
        continue;
      if (end())
        break;
      if (peek() == '[')
        header();
      else
        field();
      space();
      comment();
      if (!end() && !newline())
        fail("unexpected trailing input");
    }
    finish_section();
    required(version, "manifest-version");
    required(result.package.has_value() || result.workspace.has_value(),
             "package or workspace");
    if (!result.package && package_section) {
      pos = *package_section;
      fail("package section requires [package]");
    }
    return std::move(result);
  }
};
} // namespace
/// Parses manifest bytes and converts parser diagnostics to the result type.
/// @param[in] bytes std::string_view — Borrowed manifest text for this call.
/// @param[in] source std::string_view — Borrowed source identity copied into
/// diagnostics.
/// @return core::Result<Manifest> — Owned manifest on success; invalid_input
/// diagnostic with source and one-based line/column on parse failure.
/// @throws std::bad_alloc, std::length_error — Allocation/size failures are
/// not caught; only core::Diagnostic is returned as an error.
core::Result<Manifest> parse_manifest(std::string_view bytes,
                                      std::string_view source) {
  try {
    return Reader(bytes, source).parse();
  } catch (core::Diagnostic &d) {
    return std::unexpected(std::move(d));
  }
}
} // namespace cap::manifest
