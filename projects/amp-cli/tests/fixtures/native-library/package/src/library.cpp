#include "library.h"

import fixture.library.value;

/// Returns the C contribution to the fixture value.
/// @return int — The value 19; there is no failure result.
/// @note Does not throw.
extern "C" int c_value() noexcept;

int library_value() noexcept { return c_value() + module_value(); }
