#include "library.h"
#include <cstdio>

/// Runs the independent archive consumer.
/// @return int — Zero after printing `library: 42`; nonzero for a wrong value
///         or an output error.
/// @note Does not throw.
int main() {
  const int value = library_value();
  if (value != 42)
    return 1;
  return std::printf("library: %d\n", value) < 0 ? 1 : 0;
}
