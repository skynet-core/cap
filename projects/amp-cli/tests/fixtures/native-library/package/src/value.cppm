export module fixture.library.value;

/// Returns the named-module contribution to the fixture value.
/// @return int — The value 23; there is no failure result.
/// @note Does not throw.
export int module_value() noexcept { return 23; }
