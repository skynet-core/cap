module;
#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <expected>
#include <filesystem>
#include <initializer_list>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

export module amp.cli.standalone;
import amp.core.diagnostic;
import amp.core.filesystem;
import amp.core.process;
import amp.manifest;
import amp.manifest.sources;
import amp.workspace;
import amp.workspace.dependencies;
import amp.toolchain.host;
import amp.build.actions;
import amp.build.modules;
import amp.build.native_binary;
import amp.compdb;
import amp.cli.build_options;
import amp.cli.workspace_targets;

export namespace amp::cli {
/// Owned result of a standalone or workspace build, or a non-executing help
/// request.
struct StandaloneOutput {
  /// std::vector<std::string> — Absolute artifact paths in selection order.
  std::vector<std::string> artifacts;
  /// std::string — Absolute published database path on success, otherwise
  /// empty.
  std::string compdb;
  /// std::string — Owned help text for a help request, otherwise empty.
  std::string help;
};
/// Build standalone or workspace targets, or return a structured diagnostic.
/// The caller-borrowed span and argument views need only remain valid for the
/// call.
/// @param[in] args std::span<const std::string_view> — Borrowed tokens after
/// `build`.
/// @return core::Result<StandaloneOutput> — Owned paths or help on success;
///         a core diagnostic on option, input, IO, tool or execution failure.
/// @throws std::bad_alloc If allocating an owned value fails.
/// @throws std::length_error If a constructed value exceeds its maximum size.
/// @throws std::exception If a dependency throws an untranslated exception.
core::Result<StandaloneOutput>
run_standalone(std::span<const std::string_view> args);
} // namespace amp::cli

