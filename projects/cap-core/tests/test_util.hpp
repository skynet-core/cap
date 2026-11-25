// Shared assert-based test helpers for cap-core *_test.cpp runners.
// Replaced by Google Test/Catch2 adapters in Phase 4; no external dependency.
#pragma once

#include <cstddef>
#include <iostream>
#include <string>

namespace cap_test {

inline int &failures() {
  static int failures = 0;
  return failures;
}

inline int summary(const char *name) {
  if (failures() != 0) {
    std::cerr << failures() << " " << name << " check(s) failed\n";
    return 1;
  }
  std::cout << name << ": all checks passed\n";
  return 0;
}

} // namespace cap_test

#define EXPECT_EQ(actual, expected)                                            \
  do {                                                                         \
    const auto a_ = (actual);                                                  \
    const auto e_ = (expected);                                                \
    if (!(a_ == e_)) {                                                         \
      ++cap_test::failures();                                                  \
      std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "\n"              \
                << "  actual:   " << a_ << "\n"                                \
                << "  expected: " << e_ << "\n";                               \
    }                                                                          \
  } while (0)

#define EXPECT_TRUE(cond)                                                      \
  do {                                                                         \
    if (!(cond)) {                                                             \
      ++cap_test::failures();                                                  \
      std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << ": " #cond "\n";  \
    }                                                                          \
  } while (0)
