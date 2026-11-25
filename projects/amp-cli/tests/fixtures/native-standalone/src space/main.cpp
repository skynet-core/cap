#include <cstdio>

import standalone.value;

extern "C" int c_value();

int main() {
  std::printf("standalone: %d\n", module_value() + c_value());
  return 0;
}
