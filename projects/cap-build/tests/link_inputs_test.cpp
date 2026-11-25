#include <cassert>
#include <string>
#include <vector>

import cap.build.actions;

/// Check archive ordering, validation, SDK duplicates and a long chain.
/// @return int — Zero when all assertions pass; assertion failure aborts.
/// @throws std::bad_alloc If allocation fails.
/// @throws std::length_error If a string or vector exceeds its maximum size.
int main() {
  using namespace cap::build;
  /// const std::vector<ArchiveLinkFact> — Diamond and unreachable facts.
  const std::vector<ArchiveLinkFact> facts{
      {"first", "/out/libfirst.a", {"left", "right"}, {"/sdk/libm.so"}},
      {"left", "/out/libleft.a", {"leaf"}, {"/sdk/libz.so"}},
      {"right", "/out/libright.a", {"leaf"}, {"/sdk/libz.so"}},
      {"leaf", "/out/libleaf.a", {}, {"/sdk/libc.so"}},
      {"unrelated", "/out/libunused.a", {"missing"}, {"/sdk/libunused.so"}}};
  /// const cap::core::Result<LinkInputs> — Planned diamond closure.
  const auto planned =
      plan_link_inputs({"first"}, facts, {"/sdk/libbin.so", "/sdk/libbin.so"});
  assert(planned);
  assert((planned->ordered_archives ==
          std::vector<std::string>{"/out/libfirst.a", "/out/libleft.a",
                                   "/out/libright.a", "/out/libleaf.a"}));
  assert((planned->system_libraries ==
          std::vector<std::string>{"/sdk/libbin.so", "/sdk/libbin.so",
                                   "/sdk/libm.so", "/sdk/libz.so",
                                   "/sdk/libz.so", "/sdk/libc.so"}));
  /// const cap::core::Result<LinkInputs> — Closure with a prerequisite root.
  const auto root_leaf = plan_link_inputs({"leaf", "first"}, facts, {});
  assert(root_leaf && root_leaf->ordered_archives == planned->ordered_archives);
  assert(!plan_link_inputs({"first", "first"}, facts, {}));
  assert(!plan_link_inputs({"missing"}, facts, {}));
  assert(!plan_link_inputs(
      {"first"}, {{"first", "/out/libfirst.a", {"missing"}, {}}}, {}));
  assert(!plan_link_inputs({"first"},
                           {{"first", "/out/libfirst.a", {"second"}, {}},
                            {"second", "/out/libsecond.a", {"first"}, {}}},
                           {}));
  assert(!plan_link_inputs(
      {"same"},
      {{"same", "/out/one.a", {}, {}}, {"same", "/out/two.a", {}, {}}}, {}));
  assert(!plan_link_inputs(
      {"first"},
      {{"first", "/out/same.a", {}, {}}, {"second", "/out/same.a", {}, {}}},
      {}));
  assert(!plan_link_inputs({"first"}, {{"first", "relative", {}, {}}}, {}));
  assert(!plan_link_inputs({"first"}, {{"first", "/out/libfirst.a", {}, {}}},
                           {std::string{"/sdk/lib\0m.so", 13}}));
  std::vector<ArchiveLinkFact> chain;
  for (int i = 0; i < 10000; ++i)
    chain.push_back(
        {"archive-" + std::to_string(i),
         "/out/archive-" + std::to_string(i) + ".a",
         i == 9999
             ? std::vector<std::string>{}
             : std::vector<std::string>{"archive-" + std::to_string(i + 1)},
         {}});
  /// const cap::core::Result<LinkInputs> — Planned long archive chain.
  const auto long_chain = plan_link_inputs({"archive-0"}, chain, {});
  assert(long_chain && long_chain->ordered_archives.size() == chain.size());
}
