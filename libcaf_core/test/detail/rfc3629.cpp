// This file is part of CAF, the C++ Actor Framework. See the file LICENSE in
// the main distribution directory for license terms and copyright or visit
// https://github.com/actor-framework/actor-framework/blob/master/LICENSE.

#define CAF_SUITE detail.rfc3629

#include "caf/detail/rfc3629.hpp"

#include "core-test.hpp"

using namespace caf;

namespace {

bool valid_utf8(const_byte_span bytes) noexcept {
  return detail::rfc3629::valid(bytes);
}

bool valid_utf8(std::string_view bytes) noexcept {
  return detail::rfc3629::valid(bytes);
}

constexpr std::byte operator""_b(unsigned long long x) {
  return static_cast<std::byte>(x);
}

// Missing continuation byte.
constexpr std::byte invalid_two_byte_1[] = {0xc8_b};

// Illegal non-shortest form (1).
constexpr std::byte invalid_two_byte_2[] = {0xc0_b, 0x80_b};

// Illegal non-shortest form (2).
constexpr std::byte invalid_two_byte_3[] = {0xc1_b, 0x80_b};

// Invalid continuation byte.
constexpr std::byte invalid_two_byte_4[] = {0xc8_b, 0x0f_b};

// Missing continuation bytes.
constexpr std::byte invalid_three_byte_1[] = {0xe8_b};

// Missing continuation byte.
constexpr std::byte invalid_three_byte_2[] = {0xe8_b, 0x80_b};

// Invalid continuation byte (1).
constexpr std::byte invalid_three_byte_3[] = {0xe8_b, 0x0f_b, 0x80_b};

// Invalid continuation byte (2).
constexpr std::byte invalid_three_byte_4[] = {0xe8_b, 0x80_b, 0x0f_b};

// Illegal non-shortest form (1).
constexpr std::byte invalid_three_byte_5[] = {0xe0_b, 0x80_b, 0x80_b};

// Illegal non-shortest form (2).
constexpr std::byte invalid_three_byte_6[] = {0xe0_b, 0x9f_b, 0x8f_b};

// Illegal surrogate (smallest).
constexpr std::byte invalid_three_byte_7[] = {0xed_b, 0xa0_b, 0x80_b};

// Illegal surrogate (largest).
constexpr std::byte invalid_three_byte_8[] = {0xed_b, 0xbf_b, 0xbf_b};

// Missing continuation bytes.
constexpr std::byte invalid_four_byte_1[] = {0xf1_b};

// Missing continuation bytes 3 and 4.
constexpr std::byte invalid_four_byte_2[] = {0xf1_b, 0xbc_b};

// Missing continuation byte 4.
constexpr std::byte invalid_four_byte_3[] = {0xf1_b, 0xbc_b, 0xbc_b};

// Invalid continuation byte (1).
constexpr std::byte invalid_four_byte_4[] = {0xf1_b, 0x08_b, 0x80_b, 0x80_b};

// Invalid continuation byte (2).
constexpr std::byte invalid_four_byte_5[] = {0xf1_b, 0x80_b, 0x08_b, 0x80_b};

// Invalid continuation byte (3).
constexpr std::byte invalid_four_byte_6[] = {0xf1_b, 0x80_b, 0x80_b, 0x08_b};

// Illegal non-shortest form.
constexpr std::byte invalid_four_byte_7[] = {0xf0_b, 0x8f_b, 0x8f_b, 0x8f_b};

// Illegal start of a sequence.
constexpr std::byte invalid_four_byte_8[] = {0xf8_b, 0x80_b, 0x80_b, 0x80_b};

// Smallest valid 2-byte sequence.
constexpr std::byte valid_two_byte_1[] = {0xc2_b, 0x80_b};

// Largest valid 2-byte sequence.
constexpr std::byte valid_two_byte_2[] = {0xdf_b, 0xbf_b};

// Smallest valid 3-byte sequence.
constexpr std::byte valid_three_byte_1[] = {0xe0_b, 0xa0_b, 0x80_b};

// Largest valid 3-byte sequence.
constexpr std::byte valid_three_byte_2[] = {0xef_b, 0xbf_b, 0xbf_b};

// UTF-8 standard covers code points in the sequences [0x0 - 0x110000)
// Theoretically, a larger value can be encoded in the 4-byte sequence.
// Smallest valid 4-byte sequence.
constexpr std::byte valid_four_byte_1[] = {0xf0_b, 0x90_b, 0x80_b, 0x80_b};

// Largest valid 4-byte sequence - code point 0x10FFFF.
constexpr std::byte valid_four_byte_2[] = {0xf4_b, 0x8f_b, 0xbf_b, 0xbf_b};

// Smallest invalid 4-byte sequence - code point 0x110000.
constexpr std::byte invalid_four_byte_9[] = {0xf4_b, 0x90_b, 0x80_b, 0x80_b};

// Largest invalid 4-byte sequence - invalid code point.
constexpr std::byte invalid_four_byte_10[] = {0xf7_b, 0xbf_b, 0xbf_b, 0xbf_b};

// Single line ASCII text.
constexpr std::string_view ascii_1 = "Hello World!";

// Multi-line ASCII text.
constexpr std::string_view ascii_2 = R"__(
 *                       ____    _    _____                                   *
 *                      / ___|  / \  |  ___|    CAF                           *
 *                     | |     / _ \ | |_       Actor                         *
 *                     | |___ / ___ \|  _|      Framework                     *
 *                      \____/_/   \_|_|                                      *
)__";

} // namespace

