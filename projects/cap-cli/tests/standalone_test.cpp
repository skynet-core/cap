#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

import cap.core.diagnostic;
import cap.cli.standalone;

namespace {
namespace fs = std::filesystem;
/// Write fixture text to path, asserting that the operation succeeds.
/// @param[in] path const fs::path& — Destination fixture file.
/// @param[in] text const std::string& — Bytes to write.
/// @return void — No value; overwrites the fixture or fails an assertion.
/// @throws std::exception If stream construction or allocation fails.
void write(const fs::path &path, const std::string &text) {
  std::ofstream out(path);
  assert(out);
  out << text;
  assert(out);
}
/// Read fixture bytes from path, asserting that the file opens.
/// @param[in] path const fs::path& — Fixture file to read.
/// @return std::string — Owned contents; assertion fails if opening fails.
/// @throws std::exception If stream construction or allocation fails.
std::string read(const fs::path &path) {
  std::ifstream in(path);
  assert(in);
  return {std::istreambuf_iterator<char>(in), {}};
}
/// Return a fixture manifest with the supplied sections and binary entry.
/// @param[in] extra const std::string& — Text inserted before the binary.
/// @param[in] bin_extra const std::string& — Additional binary fields.
/// @param[in] entry const std::string& — Binary entry path.
/// @return std::string — Owned fixture text; no failure result.
/// @throws std::bad_alloc If constructing the manifest fails.
/// @throws std::length_error If the result exceeds the string size limit.
std::string manifest(const std::string &extra = "",
                     const std::string &bin_extra = "",
                     const std::string &entry = "main.cpp") {
  return "manifest-version = 1\n[package]\nname = \"example\"\n"
         "version = \"1\"\n" +
         extra + "\n[[bin]]\nname = \"example\"\nentry = \"" + entry + "\"\n" +
         bin_extra + "\n";
}
} // namespace

