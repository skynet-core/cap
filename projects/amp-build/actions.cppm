module;

#include <algorithm>
#include <expected>
#include <map>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

export module amp.build.actions;

import amp.core.diagnostic;

export namespace amp::build {

/// Kind of native action represented by an Action.
enum class ActionKind {
  /// Compile one source to an object.
  compile,
  /// Archive ordered object files.
  archive,
  /// Link ordered inputs into an output.
  link
};

/// Owned, deterministic direct-execution plan; planning performs no I/O.
struct Action {
  /// ActionKind — Operation to perform.
  ActionKind kind;
  /// std::string — Absolute working directory.
  std::string directory;
  /// std::vector<std::string> — Direct argv, including the tool at index zero.
  std::vector<std::string> arguments;
  /// std::vector<std::string> — Declared input paths in operand order.
  std::vector<std::string> inputs;
  /// std::vector<std::string> — Declared output paths.
  std::vector<std::string> outputs;
};

/// Owned inputs for planning a single-source C or C++ compilation.
struct CompileRequest {
  std::string
      /// std::string — Absolute compiler path.
      tool,
      /// std::string — Absolute working directory.
      directory,
      /// std::string — Absolute source path.
      source,
      /// std::string — Absolute object output path.
      output,
      /// std::string — Requested C or C++ standard spelling.
      standard;
  /// std::vector<std::string> — Ordered absolute include directories.
  std::vector<std::string> include_dirs;
  /// bool — Enable C++ module mode; required for .cppm sources.
  bool cxx_modules = false;
};

/// Owned inputs for planning an archive of ordered object files.
struct ArchiveRequest {
  std::string
      /// std::string — Absolute archiver path.
      tool,
      /// std::string — Absolute working directory.
      directory,
      /// std::string — Absolute archive output path.
      output;
  /// std::vector<std::string> — Nonempty ordered absolute object paths.
  std::vector<std::string> objects;
};

/// Owned inputs for planning a link with resolved ordered operands.
struct LinkRequest {
  std::string
      /// std::string — Absolute linker driver path.
      tool,
      /// std::string — Absolute working directory.
      directory,
      /// std::string — Absolute link output path.
      output;
  std::vector<std::string>
      /// std::vector<std::string> — Ordered absolute object paths.
      objects,
      /// std::vector<std::string> — Ordered absolute archive paths.
      dependent_archives,
      /// std::vector<std::string> — Ordered resolved absolute SDK operands.
      system_libraries;
};

/// Caller-resolved archive identity, output and direct link requirements.
struct ArchiveLinkFact {
  std::string
      /// std::string — Nonempty NUL-free archive identity.
      id,
      /// std::string — Absolute archive output path.
      output;
  std::vector<std::string>
      /// std::vector<std::string> — Ordered direct prerequisite IDs.
      direct_prerequisites,
      /// std::vector<std::string> — Ordered resolved absolute SDK operands.
      system_libraries;
};

/// Owned ordered operands for a final link.
struct LinkInputs {
  std::vector<std::string>
      /// std::vector<std::string> — Ordered unique archive paths.
      ordered_archives,
      /// std::vector<std::string> — Ordered SDK operands, retaining duplicates.
      system_libraries;
};

/// Plan a compile without executing it.
/// @param[in] request const CompileRequest & — Owned request to validate.
/// @return amp::core::Result<Action> — Planned action, or invalid_input for
/// invalid paths, language, standard or module mode.
/// @throws std::bad_alloc If allocation fails.
/// @throws std::length_error If a constructed string or vector is too large.
amp::core::Result<Action> plan_compile(const CompileRequest &request);
/// Plan an archive without executing it.
/// @param[in] request const ArchiveRequest & — Request to validate.
/// @return amp::core::Result<Action> — Planned action, or invalid_input for
/// invalid paths or an empty object list.
/// @throws std::bad_alloc If allocation fails.
/// @throws std::length_error If a constructed string or vector is too large.
amp::core::Result<Action> plan_archive(const ArchiveRequest &request);
/// Plan a link without resolving operands or executing it.
/// @param[in] request const LinkRequest & — Request to validate.
/// @return amp::core::Result<Action> — Planned action, or invalid_input for
/// invalid paths or an empty object and archive list.
/// @throws std::bad_alloc If allocation fails.
/// @throws std::length_error If a constructed string or vector is too large.
amp::core::Result<Action> plan_link(const LinkRequest &request);
/// Order reachable archives before prerequisites and concatenate SDK operands.
/// @param[in] direct_ids const std::vector<std::string> & — Ordered root IDs.
/// @param[in] archive_facts const std::vector<ArchiveLinkFact> & — Available
/// archive identities, paths and direct requirements.
/// @param[in] system_libraries const std::vector<std::string> & — Ordered
/// resolved binary SDK operands.
/// @return amp::core::Result<LinkInputs> — Ordered operands, or invalid_input
/// for invalid facts, paths, reachable references, duplicate roots or cycles.
/// @throws std::bad_alloc If allocation fails.
/// @throws std::length_error If a constructed string or vector is too large.
amp::core::Result<LinkInputs>
plan_link_inputs(const std::vector<std::string> &direct_ids,
                 const std::vector<ArchiveLinkFact> &archive_facts,
                 const std::vector<std::string> &system_libraries);
} // namespace amp::build

