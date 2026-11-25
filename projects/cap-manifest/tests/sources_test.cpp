#include <cassert>
#include <cstddef>
#include <expected>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <type_traits>
#include <vector>

import cap.core.diagnostic;
import cap.core.filesystem;
import cap.manifest;
import cap.manifest.sources;

using namespace cap::manifest;
using Paths = std::vector<std::string>;
class FaultFileSystem final : public cap::core::IFileSystem {
public:
  cap::core::LocalFileSystem disk;
  std::string canonical_failure, enumerate_failure, invalid_entries;
  cap::core::Diagnostic failure() const {
    cap::core::Diagnostic d;
    d.code = cap::core::ErrorCode::transient_io;
    d.message = "injected failure";
    return d;
  }
  cap::core::Result<std::vector<std::byte>>
  read(const std::string &path) const override {
    return disk.read(path);
  }
  cap::core::Status
  write_atomic(const std::string &path,
               std::span<const std::byte> bytes) const override {
    return disk.write_atomic(path, bytes);
  }
  cap::core::Result<Paths> enumerate(const std::string &path) const override {
    if (path == enumerate_failure)
      return std::unexpected(failure());
    if (path == invalid_entries)
      return Paths{"../escape"};
    return disk.enumerate(path);
  }
  cap::core::Result<std::string>
  canonicalize(const std::string &path) const override {
    if (path == canonical_failure)
      return std::unexpected(failure());
    return disk.canonicalize(path);
  }
};
static_assert(
    std::is_same_v<decltype(select_sources(
                       std::declval<const std::string &>(),
                       std::declval<const Binary &>(),
                       std::declval<const cap::core::IFileSystem &>())),
                   cap::core::Result<Paths>>);
