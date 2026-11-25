#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <type_traits>

import amp.core.diagnostic;
import amp.manifest;

using amp::manifest::parse_manifest;
/// Compile-time check that the exported parser retains its structured result.
/// @return void — No runtime value or effect; compilation fails if the type
/// contract changes.
static_assert(std::is_same_v<decltype(parse_manifest({}, {})),
                             amp::core::Result<amp::manifest::Manifest>>);

/// Reads a fixture file into an owned string for parser regression assertions.
/// @param[in] path const char* — Path to an existing readable fixture file.
/// @return std::string — Entire file contents, including line endings.
/// @note The test asserts that opening succeeds; stream/allocation failures
/// are not converted and may terminate the test.
std::string read(const char *path) {
  std::ifstream in(path);
  assert(in);
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}
/// Runs inline parser regressions and parses the shipped workspace fixtures.
/// @param[in] argc int — Process argument count; must be 2.
/// @param[in] argv char** — Process arguments; argv[1] names the fixture
/// workspace directory.
/// @return int — Zero when every assertion passes; assertion failure aborts
/// the test process.
/// @throws std::bad_alloc, std::length_error — Allocation/size failures in
/// fixture loading or parser operations are not caught.
int main(int argc, char **argv) {
  assert(argc == 2);
  auto all = parse_manifest(
      "manifest-version=1\r\n[package]\r\nname=\"pkg\"\r\nversion=\"1\"\r\n"
      "publish=true\r\nlicense=\"a\\\"b\\\\c\\n\\r\\t\\b\\f\"\r\n"
      "[language]\r\nc=\"c23\"\r\n[workspace]\r\nmembers=[]\r\nexclude=[]\r\n"
      "[lib]\r\nname=\"lib\"\r\nsources=[]\r\nheaders=[]\r\n"
      "public-include-dirs=[]\r\nsystem-libraries=[\"z\"]\r\n"
      "[[bin]]\r\nname=\"exe\"\r\nentry=\"main.cpp\"\r\n"
      "exclude=[]\r\ninclude-dirs=[]\r\n",
      "all");
  assert(all && all->package->publish == true && all->language.c == "c23");
  assert(all->library->system_libraries[0] == "z");
  auto late_package = parse_manifest(
      "manifest-version=1\n[workspace]\nmembers=[]\n[language]\ncpp=\"c++26\"\n"
      "[dependencies]\na={path=\"../a\"}\n[lib]\nname=\"lib\"\nsources=[]\n"
      "[[bin]]\nname=\"app\"\nentry=\"x\"\n[package]\nname=\"pkg\"\nversion="
      "\"1\"\n",
      "late");
  assert(late_package && late_package->workspace && late_package->package);
  for (std::string_view section :
       {"[language]\n", "[dependencies]\n", "[lib]\nname=\"x\"\nsources=[]\n",
        "[[bin]]\nname=\"x\"\nentry=\"x\"\n"}) {
    auto orphan = parse_manifest(
        std::string("manifest-version=1\n[workspace]\nmembers=[]\n") +
            std::string(section),
        "orphan");
    assert(!orphan && orphan.error().span.line == 4 &&
           orphan.error().span.column == 1);
  }
  auto bare_cr = parse_manifest(
      "manifest-version=1\n[workspace]\nmembers=[\"x\",\r\"y\"]\n", "cr");
  assert(!bare_cr && bare_cr.error().span.line == 3);
  assert(!parse_manifest(
      "manifest-version=1\n[package]\nname=\"x\"\nversion=\"1\"\n[[bin]]\nname="
      "\"x\"\nentry=\"x\"\nsystem-libraries=[\"-bad\"]",
      "bad"));
  std::string root = argv[1];
  auto workspace =
      parse_manifest(read((root + "/amp.toml").c_str()), "workspace");
  assert(workspace && workspace->workspace &&
         workspace->workspace->members.size() == 2);
  auto library = parse_manifest(read((root + "/lib/amp.toml").c_str()), "lib");
  assert(library && library->library && library->library->type == "static");
  auto app = parse_manifest(read((root + "/app/amp.toml").c_str()), "app");
  assert(app && app->bins.size() == 2 &&
         app->bins[0].entry == "src space/first.cpp");
  assert(app->dependencies.size() == 1 &&
         app->dependencies[0].path == "../lib");
  auto one =
      parse_manifest("manifest-version=1\n[package]\nname=\"demo\"\nversion="
                     "\"1\"\n[[bin]]\nname=\"app\"\nentry=\"a.cpp\"\nsystem-"
                     "libraries=[\n\"a\", # comment\n\"b\",\n]\n",
                     "one");
  assert(one && one->bins[0].system_libraries[1] == "b");
  for (std::string_view bad :
       {"", "manifest-version=2",
        "manifest-version=1\n[package]\nname=\"../bad\"\nversion=\"1\"",
        "manifest-version=1\n[workspace]\nmembers=[]\nmembers=[]",
        "manifest-version=1\n[workspace]\nmembers=[]\n[workspace]\nmembers=[]",
        "manifest-version=1\n[workspace]\nmembers=3",
        "manifest-version=1\n[workspace]\nmembers=[]\nunknown=true",
        "manifest-version=1\n[package]\nname=\"x\"\nversion=\"\"",
        ("manifest-version=1\n[package]\nname=\"x\"\nversion=\"1\"\n[[bin]]"
         "\nname=\"x\"\nentry=\"x\"\n[[bin]]\nname=\"x\"\nentry=\"y\""),
        "manifest-version=1\n[workspace]\nmembers=[\"\\q\"]",
        ("manifest-version=1\n[workspace]\nmembers=[]\n[dependencies]\nx={git="
         "\"x\"}")}) {
    auto result = parse_manifest(bad, "bad.toml");
    assert(!result && result.error().span.file == "bad.toml" &&
           result.error().span.line > 0 && result.error().span.column > 0);
  }
  std::string nul = "manifest-version=1\n";
  nul.push_back('\0');
  assert(!parse_manifest(nul, "nul"));
}
