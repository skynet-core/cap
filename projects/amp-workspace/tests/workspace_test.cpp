#include <cassert>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

import amp.core.diagnostic;
import amp.core.filesystem;
import amp.manifest;
import amp.workspace;

namespace fs = std::filesystem;
using namespace amp;
class CountingFileSystem final : public core::IFileSystem {
public:
  mutable int calls = 0;
  core::LocalFileSystem local;
  core::Result<std::vector<std::byte>>
  read(const std::string &p) const override {
    ++calls;
    return local.read(p);
  }
  core::Status write_atomic(const std::string &p,
                            std::span<const std::byte> b) const override {
    ++calls;
    return local.write_atomic(p, b);
  }
  core::Result<std::vector<std::string>>
  enumerate(const std::string &p) const override {
    ++calls;
    return local.enumerate(p);
  }
  core::Result<std::string> canonicalize(const std::string &p) const override {
    ++calls;
    return local.canonicalize(p);
  }
};
static_assert(std::is_same_v<decltype(workspace::load_workspace(
                                 std::string_view{},
                                 std::declval<const core::IFileSystem &>())),
                             core::Result<workspace::Workspace>>);

void put(const fs::path &dir, const std::string &text) {
  fs::create_directories(dir);
  std::ofstream(dir / "amp.toml") << text;
}
std::string pkg(std::string name) {
  return "manifest-version = 1\n[package]\nname = \"" + name +
         "\"\nversion = \"1\"\n";
}
std::string ws(std::string members, std::string extra = "") {
  return "manifest-version = 1\n[workspace]\nmembers = [" + members + "]\n" +
         extra;
}
int main(int argc, char **argv) {
  assert(argc == 3);
  fs::path root = argv[1];
  assert(!fs::exists(root));
  fs::create_directories(root);
  core::LocalFileSystem io;
  CountingFileSystem counting;
  auto bad_start = workspace::load_workspace("", counting);
  assert(!bad_start &&
         bad_start.error().code == core::ErrorCode::invalid_input);
  bad_start = workspace::load_workspace(std::string_view("x\0y", 3), counting);
  assert(!bad_start &&
         bad_start.error().code == core::ErrorCode::invalid_input &&
         counting.calls == 0);
  put(root / "single",
      pkg("single") + "[[bin]]\nname = \"single\"\nentry = \"main.cpp\"\n");
  auto single = workspace::load_workspace((root / "single").string(), io);
  assert(single && single->members.size() == 1 &&
         single->members.front().manifest.bins.size() == 1);
  auto selected = workspace::select_packages(*single, {}, false);
  assert(selected && selected->size() == 1);
  auto shipped = workspace::load_workspace(argv[2], io);
  assert(shipped && shipped->members.size() == 2);
  selected = workspace::select_packages(*shipped, {}, false);
  assert(selected && selected->size() == 1 &&
         selected->front().manifest.package->name == "fixture-app");
  selected = workspace::select_packages(*shipped, {}, true);
  assert(selected && selected->size() == 2);
  auto member =
      workspace::load_workspace((fs::path(argv[2]) / "lib").string(), io);
  assert(member && member->root == shipped->root);
  selected = workspace::select_packages(*member, {}, false);
  assert(selected && selected->size() == 1 &&
         selected->front().root == member->current);
  assert(workspace::select_packages(*member, "fixture-app", false));
  assert(!workspace::select_packages(*member, "missing", false));
  assert(!workspace::select_packages(*member, "fixture-app", true));
  put(root / "tree",
      ws("\"members/*\"", "default-members = [\"members/a\"]\n"));
  put(root / "tree/members/a", pkg("a"));
  put(root / "tree/members/b", pkg("b"));
  auto tree = workspace::load_workspace((root / "tree").string(), io);
  assert(tree && tree->members.size() == 2 &&
         tree->members[0].manifest.package->name == "a");
  put(root / "tree/members/b", pkg("a"));
  assert(!workspace::load_workspace((root / "tree").string(), io));
  put(root / "tree/members/b", pkg("b"));
  put(root / "tree",
      ws("\"members/a\"", "default-members = [\"members/b\"]\n"));
  assert(!workspace::load_workspace((root / "tree").string(), io));
  put(root / "tree", ws("\"members/?\""));
  assert(!workspace::load_workspace((root / "tree").string(), io));
  fs::create_directories(root / "tree/empty");
  put(root / "tree", ws("\"empty/*\""));
  assert(!workspace::load_workspace((root / "tree").string(), io));
  put(root / "tree", ws("\"empty\""));
  assert(!workspace::load_workspace((root / "tree").string(), io));
  put(root / "tree",
      pkg("root") + "[workspace]\nmembers = [\"members/a\", \"members/b\"]\n"
                    "default-members = [\"members/b\", \"members/a\"]\n");
  auto mixed = workspace::load_workspace((root / "tree").string(), io);
  assert(mixed && mixed->members.size() == 3 && !mixed->current &&
         mixed->members.front().manifest.package->name == "root");
  selected = workspace::select_packages(*mixed, {}, false);
  assert(selected && selected->size() == 2 &&
         (*selected)[0].manifest.package->name == "a" &&
         (*selected)[1].manifest.package->name == "b");
  selected = workspace::select_packages(*mixed, "root", false);
  assert(selected && selected->size() == 1 &&
         selected->front().manifest.package->name == "root");
  auto mixed_member =
      workspace::load_workspace((root / "tree/members/b").string(), io);
  assert(mixed_member && mixed_member->current);
  selected = workspace::select_packages(*mixed_member, {}, false);
  assert(selected && selected->size() == 1 &&
         selected->front().manifest.package->name == "b");
  put(root / "tree",
      pkg("root") + "[workspace]\nmembers = [\"members/a\", \"members/b\"]\n");
  mixed = workspace::load_workspace((root / "tree").string(), io);
  assert(mixed && !mixed->current);
  selected = workspace::select_packages(*mixed, {}, false);
  assert(selected && selected->size() == 3 &&
         selected->front().manifest.package->name == "root");
  put(root / "tree", ws("\"members/a\", \"members/a\""));
  assert(!workspace::load_workspace((root / "tree").string(), io));
  put(root / "tree", ws("\"../single\""));
  assert(!workspace::load_workspace((root / "tree").string(), io));
  fs::create_directory_symlink(root / "single", root / "tree/escape");
  put(root / "tree", ws("\"escape\""));
  assert(!workspace::load_workspace((root / "tree").string(), io));
  put(root / "tree",
      ws("\"members/a\"",
         "exclude = [\"members/a\"]\ndefault-members = [\"members/a\"]\n"));
  assert(!workspace::load_workspace((root / "tree").string(), io));
  put(root / "tree", ws("\"members/a\"", "exclude = [\"members/b\"]\n"));
  assert(!workspace::load_workspace((root / "tree").string(), io));
  put(root / "tree", ws("\"members/a\""));
  assert(!workspace::load_workspace((root / "tree/members/b").string(), io));
  put(root / "tree/members/a", pkg("a") + "[workspace]\nmembers = []\n");
  assert(!workspace::load_workspace((root / "tree").string(), io));
  fs::remove_all(root);
}