TEST_CASE("ASCII input") {
  CHECK(valid_utf8(ascii_1));
  CHECK(valid_utf8(ascii_2));
}

TEST_CASE("valid UTF-8 input") {
  CHECK(valid_utf8(valid_two_byte_1));
  CHECK(valid_utf8(valid_two_byte_2));
  CHECK(valid_utf8(valid_three_byte_1));
  CHECK(valid_utf8(valid_three_byte_2));
  CHECK(valid_utf8(valid_four_byte_1));
  CHECK(valid_utf8(valid_four_byte_2));
}

TEST_CASE("invalid UTF-8 input") {
  CHECK(!valid_utf8(invalid_two_byte_1));
  CHECK(!valid_utf8(invalid_two_byte_2));
  CHECK(!valid_utf8(invalid_two_byte_3));
  CHECK(!valid_utf8(invalid_two_byte_4));
  CHECK(!valid_utf8(invalid_three_byte_1));
  CHECK(!valid_utf8(invalid_three_byte_2));
  CHECK(!valid_utf8(invalid_three_byte_3));
  CHECK(!valid_utf8(invalid_three_byte_4));
  CHECK(!valid_utf8(invalid_three_byte_5));
  CHECK(!valid_utf8(invalid_three_byte_6));
  CHECK(!valid_utf8(invalid_three_byte_7));
  CHECK(!valid_utf8(invalid_three_byte_8));
  CHECK(!valid_utf8(invalid_four_byte_1));
  CHECK(!valid_utf8(invalid_four_byte_2));
  CHECK(!valid_utf8(invalid_four_byte_3));
  CHECK(!valid_utf8(invalid_four_byte_4));
  CHECK(!valid_utf8(invalid_four_byte_5));
  CHECK(!valid_utf8(invalid_four_byte_6));
  CHECK(!valid_utf8(invalid_four_byte_7));
  CHECK(!valid_utf8(invalid_four_byte_8));
  CHECK(!valid_utf8(invalid_four_byte_9));
  CHECK(!valid_utf8(invalid_four_byte_10));
}

namespace {

std::byte hex_to_byte(char c) {
  if (std::isdigit(c))
    return static_cast<std::byte>(c - '0');
  if (c >= 'a' && c <= 'f')
    return static_cast<std::byte>(c - 'a' + 10);
  return static_cast<std::byte>(c - 'A' + 10);
}

byte_buffer hex_to_bytes(std::string_view hex) {
  byte_buffer result;
  for (auto it = hex.begin(); it != hex.end(); it += 2)
    result.push_back(hex_to_byte(*it) << 4 | hex_to_byte(*(it + 1)));
  return result;
}

std::string_view to_msg(const byte_buffer& bs) {
  return {reinterpret_cast<const char*>(bs.data()), bs.size()};
}

} // namespace

using namespace std::literals;

TEST_CASE("invalid UTF-8 sequence") {
  {
    byte_buffer bs{0xce_b, 0xba_b, 0xe1_b, 0xbd_b, 0xb9_b, 0xcf_b, 0x83_b,
                   0xce_b, 0xbc_b, 0xce_b, 0xb5_b, 0xed_b, 0xa0_b, 0x80_b,
                   0x65_b, 0x64_b, 0x69_b, 0x74_b, 0x65_b, 0x64_b};
    MESSAGE("UTF-8 Payload: " << to_msg(bs));
    CHECK(valid_utf8(bs));
  }

  {
    auto bs = byte_buffer{0x48_b, 0x65_b, 0x6c_b, 0x6c_b, 0x6f_b, 0x2d_b,
                          0xc2_b, 0xb5_b, 0x40_b, 0xc3_b, 0x9f_b, 0xc3_b,
                          0xb6_b, 0xc3_b, 0xa4_b, 0xc3_b, 0xbc_b, 0xc3_b,
                          0xa0_b, 0xc3_b, 0xa1_b, 0x2d_b, 0x55_b, 0x54_b,
                          0x46_b, 0x2d_b, 0x38_b, 0x21_b, 0x21_b};
    MESSAGE("UTF-8 Payload: " << to_msg(bs));
    CHECK(valid_utf8(bs));
  }

  {
    auto bs = hex_to_bytes("48656c6c6f2dc2b540c39fc3b6c3a4"sv);
    MESSAGE("UTF-8 Payload: " << to_msg(bs));
    CHECK(valid_utf8(bs));
    bs = hex_to_bytes("c3bcc3a0c3a12d5554462d382121"sv);
    MESSAGE("UTF-8 Payload: " << to_msg(bs));
    CHECK(valid_utf8(bs));
  }

  auto b1 = byte_buffer({0xce_b, 0xba_b, 0xe1_b, 0xbd_b, 0xb9_b, 0xcf_b, 0x83_b,
                         0xce_b, 0xbc_b, 0xce_b, 0xb5_b});
  auto b2 = byte_buffer({0xf4_b, 0x90_b, 0x80_b, 0x80_b});
  auto b3 = byte_buffer({0x65_b, 0x64_b, 0x69_b, 0x74_b, 0x65_b, 0x64_b});

  CHECK(valid_utf8(b1));
  CHECK(valid_utf8(b2));
  CHECK(valid_utf8(b3));
}
