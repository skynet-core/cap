module;
#include <algorithm>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <functional>
#include <string>
#include <utility>
#include <vector>

export module cap.workspace.dependencies;
import cap.core.diagnostic;
import cap.core.filesystem;
import cap.workspace;

export namespace cap::workspace {
/// Resolved local package dependency graph for one workspace.
/// @invariant Every index refers to the corresponding workspace member; edges
///             retain manifest declaration order.
/// @note Indices are valid only while used with the workspace whose member
///       ordering was passed to resolve_dependencies.
struct DependencyPlan {
  /// Direct package dependency indices, grouped by declaring workspace member.
  std::vector<std::vector<std::size_t>> edges;
  /// Binary dependency indices, grouped by member then by manifest binary.
  std::vector<std::vector<std::vector<std::size_t>>> binary_edges;
  /// Selected members and prerequisites in dependency-first order, without
  /// duplicates.
  std::vector<std::size_t> build_order;
};
/// Validates local dependency declarations and computes the selected build
/// closure.
/// @param[in] workspace const Workspace& — Validated workspace members and
/// manifests.
/// @param[in] selected const std::vector<PackageMember>& — Requested members,
/// matching root and package name.
/// @param[in] fs const core::IFileSystem& — Injected filesystem for path
/// canonicalization.
/// @return core::Result<DependencyPlan> — Graph on success; invalid_input for
/// malformed or invalid graph
///         declarations/selections; preserves filesystem diagnostics.
/// @throws Any exception escaping filesystem canonicalization or allocation.
core::Result<DependencyPlan>
resolve_dependencies(const Workspace &workspace,
                     const std::vector<PackageMember> &selected,
                     const core::IFileSystem &fs);
} // namespace cap::workspace

