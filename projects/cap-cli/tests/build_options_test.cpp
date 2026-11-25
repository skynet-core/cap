#include <cassert>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

import cap.core.diagnostic;
import cap.core.filesystem;
import cap.workspace;
import cap.cli.build_options;

using namespace cap;
namespace fs = std::filesystem;

static_assert(std::is_same_v<decltype(cli::parse_build_options({})),
                             core::Result<cli::BuildOptions>>);
static_assert(std::is_same_v<decltype(cli::TargetId::member), std::size_t>);

/// Exercise pure option parsing and target selection against workspace
/// fixtures.
/// @param[in] argc int — Argument count, required to equal three.
/// @param[in] argv char** — Program name, shipped workspace and scratch path.
/// @return int — Zero on success; failed assertions abort.
/// @throws std::bad_alloc If test inputs or parsed results cannot allocate.
/// @throws std::length_error If test input or result size limits are exceeded.
/// @throws std::filesystem::filesystem_error If filesystem operations fail.
int main(int argc, char **argv) {
  assert(argc == 3);
  /// Parse argument views while their backing storage remains alive.
  /// @param[in] args std::vector<std::string_view> — Build argument tokens.
  /// @return core::Result<cli::BuildOptions> — Owned options or diagnostic.
  /// @throws std::bad_alloc If parsing or result storage cannot allocate.
  /// @throws std::length_error If a diagnostic exceeds the string size limit.
  /// @throws std::filesystem::filesystem_error If tool-path conversion fails.
  auto parse = [](std::vector<std::string_view> args) {
    return cli::parse_build_options(args);
  };
  const std::vector<std::string_view> base = {"--cc", "/cc",  "--cxx",
                                              "/cxx", "--ar", "/ar"};
  auto options = parse(base);
  assert(options && options->cc == "/cc");
  assert(parse({"--help"})->help);
  assert(cli::build_help().find("static libraries") != std::string_view::npos);
  assert(cli::build_help().find("--lib") != std::string_view::npos);
  assert(cli::build_help().find("--workspace") != std::string_view::npos);
  assert(!parse({}));
  assert(!parse({"--help", "--lib"}));
  assert(!parse({"--unknown"}));
  assert(!parse({"--cc"}));
  assert(!parse({"--cc", ""}));
  assert(!parse({"--cc", "--ar"}));
  assert(!parse({"--cc", "relative"}));
  assert(!parse({"--cc", std::string_view("/x\0y", 4)}));
  for (auto extra : {std::vector<std::string_view>{"--cc", "/x"},
                     {"--workspace", "--workspace"},
                     {"--lib", "--lib"},
                     {"--package", "x", "--package", "y"},
                     {"--package", "x", "--workspace"},
                     {"--bin", "x", "--lib"}}) {
    auto args = base;
    args.insert(args.end(), extra.begin(), extra.end());
    assert(!parse(args));
  }
  std::string owned = "original";
  auto args = base;
  args.insert(args.end(), {"--manifest-path", owned, "--build-root", "relative",
                           "--package", "p", "--bin", "b"});
  auto stored = parse(args);
  owned = "changed";
  assert(stored && *stored->manifest_path == "original" &&
         *stored->build_root == "relative" && *stored->package == "p" &&
         *stored->bin == "b");

  core::LocalFileSystem io;
  auto shipped = workspace::load_workspace(argv[1], io);
  assert(shipped && shipped->members.size() == 2);
  auto targets = cli::select_build_targets(*shipped, *options);
  assert(targets && !targets->empty());
  auto all = *options;
  all.workspace = true;
  auto all_targets = cli::select_build_targets(*shipped, all);
  assert(all_targets && all_targets->size() > targets->size());
  auto lib = all;
  lib.lib = true;
  auto libs = cli::select_build_targets(*shipped, lib);
  assert(libs && libs->size() == 1 && !libs->front().bin);
  auto member =
      workspace::load_workspace((fs::path(argv[1]) / "lib").string(), io);
  assert(member);
  auto member_targets = cli::select_build_targets(*member, *options);
  assert(member_targets && *member_targets == *libs);
  auto missing = *options;
  missing.package = "missing";
  assert(!cli::select_build_targets(*shipped, missing));
  missing = *options;
  missing.bin = "missing";
  assert(!cli::select_build_targets(*shipped, missing));
  assert(!cli::select_build_targets(*shipped, *parse({"--help"})));

  fs::path root = argv[2];
  assert(!fs::exists(root));
  fs::create_directories(root / "a");
  fs::create_directories(root / "b");
  std::ofstream(root / "cap.toml")
      << "manifest-version = 1\n[workspace]\nmembers = [\"a\", \"b\"]\n";
  for (auto name : {"a", "b"})
    std::ofstream(root / name / "cap.toml")
        << "manifest-version = 1\n[package]\nname = \"" << name
        << "\"\nversion = \"1\"\n[[bin]]\nname = \"shared\"\n"
           "entry = \"main.cpp\"\n[[bin]]\nname = \""
        << name << "\"\nentry = \"other.cpp\"\n";
  auto multi = workspace::load_workspace(root.string(), io);
  assert(multi);
  auto multi_targets = cli::select_build_targets(*multi, *options);
  assert(multi_targets && multi_targets->size() == 4);
  auto filter = *options;
  filter.bin = "shared";
  assert(!cli::select_build_targets(*multi, filter));
  filter.package = "a";
  assert((cli::select_build_targets(*multi, filter)->front() ==
          cli::TargetId{0, 0}));
  filter.lib = true;
  filter.bin.reset();
  assert(!cli::select_build_targets(*multi, filter));
  fs::path single = root;
  single += "-standalone";
  assert(!fs::exists(single));
  fs::create_directories(single);
  std::ofstream(single / "cap.toml")
      << "manifest-version = 1\n[package]\nname = \"single\"\n"
         "version = \"1\"\n[[bin]]\nname = \"one\"\nentry = \"one.cpp\"\n"
         "[[bin]]\nname = \"two\"\nentry = \"two.cpp\"\n";
  auto standalone = workspace::load_workspace(single.string(), io);
  assert(standalone);
  assert(cli::select_build_targets(*standalone, *options)->size() == 2);
  fs::remove_all(root);
  fs::remove_all(single);
}
