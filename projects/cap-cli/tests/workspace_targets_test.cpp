#include <cassert>
#include <optional>
#include <span>
#include <type_traits>
#include <vector>

import cap.core.diagnostic;
import cap.core.filesystem;
import cap.workspace;
import cap.workspace.dependencies;
import cap.cli.build_options;
import cap.cli.workspace_targets;

using namespace cap;
static_assert(
    std::is_same_v<decltype(cli::plan_workspace_targets(
                       std::declval<std::span<const cli::TargetId>>(),
                       std::declval<const workspace::DependencyPlan &>())),
                   core::Result<std::vector<cli::TargetId>>>);

int main(int argc, char **argv) {
  assert(argc == 2);
  core::LocalFileSystem io;
  auto shipped = workspace::load_workspace(argv[1], io);
  assert(shipped);
  auto packages = workspace::select_packages(*shipped, {}, true);
  assert(packages);
  auto plan = workspace::resolve_dependencies(*shipped, *packages, io);
  assert(plan);
  cli::BuildOptions options;
  auto default_selection = cli::select_build_targets(*shipped, options);
  assert(default_selection);
  auto default_targets = cli::plan_workspace_targets(*default_selection, *plan);
  assert(default_targets &&
         (*default_targets ==
          std::vector<cli::TargetId>{{0, std::nullopt}, {1, 0}, {1, 1}}));
  options.workspace = true;
  auto all_selection = cli::select_build_targets(*shipped, options);
  assert(all_selection);
  auto all_targets = cli::plan_workspace_targets(*all_selection, *plan);
  assert(all_targets &&
         (*all_targets ==
          std::vector<cli::TargetId>{{0, std::nullopt}, {1, 0}, {1, 1}}));
  options.workspace = false;
  options.bin = "first";
  auto bin_selection = cli::select_build_targets(*shipped, options);
  assert(bin_selection);
  auto bin_targets = cli::plan_workspace_targets(*bin_selection, *plan);
  assert(bin_targets && (*bin_targets == std::vector<cli::TargetId>{
                                             {0, std::nullopt}, {1, 0}}));

  // A's selected binary needs root and left; right is only used by its sibling
  // binary.
  workspace::DependencyPlan diamond{
      {{}, {0}, {0}, {1, 2}}, {{}, {}, {}, {{1}, {2}}}, {0, 1, 2, 3}};
  const std::vector<cli::TargetId> one_bin = {{3, 0}};
  auto selected = cli::plan_workspace_targets(one_bin, diamond);
  assert(selected);
  assert((*selected == std::vector<cli::TargetId>{
                           {0, std::nullopt}, {1, std::nullopt}, {3, 0}}));
  const std::vector<cli::TargetId> repeated_libraries = {
      {3, 0}, {0, std::nullopt}, {0, std::nullopt}};
  auto deduplicated = cli::plan_workspace_targets(repeated_libraries, diamond);
  assert(deduplicated &&
         (*deduplicated == std::vector<cli::TargetId>{
                               {0, std::nullopt}, {1, std::nullopt}, {3, 0}}));
  const std::vector<cli::TargetId> bad = {{4, 0}};
  assert(!cli::plan_workspace_targets(bad, diamond));
  auto missing_order = diamond;
  missing_order.build_order = {1, 2, 3};
  assert(!cli::plan_workspace_targets(one_bin, missing_order));
  auto duplicate_order = diamond;
  duplicate_order.build_order = {0, 1, 1, 2, 3};
  assert(!cli::plan_workspace_targets(one_bin, duplicate_order));
  auto bad_edge = diamond;
  bad_edge.edges[3] = {4};
  assert(!cli::plan_workspace_targets(one_bin, bad_edge));
}