namespace amp::cli {
namespace {
namespace fs = std::filesystem;
/// Validated input paths for one selected binary or static library.
struct SelectedTarget {
  /// std::string — Validated manifest target name.
  std::string name;
  /// bool — True for a static library, false for a binary.
  bool library;
  /// std::vector<std::string> — Canonical source files in selection order.
  std::vector<std::string> sources;
  /// std::vector<std::string> — Canonical include directories in manifest
  /// order.
  std::vector<std::string> includes;
};
/// Workspace-local build inputs and precomputed link plan for one target.
struct Prepared {
  /// TargetId — Workspace target whose manifest owns this build input.
  TargetId id;
  /// SelectedTarget — Canonical sources and inherited include paths.
  SelectedTarget target;
  /// std::vector<std::string> — Manifest SDK names for this target.
  std::vector<std::string> sdk;
  /// build::LinkInputs — Preplanned requester-first binary link operands.
  build::LinkInputs links;
  /// std::set<std::size_t> — Transitive dependency archive member indices.
  std::set<std::size_t> archive_members;
  /// std::string — Validated C standard for this member.
  std::string c_standard;
  /// std::string — Validated C++ standard for this member.
  std::string cpp_standard;
  /// fs::path — Private output directory, assigned during output preparation.
  fs::path directory;
  /// std::string — Absolute archive or executable path, assigned before
  /// execution.
  std::string output;
};
/// Owned workspace and selected execution closure for one request.
struct BuildContext {
  /// workspace::Workspace — Loaded, validated workspace.
  workspace::Workspace loaded;
  /// bool — Whether the loaded manifest declares a workspace.
  bool is_workspace;
  /// std::vector<TargetId> — Requested roots in publication order.
  std::vector<TargetId> selected;
  /// workspace::DependencyPlan — Validated workspace dependency edges.
  workspace::DependencyPlan dependencies;
  /// std::vector<TargetId> — Targets to build in execution order.
  std::vector<TargetId> work_targets;
};
/// Validated target inputs and archive facts for one build.
struct PreparedInputs {
  /// std::vector<Prepared> — Targets in execution order.
  std::vector<Prepared> targets;
  /// std::map<std::size_t, build::ArchiveLinkFact> — Facts keyed by member.
  std::map<std::size_t, build::ArchiveLinkFact> facts;
};
/// Output paths, link operands and toolchain ready for execution.
struct ExecutionPlan {
  /// std::vector<Prepared> — Validated targets with planned link operands.
  std::vector<Prepared> targets;
  /// fs::path — Private invocation directory.
  fs::path invocation;
  /// std::vector<std::string> — Selected artifact paths in request order.
  std::vector<std::string> artifacts;
  /// toolchain::HostToolchain — Probed explicit tools.
  toolchain::HostToolchain host;
};
/// Convert a workspace member index to its stable archive identity.
/// @param[in] index std::size_t — Index in the loaded workspace.
/// @return std::string — Owned decimal identity.
/// @throws std::bad_alloc If allocation fails.
/// @throws std::length_error If the identity exceeds its size limit.
std::string member_id(std::size_t index) { return std::to_string(index); }
/// Return an invalid-input diagnostic owning the supplied message.
/// @param[in] message std::string — Message moved into the diagnostic.
/// @return core::Diagnostic — Error-severity invalid-input diagnostic.
/// @throws std::bad_alloc If allocating a diagnostic field fails.
core::Diagnostic invalid(std::string message) {
  return {core::ErrorCode::invalid_input,
          core::Severity::error,
          std::move(message),
          {},
          {},
          {}};
}
/// Return whether the canonical child path is inside or equal to root.
/// @param[in] root const fs::path& — Canonical root.
/// @param[in] child const fs::path& — Canonical candidate.
/// @return bool — True when root is a component prefix of child.
/// @throws std::exception If path iteration fails; no failure result.
bool inside(const fs::path &root, const fs::path &child) {
  auto a = root.begin(), b = child.begin();
  for (; a != root.end(); ++a, ++b)
    if (b == child.end() || *a != *b)
      return false;
  return true;
}
/// Resolve a package-relative path to a canonical contained directory or file.
/// Return invalid-input on an invalid, escaping, missing or wrong-type path.
/// @param[in] root const fs::path& — Canonical package root.
/// @param[in] relative const std::string& — Package-relative path.
/// @param[in] dir bool — Require a directory if true, regular file otherwise.
/// @return core::Result<std::string> — Owned canonical path or diagnostic.
/// @throws std::bad_alloc If constructing paths or diagnostics fails.
/// @throws std::filesystem::filesystem_error If path operations throw.
/// @throws std::length_error If constructing a string exceeds its limit.
core::Result<std::string> contained(const fs::path &root,
                                    const std::string &relative, bool dir) {
  fs::path p(relative);
  if (relative.empty() || p.is_absolute() ||
      relative.find('\0') != relative.npos)
    return std::unexpected(invalid("invalid package path: " + relative));
  for (const auto &part : p)
    if (part == "..")
      return std::unexpected(invalid("package path traversal: " + relative));
  std::error_code ec;
  auto resolved = fs::canonical(root / p, ec);
  if (ec || !inside(root, resolved) ||
      !(dir ? fs::is_directory(resolved, ec)
            : fs::is_regular_file(resolved, ec)) ||
      ec)
    return std::unexpected(invalid("invalid package path: " + relative));
  return resolved.string();
}
/// Verify that path names a regular output without following a final symlink.
/// Return invalid-input when metadata is unavailable or has another type.
/// @param[in] path const std::string& — Output path to inspect.
/// @return core::Status — Success for a regular file, diagnostic otherwise.
/// @throws std::bad_alloc If constructing a path or diagnostic fails.
/// @throws std::filesystem::filesystem_error If path construction throws.
core::Status regular(const std::string &path) {
  std::error_code ec;
  if (!fs::is_regular_file(fs::symlink_status(path, ec)) || ec)
    return std::unexpected(invalid("expected regular output: " + path));
  return {};
}
/// Load and select targets and their workspace dependency closure.
/// @param[in] options const BuildOptions& — Parsed non-help request.
/// @param[in] filesystem const core::IFileSystem& — Workspace read adapter.
/// @return core::Result<BuildContext> — Owned context or original diagnostic.
/// @throws std::bad_alloc If allocation fails.
/// @throws std::length_error If owned storage exceeds its maximum size.
/// @throws std::filesystem::filesystem_error If a path operation fails.
/// @throws std::exception If a dependency or adapter throws another exception.
core::Result<BuildContext>
resolve_context(const BuildOptions &options,
                const core::IFileSystem &filesystem) {
  fs::path start = fs::current_path();
  fs::path explicit_manifest;
  if (options.manifest_path) {
    fs::path given(*options.manifest_path);
    std::error_code ec;
    if (given.filename() != "amp.toml" || !fs::is_regular_file(given, ec) || ec)
      return std::unexpected(invalid("manifest must be an existing amp.toml"));
    explicit_manifest = fs::canonical(given);
    if (explicit_manifest.filename() != "amp.toml")
      return std::unexpected(invalid("manifest must resolve to amp.toml"));
    start = explicit_manifest.parent_path();
  }
  auto loaded = workspace::load_workspace(start.string(), filesystem);
  if (!loaded)
    return std::unexpected(loaded.error());
  const bool is_workspace = loaded->manifest.workspace.has_value();
  if (!is_workspace && options.workspace)
    return std::unexpected(invalid("workspaces are unsupported"));
  if (!explicit_manifest.empty() &&
      explicit_manifest != fs::path(loaded->root) / "amp.toml" &&
      std::none_of(
          loaded->members.begin(), loaded->members.end(),
          /// Matches a loaded member's canonical manifest.
          /// @param[in] m const workspace::PackageMember& — Candidate member.
          /// @return bool — Whether the explicit manifest identifies it.
          /// @throws std::filesystem::filesystem_error If path construction
          /// fails.
          [&explicit_manifest](const auto &m) {
            return explicit_manifest == fs::path(m.root) / "amp.toml";
          }))
    return std::unexpected(invalid("manifest does not identify package"));
  auto selected_packages = workspace::select_packages(
      *loaded,
      options.package ? std::optional<std::string_view>(*options.package)
                      : std::nullopt,
      options.workspace);
  if (!selected_packages)
    return std::unexpected(selected_packages.error());
  auto selected = select_build_targets(*loaded, options);
  if (!selected)
    return std::unexpected(selected.error());
  workspace::DependencyPlan dependencies;
  std::vector<TargetId> work_targets = *selected;
  if (is_workspace) {
    auto resolved = workspace::resolve_dependencies(*loaded, *selected_packages,
                                                    filesystem);
    if (!resolved)
      return std::unexpected(resolved.error());
    dependencies = std::move(*resolved);
    auto planned = plan_workspace_targets(*selected, dependencies);
    if (!planned)
      return std::unexpected(planned.error());
    work_targets = std::move(*planned);
  } else if (!loaded->members[selected->front().member]
                  .manifest.dependencies.empty()) {
    return std::unexpected(
        invalid("dependencies and SDK libraries are unsupported"));
  }
  return BuildContext{std::move(*loaded), is_workspace, std::move(*selected),
                      std::move(dependencies), std::move(work_targets)};
}
/// Resolve and validate the build root, creating the default directories.
/// @param[in] options const BuildOptions& — Parsed build options.
/// @param[in] loaded const workspace::Workspace& — Validated workspace.
/// @param[in] selected const std::vector<TargetId>& — Nonempty selected roots.
/// @param[in] is_workspace bool — Whether the workspace root owns outputs.
/// @return core::Result<fs::path> — Canonical root or invalid-input diagnostic.
/// @throws std::bad_alloc If allocation fails.
/// @throws std::length_error If owned storage exceeds its maximum size.
/// @throws std::filesystem::filesystem_error If a path operation fails.
core::Result<fs::path> resolve_build_root(const BuildOptions &options,
                                          const workspace::Workspace &loaded,
                                          const std::vector<TargetId> &selected,
                                          bool is_workspace) {
  fs::path root =
      options.build_root
          ? fs::path(*options.build_root)
          : fs::path(is_workspace
                         ? loaded.root
                         : loaded.members[selected.front().member].root) /
                ".amp" / "build";
  if (!options.build_root) {
    auto base =
        fs::path(is_workspace ? loaded.root
                              : loaded.members[selected.front().member].root);
    for (const auto *component : {".amp", "build"}) {
      auto next = base / component;
      fs::create_directory(next);
      base = fs::canonical(next);
      if (!inside(is_workspace
                      ? fs::path(loaded.root)
                      : fs::path(loaded.members[selected.front().member].root),
                  base))
        return std::unexpected(invalid("default build root escapes package"));
    }
    root = base;
  } else {
    root = fs::canonical(root);
    if (!fs::is_directory(root))
      return std::unexpected(invalid("build root is not a directory"));
  }
  if (root.string().find_first_of("\r\n") != std::string::npos)
    return std::unexpected(invalid("newline in build root"));
  return root;
}
/// Merge verified providers from the target's prepared archive closure.
/// @param[in] members const std::set<std::size_t>& — Dependency member indices.
/// @param[in] archives const std::map<std::size_t, build::ModuleMap>& —
/// Successfully built archives keyed by member index.
/// @return core::Result<build::ModuleMap> — Visible bindings or a diagnostic
/// for a missing archive or duplicate module name.
/// @throws std::bad_alloc If allocation fails.
/// @throws std::length_error If owned storage exceeds its maximum size.
core::Result<build::ModuleMap>
visible_modules(const std::set<std::size_t> &members,
                const std::map<std::size_t, build::ModuleMap> &archives) {
  build::ModuleMap visible;
  for (auto member : members) {
    auto archive = archives.find(member);
    if (archive == archives.end())
      return std::unexpected(invalid("dependency archive was not built"));
    for (const auto &[name, path] : archive->second)
      if (!visible.emplace(name, path).second)
        return std::unexpected(invalid("duplicate module provider: " + name));
  }
  return visible;
}
/// Execute prepared targets in order using the supplied process and file
/// adapters.
/// @param[in] prepared const std::vector<Prepared>& — Validated target inputs.
/// @param[in] is_workspace bool — Enable closure-scoped module bindings.
/// @param[in] host const toolchain::HostToolchain& — Probed explicit tools.
/// @param[in] runner const core::IProcessRunner& — Process adapter.
/// @param[in] filesystem const core::IFileSystem& — Scan report reader and
/// atomic private mapper writer.
/// @return core::Result<std::vector<build::Action>> — Executed compile actions
/// or diagnostic.
/// @throws std::bad_alloc If allocation fails.
/// @throws std::length_error If owned storage exceeds its maximum size.
/// @throws std::filesystem::filesystem_error If a path operation fails.
/// @throws std::exception If a dependency or adapter throws another exception.
core::Result<std::vector<build::Action>>
execute_targets(const std::vector<Prepared> &prepared, bool is_workspace,
                const toolchain::HostToolchain &host,
                const core::IProcessRunner &runner,
                const core::IFileSystem &filesystem) {
  std::vector<build::Action> actions;
  std::map<std::size_t, build::ModuleMap> archives;
  for (const auto &item : prepared) {
    std::optional<build::ModuleMap> modules;
    if (is_workspace) {
      auto visible = visible_modules(item.archive_members, archives);
      if (!visible)
        return std::unexpected(visible.error());
      modules = std::move(*visible);
    }
    const auto &directory = item.directory;
    std::vector<build::CompileRequest> compiles;
    for (std::size_t i = 0; i < item.target.sources.size(); ++i) {
      bool is_c = item.target.sources[i].ends_with(".c");
      compiles.push_back(
          {is_c ? host.tools.c_compiler : host.tools.cpp_compiler,
           directory.string(), item.target.sources[i],
           (directory / ("object-" + std::to_string(i) + ".o")).string(),
           is_c ? item.c_standard : item.cpp_standard, item.target.includes,
           !is_c});
    }
    if (item.target.library) {
      auto result = build::run_native_archive(
          {directory.string(), host.tools.archiver, item.output,
           std::move(compiles), std::move(modules)},
          runner, filesystem, regular);
      if (!result)
        return std::unexpected(result.error());
      actions.insert(actions.end(), result->compile_actions.begin(),
                     result->compile_actions.end());
      if (is_workspace)
        archives.emplace(item.id.member, std::move(result->provided_modules));
    } else {
      build::NativeBinaryRequest request{
          directory.string(), host.tools.cpp_compiler,
          item.output,        std::move(compiles),
          item.links,         std::move(modules)};
      auto result =
          build::run_native_binary(request, runner, filesystem, regular);
      if (!result)
        return std::unexpected(result.error());
      actions.insert(actions.end(), result->compile_actions.begin(),
                     result->compile_actions.end());
    }
  }
  return actions;
}
/// Publish the exact executed compile actions after all targets succeed.
/// @param[in] actions const std::vector<build::Action>& — Executed actions.
/// @param[in] invocation const fs::path& — Private invocation directory.
/// @param[in] filesystem const core::IFileSystem& — Atomic output adapter.
/// @return core::Result<std::string> — Database path or serialization/IO
/// diagnostic.
/// @throws std::bad_alloc If allocation fails.
/// @throws std::length_error If owned storage exceeds its maximum size.
/// @throws std::filesystem::filesystem_error If a path operation fails.
/// @throws std::exception If a dependency or adapter throws another exception.
core::Result<std::string>
publish_database(const std::vector<build::Action> &actions,
                 const fs::path &invocation,
                 const core::IFileSystem &filesystem) {
  auto json = compdb::to_json(actions);
  if (!json)
    return std::unexpected(json.error());
  auto db = (invocation / "compile_commands.json").string();
  auto status = filesystem.write_atomic(
      db, std::as_bytes(std::span(json->data(), json->size())));
  if (!status)
    return std::unexpected(status.error());
  return db;
}
/// Validate selected target inputs and assemble archive facts.
/// @param[in] context_value const BuildContext& — Resolved workspace and
/// targets.
/// @param[in] filesystem const core::IFileSystem& — Source selection adapter.
/// @return core::Result<PreparedInputs> — Owned inputs or validation
/// diagnostic.
/// @throws std::bad_alloc If allocation fails.
/// @throws std::length_error If owned storage exceeds its maximum size.
/// @throws std::filesystem::filesystem_error If a path operation fails.
/// @throws std::exception If a dependency or adapter throws another exception.
core::Result<PreparedInputs>
prepare_inputs(const BuildContext &context_value,
               const core::IFileSystem &filesystem) {
  const auto &loaded = context_value.loaded;
  const auto &dependencies = context_value.dependencies;
  const auto &work_targets = context_value.work_targets;
  const bool is_workspace = context_value.is_workspace;

  std::vector<Prepared> prepared;
  std::set<std::string> owned_sources;
  std::map<std::size_t, build::ArchiveLinkFact> facts;
  for (auto id : work_targets) {
    const auto &member = loaded.members[id.member];
    const auto &model = member.manifest;
    const auto *binary = id.bin ? &model.bins[*id.bin] : nullptr;
    const auto *library =
        binary ? nullptr : (model.library ? &*model.library : nullptr);
    if (!binary && !library)
      return std::unexpected(invalid("selected library is missing"));
    if ((library && library->type != "static") ||
        (!is_workspace && binary && !binary->depends_on.empty()) ||
        (!is_workspace && binary && !binary->system_libraries.empty()) ||
        (!is_workspace && library && !library->system_libraries.empty()))
      return std::unexpected(invalid("dependencies, SDK libraries and "
                                     "non-static libraries are unsupported"));
    auto sources =
        binary ? manifest::select_sources(member.root, *binary, filesystem)
               : manifest::select_sources(member.root, *library, filesystem);
    if (!sources)
      return std::unexpected(sources.error());
    auto c = model.language.c.value_or("c23");
    auto cpp = model.language.cpp.value_or("c++26");
    if ((c != "c11" && c != "c17" && c != "c23") ||
        (cpp != "c++20" && cpp != "c++23" && cpp != "c++26"))
      return std::unexpected(invalid("unsupported language standard"));
    Prepared item{{id.member, id.bin},
                  {binary ? binary->name : library->name, !binary, {}, {}},
                  {},
                  {},
                  {},
                  std::move(c),
                  std::move(cpp),
                  {},
                  {}};
    for (const auto &source : *sources) {
      auto path = contained(member.root, source, false);
      if (!path)
        return std::unexpected(path.error());
      if (!owned_sources.insert(*path).second)
        return std::unexpected(invalid("duplicate canonical source: " + *path));
      item.target.sources.push_back(std::move(*path));
    }
    for (const auto &include :
         binary ? binary->include_dirs : library->public_include_dirs) {
      auto path = contained(member.root, include, true);
      if (!path)
        return std::unexpected(path.error());
      item.target.includes.push_back(std::move(*path));
    }
    const auto &sdk_names =
        binary ? binary->system_libraries : library->system_libraries;
    if (is_workspace) {
      std::vector<std::size_t> pending;
      if (id.bin)
        pending = dependencies.binary_edges[id.member][*id.bin];
      else
        pending = dependencies.edges[id.member];
      while (!pending.empty()) {
        auto dep = pending.back();
        pending.pop_back();
        if (!item.archive_members.insert(dep).second)
          continue;
        pending.insert(pending.end(), dependencies.edges[dep].begin(),
                       dependencies.edges[dep].end());
      }
      for (auto dep : item.archive_members) {
        const auto &dep_member = loaded.members[dep];
        if (!dep_member.manifest.library)
          return std::unexpected(invalid("dependency library is missing"));
        for (const auto &include :
             dep_member.manifest.library->public_include_dirs) {
          auto path = contained(dep_member.root, include, true);
          if (!path)
            return std::unexpected(path.error());
          item.target.includes.push_back(std::move(*path));
        }
      }
    }
    if (!binary) {
      build::ArchiveLinkFact fact{member_id(id.member), {}, {}, sdk_names};
      for (auto dep : dependencies.edges.empty()
                          ? std::vector<std::size_t>{}
                          : dependencies.edges[id.member])
        fact.direct_prerequisites.push_back(member_id(dep));
      facts.emplace(id.member, std::move(fact));
    }
    item.sdk = sdk_names;
    prepared.push_back(std::move(item));
  }
  return PreparedInputs{std::move(prepared), std::move(facts)};
}
/// Reserve output directories and resolve link operands before compilation.
/// @param[in] options_value const BuildOptions& — Parsed non-help request.
/// @param[in] context_value const BuildContext& — Resolved workspace and
/// targets.
/// @param[in] inputs PreparedInputs — Owned validated inputs to complete.
/// @param[in] runner const core::IProcessRunner& — Tool probe adapter.
/// @return core::Result<ExecutionPlan> — Owned execution plan or diagnostic.
/// @throws std::bad_alloc If allocation fails.
/// @throws std::length_error If owned storage exceeds its maximum size.
/// @throws std::filesystem::filesystem_error If a path operation fails.
/// @throws std::exception If a dependency or adapter throws another exception.
core::Result<ExecutionPlan>
prepare_outputs(const BuildOptions &options_value,
                const BuildContext &context_value, PreparedInputs inputs,
                const core::IProcessRunner &runner) {
  const auto &options = options_value;
  const auto &loaded = context_value.loaded;
  const auto &selected = context_value.selected;
  const auto &dependencies = context_value.dependencies;
  const bool is_workspace = context_value.is_workspace;
  auto &prepared = inputs.targets;
  auto &facts = inputs.facts;
  auto root = resolve_build_root(options, loaded, selected, is_workspace);
  if (!root)
    return std::unexpected(root.error());
  auto host = toolchain::probe_host_toolchain(
      {options.cc, options.cxx, options.ar}, runner);
  if (!host)
    return std::unexpected(host.error());

  for (auto &[member_index, fact] : facts) {
    const auto &library = *loaded.members[member_index].manifest.library;
    auto resolved = toolchain::resolve_system_libraries(
        *host, library.system_libraries, runner);
    if (!resolved)
      return std::unexpected(resolved.error());
    fact.system_libraries = std::move(*resolved);
  }
  // Output paths are deterministic within this invocation and feed the same
  // planner used by the native linker; no compiler starts before this phase
  // ends.
  std::string reserved = (*root / "amp-XXXXXX").string();
  if (!::mkdtemp(reserved.data()))
    return std::unexpected(
        invalid("cannot create private invocation directory"));
  const auto invocation = fs::path(reserved);
  for (auto &item : prepared) {
    item.directory =
        !is_workspace && prepared.size() == 1
            ? invocation
            : invocation /
                  (is_workspace ? "member-" + std::to_string(item.id.member)
                                : std::string{}) /
                  ((item.target.library ? "lib." : "") + item.target.name);
    if (item.directory != invocation && !fs::create_directories(item.directory))
      return std::unexpected(invalid("cannot create target directory"));
    if (item.target.library) {
      item.output =
          (item.directory / ("lib" + item.target.name + ".a")).string();
      facts[item.id.member].output = item.output;
    } else
      item.output = (item.directory / "artifact").string();
  }
  std::vector<build::ArchiveLinkFact> archive_facts;
  for (const auto &[_, fact] : facts)
    archive_facts.push_back(fact);
  std::vector<std::string> artifacts;
  for (auto id : selected) {
    auto item_it = std::find_if(
        prepared.begin(), prepared.end(),
        /// Finds the prepared selected target.
        /// @param[in] p const Prepared& — Candidate execution target.
        /// @return bool — True when its identity matches the selected ID.
        /// @note Does not throw.
        [&id](const auto &p) { return p.id == id; });
    if (item_it == prepared.end())
      return std::unexpected(invalid("selected target was not prepared"));
    auto &item = *item_it;
    artifacts.push_back(item.output);
    if (id.bin) {
      std::vector<std::string> direct;
      if (is_workspace)
        for (auto dep : dependencies.binary_edges[id.member][*id.bin])
          direct.push_back(member_id(dep));
      auto sdk = toolchain::resolve_system_libraries(*host, item.sdk, runner);
      if (!sdk)
        return std::unexpected(sdk.error());
      auto links = build::plan_link_inputs(direct, archive_facts, *sdk);
      if (!links)
        return std::unexpected(links.error());
      item.links = std::move(*links);
    }
  }

  return ExecutionPlan{std::move(prepared), invocation, std::move(artifacts),
                       std::move(*host)};
}
} // namespace

core::Result<StandaloneOutput>
run_standalone(std::span<const std::string_view> args) {
  auto options = parse_build_options(args);
  if (!options)
    return std::unexpected(options.error());
  if (options->help)
    return StandaloneOutput{{}, "", std::string(build_help())};
  try {
    core::LocalFileSystem filesystem;
    core::LocalProcessRunner runner;
    auto context = resolve_context(*options, filesystem);
    if (!context)
      return std::unexpected(context.error());
    auto inputs = prepare_inputs(*context, filesystem);
    if (!inputs)
      return std::unexpected(inputs.error());
    auto plan = prepare_outputs(*options, *context, std::move(*inputs), runner);
    if (!plan)
      return std::unexpected(plan.error());
    auto actions = execute_targets(plan->targets, context->is_workspace,
                                   plan->host, runner, filesystem);
    if (!actions)
      return std::unexpected(actions.error());
    auto db = publish_database(*actions, plan->invocation, filesystem);
    if (!db)
      return std::unexpected(db.error());
    return StandaloneOutput{std::move(plan->artifacts), std::move(*db), ""};
  } catch (const fs::filesystem_error &error) {
    return std::unexpected(invalid(error.what()));
  }
}
} // namespace amp::cli
