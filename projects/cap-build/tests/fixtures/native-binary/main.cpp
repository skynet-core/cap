#include <iostream>

import answer;

extern "C" int c_value(void);
int main() {
  if (answer() != 42 || c_value() != 40)
    return 1;
  std::cout << "native binary: 42\n";
  return 0;
}
