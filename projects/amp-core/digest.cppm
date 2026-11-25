// amp-core cryptographic digest (M0). SHA-256 per FIPS 180-4.
// Canonical encoding: 64 lowercase hex characters (CONTRACTS.md).
// Security-sensitive: docs/QUALITY.md requires >=95% line coverage here.
module;

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <expected>
#include <span>
#include <string>
#include <string_view>

export module amp.core.digest;

import amp.core.diagnostic;

namespace amp::core {

export struct Digest {
  static constexpr std::size_t size = 32;
  std::array<std::uint8_t, size> bytes{};

  // SHA-256 over the given bytes.
  static Digest from_bytes(std::span<const std::byte> data);

  // Parse a 64-character hex encoding (case-insensitive input).
  // Wrong length, non-hex input -> invalid_input.
  static Result<Digest> parse(std::string_view hex);

  // Canonical lowercase hex encoding.
  std::string hex() const;

  bool operator==(const Digest &) const = default;
};

namespace detail {

constexpr std::uint32_t k_sha256_constants[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1,
    0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786,
    0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147,
    0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
    0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a,
    0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline constexpr std::uint32_t rotr(std::uint32_t x, unsigned n) {
  return (x >> n) | (x << (32 - n));
}

// Compress one 64-byte block into h[8].
inline void sha256_compress(std::uint32_t h[8], const std::uint8_t block[64]) {
  std::uint32_t w[64];
  for (int i = 0; i < 16; ++i) {
    w[i] = (static_cast<std::uint32_t>(block[i * 4]) << 24) |
           (static_cast<std::uint32_t>(block[i * 4 + 1]) << 16) |
           (static_cast<std::uint32_t>(block[i * 4 + 2]) << 8) |
           static_cast<std::uint32_t>(block[i * 4 + 3]);
  }
  for (int i = 16; i < 64; ++i) {
    const std::uint32_t s0 =
        rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
    const std::uint32_t s1 =
        rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
    w[i] = w[i - 16] + s0 + w[i - 7] + s1;
  }
  std::uint32_t a = h[0], b = h[1], c = h[2], d = h[3];
  std::uint32_t e = h[4], f = h[5], g = h[6], hh = h[7];
  for (int i = 0; i < 64; ++i) {
    // FIPS 180-4: T1 = h + S1(e) + Ch(e,f,g) + K + W; T2 = S0(a) + Maj(a,b,c);
    // then h<-g, g<-f, f<-e, e<-d+T1, d<-c, c<-b, b<-a, a<-T1+T2.
    const std::uint32_t ch = (e & f) ^ (~e & g);
    const std::uint32_t s1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
    const std::uint32_t t1 = hh + s1 + ch + k_sha256_constants[i] + w[i];
    const std::uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
    const std::uint32_t s0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
    hh = g;
    g = f;
    f = e;
    e = d + t1;
    d = c;
    c = b;
    b = a;
    a = t1 + s0 + maj;
  }
  h[0] += a;
  h[1] += b;
  h[2] += c;
  h[3] += d;
  h[4] += e;
  h[5] += f;
  h[6] += g;
  h[7] += hh;
}

inline Digest sha256(std::span<const std::byte> message) {
  std::uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                        0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
  const std::size_t length = message.size();
  // Minimal block multiple holding message + 0x80 + 64-bit length.
  const std::size_t padded_length = ((length + 8) / 64 + 1) * 64;
  // shortcut: heap padding buffer is fine at stage-0; switch to a fixed
  // two-buffer path if digests become a hot path (benchmark evidence).
  std::string padded(padded_length, '\0');
  if (length > 0) { // memcpy with a null pointer is UB even for size 0
    std::memcpy(padded.data(), message.data(), length);
  }
  padded[length] = '\x80';
  const std::uint64_t bit_length = static_cast<std::uint64_t>(length) * 8;
  for (int i = 0; i < 8; ++i) {
    padded[padded_length - 1 - i] = static_cast<char>(bit_length >> (8 * i));
  }
  for (std::size_t offset = 0; offset < padded_length; offset += 64) {
    sha256_compress(
        h, reinterpret_cast<const std::uint8_t *>(padded.data() + offset));
  }
  Digest digest;
  for (std::size_t i = 0; i < 8; ++i) {
    digest.bytes[i * 4] = static_cast<std::uint8_t>(h[i] >> 24);
    digest.bytes[i * 4 + 1] = static_cast<std::uint8_t>(h[i] >> 16);
    digest.bytes[i * 4 + 2] = static_cast<std::uint8_t>(h[i] >> 8);
    digest.bytes[i * 4 + 3] = static_cast<std::uint8_t>(h[i]);
  }
  return digest;
}

inline int hex_value(char c) {
  if (c >= '0' && c <= '9')
    return c - '0';
  if (c >= 'a' && c <= 'f')
    return c - 'a' + 10;
  if (c >= 'A' && c <= 'F')
    return c - 'A' + 10;
  return -1;
}

} // namespace detail

Digest Digest::from_bytes(std::span<const std::byte> data) {
  return detail::sha256(data);
}

Result<Digest> Digest::parse(std::string_view hex) {
  if (hex.size() != size * 2) {
    Diagnostic d;
    d.code = ErrorCode::invalid_input;
    d.message = "digest hex must be exactly 64 characters";
    return std::unexpected<Diagnostic>(d);
  }
  Digest digest;
  for (std::size_t i = 0; i < size; ++i) {
    const int hi = detail::hex_value(hex[i * 2]);
    const int lo = detail::hex_value(hex[i * 2 + 1]);
    if (hi < 0 || lo < 0) {
      Diagnostic d;
      d.code = ErrorCode::invalid_input;
      d.message = "digest hex contains a non-hex character";
      d.span = Span{std::string(), static_cast<int>(i * 2 + 1),
                    static_cast<int>((hi < 0 ? 0 : 1) + 1)};
      return std::unexpected<Diagnostic>(d);
    }
    digest.bytes[i] = static_cast<std::uint8_t>((hi << 4) | lo);
  }
  return digest;
}

std::string Digest::hex() const {
  static constexpr char digits[] = "0123456789abcdef";
  std::string out;
  out.reserve(size * 2);
  for (const std::uint8_t byte : bytes) {
    out += digits[byte >> 4];
    out += digits[byte & 0x0f];
  }
  return out;
}

} // namespace amp::core
