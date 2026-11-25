module;
#include <algorithm>
#include <cstddef>
#include <expected>
#include <span>
#include <string>
#include <utility>
#include <vector>

export module cap.cli.workspace_targets;
import cap.core.diagnostic;
import cap.workspace.dependencies;
import cap.cli.build_options;

export namespace cap::cli {
/// Plans selected workspace targets and the libraries they require.
/// @param[in] selected span<const TargetId> — Selected IDs for the same
/// validated workspace.
/// @param[in] dependencies const workspace::DependencyPlan& — Validated
/// dependency plan for that workspace.
/// @return core::Result<std::vector<TargetId>> — Required libraries in build
/// order, then selected binaries in input order; invalid shape or indexes
/// return invalid_input, including any required library missing from
/// `build_order`.
/// @throws std::bad_alloc if result storage or a diagnostic cannot be
/// allocated.
/// @throws std::length_error if result storage exceeds the container limit.
core::Result<std::vector<TargetId>>
plan_workspace_targets(std::span<const TargetId> selected,
                       const workspace::DependencyPlan &dependencies);
} // namespace cap::cli

namespace cap::cli {
namespace {
/// Creates the invalid-input diagnostic used for malformed dependency plans.
/// @param[in] message std::string — Diagnostic message.
/// @return core::Diagnostic — Error diagnostic.
/// @throws std::bad_alloc if allocating the message fails.
core::Diagnostic invalid(std::string message) {
  return {core::ErrorCode::invalid_input,
          core::Severity::error,
          std::move(message),
          {},
          {},
          {}};
}
} // namespace

core::Result<std::vector<TargetId>>
plan_workspace_targets(std::span<const TargetId> selected,
                       const workspace::DependencyPlan &dependencies) {
  const auto count = dependencies.edges.size();
  if (dependencies.binary_edges.size() != count)
    return std::unexpected(invalid("dependency plan member shape mismatch"));
  std::vector<unsigned char> order_seen(count), needed(count);
  for (auto member : dependencies.build_order) {
    if (member >= count || order_seen[member]++)
      return std::unexpected(invalid("invalid dependency build order"));
  }
  for (std::size_t i = 0; i < count; ++i) {
    for (auto dependency : dependencies.edges[i])
      if (dependency >= count)
        return std::unexpected(invalid("invalid library dependency index"));
    for (const auto &edges : dependencies.binary_edges[i])
      for (auto dependency : edges)
        if (dependency >= count)
          return std::unexpected(invalid("invalid binary dependency index"));
  }
  for (const auto &target : selected)
    if (target.member >= count ||
        (target.bin &&
         *target.bin >= dependencies.binary_edges[target.member].size()))
      return std::unexpected(invalid("invalid selected target index"));

  std::vector<std::size_t> pending;
  for (const auto &target : selected) {
    if (!target.bin) {
      pending.push_back(target.member);
    } else {
      for (auto dependency :
           dependencies.binary_edges[target.member][*target.bin])
        pending.push_back(dependency);
    }
  }
  while (!pending.empty()) {
    auto member = pending.back();
    pending.pop_back();
    if (needed[member])
      continue;
    needed[member] = 1;
    pending.insert(pending.end(), dependencies.edges[member].begin(),
                   dependencies.edges[member].end());
  }
  for (std::size_t member = 0; member < count; ++member)
    if (needed[member] && !order_seen[member])
      return std::unexpected(
          invalid("required library missing from build order"));
  std::vector<TargetId> result;
  for (auto member : dependencies.build_order)
    if (needed[member])
      result.push_back({member, std::nullopt});
  for (const auto &target : selected)
    if (target.bin)
      result.push_back(target);
  return result;
}
} // namespace cap::cli
