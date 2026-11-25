module;

#include <algorithm>
#include <boost/json.hpp>
#include <boost/json/basic_parser_impl.hpp>
#include <boost/system/error_code.hpp>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <map>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module cap.build.modules;

import cap.build.actions;
import cap.core.diagnostic;

export namespace cap::build {

/// One GCC named-module unit's primary object and reported logical names.
struct ModuleUnit {
  /// Absolute primary object path; invalid paths fail module ordering.
  std::string object;
  /// Logical names provided by this compile unit.
  std::vector<std::string> provides;
  /// Logical names imported by this compile unit.
  std::vector<std::string> required_modules;
};

/// Sorted caller-approved logical module names to absolute lexically canonical
/// `.gcm` paths; this value does not establish that a file exists.
using ModuleMap = std::map<std::string, std::string, std::less<>>;

/// Projects a planned C++ compile action into GCC P1689 scan argv.
/// @param[in] action const Action& — Canonical module-enabled compile action.
/// @param[in] report_path std::string_view — Absolute caller-owned report path.
/// @return cap::core::Result<std::vector<std::string>> — Direct-exec argv, or
/// `invalid_input` for malformed actions or paths.
/// May propagate allocation and length exceptions.
cap::core::Result<std::vector<std::string>>
plan_module_scan(const Action &action, std::string_view report_path);

/// Parses one GCC 16 P1689 report for the expected primary object.
/// @param[in] report std::string_view — GCC JSON report bytes.
/// @param[in] expected_object std::string_view — Absolute primary object path.
/// @return cap::core::Result<ModuleUnit> — Owned metadata, or `invalid_input`
/// for malformed/unsupported data. May propagate allocation and length
/// exceptions.
cap::core::Result<ModuleUnit>
parse_module_scan(std::string_view report, std::string_view expected_object);

/// Orders local module units, resolving imports only within those units.
/// @param[in] units std::vector<ModuleUnit> — Owned units to validate and
/// order.
/// @return cap::core::Result<std::vector<ModuleUnit>> — Deterministic
/// provider-before-importer order, or `invalid_input`.
/// May propagate allocation and length exceptions.
cap::core::Result<std::vector<ModuleUnit>>
order_module_units(std::vector<ModuleUnit> units);

/// Orders local units while treating validated external map entries as built.
/// @param[in] units std::vector<ModuleUnit> — Owned local units to order.
/// @param[in] external_providers const ModuleMap& — Sorted module names bound
/// to lexically canonical `.gcm` paths; existence is caller-verified.
/// @return cap::core::Result<std::vector<ModuleUnit>> — Deterministic
/// provider-before-importer order, or `invalid_input` for invalid bindings,
/// collisions, missing imports, or invalid local graphs.
/// May propagate `std::bad_alloc`, `std::length_error`, or
/// `std::filesystem::filesystem_error`.
cap::core::Result<std::vector<ModuleUnit>>
order_module_units(std::vector<ModuleUnit> units,
                   const ModuleMap &external_providers);

/// Formats caller-approved providers as GCC `NAME PATH` mapper rows.
/// @param[in] providers const ModuleMap& — Sorted logical names bound to
/// absolute lexically canonical `.gcm` paths; existence is caller-verified.
/// @return cap::core::Result<std::string> — Deterministic bytes with
/// newline-terminated nonempty rows, or `invalid_input` for malformed
/// bindings. Spaces are preserved; control bytes are rejected. May propagate
/// `std::bad_alloc`, `std::length_error`, or
/// `std::filesystem::filesystem_error`.
cap::core::Result<std::string> format_module_map(const ModuleMap &providers);
} // namespace cap::build

