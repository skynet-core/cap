#include <cstdio>

import multibin.value;

/// Supply the C prerequisite's contribution.
/// @return int — The value two.
/// @note Does not throw.
extern "C" int c_value();

/// Print the second binary's combined result.
/// @return int — Zero on success.
/// @note Does not throw.
int main() {
  std::printf("second: %d\n", module_value() + c_value());
  return 0;
}