namespace amp::build {
namespace {
using amp::core::Diagnostic;
using amp::core::ErrorCode;

/// Construct an invalid-input diagnostic for an action request.
/// @param[in] message std::string — Diagnostic text, moved into the result.
/// @return amp::core::Result<Action> — An invalid_input error.
/// @note Returns only the diagnostic and does not throw.
auto invalid(std::string message) -> amp::core::Result<Action> {
  Diagnostic error;
  error.code = ErrorCode::invalid_input;
  error.message = std::move(message);
  return std::unexpected(std::move(error));
}

/// Check the lexical path requirements without accessing the filesystem.
/// @param[in] path const std::string & — Path to check.
/// @return bool — True for a nonempty absolute path without embedded NUL.
/// @note Does not throw.
bool path_ok(const std::string &path) {
  return !path.empty() && path.front() == '/' &&
         path.find('\0') == std::string::npos;
}

/// Check each path against the planner's lexical requirements.
/// @param[in] paths const std::vector<std::string> & — Paths to check.
/// @return bool — True when every path is valid, including an empty list.
/// @note Does not throw.
bool paths_ok(const std::vector<std::string> &paths) {
  return std::ranges::all_of(paths, path_ok);
}

/// Source language inferred from a supported filename suffix.
enum class SourceLanguage {
  /// C source.
  c,
  /// C++ source, including a module interface.
  cpp,
  /// Unsupported filename suffix.
  unknown
};

/// Infer the source language from its case-sensitive suffix.
/// @param[in] source std::string_view — Filename or path to inspect.
/// @return SourceLanguage — The recognized language, or unknown.
/// @note Does not throw.
SourceLanguage source_language(std::string_view source) {
  if (source.ends_with(".c"))
    return SourceLanguage::c;
  if (source.ends_with(".cc") || source.ends_with(".cpp") ||
      source.ends_with(".cxx") || source.ends_with(".cppm"))
    return SourceLanguage::cpp;
  return SourceLanguage::unknown;
}

} // namespace

amp::core::Result<Action> plan_compile(const CompileRequest &r) {
  /// SourceLanguage — Language inferred from the request's source suffix.
  const auto language = source_language(r.source);
  /// bool — Whether the source is recognized as C++.
  const bool cpp = language == SourceLanguage::cpp;
  /// bool — Whether the source is a .cppm interface.
  const bool module = r.source.ends_with(".cppm");
  /// bool — Whether the standard is one of the supported spellings.
  const bool standard_ok = r.standard == "c11" || r.standard == "c17" ||
                           r.standard == "c23" || r.standard == "c++20" ||
                           r.standard == "c++23" || r.standard == "c++26";
  if (!path_ok(r.tool) || !path_ok(r.directory) || !path_ok(r.source) ||
      !path_ok(r.output) || !paths_ok(r.include_dirs) || !standard_ok ||
      language == SourceLanguage::unknown ||
      (!cpp && r.standard.starts_with("c++")) ||
      (cpp && !r.standard.starts_with("c++")) || (module && !cpp) ||
      (r.cxx_modules && !cpp) || (module && !r.cxx_modules))
    return invalid("invalid compile action request");
  std::vector<std::string> argv{r.tool, "-std=" + r.standard};
  if (r.cxx_modules)
    argv.emplace_back("-fmodules");
  if (module)
    argv.insert(argv.end(), {"-x", "c++"});
  for (const auto &dir : r.include_dirs)
    argv.insert(argv.end(), {"-I", dir});
  argv.insert(argv.end(), {"-c", r.source, "-o", r.output});
  return Action{ActionKind::compile,
                r.directory,
                std::move(argv),
                {r.source},
                {r.output}};
}

amp::core::Result<Action> plan_archive(const ArchiveRequest &r) {
  if (!path_ok(r.tool) || !path_ok(r.directory) || !path_ok(r.output) ||
      r.objects.empty() || !paths_ok(r.objects))
    return invalid("invalid archive action request");
  std::vector<std::string> argv{r.tool, "rcs", r.output};
  argv.insert(argv.end(), r.objects.begin(), r.objects.end());
  return Action{
      ActionKind::archive, r.directory, std::move(argv), r.objects, {r.output}};
}

amp::core::Result<Action> plan_link(const LinkRequest &r) {
  if (!path_ok(r.tool) || !path_ok(r.directory) || !path_ok(r.output) ||
      (r.objects.empty() && r.dependent_archives.empty()) ||
      !paths_ok(r.objects) || !paths_ok(r.dependent_archives) ||
      !paths_ok(r.system_libraries))
    return invalid("invalid link action request");
  std::vector<std::string> inputs = r.objects;
  inputs.insert(inputs.end(), r.dependent_archives.begin(),
                r.dependent_archives.end());
  inputs.insert(inputs.end(), r.system_libraries.begin(),
                r.system_libraries.end());
  std::vector<std::string> argv{r.tool, "-o", r.output};
  argv.insert(argv.end(), inputs.begin(), inputs.end());
  return Action{ActionKind::link,
                r.directory,
                std::move(argv),
                std::move(inputs),
                {r.output}};
}

amp::core::Result<LinkInputs>
plan_link_inputs(const std::vector<std::string> &direct_ids,
                 const std::vector<ArchiveLinkFact> &archive_facts,
                 const std::vector<std::string> &system_libraries) {
  /// Construct an invalid-input diagnostic for a link-input request.
  /// @param[in] message std::string — Diagnostic text, moved into the result.
  /// @return amp::core::Result<LinkInputs> — An invalid_input error.
  /// @note Does not throw.
  const auto invalid_inputs =
      [](std::string message) -> amp::core::Result<LinkInputs> {
    Diagnostic error;
    error.code = ErrorCode::invalid_input;
    error.message = std::move(message);
    return std::unexpected(std::move(error));
  };
  std::map<std::string, const ArchiveLinkFact *, std::less<>> facts;
  std::set<std::string, std::less<>> outputs;
  for (const auto &fact : archive_facts) {
    if (fact.id.empty() || fact.id.find('\0') != std::string::npos ||
        !path_ok(fact.output) || !paths_ok(fact.system_libraries) ||
        !facts.emplace(fact.id, &fact).second ||
        !outputs.insert(fact.output).second)
      return invalid_inputs("invalid or duplicate archive fact");
  }
  if (!paths_ok(system_libraries))
    return invalid_inputs("invalid resolved system-library operand");

  std::map<std::string, std::size_t, std::less<>> indices;
  std::vector<const ArchiveLinkFact *> discovered;
  std::vector<std::size_t> incoming;
  std::set<std::string, std::less<>> direct_seen;
  for (const auto &id : direct_ids) {
    if (!direct_seen.insert(id).second || !facts.contains(id))
      return invalid_inputs("invalid archive reference or reachable cycle: " +
                            id);
    if (indices.contains(id))
      continue;
    /// Add an existing archive to the discovery order and initialize its count.
    /// @param[in] next const std::string & — ID already present in facts.
    /// @return void — No value; updates indices, discovered and incoming.
    /// @throws std::bad_alloc If allocation fails.
    /// @throws std::length_error If a container exceeds its maximum size.
    /// @throws std::out_of_range If next is not present in facts.
    const auto discover = [&](const std::string &next) {
      indices.emplace(next, discovered.size());
      discovered.push_back(facts.at(next));
      incoming.push_back(0);
    };
    discover(id);
    std::vector<std::pair<std::size_t, std::size_t>> pending{
        {indices.at(id), 0}};
    while (!pending.empty()) {
      auto &[current, next_index] = pending.back();
      const auto &prerequisites = discovered[current]->direct_prerequisites;
      if (next_index == prerequisites.size()) {
        pending.pop_back();
        continue;
      }
      const auto &prerequisite = prerequisites[next_index++];
      if (!facts.contains(prerequisite))
        return invalid_inputs("invalid archive reference or reachable cycle: " +
                              prerequisite);
      /// bool — Whether this prerequisite has not yet been discovered.
      const bool fresh = !indices.contains(prerequisite);
      if (fresh)
        discover(prerequisite);
      /// std::size_t — Discovery index of the prerequisite.
      const auto index = indices.at(prerequisite);
      ++incoming[index];
      if (fresh)
        pending.emplace_back(index, 0);
    }
  }
  std::set<std::size_t> ready;
  for (std::size_t i = 0; i < incoming.size(); ++i)
    if (incoming[i] == 0)
      ready.insert(i);
  LinkInputs result{{}, system_libraries};
  while (!ready.empty()) {
    /// std::size_t — Earliest-discovered archive whose dependents are emitted.
    const auto index = *ready.begin();
    ready.erase(ready.begin());
    const auto &fact = *discovered[index];
    result.ordered_archives.push_back(fact.output);
    result.system_libraries.insert(result.system_libraries.end(),
                                   fact.system_libraries.begin(),
                                   fact.system_libraries.end());
    for (const auto &prerequisite : fact.direct_prerequisites) {
      /// std::size_t — Discovery index of this prerequisite.
      const auto next = indices.at(prerequisite);
      if (--incoming[next] == 0)
        ready.insert(next);
    }
  }
  if (result.ordered_archives.size() != discovered.size())
    return invalid_inputs("reachable archive cycle");
  return result;
}
} // namespace amp::build
