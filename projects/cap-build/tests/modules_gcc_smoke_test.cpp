#include <cassert>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

import cap.build.modules;

int main(int argc, char **argv) {
  assert(argc == 5);
  auto read_report = [](const char *path) {
    std::ifstream input(path);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
  };
  auto provider = cap::build::parse_module_scan(read_report(argv[1]), argv[2]);
  auto importer = cap::build::parse_module_scan(read_report(argv[3]), argv[4]);
  assert(provider && importer && provider->provides.size() == 1 &&
         importer->required_modules == provider->provides);
  auto order = cap::build::order_module_units({*importer, *provider});
  assert(order && order->size() == 2);
  assert((*order)[0].object == provider->object);
  assert((*order)[1].object == importer->object);
}
