#include <cassert>
#include <filesystem>
#include <fstream>
#include <string>
#include <type_traits>
#include <vector>

import amp.core.diagnostic;
import amp.core.filesystem;
import amp.manifest;
import amp.workspace;
import amp.workspace.dependencies;

namespace fs = std::filesystem;
using namespace amp;
/// Confirms the exported resolver's exact result type at compile time.
static_assert(std::is_same_v<
              decltype(workspace::resolve_dependencies(
                  std::declval<const workspace::Workspace &>(),
                  std::declval<const std::vector<workspace::PackageMember> &>(),
                  std::declval<const core::IFileSystem &>())),
              core::Result<workspace::DependencyPlan>>);

/// Creates a directory and writes its manifest fixture.
/// @param[in] dir const fs::path& — Directory to create.
/// @param[in] text const std::string& — Manifest contents written as amp.toml.
/// @return void — Creates the fixture; stream failure is not checked.
/// @throws Any exception escaping filesystem path or directory operations.
void put(const fs::path &dir, const std::string &text) {
  fs::create_directories(dir);
  std::ofstream(dir / "amp.toml") << text;
}
/// Builds minimal package manifest text for graph fixtures.
/// @param[in] name const std::string& — Package name.
/// @param[in] extra const std::string& — Optional manifest sections appended
/// verbatim.
/// @return std::string — Manifest text with version and package identity.
/// @throws Any exception escaping string construction.
std::string package(const std::string &name, const std::string &extra = "") {
  return "manifest-version = 1\n[package]\nname = \"" + name +
         "\"\nversion = \"1\"\n" + extra;
}
/// Exercises dependency graph validation, closure, ordering, and filesystem
/// failures.
/// @param[in] argc int — Process argument count; expected to be three.
/// @param[in] argv char** — Arguments: executable, fresh scratch directory,
/// shipped fixture.
/// @return int — Zero when all assertions pass; assertion failure aborts the
/// process.
/// @throws Any exception escaping filesystem, workspace, or resolver
/// operations.
int main(int argc, char **argv) {
  assert(argc == 3);
  core::LocalFileSystem io;
  auto shipped = workspace::load_workspace(argv[2], io);
  assert(shipped);
  auto selected = workspace::select_packages(*shipped, {}, false);
  assert(selected);
  auto result = workspace::resolve_dependencies(*shipped, *selected, io);
  assert(result && result->build_order == std::vector<std::size_t>({0, 1}));
  assert(result->edges[1] == std::vector<std::size_t>({0}));
  assert(result->binary_edges[1].size() == 2 &&
         result->binary_edges[1][0] == result->edges[1]);
  fs::path root = argv[1];
  assert(!fs::exists(root));
  fs::create_directories(root);
  put(root, "manifest-version = 1\n[workspace]\nmembers = [\"app\", \"a\", "
            "\"b\", \"c\", \"unused\"]\n");
  put(root / "app",
      package("app", "[dependencies]\nb = {path=\"../b\"}\n"
                     "a = {path=\"../a\"}\n"
                     "[[bin]]\nname=\"app\"\nentry=\"main.cpp\"\n"));
  put(root / "a", package("a", "[lib]\nname=\"a\"\nsources=[]\n[dependencies]\n"
                               "c = {path=\"../c\"}\n"));
  put(root / "b", package("b", "[lib]\nname=\"b\"\nsources=[]\n[dependencies]\n"
                               "c = {path=\"../c\"}\n"));
  put(root / "c", package("c", "[lib]\nname=\"c\"\nsources=[]\n"));
  put(root / "unused", package("unused"));
  auto ws = workspace::load_workspace(root.string(), io);
  assert(ws);
  /// Resolves a selection against the fixture workspace and injected
  /// filesystem.
  /// @param[in] w const workspace::Workspace& — Workspace being resolved.
  /// @param[in] s const std::vector<workspace::PackageMember>& — Requested
  /// roots.
  /// @return core::Result<workspace::DependencyPlan> — Resolver result with
  /// structured errors.
  /// @throws Any exception escaping resolve_dependencies.
  auto resolve = [&](const workspace::Workspace &w,
                     const std::vector<workspace::PackageMember> &s) {
    return workspace::resolve_dependencies(w, s, io);
  };
  std::vector<workspace::PackageMember> roots{ws->members[0]};
  result = resolve(*ws, roots);
  assert(result && result->edges[0] == std::vector<std::size_t>({2, 1}));
  assert(result->build_order == std::vector<std::size_t>({3, 2, 1, 0}));
  auto repeated = resolve(*ws, roots);
  assert(repeated && repeated->build_order == result->build_order &&
         repeated->edges == result->edges);
  roots.push_back(ws->members[1]);
  roots.push_back(ws->members[0]);
  result = resolve(*ws, roots);
  assert(result &&
         result->build_order == std::vector<std::size_t>({3, 2, 1, 0}));
  auto changed = *ws;
  changed.members[0].manifest.bins[0].depends_on = {"a"};
  result = resolve(changed, {changed.members[0]});
  assert(result && result->build_order == std::vector<std::size_t>({3, 1, 0}));
  changed.members[0].manifest.bins[0].depends_on = {"missing"};
  assert(!resolve(changed, {changed.members[0]}));
  changed.members[0].manifest.bins[0].depends_on = {"a", "a"};
  assert(!resolve(changed, {changed.members[0]}));
  changed = *ws;
  changed.members[3].manifest.dependencies.push_back({"a", "../a"});
  assert(!resolve(changed, {changed.members[4]}));
  changed = *ws;
  changed.members[0].manifest.dependencies[0].path = "../missing";
  auto missing = resolve(changed, {changed.members[0]});
  assert(!missing && missing.error().code == core::ErrorCode::missing_resource);
  changed = *ws;
  changed.members[0].manifest.dependencies[0].name = "wrong";
  assert(!resolve(changed, {changed.members[0]}));
  changed = *ws;
  changed.members[2].manifest.library.reset();
  assert(!resolve(changed, {changed.members[0]}));
  changed = *ws;
  changed.members[2].manifest.library->type = "shared";
  assert(!resolve(changed, {changed.members[0]}));
  changed = *ws;
  changed.members[0].manifest.dependencies.push_back({"a", "../a"});
  assert(!resolve(changed, {changed.members[0]}));
  changed = *ws;
  changed.members[0].manifest.dependencies[0].path = "";
  assert(!resolve(changed, {changed.members[0]}));
  changed.members[0].manifest.dependencies[0].path = "/tmp";
  assert(!resolve(changed, {changed.members[0]}));
  changed.members[0].manifest.dependencies[0].path = std::string("a\0b", 3);
  assert(!resolve(changed, {changed.members[0]}));
  changed.members[0].manifest.dependencies[0].path = "../unused";
  assert(!resolve(changed, {changed.members[0]}));
  fs::create_directories(root / "nonmember");
  changed.members[0].manifest.dependencies[0].path = "../nonmember";
  assert(!resolve(changed, {changed.members[0]}));
  fs::create_directory_symlink(root / "nonmember", root / "alias");
  changed.members[0].manifest.dependencies[0].path = "../alias";
  assert(!resolve(changed, {changed.members[0]}));
  fs::create_directory_symlink(root.parent_path(), root / "outside");
  changed.members[0].manifest.dependencies[0].path = "../outside";
  assert(!resolve(changed, {changed.members[0]}));
  auto foreign = ws->members[0];
  foreign.root += "-unknown";
  assert(!resolve(*ws, {foreign}));
  foreign = ws->members[0];
  foreign.manifest.package->name = "wrong";
  assert(!resolve(*ws, {foreign}));
  fs::path standalone = root.string() + "-standalone";
  assert(!fs::exists(standalone));
  put(standalone, package("single"));
  auto single = workspace::load_workspace(standalone.string(), io);
  assert(single);
  result = resolve(*single, {single->members[0]});
  assert(result && result->build_order == std::vector<std::size_t>({0}));
  fs::remove_all(standalone);
  result = resolve(*ws, {ws->members[4]});
  assert(result && result->build_order == std::vector<std::size_t>({4}));
  fs::remove_all(root);
}