int main(int argc, char **argv) {
  assert(argc == 3);
  namespace fs = std::filesystem;
  fs::path root = fs::canonical(argv[1]);
  fs::create_directories(root / "source/sub");
  for (auto name : {"main.cpp", "other.cpp", "source/a.cpp",
                    "source/a_test.cpp", "source/a_windows.cpp",
                    "source/a_linux_arm64.cpp", "source/a_linux_x86_64.cpp",
                    "source/a_unknown.cpp", "source/sub/with space.cpp",
                    "source/sub/mod.cppm", "source/sub/file.txt"}) {
    std::ofstream(root / name) << "x";
  }
  if (!fs::is_symlink(root / "source/sub/cycle"))
    fs::create_directory_symlink(root / "source", root / "source/sub/cycle");
  if (!fs::is_symlink(root / "outside"))
    fs::create_directory_symlink(root.parent_path(), root / "outside");
  cap::core::LocalFileSystem disk;
  auto r = root.string();
  Library lib;
  lib.sources = {"source/**/*.cpp", "source/a.cpp", "source/sub/*.cppm"};
  auto selected = select_sources(r, lib, disk);
  assert(selected &&
         *selected == (Paths{"source/a.cpp", "source/a_linux_x86_64.cpp",
                             "source/a_unknown.cpp", "source/sub/mod.cppm",
                             "source/sub/with space.cpp"}));
  Binary a;
  a.entry = "main.cpp";
  a.sources = {"source/sub/with space.cpp"};
  Binary b;
  b.entry = "other.cpp";
  assert(*select_sources(r, a, disk) ==
         (Paths{"main.cpp", "source/sub/with space.cpp"}));
  assert(*select_sources(r, b, disk) == (Paths{"other.cpp"}));
  a.exclude = {"main.cpp"};
  auto excluded_entry = select_sources(r, a, disk);
  assert(!excluded_entry &&
         excluded_entry.error().message == "binary entry excluded: main.cpp");
  for (auto bad : {"../x.cpp", "/x.cpp", "source//a.cpp", "source/./a.cpp",
                   "source/[a].cpp", "source/**x.cpp", "source/**",
                   "missing.cpp", "outside/a.cpp", "source/sub/file.txt",
                   "source/a_test.cpp", "source/a_windows.cpp"}) {
    b.entry = bad;
    assert(!select_sources(r, b, disk));
  }
  b.entry = std::string("main.cpp\0bad", 12);
  assert(!select_sources(r, b, disk));
  lib.sources = {"source/*.none"};
  assert(!select_sources(r, lib, disk));
  lib.sources = {"outside/*.cpp"};
  assert(!select_sources(r, lib, disk));
  assert(!select_sources(r + "/.", lib, disk));
  for (auto pattern :
       {"source/**", "source/[x].cpp", "source", "source/*.ccx"}) {
    lib.sources = {pattern};
    assert(!select_sources(r, lib, disk));
  }
  lib.sources = {"source/a*.cpp"};
  assert(select_sources(r, lib, disk));
  assert(!select_sources("", lib, disk));
  assert(!select_sources(r + "/missing", lib, disk));
  b.entry = "main.cpp";
  b.exclude = {"bad/**"};
  assert(!select_sources(r, b, disk));
  fs::path fixture = fs::canonical(argv[2]);
  for (auto package : {"app", "lib"}) {
    auto path = fixture / package;
    std::ifstream input(path / "cap.toml");
    assert(input);
    std::string text{std::istreambuf_iterator<char>(input), {}};
    auto manifest = parse_manifest(text, (path / "cap.toml").string());
    assert(manifest);
    if (manifest->library)
      assert(*select_sources(path.string(), *manifest->library, disk) ==
             (Paths{"src/fixture.cpp"}));
    else {
      assert(manifest->bins.size() == 2);
      assert(*select_sources(path.string(), manifest->bins[0], disk) ==
             (Paths{"src space/first.cpp", "src space/platform_linux.cpp"}));
      assert(*select_sources(path.string(), manifest->bins[1], disk) ==
             (Paths{"src space/second.cpp"}));
    }
  }
  FaultFileSystem fault;
  lib.sources = {"*.cpp"};
  fault.invalid_entries = r;
  auto bad_entries = select_sources(r, lib, fault);
  assert(!bad_entries &&
         bad_entries.error().code == cap::core::ErrorCode::invalid_input);
  fault.invalid_entries.clear();
  lib.sources = {"main.cpp"};
  for (auto path : {r, r + "/main.cpp"}) {
    fault.canonical_failure = path;
    auto failed = select_sources(r, lib, fault);
    assert(!failed &&
           failed.error().code == cap::core::ErrorCode::transient_io);
  }
  fault.canonical_failure.clear();
  fault.enumerate_failure = r + "/main.cpp";
  auto file_failure = select_sources(r, lib, fault);
  assert(!file_failure &&
         file_failure.error().code == cap::core::ErrorCode::transient_io);
  fault.enumerate_failure = r;
  lib.sources = {"*.cpp"};
  auto root_failure = select_sources(r, lib, fault);
  assert(!root_failure &&
         root_failure.error().code == cap::core::ErrorCode::transient_io);
  fault.enumerate_failure = r + "/source/sub";
  lib.sources = {"source/**/*.cpp"};
  auto failed = select_sources(r, lib, fault);
  assert(!failed && failed.error().code == cap::core::ErrorCode::transient_io);
  for (auto [alias, target] :
       {std::pair{"alias-test.cpp", "source/a_test.cpp"},
        std::pair{"alias-os.cpp", "source/a_windows.cpp"}}) {
    if (!fs::is_symlink(root / alias))
      fs::create_symlink(root / target, root / alias);
    b.entry = alias;
    b.exclude.clear();
    auto result = select_sources(r, b, disk);
    assert(
        !result && result.error().code == cap::core::ErrorCode::invalid_input &&
        result.error().message.find("source file alias") != std::string::npos);
  }
  lib.sources = {"alias-*.cpp"};
  auto alias_result = select_sources(r, lib, disk);
  assert(!alias_result && alias_result.error().message.find(
                              "source file alias") != std::string::npos);
  if (!fs::is_symlink(root / "dir-alias"))
    fs::create_directory_symlink(root / "source", root / "dir-alias");
  lib.sources = {"dir-alias/a.cpp"};
  assert(*select_sources(r, lib, disk) == (Paths{"dir-alias/a.cpp"}));
}