namespace cap::build {
namespace {
using cap::core::Diagnostic;
using cap::core::ErrorCode;

auto invalid(std::string message)
    -> cap::core::Result<std::vector<std::string>> {
  Diagnostic error;
  error.code = ErrorCode::invalid_input;
  error.message = std::move(message);
  return std::unexpected(std::move(error));
}
auto invalid_unit(std::string message) -> cap::core::Result<ModuleUnit> {
  Diagnostic error;
  error.code = ErrorCode::invalid_input;
  error.message = std::move(message);
  return std::unexpected(std::move(error));
}
auto invalid_order(std::string message)
    -> cap::core::Result<std::vector<ModuleUnit>> {
  Diagnostic error;
  error.code = ErrorCode::invalid_input;
  error.message = std::move(message);
  return std::unexpected(std::move(error));
}
auto invalid_names(std::string message)
    -> cap::core::Result<std::vector<std::string>> {
  Diagnostic error;
  error.code = ErrorCode::invalid_input;
  error.message = std::move(message);
  return std::unexpected(std::move(error));
}

bool absolute(std::string_view value) {
  return !value.empty() && value.front() == '/' &&
         value.find('\0') == std::string_view::npos;
}

/// Checks an absolute path's lexical `.gcm` form without querying the
/// filesystem.
/// @param[in] value std::string_view — Path bytes to validate.
/// @return bool — True when absolute, control-free, `.gcm`-suffixed and already
/// lexically normalized; false otherwise. Does not throw except for allocation
/// failure or filesystem path conversion exceptions (`std::bad_alloc`,
/// `std::length_error`, or `std::filesystem::filesystem_error`).
bool lexical_gcm_path(std::string_view value) {
  if (!absolute(value) || !value.ends_with(".gcm") ||
      std::ranges::any_of(
          value, [](unsigned char c) { return c < 0x20 || c == 0x7f; }))
    return false;
  const std::filesystem::path path(value);
  return path.lexically_normal().string() == value;
}

bool module_name(std::string_view name) {
  if (name.empty())
    return false;
  bool segment_start = true;
  bool partition_seen = false;
  for (char c : name) {
    if (c == '.' || c == ':') {
      if (segment_start)
        return false;
      if (c == ':' && std::exchange(partition_seen, true))
        return false;
      segment_start = true;
    } else if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' ||
               (!segment_start && c >= '0' && c <= '9')) {
      segment_start = false;
    } else {
      return false;
    }
  }
  return !segment_start;
}

/// Validates every external map binding without formatting or file access.
/// @param[in] providers const ModuleMap& — Logical names and paths to check.
/// @return bool — True only when each name and lexical path is valid.
/// Does not throw except for `std::bad_alloc`, `std::length_error`, or
/// `std::filesystem::filesystem_error` during path conversion.
bool valid_module_map(const ModuleMap &providers) {
  /// Validates one logical-name/path binding.
  /// @param[in] binding const ModuleMap::value_type& — Binding to inspect.
  /// @return bool — True for a valid name and lexically canonical path.
  /// May propagate `std::bad_alloc`, `std::length_error`, or
  /// `std::filesystem::filesystem_error` from path conversion.
  const auto valid_binding = [](const ModuleMap::value_type &binding) {
    return module_name(binding.first) && lexical_gcm_path(binding.second);
  };
  return std::ranges::all_of(providers, valid_binding);
}

bool allowed_keys(const boost::json::object &object,
                  std::initializer_list<std::string_view> allowed) {
  return std::ranges::all_of(object, [&](const auto &item) {
    const std::string_view key(item.key().data(), item.key().size());
    return std::ranges::find(allowed, key) != allowed.end();
  });
}

struct json_member_counter {
  static constexpr std::size_t max_array_size = -1;
  static constexpr std::size_t max_object_size = -1;
  static constexpr std::size_t max_string_size = -1;
  static constexpr std::size_t max_key_size = -1;
  std::size_t count = 0;

