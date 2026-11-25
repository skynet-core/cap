#include <cassert>
#include <string>
#include <vector>

import cap.build.actions;

/// Check action argv, declared paths and invalid request diagnostics.
/// @return int — Zero when all assertions pass; assertion failure aborts.
/// @throws std::bad_alloc If allocation fails.
/// @throws std::length_error If a string or vector exceeds its maximum size.
int main() {
  using namespace cap::build;
  /// const std::string — Invalid path containing an embedded NUL.
  const std::string nul_path("/bad\0path", 9);
  auto c = plan_compile({"/tools/gcc",
                         "/work dir",
                         "/src/a file.c",
                         "/out/a.o",
                         "c23",
                         {"/include dir"}});
  assert(c && c->kind == ActionKind::compile);
  assert((c->arguments == std::vector<std::string>{
                              "/tools/gcc", "-std=c23", "-I", "/include dir",
                              "-c", "/src/a file.c", "-o", "/out/a.o"}));
  assert(c->directory == "/work dir" && c->inputs.front() == "/src/a file.c" &&
         c->outputs.front() == "/out/a.o");
  auto cpp = plan_compile(
      {"/tools/g++", "/work", "/src/a.cpp", "/out/a.o", "c++26", {}, true});
  assert(cpp && (cpp->arguments == std::vector<std::string>{
                                       "/tools/g++", "-std=c++26", "-fmodules",
                                       "-c", "/src/a.cpp", "-o", "/out/a.o"}));
  auto module = plan_compile(
      {"/tools/g++", "/work", "/src/a.cppm", "/out/a.o", "c++26", {}, true});
  assert(module && (module->arguments ==
                    std::vector<std::string>{"/tools/g++", "-std=c++26",
                                             "-fmodules", "-x", "c++", "-c",
                                             "/src/a.cppm", "-o", "/out/a.o"}));
  auto archive = plan_archive(
      {"/tools/ar", "/work", "/out/lib.a", {"/out/one.o", "/out/two.o"}});
  assert(archive && archive->arguments[2] == "/out/lib.a" &&
         archive->arguments[3] == "/out/one.o" &&
         archive->arguments[4] == "/out/two.o");
  auto link = plan_link({"/tools/ld",
                         "/work",
                         "/out/bin",
                         {"/out/main.o"},
                         {"/out/libone.a", "/out/libtwo.a"},
                         {"/sdk/libssl.so", "/sdk/libcrypto.so"}});
  assert(link && link->arguments[2] == "/out/bin" &&
         link->arguments[3] == "/out/main.o" &&
         link->arguments[4] == "/out/libone.a" &&
         link->arguments[5] == "/out/libtwo.a" &&
         link->arguments[6] == "/sdk/libssl.so" &&
         link->arguments[7] == "/sdk/libcrypto.so");
  assert((link->inputs == std::vector<std::string>{
                              "/out/main.o", "/out/libone.a", "/out/libtwo.a",
                              "/sdk/libssl.so", "/sdk/libcrypto.so"}));
  assert(!plan_compile(
      {"/tools/gcc", "relative", "/src/a.c", "/out/a.o", "c17", {}}));
  assert(!plan_compile(
      {"/tools/gcc", "/work", "/src/a.cpp", "/out/a.o", "c17", {}}));
  assert(!plan_compile(
      {"/tools/gcc", "/work", "/src/a.cppm", "/out/a.o", "c++26", {}}));
  assert(!plan_compile(
      {"/tools/gcc", "/work", "/src/a.c", "/out/a.o", "c11", {}, true}));
  assert(plan_compile(
      {"/tools/gcc", "/work", "/src/a.cpp", "/out/a.o", "c++26", {}}));
  assert(!plan_compile(
      {"/tools/gcc", "/work", "/src/a.unknown", "/out/a.o", "c17", {}}));
  assert(
      !plan_compile({"/tools/gcc", "/work", "/src/a.c", "/out/a.o", "", {}}));
  assert(!plan_compile(
      {"/tools/gcc", "/work", "/src/a.c", "/out/a.o", "-O2", {}}));
  assert(!plan_compile(
      {"/tools/gcc", "/work", "/src/a.c", "/out/a.o", "c++20", {}}));
  assert(!plan_compile(
      {"/tools/gcc", "/work", "/src/a.cpp", "/out/a.o", "c20", {}, true}));
  assert(plan_compile(
      {"/tools/g++", "/work", "/src/a.cpp", "/out/a.o", "c++20", {}, true}));
  assert(plan_compile(
      {"/tools/g++", "/work", "/src/a.cpp", "/out/a.o", "c++23", {}, true}));
  for (const auto &bad_path :
       {std::string{}, std::string{"relative"}, nul_path}) {
    assert(
        !plan_compile({bad_path, "/work", "/src/a.c", "/out/a.o", "c17", {}}));
    assert(!plan_compile(
        {"/tools/gcc", bad_path, "/src/a.c", "/out/a.o", "c17", {}}));
    assert(!plan_compile(
        {"/tools/gcc", "/work", bad_path, "/out/a.o", "c17", {}}));
    assert(!plan_compile(
        {"/tools/gcc", "/work", "/src/a.c", bad_path, "c17", {}}));
    assert(!plan_compile(
        {"/tools/gcc", "/work", "/src/a.c", "/out/a.o", "c17", {bad_path}}));
  }
  assert(!plan_archive({"/tools/ar", "/work", "/out/lib.a", {}}));
  for (const auto &bad_path :
       {std::string{}, std::string{"relative"}, nul_path}) {
    assert(!plan_archive({bad_path, "/work", "/out/lib.a", {"/out/one.o"}}));
    assert(
        !plan_archive({"/tools/ar", bad_path, "/out/lib.a", {"/out/one.o"}}));
    assert(!plan_archive({"/tools/ar", "/work", bad_path, {"/out/one.o"}}));
    assert(!plan_archive({"/tools/ar", "/work", "/out/lib.a", {bad_path}}));
  }
  assert(!plan_link({"/tools/ld", "/work", "/out/bin", {}, {}, {}}));
  assert(plan_link({"/tools/ld", "/work", "/out/bin", {}, {"/out/lib.a"}, {}}));
  assert(
      plan_link({"/tools/ld", "/work", "/out/bin", {"/out/main.o"}, {}, {}}));
  for (const auto &bad_library :
       {std::string{}, std::string{"-lssl"}, std::string{"relative"}, nul_path})
    assert(!plan_link({"/tools/ld",
                       "/work",
                       "/out/bin",
                       {"/out/main.o"},
                       {},
                       {bad_library}}));
  for (const auto &bad_path :
       {std::string{}, std::string{"relative"}, nul_path}) {
    assert(
        !plan_link({bad_path, "/work", "/out/bin", {"/out/main.o"}, {}, {}}));
    assert(!plan_link(
        {"/tools/ld", bad_path, "/out/bin", {"/out/main.o"}, {}, {}}));
    assert(
        !plan_link({"/tools/ld", "/work", bad_path, {"/out/main.o"}, {}, {}}));
    assert(!plan_link({"/tools/ld", "/work", "/out/bin", {bad_path}, {}, {}}));
    assert(!plan_link({"/tools/ld", "/work", "/out/bin", {}, {bad_path}, {}}));
  }
}
