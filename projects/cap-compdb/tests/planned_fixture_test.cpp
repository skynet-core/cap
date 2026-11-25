#include <cassert>
#include <iostream>
#include <string>
#include <vector>

import cap.build.actions;
import cap.compdb;

/// Builds planner actions for fixture sources and writes their deterministic
/// compilation database JSON to standard output for the fixture harness.
/// @param[in] argc int — Argument count; must be exactly four.
/// @param[in] argv char** — Argument vector whose elements 1, 2, and 3 are
/// the fixture root, private build directory, and compiler path respectively;
/// those strings are copied before use.
/// @return int — Returns zero if action planning and repeated serialization
/// succeed; invalid inputs or failed assertions terminate unsuccessfully.
/// @throws std::bad_alloc — If copying inputs, planning actions, or
/// serializing JSON exhausts memory.
/// @throws std::length_error — If a string or vector operation exceeds its
/// maximum representable size.
int main(int argc, char **argv) {
  assert(argc == 4);
  const std::string root = argv[1], build = argv[2], compiler = argv[3];
  std::vector<cap::build::Action> actions;
  for (const auto &[source, object] :
       std::vector<std::pair<std::string, std::string>>{
           {root + "/lib/src/fixture.cpp", build + "/fixture.o"},
           {root + "/app/src space/second.cpp", build + "/second.o"}}) {
    auto action = cap::build::plan_compile(
        {compiler, build, source, object, "c++26", {}, true});
    assert(action);
    actions.push_back(*action);
  }
  auto json = cap::compdb::to_json(actions);
  auto repeat = cap::compdb::to_json(actions);
  assert(json && repeat && *json == *repeat);
  std::cout << *json;
}
