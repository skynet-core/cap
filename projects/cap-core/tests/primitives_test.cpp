// Contract test for cap-core primitives: StopToken, IClock, ILogger (M0).
#include <chrono>
#include <concepts>
#include <type_traits>

#include "test_util.hpp"

import cap.core.clock;
import cap.core.diagnostic;
import cap.core.logger;
import cap.core.stop_token;

using cap::core::CaptureLogger;
using cap::core::Diagnostic;
using cap::core::ErrorCode;
using cap::core::Severity;
using cap::core::StopToken;
using cap::core::SystemClock;

static_assert(std::is_abstract_v<cap::core::IClock>);
static_assert(std::derived_from<SystemClock, cap::core::IClock>);
static_assert(std::same_as<decltype(std::declval<const cap::core::IClock &>()
                                        .monotonic_now()),
                           std::chrono::steady_clock::time_point>);

int main() {
  // StopToken: initially clear, sticky after request, repeatable request.
  // Member-function pointers keep the calls out-of-line; direct calls are
  // constant-folded at -O1, which would drop stop_token.cppm from coverage.
  StopToken token;
  auto is_stop = &StopToken::is_stop_requested;
  auto request = &StopToken::request_stop;
  EXPECT_TRUE(!(token.*is_stop)());
  (token.*request)();
  EXPECT_TRUE((token.*is_stop)());
  (token.*request)();
  EXPECT_TRUE((token.*is_stop)());

  // IClock: monotonic_now is non-decreasing across calls.
  SystemClock clock;
  const auto t0 = clock.monotonic_now();
  const auto t1 = clock.monotonic_now();
  EXPECT_TRUE(t0 <= t1);

  // ILogger: CaptureLogger records canonical diagnostic lines.
  CaptureLogger logger;
  Diagnostic d;
  d.code = ErrorCode::invalid_input;
  d.severity = Severity::warning;
  d.message = "captured";
  logger.emit(d);
  EXPECT_EQ(logger.lines().size(), std::size_t(1));
  if (logger.lines().size() == 1) {
    EXPECT_EQ(logger.lines()[0], "warning code=invalid-input message=captured");
  }
  Diagnostic sensitive;
  sensitive.message = "caller-redacted";
  logger.emit(sensitive);
  EXPECT_EQ(logger.lines().size(), std::size_t(2));
  if (logger.lines().size() == 2) {
    EXPECT_EQ(logger.lines()[1], "error code=internal message=caller-redacted");
    EXPECT_TRUE(logger.lines()[1].find('\n') == std::string::npos);
    EXPECT_TRUE(logger.lines()[1].find('\r') == std::string::npos);
  }

  return cap_test::summary("cap-core primitives");
}