  bool on_object_end(std::size_t members, boost::system::error_code &) {
    count += members;
    return true;
  }
  template <class... T> bool on_document_begin(T &&...) { return true; }
  template <class... T> bool on_document_end(T &&...) { return true; }
  template <class... T> bool on_object_begin(T &&...) { return true; }
  template <class... T> bool on_array_begin(T &&...) { return true; }
  template <class... T> bool on_array_end(T &&...) { return true; }
  bool on_key_part(boost::json::string_view, std::size_t,
                   boost::system::error_code &) {
    return true;
  }
  bool on_key(boost::json::string_view, std::size_t,
              boost::system::error_code &) {
    return true;
  }
  bool on_string_part(boost::json::string_view, std::size_t,
                      boost::system::error_code &) {
    return true;
  }
  bool on_string(boost::json::string_view, std::size_t,
                 boost::system::error_code &) {
    return true;
  }
  bool on_number_part(boost::json::string_view, boost::system::error_code &) {
    return true;
  }
  bool on_int64(std::int64_t, boost::json::string_view,
                boost::system::error_code &) {
    return true;
  }
  bool on_uint64(std::uint64_t, boost::json::string_view,
                 boost::system::error_code &) {
    return true;
  }
  bool on_double(double, boost::json::string_view,
                 boost::system::error_code &) {
    return true;
  }
  template <class... T> bool on_bool(T &&...) { return true; }
  template <class... T> bool on_null(T &&...) { return true; }
  bool on_comment_part(boost::json::string_view, boost::system::error_code &) {
    return true;
  }
  bool on_comment(boost::json::string_view, boost::system::error_code &) {
    return true;
  }
};

std::size_t dom_member_count(const boost::json::value &value) {
  if (value.is_object()) {
    std::size_t count = value.as_object().size();
    for (const auto &member : value.as_object())
      count += dom_member_count(member.value());
    return count;
  }
  if (value.is_array()) {
    std::size_t count = 0;
    for (const auto &element : value.as_array())
      count += dom_member_count(element);
    return count;
  }
  return 0;
}

bool has_unique_json_keys(std::string_view report,
                          const boost::json::value &parsed) {
  boost::system::error_code ec;
  boost::json::basic_parser<json_member_counter> parser({});
  parser.write_some(false, report.data(), report.size(), ec);
  return !ec && parser.handler().count == dom_member_count(parsed);
}

auto module_names(const boost::json::value &value, bool provide)
    -> cap::core::Result<std::vector<std::string>> {
  if (!value.is_array())
    return invalid_names("module report field must be an array");
  std::vector<std::string> result;
  for (const auto &entry : value.as_array()) {
    if (!entry.is_object() ||
        !allowed_keys(
            entry.as_object(),
            provide ? std::initializer_list<std::string_view>{"logical-name",
                                                              "is-interface"}
                    : std::initializer_list<std::string_view>{"logical-name"}))
      return invalid_names("unsupported or malformed module metadata");
    const auto &object = entry.as_object();
    const auto *name = object.if_contains("logical-name");
    if (name == nullptr || !name->is_string() ||
        !module_name(name->as_string()))
      return invalid_names("invalid module logical-name");
    if (const auto *iface = object.if_contains("is-interface");
        iface != nullptr && !iface->is_bool())
      return invalid_names("module is-interface must be boolean");
    result.emplace_back(name->as_string());
  }
  std::ranges::sort(result);
  if (std::ranges::adjacent_find(result) != result.end())
    return invalid_names("duplicate module name in report");
  return result;
}
} // namespace

cap::core::Result<std::vector<std::string>>
plan_module_scan(const Action &action, std::string_view report_path) {
  if (action.kind != ActionKind::compile || !absolute(report_path) ||
      action.inputs.size() != 1 || action.outputs.size() != 1 ||
      !absolute(action.inputs.front()) || !absolute(action.outputs.front()) ||
      action.arguments.size() < 7 || !absolute(action.arguments.front()) ||
      !absolute(action.directory))
    return invalid("invalid module scan action or report path");
  const auto &a = action.arguments;
  if (std::ranges::any_of(a, [](const auto &arg) { return arg.empty(); }))
    return invalid("compile action contains an empty argument");
  if (a[1] != "-std=c++20" && a[1] != "-std=c++23" && a[1] != "-std=c++26")
    return invalid("module scan requires a C++ compile action");
  const auto &source = action.inputs.front();
  const bool module_source = source.ends_with(".cppm");
  if (!(source.ends_with(".cc") || source.ends_with(".cpp") ||
        source.ends_with(".cxx") || module_source))
    return invalid("module scan requires a planned C++ source");
  std::size_t i = 2;
  if (i < a.size() && a[i] == "-fmodules")
    ++i;
  else
    return invalid("module scan requires planner module mode");
  if (module_source && i + 1 < a.size() && a[i] == "-x" && a[i + 1] == "c++")
    i += 2;
  else if (module_source)
    return invalid("module interface action is missing its language switch");
  else if (i < a.size() && a[i] == "-x")
    return invalid("ordinary C++ action has an unexpected language switch");
  while (i < a.size() && a[i] == "-I") {
    if (i + 1 >= a.size() || !absolute(a[i + 1]))
      return invalid("malformed compile include projection");
    i += 2;
  }
  if (i + 4 != a.size() || a[i] != "-c" || a[i + 1] != action.inputs.front() ||
      a[i + 2] != "-o" || a[i + 3] != action.outputs.front())
    return invalid("compile action does not match canonical planner form");
  std::vector<std::string> scan(a.begin(), a.begin() + static_cast<long>(i));
  scan.insert(scan.end(), {"-M", "-MF", "/dev/null", "-fdeps-format=p1689r5",
                           "-fdeps-target=" + action.outputs.front(),
                           "-fdeps-file=" + std::string(report_path),
                           action.inputs.front()});
  return scan;
}