/// Exercise standalone preflight and real builds using three explicit tools.
/// @param[in] argc int — Argument count; must equal four.
/// @param[in] argv char** — Program name, C compiler, C++ compiler and ar.
/// @return int — Zero when all assertions pass; assertion failure aborts.
/// @throws std::exception If filesystem, IO or allocation operations fail.
int main(int argc, char **argv) {
  assert(argc == 4);
  static_assert(std::is_same_v<decltype(cap::cli::StandaloneOutput{}.artifacts),
                               std::vector<std::string>>);
  /// const std::string_view[] — Standalone help request.
  const std::string_view help[] = {"--help"};
  auto result = cap::cli::run_standalone(help);
  assert(result && !result->help.empty() && result->artifacts.empty());
  /// const std::string_view[] — Unsupported-option request.
  const std::string_view invalid[] = {"--unsupported"};
  result = cap::cli::run_standalone(invalid);
  assert(!result && result.error().code == cap::core::ErrorCode::invalid_input);

  std::string scratch =
      (fs::temp_directory_path() / "cap-cli-test-XXXXXX").string();
  assert(::mkdtemp(scratch.data()));
  fs::path root = scratch;
  fs::path pkg = root / "package", build = root / "build";
  fs::create_directory(pkg);
  fs::create_directory(build);
  write(pkg / "main.cpp", "int main() { return 0; }\n");
  write(pkg / "cap.toml", manifest());
  /// Run the coordinator with arguments kept alive throughout the call.
  /// @param[in] args std::vector<std::string> — Tokens following `build`.
  /// @return core::Result<StandaloneOutput> — Build result or diagnostic.
  /// @throws std::exception If argument preparation or the coordinator throws.
  auto call = [&](std::vector<std::string> args) {
    std::vector<std::string_view> views(args.begin(), args.end());
    return cap::cli::run_standalone(views);
  };
  /// Construct a request for the current package and explicit build root.
  /// @param[in] tool const std::string& — Tool path, invalid by default.
  /// @return std::vector<std::string> — Owned request tokens.
  /// @throws std::exception If constructing paths or tokens fails.
  auto args = [&](const std::string &tool = "/bin/false") {
    return std::vector<std::string>{"--cc",
                                    tool,
                                    "--cxx",
                                    tool,
                                    "--ar",
                                    tool,
                                    "--manifest-path",
                                    (pkg / "cap.toml").string(),
                                    "--build-root",
                                    build.string()};
  };
  /// Assert a request fails with the expected message before creating output.
  /// @param[in] text const std::string& — Required diagnostic substring.
  /// @return void — No value; failed expectations abort the test.
  /// @throws std::exception If the request or filesystem inspection throws.
  auto expect = [&](const std::string &text) {
    auto r = call(args());
    if (r || r.error().message.find(text) == std::string::npos)
      std::fprintf(stderr, "expected %s: %s\n", text.c_str(),
                   r ? "success" : r.error().message.c_str());
    assert(!r && r.error().message.find(text) != std::string::npos);
    assert(fs::is_empty(build));
  };
  auto bad = args();
  bad[7] = (pkg / "missing" / "cap.toml").string();
  assert(!call(bad));
  bad[7] = (pkg / "main.cpp").string();
  assert(!call(bad));
  fs::create_symlink(pkg / "main.cpp", root / "cap.toml");
  bad[7] = (root / "cap.toml").string();
  assert(!call(bad));
  fs::remove(root / "cap.toml");
  write(pkg / "cap.toml", "invalid");
  assert(!call(args()));
  write(pkg / "cap.toml", manifest("[workspace]\nmembers = []"));
  expect("failed with exit code 1");
  write(pkg / "cap.toml", manifest());
  bad = args();
  bad.push_back("--workspace");
  auto rejected = call(bad);
  assert(!rejected &&
         rejected.error().message.find("workspaces") != std::string::npos);
  bad = args();
  bad.insert(bad.end(), {"--bin", "missing"});
  assert(!call(bad));
  bad = args();
  bad.push_back("--lib");
  assert(!call(bad));
  write(pkg / "cap.toml",
        manifest("", "") +
            "[[bin]]\nname = \"second\"\nentry = \"main.cpp\"\n");
  expect("duplicate canonical source");
  write(pkg / "second.cpp", "int main() { return 0; }\n");
  write(pkg / "cap.toml",
        manifest() + "[[bin]]\nname = \"second\"\nentry = \"second.cpp\"\n"
                     "system-libraries = [\"m\"]\n");
  expect("dependencies");
  write(pkg / "cap.toml",
        manifest() + "[[bin]]\nname = \"second\"\nentry = \"missing.cpp\"\n");
  assert(!call(args()));
  assert(fs::is_empty(build));
  write(pkg / "cap.toml",
        manifest("[dependencies]\nother = { path = \"other\" }"));
  expect("dependencies");
  write(pkg / "cap.toml", manifest("", "depends-on = [\"other\"]"));
  expect("dependencies");
  write(pkg / "cap.toml", manifest("", "system-libraries = [\"m\"]"));
  expect("dependencies");
  write(pkg / "cap.toml", manifest("", "", "../outside.cpp"));
  assert(!call(args()));
  write(pkg / "cap.toml", manifest("", "", "missing.cpp"));
  assert(!call(args()));
  write(root / "outside.cpp", "int x;\n");
  fs::create_symlink(root / "outside.cpp", pkg / "escape.cpp");
  write(pkg / "cap.toml", manifest("", "", "escape.cpp"));
  assert(!call(args()));
  for (const auto &include : {"../outside", "/tmp", "missing", "main.cpp"}) {
    write(pkg / "cap.toml",
          manifest("", "include-dirs = [\"" + std::string(include) + "\"]"));
    expect("package path");
  }
  fs::create_directory(root / "outside");
  fs::create_directory_symlink(root / "outside", pkg / "escape");
  write(pkg / "cap.toml", manifest("", "include-dirs = [\"escape\"]"));
  expect("package path");
  write(pkg / "cap.toml", manifest("[language]\nc = \"c99\""));
  expect("unsupported language");
  write(pkg / "cap.toml", manifest("[language]\ncpp = \"c++17\""));
  expect("unsupported language");
  write(pkg / "cap.toml", manifest());
  fs::create_directory_symlink(root / "outside", pkg / ".cap");
  bad = args();
  bad.erase(bad.end() - 2, bad.end());
  rejected = call(bad);
  assert(!rejected &&
         rejected.error().message.find("escapes package") != std::string::npos);
  assert(!fs::exists(root / "outside" / "build"));
  fs::remove(pkg / ".cap");
  bad = args();
  bad.back() = (root / "missing").string();
  assert(!call(bad));
  fs::path newline = root / "bad\nroot";
  fs::create_directory(newline);
  bad.back() = newline.string();
  assert(!call(bad));
  bad = args("/nonexistent/cc");
  assert(!call(bad));
  assert(fs::is_empty(build));

  auto good = args();
  good[1] = argv[1];
  good[3] = argv[2];
  good[5] = argv[3];
  auto built = call(good);
  assert(built && built->help.empty() && built->artifacts.size() == 1 &&
         fs::is_regular_file(built->artifacts[0]));
  assert(fs::is_regular_file(built->compdb));
  auto db = read(built->compdb);
  assert(db.find("-std=c++26") != std::string::npos);
  assert(db.find((pkg / "main.cpp").string()) != std::string::npos);
  assert(!fs::exists(pkg / "compile_commands.json"));
  auto old_artifact = fs::file_size(built->artifacts[0]);
  write(pkg / "main.cpp", "#error expected_failure\n");
  assert(!call(good));
  assert(read(built->compdb) == db &&
         fs::file_size(built->artifacts[0]) == old_artifact);
  write(pkg / "main.cpp", "int main() { return 0; }\n");
  auto default_args = good;
  default_args.erase(default_args.end() - 2, default_args.end());
  auto default_build = call(default_args);
  assert(default_build && fs::is_regular_file(default_build->compdb));
  assert(fs::path(default_build->compdb)
             .string()
             .starts_with((pkg / ".cap" / "build").string()));

  write(pkg / "second.cpp", "int main() { return 0; }\n");
  write(pkg / "cap.toml",
        manifest() + "[[bin]]\nname = \"second\"\nentry = \"second.cpp\"\n");
  auto multi = call(good);
  assert(multi && multi->artifacts.size() == 2);
  assert(fs::path(multi->artifacts[0]).parent_path().filename() == "example");
  assert(fs::path(multi->artifacts[1]).parent_path().filename() == "second");
  auto multi_db = read(multi->compdb);
  assert(multi_db.find((pkg / "main.cpp").string()) != std::string::npos);
  assert(multi_db.find((pkg / "second.cpp").string()) != std::string::npos);
  write(pkg / "main.cpp", "#error unselected\n");
  auto second = good;
  second.insert(second.end(), {"--bin", "second"});
  auto only_second = call(second);
  assert(only_second && only_second->artifacts.size() == 1);
  auto second_db = read(only_second->compdb);
  auto second_artifact = read(only_second->artifacts[0]);
  assert(second_db.find((pkg / "main.cpp").string()) == std::string::npos);
  assert(second_db.find((pkg / "second.cpp").string()) != std::string::npos);
  write(pkg / "second.cpp", "#error selected_failure\n");
  std::set<fs::path> before_selected_failure;
  for (const auto &entry : fs::directory_iterator(build))
    before_selected_failure.insert(entry.path());
  auto selected_failure = call(second);
  assert(!selected_failure && selected_failure.error().message.find(
                                  "selected_failure") != std::string::npos);
  std::size_t failed_selected_invocations = 0;
  for (const auto &entry : fs::directory_iterator(build))
    if (!before_selected_failure.contains(entry.path())) {
      ++failed_selected_invocations;
      assert(!fs::exists(entry.path() / "compile_commands.json"));
    }
  assert(failed_selected_invocations == 1);
  assert(read(only_second->compdb) == second_db);
  assert(read(only_second->artifacts[0]) == second_artifact);
  write(pkg / "main.cpp", "int main() { return 0; }\n");
  write(pkg / "second.cpp", "#error later_target\n");
  std::set<fs::path> previous;
  for (const auto &entry : fs::directory_iterator(build))
    previous.insert(entry.path());
  assert(!call(good));
  std::size_t new_invocations = 0;
  for (const auto &entry : fs::directory_iterator(build))
    if (!previous.contains(entry.path())) {
      ++new_invocations;
      assert(!fs::exists(entry.path() / "compile_commands.json"));
      assert(fs::is_regular_file(entry.path() / "example" / "artifact"));
    }
  assert(new_invocations == 1);
  assert(read(multi->compdb) == multi_db);
  assert(fs::is_regular_file(multi->artifacts[0]));
  assert(fs::is_regular_file(multi->artifacts[1]));

  write(pkg / "main.cpp", "int main() { return 0; }\n");
  write(pkg / "library.c", "int value(void) { return 42; }\n");
  fs::create_directory(pkg / "include space");
  write(pkg / "cap.toml",
        "manifest-version = 1\n[package]\nname = \"example\"\n"
        "version = \"1\"\n[lib]\nname = \"example\"\n"
        "sources = [\"library.c\"]\npublic-include-dirs = [\"include space\"]\n"
        "[[bin]]\nname = \"example\"\nentry = \"main.cpp\"\n");
  auto mixed = call(good);
  assert(mixed && mixed->artifacts.size() == 2);
  assert(fs::path(mixed->artifacts[0]).filename() == "libexample.a");
  assert(fs::path(mixed->artifacts[0]).parent_path().filename() ==
         "lib.example");
  assert(fs::path(mixed->artifacts[1]).parent_path().filename() == "example");
  auto mixed_db = read(mixed->compdb);
  assert(mixed_db.find((pkg / "library.c").string()) != std::string::npos);
  assert(mixed_db.find((pkg / "include space").string()) != std::string::npos);
  auto only_lib_args = good;
  only_lib_args.push_back("--lib");
  auto only_lib = call(only_lib_args);
  assert(only_lib && only_lib->artifacts.size() == 1);
  assert(fs::path(only_lib->artifacts[0]).filename() == "libexample.a");
  assert(read(only_lib->compdb).find((pkg / "main.cpp").string()) ==
         std::string::npos);
  write(pkg / "cap.toml",
        "manifest-version = 1\n[package]\nname = \"example\"\n"
        "version = \"1\"\n[lib]\nname = \"example\"\n"
        "sources = [\"library.c\"]\npublic-include-dirs = [\"escape\"]\n");
  assert(!call(only_lib_args));
  assert(read(mixed->compdb) == mixed_db);

  // A transitive import needs both private archive BMIs, even across spaces.
  fs::path workspace = root / "workspace space";
  fs::create_directories(workspace / "app" / "source space");
  fs::create_directories(workspace / "middle" / "source space");
  fs::create_directories(workspace / "leaf" / "source space");
  fs::create_directories(workspace / "duplicate" / "source space");
  write(workspace / "cap.toml",
        "manifest-version = 1\n[workspace]\n"
        "members = [\"app\", \"middle\", \"leaf\", \"duplicate\"]\n");
  write(workspace / "app" / "cap.toml",
        "manifest-version = 1\n[package]\nname = \"app\"\nversion = \"1\"\n"
        "[dependencies]\nmiddle = { path = \"../middle\" }\n"
        "[[bin]]\nname = \"app\"\nentry = \"source space/app.cpp\"\n");
  write(workspace / "middle" / "cap.toml",
        "manifest-version = 1\n[package]\nname = \"middle\"\nversion = \"1\"\n"
        "[dependencies]\nleaf = { path = \"../leaf\" }\n"
        "[lib]\nname = \"middle\"\nsources = [\"source space/middle.cppm\"]\n");
  write(workspace / "leaf" / "cap.toml",
        "manifest-version = 1\n[package]\nname = \"leaf\"\nversion = \"1\"\n"
        "[lib]\nname = \"leaf\"\nsources = [\"source space/leaf.cppm\"]\n");
  write(workspace / "duplicate" / "cap.toml",
        "manifest-version = 1\n[package]\nname = \"duplicate\"\n"
        "version = \"1\"\n[lib]\nname = \"duplicate\"\n"
        "sources = [\"source space/duplicate.cppm\"]\n");
  write(workspace / "leaf" / "source space" / "leaf.cppm",
        "export module fixture.leaf;\nexport int leaf() { return 42; }\n");
  write(workspace / "middle" / "source space" / "middle.cppm",
        "export module fixture.middle;\nimport fixture.leaf;\n"
        "export int middle() { return leaf(); }\n");
  write(workspace / "app" / "source space" / "app.cpp",
        "import fixture.middle;\nint main() { return middle() != 42; }\n");
  write(workspace / "duplicate" / "source space" / "duplicate.cppm",
        "export module fixture.leaf;\nexport int other() { return 0; }\n");
  auto workspace_args = good;
  workspace_args[7] = (workspace / "app" / "cap.toml").string();
  auto transitive = call(workspace_args);
  if (!transitive)
    std::fprintf(stderr, "transitive workspace: %s\n",
                 transitive.error().message.c_str());
  assert(transitive && transitive->artifacts.size() == 1);
  assert(fs::is_regular_file(transitive->artifacts[0]));
  auto transitive_db = read(transitive->compdb);
  auto transitive_artifact = read(transitive->artifacts[0]);
  for (const auto &source :
       {"leaf/source space/leaf.cppm", "middle/source space/middle.cppm",
        "app/source space/app.cpp"})
    assert(transitive_db.find("\"file\":\"" + (workspace / source).string() +
                              "\"") != std::string::npos);
  assert(transitive_db.find("duplicate.cppm") == std::string::npos);
  std::size_t rows = 0, pos = 0;
  while ((pos = transitive_db.find("\"file\":", pos)) != std::string::npos) {
    ++rows;
    pos += 7;
  }
  assert(rows == 3);
  assert(transitive_db.find("-fmodule-mapper=") != std::string::npos);

  write(workspace / "app" / "cap.toml",
        "manifest-version = 1\n[package]\nname = \"app\"\nversion = \"1\"\n"
        "[dependencies]\nmiddle = { path = \"../middle\" }\n"
        "duplicate = { path = \"../duplicate\" }\n"
        "[[bin]]\nname = \"app\"\nentry = \"source space/app.cpp\"\n");
  std::set<fs::path> before_duplicate;
  for (const auto &entry : fs::directory_iterator(build))
    before_duplicate.insert(entry.path());
  auto duplicate = call(workspace_args);
  assert(!duplicate &&
         duplicate.error().message.find("duplicate") != std::string::npos &&
         duplicate.error().message.find("provider") != std::string::npos);
  for (const auto &entry : fs::directory_iterator(build))
    if (!before_duplicate.contains(entry.path()))
      assert(!fs::exists(entry.path() / "compile_commands.json"));
  assert(read(transitive->compdb) == transitive_db);
  assert(read(transitive->artifacts[0]) == transitive_artifact);
}
