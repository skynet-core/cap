// Contract test for amp-core digest (M0). SHA-256 vectors: published test
// vectors for "" and "abc"; padding-edge vectors cross-checked with
// sha256sum/python3 at test-authoring time.
#include <cstddef>
#include <string>
#include <vector>

#include "test_util.hpp"

import amp.core.diagnostic;
import amp.core.digest;

using amp::core::Digest;

namespace {

std::vector<std::byte> bytes_of(const std::string &text) {
  std::vector<std::byte> out;
  out.reserve(text.size());
  for (const char c : text)
    out.push_back(static_cast<std::byte>(c));
  return out;
}

Digest make_digest(const std::string &hex) {
  auto parsed = Digest::parse(hex);
  EXPECT_TRUE(static_cast<bool>(parsed));
  return *parsed;
}

void expect_digest(const std::string &input, const std::string &expected_hex) {
  const std::vector<std::byte> data = bytes_of(input);
  EXPECT_EQ(Digest::from_bytes(data).hex(), expected_hex);
  // parse/hex round-trip
  EXPECT_EQ(make_digest(expected_hex).hex(), expected_hex);
  // canonical encoding is lowercase
  EXPECT_TRUE(expected_hex.find_first_not_of("0123456789abcdef") ==
              std::string::npos);
}

} // namespace

int main() {
  expect_digest(
      "", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
  expect_digest(
      "abc",
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
  // padding edges: 55 fits the first block, 56 and 57 need the length
  // spillover block; 1000 exercises multi-block compression.
  expect_digest(
      std::string(55, 'a'),
      "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318");
  expect_digest(
      std::string(56, 'a'),
      "b35439a4ac6f0948b6d6f9e3c6af0f5f590ce20f1bde7090ef7970686ec6738a");
  expect_digest(
      std::string(57, 'a'),
      "f13b2d724659eb3bf47f2dd6af1accc87b81f09f59f2b75e5c0bed6589dfe8c6");
  expect_digest(
      std::string(1000, 'a'),
      "41edece42d63e8d9bf515a9ba6932e1c20cbc9f5a5d134645adb5db1b9737ea3");

  // deterministic and value-equal
  const auto d1 = Digest::from_bytes(bytes_of("abc"));
  const auto d2 = Digest::from_bytes(bytes_of("abc"));
  EXPECT_TRUE(d1 == d2);
  EXPECT_TRUE(d1 != Digest::from_bytes(bytes_of("abd")));

  // parse: uppercase input accepted, canonical output lowercase
  auto upper = Digest::parse(
      "BA7816BF8F01CFEA414140DE5DAE2223B00361A396177A9CB410FF61F20015AD");
  EXPECT_TRUE(static_cast<bool>(upper));
  if (upper) {
    EXPECT_EQ(
        upper->hex(),
        "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    EXPECT_TRUE(*upper == d1);
  }

  // parse: length and character errors -> invalid_input
  auto short_parse = Digest::parse("abc");
  EXPECT_TRUE(!short_parse);
  EXPECT_EQ(amp::core::error_code_name(short_parse.error().code),
            "invalid-input");
  auto nonhex = Digest::parse(std::string(64, '0') + "zz");
  // 66 chars: wrong length
  EXPECT_TRUE(!nonhex);
  auto bad_chars = Digest::parse(std::string(62, '0') + "zz");
  EXPECT_TRUE(!bad_chars);
  EXPECT_EQ(amp::core::error_code_name(bad_chars.error().code),
            "invalid-input");
  // error span points at the offending byte (byte 31, chars 62/63)
  EXPECT_EQ(bad_chars.error().span.line, 63);

  return amp_test::summary("amp-core digest");
}
