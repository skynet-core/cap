#include <algorithm>
#include <cassert>
#include <string>
#include <vector>

import amp.build.actions;
import amp.build.modules;

int main() {
  using namespace amp::build;
  const Action compile{ActionKind::compile,
                       "/work dir",
                       {"/tools/g++", "-std=c++26", "-fmodules", "-I",
                        "/include dir", "-c", "/src/second file.cpp", "-o",
                        "/out/second file.o"},
                       {"/src/second file.cpp"},
                       {"/out/second file.o"}};
  auto scan = plan_module_scan(compile, "/out/reports/second.json");
  assert(scan);
  assert((*scan ==
          std::vector<std::string>{
              "/tools/g++", "-std=c++26", "-fmodules", "-I", "/include dir",
              "-M", "-MF", "/dev/null", "-fdeps-format=p1689r5",
              "-fdeps-target=/out/second file.o",
              "-fdeps-file=/out/reports/second.json", "/src/second file.cpp"}));
  auto planned_interface = plan_compile({"/tools/g++",
                                         "/work dir",
                                         "/src/module provider.cppm",
                                         "/out/provider.o",
                                         "c++26",
                                         {},
                                         true});
  assert(planned_interface);
  auto planned_scan =
      plan_module_scan(*planned_interface, "/out/provider.json");
  assert(planned_scan &&
         (*planned_scan ==
          std::vector<std::string>{
              "/tools/g++", "-std=c++26", "-fmodules", "-x", "c++", "-M", "-MF",
              "/dev/null", "-fdeps-format=p1689r5",
              "-fdeps-target=/out/provider.o", "-fdeps-file=/out/provider.json",
              "/src/module provider.cppm"}));
  auto planned_importer = plan_compile({"/tools/g++",
                                        "/work dir",
                                        "/src/importer.cpp",
                                        "/out/importer.o",
                                        "c++26",
                                        {},
                                        true});
  assert(planned_importer);
  auto importer_scan =
      plan_module_scan(*planned_importer, "/out/importer.json");
  assert(importer_scan &&
         (*importer_scan ==
          std::vector<std::string>{
              "/tools/g++", "-std=c++26", "-fmodules", "-M", "-MF", "/dev/null",
              "-fdeps-format=p1689r5", "-fdeps-target=/out/importer.o",
              "-fdeps-file=/out/importer.json", "/src/importer.cpp"}));
  assert(!plan_module_scan(compile, "relative.json"));
  auto wrong_kind = compile;
  wrong_kind.kind = ActionKind::link;
  assert(!plan_module_scan(wrong_kind, "/report.json"));
  auto bad_suffix = compile;
  bad_suffix.arguments.back() = "/different.cpp";
  assert(!plan_module_scan(bad_suffix, "/report.json"));
  auto empty_arg = compile;
  empty_arg.arguments[3].clear();
  assert(!plan_module_scan(empty_arg, "/report.json"));
  auto bad_standard = compile;
  bad_standard.arguments[1] = "-std=c17";
  assert(!plan_module_scan(bad_standard, "/report.json"));
  auto missing_include = compile;
  missing_include.arguments.erase(missing_include.arguments.begin() + 4);
  assert(!plan_module_scan(missing_include, "/report.json"));
  auto relative_include = compile;
  relative_include.arguments[4] = "relative";
  assert(!plan_module_scan(relative_include, "/report.json"));
  auto extra_flag = compile;
  extra_flag.arguments.insert(extra_flag.arguments.begin() + 5, "-O2");
  assert(!plan_module_scan(extra_flag, "/report.json"));
  auto no_modules = compile;
  no_modules.arguments.erase(no_modules.arguments.begin() + 2);
  assert(!plan_module_scan(no_modules, "/report.json"));
  auto bad_source = compile;
  bad_source.inputs.front() = "/src/not-cpp.c";
  bad_source.arguments[6] = "/src/not-cpp.c";
  assert(!plan_module_scan(bad_source, "/report.json"));
  auto bad_interface = compile;
  bad_interface.inputs.front() = "/src/interface.cppm";
  bad_interface.arguments[6] = "/src/interface.cppm";
  bad_interface.arguments.insert(bad_interface.arguments.begin() + 3,
                                 {"-x", "c++"});
  assert(plan_module_scan(bad_interface, "/report.json"));
  bad_interface.arguments.erase(bad_interface.arguments.begin() + 3,
                                bad_interface.arguments.begin() + 5);
  assert(!plan_module_scan(bad_interface, "/report.json"));

  auto provider = parse_module_scan(
      R"({"version":0,"revision":0,"rules":[{"primary-output":"/out/provider.o","provides":[{"logical-name":"fixture","is-interface":true}],"requires":[]}]})",
      "/out/provider.o");
  assert(provider && provider->provides == std::vector<std::string>{"fixture"});
  auto partition = parse_module_scan(
      R"({"version":0,"revision":0,"rules":[{"primary-output":"/out/partition.o","provides":[{"logical-name":"fixture:api","is-interface":false}]}]})",
      "/out/partition.o");
  assert(partition &&
         partition->provides == std::vector<std::string>{"fixture:api"});
  auto importer = parse_module_scan(
      R"({"version":0,"revision":0,"rules":[{"primary-output":"/out/importer.o","requires":[{"logical-name":"fixture"}]}]})",
      "/out/importer.o");
  assert(importer &&
         importer->required_modules == std::vector<std::string>{"fixture"});
  auto ordinary = parse_module_scan(
      R"({"version":0,"revision":0,"rules":[{"primary-output":"/out/plain.o"}]})",
      "/out/plain.o");
  assert(ordinary && ordinary->provides.empty() &&
         ordinary->required_modules.empty());
  assert(!parse_module_scan("{", "/out/plain.o"));
  assert(!parse_module_scan(
      R"({"version":42,"version":0,"revision":0,"rules":[{"primary-output":"/out/plain.o"}]})",
      "/out/plain.o"));
  assert(!parse_module_scan(
      R"({"version":42,"\u0076ersion":0,"revision":0,"rules":[{"primary-output":"/out/plain.o"}]})",
      "/out/plain.o"));
  assert(!parse_module_scan(
      R"({"version":0,"revision":0,"rules":[{"primary-output":"/out/wrong.o","primary\u002doutput":"/out/plain.o"}]})",
      "/out/plain.o"));
  assert(!parse_module_scan(
      R"({"version":0,"revision":0,"rules":[{"primary-output":"/out/plain.o","provides":[{"logical-name":"fixture","logical\u002dname":"other"}]}]})",
      "/out/plain.o"));
  assert(!parse_module_scan(R"({"version":1,"revision":0,"rules":[]})",
                            "/out/plain.o"));
  assert(!parse_module_scan(R"({"version":0,"revision":"0","rules":[]})",
                            "/out/plain.o"));
  assert(!parse_module_scan(R"({"version":0,"revision":0,"rules":{}})",
                            "/out/plain.o"));
  assert(!parse_module_scan(R"({"version":0,"revision":0,"rules":[{},{}]})",
                            "/out/plain.o"));
  assert(!parse_module_scan(
      R"({"version":0,"revision":0,"rules":[{"primary-output":42}]})",
      "/out/plain.o"));
  assert(!parse_module_scan(
      R"({"version":0,"revision":0,"rules":[{"primary-output":"/out/plain.o","unsupported":true}]})",
      "/out/plain.o"));
  assert(!parse_module_scan(
      R"({"version":0,"revision":0,"rules":[{"primary-output":"/other.o"}]})",
      "/out/plain.o"));
  assert(!parse_module_scan(
      R"({"version":0,"revision":0,"rules":[{"primary-output":"/out/plain.o","requires":[{"logical-name":"<vector>","source-path":"vector"}]}]})",
      "/out/plain.o"));
  assert(!parse_module_scan(
      R"({"version":0,"revision":0,"rules":[{"primary-output":"/out/plain.o","provides":"bad"}]})",
      "/out/plain.o"));
  assert(!parse_module_scan(
      R"({"version":0,"revision":0,"rules":[{"primary-output":"/out/plain.o","provides":[{"logical-name":"fixture","is-interface":"yes"}]}]})",
      "/out/plain.o"));
  assert(!parse_module_scan(
      R"({"version":0,"revision":0,"rules":[{"primary-output":"/out/plain.o","provides":[{}]}]})",
      "/out/plain.o"));
  assert(!parse_module_scan(
      R"({"version":0,"revision":0,"rules":[{"primary-output":"/out/plain.o","provides":[{"logical-name":"bad name"}]}]})",
      "/out/plain.o"));
  assert(!parse_module_scan(
      R"({"version":0,"revision":0,"rules":[{"primary-output":"/out/plain.o","provides":[{"logical-name":"fixture"},{"logical-name":"fixture"}]}]})",
      "/out/plain.o"));
  assert(!parse_module_scan(
      R"({"version":0,"revision":0,"rules":[{"primary-output":"/out/plain.o"}]})",
      "relative.o"));

  auto ordered = order_module_units(
      {*importer, *provider, ModuleUnit{"/out/plain.o", {}, {}}});
  assert(ordered && ordered->size() == 3);
  assert((*ordered)[0].object == "/out/plain.o");
  assert((*ordered)[1].object == "/out/provider.o");
  assert((*ordered)[2].object == "/out/importer.o");
  assert(!order_module_units({*provider, *provider}));
  assert(
      !order_module_units({ModuleUnit{"/out/missing.o", {}, {"not_found"}}}));
  assert(!order_module_units({ModuleUnit{"/out/a.o", {"a"}, {"b"}},
                              ModuleUnit{"/out/b.o", {"b"}, {"a"}}}));
  assert(!order_module_units({ModuleUnit{"/out/a.o", {"same"}, {}},
                              ModuleUnit{"/out/b.o", {"same"}, {}}}));
  assert(!order_module_units(
      {ModuleUnit{"/out/a.o", {}, {"bad:name:partition"}}}));
  assert(!order_module_units({ModuleUnit{"relative.o", {}, {}}}));
  assert(!order_module_units({ModuleUnit{"/out/a.o", {"bad name"}, {}}}));
  assert(!order_module_units({ModuleUnit{"/out/a.o", {"same", "same"}, {}}}));
  assert(!order_module_units({ModuleUnit{"/out/a.o", {}, {"same", "same"}}}));
  assert(!order_module_units({ModuleUnit{"/out/a.o", {"same"}, {"same"}}}));

  const ModuleMap external{{"ext.alpha", "/external modules/alpha.gcm"},
                           {"ext.beta", "/external/beta.gcm"}};
  auto mapper = format_module_map(external);
  assert(mapper && *mapper == "ext.alpha /external modules/alpha.gcm\n"
                              "ext.beta /external/beta.gcm\n");
  assert(format_module_map({}));
  assert(!format_module_map({{"bad name", "/external/a.gcm"}}));
  assert(!format_module_map({{"valid", "relative.gcm"}}));
  assert(!format_module_map({{"valid", "/external/a/../a.gcm"}}));
  assert(!format_module_map({{"valid", "/external/a\n.gcm"}}));
  assert(!format_module_map({{"valid", std::string("/external/a\0.gcm", 16)}}));
  auto external_importer =
      ModuleUnit{"/out/external-importer.o", {}, {"ext.alpha"}};
  auto external_order = order_module_units({external_importer}, external);
  assert(external_order && external_order->size() == 1 &&
         external_order->front().object == external_importer.object);
  assert(!order_module_units({external_importer}));
  assert(!order_module_units({ModuleUnit{"/out/local.o", {"ext.alpha"}, {}}},
                             external));

  std::vector<ModuleUnit> chain;
  constexpr std::size_t graph_size = 10'000;
  chain.reserve(graph_size);
  for (std::size_t i = 0; i < graph_size; ++i) {
    auto n = std::to_string(i);
    auto padded = std::string(5 - n.size(), '0') + n;
    chain.push_back(ModuleUnit{
        "/out/" + padded + ".o",
        {"m" + n},
        i == 0 ? std::vector<std::string>{}
               : std::vector<std::string>{"m" + std::to_string(i - 1)}});
  }
  std::ranges::reverse(chain);
  auto ordered_chain = order_module_units(std::move(chain));
  assert(ordered_chain && ordered_chain->size() == graph_size);
  assert(ordered_chain->front().object == "/out/00000.o");
  assert(ordered_chain->back().object == "/out/09999.o");

  std::vector<ModuleUnit> independent;
  independent.reserve(graph_size);
  for (std::size_t i = 0; i < graph_size; ++i) {
    auto n = std::to_string(i);
    auto padded = std::string(5 - n.size(), '0') + n;
    independent.push_back(ModuleUnit{"/out/" + padded + ".o", {}, {}});
  }
  std::ranges::reverse(independent);
  auto ordered_independent = order_module_units(std::move(independent));
  assert(ordered_independent && ordered_independent->size() == graph_size);
  assert(ordered_independent->front().object == "/out/00000.o");
  assert(ordered_independent->back().object == "/out/09999.o");
}