cap::core::Result<ModuleUnit>
parse_module_scan(std::string_view report, std::string_view expected_object) {
  if (!absolute(expected_object))
    return invalid_unit("expected module object must be absolute");
  boost::system::error_code ec;
  auto parsed = boost::json::parse(report, ec);
  if (ec || !parsed.is_object() || !has_unique_json_keys(report, parsed))
    return invalid_unit("malformed GCC module dependency JSON");
  const auto &root = parsed.as_object();
  if (!allowed_keys(root, {"version", "revision", "rules"}))
    return invalid_unit("unsupported GCC module dependency fields");
  const auto *version = root.if_contains("version");
  const auto *revision = root.if_contains("revision");
  const auto *rules = root.if_contains("rules");
  if (version == nullptr || revision == nullptr || rules == nullptr ||
      !version->is_int64() || version->as_int64() != 0 ||
      !revision->is_int64() || revision->as_int64() != 0 ||
      !rules->is_array() || rules->as_array().size() != 1 ||
      !rules->as_array().front().is_object())
    return invalid_unit("unsupported GCC module dependency version or rules");
  const auto &rule = rules->as_array().front().as_object();
  if (!allowed_keys(rule, {"primary-output", "provides", "requires"}))
    return invalid_unit("unsupported GCC module rule fields");
  const auto *primary = rule.if_contains("primary-output");
  if (primary == nullptr || !primary->is_string() ||
      primary->as_string() != expected_object)
    return invalid_unit("module report primary output mismatch");
  ModuleUnit unit{std::string(expected_object), {}, {}};
  if (const auto *provides = rule.if_contains("provides")) {
    auto parsed_names = module_names(*provides, true);
    if (!parsed_names)
      return std::unexpected(parsed_names.error());
    unit.provides = std::move(*parsed_names);
  }
  if (const auto *imports = rule.if_contains("requires")) {
    auto parsed_names = module_names(*imports, false);
    if (!parsed_names)
      return std::unexpected(parsed_names.error());
    unit.required_modules = std::move(*parsed_names);
  }
  return unit;
}

cap::core::Result<std::vector<ModuleUnit>>
order_module_units(std::vector<ModuleUnit> units) {
  return order_module_units(std::move(units), {});
}

cap::core::Result<std::string> format_module_map(const ModuleMap &providers) {
  if (!valid_module_map(providers)) {
    Diagnostic error;
    error.code = ErrorCode::invalid_input;
    error.message = "invalid GCC module mapper binding";
    return std::unexpected(std::move(error));
  }
  std::string result;
  for (const auto &[name, path] : providers) {
    result.append(name).push_back(' ');
    result.append(path).push_back('\n');
  }
  return result;
}

cap::core::Result<std::vector<ModuleUnit>>
order_module_units(std::vector<ModuleUnit> units,
                   const ModuleMap &external_providers) {
  if (!valid_module_map(external_providers))
    return invalid_order("invalid GCC module mapper binding");
  std::map<std::string, std::size_t, std::less<>> providers;
  std::set<std::string, std::less<>> objects;
  for (std::size_t i = 0; i < units.size(); ++i) {
    auto &unit = units[i];
    if (!absolute(unit.object) || !objects.insert(unit.object).second)
      return invalid_order("invalid or duplicate module unit object");
    std::set<std::string, std::less<>> names;
    for (const auto &name : unit.provides) {
      if (!module_name(name) || !names.insert(name).second ||
          !providers.emplace(name, i).second)
        return invalid_order("invalid or duplicate module provider");
    }
    for (const auto &name : unit.required_modules)
      if (!module_name(name) || !names.insert(name).second)
        return invalid_order("invalid required module name");
  }
  std::vector<std::set<std::size_t>> edges(units.size());
  std::vector<std::size_t> indegree(units.size());
  for (std::size_t i = 0; i < units.size(); ++i)
    for (const auto &name : units[i].required_modules) {
      const auto found = providers.find(name);
      if (found == providers.end()) {
        if (external_providers.contains(name))
          continue;
        return invalid_order("missing module provider: " + name);
      }
      if (found->second != i && edges[found->second].insert(i).second)
        ++indegree[i];
      if (found->second == i)
        return invalid_order("module unit imports its own provider");
    }
  for (const auto &[name, _] : external_providers)
    if (providers.contains(name))
      return invalid_order(
          "external module provider collides with local provider: " + name);
  std::set<std::pair<std::string, std::size_t>> ready;
  for (std::size_t i = 0; i < units.size(); ++i)
    if (indegree[i] == 0)
      ready.emplace(units[i].object, i);
  std::vector<ModuleUnit> ordered;
  ordered.reserve(units.size());
  while (!ready.empty()) {
    const auto next = ready.begin()->second;
    ready.erase(ready.begin());
    ordered.push_back(std::move(units[next]));
    for (const auto dependant : edges[next])
      if (--indegree[dependant] == 0)
        ready.emplace(units[dependant].object, dependant);
  }
  if (ordered.size() != units.size())
    return invalid_order("cyclic module imports");
  return ordered;
}
} // namespace cap::build
