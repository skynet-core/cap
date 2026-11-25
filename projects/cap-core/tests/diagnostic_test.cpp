// Contract test for cap-core diagnostics (M0 slice).
// Assert-based runner; the Google Test/Catch2 adapters land in Phase 4.
// Usage: cap-core-diagnostic-test <golden-file>
// Golden file: projects/cap-core/tests/golden/diagnostic_format.txt
#include <cstddef>
#include <expected>
#include <fstream>
#include <string>
#include <type_traits>
#include <vector>

#include "test_util.hpp"

import cap.core.diagnostic;

using cap::core::Diagnostic;
using cap::core::ErrorCode;
using cap::core::Result;
using cap::core::Severity;
using cap::core::Span;
using cap::core::Status;

static_assert(std::is_same_v<Result<int>, std::expected<int, Diagnostic>>);
static_assert(std::is_same_v<Status, std::expected<void, Diagnostic>>);

namespace {

// Fixtures mirror the golden lines one-to-one, in file order.
Diagnostic fixture_1() {
  Diagnostic d;
  d.code = ErrorCode::invalid_input;
  d.severity = Severity::error;
  d.span = Span{"manifest.toml", 12, 3};
  d.message = "unknown key 'features.x'";
  d.remediation = "remove the key";
  return d;
}

Diagnostic fixture_2() {
  Diagnostic d;
  d.code = ErrorCode::transient_io;
  d.severity = Severity::warning;
  d.message = "retry fetch after integrity failure";
  Diagnostic missing;
  missing.code = ErrorCode::missing_resource;
  Diagnostic integrity;
  integrity.code = ErrorCode::integrity_failure;
  d.causes.push_back(missing);
  d.causes.push_back(integrity);
  return d;
}

Diagnostic fixture_3() {
  Diagnostic d;
  d.code = ErrorCode::unsupported_capability;
  d.severity = Severity::fatal;
  d.message = "C++26 reflection unavailable";
  return d;
}

Diagnostic fixture_4() {
  Diagnostic d;
  d.code = ErrorCode::internal;
  d.severity = Severity::info;
  d.message = "ok";
  return d;
}

Diagnostic fixture_5() {
  Diagnostic d;
  d.code = ErrorCode::internal;
  d.severity = Severity::error;
  d.span = Span{"f\\\n\t\x7f", 2, 4};
  constexpr char message[] = "m\\\0\a\b\t\n\v\f\r\x01\x1f\x7f";
  d.message = std::string(message, sizeof(message) - 1);
  d.remediation = "r\\\n\x02\x7f";
  return d;
}

std::vector<std::string> read_golden_lines(const std::string &path) {
  std::vector<std::string> lines;
  std::ifstream in(path);
  EXPECT_TRUE(static_cast<bool>(in));
  std::string line;
  while (std::getline(in, line))
    lines.push_back(line);
  return lines;
}

void test_golden_format() {
  const std::vector<Diagnostic (*)()> fixtures = {
      &fixture_1, &fixture_2, &fixture_3, &fixture_4, &fixture_5};
  for (const auto &build : fixtures) {
    // Determinism: repeated calls and independent copies must match.
    const Diagnostic d = build();
    EXPECT_EQ(d.format(), d.format());
    EXPECT_EQ(d.format(), build().format());
  }
}

void test_omitted_segments() {
  const std::string d1 = fixture_1().format();
  EXPECT_TRUE(d1.find(" causes=") == std::string::npos);
  const std::string d2 = fixture_2().format();
  EXPECT_TRUE(d2.find(" file=") == std::string::npos);
  EXPECT_TRUE(d2.find(" fix=") == std::string::npos);
  const std::string d3 = fixture_3().format();
  EXPECT_TRUE(d3.find(" file=") == std::string::npos);
  EXPECT_TRUE(d3.find(" causes=") == std::string::npos);
}

void test_taxonomy_stability() {
  const std::vector<ErrorCode> codes = {ErrorCode::invalid_input,
                                        ErrorCode::missing_resource,
                                        ErrorCode::unsupported_capability,
                                        ErrorCode::integrity_failure,
                                        ErrorCode::cancellation,
                                        ErrorCode::transient_io,
                                        ErrorCode::internal};
  for (std::size_t i = 0; i < codes.size(); ++i) {
    EXPECT_TRUE(cap::core::error_code_name(codes[i]).size() > 0);
    for (std::size_t j = i + 1; j < codes.size(); ++j) {
      EXPECT_TRUE(cap::core::error_code_name(codes[i]) !=
                  cap::core::error_code_name(codes[j]));
    }
  }
  const std::vector<Severity> severities = {Severity::info, Severity::warning,
                                            Severity::error, Severity::fatal};
  for (std::size_t i = 0; i < severities.size(); ++i) {
    for (std::size_t j = i + 1; j < severities.size(); ++j) {
      EXPECT_TRUE(cap::core::severity_name(severities[i]) !=
                  cap::core::severity_name(severities[j]));
    }
  }
  EXPECT_EQ(cap::core::error_code_name(ErrorCode::invalid_input),
            "invalid-input");
  EXPECT_EQ(cap::core::severity_name(Severity::fatal), "fatal");
}

void test_result_semantics() {
  Result<int> ok = 42;
  EXPECT_TRUE(ok.has_value());
  EXPECT_EQ(*ok, 42);

  Diagnostic bad;
  bad.code = ErrorCode::missing_resource;
  bad.message = "no toolchain";
  Result<int> fail;
  fail = std::unexpected<Diagnostic>(bad);
  EXPECT_TRUE(!fail.has_value());
  EXPECT_EQ(cap::core::error_code_name(fail.error().code), "missing-resource");

  Status good{};
  EXPECT_TRUE(good.has_value());
  Status refused;
  refused = std::unexpected<Diagnostic>(bad);
  EXPECT_TRUE(!refused.has_value());
  EXPECT_EQ(refused.error().message, "no toolchain");
}

} // namespace

int main(int argc, char **argv) {
  const std::string golden_path =
      argc > 1 ? argv[1]
               : "projects/cap-core/tests/golden/diagnostic_format.txt";
  const std::vector<std::string> golden = read_golden_lines(golden_path);
  EXPECT_EQ(golden.size(), std::size_t(5));
  if (golden.size() == 5) {
    const std::vector<std::string> expected = {
        fixture_1().format(), fixture_2().format(), fixture_3().format(),
        fixture_4().format(), fixture_5().format()};
    for (std::size_t i = 0; i < golden.size(); ++i) {
      EXPECT_EQ(expected[i], golden[i]);
    }
  }
  EXPECT_EQ(fixture_5().format(),
            "error code=internal file=f\\\\\\n\\t\\x7F:2:4 "
            "message=m\\\\\\0\\a\\b\\t\\n\\v\\f\\r\\x01\\x1F\\x7F "
            "fix=r\\\\\\n\\x02\\x7F");
  EXPECT_TRUE(fixture_5().format().find('\n') == std::string::npos);
  EXPECT_TRUE(fixture_5().format().find('\r') == std::string::npos);

  test_golden_format();
  test_omitted_segments();
  test_taxonomy_stability();
  test_result_semantics();

  return cap_test::summary("cap-core diagnostics");
}