namespace cap::workspace {
namespace {
/// Creates an invalid-input diagnostic with no location or causal chain.
/// @param[in] message std::string — Human-readable failure detail.
/// @return core::Diagnostic — Error-severity invalid_input diagnostic.
/// @throws Any exception escaping string or diagnostic construction.
core::Diagnostic invalid(std::string message) {
  return {core::ErrorCode::invalid_input,
          core::Severity::error,
          std::move(message),
          {},
          {},
          {}};
}
/// Tests whether child has root as a path-component prefix.
/// @param[in] root const std::filesystem::path& — Candidate containing path.
/// @param[in] child const std::filesystem::path& — Candidate contained path.
/// @return bool — True when every root component matches the corresponding
/// child component.
/// @throws Any exception escaping path component comparison.
bool inside(const std::filesystem::path &root,
            const std::filesystem::path &child) {
  auto a = root.begin(), b = child.begin();
  for (; a != root.end(); ++a, ++b)
    if (b == child.end() || *a != *b)
      return false;
  return true;
}
} // namespace

/// Resolves workspace-local dependencies and selected build ordering.
/// @param[in] workspace const Workspace& — Workspace and ordered member set.
/// @param[in] selected const std::vector<PackageMember>& — Selected members to
/// build.
/// @param[in] fs const core::IFileSystem& — Filesystem used to canonicalize
/// dependency paths.
/// @return core::Result<DependencyPlan> — Plan on success; invalid_input for
/// dependency or selection
///         violations, otherwise the original filesystem diagnostic.
/// @throws Any exception escaping filesystem operations or allocation.
core::Result<DependencyPlan>
resolve_dependencies(const Workspace &workspace,
                     const std::vector<PackageMember> &selected,
                     const core::IFileSystem &fs) {
  const auto &members = workspace.members;
  DependencyPlan plan;
  plan.edges.resize(members.size());
  plan.binary_edges.resize(members.size());
  for (std::size_t i = 0; i < members.size(); ++i) {
    const auto &member = members[i];
    for (const auto &dep : member.manifest.dependencies) {
      std::filesystem::path relative(dep.path);
      if (dep.path.empty() || dep.path.find('\0') != std::string::npos ||
          relative.is_absolute())
        return std::unexpected(invalid("invalid dependency path: " + dep.path));
      auto target = fs.canonicalize(
          (std::filesystem::path(member.root) / relative).string());
      if (!target)
        return std::unexpected(target.error());
      if (!inside(workspace.root, *target))
        return std::unexpected(
            invalid("dependency escapes workspace: " + dep.path));
      auto found = std::find_if(
          members.begin(), members.end(),
          /// Finds the workspace member at the canonical dependency root.
          /// @param[in] m const auto& — Candidate package member.
          /// @return bool — True when its root equals the canonical target.
          [&](const auto &m) { return m.root == *target; });
      if (found == members.end())
        return std::unexpected(
            invalid("dependency is not a member: " + dep.path));
      auto index = static_cast<std::size_t>(found - members.begin());
      if (found->manifest.package->name != dep.name)
        return std::unexpected(
            invalid("dependency name mismatch: " + dep.name));
      if (!found->manifest.library || found->manifest.library->type != "static")
        return std::unexpected(
            invalid("dependency has no static library: " + dep.name));
      auto &edges = plan.edges[i];
      if (std::find(edges.begin(), edges.end(), index) != edges.end())
        return std::unexpected(invalid("duplicate dependency: " + dep.name));
      edges.push_back(index);
    }
    for (const auto &bin : member.manifest.bins) {
      std::vector<std::size_t> edges;
      if (bin.depends_on.empty()) {
        edges = plan.edges[i];
      } else {
        for (const auto &name : bin.depends_on) {
          auto found = std::find_if(
              plan.edges[i].begin(), plan.edges[i].end(),
              /// Matches a declared dependency by its target package name.
              /// @param[in] j std::size_t — Candidate dependency member index.
              /// @return bool — True when the candidate package name equals the
              /// selector.
              [&](std::size_t j) {
                return members[j].manifest.package->name == name;
              });
          if (found == plan.edges[i].end())
            return std::unexpected(
                invalid("unknown binary dependency: " + name));
          if (std::find(edges.begin(), edges.end(), *found) != edges.end())
            return std::unexpected(
                invalid("duplicate binary dependency: " + name));
          edges.push_back(*found);
        }
      }
      plan.binary_edges[i].push_back(std::move(edges));
    }
  }
  std::vector<unsigned char> state(members.size());
  /// Detects a dependency cycle by tri-color depth-first traversal.
  /// @param[in] i std::size_t — Workspace member index to visit.
  /// @return bool — True when a back edge reaches an active member.
  /// @throws Any exception escaping recursive function invocation.
  std::function<bool(std::size_t)> cycle = [&](std::size_t i) {
    if (state[i] == 1)
      return true;
    if (state[i] == 2)
      return false;
    state[i] = 1;
    for (auto j : plan.edges[i])
      if (cycle(j))
        return true;
    state[i] = 2;
    return false;
  };
  for (std::size_t i = 0; i < members.size(); ++i)
    if (cycle(i))
      return std::unexpected(invalid("local dependency cycle"));

  std::vector<std::size_t> roots;
  for (const auto &selection : selected) {
    auto found = std::find_if(
        members.begin(), members.end(),
        /// Matches a selected identity to a workspace member.
        /// @param[in] m const auto& — Candidate package member.
        /// @return bool — True when root and package name match.
        [&](const auto &m) {
          return m.root == selection.root && selection.manifest.package &&
                 m.manifest.package->name == selection.manifest.package->name;
        });
    if (found == members.end())
      return std::unexpected(invalid(
          "selected package is not a workspace member: " + selection.root));
    roots.push_back(static_cast<std::size_t>(found - members.begin()));
  }
  std::fill(state.begin(), state.end(), 0);
  /// Appends one reachable member after visiting its applicable prerequisites.
  /// @param[in] i std::size_t — Workspace member index to include.
  /// @return void — Appends each reachable member once to the build order.
  /// @throws Any exception escaping recursive invocation or vector growth.
  std::function<void(std::size_t)> visit = [&](std::size_t i) {
    if (state[i])
      return;
    state[i] = 1;
    if (members[i].manifest.library || members[i].manifest.bins.empty()) {
      for (auto j : plan.edges[i])
        visit(j);
    } else {
      for (const auto &edges : plan.binary_edges[i])
        for (auto j : edges)
          visit(j);
    }
    plan.build_order.push_back(i);
  };
  for (auto i : roots)
    visit(i);
  return plan;
}
} // namespace cap::workspace
